#pragma once
#include <stddef.h>
#include <stdint.h>

/// Per-node "at most once per period" limiter over a fixed table; when full, the least recently allowed entry is reused.
class NodeRateLimiter {
 public:
  static constexpr size_t kSlots = 32;
  /// True (and records nowMs) if node `num` was not allowed within the last periodMs; wraparound-safe.
  bool allow(uint32_t num, uint32_t nowMs, uint32_t periodMs);

 private:
  uint32_t num_[kSlots] = {};
  uint32_t at_[kSlots] = {};
  bool used_[kSlots] = {};
};
