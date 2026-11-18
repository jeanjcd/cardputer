#include <unity.h>
#include "mesh/region.h"

void setUp() {}
void tearDown() {}

void test_djb2_longfast() { TEST_ASSERT_EQUAL_UINT32(130429955u, djb2("LongFast")); }
void test_freq_anz() { TEST_ASSERT_FLOAT_WITHIN(0.001f, 919.875f, regionFreqMHz(Region::ANZ, "LongFast", 250.0f)); }
void test_freq_br902() { TEST_ASSERT_FLOAT_WITHIN(0.001f, 903.875f, regionFreqMHz(Region::BR_902, "LongFast", 250.0f)); }
void test_channel_hash_default() { TEST_ASSERT_EQUAL_HEX8(0x08, channelHash("LongFast", kDefaultKey, 16)); }

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_djb2_longfast);
  RUN_TEST(test_freq_anz);
  RUN_TEST(test_freq_br902);
  RUN_TEST(test_channel_hash_default);
  return UNITY_END();
}
