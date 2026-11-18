// SPDX-License-Identifier: GPL-3.0-or-later
#include "greeter.h"
#include <string.h>

namespace {
constexpr size_t kBlob = 3 + Greeter::kMaxGreeted * 4;
constexpr uint8_t kVer = 1;
}

Greeter::Greeter(Kv& kv, uint32_t intervalMs, size_t queueCap, size_t persistCap)
    : kv_(kv), interval_(intervalMs),
      queueCap_(queueCap > kMaxQueue ? kMaxQueue : queueCap),
      persistCap_(persistCap > kMaxGreeted ? kMaxGreeted : persistCap) {}

void Greeter::load() {
  gn_ = 0;
  uint8_t b[kBlob];
  if (!kv_.get("greeted", b, kBlob) || b[0] != kVer) return;
  size_t n = b[1] | (b[2] << 8);
  if (n > kMaxGreeted) return;
  for (size_t i = 0; i < n; i++) {
    const uint8_t* p = b + 3 + i * 4;
    greeted_[i] = p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
  }
  gn_ = n;
}

bool Greeter::persist() {
  uint8_t b[kBlob]; memset(b, 0, sizeof b);
  b[0] = kVer; b[1] = (uint8_t)gn_; b[2] = (uint8_t)(gn_ >> 8);
  for (size_t i = 0; i < gn_; i++)
    for (int k = 0; k < 4; k++) b[3 + i * 4 + k] = (uint8_t)(greeted_[i] >> (8 * k));
  return kv_.put("greeted", b, kBlob);
}

void Greeter::setEnabled(bool on) {
  enabled_ = on;
  if (!on) qn_ = 0;
}

bool Greeter::wasGreeted(uint32_t num) const {
  for (size_t i = 0; i < gn_; i++) if (greeted_[i] == num) return true;
  return false;
}

bool Greeter::queued(uint32_t num) const {
  for (size_t i = 0; i < qn_; i++) if (q_[i] == num) return true;
  return false;
}

void Greeter::onNodeWithKey(uint32_t num) {
  if (!enabled_ || wasGreeted(num) || queued(num)) return;
  if (qn_ >= queueCap_ || gn_ + qn_ >= persistCap_) return;
  q_[qn_++] = num;
}

bool Greeter::due(uint32_t nowMs, uint32_t& outNum) {
  if (!enabled_ || qn_ == 0 || gn_ >= persistCap_) return false;
  if (haveLast_ && (uint32_t)(nowMs - last_) < interval_) return false;
  if (haveFail_ && (uint32_t)(nowMs - failAt_) < kRetryBackoffMs) return false;
  greeted_[gn_++] = q_[0];
  if (!persist()) {  // never hand out a node we could not record as greeted
    gn_--;
    haveFail_ = true; failAt_ = nowMs;
    return false;
  }
  haveFail_ = false;
  outNum = q_[0];
  for (size_t i = 0; i + 1 < qn_; i++) q_[i] = q_[i + 1];
  qn_--;
  haveLast_ = true; last_ = nowMs;
  return true;
}

bool Greeter::markGreeted(uint32_t num) {
  if (wasGreeted(num)) return true;
  if (gn_ >= persistCap_) return false;
  greeted_[gn_++] = num;
  if (!persist()) { gn_--; return false; }
  size_t w = 0;
  for (size_t i = 0; i < qn_; i++) if (q_[i] != num) q_[w++] = q_[i];
  qn_ = w;
  return true;
}
