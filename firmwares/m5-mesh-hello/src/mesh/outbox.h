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
class Outbox {
 public:
  static constexpr size_t kMax = 10;
  /// Queues a DM (ready if keyKnown, else held WaitingKey; the caller sends a NodeInfo request); returns false if the outbox is full.
  bool queueDirect(const OutItem& item, bool keyKnown);
  /// Releases all items held for node `num` to the ready queue, in FIFO order.
  void onKeyArrived(uint32_t num);
  /// Pops the oldest ready item into out; false if none is ready.
  bool next(OutItem& out);

 private:
  struct Slot { OutItem item; bool ready; uint32_t readySeq; };
  Slot s_[kMax];
  size_t n_ = 0;
  uint32_t seq_ = 0;
};
