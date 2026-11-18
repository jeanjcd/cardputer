// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>

/// A direct message waiting to be sent.
struct OutItem {
  uint32_t to;
  char text[200];
  uint32_t chatId;
};

/// Outgoing DM queue: items without a known peer key are held until onKeyArrived; at most 10 items held+ready in total.
/// Held items expire after kHeldTtlMs (the caller marks them failed), and a ready DM that finds the outbox full evicts
/// the oldest held item, so DMs to nodes that never answer cannot block the queue until reboot.
class Outbox {
 public:
  static constexpr size_t kMax = 10;
  static constexpr uint32_t kHeldTtlMs = 10UL * 60 * 1000;  // a held DM gives up after 10 minutes
  /// Queues a DM (ready if keyKnown, else held WaitingKey since nowMs; the caller sends a NodeInfo request). If the
  /// outbox is full, a ready DM evicts the oldest held item (its chatId goes to *evictedChatId, else 0 is written);
  /// returns false if it is full and nothing could be evicted.
  bool queueDirect(const OutItem& item, bool keyKnown, uint32_t nowMs, uint32_t* evictedChatId = nullptr);
  /// Releases all items held for node `num` to the ready queue, in FIFO order.
  void onKeyArrived(uint32_t num);
  /// Pops the oldest ready item into out; false if none is ready.
  bool next(OutItem& out);
  /// Removes ONE held item older than kHeldTtlMs and returns its chatId in *chatId; false if none expired. Call in a loop.
  bool expireOne(uint32_t nowMs, uint32_t* chatId);
  /// Writes up to cap distinct node numbers that have held items (oldest first); returns how many were written.
  size_t heldPeers(uint32_t* out, size_t cap) const;
  /// Number of items queued (held + ready).
  size_t count() const { return n_; }

 private:
  struct Slot { OutItem item; bool ready; uint32_t readySeq; uint32_t heldSinceMs; };
  void removeAt(size_t i);
  Slot s_[kMax];
  size_t n_ = 0;
  uint32_t seq_ = 0;
};
