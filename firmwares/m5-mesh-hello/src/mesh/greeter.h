#pragma once
#include <stddef.h>
#include <stdint.h>
#include "kv.h"

/// Auto-greeter: queues nodes whose key we know and hands them out at most one per interval, never the same node twice.
/// The greeted set is persisted under key "greeted" as a fixed 2003-byte blob [ver=1 u8][count u16 LE][500 x u32 LE];
/// a blob with wrong size/version/count loads as an empty set.
/// When the persisted set is full, auto-greeting of new nodes stops (old IDs are never forgotten, so nobody is greeted twice).
/// The interval is kept across disable/enable; the very first greeting is not delayed.
class Greeter {
 public:
  static constexpr size_t kMaxQueue = 10;
  static constexpr size_t kMaxGreeted = 500;

  /// Creates a greeter; queueCap and persistCap are clamped to 10 and 500.
  Greeter(Kv& kv, uint32_t intervalMs = 30000, size_t queueCap = 10, size_t persistCap = 500);
  /// Loads the persisted greeted set (call once at boot).
  void load();
  /// Enables or disables greeting; disabling clears the pending queue.
  void setEnabled(bool on);
  /// True if auto-greet is on.
  bool enabled() const { return enabled_; }
  /// Enqueues a node if enabled, not greeted, not already queued, queue not full and the greeted set not full.
  void onNodeWithKey(uint32_t num);
  /// If the interval elapsed since the last greeting, pops the oldest queued node into outNum, marks it greeted, persists and returns true.
  bool due(uint32_t nowMs, uint32_t& outNum);
  /// True if the node is in the greeted set.
  bool wasGreeted(uint32_t num) const;

 private:
  bool queued(uint32_t num) const;
  void persist();
  Kv& kv_;
  uint32_t interval_;
  size_t queueCap_, persistCap_;
  bool enabled_ = false, haveLast_ = false;
  uint32_t last_ = 0;
  uint32_t q_[kMaxQueue] = {};
  size_t qn_ = 0;
  uint32_t greeted_[kMaxGreeted] = {};
  size_t gn_ = 0;
};
