// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>
#include <unity.h>
#include "mesh/crypto.h"
#include "mesh/region.h"
#include "mesh/secure_zero.h"
#include "../vectors.h"

static const uint32_t kId = 0x12345678u;
static const uint32_t kFrom = 0xA1B2C3D4u;
static const uint32_t kExtra = 0xCAFEBABEu;

void setUp() {}
void tearDown() {}

void test_ctr_vector() {
  uint8_t buf[40];
  memcpy(buf, CTR_PLAIN, CTR_PLAIN_LEN);
  ctrCrypt(kDefaultKey, kId, kFrom, buf, CTR_PLAIN_LEN);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(CTR_CIPHER, buf, CTR_CIPHER_LEN);
}

void test_ctr_roundtrip() {
  uint8_t buf[40];
  memcpy(buf, CTR_CIPHER, CTR_CIPHER_LEN);
  ctrCrypt(kDefaultKey, kId, kFrom, buf, CTR_CIPHER_LEN);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(CTR_PLAIN, buf, CTR_PLAIN_LEN);
}

void test_x25519_vector() {
  uint8_t ka[32], kb[32];
  TEST_ASSERT_TRUE(x25519Shared(X25519_PRIV_A, X25519_PUB_B, ka));
  TEST_ASSERT_TRUE(x25519Shared(X25519_PRIV_B, X25519_PUB_A, kb));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(ka, kb, 32);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(AES256_KEY, ka, 32);
}

static void rngPrivA(uint8_t* buf, size_t len) { memcpy(buf, X25519_PRIV_A, len); }

void test_x25519_keypair_clamps_and_derives_pub() {
  uint8_t priv[32], pub[32];
  x25519Keypair(rngPrivA, priv, pub);
  TEST_ASSERT_EQUAL_HEX8(X25519_PRIV_A[0] & 248, priv[0]);
  TEST_ASSERT_EQUAL_HEX8((X25519_PRIV_A[31] & 127) | 64, priv[31]);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(X25519_PRIV_A + 1, priv + 1, 30);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(X25519_PUB_A, pub, 32);
}

void test_ccm_vector() {
  uint8_t wire[64];
  size_t n = pkiEncrypt(AES256_KEY, kId, kFrom, kExtra, CCM_PLAIN, CCM_PLAIN_LEN, wire);
  TEST_ASSERT_EQUAL_UINT32(CCM_WIRE_LEN, n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(CCM_CIPHER, wire, CCM_CIPHER_LEN);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(CCM_TAG, wire + CCM_CIPHER_LEN, CCM_TAG_LEN);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(CCM_WIRE, wire, CCM_WIRE_LEN);

  uint8_t plain[64];
  int m = pkiDecrypt(AES256_KEY, kId, kFrom, CCM_WIRE, CCM_WIRE_LEN, plain);
  TEST_ASSERT_EQUAL_INT((int)CCM_PLAIN_LEN, m);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(CCM_PLAIN, plain, CCM_PLAIN_LEN);
}

void test_ccm_rejects_tamper() {
  uint8_t plain[64];
  for (size_t i = 0; i < CCM_WIRE_LEN; i++) {
    uint8_t wire[64];
    memcpy(wire, CCM_WIRE, CCM_WIRE_LEN);
    wire[i] ^= 0x01;
    TEST_ASSERT_EQUAL_INT(-1, pkiDecrypt(AES256_KEY, kId, kFrom, wire, CCM_WIRE_LEN, plain));
  }
  // Wrong sender / packet id also changes the nonce.
  TEST_ASSERT_EQUAL_INT(-1, pkiDecrypt(AES256_KEY, kId, kFrom ^ 1, CCM_WIRE, CCM_WIRE_LEN, plain));
  TEST_ASSERT_EQUAL_INT(-1, pkiDecrypt(AES256_KEY, kId ^ 1, kFrom, CCM_WIRE, CCM_WIRE_LEN, plain));
}

void test_ccm_rejects_short() {
  uint8_t plain[16];
  for (size_t n = 0; n < 12; n++)
    TEST_ASSERT_EQUAL_INT(-1, pkiDecrypt(AES256_KEY, kId, kFrom, CCM_WIRE, n, plain));
}

void test_ccm_empty_roundtrip() {
  uint8_t wire[16], plain[4];
  TEST_ASSERT_EQUAL_UINT32(12, pkiEncrypt(AES256_KEY, kId, kFrom, kExtra, nullptr, 0, wire));
  TEST_ASSERT_EQUAL_INT(0, pkiDecrypt(AES256_KEY, kId, kFrom, wire, 12, plain));
}

// wireLen == 12 means an empty plaintext: a valid tag yields 0, a bad one -1.
// Either way nothing may be written to out (wireLen - 12 == 0 bytes).
void test_pki_decrypt_len12_writes_nothing() {
  uint8_t wire[12];
  TEST_ASSERT_EQUAL_UINT32(12, pkiEncrypt(AES256_KEY, kId, kFrom, kExtra, nullptr, 0, wire));
  uint8_t out[8];
  memset(out, 0xA5, sizeof(out));
  TEST_ASSERT_EQUAL_INT(0, pkiDecrypt(AES256_KEY, kId, kFrom, wire, 12, out));
  for (size_t i = 0; i < sizeof(out); i++) TEST_ASSERT_EQUAL_HEX8(0xA5, out[i]);
  wire[0] ^= 0x80;  // corrupt the tag
  TEST_ASSERT_EQUAL_INT(-1, pkiDecrypt(AES256_KEY, kId, kFrom, wire, 12, out));
  for (size_t i = 0; i < sizeof(out); i++) TEST_ASSERT_EQUAL_HEX8(0xA5, out[i]);
}

// pkiDecrypt writes at most wireLen - 12 bytes, on success and on failure.
void test_pki_decrypt_never_writes_past_plain_len() {
  uint8_t out[64];
  memset(out, 0xA5, sizeof(out));
  TEST_ASSERT_EQUAL_INT((int)CCM_PLAIN_LEN, pkiDecrypt(AES256_KEY, kId, kFrom, CCM_WIRE, CCM_WIRE_LEN, out));
  for (size_t i = CCM_PLAIN_LEN; i < sizeof(out); i++) TEST_ASSERT_EQUAL_HEX8(0xA5, out[i]);
  uint8_t wire[64];
  memcpy(wire, CCM_WIRE, CCM_WIRE_LEN);
  wire[3] ^= 1;
  memset(out, 0xA5, sizeof(out));
  TEST_ASSERT_EQUAL_INT(-1, pkiDecrypt(AES256_KEY, kId, kFrom, wire, CCM_WIRE_LEN, out));
  for (size_t i = CCM_PLAIN_LEN; i < sizeof(out); i++) TEST_ASSERT_EQUAL_HEX8(0xA5, out[i]);
  for (size_t i = 0; i < CCM_PLAIN_LEN; i++) TEST_ASSERT_EQUAL_HEX8(0x00, out[i]);  // failed plaintext wiped
}

void test_secure_zero() {
  uint8_t buf[33];
  memset(buf, 0xFF, sizeof(buf));
  secureZero(buf + 1, 31);
  TEST_ASSERT_EQUAL_HEX8(0xFF, buf[0]);
  for (size_t i = 1; i < 32; i++) TEST_ASSERT_EQUAL_HEX8(0x00, buf[i]);
  TEST_ASSERT_EQUAL_HEX8(0xFF, buf[32]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_ctr_vector);
  RUN_TEST(test_ctr_roundtrip);
  RUN_TEST(test_x25519_vector);
  RUN_TEST(test_x25519_keypair_clamps_and_derives_pub);
  RUN_TEST(test_ccm_vector);
  RUN_TEST(test_ccm_rejects_tamper);
  RUN_TEST(test_ccm_rejects_short);
  RUN_TEST(test_ccm_empty_roundtrip);
  RUN_TEST(test_pki_decrypt_len12_writes_nothing);
  RUN_TEST(test_pki_decrypt_never_writes_past_plain_len);
  RUN_TEST(test_secure_zero);
  return UNITY_END();
}
