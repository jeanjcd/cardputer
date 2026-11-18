#include "chat.h"
#include <string.h>

namespace {
// Cuts s (len bytes) back so it does not end inside a UTF-8 sequence; returns the new length.
size_t utf8Trim(const char* s, size_t len) {
  size_t i = len;
  size_t back = 0;
  while (i > 0 && back < 4 && ((uint8_t)s[i - 1] & 0xC0) == 0x80) { i--; back++; }
  if (i == 0) return len;
  uint8_t lead = (uint8_t)s[i - 1];
  size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
  if (need == 1) return len;
  size_t have = back + 1;
  return have < need ? i - 1 : len;
}
}  // namespace

void ChatMsg::setText(const char* s) {
  size_t n = 0;
  while (n < sizeof text - 1 && s[n]) n++;
  n = utf8Trim(s, n);
  memcpy(text, s, n);
  memset(text + n, 0, sizeof text - n);
}

const ChatStore::Conv* ChatStore::findConv(uint32_t peer) const {
  for (size_t i = 0; i < nConvs_; i++) if (convs_[i].peer == peer) return &convs_[i];
  return nullptr;
}

void ChatStore::add(uint32_t peer, const ChatMsg& in) {
  Conv* c = const_cast<Conv*>(findConv(peer));
  if (!c) {
    size_t slot;
    if (nConvs_ < kMaxConvs) slot = nConvs_++;
    else {
      slot = 0;
      for (size_t i = 1; i < kMaxConvs; i++) if ((int32_t)(convs_[i].seq - convs_[slot].seq) < 0) slot = i;
    }
    c = &convs_[slot];
    c->peer = peer; c->n = 0;
  }
  c->seq = ++seq_;
  if (c->n == kMaxMsgs) {
    for (size_t i = 0; i + 1 < kMaxMsgs; i++) c->m[i] = c->m[i + 1];
    c->n--;
  }
  ChatMsg& m = c->m[c->n++];
  m = in;
  // Re-sanitize: caller may not have NUL-terminated or cut at a character boundary.
  char tmp[sizeof m.text];
  memcpy(tmp, in.text, sizeof tmp);
  tmp[sizeof tmp - 1] = 0;
  m.setText(tmp);
}

ChatMsg* ChatStore::findById(uint32_t id) {
  if (id == 0) return nullptr;
  for (size_t i = 0; i < nConvs_; i++)
    for (size_t j = 0; j < convs_[i].n; j++)
      if (convs_[i].m[j].id == id) return &convs_[i].m[j];
  return nullptr;
}

void ChatStore::markAcked(uint32_t packetId) {
  if (packetId == 0) return;
  for (size_t i = 0; i < nConvs_; i++)
    for (size_t j = 0; j < convs_[i].n; j++) {
      ChatMsg& m = convs_[i].m[j];
      if (m.id == packetId && m.mine && m.state == ChatMsg::Sent) { m.state = ChatMsg::Acked; return; }
    }
}

size_t ChatStore::count(uint32_t peer) const {
  const Conv* c = findConv(peer);
  return c ? c->n : 0;
}

const ChatMsg& ChatStore::at(uint32_t peer, size_t i) const {
  static const ChatMsg empty = {};
  const Conv* c = findConv(peer);
  return (c && i < c->n) ? c->m[i] : empty;
}
