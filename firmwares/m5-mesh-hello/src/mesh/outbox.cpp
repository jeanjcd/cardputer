#include "outbox.h"
#include <string.h>

bool Outbox::queueDirect(const OutItem& item, bool keyKnown) {
  if (n_ >= kMax) return false;
  Slot& s = s_[n_++];
  s.item = item;
  s.item.text[sizeof s.item.text - 1] = 0;
  s.ready = keyKnown;
  s.readySeq = keyKnown ? seq_++ : 0;
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
  for (size_t i = best; i + 1 < n_; i++) s_[i] = s_[i + 1];
  n_--;
  return true;
}
