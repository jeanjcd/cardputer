#pragma once
#include <stdint.h>
#include <vector>

#include "hid_output.h"
#include "macro_actions.h"

// Non-blocking macro engine: call update() every loop iteration.
class MacroRunner {
 public:
  MacroRunner(HidOutput& hid, RandomFn randomFn);

  // Turning off mid-cycle releases all keys and drops the rest of the cycle.
  void setEnabled(bool on, uint32_t nowMs);
  bool enabled() const { return enabled_; }

  void update(uint32_t nowMs, bool connected);

  // 0 while a cycle is running or about to start.
  uint32_t msUntilNextCycle(uint32_t nowMs) const;
  uint32_t cyclesDone() const { return cycles_; }

 private:
  bool running() const { return idx_ < queue_.size(); }
  void abortCycle();

  HidOutput& hid_;
  RandomFn randomFn_;
  bool enabled_ = false;
  bool firstPending_ = true;
  std::vector<Action> queue_;
  size_t idx_ = 0;
  bool waiting_ = false;
  uint32_t waitStart_ = 0;
  uint32_t lastCycleStart_ = 0;
  uint32_t cycles_ = 0;
};
