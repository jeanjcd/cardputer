#include "settings.h"
#include <string.h>
#include "text.h"

namespace {
constexpr uint8_t kVer = 1;
constexpr size_t kOffGreeting = 4, kLenGreeting = 201;
constexpr size_t kOffLong = kOffGreeting + kLenGreeting, kLenLong = 25;
constexpr size_t kOffShort = kOffLong + kLenLong, kLenShort = 5;
static_assert(kOffShort + kLenShort == kSettingsBlobLen, "settings layout");

// "Saudações de " ... "! 👋 (Cardputer-Adv)" in UTF-8.
constexpr const char* kGreetPrefix = "Sauda\xC3\xA7\xC3\xB5" "es de ";
constexpr const char* kGreetSuffix = "! \xF0\x9F\x91\x8B (Cardputer-Adv)";

// Reads a NUL-padded string field; false if no NUL inside the field.
bool readField(const uint8_t* in, size_t len, char* out) {
  if (memchr(in, 0, len) == nullptr) return false;
  memcpy(out, in, len);
  return true;
}
}  // namespace

uint8_t regionToByte(Region r) {
  switch (r) {
    case Region::BR_902: return 1;
    case Region::ANZ:
    default: return 0;
  }
}

bool regionFromByte(uint8_t b, Region& out) {
  switch (b) {
    case 0: out = Region::ANZ; return true;
    case 1: out = Region::BR_902; return true;
    default: return false;
  }
}

bool validTxPower(int p) { return p >= Settings::kMinPower && p <= Settings::kMaxPower; }
bool validLongName(const char* s) { return utf8CleanText(s, Settings::kMaxLongName); }
bool validShortName(const char* s) { return utf8CleanText(s, Settings::kMaxShortName); }
bool validGreeting(const char* s) { return utf8CleanText(s, Settings::kMaxGreeting); }

void settingsDefaults(Settings& s) {
  memset(&s, 0, sizeof s);
  s.region = Region::ANZ;
  s.txPower = 22;
  s.customGreeting = false;
  utf8Copy(s.longName, sizeof s.longName, "4stro-031");
  utf8Copy(s.shortName, sizeof s.shortName, "4S31");
}

void settingsEncode(const Settings& s, uint8_t out[kSettingsBlobLen]) {
  memset(out, 0, kSettingsBlobLen);
  out[0] = kVer;
  out[1] = regionToByte(s.region);
  out[2] = (uint8_t)s.txPower;
  out[3] = s.customGreeting ? 1 : 0;
  if (s.customGreeting) memcpy(out + kOffGreeting, s.greeting, strnlen(s.greeting, kLenGreeting - 1));
  memcpy(out + kOffLong, s.longName, strnlen(s.longName, kLenLong - 1));
  memcpy(out + kOffShort, s.shortName, strnlen(s.shortName, kLenShort - 1));
}

bool settingsDecode(const uint8_t* in, size_t len, Settings& s) {
  if (!in || len != kSettingsBlobLen || in[0] != kVer) return false;
  Settings t;
  memset(&t, 0, sizeof t);
  if (!regionFromByte(in[1], t.region)) return false;
  t.txPower = (int8_t)in[2];
  if (!validTxPower(t.txPower)) return false;
  if (in[3] > 1) return false;
  t.customGreeting = in[3] == 1;
  if (!readField(in + kOffGreeting, kLenGreeting, t.greeting)) return false;
  if (!readField(in + kOffLong, kLenLong, t.longName)) return false;
  if (!readField(in + kOffShort, kLenShort, t.shortName)) return false;
  if (t.customGreeting ? !validGreeting(t.greeting) : t.greeting[0] != 0) return false;
  if (!validLongName(t.longName) || !validShortName(t.shortName)) return false;
  s = t;
  return true;
}

bool settingsLoad(Kv& kv, Settings& s) {
  uint8_t b[kSettingsBlobLen];
  if (kv.get("settings", b, sizeof b) && settingsDecode(b, sizeof b, s)) return true;
  settingsDefaults(s);
  uint8_t r = 0;  // the diagnostic build stored the region alone
  if (kv.get("region", &r, 1)) regionFromByte(r, s.region);
  return false;
}

bool settingsSave(Kv& kv, const Settings& s) {
  uint8_t b[kSettingsBlobLen];
  settingsEncode(s, b);
  return kv.put("settings", b, sizeof b);
}

void composeGreeting(const Settings& s, char* out, size_t cap) {
  if (!out || cap == 0) return;
  if (s.customGreeting) { utf8Copy(out, cap, s.greeting); return; }
  char tmp[Settings::kMaxGreeting + 1];
  size_t n = 0;
  const char* parts[3] = {kGreetPrefix, s.longName, kGreetSuffix};
  for (const char* p : parts)
    for (size_t i = 0; p[i] && n + 1 < sizeof tmp; i++) tmp[n++] = p[i];
  tmp[n] = 0;
  utf8Copy(out, cap, tmp);
}
