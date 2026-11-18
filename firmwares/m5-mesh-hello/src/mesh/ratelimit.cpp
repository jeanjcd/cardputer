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
