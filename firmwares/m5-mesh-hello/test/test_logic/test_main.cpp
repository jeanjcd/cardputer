#include <unity.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>
#include "mesh/kv.h"
#include "mesh/nodedb.h"
#include "mesh/chat.h"
#include "mesh/greeter.h"
#include "mesh/outbox.h"

struct FakeKv : Kv {
  std::map<std::string, std::vector<uint8_t>> m;
  bool get(const char* k, void* buf, size_t len) override {
    auto it = m.find(k);
    if (it == m.end() || it->second.size() != len) return false;
    memcpy(buf, it->second.data(), len);
    return true;
  }
  bool put(const char* k, const void* buf, size_t len) override {
    if (strlen(k) > 15) return false;  // NVS key limit
    const uint8_t* p = (const uint8_t*)buf;
    m[k] = std::vector<uint8_t>(p, p + len);
    return true;
  }
};

void setUp() {}
void tearDown() {}

static MeshUser mkUser(const char* longName, uint8_t keyFill, bool withKey = true) {
  MeshUser u; memset(&u, 0, sizeof u);
  strcpy(u.longName, longName); strcpy(u.shortName, "SN");
  if (withKey) { memset(u.publicKey, keyFill, 32); u.publicKeyLen = 32; }
  return u;
}

void test_touch_and_evict_oldest() {
  FakeKv kv; NodeDb db(kv);
  for (uint32_t i = 1; i <= 101; i++) db.touch(i, -50, 5.0f, i * 10);
  TEST_ASSERT_EQUAL_UINT32(100, db.count());
  bool found1 = false;
  for (size_t i = 0; i < db.count(); i++) if (db.at(i).num == 1) found1 = true;
  TEST_ASSERT_FALSE(found1);
  TEST_ASSERT_EQUAL_UINT32(101, db.at(0).num);  // newest first
  TEST_ASSERT_EQUAL_UINT32(2, db.at(99).num);
  // re-touching moves to front and updates fields
  NodeEntry& e = db.touch(2, -70, 1.5f, 5000);
  TEST_ASSERT_EQUAL_UINT32(2, e.num);
  TEST_ASSERT_EQUAL_INT16(-70, e.rssi);
  TEST_ASSERT_EQUAL_UINT32(2, db.at(0).num);
  TEST_ASSERT_EQUAL_UINT32(100, db.count());
}

void test_first_key_wins() {
  FakeKv kv; NodeDb db(kv);
  db.touch(7, 0, 0, 1);
  TEST_ASSERT_TRUE(db.applyUser(7, mkUser("Alice", 0xAA)));
  TEST_ASSERT_NOT_NULL(db.pubKey(7));
  TEST_ASSERT_EQUAL_UINT8(0xAA, db.pubKey(7)[0]);
  // different key: rejected, nothing updated
  TEST_ASSERT_FALSE(db.applyUser(7, mkUser("Mallory", 0xBB)));
  TEST_ASSERT_EQUAL_UINT8(0xAA, db.pubKey(7)[0]);
  TEST_ASSERT_EQUAL_STRING("Alice", db.at(0).user.longName);
  // absent key: rejected too
  TEST_ASSERT_FALSE(db.applyUser(7, mkUser("Mallory", 0, false)));
  TEST_ASSERT_EQUAL_STRING("Alice", db.at(0).user.longName);
  // same key: accepted, names update
  TEST_ASSERT_TRUE(db.applyUser(7, mkUser("Alice2", 0xAA)));
  TEST_ASSERT_EQUAL_STRING("Alice2", db.at(0).user.longName);
  // keyless node accepts names, no key
  db.touch(8, 0, 0, 2);
  TEST_ASSERT_TRUE(db.applyUser(8, mkUser("Bob", 0, false)));
  TEST_ASSERT_NULL(db.pubKey(8));
  TEST_ASSERT_NULL(db.pubKey(999));
}

void test_peer_persist_reload() {
  FakeKv kv; NodeDb db(kv);
  db.touch(7, 0, 0, 1);
  db.applyUser(7, mkUser("Alice", 0xAA));
  db.persistPeer(7);
  NodeDb db2(kv); db2.load();
  TEST_ASSERT_NOT_NULL(db2.pubKey(7));
  TEST_ASSERT_EQUAL_UINT8(0xAA, db2.pubKey(7)[31]);
  TEST_ASSERT_EQUAL_UINT32(1, db2.count());
  TEST_ASSERT_EQUAL_STRING("Alice", db2.at(0).user.longName);
  for (auto& p : kv.m) TEST_ASSERT_TRUE(p.first.size() <= 15);
}

void test_peer_persist_cap_overwrites_least_recent() {
  FakeKv kv; NodeDb db(kv);
  for (uint32_t i = 1; i <= 51; i++) {
    db.touch(i, 0, 0, i);
    db.applyUser(i, mkUser("P", (uint8_t)i));
    db.persistPeer(i);
  }
  NodeDb db2(kv); db2.load();
  TEST_ASSERT_EQUAL_UINT32(50, db2.count());
  TEST_ASSERT_NULL(db2.pubKey(1));      // least recently persisted overwritten
  TEST_ASSERT_NOT_NULL(db2.pubKey(51));
  TEST_ASSERT_NOT_NULL(db2.pubKey(2));
}

void test_corrupt_blobs_load_empty() {
  FakeKv kv;
  // wrong-size, bad version, absurd count, garbage records
  kv.m["greeted"] = std::vector<uint8_t>(10, 0xFF);
  kv.m["peer0"] = std::vector<uint8_t>(5, 0xFF);
  kv.m["peer1"] = std::vector<uint8_t>(89, 0xFF);
  NodeDb db(kv); db.load();
  TEST_ASSERT_EQUAL_UINT32(0, db.count());
  Greeter g(kv); g.load();
  TEST_ASSERT_FALSE(g.wasGreeted(0xFFFFFFFF));
  // right size, bad version / huge count
  FakeKv kv2;
  kv2.m["greeted"] = std::vector<uint8_t>(3 + 500 * 4, 0xFF);
  Greeter g2(kv2); g2.load();
  TEST_ASSERT_FALSE(g2.wasGreeted(0xFFFFFFFF));
  g2.setEnabled(true); g2.onNodeWithKey(5);
  uint32_t n = 0;
  TEST_ASSERT_TRUE(g2.due(0, n));
  TEST_ASSERT_EQUAL_UINT32(5, n);
  // valid header, count over the max
  FakeKv kv3;
  std::vector<uint8_t> b(3 + 500 * 4, 0); b[0] = 1; b[1] = 0xFF; b[2] = 0xFF;
  kv3.m["greeted"] = b;
  Greeter g3(kv3); g3.load();
  TEST_ASSERT_FALSE(g3.wasGreeted(0));
}

void test_chat_cap_and_ack() {
  ChatStore cs;
  for (uint32_t i = 1; i <= 35; i++) {
    ChatMsg m; memset(&m, 0, sizeof m);
    m.mine = true; m.id = i; m.state = ChatMsg::Sent; strcpy(m.text, "hi");
    cs.add(0xFFFFFFFF, m);
  }
  TEST_ASSERT_EQUAL_UINT32(30, cs.count(0xFFFFFFFF));
  TEST_ASSERT_EQUAL_UINT32(6, cs.at(0xFFFFFFFF, 0).id);   // oldest kept
  TEST_ASSERT_EQUAL_UINT32(35, cs.at(0xFFFFFFFF, 29).id);
  TEST_ASSERT_NULL(cs.findById(1));
  cs.markAcked(20);
  TEST_ASSERT_TRUE(cs.findById(20)->state == ChatMsg::Acked);
  TEST_ASSERT_TRUE(cs.findById(21)->state == ChatMsg::Sent);
  cs.markAcked(9999);  // unknown id: no crash
  TEST_ASSERT_EQUAL_UINT32(0, cs.count(42));
}

void test_chat_utf8_truncation() {
  ChatStore cs;
  ChatMsg m; memset(&m, 'a', sizeof m.text);  // 200 bytes, no NUL
  m.mine = false; m.id = 1; m.state = ChatMsg::Sent;
  // put a 2-byte char straddling the 199-byte limit: bytes 198,199
  m.text[198] = (char)0xC3; m.text[199] = (char)0xA9;
  cs.add(5, m);
  const char* t = cs.at(5, 0).text;
  TEST_ASSERT_EQUAL_UINT32(198, strlen(t));
}

void test_greeter_disabled_ignores() {
  FakeKv kv; Greeter g(kv);
  g.onNodeWithKey(5);
  uint32_t n;
  TEST_ASSERT_FALSE(g.due(0, n));
  g.setEnabled(true);
  TEST_ASSERT_FALSE(g.due(0, n));  // nothing was queued while disabled
}

void test_greeter_once_per_node() {
  FakeKv kv; Greeter g(kv); g.setEnabled(true);
  g.onNodeWithKey(5); g.onNodeWithKey(5);
  uint32_t n = 0;
  TEST_ASSERT_TRUE(g.due(0, n));
  TEST_ASSERT_EQUAL_UINT32(5, n);
  TEST_ASSERT_TRUE(g.wasGreeted(5));
  TEST_ASSERT_FALSE(g.due(100000, n));
  g.onNodeWithKey(5);
  TEST_ASSERT_FALSE(g.due(200000, n));
}

void test_greeter_rate_and_queue() {
  FakeKv kv; Greeter g(kv); g.setEnabled(true);
  for (uint32_t i = 1; i <= 15; i++) g.onNodeWithKey(i);
  uint32_t n = 0, got = 0;
  TEST_ASSERT_TRUE(g.due(0, n)); got++;
  TEST_ASSERT_EQUAL_UINT32(1, n);  // FIFO
  TEST_ASSERT_FALSE(g.due(29999, n));
  TEST_ASSERT_TRUE(g.due(30000, n)); got++;
  uint32_t t = 30000;
  while (g.due(t += 30000, n)) got++;
  TEST_ASSERT_EQUAL_UINT32(10, got);  // only 10 were queued
  TEST_ASSERT_FALSE(g.wasGreeted(11));
}

void test_greeter_wraparound() {
  FakeKv kv; Greeter g(kv); g.setEnabled(true);
  g.onNodeWithKey(1); g.onNodeWithKey(2);
  uint32_t n;
  TEST_ASSERT_TRUE(g.due(0xFFFFFF00u, n));
  TEST_ASSERT_FALSE(g.due(0xFFFFFF00u + 29999u, n));  // wraps past 0
  TEST_ASSERT_TRUE(g.due(0xFFFFFF00u + 30000u, n));
}

void test_greeter_persisted() {
  FakeKv kv;
  { Greeter g(kv); g.setEnabled(true); g.onNodeWithKey(5);
    uint32_t n; TEST_ASSERT_TRUE(g.due(0, n)); }
  Greeter g2(kv); g2.load(); g2.setEnabled(true);
  TEST_ASSERT_TRUE(g2.wasGreeted(5));
  g2.onNodeWithKey(5);
  uint32_t n;
  TEST_ASSERT_FALSE(g2.due(1000000, n));
}

void test_greeter_full_set_stops_greeting() {
  FakeKv kv; Greeter g(kv, 30000, 10, 3); g.setEnabled(true);
  uint32_t n, t = 0;
  for (uint32_t i = 1; i <= 3; i++) { g.onNodeWithKey(i); TEST_ASSERT_TRUE(g.due(t, n)); t += 30000; }
  g.onNodeWithKey(4);
  TEST_ASSERT_FALSE(g.due(t, n));
  TEST_ASSERT_TRUE(g.wasGreeted(1));  // old entries never forgotten
  TEST_ASSERT_FALSE(g.wasGreeted(4));
}

static OutItem mkItem(uint32_t to, const char* t, uint32_t chatId) {
  OutItem o; memset(&o, 0, sizeof o); o.to = to; strcpy(o.text, t); o.chatId = chatId; return o;
}

void test_dm_waits_for_key() {
  Outbox ob; OutItem out;
  TEST_ASSERT_TRUE(ob.queueDirect(mkItem(9, "a", 1), false));
  TEST_ASSERT_TRUE(ob.queueDirect(mkItem(8, "b", 2), false));
  TEST_ASSERT_TRUE(ob.queueDirect(mkItem(9, "c", 3), false));
  TEST_ASSERT_FALSE(ob.next(out));
  ob.onKeyArrived(9);
  TEST_ASSERT_TRUE(ob.next(out)); TEST_ASSERT_EQUAL_STRING("a", out.text);
  TEST_ASSERT_TRUE(ob.next(out)); TEST_ASSERT_EQUAL_STRING("c", out.text);
  TEST_ASSERT_FALSE(ob.next(out));  // node 8 still held
  ob.onKeyArrived(8);
  TEST_ASSERT_TRUE(ob.next(out)); TEST_ASSERT_EQUAL_UINT32(2, out.chatId);
  TEST_ASSERT_FALSE(ob.next(out));
}

void test_outbox_known_key_fifo_and_cap() {
  Outbox ob; OutItem out;
  TEST_ASSERT_TRUE(ob.queueDirect(mkItem(1, "x", 1), false));
  for (uint32_t i = 0; i < 9; i++) TEST_ASSERT_TRUE(ob.queueDirect(mkItem(2, "k", 10 + i), true));
  TEST_ASSERT_FALSE(ob.queueDirect(mkItem(3, "z", 99), true));   // full (10)
  TEST_ASSERT_FALSE(ob.queueDirect(mkItem(3, "z", 99), false));
  TEST_ASSERT_TRUE(ob.next(out));
  TEST_ASSERT_EQUAL_UINT32(10, out.chatId);
  TEST_ASSERT_TRUE(ob.queueDirect(mkItem(3, "z", 99), true));    // room again
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_touch_and_evict_oldest);
  RUN_TEST(test_first_key_wins);
  RUN_TEST(test_peer_persist_reload);
  RUN_TEST(test_peer_persist_cap_overwrites_least_recent);
  RUN_TEST(test_corrupt_blobs_load_empty);
  RUN_TEST(test_chat_cap_and_ack);
  RUN_TEST(test_chat_utf8_truncation);
  RUN_TEST(test_greeter_disabled_ignores);
  RUN_TEST(test_greeter_once_per_node);
  RUN_TEST(test_greeter_rate_and_queue);
  RUN_TEST(test_greeter_wraparound);
  RUN_TEST(test_greeter_persisted);
  RUN_TEST(test_greeter_full_set_stops_greeting);
  RUN_TEST(test_dm_waits_for_key);
  RUN_TEST(test_outbox_known_key_fifo_and_cap);
  return UNITY_END();
}
