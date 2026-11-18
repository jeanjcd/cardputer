// SPDX-License-Identifier: GPL-3.0-or-later
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
  bool failPut = false;
  bool put(const char* k, const void* buf, size_t len) override {
    if (failPut) return false;
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

// Attack: flood ~100 forged keyed NodeInfos so a saved peer (loaded at boot, lastHeardMs = 0) is evicted, then
// claim that peer's node number with another key. The persisted pin must keep the original key.
void test_pinned_key_survives_eviction() {
  FakeKv kv;
  {
    NodeDb db(kv);
    db.touch(7, 0, 0, 1);
    db.applyUser(7, mkUser("Alice", 0xAA));
    db.persistPeer(7);
  }
  NodeDb db(kv); db.load();
  TEST_ASSERT_NOT_NULL(db.get(7));
  for (uint32_t i = 1000; i < 1000 + NodeDb::kMaxNodes; i++) {
    db.touch(i, -90, 0, i);
    TEST_ASSERT_TRUE(db.applyUser(i, mkUser("F", 0x11)));
  }
  TEST_ASSERT_NULL(db.get(7));                     // evicted from the RAM table (all others are keyed)
  TEST_ASSERT_NOT_NULL(db.pubKey(7));              // ...but the pinned key is still returned
  TEST_ASSERT_EQUAL_UINT8(0xAA, db.pubKey(7)[0]);
  NodeEntry& e = db.touch(7, -80, 0, 5000);        // forged NodeInfo for node 7 arrives
  TEST_ASSERT_TRUE(e.hasKey);                      // re-created entry carries the pinned key
  TEST_ASSERT_FALSE(db.applyUser(7, mkUser("Mallory", 0xBB)));
  TEST_ASSERT_FALSE(db.applyUser(7, mkUser("Mallory", 0, false)));
  TEST_ASSERT_EQUAL_UINT8(0xAA, db.pubKey(7)[0]);
  TEST_ASSERT_TRUE(db.applyUser(7, mkUser("Alice", 0xAA)));  // the real owner is still accepted
  TEST_ASSERT_EQUAL_STRING("Alice", db.get(7)->user.longName);
}

// The pin is checked even if a RAM entry somehow has no key (defence in depth for applyUser).
void test_pin_checked_in_apply_user() {
  FakeKv kv;
  { NodeDb db(kv); db.touch(9, 0, 0, 1); db.applyUser(9, mkUser("Bob", 0xCC)); db.persistPeer(9); }
  NodeDb db(kv); db.load();
  TEST_ASSERT_FALSE(db.applyUser(9, mkUser("Eve", 0xDD)));
  TEST_ASSERT_EQUAL_UINT8(0xCC, db.pubKey(9)[0]);
}

// Eviction prefers keyless, non-pinned entries over keyed or pinned ones, even if those are older.
void test_eviction_prefers_keyless() {
  FakeKv kv; NodeDb db(kv);
  db.touch(1, 0, 0, 1); db.applyUser(1, mkUser("K", 0x01));       // oldest, keyed
  db.touch(2, 0, 0, 2);                                              // keyless
  for (uint32_t i = 3; i <= NodeDb::kMaxNodes; i++) { db.touch(i, 0, 0, i); db.applyUser(i, mkUser("K", 0x02)); }
  db.touch(500, 0, 0, 1000);
  TEST_ASSERT_EQUAL_UINT32(NodeDb::kMaxNodes, db.count());
  TEST_ASSERT_NULL(db.get(2));     // the keyless one went
  TEST_ASSERT_NOT_NULL(db.get(1)); // the older keyed one stayed
  db.touch(501, 0, 0, 1001);       // now 500 is the only keyless entry left (and newest-but-one)
  TEST_ASSERT_NULL(db.get(500));
  TEST_ASSERT_NOT_NULL(db.get(1));
  db.applyUser(501, mkUser("K", 0x03));
  db.touch(502, 0, 0, 1002);       // all keyed: oldest-heard goes
  TEST_ASSERT_NULL(db.get(1));
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
  TEST_ASSERT_TRUE(ob.queueDirect(mkItem(9, "a", 1), false, 0));
  TEST_ASSERT_TRUE(ob.queueDirect(mkItem(8, "b", 2), false, 0));
  TEST_ASSERT_TRUE(ob.queueDirect(mkItem(9, "c", 3), false, 0));
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
  for (uint32_t i = 0; i < 10; i++) TEST_ASSERT_TRUE(ob.queueDirect(mkItem(2, "k", 10 + i), true, 0));
  uint32_t ev = 123;
  TEST_ASSERT_FALSE(ob.queueDirect(mkItem(3, "z", 99), true, 0, &ev));   // full (10) of ready items: no eviction
  TEST_ASSERT_EQUAL_UINT32(0, ev);
  TEST_ASSERT_FALSE(ob.queueDirect(mkItem(3, "z", 99), false, 0));
  TEST_ASSERT_TRUE(ob.next(out));
  TEST_ASSERT_EQUAL_UINT32(10, out.chatId);
  TEST_ASSERT_TRUE(ob.queueDirect(mkItem(3, "z", 99), true, 0));    // room again
}

// Held DMs to a node that never answers expire after 10 minutes instead of blocking the outbox forever.
void test_outbox_held_expiry() {
  Outbox ob; uint32_t id = 0;
  TEST_ASSERT_TRUE(ob.queueDirect(mkItem(9, "a", 1), false, 0xFFFFF000u));  // wraps around during its lifetime
  TEST_ASSERT_TRUE(ob.queueDirect(mkItem(8, "b", 2), false, 0xFFFFF000u + 60000));
  TEST_ASSERT_TRUE(ob.queueDirect(mkItem(7, "r", 3), true, 0xFFFFF000u));   // ready items never expire
  TEST_ASSERT_FALSE(ob.expireOne(0xFFFFF000u + Outbox::kHeldTtlMs - 1, &id));
  TEST_ASSERT_TRUE(ob.expireOne(0xFFFFF000u + Outbox::kHeldTtlMs, &id));
  TEST_ASSERT_EQUAL_UINT32(1, id);
  TEST_ASSERT_FALSE(ob.expireOne(0xFFFFF000u + Outbox::kHeldTtlMs, &id));
  TEST_ASSERT_EQUAL_UINT32(2, ob.count());
  TEST_ASSERT_TRUE(ob.expireOne(0xFFFFF000u + 60000 + Outbox::kHeldTtlMs, &id));
  TEST_ASSERT_EQUAL_UINT32(2, id);
  TEST_ASSERT_EQUAL_UINT32(1, ob.count());
  OutItem out;
  TEST_ASSERT_TRUE(ob.next(out));
  TEST_ASSERT_EQUAL_UINT32(3, out.chatId);
  ob.onKeyArrived(9);  // key of an expired item arriving late releases nothing
  TEST_ASSERT_FALSE(ob.next(out));
}

// A ready DM that finds the outbox full evicts the oldest held item (the caller marks it failed).
void test_outbox_full_evicts_oldest_held() {
  Outbox ob; OutItem out; uint32_t ev = 0;
  TEST_ASSERT_TRUE(ob.queueDirect(mkItem(2, "r", 100), true, 0));
  for (uint32_t i = 0; i < 9; i++) TEST_ASSERT_TRUE(ob.queueDirect(mkItem(50 + i, "h", 1 + i), false, i));
  TEST_ASSERT_FALSE(ob.queueDirect(mkItem(60, "h", 77), false, 20, &ev));  // held cannot evict
  TEST_ASSERT_EQUAL_UINT32(0, ev);
  TEST_ASSERT_TRUE(ob.queueDirect(mkItem(3, "k", 200), true, 20, &ev));
  TEST_ASSERT_EQUAL_UINT32(1, ev);  // oldest held
  TEST_ASSERT_TRUE(ob.queueDirect(mkItem(3, "k", 201), true, 21, &ev));
  TEST_ASSERT_EQUAL_UINT32(2, ev);
  TEST_ASSERT_EQUAL_UINT32(Outbox::kMax, ob.count());
  TEST_ASSERT_TRUE(ob.next(out)); TEST_ASSERT_EQUAL_UINT32(100, out.chatId);
  TEST_ASSERT_TRUE(ob.next(out)); TEST_ASSERT_EQUAL_UINT32(200, out.chatId);
  TEST_ASSERT_TRUE(ob.next(out)); TEST_ASSERT_EQUAL_UINT32(201, out.chatId);
  TEST_ASSERT_FALSE(ob.next(out));
}

void test_outbox_held_peers() {
  Outbox ob; uint32_t peers[4];
  TEST_ASSERT_EQUAL_UINT32(0, ob.heldPeers(peers, 4));
  ob.queueDirect(mkItem(9, "a", 1), false, 0);
  ob.queueDirect(mkItem(8, "b", 2), false, 0);
  ob.queueDirect(mkItem(9, "c", 3), false, 0);
  ob.queueDirect(mkItem(5, "d", 4), true, 0);
  TEST_ASSERT_EQUAL_UINT32(2, ob.heldPeers(peers, 4));
  TEST_ASSERT_EQUAL_UINT32(9, peers[0]);
  TEST_ASSERT_EQUAL_UINT32(8, peers[1]);
  TEST_ASSERT_EQUAL_UINT32(1, ob.heldPeers(peers, 1));
  ob.onKeyArrived(9);
  TEST_ASSERT_EQUAL_UINT32(1, ob.heldPeers(peers, 4));
  TEST_ASSERT_EQUAL_UINT32(8, peers[0]);
}

static ChatMsg mkMsg(uint32_t id, bool mine = false) {
  ChatMsg m; memset(&m, 0, sizeof m); m.id = id; m.mine = mine; m.state = ChatMsg::Sent; strcpy(m.text, "x"); return m;
}

void test_chat_pool_evicts_globally_oldest() {
  ChatStore cs;
  for (uint32_t i = 1; i <= 96; i++) cs.add(1000 + (i % 4), mkMsg(i));  // 4 peers x 24
  cs.add(2000, mkMsg(500));
  TEST_ASSERT_NULL(cs.findById(1));          // globally oldest gone
  TEST_ASSERT_NOT_NULL(cs.findById(2));
  TEST_ASSERT_NOT_NULL(cs.findById(500));
  cs.add(2000, mkMsg(501));
  TEST_ASSERT_NULL(cs.findById(2));
}

void test_chat_public_survives_dm_flood() {
  ChatStore cs;
  for (uint32_t i = 1; i <= 5; i++) cs.add(ChatStore::kPublic, mkMsg(i));
  for (uint32_t p = 1; p <= 12; p++)
    for (uint32_t k = 0; k < 10; k++) cs.add(p, mkMsg(100 + p * 10 + k));  // 120 DMs from 12 peers
  // pool is 96: only the oldest-by-seq went, i.e. public msgs first, then oldest DMs; no unit drop
  size_t total = cs.count(ChatStore::kPublic);
  for (uint32_t p = 1; p <= 12; p++) total += cs.count(p);
  TEST_ASSERT_EQUAL_UINT32(96, total);
  TEST_ASSERT_EQUAL_UINT32(0, cs.count(ChatStore::kPublic));  // oldest 5 + 19 DMs evicted
  TEST_ASSERT_EQUAL_UINT32(0, cs.count(1));
  TEST_ASSERT_EQUAL_UINT32(10, cs.count(12));
  // fresher public traffic is kept alongside the flood
  ChatStore c2;
  for (uint32_t p = 1; p <= 12; p++) for (uint32_t k = 0; k < 8; k++) c2.add(p, mkMsg(100 + p * 10 + k));
  for (uint32_t i = 1; i <= 5; i++) c2.add(ChatStore::kPublic, mkMsg(i));
  for (uint32_t p = 13; p <= 20; p++) c2.add(p, mkMsg(900 + p));
  TEST_ASSERT_EQUAL_UINT32(5, c2.count(ChatStore::kPublic));
}

void test_chat_per_peer_cap_and_order() {
  ChatStore cs;
  for (uint32_t i = 1; i <= 40; i++) { cs.add(7, mkMsg(i)); cs.add(8, mkMsg(1000 + i)); }
  TEST_ASSERT_EQUAL_UINT32(30, cs.count(7));
  TEST_ASSERT_EQUAL_UINT32(30, cs.count(8));
  for (size_t i = 0; i < 30; i++) {
    TEST_ASSERT_EQUAL_UINT32(11 + i, cs.at(7, i).id);
    TEST_ASSERT_EQUAL_UINT32(1011 + i, cs.at(8, i).id);
  }
  TEST_ASSERT_EQUAL_UINT32(0, cs.at(7, 30).id);
}

void test_chat_settext_alias() {
  ChatMsg m; memset(&m, 0, sizeof m); strcpy(m.text, "hello");
  m.setText(m.text + 1);
  TEST_ASSERT_EQUAL_STRING("ello", m.text);
  ChatStore cs; cs.add(1, mkMsg(1));
  cs.add(1, cs.at(1, 0));  // aliasing stored data
  TEST_ASSERT_EQUAL_UINT32(2, cs.count(1));
}

void test_greeter_persist_failure_keeps_queued() {
  FakeKv kv; Greeter g(kv); g.setEnabled(true);
  g.onNodeWithKey(5);
  uint32_t n = 0;
  kv.failPut = true;
  TEST_ASSERT_FALSE(g.due(0, n));
  TEST_ASSERT_FALSE(g.wasGreeted(5));
  kv.failPut = false;
  TEST_ASSERT_FALSE(g.due(1, n));       // backing off: Kv not retried yet
  TEST_ASSERT_FALSE(g.due(59999, n));
  TEST_ASSERT_TRUE(g.due(60000, n));
  TEST_ASSERT_EQUAL_UINT32(5, n);
}

// Counts put() calls to prove the greeter does not hammer the Kv while backing off.
struct CountingKv : FakeKv {
  int puts = 0;
  bool put(const char* k, const void* buf, size_t len) override { puts++; return FakeKv::put(k, buf, len); }
};

void test_greeter_persist_failure_backoff() {
  CountingKv kv; Greeter g(kv); g.setEnabled(true);
  g.onNodeWithKey(5);
  uint32_t n = 0;
  kv.failPut = true;
  TEST_ASSERT_FALSE(g.due(1000, n));
  TEST_ASSERT_EQUAL_INT(1, kv.puts);
  for (uint32_t t = 1001; t < 61000; t += 7) g.due(t, n);
  TEST_ASSERT_EQUAL_INT(1, kv.puts);
  TEST_ASSERT_FALSE(g.due(61000, n));  // retried once after 60 s, failed again
  TEST_ASSERT_EQUAL_INT(2, kv.puts);
  kv.failPut = false;
  TEST_ASSERT_FALSE(g.due(61001, n));
  TEST_ASSERT_TRUE(g.due(121000, n));
  TEST_ASSERT_EQUAL_UINT32(5, n);
  TEST_ASSERT_EQUAL_INT(3, kv.puts);
  // a success clears the backoff: the next greeting only waits the normal interval
  g.onNodeWithKey(6);
  TEST_ASSERT_TRUE(g.due(151000, n));
  TEST_ASSERT_EQUAL_UINT32(6, n);
}

void test_greeter_mark_manual() {
  FakeKv kv; Greeter g(kv); g.setEnabled(true);
  g.onNodeWithKey(5); g.onNodeWithKey(6);
  TEST_ASSERT_TRUE(g.markGreeted(5));
  TEST_ASSERT_TRUE(g.wasGreeted(5));
  TEST_ASSERT_TRUE(g.markGreeted(5));  // idempotent
  TEST_ASSERT_EQUAL_UINT32(1, g.greetedCount());
  uint32_t n = 0;
  TEST_ASSERT_TRUE(g.due(0, n));
  TEST_ASSERT_EQUAL_UINT32(6, n);  // 5 left the queue
  TEST_ASSERT_FALSE(g.due(100000, n));
  kv.failPut = true;
  TEST_ASSERT_FALSE(g.markGreeted(7));
  TEST_ASSERT_FALSE(g.wasGreeted(7));
  kv.failPut = false;
  Greeter g2(kv); g2.load();
  TEST_ASSERT_TRUE(g2.wasGreeted(5));
  TEST_ASSERT_TRUE(g2.wasGreeted(6));
}

void test_nodedb_get_mutable() {
  FakeKv kv; NodeDb db(kv);
  TEST_ASSERT_NULL(db.get(3));
  db.touch(3, -80, 1.0f, 10);
  NodeEntry* e = db.get(3);
  TEST_ASSERT_NOT_NULL(e);
  e->unread = 2;
  TEST_ASSERT_EQUAL_UINT8(2, db.at(0).unread);
  db.touch(3, -81, 1.0f, 20);  // touch keeps UI flags
  TEST_ASSERT_EQUAL_UINT8(2, db.at(0).unread);
}

void test_sizes() {
  printf("sizeof ChatStore=%u NodeDb=%u Greeter=%u Outbox=%u\n", (unsigned)sizeof(ChatStore),
         (unsigned)sizeof(NodeDb), (unsigned)sizeof(Greeter), (unsigned)sizeof(Outbox));
  TEST_ASSERT_TRUE(sizeof(ChatStore) <= 22 * 1024);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_touch_and_evict_oldest);
  RUN_TEST(test_first_key_wins);
  RUN_TEST(test_pinned_key_survives_eviction);
  RUN_TEST(test_pin_checked_in_apply_user);
  RUN_TEST(test_eviction_prefers_keyless);
  RUN_TEST(test_peer_persist_reload);
  RUN_TEST(test_peer_persist_cap_overwrites_least_recent);
  RUN_TEST(test_corrupt_blobs_load_empty);
  RUN_TEST(test_chat_cap_and_ack);
  RUN_TEST(test_chat_utf8_truncation);
  RUN_TEST(test_chat_pool_evicts_globally_oldest);
  RUN_TEST(test_chat_public_survives_dm_flood);
  RUN_TEST(test_chat_per_peer_cap_and_order);
  RUN_TEST(test_chat_settext_alias);
  RUN_TEST(test_greeter_persist_failure_keeps_queued);
  RUN_TEST(test_greeter_persist_failure_backoff);
  RUN_TEST(test_greeter_mark_manual);
  RUN_TEST(test_nodedb_get_mutable);
  RUN_TEST(test_sizes);
  RUN_TEST(test_greeter_disabled_ignores);
  RUN_TEST(test_greeter_once_per_node);
  RUN_TEST(test_greeter_rate_and_queue);
  RUN_TEST(test_greeter_wraparound);
  RUN_TEST(test_greeter_persisted);
  RUN_TEST(test_greeter_full_set_stops_greeting);
  RUN_TEST(test_dm_waits_for_key);
  RUN_TEST(test_outbox_known_key_fifo_and_cap);
  RUN_TEST(test_outbox_held_expiry);
  RUN_TEST(test_outbox_full_evicts_oldest_held);
  RUN_TEST(test_outbox_held_peers);
  return UNITY_END();
}
