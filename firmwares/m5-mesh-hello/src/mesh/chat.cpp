// SPDX-License-Identifier: GPL-3.0-or-later
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
  char tmp[sizeof text];  // copy first: s may point into text itself
  size_t n = 0;
  while (n < sizeof text - 1 && s[n]) { tmp[n] = s[n]; n++; }
  n = utf8Trim(tmp, n);
  memset(text, 0, sizeof text);
  memcpy(text, tmp, n);
}

void ChatStore::add(uint32_t peer, const ChatMsg& in) {
  ChatMsg m = in;  // local copy: `in` may alias a pool slot we are about to free
  char tmp[sizeof m.text];
  memcpy(tmp, in.text, sizeof tmp);
  tmp[sizeof tmp - 1] = 0;
  m.setText(tmp);

  size_t cnt = 0, oldestOfPeer = kPoolSize, free_ = kPoolSize, oldestAll = kPoolSize;
  for (size_t i = 0; i < kPoolSize; i++) {
    const Slot& s = pool_[i];
    if (!s.used) { if (free_ == kPoolSize) free_ = i; continue; }
    if (oldestAll == kPoolSize || (int32_t)(s.seq - pool_[oldestAll].seq) < 0) oldestAll = i;
    if (s.peer == peer) {
      cnt++;
      if (oldestOfPeer == kPoolSize || (int32_t)(s.seq - pool_[oldestOfPeer].seq) < 0) oldestOfPeer = i;
    }
  }
  size_t slot;
  if (cnt >= kMaxMsgs) slot = oldestOfPeer;        // per-peer cap: reuse that peer's oldest
  else if (free_ != kPoolSize) slot = free_;
  else slot = oldestAll;                           // pool full: evict the globally oldest
  Slot& s = pool_[slot];
  s.used = true; s.peer = peer; s.seq = ++seq_; s.m = m;
}

ChatMsg* ChatStore::findById(uint32_t id) {
  if (id == 0) return nullptr;
  for (size_t i = 0; i < kPoolSize; i++)
    if (pool_[i].used && pool_[i].m.id == id) return &pool_[i].m;
  return nullptr;
}

void ChatStore::markAcked(uint32_t packetId) {
  if (packetId == 0) return;
  for (size_t i = 0; i < kPoolSize; i++) {
    ChatMsg& m = pool_[i].m;
    if (pool_[i].used && m.id == packetId && m.mine && m.state == ChatMsg::Sent) { m.state = ChatMsg::Acked; return; }
  }
}

size_t ChatStore::count(uint32_t peer) const {
  size_t n = 0;
  for (size_t i = 0; i < kPoolSize; i++) if (pool_[i].used && pool_[i].peer == peer) n++;
  return n;
}

const ChatMsg& ChatStore::at(uint32_t peer, size_t idx) const {
  static const ChatMsg empty = {};
  for (size_t i = 0; i < kPoolSize; i++) {
    if (!pool_[i].used || pool_[i].peer != peer) continue;
    size_t rank = 0;  // number of this peer's messages older than this one
    for (size_t j = 0; j < kPoolSize; j++)
      if (pool_[j].used && pool_[j].peer == peer && (int32_t)(pool_[j].seq - pool_[i].seq) < 0) rank++;
    if (rank == idx) return pool_[i].m;
  }
  return empty;
}
