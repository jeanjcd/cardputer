#include <unity.h>
#include <string.h>
#include "mesh/packet.h"
#include "mesh/region.h"
#include "mesh/crypto.h"
#include "../vectors.h"

static Identity A, B;

void setUp() {
  memset(&A, 0, sizeof A); memset(&B, 0, sizeof B);
  A.nodeNum = 0xA1B2C3D4; B.nodeNum = 0x11223344;
  memcpy(A.priv, X25519_PRIV_A, 32); memcpy(A.pub, X25519_PUB_A, 32);
  memcpy(B.priv, X25519_PRIV_B, 32); memcpy(B.pub, X25519_PUB_B, 32);
  strcpy(A.longName, "4stro-031"); strcpy(A.shortName, "4S31");
  strcpy(B.longName, "Bravo"); strcpy(B.shortName, "BRV");
}
void tearDown() {}

static const uint8_t* lookupA(uint32_t n) { return n == 0xA1B2C3D4 ? X25519_PUB_A : nullptr; }
static const uint8_t* lookupNone(uint32_t) { return nullptr; }

void test_text_broadcast_roundtrip() {
  uint8_t f[256]; Inbound in;
  size_t n = buildBroadcastText(A, 0x1234, "hello mesh", f);
  TEST_ASSERT_TRUE(n > 16);
  TEST_ASSERT_TRUE(parseFrame(B, f, n, lookupNone, in));
  TEST_ASSERT_TRUE(in.kind == InKind::Text);
  TEST_ASSERT_FALSE(in.direct);
  TEST_ASSERT_FALSE(in.pki);
  TEST_ASSERT_EQUAL_UINT(10, in.data.payloadLen);
  TEST_ASSERT_EQUAL_MEMORY("hello mesh", in.data.payload, 10);
  TEST_ASSERT_EQUAL_UINT8(3, in.h.hopLimit);
  TEST_ASSERT_EQUAL_UINT8(3, in.h.hopStart);
  TEST_ASSERT_EQUAL_HEX8(0x08, in.h.channel);
  TEST_ASSERT_FALSE(in.h.wantAck);
  TEST_ASSERT_EQUAL_HEX8(0xD4, in.h.relayNode);
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, in.h.to);
}

void test_text_too_long_returns_zero() {
  uint8_t f[256]; char big[400]; memset(big, 'x', 399); big[399] = 0;
  TEST_ASSERT_EQUAL_UINT(0, buildBroadcastText(A, 1, big, f));
}

void test_nodeinfo_roundtrip() {
  uint8_t f[256]; Inbound in;
  size_t n = buildNodeInfo(A, 0x99, 0xFFFFFFFF, true, f);
  TEST_ASSERT_TRUE(n > 16);
  TEST_ASSERT_TRUE(parseFrame(B, f, n, lookupNone, in));
  TEST_ASSERT_TRUE(in.kind == InKind::NodeInfo);
  TEST_ASSERT_TRUE(in.data.wantResponse);
  TEST_ASSERT_EQUAL_STRING("!a1b2c3d4", in.user.id);
  TEST_ASSERT_EQUAL_STRING("4stro-031", in.user.longName);
  TEST_ASSERT_EQUAL_STRING("4S31", in.user.shortName);
  TEST_ASSERT_EQUAL_UINT32(112, in.user.hwModel);
  TEST_ASSERT_EQUAL_UINT32(1, in.user.role);
  TEST_ASSERT_EQUAL_UINT(32, in.user.publicKeyLen);
  TEST_ASSERT_EQUAL_MEMORY(X25519_PUB_A, in.user.publicKey, 32);
  TEST_ASSERT_EQUAL_HEX8(0x08, in.h.channel);
  // unicast nodeinfo still uses the channel key
  n = buildNodeInfo(A, 0x9A, B.nodeNum, false, f);
  TEST_ASSERT_TRUE(parseFrame(B, f, n, lookupNone, in));
  TEST_ASSERT_TRUE(in.kind == InKind::NodeInfo);
  TEST_ASSERT_FALSE(in.pki);
}

void test_direct_text_roundtrip_pki() {
  uint8_t f[256]; Inbound in;
  size_t n = buildDirectText(A, 0x5555, B.nodeNum, B.pub, 0xCAFEBABE, "psst", f);
  TEST_ASSERT_TRUE(n > 16);
  TEST_ASSERT_TRUE(parseFrame(B, f, n, lookupA, in));
  TEST_ASSERT_TRUE(in.kind == InKind::Text);
  TEST_ASSERT_TRUE(in.direct);
  TEST_ASSERT_TRUE(in.pki);
  TEST_ASSERT_TRUE(in.h.wantAck);
  TEST_ASSERT_EQUAL_HEX8(0x00, in.h.channel);
  TEST_ASSERT_EQUAL_UINT(4, in.data.payloadLen);
  TEST_ASSERT_EQUAL_MEMORY("psst", in.data.payload, 4);
  // unknown sender key -> cannot decrypt
  TEST_ASSERT_FALSE(parseFrame(B, f, n, lookupNone, in));
}

void test_ack_roundtrip() {
  uint8_t f[256]; Inbound in;
  size_t n = buildAck(B, 0x77, A.nodeNum, 0x5555, f);
  TEST_ASSERT_TRUE(n > 16);
  TEST_ASSERT_TRUE(parseFrame(A, f, n, lookupNone, in));
  TEST_ASSERT_TRUE(in.kind == InKind::Ack);
  TEST_ASSERT_EQUAL_HEX32(0x5555, in.ackedId);
  TEST_ASSERT_FALSE(in.h.wantAck);
  TEST_ASSERT_EQUAL_HEX8(0x08, in.h.channel);
}

void test_parse_rejects_garbage() {
  uint8_t f[400]; Inbound in;
  memset(f, 0xA5, sizeof f);
  TEST_ASSERT_FALSE(parseFrame(B, f, 10, lookupNone, in));
  TEST_ASSERT_FALSE(parseFrame(B, f, 300, lookupNone, in));
  // valid header + random payload on channel key
  PacketHeader h{}; h.to = 0xFFFFFFFF; h.from = 0x01020304; h.id = 7; h.hopLimit = 3; h.hopStart = 3; h.channel = 0x08;
  headerWrite(h, f);
  for (int i = 16; i < 60; i++) f[i] = (uint8_t)(i * 37 + 11);
  parseFrame(B, f, 60, lookupNone, in);  // must not crash
  // PKI with wrong key (sender claimed as A but lookup gives B's pub)
  uint8_t g[256];
  size_t n = buildDirectText(A, 0x5555, B.nodeNum, B.pub, 1, "x", g);
  auto lookupWrong = [](uint32_t) -> const uint8_t* { return X25519_PUB_B; };
  TEST_ASSERT_FALSE(parseFrame(B, g, n, lookupWrong, in));
  // PKI frame truncated below tag size
  TEST_ASSERT_FALSE(parseFrame(B, g, 16 + 5, lookupA, in));
  // flipped tag byte
  g[n - 6] ^= 1;
  TEST_ASSERT_FALSE(parseFrame(B, g, n, lookupA, in));
}

void test_ignores_own_and_duplicate() {
  uint8_t f[256]; Inbound in;
  size_t n = buildBroadcastText(A, 0x1234, "me", f);
  TEST_ASSERT_FALSE(parseFrame(A, f, n, lookupNone, in));
  DedupCache d;
  TEST_ASSERT_FALSE(d.seen(1, 2));
  TEST_ASSERT_TRUE(d.seen(1, 2));
  TEST_ASSERT_FALSE(d.seen(2, 2));
  for (uint32_t i = 0; i < 70; i++) d.seen(100, i);
  TEST_ASSERT_FALSE(d.seen(1, 2));  // evicted from the 64-entry ring
  TEST_ASSERT_TRUE(d.seen(100, 69));
}

void test_legacy_text_dm_rejected() {
  // text to B over the channel key, hand-built
  MeshData d; d.portnum = 1; memcpy(d.payload, "dm", 2); d.payloadLen = 2;
  uint8_t pl[64]; size_t pn = pbEncodeData(d, pl, sizeof pl);
  uint8_t f[128];
  PacketHeader h{}; h.to = B.nodeNum; h.from = A.nodeNum; h.id = 42; h.hopLimit = 3; h.hopStart = 3; h.channel = 0x08;
  headerWrite(h, f);
  memcpy(f + 16, pl, pn);
  ctrCrypt(kDefaultKey, h.id, h.from, f + 16, pn);
  Inbound in;
  TEST_ASSERT_FALSE(parseFrame(B, f, 16 + pn, lookupA, in));
}

void test_packet_id_nonzero_and_counter() {
  uint32_t a = nextPacketId(5), b = nextPacketId(5);
  TEST_ASSERT_TRUE(a != 0 && b != 0);
  TEST_ASSERT_TRUE(a != b);
  TEST_ASSERT_EQUAL_HEX32(5, a >> 10);
  TEST_ASSERT_EQUAL_UINT32(1, (b & 0x3FF) - (a & 0x3FF) + ((b & 0x3FF) < (a & 0x3FF) ? 1024 : 0));
  TEST_ASSERT_TRUE(nextPacketId(0) != 0);
  for (int i = 0; i < 3000; i++) TEST_ASSERT_TRUE(nextPacketId(0) != 0);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_text_broadcast_roundtrip);
  RUN_TEST(test_text_too_long_returns_zero);
  RUN_TEST(test_nodeinfo_roundtrip);
  RUN_TEST(test_direct_text_roundtrip_pki);
  RUN_TEST(test_ack_roundtrip);
  RUN_TEST(test_parse_rejects_garbage);
  RUN_TEST(test_ignores_own_and_duplicate);
  RUN_TEST(test_legacy_text_dm_rejected);
  RUN_TEST(test_packet_id_nonzero_and_counter);
  return UNITY_END();
}
