#include <unity.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>
#include "mesh/hwmodel.h"
#include "mesh/kv.h"
#include "mesh/ratelimit.h"
#include "mesh/settings.h"
#include "mesh/text.h"

struct FakeKv : Kv {
  std::map<std::string, std::vector<uint8_t>> m;
  bool failPut = false;
  bool get(const char* k, void* buf, size_t len) override {
    auto it = m.find(k);
    if (it == m.end() || it->second.size() != len) return false;
    memcpy(buf, it->second.data(), len);
    return true;
  }
  bool put(const char* k, const void* buf, size_t len) override {
    if (failPut || strlen(k) > 15) return false;
    const uint8_t* p = (const uint8_t*)buf;
    m[k] = std::vector<uint8_t>(p, p + len);
    return true;
  }
};

void setUp() {}
void tearDown() {}

// ---- greeting text ----

void test_default_greeting_exact_utf8() {
  Settings s; settingsDefaults(s);
  char g[201];
  composeGreeting(s, g, sizeof g);
  TEST_ASSERT_EQUAL_STRING("Sauda\xC3\xA7\xC3\xB5" "es de 4stro-031! \xF0\x9F\x91\x8B (Cardputer-Adv)", g);
}

void test_greeting_follows_long_name_unless_custom() {
  Settings s; settingsDefaults(s);
  strcpy(s.longName, "Zeca");
  char g[201];
  composeGreeting(s, g, sizeof g);
  TEST_ASSERT_EQUAL_STRING("Sauda\xC3\xA7\xC3\xB5" "es de Zeca! \xF0\x9F\x91\x8B (Cardputer-Adv)", g);
  s.customGreeting = true;
  strcpy(s.greeting, "oi pessoal");
  composeGreeting(s, g, sizeof g);
  TEST_ASSERT_EQUAL_STRING("oi pessoal", g);
  // small buffer: cut at a character boundary, never inside the emoji
  s.customGreeting = false;
  char small[21];  // prefix is 15 bytes + "Zeca" 4 = 19, then '!'
  composeGreeting(s, small, sizeof small);
  TEST_ASSERT_EQUAL_STRING("Sauda\xC3\xA7\xC3\xB5" "es de Zeca!", small);
  char tiny[7];  // "Sauda" + first byte of the 2-byte cedilla would split it
  composeGreeting(s, tiny, sizeof tiny);
  TEST_ASSERT_EQUAL_STRING("Sauda", tiny);
}

// ---- settings ----

void test_settings_defaults_and_roundtrip() {
  Settings s; settingsDefaults(s);
  TEST_ASSERT_TRUE(s.region == Region::ANZ);
  TEST_ASSERT_EQUAL_INT8(22, s.txPower);
  TEST_ASSERT_FALSE(s.customGreeting);
  TEST_ASSERT_EQUAL_STRING("4stro-031", s.longName);
  TEST_ASSERT_EQUAL_STRING("4S31", s.shortName);

  s.region = Region::BR_902; s.txPower = 10; s.customGreeting = true;
  strcpy(s.greeting, "Ol\xC3\xA1!"); strcpy(s.longName, "Meu No"); strcpy(s.shortName, "MN");
  FakeKv kv;
  TEST_ASSERT_TRUE(settingsSave(kv, s));
  TEST_ASSERT_EQUAL_UINT32(kSettingsBlobLen, kv.m["settings"].size());
  TEST_ASSERT_EQUAL_UINT8(1, kv.m["settings"][1]);  // region persisted as explicit byte 1 = BR_902
  Settings t;
  TEST_ASSERT_TRUE(settingsLoad(kv, t));
  TEST_ASSERT_TRUE(t.region == Region::BR_902);
  TEST_ASSERT_EQUAL_INT8(10, t.txPower);
  TEST_ASSERT_TRUE(t.customGreeting);
  TEST_ASSERT_EQUAL_STRING("Ol\xC3\xA1!", t.greeting);
  TEST_ASSERT_EQUAL_STRING("Meu No", t.longName);
  TEST_ASSERT_EQUAL_STRING("MN", t.shortName);
  kv.failPut = true;
  TEST_ASSERT_FALSE(settingsSave(kv, s));
}

void test_settings_reject_invalid_blobs() {
  Settings def; settingsDefaults(def);
  uint8_t b[kSettingsBlobLen];
  settingsEncode(def, b);
  Settings t;
  TEST_ASSERT_TRUE(settingsDecode(b, sizeof b, t));
  TEST_ASSERT_FALSE(settingsDecode(b, sizeof b - 1, t));
  uint8_t c[kSettingsBlobLen];
  memcpy(c, b, sizeof c); c[0] = 2;  TEST_ASSERT_FALSE(settingsDecode(c, sizeof c, t));  // version
  memcpy(c, b, sizeof c); c[1] = 18; TEST_ASSERT_FALSE(settingsDecode(c, sizeof c, t));  // region
  memcpy(c, b, sizeof c); c[2] = 1;  TEST_ASSERT_FALSE(settingsDecode(c, sizeof c, t));  // power < 2
  memcpy(c, b, sizeof c); c[2] = 23; TEST_ASSERT_FALSE(settingsDecode(c, sizeof c, t));  // power > 22
  memcpy(c, b, sizeof c); c[3] = 1;  TEST_ASSERT_FALSE(settingsDecode(c, sizeof c, t));  // custom but empty greeting
  memcpy(c, b, sizeof c); c[3] = 3;  TEST_ASSERT_FALSE(settingsDecode(c, sizeof c, t));  // unknown flags
  memcpy(c, b, sizeof c); memset(c + 4 + 201, 'x', 25);  // long name without NUL
  TEST_ASSERT_FALSE(settingsDecode(c, sizeof c, t));
  memcpy(c, b, sizeof c); memset(c + 4 + 201, 0, 25);  // empty long name
  TEST_ASSERT_FALSE(settingsDecode(c, sizeof c, t));
  memcpy(c, b, sizeof c); c[4 + 201] = 0x01;  // control character
  TEST_ASSERT_FALSE(settingsDecode(c, sizeof c, t));
  memcpy(c, b, sizeof c); c[4 + 201 + 25] = (char)0xC3; c[4 + 201 + 26] = 0;  // truncated UTF-8 short name
  TEST_ASSERT_FALSE(settingsDecode(c, sizeof c, t));
  // corrupt stored blob loads as defaults
  FakeKv kv; kv.m["settings"] = std::vector<uint8_t>(c, c + sizeof c);
  Settings l; l.txPower = 5;
  TEST_ASSERT_FALSE(settingsLoad(kv, l));
  TEST_ASSERT_EQUAL_INT8(22, l.txPower);
  TEST_ASSERT_EQUAL_STRING("4stro-031", l.longName);
}

void test_settings_legacy_region_key() {
  FakeKv kv; kv.m["region"] = {1};
  Settings s;
  TEST_ASSERT_FALSE(settingsLoad(kv, s));
  TEST_ASSERT_TRUE(s.region == Region::BR_902);
  kv.m["region"] = {99};
  TEST_ASSERT_FALSE(settingsLoad(kv, s));
  TEST_ASSERT_TRUE(s.region == Region::ANZ);
}

void test_name_and_power_validation() {
  TEST_ASSERT_TRUE(validLongName("4stro-031"));
  TEST_ASSERT_TRUE(validLongName("123456789012345678901234"));     // 24 bytes
  TEST_ASSERT_FALSE(validLongName("1234567890123456789012345"));   // 25 bytes
  TEST_ASSERT_FALSE(validLongName(""));
  TEST_ASSERT_FALSE(validLongName("tab\there"));
  TEST_ASSERT_TRUE(validLongName("Jo\xC3\xA3o"));
  TEST_ASSERT_FALSE(validLongName("bad\xC3"));
  TEST_ASSERT_FALSE(validLongName("\xC0\xAF"));  // overlong
  TEST_ASSERT_TRUE(validShortName("4S31"));
  TEST_ASSERT_FALSE(validShortName("4S31X"));
  TEST_ASSERT_TRUE(validShortName("\xF0\x9F\x91\x8B"));  // one emoji = 4 bytes
  TEST_ASSERT_TRUE(validTxPower(2));
  TEST_ASSERT_TRUE(validTxPower(22));
  TEST_ASSERT_FALSE(validTxPower(1));
  TEST_ASSERT_FALSE(validTxPower(23));
  char g[202]; memset(g, 'a', 199); g[199] = 0;
  TEST_ASSERT_TRUE(validGreeting(g));
  g[199] = 'a'; g[200] = 0;
  TEST_ASSERT_FALSE(validGreeting(g));
  Region r;
  TEST_ASSERT_TRUE(regionFromByte(0, r) && r == Region::ANZ);
  TEST_ASSERT_TRUE(regionFromByte(1, r) && r == Region::BR_902);
  TEST_ASSERT_TRUE(regionFromByte(2, r) && r == Region::US);
  TEST_ASSERT_TRUE(regionFromByte(3, r) && r == Region::EU_868);
  TEST_ASSERT_TRUE(regionFromByte(11, r) && r == Region::IN);
  TEST_ASSERT_TRUE(regionFromByte(17, r) && r == Region::TH);
  TEST_ASSERT_FALSE(regionFromByte(18, r));
  TEST_ASSERT_FALSE(regionFromByte(255, r));
  TEST_ASSERT_EQUAL_UINT8(0, regionToByte(Region::ANZ));
  TEST_ASSERT_EQUAL_UINT8(1, regionToByte(Region::BR_902));
}

// ---- display text ----

void test_cp437_display_conversion() {
  char out[64];
  // "Saudações de X! 👋" : ç -> 0x87, õ -> 'o', emoji -> '?'
  size_t n = utf8ToCp437("Sauda\xC3\xA7\xC3\xB5" "es! \xF0\x9F\x91\x8B", out, sizeof out);
  TEST_ASSERT_EQUAL_UINT32(12, n);
  TEST_ASSERT_EQUAL_STRING("Sauda\x87oes! ?", out);
  utf8ToCp437("\xC3\xA9\xC3\xA1\xC3\xA3\xC3\x87", out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("\x82\xA0" "a\x80", out);
  // control bytes and invalid sequences become '?', variation selector dropped
  utf8ToCp437("a\x01" "b\xFF" "c\xE2\x9D\xA4\xEF\xB8\x8F", out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("a?b?c?", out);
  // truncated multibyte at end of string does not read past NUL
  utf8ToCp437("x\xE2\x82", out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("x??", out);
  // cap respected
  char small[4];
  TEST_ASSERT_EQUAL_UINT32(3, utf8ToCp437("abcdef", small, sizeof small));
  TEST_ASSERT_EQUAL_STRING("abc", small);
  TEST_ASSERT_EQUAL_UINT32(0, utf8ToCp437(nullptr, small, sizeof small));
}

void test_text_editing_helpers() {
  char s[8] = "";
  TEST_ASSERT_TRUE(textAppendAscii(s, sizeof s, 3, 'a'));
  TEST_ASSERT_TRUE(textAppendAscii(s, sizeof s, 3, 'b'));
  TEST_ASSERT_TRUE(textAppendAscii(s, sizeof s, 3, 'c'));
  TEST_ASSERT_FALSE(textAppendAscii(s, sizeof s, 3, 'd'));  // max bytes
  TEST_ASSERT_FALSE(textAppendAscii(s, sizeof s, 10, '\n'));
  TEST_ASSERT_EQUAL_STRING("abc", s);
  char t[4] = "ab";
  TEST_ASSERT_TRUE(textAppendAscii(t, sizeof t, 100, 'c'));
  TEST_ASSERT_FALSE(textAppendAscii(t, sizeof t, 100, 'd'));  // buffer cap
  char u[16] = "a\xC3\xA7\xF0\x9F\x91\x8B";
  utf8PopBack(u); TEST_ASSERT_EQUAL_STRING("a\xC3\xA7", u);
  utf8PopBack(u); TEST_ASSERT_EQUAL_STRING("a", u);
  utf8PopBack(u); TEST_ASSERT_EQUAL_STRING("", u);
  utf8PopBack(u); TEST_ASSERT_EQUAL_STRING("", u);
  char v[8] = "a\x80\x80";  // stray continuation bytes: removed one at a time
  utf8PopBack(v); TEST_ASSERT_EQUAL_STRING("a\x80", v);
  char w[6];
  utf8Copy(w, sizeof w, "abc\xF0\x9F\x91\x8B");
  TEST_ASSERT_EQUAL_STRING("abc", w);
  utf8Copy(w, 5, "abc\xC3\xA7");
  TEST_ASSERT_EQUAL_STRING("abc", w);
  utf8Copy(w, sizeof w, "ab\xC3\xA7zz");
  TEST_ASSERT_EQUAL_STRING("ab\xC3\xA7z", w);
}

// ---- hardware models ----

void test_hw_model_names() {
  char b[20];
  hwModelName(112, b, sizeof b); TEST_ASSERT_EQUAL_STRING("CARDPUTER_ADV", b);
  TEST_ASSERT_EQUAL_STRING("ESP32-S3", hwChipFamily(112));
  hwModelName(43, b, sizeof b); TEST_ASSERT_EQUAL_STRING("HELTEC_V3", b);
  hwModelName(110, b, sizeof b); TEST_ASSERT_EQUAL_STRING("HELTEC_V4", b);
  hwModelName(9, b, sizeof b); TEST_ASSERT_EQUAL_STRING("RAK4631", b);
  TEST_ASSERT_EQUAL_STRING("nRF52", hwChipFamily(9));
  hwModelName(7, b, sizeof b); TEST_ASSERT_EQUAL_STRING("T_ECHO", b);
  hwModelName(4, b, sizeof b); TEST_ASSERT_EQUAL_STRING("TBEAM", b);
  TEST_ASSERT_EQUAL_STRING("ESP32", hwChipFamily(4));
  hwModelName(71, b, sizeof b); TEST_ASSERT_EQUAL_STRING("TRACKER_T1000_E", b);
  hwModelName(50, b, sizeof b); TEST_ASSERT_EQUAL_STRING("T_DECK", b);
  hwModelName(255, b, sizeof b); TEST_ASSERT_EQUAL_STRING("PRIVATE_HW", b);
  hwModelName(4242, b, sizeof b); TEST_ASSERT_EQUAL_STRING("HW#4242", b);
  TEST_ASSERT_EQUAL_STRING("", hwChipFamily(4242));
  char tiny[4];
  hwModelName(112, tiny, sizeof tiny); TEST_ASSERT_EQUAL_STRING("CAR", tiny);
}

// ---- rate limiter ----

void test_rate_limiter() {
  NodeRateLimiter r;
  const uint32_t P = 12u * 3600u * 1000u;
  TEST_ASSERT_TRUE(r.allow(1, 1000, P));
  TEST_ASSERT_FALSE(r.allow(1, 2000, P));
  TEST_ASSERT_TRUE(r.allow(2, 2000, P));
  TEST_ASSERT_FALSE(r.allow(1, 1000 + P - 1, P));
  TEST_ASSERT_TRUE(r.allow(1, 1000 + P, P));
  // wraparound
  NodeRateLimiter w;
  TEST_ASSERT_TRUE(w.allow(5, 0xFFFFFF00u, 1000));
  TEST_ASSERT_FALSE(w.allow(5, 0x00000100u, 1000));  // 512 ms later
  TEST_ASSERT_TRUE(w.allow(5, 0x00000400u, 1000));
  // full table recycles the least recently allowed entry
  NodeRateLimiter f;
  for (uint32_t i = 0; i < NodeRateLimiter::kSlots; i++) TEST_ASSERT_TRUE(f.allow(100 + i, i, P));
  TEST_ASSERT_TRUE(f.allow(999, 100, P));     // evicts node 100
  TEST_ASSERT_TRUE(f.allow(100, 101, P));     // forgotten -> allowed again (evicts 101)
  TEST_ASSERT_FALSE(f.allow(102, 102, P));    // still remembered
}

void test_reply_limiter_global_interval() {
  ReplyLimiter r(12u * 3600u * 1000u, 30000);
  TEST_ASSERT_TRUE(r.allow(1, 1000));
  // spoofed / different senders cannot get more than one reply per 30 s
  for (uint32_t n = 2; n < 200; n++) TEST_ASSERT_FALSE(r.allow(n, 1000 + n * 100));
  TEST_ASSERT_TRUE(r.allow(2, 31000));
  TEST_ASSERT_FALSE(r.allow(3, 60999));
  TEST_ASSERT_TRUE(r.allow(3, 61000));
  // the per-node rule still applies after the global interval
  TEST_ASSERT_FALSE(r.allow(1, 200000));
  TEST_ASSERT_TRUE(r.allow(4, 200000));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_default_greeting_exact_utf8);
  RUN_TEST(test_greeting_follows_long_name_unless_custom);
  RUN_TEST(test_settings_defaults_and_roundtrip);
  RUN_TEST(test_settings_reject_invalid_blobs);
  RUN_TEST(test_settings_legacy_region_key);
  RUN_TEST(test_name_and_power_validation);
  RUN_TEST(test_cp437_display_conversion);
  RUN_TEST(test_text_editing_helpers);
  RUN_TEST(test_hw_model_names);
  RUN_TEST(test_rate_limiter);
  RUN_TEST(test_reply_limiter_global_interval);
  return UNITY_END();
}
