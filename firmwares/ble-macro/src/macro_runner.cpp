#include "macro_runner.h"

MacroRunner::MacroRunner(HidOutput& hid, RandomFn randomFn) : hid_(hid), randomFn_(randomFn) {}

void MacroRunner::abortCycle() {
  if (running()) hid_.releaseAll();
  queue_.clear();
  idx_ = 0;
  waiting_ = false;
}

void MacroRunner::setEnabled(bool on, uint32_t nowMs) {
  (void)nowMs;
  if (on == enabled_) return;
  enabled_ = on;
  if (!on) abortCycle();
  firstPending_ = true;
}

void MacroRunner::update(uint32_t nowMs, bool connected) {
  if (!enabled_) return;

  if (!connected) {
    abortCycle();
    firstPending_ = true;  // run right away once the PC reconnects
    return;
  }

  if (!running()) {
    if (!firstPending_ && nowMs - lastCycleStart_ < kIntervalMs) return;
    queue_ = buildCycle(randomFn_);
    idx_ = 0;
    waiting_ = false;
    lastCycleStart_ = nowMs;
    firstPending_ = false;
  }

  while (idx_ < queue_.size()) {
    const Action& a = queue_[idx_];
    if (a.type == ActionType::Wait) {
      if (!waiting_) {
        waiting_ = true;
        waitStart_ = nowMs;
      }
      if (nowMs - waitStart_ < a.waitMs) return;
      waiting_ = false;
      idx_++;
      continue;
    }
    switch (a.type) {
      case ActionType::Move: hid_.mouseMove(a.dx, a.dy); break;
      case ActionType::KeyDown: hid_.keyDown(a.modifier); break;
      case ActionType::ReleaseAll: hid_.releaseAll(); break;
      default: break;
    }
    idx_++;
  }

  // Queue drained: cycle complete.
  queue_.clear();
  idx_ = 0;
  cycles_++;
}

uint32_t MacroRunner::msUntilNextCycle(uint32_t nowMs) const {
  if (!enabled_ || running() || firstPending_) return 0;
  uint32_t elapsed = nowMs - lastCycleStart_;
  return elapsed >= kIntervalMs ? 0 : kIntervalMs - elapsed;
}
