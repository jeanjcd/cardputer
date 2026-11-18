#include "header.h"

static uint32_t rd32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void wr32(uint8_t* p, uint32_t v) {
  p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}

bool headerParse(const uint8_t* buf, size_t len, PacketHeader& out) {
  if (len < 16) return false;
  out.to = rd32(buf);
  out.from = rd32(buf + 4);
  out.id = rd32(buf + 8);
  uint8_t f = buf[12];
  out.hopLimit = f & 0x07;
  out.wantAck = (f & 0x08) != 0;
  out.viaMqtt = (f & 0x10) != 0;
  out.hopStart = (f >> 5) & 0x07;
  out.channel = buf[13];
  out.nextHop = buf[14];
  out.relayNode = buf[15];
  return true;
}

void headerWrite(const PacketHeader& h, uint8_t out[16]) {
  wr32(out, h.to);
  wr32(out + 4, h.from);
  wr32(out + 8, h.id);
  out[12] = (uint8_t)((h.hopLimit & 0x07) | (h.wantAck ? 0x08 : 0) | (h.viaMqtt ? 0x10 : 0) |
                      ((h.hopStart & 0x07) << 5));
  out[13] = h.channel;
  out[14] = h.nextHop;
  out[15] = h.relayNode;
}
