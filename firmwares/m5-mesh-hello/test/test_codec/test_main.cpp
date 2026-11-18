// SPDX-License-Identifier: GPL-3.0-or-later
#include <unity.h>
#include <string.h>
#include "mesh/header.h"
#include "mesh/pb.h"

void setUp() {}
void tearDown() {}

void test_header_roundtrip() {
  PacketHeader h{};
  h.to = 0xFFFFFFFF; h.from = 0xA1B2C3D4; h.id = 0x12345678;
  h.hopLimit = 3; h.hopStart = 3; h.wantAck = true; h.viaMqtt = false;
  h.channel = 0x08; h.nextHop = 0x11; h.relayNode = 0x22;
  uint8_t b[16];
  headerWrite(h, b);
  TEST_ASSERT_EQUAL_HEX8(0x6B, b[12]);
  TEST_ASSERT_EQUAL_HEX8(0xFF, b[0]);
  TEST_ASSERT_EQUAL_HEX8(0xD4, b[4]);
  TEST_ASSERT_EQUAL_HEX8(0x78, b[8]);
  PacketHeader o{};
  TEST_ASSERT_TRUE(headerParse(b, 16, o));
  TEST_ASSERT_EQUAL_HEX32(h.to, o.to);
  TEST_ASSERT_EQUAL_HEX32(h.from, o.from);
  TEST_ASSERT_EQUAL_HEX32(h.id, o.id);
  TEST_ASSERT_EQUAL_UINT8(3, o.hopLimit);
  TEST_ASSERT_EQUAL_UINT8(3, o.hopStart);
  TEST_ASSERT_TRUE(o.wantAck);
  TEST_ASSERT_FALSE(o.viaMqtt);
  TEST_ASSERT_EQUAL_HEX8(0x08, o.channel);
  TEST_ASSERT_EQUAL_HEX8(0x11, o.nextHop);
  TEST_ASSERT_EQUAL_HEX8(0x22, o.relayNode);
}

void test_header_short_rejected() {
  uint8_t b[16] = {0};
  PacketHeader o{};
  TEST_ASSERT_FALSE(headerParse(b, 15, o));
}

void test_data_roundtrip_text() {
  MeshData d;
  d.portnum = 1;
  memcpy(d.payload, "hello", 5); d.payloadLen = 5;
  d.wantResponse = true; d.dest = 0xAABBCCDD; d.source = 1; d.requestId = 0x01020304; d.replyId = 7;
  uint8_t buf[64];
  size_t n = pbEncodeData(d, buf, sizeof buf);
  TEST_ASSERT_TRUE(n > 0);
  MeshData o;
  TEST_ASSERT_TRUE(pbDecodeData(buf, n, o));
  TEST_ASSERT_EQUAL_UINT32(1, o.portnum);
  TEST_ASSERT_EQUAL_UINT(5, o.payloadLen);
  TEST_ASSERT_EQUAL_MEMORY("hello", o.payload, 5);
  TEST_ASSERT_TRUE(o.wantResponse);
  TEST_ASSERT_EQUAL_HEX32(0xAABBCCDD, o.dest);
  TEST_ASSERT_EQUAL_UINT32(1, o.source);
  TEST_ASSERT_EQUAL_HEX32(0x01020304, o.requestId);
  TEST_ASSERT_EQUAL_UINT32(7, o.replyId);
  // overflow
  TEST_ASSERT_EQUAL_UINT(0, pbEncodeData(d, buf, 5));
}

void test_data_known_bytes() {
  MeshData d;
  d.portnum = 1; memcpy(d.payload, "hi", 2); d.payloadLen = 2;
  uint8_t buf[16];
  size_t n = pbEncodeData(d, buf, sizeof buf);
  const uint8_t exp[] = {0x08, 0x01, 0x12, 0x02, 0x68, 0x69};
  TEST_ASSERT_EQUAL_UINT(6, n);
  TEST_ASSERT_EQUAL_MEMORY(exp, buf, 6);
}

void test_data_skips_unknown_field() {
  const uint8_t in[] = {0x08, 0x01, 0x12, 0x02, 0x68, 0x69,
                        0x78, 0x05,                         // field 15 varint
                        0x85, 0x01, 1, 2, 3, 4,             // field 16 fixed32
                        0x91, 0x01, 1, 2, 3, 4, 5, 6, 7, 8, // field 18 fixed64
                        0x52, 0x03, 9, 9, 9};               // field 10 bytes
  MeshData o;
  TEST_ASSERT_TRUE(pbDecodeData(in, sizeof in, o));
  TEST_ASSERT_EQUAL_UINT32(1, o.portnum);
  TEST_ASSERT_EQUAL_UINT(2, o.payloadLen);
}

void test_data_malformed() {
  const uint8_t trunc[] = {0x08, 0x01, 0x12, 0x05, 0x68, 0x69};
  MeshData o;
  TEST_ASSERT_FALSE(pbDecodeData(trunc, sizeof trunc, o));
  const uint8_t trunc32[] = {0x25, 1, 2};
  TEST_ASSERT_FALSE(pbDecodeData(trunc32, sizeof trunc32, o));
  const uint8_t badvar[] = {0x08, 0x80};
  TEST_ASSERT_FALSE(pbDecodeData(badvar, sizeof badvar, o));
  // payload larger than 233
  uint8_t big[240]; big[0] = 0x12; big[1] = 0xEC; big[2] = 0x01; memset(big + 3, 0, 236);
  TEST_ASSERT_FALSE(pbDecodeData(big, 239, o));
}

void test_user_roundtrip_with_key() {
  MeshUser u;
  memset(&u, 0, sizeof u);
  strcpy(u.id, "!a1b2c3d4"); strcpy(u.longName, "4stro-031"); strcpy(u.shortName, "4S31");
  u.hwModel = 112; u.role = 1;
  for (int i = 0; i < 32; i++) u.publicKey[i] = (uint8_t)i;
  u.publicKeyLen = 32;
  uint8_t buf[128];
  size_t n = pbEncodeUser(u, buf, sizeof buf);
  TEST_ASSERT_TRUE(n > 0);
  MeshUser o;
  TEST_ASSERT_TRUE(pbDecodeUser(buf, n, o));
  TEST_ASSERT_EQUAL_STRING("!a1b2c3d4", o.id);
  TEST_ASSERT_EQUAL_STRING("4stro-031", o.longName);
  TEST_ASSERT_EQUAL_STRING("4S31", o.shortName);
  TEST_ASSERT_EQUAL_UINT32(112, o.hwModel);
  TEST_ASSERT_EQUAL_UINT32(1, o.role);
  TEST_ASSERT_EQUAL_UINT(32, o.publicKeyLen);
  TEST_ASSERT_EQUAL_MEMORY(u.publicKey, o.publicKey, 32);
  TEST_ASSERT_EQUAL_UINT(0, pbEncodeUser(u, buf, 10));
}

void test_user_longname_truncated_utf8() {
  MeshUser u;
  memset(&u, 0, sizeof u);
  // 11 x 'a' then 7 x 2-byte chars = 25 bytes... build 23 'a' + 'é'(2) = 25 bytes
  memset(u.longName, 'a', 23); u.longName[23] = (char)0xC3; u.longName[24] = (char)0xA9;
  uint8_t buf[64];
  size_t n = pbEncodeUser(u, buf, sizeof buf);
  MeshUser o;
  TEST_ASSERT_TRUE(pbDecodeUser(buf, n, o));
  TEST_ASSERT_EQUAL_UINT(23, strlen(o.longName));
}

void test_user_decode_skips_and_truncates() {
  // field 4 macaddr (6 bytes) is skipped; a 10-char short name is truncated to the 8-byte buffer (7 chars)
  uint8_t in[64]; size_t i = 0;
  in[i++] = 0x22; in[i++] = 6; for (int k = 0; k < 6; k++) in[i++] = 0xEE;
  in[i++] = 0x1A; in[i++] = 10; for (int k = 0; k < 10; k++) in[i++] = 'x';
  MeshUser o;
  TEST_ASSERT_TRUE(pbDecodeUser(in, i, o));
  TEST_ASSERT_EQUAL_UINT(7, strlen(o.shortName));
}

void test_routing_ack_roundtrip() {
  uint8_t buf[8];
  size_t n = pbEncodeRoutingAck(buf, sizeof buf);
  TEST_ASSERT_EQUAL_UINT(2, n);
  TEST_ASSERT_EQUAL_HEX8(0x18, buf[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00, buf[1]);
  uint32_t err = 99;
  TEST_ASSERT_TRUE(pbDecodeRoutingError(buf, n, err));
  TEST_ASSERT_EQUAL_UINT32(0, err);
  const uint8_t e5[] = {0x18, 0x05};
  TEST_ASSERT_TRUE(pbDecodeRoutingError(e5, 2, err));
  TEST_ASSERT_EQUAL_UINT32(5, err);
  TEST_ASSERT_EQUAL_UINT(0, pbEncodeRoutingAck(buf, 1));
}

void test_routing_absent_is_none() {
  uint32_t err = 99;
  TEST_ASSERT_TRUE(pbDecodeRoutingError(nullptr, 0, err));
  TEST_ASSERT_EQUAL_UINT32(0, err);
  err = 99;
  const uint8_t unk[] = {0x78, 0x05, 0x52, 0x02, 1, 2};  // unknown fields only
  TEST_ASSERT_TRUE(pbDecodeRoutingError(unk, sizeof unk, err));
  TEST_ASSERT_EQUAL_UINT32(0, err);
  const uint8_t bad[] = {0x18};
  TEST_ASSERT_FALSE(pbDecodeRoutingError(bad, sizeof bad, err));
}

void test_user_bad_key_length_rejected() {
  uint8_t in[8] = {0x42, 0x03, 1, 2, 3};  // field 8, 3 bytes
  MeshUser o;
  TEST_ASSERT_FALSE(pbDecodeUser(in, 5, o));
  const uint8_t none[] = {0x28, 0x70};
  TEST_ASSERT_TRUE(pbDecodeUser(none, sizeof none, o));
  TEST_ASSERT_EQUAL_UINT(0, o.publicKeyLen);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_header_roundtrip);
  RUN_TEST(test_header_short_rejected);
  RUN_TEST(test_data_roundtrip_text);
  RUN_TEST(test_data_known_bytes);
  RUN_TEST(test_data_skips_unknown_field);
  RUN_TEST(test_data_malformed);
  RUN_TEST(test_user_roundtrip_with_key);
  RUN_TEST(test_user_longname_truncated_utf8);
  RUN_TEST(test_user_decode_skips_and_truncates);
  RUN_TEST(test_routing_ack_roundtrip);
  RUN_TEST(test_routing_absent_is_none);
  RUN_TEST(test_user_bad_key_length_rejected);
  return UNITY_END();
}
