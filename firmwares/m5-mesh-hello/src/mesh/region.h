#pragma once
#include <stddef.h>
#include <stdint.h>

/// Supported LoRa regions: the Meshtastic regions whose LongFast slot falls inside the Cap LoRa-1262 rated band
/// (868–923 MHz), plus the 863–868 MHz regions flagged experimental (outside the Cap's rating; the SX1262 tunes them).
/// The numeric value is the persisted region id (0 = ANZ and 1 = BR_902 match the earlier builds).
enum class Region : uint8_t {
  ANZ = 0, BR_902 = 1, US = 2, EU_868 = 3, RU = 4, KR = 5, UA_868 = 6, MY_919 = 7, SG_923 = 8, PH_868 = 9, PH_915 = 10,
  IN = 11, NZ_865 = 12, NP_865 = 13, KZ_863 = 14
};

/// One row of the Meshtastic region table (firmware src/mesh/RadioInterface.cpp, RDEF).
struct RegionInfo {
  Region id;
  const char* name;
  float freqStart, freqEnd;  // MHz
  uint8_t dutyCycle;         // percent; 100 = unrestricted
  float spacing;             // MHz between channels
  int8_t powerLimit;         // dBm; 0 = unknown (Meshtastic then uses 17)
  bool experimental;         // LongFast slot outside the Cap's rated 868–923 MHz (may perform worse)
};

/// Default LongFast channel key (AES-128).
extern const uint8_t kDefaultKey[16];

/// Number of supported regions.
size_t regionCount();
/// Region table row i (0..regionCount()-1), in Config-screen order (ANZ first).
const RegionInfo& regionAt(size_t i);
/// Table row for a region (ANZ's row for an out-of-range value).
const RegionInfo& regionInfo(Region r);
/// Maps a persisted id byte to a region; false for unknown ids.
bool regionFromId(uint8_t id, Region& out);
/// The region after r in table order (wraps around).
Region regionNext(Region r);

/// DJB2 hash of a C string (Meshtastic channel-to-slot hash).
uint32_t djb2(const char* s);

/// Centre frequency in MHz for a channel name, region and bandwidth in kHz, computed exactly like Meshtastic:
/// numChannels = floor((end-start)/(spacing + bw/1000)); freq = start + bw/2000 + (djb2(name) % numChannels) * bw/1000.
float regionFreqMHz(Region region, const char* channelName, float bwKHz);

/// Effective TX power: min(userDbm, region limit (17 if the limit is 0), 22), and at least 2 dBm.
int8_t effectiveTxPower(Region region, int userDbm);

/// One-byte channel hash: XOR of name bytes XOR key bytes.
uint8_t channelHash(const char* name, const uint8_t* key, size_t keyLen);

/// LoRa time-on-air in microseconds (Semtech SX126x formula) for a payload of len bytes.
/// crDenom is 5..8 (4/5..4/8); explicitHeader, crc and lowDataRateOpt as configured on the radio.
uint32_t loraAirtimeUs(uint8_t sf, uint32_t bwHz, uint8_t crDenom, uint16_t preamble, bool explicitHeader, bool crc,
                       bool lowDataRateOpt, size_t len);
/// Time-on-air of a LongFast frame (SF11, BW 250 kHz, CR 4/5, preamble 16, explicit header, CRC on, LDRO off).
uint32_t longFastAirtimeUs(size_t len);

/// Rolling one-hour airtime accounting for duty-cycle-limited regions, in 60 one-minute buckets (conservative:
/// the window covers the current minute plus the previous 59). Time is a 64-bit millisecond uptime (no wraparound).
class AirtimeBudget {
 public:
  /// Records airMs of transmission at nowMs.
  void record(uint64_t nowMs, uint32_t airMs);
  /// Airtime (ms) used within the window ending at nowMs.
  uint32_t usedMs(uint64_t nowMs) const;
  /// True if sending airMs more keeps usage within dutyPct% of one hour (always true for dutyPct >= 100).
  bool allows(uint64_t nowMs, uint32_t airMs, uint8_t dutyPct) const;

 private:
  static constexpr size_t kBuckets = 60;
  uint64_t minute_[kBuckets] = {};
  uint32_t ms_[kBuckets] = {};
  bool used_[kBuckets] = {};
};
