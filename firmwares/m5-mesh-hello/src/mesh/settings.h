// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "kv.h"
#include "region.h"

/// User settings persisted in Kv under "settings" as a fixed 235-byte blob:
/// [ver=1][region id u8 (Region value: 0=ANZ 1=BR_902 2=US ...)][txPower i8][flags u8: bit0 custom greeting][greeting 201][longName 25][shortName 5].
/// Strings are NUL-padded UTF-8. A blob with a wrong size, version or any invalid field loads as defaults (see settingsLoad).
struct Settings {
  static constexpr size_t kMaxGreeting = 199;  // bytes: ChatMsg/OutItem hold 199 bytes of text plus NUL
  static constexpr size_t kMaxLongName = 24;   // bytes
  static constexpr size_t kMaxShortName = 4;   // bytes
  static constexpr int8_t kMinPower = 2, kMaxPower = 22;

  Region region;
  int8_t txPower;  // user setting; the radio uses effectiveTxPower(region, txPower)
  bool customGreeting;  // false: greeting is composed from the current long name
  char greeting[kMaxGreeting + 1];
  char longName[kMaxLongName + 1];
  char shortName[kMaxShortName + 1];
};

/// Size of the persisted settings blob.
constexpr size_t kSettingsBlobLen = 4 + 201 + 25 + 5;

/// Fills s with defaults: ANZ, 22 dBm, composed greeting, "4stro-031" / "4S31".
void settingsDefaults(Settings& s);
/// Serialises s into out[kSettingsBlobLen].
void settingsEncode(const Settings& s, uint8_t out[kSettingsBlobLen]);
/// Parses and validates a blob; returns false (s untouched) if anything is invalid.
bool settingsDecode(const uint8_t* in, size_t len, Settings& s);
/// Outcome of settingsLoad.
enum class SettingsLoad : uint8_t {
  Ok,         // a valid blob was read
  Recovered,  // blob invalid or absent, but a valid stored region byte was kept (other fields are defaults)
  FirstBoot   // no usable region stored: defaults (ANZ preselected); the user must confirm a region before any TX
};
/// Loads settings from Kv (key "settings"). On a decode failure keeps the blob's region byte if it is valid, else the
/// legacy 1-byte "region" key if valid; otherwise defaults with FirstBoot.
SettingsLoad settingsLoad(Kv& kv, Settings& s);
/// True if TX must stay off until the user confirms a region (first boot).
inline bool settingsNeedRegionPrompt(SettingsLoad r) { return r == SettingsLoad::FirstBoot; }
/// Persists settings; returns the Kv result.
bool settingsSave(Kv& kv, const Settings& s);

/// Region <-> explicit persisted id byte (the Region value: 0 = ANZ, 1 = BR_902, 2 = US, ...); unknown ids return false.
uint8_t regionToByte(Region r);
bool regionFromByte(uint8_t b, Region& out);

/// True if p is within 2..22 dBm.
bool validTxPower(int p);
/// Long name: 1..24 bytes of clean UTF-8 (no control characters).
bool validLongName(const char* s);
/// Short name: 1..4 bytes of clean UTF-8.
bool validShortName(const char* s);
/// Greeting: 1..199 bytes of clean UTF-8.
bool validGreeting(const char* s);

/// Writes the greeting to send: the custom text verbatim, or "Saudações de <longName>! 👋 (Cardputer-Adv)".
void composeGreeting(const Settings& s, char* out, size_t cap);
