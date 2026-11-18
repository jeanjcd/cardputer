// SPDX-License-Identifier: GPL-3.0-or-later
#include "outbox.h"
#include <string.h>

void Outbox::removeAt(size_t i) {
  for (; i + 1 < n_; i++) s_[i] = s_[i + 1];
  n_--;
}

bool Outbox::queueDirect(const OutItem& item, bool keyKnown, uint32_t nowMs, uint32_t* evictedChatId) {
  if (evictedChatId) *evictedChatId = 0;
  if (n_ >= kMax) {
    if (!keyKnown) return false;
    size_t oldest = n_;  // slots are in arrival order, so the first held one is the oldest held
    for (size_t i = 0; i < n_; i++)
      if (!s_[i].ready) { oldest = i; break; }
    if (oldest == n_) return false;
    if (evictedChatId) *evictedChatId = s_[oldest].item.chatId;
    removeAt(oldest);
  }
  Slot& s = s_[n_++];
  s.item = item;
  s.item.text[sizeof s.item.text - 1] = 0;
  s.ready = keyKnown;
  s.readySeq = keyKnown ? seq_++ : 0;
  s.heldSinceMs = nowMs;
  return true;
}

void Outbox::onKeyArrived(uint32_t num) {
  for (size_t i = 0; i < n_; i++)
    if (!s_[i].ready && s_[i].item.to == num) { s_[i].ready = true; s_[i].readySeq = seq_++; }
}

bool Outbox::next(OutItem& out) {
  size_t best = n_;
  for (size_t i = 0; i < n_; i++)
    if (s_[i].ready && (best == n_ || (int32_t)(s_[i].readySeq - s_[best].readySeq) < 0)) best = i;
  if (best == n_) return false;
  out = s_[best].item;
  removeAt(best);
  return true;
}

bool Outbox::expireOne(uint32_t nowMs, uint32_t* chatId) {
  for (size_t i = 0; i < n_; i++) {
    if (s_[i].ready || (uint32_t)(nowMs - s_[i].heldSinceMs) < kHeldTtlMs) continue;
    if (chatId) *chatId = s_[i].item.chatId;
    removeAt(i);
    return true;
  }
  return false;
}

size_t Outbox::heldPeers(uint32_t* out, size_t cap) const {
  size_t w = 0;
  for (size_t i = 0; i < n_ && w < cap; i++) {
    if (s_[i].ready) continue;
    bool dup = false;
    for (size_t j = 0; j < w; j++) if (out[j] == s_[i].item.to) { dup = true; break; }
    if (!dup) out[w++] = s_[i].item.to;
  }
  return w;
}
