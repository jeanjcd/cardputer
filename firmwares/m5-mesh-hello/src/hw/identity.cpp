#include "identity.h"

#include <string.h>

#include "../mesh/crypto.h"
#include "../mesh/secure_zero.h"
#include "bootloader_random.h"
#include "esp_mac.h"
#include "esp_random.h"

namespace {
constexpr const char* kKeyName = "idkey";
constexpr uint8_t kRecordVersion = 1;
constexpr size_t kRecordLen = 1 + 32 + 32;  // [ver][priv 32][pub 32]
constexpr const char* kLongName = "4stro-031";
constexpr const char* kShortName = "4S31";

// Source bytes for the deterministic "rng" used to recompute a public key from a stored private key.
const uint8_t* gReplaySrc = nullptr;

void replayRng(uint8_t* buf, size_t len) {
  if (gReplaySrc) memcpy(buf, gReplaySrc, len);
}

bool allZero(const uint8_t* p, size_t n) {
  uint8_t acc = 0;
  for (size_t i = 0; i < n; i++) acc |= p[i];
  return acc == 0;
}

// Checks that pub == X25519(priv, 9); x25519Keypair re-clamps priv, which is idempotent for a clamped key.
bool keypairConsistent(const uint8_t priv[32], const uint8_t pub[32]) {
  uint8_t p2[32], q2[32];
  gReplaySrc = priv;
  x25519Keypair(replayRng, p2, q2);
  gReplaySrc = nullptr;
  bool ok = memcmp(p2, priv, 32) == 0 && memcmp(q2, pub, 32) == 0;
  secureZero(p2, sizeof p2);
  return ok;
}

bool reservedNodeNum(uint32_t n) { return n < 4 || n == 0xFFFFFFFFu; }

// Node number = last 4 bytes of the factory MAC. If the MAC is unreadable or yields a reserved value,
// fall back to an FNV-1a hash of the (persisted, stable) public key, nudged out of the reserved range.
uint32_t deriveNodeNum(const uint8_t pub[32]) {
  uint8_t mac[6] = {};
  if (esp_efuse_mac_get_default(mac) == ESP_OK) {
    uint32_t n = (uint32_t)mac[2] << 24 | (uint32_t)mac[3] << 16 | (uint32_t)mac[4] << 8 | mac[5];
    if (!reservedNodeNum(n)) return n;
  }
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < 32; i++) h = (h ^ pub[i]) * 16777619u;
  if (h == 0xFFFFFFFFu) h = 0xFFFFFFFEu;
  if (h < 4) h += 4;
  return h;
}
}  // namespace

void hwRandomBytes(uint8_t* buf, size_t len) {
  // Wi-Fi/BT are off, so the RNG is only truly random while the bootloader entropy source is enabled.
  bootloader_random_enable();
  esp_fill_random(buf, len);
  bootloader_random_disable();
}

IdentityStatus loadOrCreateIdentity(Kv& kv, Identity& me) {
  IdentityStatus status = IdentityStatus::Loaded;
  uint8_t rec[kRecordLen];
  bool valid = kv.get(kKeyName, rec, sizeof rec) && rec[0] == kRecordVersion &&
               !allZero(rec + 1, 32) && keypairConsistent(rec + 1, rec + 33);
  if (valid) {
    memcpy(me.priv, rec + 1, 32);
    memcpy(me.pub, rec + 33, 32);
  } else {
    // Absent or corrupt record: generate once with hardware entropy and persist.
    x25519Keypair(hwRandomBytes, me.priv, me.pub);
    rec[0] = kRecordVersion;
    memcpy(rec + 1, me.priv, 32);
    memcpy(rec + 33, me.pub, 32);
    status = kv.put(kKeyName, rec, sizeof rec) ? IdentityStatus::Created : IdentityStatus::CreatedNotSaved;
  }
  secureZero(rec, sizeof rec);

  me.nodeNum = deriveNodeNum(me.pub);
  memset(me.longName, 0, sizeof me.longName);
  memset(me.shortName, 0, sizeof me.shortName);
  strncpy(me.longName, kLongName, sizeof me.longName - 1);
  strncpy(me.shortName, kShortName, sizeof me.shortName - 1);
  return status;
}
