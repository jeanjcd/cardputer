// SPDX-License-Identifier: GPL-3.0-or-later
#include "nodedb.h"
#include <stdio.h>
#include <string.h>

namespace {
constexpr size_t kRec = 1 + 4 + 4 + 40 + 8 + 32;
constexpr uint8_t kVer = 1;

void putU32(uint8_t* p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }
uint32_t getU32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

// Bounded copy that always NUL-terminates dst.
template <size_t N> void copyStr(char (&dst)[N], const char* src) {
  size_t i = 0;
  for (; i + 1 < N && src[i]; i++) dst[i] = src[i];
  dst[i] = 0;
  for (i++; i < N; i++) dst[i] = 0;
}
void slotKey(size_t i, char* out) { snprintf(out, 16, "peer%u", (unsigned)i); }
}  // namespace

int NodeDb::find(uint32_t num) const {
  for (size_t i = 0; i < n_; i++) if (e_[i].num == num) return (int)i;
  return -1;
}

void NodeDb::load() {
  n_ = 0; seq_ = 0;
  memset(slotNum_, 0, sizeof slotNum_); memset(slotSeq_, 0, sizeof slotSeq_);
  for (size_t s = 0; s < kMaxPeers; s++) {
    char key[16]; slotKey(s, key);
    uint8_t r[kRec];
    if (!kv_.get(key, r, kRec) || r[0] != kVer) continue;
    uint32_t num = getU32(r + 1), seq = getU32(r + 5);
    if (num == 0 || find(num) >= 0) continue;
    slotNum_[s] = num; slotSeq_[s] = seq;
    if (seq > seq_) seq_ = seq;
    NodeEntry& e = touch(num, 0, 0, 0);
    char lname[40], sname[8];
    memcpy(lname, r + 9, 40); lname[39] = 0;
    memcpy(sname, r + 49, 8); sname[7] = 0;
    copyStr(e.user.longName, lname); copyStr(e.user.shortName, sname);
    memcpy(e.user.publicKey, r + 57, 32);
    e.user.publicKeyLen = 32; e.hasKey = true;
  }
}

NodeEntry& NodeDb::touch(uint32_t num, int16_t rssi, float snr, uint32_t nowMs) {
  int idx = find(num);
  NodeEntry cur;
  if (idx >= 0) {
    cur = e_[idx];
    for (size_t i = (size_t)idx; i + 1 < n_; i++) e_[i] = e_[i + 1];
    n_--;
  } else {
    memset(&cur, 0, sizeof cur);
    cur.num = num;
    if (n_ == kMaxNodes) n_--;  // drop the oldest-heard (last)
  }
  cur.rssi = rssi; cur.snr = snr; cur.lastHeardMs = nowMs;
  size_t pos = 0;  // first entry strictly older than us (wraparound-safe compare)
  while (pos < n_ && (int32_t)(e_[pos].lastHeardMs - nowMs) >= 0) pos++;
  for (size_t i = n_; i > pos; i--) e_[i] = e_[i - 1];
  e_[pos] = cur; n_++;
  return e_[pos];
}

bool NodeDb::applyUser(uint32_t num, const MeshUser& u) {
  int idx = find(num);
  if (idx < 0) return false;
  NodeEntry& e = e_[idx];
  bool incomingKey = (u.publicKeyLen == 32);
  if (e.hasKey && !(incomingKey && memcmp(e.user.publicKey, u.publicKey, 32) == 0)) return false;
  copyStr(e.user.id, u.id); copyStr(e.user.longName, u.longName); copyStr(e.user.shortName, u.shortName);
  e.user.hwModel = u.hwModel; e.user.role = u.role;
  if (!e.hasKey && incomingKey) {
    memcpy(e.user.publicKey, u.publicKey, 32);
    e.user.publicKeyLen = 32; e.hasKey = true;
  }
  return true;
}

const uint8_t* NodeDb::pubKey(uint32_t num) const {
  int idx = find(num);
  return (idx >= 0 && e_[idx].hasKey) ? e_[idx].user.publicKey : nullptr;
}

void NodeDb::persistPeer(uint32_t num) {
  int idx = find(num);
  if (idx < 0 || !e_[idx].hasKey || num == 0) return;
  size_t slot = kMaxPeers;
  for (size_t s = 0; s < kMaxPeers; s++) if (slotNum_[s] == num) { slot = s; break; }
  if (slot == kMaxPeers)
    for (size_t s = 0; s < kMaxPeers; s++) if (slotNum_[s] == 0) { slot = s; break; }
  if (slot == kMaxPeers) {  // full: overwrite the least recently persisted
    slot = 0;
    for (size_t s = 1; s < kMaxPeers; s++)
      if ((int32_t)(slotSeq_[s] - slotSeq_[slot]) < 0) slot = s;
  }
  const NodeEntry& e = e_[idx];
  uint8_t r[kRec]; memset(r, 0, sizeof r);
  r[0] = kVer; putU32(r + 1, num); putU32(r + 5, seq_ + 1);
  memcpy(r + 9, e.user.longName, 39);   // byte 39 stays NUL
  memcpy(r + 49, e.user.shortName, 7);  // byte 7 stays NUL
  memcpy(r + 57, e.user.publicKey, 32);
  char key[16]; slotKey(slot, key);
  if (!kv_.put(key, r, kRec)) return;
  seq_++;
  slotNum_[slot] = num; slotSeq_[slot] = seq_;
}
