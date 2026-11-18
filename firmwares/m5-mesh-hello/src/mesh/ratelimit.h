// SPDX-License-Identifier: GPL-3.0-or-later
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

/// Limits automatic replies to radio input: at most one per node per perNodeMs AND at most one overall per globalMs,
/// so spoofed sender ids cannot keep us transmitting. A reply that comes too soon is refused (dropped, not queued).
class ReplyLimiter {
 public:
  /// Creates a limiter with the per-node and global minimum intervals.
  ReplyLimiter(uint32_t perNodeMs, uint32_t globalMs) : perNode_(perNodeMs), global_(globalMs) {}
  /// True (and records it) if a reply to node `num` is allowed at nowMs.
  bool allow(uint32_t num, uint32_t nowMs);

 private:
  NodeRateLimiter nodes_;
  uint32_t perNode_, global_;
  bool haveLast_ = false;
  uint32_t last_ = 0;
};

/// Remembers the last 8 DMs we ACKed so a retransmission (our ACK was lost) is re-ACKed, at most kMaxReAcks times
/// per (from, id); further retransmissions are ignored so a replayed frame cannot keep us transmitting.
class AckMemory {
 public:
  static constexpr size_t kSlots = 8;
  static constexpr uint8_t kMaxReAcks = 2;
  /// Records that we ACKed packet `id` from `from` (oldest record is replaced when full; id 0 is ignored).
  void record(uint32_t from, uint32_t id);
  /// True (and counts it) if a retransmission of a recorded (from, id) may be re-ACKed now.
  bool allowReAck(uint32_t from, uint32_t id);

 private:
  uint32_t from_[kSlots] = {}, id_[kSlots] = {};
  uint8_t reAcks_[kSlots] = {};
  uint8_t next_ = 0;
};

/// Transmit policy: frames are sent only when a region has been confirmed (not first boot), no region change is
/// pending (so nothing goes out on an intermediate region while cycling), and the Cap and radio are working.
/// Otherwise frames stay queued (no airtime charged, DMs not failed).
inline bool txMayTransmit(bool regionConfirmed, bool regionChanging, bool radioReady, bool capFound) {
  return regionConfirmed && !regionChanging && radioReady && capFound;
}
