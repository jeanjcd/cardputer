#include "crypto.h"

#include <AES.h>
#include <CTR.h>
#include <Curve25519.h>
#include <SHA256.h>
#include <string.h>

#include "aes_ccm.h"

namespace {

constexpr size_t kTagLen = 8;
constexpr size_t kPkiOverhead = kTagLen + 4;  // tag + extraNonce

void putLe32(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}

uint32_t getLe32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void clamp(uint8_t k[32]) {
  k[0] &= 248;
  k[31] &= 127;
  k[31] |= 64;
}

// 13-byte CCM nonce: id_low32 LE | extraNonce LE | from LE | 0x00.
void pkiNonce(uint32_t id, uint32_t extraNonce, uint32_t from, uint8_t nonce[13]) {
  putLe32(nonce, id);
  putLe32(nonce + 4, extraNonce);
  putLe32(nonce + 8, from);
  nonce[12] = 0;
}

}  // namespace

void ctrCrypt(const uint8_t key[16], uint32_t id, uint32_t from, uint8_t* buf, size_t len) {
  uint8_t iv[16] = {0};
  putLe32(iv, id);  // packet id as u64 LE: high 32 bits are zero
  putLe32(iv + 8, from);
  CTR<AES128> ctr;
  ctr.setKey(key, 16);
  ctr.setCounterSize(4);
  ctr.setIV(iv, sizeof(iv));
  ctr.encrypt(buf, buf, len);
  ctr.clear();
}

void x25519Keypair(void (*rng)(uint8_t* buf, size_t len), uint8_t priv[32], uint8_t pub[32]) {
  rng(priv, 32);
  clamp(priv);
  Curve25519::eval(pub, priv, nullptr);
}

bool x25519Shared(const uint8_t priv[32], const uint8_t peerPub[32], uint8_t outKey[32]) {
  uint8_t k[32], f[32];
  memcpy(k, peerPub, 32);
  memcpy(f, priv, 32);
  clamp(f);  // eval does not clamp; no-op for keys made by x25519Keypair
  bool ok = Curve25519::dh2(k, f);  // k = X25519(f, k); wipes f
  if (ok) {
    SHA256 sha;
    sha.update(k, 32);
    sha.finalize(outKey, 32);
  }
  memset(k, 0, sizeof(k));
  return ok;
}

size_t pkiEncrypt(const uint8_t key[32], uint32_t id, uint32_t from, uint32_t extraNonce,
                  const uint8_t* plain, size_t len, uint8_t* out) {
  uint8_t nonce[13];
  pkiNonce(id, extraNonce, from, nonce);
  if (!aesCcmEncrypt(key, nonce, kTagLen, plain, len, out, out + len)) return 0;
  putLe32(out + len + kTagLen, extraNonce);
  return len + kPkiOverhead;
}

int pkiDecrypt(const uint8_t key[32], uint32_t id, uint32_t from, const uint8_t* wire, size_t wireLen,
               uint8_t* out) {
  if (wireLen < kPkiOverhead) return -1;
  size_t len = wireLen - kPkiOverhead;
  uint32_t extraNonce = getLe32(wire + wireLen - 4);
  uint8_t nonce[13];
  pkiNonce(id, extraNonce, from, nonce);
  if (!aesCcmDecrypt(key, nonce, kTagLen, wire, len, wire + len, out)) return -1;
  return (int)len;
}
