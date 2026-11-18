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

/// Known-node table: 100 entries in RAM (oldest evicted), up to 50 peers (name + key) persisted in Kv.
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
  /// Creates or refreshes node `num` (evicting the oldest-heard at cap) and returns it; the reference is valid until the next touch/load.
  NodeEntry& touch(uint32_t num, int16_t rssi, float snr, uint32_t nowMs);
  /// Applies a decoded User to a node previously touched; first-key-wins: returns false and changes nothing if the node is unknown or the stored key differs/is missing.
  bool applyUser(uint32_t num, const MeshUser& u);
  /// Returns the node's 32-byte public key, or nullptr if unknown or none stored.
  const uint8_t* pubKey(uint32_t num) const;
  /// Persists name and key of a node that has a key (least recently persisted slot is overwritten at cap of 50).
  void persistPeer(uint32_t num);
  /// Number of nodes in RAM.
  size_t count() const { return n_; }
  /// Node at index i (0 = most recently heard); i must be < count().
  const NodeEntry& at(size_t i) const { return e_[i]; }

 private:
  int find(uint32_t num) const;
  Kv& kv_;
  NodeEntry e_[kMaxNodes];  // sorted by lastHeardMs, newest first
  size_t n_ = 0;
  uint32_t slotNum_[kMaxPeers] = {};  // 0 = free slot
  uint32_t slotSeq_[kMaxPeers] = {};
  uint32_t seq_ = 0;
};
