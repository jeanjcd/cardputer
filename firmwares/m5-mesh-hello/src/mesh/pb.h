#pragma once
#include <stddef.h>
#include <stdint.h>

// Meshtastic Data message subset (portnum, payload, want_response, dest/source/request_id/reply_id).
struct MeshData {
  uint32_t portnum = 0;
  uint8_t payload[233];
  size_t payloadLen = 0;
  bool wantResponse = false;
  uint32_t dest = 0, source = 0, requestId = 0, replyId = 0;
};

// Meshtastic User message subset; strings are NUL-terminated.
struct MeshUser {
  char id[16];
  char longName[40];
  char shortName[8];
  uint32_t hwModel = 0;
  uint32_t role = 0;
  uint8_t publicKey[32];
  size_t publicKeyLen = 0;
};

// Encodes Data into out; returns bytes written, or 0 if cap is too small.
size_t pbEncodeData(const MeshData& d, uint8_t* out, size_t cap);
// Decodes Data, skipping unknown fields; returns false on malformed or truncated input.
bool pbDecodeData(const uint8_t* in, size_t len, MeshData& d);
// Encodes User (long name cut to 24 UTF-8 bytes); returns bytes written, or 0 on overflow.
size_t pbEncodeUser(const MeshUser& u, uint8_t* out, size_t cap);
// Decodes User, skipping unknown/deprecated fields; returns false on malformed input.
bool pbDecodeUser(const uint8_t* in, size_t len, MeshUser& u);
// Encodes Routing with error_reason=NONE (bytes 18 00); returns bytes written, or 0 on overflow.
size_t pbEncodeRoutingAck(uint8_t* out, size_t cap);
// Extracts Routing.error_reason (field 3); returns false if malformed or absent.
bool pbDecodeRoutingError(const uint8_t* in, size_t len, uint32_t& err);
