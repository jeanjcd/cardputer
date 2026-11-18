// SPDX-License-Identifier: GPL-3.0-or-later
// Fuzz-style robustness test for the RX parse path. Seeded random and mutated
// valid frames go through headerParse, pbDecode*, pkiDecrypt and parseFrame.
// Each input is copied into an exactly-sized heap buffer so AddressSanitizer
// (env native_sanitize) flags any read past the end. Iterations default to
// 200000; override with the M5MH_FUZZ_ITERS environment variable.
#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mesh/crypto.h"
#include "mesh/header.h"
#include "mesh/packet.h"
#include "mesh/pb.h"
#include "../vectors.h"

#ifndef FUZZ_DEFAULT_ITERS
#define FUZZ_DEFAULT_ITERS 200000UL
#endif

namespace {

constexpr size_t kMaxIn = 300;  // a bit beyond the 16 + 239 byte LoRa frame limit
constexpr uint8_t kCanary = 0xA5;

Identity A, B;
uint64_t rngState;

/// xorshift64* PRNG with a fixed seed: runs are reproducible.
uint64_t rnd() {
  rngState ^= rngState >> 12;
  rngState ^= rngState << 25;
  rngState ^= rngState >> 27;
  return rngState * 2685821657736338717ULL;
}
uint32_t rndBelow(uint32_t n) { return n ? (uint32_t)(rnd() % n) : 0; }

unsigned long iterations() {
  const char* s = getenv("M5MH_FUZZ_ITERS");
  if (s && *s) {
    unsigned long v = strtoul(s, nullptr, 10);
    if (v > 0) return v;
  }
  return FUZZ_DEFAULT_ITERS;
}

const uint8_t* lookupAny(uint32_t) { return X25519_PUB_A; }
const uint8_t* lookupNone(uint32_t) { return nullptr; }

// Seed corpus: valid frames (A -> B) and valid protobuf payloads.
struct Seed {
  uint8_t b[kMaxIn];
  size_t n;
};
Seed frames[8];
size_t nFrames;
Seed pbs[6];
size_t nPbs;

void addFrame(const uint8_t* p, size_t n) {
  TEST_ASSERT_TRUE(n > 0 && n <= kMaxIn);
  memcpy(frames[nFrames].b, p, n);
  frames[nFrames++].n = n;
}
void addPb(const uint8_t* p, size_t n) {
  TEST_ASSERT_TRUE(n > 0 && n <= kMaxIn);
  memcpy(pbs[nPbs].b, p, n);
  pbs[nPbs++].n = n;
}

void buildCorpus() {
  uint8_t f[256];
  nFrames = nPbs = 0;
  addFrame(f, buildBroadcastText(A, 0x1001, "hello mesh fuzz", f));
  addFrame(f, buildNodeInfo(A, 0x1002, 0xFFFFFFFF, true, f));
  addFrame(f, buildNodeInfo(A, 0x1003, B.nodeNum, false, f));
  addFrame(f, buildAck(A, 0x1004, B.nodeNum, 0x77, f));
  addFrame(f, buildRoutingError(A, 0x1005, B.nodeNum, 0x78, kRoutingPkiUnknownPubkey, f));
  addFrame(f, buildDirectText(A, 0x1006, B.nodeNum, B.pub, 0xCAFEBABE, "secret hi", f));

  MeshData d;
  d.portnum = 1;
  memcpy(d.payload, "abc", 3);
  d.payloadLen = 3;
  d.wantResponse = true;
  d.dest = 1; d.source = 2; d.requestId = 3; d.replyId = 4;
  addPb(f, pbEncodeData(d, f, sizeof f));
  MeshUser u;
  memset(&u, 0, sizeof u);
  strncpy(u.id, "!a1b2c3d4", sizeof u.id - 1);
  strncpy(u.longName, "4stro-031", sizeof u.longName - 1);
  strncpy(u.shortName, "4S31", sizeof u.shortName - 1);
  u.hwModel = 112; u.role = 1;
  memcpy(u.publicKey, X25519_PUB_A, 32);
  u.publicKeyLen = 32;
  addPb(f, pbEncodeUser(u, f, sizeof f));
  addPb(f, pbEncodeRoutingAck(f, sizeof f));
  addPb(f, pbEncodeRoutingError(kRoutingPkiUnknownPubkey, f, sizeof f));
}

/// Produces a random buffer, or a seed with 1-8 random mutations. Returns length.
size_t makeInput(const Seed* seeds, size_t nSeeds, uint8_t* out) {
  size_t n;
  if (rndBelow(8) == 0) {  // pure random
    n = rndBelow(kMaxIn + 1);
    for (size_t i = 0; i < n; i++) out[i] = (uint8_t)rnd();
    return n;
  }
  const Seed& s = seeds[rndBelow((uint32_t)nSeeds)];
  memcpy(out, s.b, s.n);
  n = s.n;
  uint32_t muts = 1 + rndBelow(8);
  for (uint32_t m = 0; m < muts; m++) {
    switch (rndBelow(7)) {
      case 0:  // flip one bit
        if (n) out[rndBelow((uint32_t)n)] ^= (uint8_t)(1u << rndBelow(8));
        break;
      case 1:  // overwrite a byte
        if (n) out[rndBelow((uint32_t)n)] = (uint8_t)rnd();
        break;
      case 2:  // interesting byte (varint edges, lengths)
        if (n) {
          static const uint8_t v[] = {0x00, 0x01, 0x7F, 0x80, 0xFF, 0x0A, 0x12, 0x1A, 0x22, 0x08};
          out[rndBelow((uint32_t)n)] = v[rndBelow(sizeof v)];
        }
        break;
      case 3:  // insert a byte
        if (n < kMaxIn) {
          size_t at = rndBelow((uint32_t)n + 1);
          memmove(out + at + 1, out + at, n - at);
          out[at] = (uint8_t)rnd();
          n++;
        }
        break;
      case 4:  // delete a byte
        if (n) {
          size_t at = rndBelow((uint32_t)n);
          memmove(out + at, out + at + 1, n - at - 1);
          n--;
        }
        break;
      case 5:  // truncate
        n = rndBelow((uint32_t)n + 1);
        break;
      default:  // append random tail
        while (n < kMaxIn && rndBelow(4) != 0) out[n++] = (uint8_t)rnd();
        break;
    }
  }
  return n;
}

/// Exactly-sized heap copy so ASan catches over-reads; never NULL.
uint8_t* exactCopy(const uint8_t* src, size_t n) {
  uint8_t* p = static_cast<uint8_t*>(malloc(n ? n : 1));
  TEST_ASSERT_NOT_NULL(p);
  if (n) memcpy(p, src, n);
  return p;
}

bool terminated(const char* s, size_t cap) { return memchr(s, 0, cap) != nullptr; }

void checkData(const MeshData& d) { TEST_ASSERT_TRUE(d.payloadLen <= sizeof d.payload); }

void checkUser(const MeshUser& u) {
  TEST_ASSERT_TRUE(terminated(u.id, sizeof u.id));
  TEST_ASSERT_TRUE(terminated(u.longName, sizeof u.longName));
  TEST_ASSERT_TRUE(terminated(u.shortName, sizeof u.shortName));
  TEST_ASSERT_TRUE(u.publicKeyLen == 0 || u.publicKeyLen == 32);
}

// Output structs followed by a canary block that decoders must never touch.
struct GuardedData { MeshData d; uint8_t canary[64]; };
struct GuardedUser { MeshUser u; uint8_t canary[64]; };
struct GuardedInbound { Inbound in; uint8_t canary[64]; };
struct GuardedPlain { uint8_t buf[kMaxIn]; uint8_t canary[64]; };

void checkCanary(const uint8_t* c, size_t n) {
  for (size_t i = 0; i < n; i++) TEST_ASSERT_EQUAL_HEX8(kCanary, c[i]);
}

}  // namespace

void setUp() {
  memset(&A, 0, sizeof A);
  memset(&B, 0, sizeof B);
  A.nodeNum = 0xA1B2C3D4;
  B.nodeNum = 0x11223344;
  memcpy(A.priv, X25519_PRIV_A, 32); memcpy(A.pub, X25519_PUB_A, 32);
  memcpy(B.priv, X25519_PRIV_B, 32); memcpy(B.pub, X25519_PUB_B, 32);
  strncpy(A.longName, "4stro-031", sizeof A.longName - 1);
  strncpy(A.shortName, "4S31", sizeof A.shortName - 1);
  strncpy(B.longName, "Bravo", sizeof B.longName - 1);
  strncpy(B.shortName, "BRV", sizeof B.shortName - 1);
  rngState = 0x4D354D4553480001ULL;  // fixed seed
}
void tearDown() {}

void test_corpus_seeds_parse() {
  buildCorpus();
  TEST_ASSERT_EQUAL_UINT(6, nFrames);
  TEST_ASSERT_EQUAL_UINT(4, nPbs);
  for (size_t i = 0; i < nFrames; i++) {
    Inbound in;
    TEST_ASSERT_TRUE(parseFrame(B, frames[i].b, frames[i].n, lookupAny, in));
  }
}

void test_fuzz_decoders() {
  buildCorpus();
  const unsigned long iters = iterations();
  uint8_t tmp[kMaxIn];
  for (unsigned long it = 0; it < iters; it++) {
    size_t n = makeInput(pbs, nPbs, tmp);
    uint8_t* in = exactCopy(tmp, n);

    GuardedData gd;
    memset(&gd, kCanary, sizeof gd);
    gd.d = MeshData();
    if (pbDecodeData(in, n, gd.d)) checkData(gd.d);
    TEST_ASSERT_TRUE(gd.d.payloadLen <= sizeof gd.d.payload);
    checkCanary(gd.canary, sizeof gd.canary);

    GuardedUser gu;
    memset(&gu, kCanary, sizeof gu);
    gu.u = MeshUser();
    if (pbDecodeUser(in, n, gu.u)) checkUser(gu.u);
    checkCanary(gu.canary, sizeof gu.canary);

    uint32_t err = 0;
    (void)pbDecodeRoutingError(in, n, err);

    PacketHeader h;
    bool ok = headerParse(in, n, h);
    TEST_ASSERT_EQUAL(n >= 16, ok);
    if (ok) {
      TEST_ASSERT_TRUE(h.hopLimit <= 7);
      TEST_ASSERT_TRUE(h.hopStart <= 7);
    }

    free(in);
  }
}

void test_fuzz_pki_decrypt() {
  buildCorpus();
  const unsigned long iters = iterations();
  uint8_t key[32];
  TEST_ASSERT_TRUE(x25519Shared(B.priv, A.pub, key));
  uint8_t tmp[kMaxIn];
  const Seed& dm = frames[5];  // PKI direct text
  for (unsigned long it = 0; it < iters; it++) {
    Seed wire;
    wire.n = dm.n - 16;
    memcpy(wire.b, dm.b + 16, wire.n);
    size_t n = makeInput(&wire, 1, tmp);
    uint8_t* in = exactCopy(tmp, n);
    GuardedPlain gp;
    memset(&gp, kCanary, sizeof gp);
    int r = pkiDecrypt(key, 0x1006, A.nodeNum, in, n, gp.buf);
    TEST_ASSERT_TRUE(r >= -1);
    if (r >= 0) TEST_ASSERT_TRUE((size_t)r + 12 == n);
    checkCanary(gp.canary, sizeof gp.canary);
    free(in);
  }
}

void test_fuzz_parse_frame() {
  buildCorpus();
  const unsigned long iters = iterations();
  uint8_t tmp[kMaxIn];
  unsigned long accepted = 0, acceptedPki = 0;
  for (unsigned long it = 0; it < iters; it++) {
    size_t n = makeInput(frames, nFrames, tmp);
    uint8_t* in = exactCopy(tmp, n);

    GuardedInbound gi;
    memset(&gi, kCanary, sizeof gi);
    gi.in = Inbound();
    // X25519 is the expensive step, so the peer-key path runs on 1 in 8 inputs.
    bool withKey = (it % 8) == 0;
    bool ok = parseFrame(B, in, n, withKey ? lookupAny : lookupNone, gi.in);
    checkCanary(gi.canary, sizeof gi.canary);
    TEST_ASSERT_TRUE(gi.in.data.payloadLen <= sizeof gi.in.data.payload);
    if (ok) {
      accepted++;
      if (gi.in.pki) acceptedPki++;
      TEST_ASSERT_TRUE(n >= 16 && n <= 16 + 239);
      TEST_ASSERT_TRUE(gi.in.kind == InKind::Text || gi.in.kind == InKind::NodeInfo ||
                       gi.in.kind == InKind::Ack);
      TEST_ASSERT_TRUE(gi.in.h.from != B.nodeNum);
      if (gi.in.pki) TEST_ASSERT_TRUE(withKey);
      if (gi.in.kind == InKind::NodeInfo) checkUser(gi.in.user);
      if (gi.in.kind == InKind::Ack) TEST_ASSERT_TRUE(gi.in.ackedId != 0);
    }
    free(in);
  }
  // Mutations must not be so destructive that nothing gets through the parser.
  TEST_ASSERT_TRUE(accepted > 0);
  char msg[96];
  snprintf(msg, sizeof msg, "iterations=%lu accepted=%lu accepted_pki=%lu", iters, accepted, acceptedPki);
  TEST_MESSAGE(msg);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_corpus_seeds_parse);
  RUN_TEST(test_fuzz_decoders);
  RUN_TEST(test_fuzz_pki_decrypt);
  RUN_TEST(test_fuzz_parse_frame);
  return UNITY_END();
}
