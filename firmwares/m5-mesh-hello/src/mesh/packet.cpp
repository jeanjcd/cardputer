#include "packet.h"
#include <stdio.h>
#include <string.h>
#include "crypto.h"
#include "region.h"
#include "secure_zero.h"

namespace {
const uint32_t kBroadcast = 0xFFFFFFFF;
const size_t kHdr = 16;
const size_t kMaxPayload = 239;
const uint8_t kChannelHash = 0x08;
const uint32_t kPortText = 1, kPortNodeInfo = 4, kPortRouting = 5;

uint32_t g_counter = 0;

void fillHeader(const Identity& me, uint32_t id, uint32_t to, bool wantAck, uint8_t channel,
                uint8_t out[16]) {
  PacketHeader h{};
  h.to = to; h.from = me.nodeNum; h.id = id;
  h.hopLimit = 3; h.hopStart = 3; h.wantAck = wantAck; h.viaMqtt = false;
  h.channel = channel; h.nextHop = 0; h.relayNode = (uint8_t)(me.nodeNum & 0xFF);
  headerWrite(h, out);
}

// Encrypts `plain` with the channel key after a fresh header; returns frame length or 0.
size_t finishChannel(const Identity& me, uint32_t id, uint32_t to, bool wantAck, const MeshData& d,
                     uint8_t* out) {
  uint8_t plain[kMaxPayload];
  size_t n = pbEncodeData(d, plain, sizeof plain);
  if (n == 0 || n > kMaxPayload) return 0;
  fillHeader(me, id, to, wantAck, kChannelHash, out);
  memcpy(out + kHdr, plain, n);
  ctrCrypt(kDefaultKey, id, me.nodeNum, out + kHdr, n);
  return kHdr + n;
}

void copyStr(char* dst, size_t cap, const char* src) {
  size_t i = 0;
  if (src) for (; i + 1 < cap && src[i]; i++) dst[i] = src[i];
  dst[i] = 0;
}

bool textData(const char* text, MeshData& d) {
  if (!text) return false;
  size_t n = strlen(text);
  if (n > sizeof d.payload) return false;
  d.portnum = kPortText;
  memcpy(d.payload, text, n);
  d.payloadLen = n;
  return true;
}
}  // namespace

uint32_t nextPacketId(uint32_t rnd) {
  uint32_t id = (rnd << 10) | (g_counter++ & 0x3FF);
  if (id == 0) id = (rnd << 10) | (g_counter++ & 0x3FF);
  return id;
}

size_t buildBroadcastText(const Identity& me, uint32_t id, const char* text, uint8_t* out) {
  MeshData d;
  if (!textData(text, d)) return 0;
  return finishChannel(me, id, kBroadcast, false, d, out);
}

size_t buildNodeInfo(const Identity& me, uint32_t id, uint32_t to, bool wantResponse, uint8_t* out) {
  MeshUser u;
  memset(&u, 0, sizeof u);
  snprintf(u.id, sizeof u.id, "!%08x", (unsigned)me.nodeNum);
  copyStr(u.longName, sizeof u.longName, me.longName);
  copyStr(u.shortName, sizeof u.shortName, me.shortName);
  u.hwModel = 112; u.role = 1;
  memcpy(u.publicKey, me.pub, 32); u.publicKeyLen = 32;
  MeshData d;
  d.portnum = kPortNodeInfo;
  d.wantResponse = wantResponse;
  size_t n = pbEncodeUser(u, d.payload, sizeof d.payload);
  if (n == 0) return 0;
  d.payloadLen = n;
  return finishChannel(me, id, to, false, d, out);
}

size_t buildAck(const Identity& me, uint32_t id, uint32_t to, uint32_t ackedId, uint8_t* out) {
  return buildRoutingError(me, id, to, ackedId, 0, out);
}

size_t buildRoutingError(const Identity& me, uint32_t id, uint32_t to, uint32_t requestId, uint32_t err, uint8_t* out) {
  MeshData d;
  d.portnum = kPortRouting;
  d.requestId = requestId;
  size_t n = pbEncodeRoutingError(err, d.payload, sizeof d.payload);
  if (n == 0) return 0;
  d.payloadLen = n;
  return finishChannel(me, id, to, false, d, out);
}

size_t buildDirectText(const Identity& me, uint32_t id, uint32_t to, const uint8_t peerPub[32],
                       uint32_t extraNonce, const char* text, uint8_t* out) {
  MeshData d;
  if (!peerPub || !textData(text, d)) return 0;
  uint8_t plain[kMaxPayload];
  size_t n = pbEncodeData(d, plain, sizeof plain);
  if (n == 0 || n + 12 > kMaxPayload) return 0;
  uint8_t key[32];
  if (!x25519Shared(me.priv, peerPub, key)) return 0;
  fillHeader(me, id, to, true, 0x00, out);
  size_t w = pkiEncrypt(key, id, me.nodeNum, extraNonce, plain, n, out + kHdr);
  secureZero(key, sizeof key);
  secureZero(plain, sizeof plain);
  if (w == 0 || kHdr + w > kHdr + kMaxPayload) return 0;
  return kHdr + w;
}

bool parseFrame(const Identity& me, const uint8_t* frame, size_t len,
                const uint8_t* (*peerPubLookup)(uint32_t nodeNum), Inbound& out) {
  out.kind = InKind::None;
  out.direct = false; out.pki = false; out.ackedId = 0; out.routingError = 0;
  if (!frame || len < kHdr || len > kHdr + kMaxPayload) return false;
  if (!headerParse(frame, len, out.h)) return false;
  const PacketHeader& h = out.h;
  if (h.from == me.nodeNum || h.from == kBroadcast) return false;
  size_t plen = len - kHdr;
  uint8_t buf[kMaxPayload];
  size_t dlen;
  bool pki = false;

  if (h.channel == 0x00 && h.to == me.nodeNum) {
    const uint8_t* peer = peerPubLookup ? peerPubLookup(h.from) : nullptr;
    if (!peer) return false;
    uint8_t key[32];
    if (!x25519Shared(me.priv, peer, key)) return false;
    int r = pkiDecrypt(key, h.id, h.from, frame + kHdr, plen, buf);
    secureZero(key, sizeof key);
    if (r < 0) return false;
    dlen = (size_t)r;
    pki = true;
  } else if (h.channel == kChannelHash) {
    memcpy(buf, frame + kHdr, plen);
    ctrCrypt(kDefaultKey, h.id, h.from, buf, plen);
    dlen = plen;
  } else {
    return false;
  }

  if (!pbDecodeData(buf, dlen, out.data)) return false;
  out.direct = (h.to == me.nodeNum);
  out.pki = pki;

  switch (out.data.portnum) {
    case kPortText:
      // Legacy channel-key text DMs are rejected, as in the official firmware.
      if (!pki && h.to != kBroadcast) return false;
      out.kind = InKind::Text;
      return true;
    case kPortNodeInfo:
      if (!pbDecodeUser(out.data.payload, out.data.payloadLen, out.user)) return false;
      out.kind = InKind::NodeInfo;
      return true;
    case kPortRouting: {
      if (h.to != me.nodeNum) return false;
      uint32_t err = 0;
      if (!pbDecodeRoutingError(out.data.payload, out.data.payloadLen, err)) return false;
      if (out.data.requestId == 0) return false;  // not a reply to one of our packets
      out.ackedId = out.data.requestId;
      out.routingError = err;  // non-zero: NAK
      out.kind = InKind::Ack;
      return true;
    }
    default:
      out.kind = InKind::Other;
      return false;
  }
}

bool DedupCache::contains(uint32_t from, uint32_t id) const {
  for (uint8_t i = 0; i < count_; i++)
    if (from_[i] == from && id_[i] == id) return true;
  return false;
}

void DedupCache::record(uint32_t from, uint32_t id) { (void)seen(from, id); }  // seen() records new pairs

bool DedupCache::seen(uint32_t from, uint32_t id) {
  if (contains(from, id)) return true;
  from_[next_] = from; id_[next_] = id;
  next_ = (uint8_t)((next_ + 1) & 63);
  if (count_ < 64) count_++;
  return false;
}
