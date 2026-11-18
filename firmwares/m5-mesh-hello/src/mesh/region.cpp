#include "mesh/region.h"
#include <math.h>

const uint8_t kDefaultKey[16] = {0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
                                 0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01};

namespace {
// Copied from Meshtastic firmware master src/mesh/RadioInterface.cpp (RDEF name, freq_start, freq_end, duty_cycle,
// spacing, power_limit). Listed: regions whose LongFast slot lies within 868–923 MHz (the Cap's rated band), then the
// 863–868 MHz regions marked experimental. 433 MHz, 470–510 MHz, 2.4 GHz and >923 MHz slots (JP, TW, TH) are excluded.
const RegionInfo kRegions[] = {
    {Region::ANZ, "ANZ", 915.0f, 928.0f, 100, 0.0f, 30, false},          // LongFast 919.875
    {Region::BR_902, "BR_902", 902.0f, 907.5f, 100, 0.0f, 30, false},    // 903.875
    {Region::US, "US", 902.0f, 928.0f, 100, 0.0f, 30, false},            // 906.875
    {Region::EU_868, "EU_868", 869.4f, 869.65f, 10, 0.0f, 27, false},    // 869.525
    {Region::RU, "RU", 868.7f, 869.2f, 100, 0.0f, 20, false},            // 869.075
    {Region::KR, "KR", 920.0f, 923.0f, 100, 0.0f, 23, false},            // 922.875
    {Region::UA_868, "UA_868", 868.0f, 868.6f, 1, 0.0f, 14, false},      // 868.375
    {Region::MY_919, "MY_919", 919.0f, 924.0f, 100, 0.0f, 27, false},    // 922.875
    {Region::SG_923, "SG_923", 917.0f, 925.0f, 100, 0.0f, 20, false},    // 917.875
    {Region::PH_868, "PH_868", 868.0f, 869.4f, 100, 0.0f, 14, false},    // 868.125
    {Region::PH_915, "PH_915", 915.0f, 918.0f, 100, 0.0f, 24, false},    // 917.875
    // Experimental: below the Cap's rated 868 MHz.
    {Region::IN, "IN", 865.0f, 867.0f, 100, 0.0f, 30, true},             // 865.875
    {Region::NZ_865, "NZ_865", 864.0f, 868.0f, 100, 0.0f, 36, true},     // 864.875
    {Region::NP_865, "NP_865", 865.0f, 868.0f, 100, 0.0f, 30, true},     // 867.875
    {Region::KZ_863, "KZ_863", 863.0f, 868.0f, 100, 0.0f, 30, true},     // 866.875
};
constexpr size_t kCount = sizeof kRegions / sizeof kRegions[0];

size_t indexOf(Region r) {
  for (size_t i = 0; i < kCount; i++)
    if (kRegions[i].id == r) return i;
  return 0;
}
}  // namespace

size_t regionCount() { return kCount; }
const RegionInfo& regionAt(size_t i) { return kRegions[i < kCount ? i : 0]; }
const RegionInfo& regionInfo(Region r) { return kRegions[indexOf(r)]; }

bool regionFromId(uint8_t id, Region& out) {
  for (size_t i = 0; i < kCount; i++)
    if ((uint8_t)kRegions[i].id == id) { out = kRegions[i].id; return true; }
  return false;
}

Region regionNext(Region r) { return kRegions[(indexOf(r) + 1) % kCount].id; }

uint32_t djb2(const char* s) {
  uint32_t h = 5381;
  while (*s) h = ((h << 5) + h) + (uint8_t)*s++;
  return h;
}

float regionFreqMHz(Region region, const char* channelName, float bwKHz) {
  const RegionInfo& r = regionInfo(region);
  // Same float arithmetic as RadioInterface::applyModemConfig().
  uint32_t numChannels = (uint32_t)floorf((r.freqEnd - r.freqStart) / (r.spacing + (bwKHz / 1000)));
  if (numChannels == 0) return r.freqStart + (bwKHz / 2000);
  uint32_t channelNum = djb2(channelName) % numChannels;
  return r.freqStart + (bwKHz / 2000) + (channelNum * (bwKHz / 1000));
}

int8_t effectiveTxPower(Region region, int userDbm) {
  int limit = regionInfo(region).powerLimit;
  if (limit == 0) limit = 17;
  int p = userDbm;
  if (p > limit) p = limit;
  if (p > 22) p = 22;
  if (p < 2) p = 2;
  return (int8_t)p;
}

uint8_t channelHash(const char* name, const uint8_t* key, size_t keyLen) {
  uint8_t h = 0;
  while (*name) h ^= (uint8_t)*name++;
  for (size_t i = 0; i < keyLen; i++) h ^= key[i];
  return h;
}

uint32_t loraAirtimeUs(uint8_t sf, uint32_t bwHz, uint8_t crDenom, uint16_t preamble, bool explicitHeader, bool crc,
                       bool lowDataRateOpt, size_t len) {
  if (sf < 5 || sf > 12 || bwHz == 0 || crDenom < 5 || crDenom > 8) return 0;
  // Symbol time in us, kept x4 to represent the 4.25-symbol preamble tail exactly.
  uint64_t tsymUs = ((uint64_t)1 << sf) * 1000000ULL / bwHz;
  uint64_t preambleQuarter = (uint64_t)preamble * 4 + 17;  // (preamble + 4.25) * 4
  int64_t num = 8 * (int64_t)len - 4 * sf + 28 + (crc ? 16 : 0) - (explicitHeader ? 0 : 20);
  int64_t den = 4 * (sf - (lowDataRateOpt ? 2 : 0));
  int64_t blocks = num > 0 ? (num + den - 1) / den : 0;
  uint64_t payloadSym = 8 + (uint64_t)blocks * crDenom;
  return (uint32_t)(preambleQuarter * tsymUs / 4 + payloadSym * tsymUs);
}

uint32_t longFastAirtimeUs(size_t len) { return loraAirtimeUs(11, 250000, 5, 16, true, true, false, len); }

void AirtimeBudget::record(uint64_t nowMs, uint32_t airMs) {
  uint64_t m = nowMs / 60000;
  size_t s = (size_t)(m % kBuckets);
  if (!used_[s] || minute_[s] != m) { used_[s] = true; minute_[s] = m; ms_[s] = 0; }
  ms_[s] += airMs;
}

uint32_t AirtimeBudget::usedMs(uint64_t nowMs) const {
  uint64_t m = nowMs / 60000;
  uint32_t sum = 0;
  for (size_t i = 0; i < kBuckets; i++)
    if (used_[i] && minute_[i] <= m && m - minute_[i] < kBuckets) sum += ms_[i];
  return sum;
}

bool AirtimeBudget::allows(uint64_t nowMs, uint32_t airMs, uint8_t dutyPct) const {
  if (dutyPct >= 100) return true;
  uint64_t budget = (uint64_t)dutyPct * 36000;  // dutyPct% of 3 600 000 ms
  return (uint64_t)usedMs(nowMs) + airMs <= budget;
}
