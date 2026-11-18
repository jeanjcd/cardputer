#pragma once
#include <stddef.h>
#include <stdint.h>
#include "header.h"
#include "pb.h"

/// Our node identity: node number, X25519 keypair and display names.
struct Identity {
  uint32_t nodeNum;
  uint8_t priv[32], pub[32];
  char longName[40], shortName[8];
};

/// Builds a packet id: (rnd << 10) | (call counter & 0x3FF); never 0. Uses a process-wide static counter.
uint32_t nextPacketId(uint32_t rnd);

/// Builds a broadcast TEXT_MESSAGE_APP frame (channel key, no want_ack); returns frame length or 0.
size_t buildBroadcastText(const Identity& me, uint32_t id, const char* text, uint8_t* out);
/// Builds a NODEINFO_APP frame carrying our User and public key (channel key); returns frame length or 0.
size_t buildNodeInfo(const Identity& me, uint32_t id, uint32_t to, bool wantResponse, uint8_t* out);
/// Builds a ROUTING ACK frame for ackedId addressed to `to` (channel key); returns frame length or 0.
size_t buildAck(const Identity& me, uint32_t id, uint32_t to, uint32_t ackedId, uint8_t* out);
/// Builds a PKI-encrypted direct text with want_ack set; returns frame length or 0.
size_t buildDirectText(const Identity& me, uint32_t id, uint32_t to, const uint8_t peerPub[32],
                       uint32_t extraNonce, const char* text, uint8_t* out);

/// Kind of a decoded inbound frame.
enum class InKind { None, Text, NodeInfo, Ack, Other };

/// Decoded inbound frame; only valid fields for `kind` are meaningful.
struct Inbound {
  InKind kind;
  PacketHeader h;
  bool direct;  // addressed to us (not broadcast)
  bool pki;     // arrived PKI-encrypted
  MeshData data;
  MeshUser user;
  uint32_t ackedId;
  uint32_t routingError;  // for Ack: Routing.error_reason (0 = ACK, otherwise a NAK for ackedId)
};

/// Parses and decrypts a frame; returns true only for Text, NodeInfo or Ack/NAK events (false for anything else or malformed).
bool parseFrame(const Identity& me, const uint8_t* frame, size_t len,
                const uint8_t* (*peerPubLookup)(uint32_t nodeNum), Inbound& out);

/// Ring of the last 64 (from,id) pairs for duplicate suppression.
class DedupCache {
 public:
  /// Returns true if (from,id) was already seen; otherwise records it and returns false.
  bool seen(uint32_t from, uint32_t id);

 private:
  uint32_t from_[64] = {};
  uint32_t id_[64] = {};
  uint8_t count_ = 0;
  uint8_t next_ = 0;
};
