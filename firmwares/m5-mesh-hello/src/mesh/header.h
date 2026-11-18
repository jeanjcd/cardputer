#pragma once
#include <stddef.h>
#include <stdint.h>

// 16-byte on-air Meshtastic packet header (little-endian fields).
struct PacketHeader {
  uint32_t to, from, id;
  uint8_t hopLimit, hopStart;
  bool wantAck, viaMqtt;
  uint8_t channel, nextHop, relayNode;
};

// Parses the 16-byte header from buf; returns false if len < 16.
bool headerParse(const uint8_t* buf, size_t len, PacketHeader& out);
// Serializes h into the 16-byte on-air header.
void headerWrite(const PacketHeader& h, uint8_t out[16]);
