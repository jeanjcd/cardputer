// AES-CCM on top of rweather AES256. The block layout and processing order
// mirror Meshtastic's src/mesh/aes-ccm.cpp (itself derived from wpa_supplicant,
// Copyright (c) 2010-2012 Jouni Malinen, BSD license), restricted to L = 2 and
// no associated data. Rewritten here so input and output buffers may alias.
#include "aes_ccm.h"

#include <AES.h>
#include <string.h>

namespace {

constexpr size_t kBlock = 16;
constexpr size_t kL = 2;
constexpr size_t kNonceLen = 15 - kL;  // 13

void putBe16(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)(v >> 8);
  p[1] = (uint8_t)(v & 0xff);
}

bool paramsOk(size_t M, size_t len) {
  return M >= 4 && M <= 16 && (M % 2) == 0 && len <= 0xFFFF;
}

// CBC-MAC over B_0 | message blocks (zero padded). Result in x.
void cbcMac(AES256& aes, const uint8_t* nonce, size_t M, const uint8_t* msg, size_t len,
            uint8_t x[kBlock]) {
  uint8_t b[kBlock];
  b[0] = (uint8_t)((((M - 2) / 2) << 3) | (kL - 1));  // no Adata flag
  memcpy(&b[1], nonce, kNonceLen);
  putBe16(&b[kBlock - kL], (uint16_t)len);
  aes.encryptBlock(x, b);  // X_1 = E(K, B_0)
  for (size_t off = 0; off < len; off += kBlock) {
    size_t n = len - off < kBlock ? len - off : kBlock;
    for (size_t i = 0; i < n; i++) x[i] ^= msg[off + i];
    aes.encryptBlock(x, x);
  }
}

// A_i = Flags(L-1) | Nonce | counter i (big endian, 2 bytes).
void counterBlock(const uint8_t* nonce, uint16_t i, uint8_t a[kBlock]) {
  a[0] = (uint8_t)(kL - 1);
  memcpy(&a[1], nonce, kNonceLen);
  putBe16(&a[kBlock - kL], i);
}

// out = in XOR (S_1 | S_2 | ...); in and out may alias.
void ctrXor(AES256& aes, const uint8_t* nonce, const uint8_t* in, size_t len, uint8_t* out) {
  uint8_t a[kBlock], s[kBlock];
  uint16_t ctr = 1;
  for (size_t off = 0; off < len; off += kBlock, ctr++) {
    counterBlock(nonce, ctr, a);
    aes.encryptBlock(s, a);
    size_t n = len - off < kBlock ? len - off : kBlock;
    for (size_t i = 0; i < n; i++) out[off + i] = in[off + i] ^ s[i];
  }
}

// S_0 = E(K, A_0), used to mask the tag.
void s0(AES256& aes, const uint8_t* nonce, uint8_t out[kBlock]) {
  uint8_t a[kBlock];
  counterBlock(nonce, 0, a);
  aes.encryptBlock(out, a);
}

// Returns 0 when equal, without data-dependent branches.
uint8_t ctDiff(const uint8_t* a, const uint8_t* b, size_t len) {
  volatile uint8_t d = 0;
  for (size_t i = 0; i < len; i++) d |= (uint8_t)(a[i] ^ b[i]);
  return d;
}

}  // namespace

bool aesCcmEncrypt(const uint8_t key[32], const uint8_t nonce[13], size_t M,
                   const uint8_t* plain, size_t len, uint8_t* crypt, uint8_t* tag) {
  if (!paramsOk(M, len)) return false;
  AES256 aes;
  aes.setKey(key, 32);
  uint8_t x[kBlock], s[kBlock];
  cbcMac(aes, nonce, M, plain, len, x);  // MAC over plaintext before it may be overwritten
  ctrXor(aes, nonce, plain, len, crypt);
  s0(aes, nonce, s);
  for (size_t i = 0; i < M; i++) tag[i] = x[i] ^ s[i];
  aes.clear();
  return true;
}

bool aesCcmDecrypt(const uint8_t key[32], const uint8_t nonce[13], size_t M,
                   const uint8_t* crypt, size_t len, const uint8_t* tag, uint8_t* plain) {
  if (!paramsOk(M, len)) return false;
  AES256 aes;
  aes.setKey(key, 32);
  uint8_t t[kBlock], x[kBlock], s[kBlock];
  s0(aes, nonce, s);
  for (size_t i = 0; i < M; i++) t[i] = tag[i] ^ s[i];
  ctrXor(aes, nonce, crypt, len, plain);
  cbcMac(aes, nonce, M, plain, len, x);
  aes.clear();
  if (ctDiff(x, t, M) != 0) {
    memset(plain, 0, len);
    return false;
  }
  return true;
}
