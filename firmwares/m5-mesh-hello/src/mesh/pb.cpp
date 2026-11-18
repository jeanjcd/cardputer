#include "pb.h"
#include <string.h>

namespace {

struct Writer {
  uint8_t* p;
  size_t cap, n = 0;
  bool ok = true;
  Writer(uint8_t* o, size_t c) : p(o), cap(c) {}
  void byte(uint8_t b) {
    if (n < cap) p[n++] = b; else ok = false;
  }
  void varint(uint64_t v) {
    while (v >= 0x80) { byte((uint8_t)(v | 0x80)); v >>= 7; }
    byte((uint8_t)v);
  }
  void tag(uint32_t field, uint32_t wt) { varint((field << 3) | wt); }
  void vfield(uint32_t f, uint64_t v) { tag(f, 0); varint(v); }
  void f32(uint32_t f, uint32_t v) {
    tag(f, 5);
    for (int i = 0; i < 4; i++) byte((v >> (8 * i)) & 0xFF);
  }
  void bytes(uint32_t f, const uint8_t* d, size_t len) {
    tag(f, 2); varint(len);
    if (n + len > cap) { ok = false; return; }
    memcpy(p + n, d, len); n += len;
  }
  size_t result() const { return ok ? n : 0; }
};

struct Reader {
  const uint8_t* p;
  size_t len, pos = 0;
  Reader(const uint8_t* b, size_t l) : p(b), len(l) {}
  bool done() const { return pos >= len; }
  bool varint(uint64_t& v) {
    v = 0;
    for (int shift = 0; shift < 70; shift += 7) {
      if (pos >= len) return false;
      uint8_t b = p[pos++];
      if (shift == 63 && (b & 0x7E)) return false;  // overflow beyond 64 bits
      v |= (uint64_t)(b & 0x7F) << shift;
      if (!(b & 0x80)) return true;
    }
    return false;
  }
  bool fixed32(uint32_t& v) {
    if (len - pos < 4) return false;
    v = (uint32_t)p[pos] | ((uint32_t)p[pos + 1] << 8) | ((uint32_t)p[pos + 2] << 16) | ((uint32_t)p[pos + 3] << 24);
    pos += 4;
    return true;
  }
  bool lenDelim(const uint8_t*& d, size_t& l) {
    uint64_t v;
    if (!varint(v)) return false;
    if (v > len - pos) return false;
    d = p + pos; l = (size_t)v; pos += l;
    return true;
  }
  bool skip(uint32_t wt) {
    uint64_t v; const uint8_t* d; size_t l; uint32_t f;
    switch (wt) {
      case 0: return varint(v);
      case 1: if (len - pos < 8) return false; pos += 8; return true;
      case 2: return lenDelim(d, l);
      case 5: return fixed32(f);
      default: return false;
    }
  }
};

// Copies src into a NUL-terminated buffer of size cap, truncating if needed.
void copyStr(char* dst, size_t cap, const uint8_t* src, size_t l) {
  size_t n = l < cap - 1 ? l : cap - 1;
  memcpy(dst, src, n);
  dst[n] = 0;
}

// Length of s (<= maxLen bytes) cut at a UTF-8 character boundary.
size_t utf8Cut(const char* s, size_t bufCap, size_t maxLen) {
  size_t l = strnlen(s, bufCap);
  if (l <= maxLen) return l;
  size_t c = maxLen;
  while (c > 0 && ((uint8_t)s[c] & 0xC0) == 0x80) c--;  // s[c] is a continuation byte: back up
  return c;
}

}  // namespace

size_t pbEncodeData(const MeshData& d, uint8_t* out, size_t cap) {
  if (d.payloadLen > sizeof d.payload) return 0;
  Writer w(out, cap);
  if (d.portnum) w.vfield(1, d.portnum);
  if (d.payloadLen) w.bytes(2, d.payload, d.payloadLen);
  if (d.wantResponse) w.vfield(3, 1);
  if (d.dest) w.f32(4, d.dest);
  if (d.source) w.f32(5, d.source);
  if (d.requestId) w.f32(6, d.requestId);
  if (d.replyId) w.f32(7, d.replyId);
  return w.result();
}

bool pbDecodeData(const uint8_t* in, size_t len, MeshData& d) {
  d.portnum = 0; d.payloadLen = 0; d.wantResponse = false;
  d.dest = d.source = d.requestId = d.replyId = 0;
  Reader r(in, len);
  while (!r.done()) {
    uint64_t key;
    if (!r.varint(key)) return false;
    uint32_t f = (uint32_t)(key >> 3), wt = (uint32_t)(key & 7);
    uint64_t v; uint32_t x; const uint8_t* b; size_t bl;
    if (f == 1 && wt == 0) { if (!r.varint(v)) return false; d.portnum = (uint32_t)v; }
    else if (f == 2 && wt == 2) {
      if (!r.lenDelim(b, bl) || bl > sizeof d.payload) return false;
      memcpy(d.payload, b, bl); d.payloadLen = bl;
    }
    else if (f == 3 && wt == 0) { if (!r.varint(v)) return false; d.wantResponse = v != 0; }
    else if (f >= 4 && f <= 7 && wt == 5) {
      if (!r.fixed32(x)) return false;
      (f == 4 ? d.dest : f == 5 ? d.source : f == 6 ? d.requestId : d.replyId) = x;
    }
    else if (!r.skip(wt)) return false;
  }
  return true;
}

size_t pbEncodeUser(const MeshUser& u, uint8_t* out, size_t cap) {
  Writer w(out, cap);
  size_t l = strnlen(u.id, sizeof u.id);
  if (l) w.bytes(1, (const uint8_t*)u.id, l);
  l = utf8Cut(u.longName, sizeof u.longName, 24);
  if (l) w.bytes(2, (const uint8_t*)u.longName, l);
  l = strnlen(u.shortName, sizeof u.shortName);
  if (l) w.bytes(3, (const uint8_t*)u.shortName, l);
  if (u.hwModel) w.vfield(5, u.hwModel);
  if (u.role) w.vfield(7, u.role);
  if (u.publicKeyLen) {
    if (u.publicKeyLen > sizeof u.publicKey) return 0;
    w.bytes(8, u.publicKey, u.publicKeyLen);
  }
  return w.result();
}

bool pbDecodeUser(const uint8_t* in, size_t len, MeshUser& u) {
  memset(&u, 0, sizeof u);
  Reader r(in, len);
  while (!r.done()) {
    uint64_t key;
    if (!r.varint(key)) return false;
    uint32_t f = (uint32_t)(key >> 3), wt = (uint32_t)(key & 7);
    uint64_t v; const uint8_t* b; size_t bl;
    if (wt == 2 && (f == 1 || f == 2 || f == 3 || f == 8)) {
      if (!r.lenDelim(b, bl)) return false;
      if (f == 1) copyStr(u.id, sizeof u.id, b, bl);
      else if (f == 2) copyStr(u.longName, sizeof u.longName, b, bl);
      else if (f == 3) copyStr(u.shortName, sizeof u.shortName, b, bl);
      else {
        if (bl != sizeof u.publicKey) return false;
        memcpy(u.publicKey, b, bl); u.publicKeyLen = bl;
      }
    }
    else if (f == 5 && wt == 0) { if (!r.varint(v)) return false; u.hwModel = (uint32_t)v; }
    else if (f == 7 && wt == 0) { if (!r.varint(v)) return false; u.role = (uint32_t)v; }
    else if (!r.skip(wt)) return false;
  }
  return true;
}

size_t pbEncodeRoutingAck(uint8_t* out, size_t cap) { return pbEncodeRoutingError(0, out, cap); }

size_t pbEncodeRoutingError(uint32_t err, uint8_t* out, size_t cap) {
  Writer w(out, cap);
  w.vfield(3, err);
  return w.result();
}

bool pbDecodeRoutingError(const uint8_t* in, size_t len, uint32_t& err) {
  err = 0;  // absent error_reason (proto3 omits zero enum) means NONE
  Reader r(in, len);
  while (!r.done()) {
    uint64_t key, v;
    if (!r.varint(key)) return false;
    uint32_t f = (uint32_t)(key >> 3), wt = (uint32_t)(key & 7);
    if (f == 3 && wt == 0) { if (!r.varint(v)) return false; err = (uint32_t)v; }
    else if (!r.skip(wt)) return false;
  }
  return true;
}
