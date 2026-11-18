#pragma once
#include <stddef.h>
#include <stdint.h>

/// One chat message; text is NUL-terminated UTF-8 (at most 199 bytes plus NUL).
struct ChatMsg {
  enum State : uint8_t { Pending, Sent, Acked, Failed, WaitingKey };
  bool mine;
  char text[200];
  uint32_t id;  // packet id (0 for none)
  State state;
  /// Copies s into text, truncating at a UTF-8 character boundary.
  void setText(const char* s);
};

/// Per-peer conversation history: 30 messages each (oldest dropped); peer 0xFFFFFFFF is the public channel.
/// At most kMaxConvs conversations are kept in RAM; the least recently active one is dropped for a new peer.
/// Read-only iteration: count(peer) then at(peer, i) for i in [0, count), oldest first.
class ChatStore {
 public:
  static constexpr size_t kMaxMsgs = 30;
  static constexpr size_t kMaxConvs = 8;
  static constexpr uint32_t kPublic = 0xFFFFFFFF;

  /// Appends a message to the peer's conversation (text is sanitized/truncated), dropping the oldest at cap.
  void add(uint32_t peer, const ChatMsg& m);
  /// Moves one of our own messages with this packet id from Sent to Acked; no-op otherwise.
  void markAcked(uint32_t packetId);
  /// Finds a message by packet id (nullptr if absent or id is 0).
  ChatMsg* findById(uint32_t id);
  /// Number of messages stored for the peer.
  size_t count(uint32_t peer) const;
  /// Message i (0 = oldest) of the peer; out-of-range returns a static empty message.
  const ChatMsg& at(uint32_t peer, size_t i) const;

 private:
  struct Conv { uint32_t peer; uint32_t seq; size_t n; ChatMsg m[kMaxMsgs]; };
  const Conv* findConv(uint32_t peer) const;
  Conv convs_[kMaxConvs] = {};
  size_t nConvs_ = 0;
  uint32_t seq_ = 0;
};
