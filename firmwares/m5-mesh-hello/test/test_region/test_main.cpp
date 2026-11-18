// SPDX-License-Identifier: GPL-3.0-or-later
#include <unity.h>
#include <string.h>
#include "mesh/region.h"

void setUp() {}
void tearDown() {}

static float lf(Region r) { return regionFreqMHz(r, "LongFast", 250.0f); }

void test_djb2_longfast() { TEST_ASSERT_EQUAL_UINT32(130429955u, djb2("LongFast")); }
void test_freq_anz() { TEST_ASSERT_FLOAT_WITHIN(0.001f, 919.875f, lf(Region::ANZ)); }
void test_freq_br902() { TEST_ASSERT_FLOAT_WITHIN(0.001f, 903.875f, lf(Region::BR_902)); }
void test_channel_hash_default() { TEST_ASSERT_EQUAL_HEX8(0x08, channelHash("LongFast", kDefaultKey, 16)); }

// Expected LongFast slots, computed with Meshtastic's float formula (see report): hash 130429955 % numChannels.
void test_freq_all_regions() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 906.875f, lf(Region::US));      // 104 ch, slot 19
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 869.525f, lf(Region::EU_868));  // 1 ch
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 869.075f, lf(Region::RU));      // 2 ch, slot 1
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 922.875f, lf(Region::KR));      // 12 ch, slot 11
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 868.375f, lf(Region::UA_868));  // 2 ch, slot 1
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 922.875f, lf(Region::MY_919));  // 20 ch, slot 15
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 917.875f, lf(Region::SG_923));  // 32 ch, slot 3
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 868.125f, lf(Region::PH_868));  // 5 ch, slot 0
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 917.875f, lf(Region::PH_915));  // 12 ch, slot 11
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 865.875f, lf(Region::IN));      // 8 ch, slot 3 (experimental)
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 864.875f, lf(Region::NZ_865));  // 16 ch, slot 3 (experimental)
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 867.875f, lf(Region::NP_865));  // 12 ch, slot 11 (experimental)
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 866.875f, lf(Region::KZ_863));  // 20 ch, slot 15 (experimental)
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 923.375f, lf(Region::JP));      // 12 ch, slot 11 (experimental)
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 923.875f, lf(Region::TW));      // 20 ch, slot 15 (experimental)
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 923.875f, lf(Region::TH));      // 20 ch, slot 15 (experimental)
}

void test_every_region_in_cap_band() {
  TEST_ASSERT_EQUAL_UINT32(18, regionCount());
  size_t experimental = 0;
  for (size_t i = 0; i < regionCount(); i++) {
    float f = lf(regionAt(i).id);
    if (regionAt(i).experimental) {
      experimental++;
      TEST_ASSERT_TRUE_MESSAGE((f >= 860.0f && f < 868.0f) || (f > 923.0f && f <= 925.0f), regionAt(i).name);
    } else {
      TEST_ASSERT_TRUE_MESSAGE(f >= 868.0f && f <= 923.0f, regionAt(i).name);
    }
    Region back;
    TEST_ASSERT_TRUE(regionFromId((uint8_t)regionAt(i).id, back));
    TEST_ASSERT_TRUE(back == regionAt(i).id);
  }
  TEST_ASSERT_EQUAL_UINT32(7, experimental);
  TEST_ASSERT_TRUE(regionInfo(Region::TH).experimental);
  TEST_ASSERT_EQUAL_UINT8(10, regionInfo(Region::TH).dutyCycle);
  TEST_ASSERT_TRUE(regionInfo(Region::IN).experimental);
  TEST_ASSERT_FALSE(regionInfo(Region::ANZ).experimental);
  TEST_ASSERT_TRUE(regionAt(0).id == Region::ANZ);  // default first
  TEST_ASSERT_EQUAL_UINT8(0, (uint8_t)Region::ANZ);     // ids of earlier builds
  TEST_ASSERT_EQUAL_UINT8(1, (uint8_t)Region::BR_902);
  Region r;
  TEST_ASSERT_FALSE(regionFromId(18, r));
  // cycling visits every region and wraps
  Region c = Region::ANZ;
  for (size_t i = 0; i < regionCount(); i++) c = regionNext(c);
  TEST_ASSERT_TRUE(c == Region::ANZ);
  // regionPrev is the inverse of regionNext and wraps the other way
  for (size_t i = 0; i < regionCount(); i++) TEST_ASSERT_TRUE(regionPrev(regionNext(regionAt(i).id)) == regionAt(i).id);
  TEST_ASSERT_TRUE(regionPrev(Region::ANZ) == regionAt(regionCount() - 1).id);
  TEST_ASSERT_EQUAL_STRING("EU_868", regionInfo(Region::EU_868).name);
  TEST_ASSERT_EQUAL_UINT8(10, regionInfo(Region::EU_868).dutyCycle);
}

void test_effective_power() {
  TEST_ASSERT_EQUAL_INT8(22, effectiveTxPower(Region::ANZ, 22));     // limit 30, cap 22
  TEST_ASSERT_EQUAL_INT8(10, effectiveTxPower(Region::ANZ, 10));
  TEST_ASSERT_EQUAL_INT8(14, effectiveTxPower(Region::UA_868, 22));  // limit 14
  TEST_ASSERT_EQUAL_INT8(20, effectiveTxPower(Region::SG_923, 22));  // limit 20
  TEST_ASSERT_EQUAL_INT8(22, effectiveTxPower(Region::EU_868, 22));  // limit 27, cap 22
  TEST_ASSERT_EQUAL_INT8(2, effectiveTxPower(Region::US, 0));
  TEST_ASSERT_EQUAL_INT8(13, effectiveTxPower(Region::JP, 22));      // JP limit 13
}

// Semtech SX126x time-on-air, SF11 / BW 250 kHz / CR 4/5 / preamble 16 / explicit header / CRC on / LDRO off:
// Tsym = 2^11 / 250 kHz = 8.192 ms; Tpreamble = (16 + 4.25) * Tsym = 165.888 ms;
// payloadSym = 8 + max(ceil((8*PL - 4*11 + 28 + 16) / (4*11)), 0) * 5.
//   PL = 0   -> 8 sym   -> 65.536 ms   -> total 231.424 ms
//   PL = 16  -> 23 sym  -> 188.416 ms  -> total 354.304 ms
//   PL = 50  -> 58 sym  -> 475.136 ms  -> total 641.024 ms
//   PL = 255 -> 243 sym -> 1990.656 ms -> total 2156.544 ms
void test_longfast_airtime() {
  TEST_ASSERT_EQUAL_UINT32(231424, longFastAirtimeUs(0));
  TEST_ASSERT_EQUAL_UINT32(354304, longFastAirtimeUs(16));
  TEST_ASSERT_EQUAL_UINT32(641024, longFastAirtimeUs(50));
  TEST_ASSERT_EQUAL_UINT32(2156544, longFastAirtimeUs(255));
  // SF12 BW125 with LDRO (Tsym 32.768 ms), PL 10, CR 4/5, preamble 8: num = 80-48+28+16 = 76, den = 40 -> 2 blocks
  // -> 18 sym = 589.824 ms; preamble 12.25 * 32.768 = 401.408 ms; total 991.232 ms.
  TEST_ASSERT_EQUAL_UINT32(991232, loraAirtimeUs(12, 125000, 5, 8, true, true, true, 10));
  TEST_ASSERT_EQUAL_UINT32(0, loraAirtimeUs(4, 125000, 5, 8, true, true, false, 10));
}

void test_airtime_budget() {
  AirtimeBudget b;
  const uint64_t t0 = 5ULL * 3600 * 1000;
  TEST_ASSERT_TRUE(b.allows(t0, 1000000, 100));      // unrestricted region
  // 10%: 360 000 ms per hour
  TEST_ASSERT_TRUE(b.allows(t0, 360000, 10));
  TEST_ASSERT_FALSE(b.allows(t0, 360001, 10));
  for (int i = 0; i < 100; i++) b.record(t0 + i * 1000, 3500);  // 350 s within two minutes
  TEST_ASSERT_EQUAL_UINT32(350000, b.usedMs(t0 + 100000));
  TEST_ASSERT_TRUE(b.allows(t0 + 100000, 10000, 10));
  TEST_ASSERT_FALSE(b.allows(t0 + 100000, 10001, 10));
  TEST_ASSERT_FALSE(b.allows(t0 + 30ULL * 60000, 20000, 10));  // still inside the hour
  // one hour later the old buckets have left the window
  TEST_ASSERT_EQUAL_UINT32(0, b.usedMs(t0 + 62ULL * 60000));
  TEST_ASSERT_TRUE(b.allows(t0 + 62ULL * 60000, 300000, 10));
  // 1%: 36 000 ms per hour
  AirtimeBudget u;
  u.record(t0, 35000);
  TEST_ASSERT_TRUE(u.allows(t0, 1000, 1));
  TEST_ASSERT_FALSE(u.allows(t0, 1001, 1));
}

// A frame must count for at least a full hour: recorded at t0+59 s, it still counts at t0+60 min+1 s and is released
// only once it is >= 60 min old.
void test_airtime_budget_full_hour() {
  AirtimeBudget b;
  const uint64_t t0 = 5ULL * 3600 * 1000;  // minute-aligned
  const uint64_t sent = t0 + 59000;
  b.record(sent, 30000);
  TEST_ASSERT_EQUAL_UINT32(30000, b.usedMs(t0 + 60ULL * 60000 + 1000));
  TEST_ASSERT_EQUAL_UINT32(30000, b.usedMs(sent + 60ULL * 60000 - 1));  // 1 ms short of an hour
  TEST_ASSERT_FALSE(b.allows(sent + 60ULL * 60000 - 1, 6001, 1));        // UA_868: 36 s budget still holds it
  uint64_t released = 0;
  for (uint64_t t = sent; t < sent + 2ULL * 3600 * 1000; t += 1000)
    if (b.usedMs(t) == 0) { released = t; break; }
  TEST_ASSERT_TRUE(released != 0);
  TEST_ASSERT_TRUE(released - sent >= 60ULL * 60000);
  TEST_ASSERT_TRUE(released - sent <= 61ULL * 60000);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_djb2_longfast);
  RUN_TEST(test_freq_anz);
  RUN_TEST(test_freq_br902);
  RUN_TEST(test_channel_hash_default);
  RUN_TEST(test_freq_all_regions);
  RUN_TEST(test_every_region_in_cap_band);
  RUN_TEST(test_effective_power);
  RUN_TEST(test_longfast_airtime);
  RUN_TEST(test_airtime_budget);
  RUN_TEST(test_airtime_budget_full_hour);
  return UNITY_END();
}
