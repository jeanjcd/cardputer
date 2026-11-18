// SPDX-License-Identifier: GPL-3.0-or-later
#include "ratelimit.h"

bool NodeRateLimiter::allow(uint32_t num, uint32_t nowMs, uint32_t periodMs) {
  size_t slot = kSlots, freeSlot = kSlots, oldest = kSlots;
  for (size_t i = 0; i < kSlots; i++) {
    if (!used_[i]) { if (freeSlot == kSlots) freeSlot = i; continue; }
    if (num_[i] == num) { slot = i; break; }
    if (oldest == kSlots || (int32_t)(at_[i] - at_[oldest]) < 0) oldest = i;
  }
  if (slot != kSlots) {
    if ((uint32_t)(nowMs - at_[slot]) < periodMs) return false;
  } else {
    slot = freeSlot != kSlots ? freeSlot : oldest;
  }
  used_[slot] = true;
  num_[slot] = num;
  at_[slot] = nowMs;
  return true;
}

bool ReplyLimiter::allow(uint32_t num, uint32_t nowMs) {
  if (haveLast_ && (uint32_t)(nowMs - last_) < global_) return false;  // checked first: refusals are not recorded
  if (!nodes_.allow(num, nowMs, perNode_)) return false;
  haveLast_ = true;
  last_ = nowMs;
  return true;
}

void AckMemory::record(uint32_t from, uint32_t id) {
  if (id == 0) return;
  for (size_t i = 0; i < kSlots; i++)
    if (from_[i] == from && id_[i] == id) return;  // already recorded: keep its re-ACK count
  from_[next_] = from;
  id_[next_] = id;
  reAcks_[next_] = 0;
  next_ = (uint8_t)((next_ + 1) % kSlots);
}

bool AckMemory::allowReAck(uint32_t from, uint32_t id) {
  if (id == 0) return false;
  for (size_t i = 0; i < kSlots; i++) {
    if (from_[i] != from || id_[i] != id) continue;
    if (reAcks_[i] >= kMaxReAcks) return false;
    reAcks_[i]++;
    return true;
  }
  return false;
}
