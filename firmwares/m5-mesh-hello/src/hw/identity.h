#pragma once
#include "../mesh/kv.h"
#include "../mesh/packet.h"

/// Outcome of loading the persistent identity.
enum class IdentityStatus {
  Loaded,          // keypair read from Kv
  Created,         // first boot: new keypair generated and persisted
  CreatedNotSaved  // new keypair generated but Kv write failed (it will change on next boot)
};

/// Fills `me`: X25519 keypair loaded from Kv key "idkey" (generated once with hardware entropy if absent or invalid),
/// node number from the factory MAC (mac[2..5], big-endian), names "4stro-031"/"4S31".
IdentityStatus loadOrCreateIdentity(Kv& kv, Identity& me);

/// Fills buf with true random bytes (RF-noise entropy source enabled around esp_fill_random).
void hwRandomBytes(uint8_t* buf, size_t len);
