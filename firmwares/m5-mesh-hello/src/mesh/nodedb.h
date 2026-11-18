// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "kv.h"
#include "pb.h"

/// One known node: identity info, last radio stats and UI flags.
struct NodeEntry {
  uint32_t num;
  MeshUser user;
  bool hasKey;  // a 32-byte public key is stored in user.publicKey
  int16_t rssi;
  float snr;
  uint32_t lastHeardMs;
  uint8_t unread;
  bool greeted;  // UI flag, owned by the application
};

/// Known-node table: 100 entries in RAM, up to 50 peers (name + key) persisted in Kv.
/// The persisted peers' keys are also kept in a separate RAM pin table, so first-key-wins survives eviction from the
/// 100-entry table: pubKey() falls back to the pin and applyUser() rejects any other key for a pinned node.
/// Eviction at cap removes the oldest keyless, non-pinned entry, else the oldest entry.
/// Persistence layout: keys "peer0".."peer49", each one fixed 89-byte blob
/// [ver=1 u8][num u32 LE][seq u32 LE][longName 40][shortName 8][key 32]; invalid blobs are ignored on load.
class NodeDb {
 public:
  static constexpr size_t kMaxNodes = 100;
  static constexpr size_t kMaxPeers = 50;

  /// Creates a database backed by `kv` (which must outlive it).
  explicit NodeDb(Kv& kv) : kv_(kv) {}
  /// Loads persisted peers into RAM (call once at boot); corrupt slots are skipped.
  void load();
  /// Creates or refreshes node `num` and returns it (a new entry of a pinned node gets its pinned key); at cap the
  /// oldest keyless non-pinned entry (else the oldest) is evicted. The reference is valid until the next touch/load.
  NodeEntry& touch(uint32_t num, int16_t rssi, float snr, uint32_t nowMs);
  /// Applies a decoded User to a node previously touched; first-key-wins: returns false and changes nothing if the node
  /// is unknown or the stored or pinned key differs/is missing.
  bool applyUser(uint32_t num, const MeshUser& u);
  /// Returns the node's 32-byte public key (RAM entry, else the persisted pin), or nullptr if none is known.
  const uint8_t* pubKey(uint32_t num) const;
  /// Persists name and key of a node that has a key (least recently persisted slot is overwritten at cap of 50).
  void persistPeer(uint32_t num);
  /// Mutable entry for node num (for UI flags such as unread), or nullptr; valid until the next touch/load.
  NodeEntry* get(uint32_t num) { int i = find(num); return i >= 0 ? &e_[i] : nullptr; }
  /// Number of nodes in RAM.
  size_t count() const { return n_; }
  /// Node at index i (0 = most recently heard); i must be < count().
  const NodeEntry& at(size_t i) const { return e_[i]; }

 private:
  int find(uint32_t num) const;
  const uint8_t* pinned(uint32_t num) const;
  Kv& kv_;
  NodeEntry e_[kMaxNodes];  // sorted by lastHeardMs, newest first
  size_t n_ = 0;
  uint32_t slotNum_[kMaxPeers] = {};  // 0 = free slot
  uint32_t slotSeq_[kMaxPeers] = {};
  uint8_t slotKey_[kMaxPeers][32] = {};  // RAM copy of each persisted peer's key (the pin)
  uint32_t seq_ = 0;
};
