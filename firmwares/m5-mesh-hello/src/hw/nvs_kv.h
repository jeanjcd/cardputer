// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <Preferences.h>

#include "../mesh/kv.h"

/// Kv backed by ESP32 NVS through Arduino Preferences (namespace "meshhello"; keys at most 15 chars).
class NvsKv : public Kv {
 public:
  /// Opens the NVS namespace read-write; returns false if NVS is unavailable (get/put then fail).
  bool begin();
  /// Reads the blob under `key`; true only if a blob of exactly `len` bytes is stored.
  bool get(const char* key, void* buf, size_t len) override;
  /// Stores `len` bytes under `key`; true only if every byte was written.
  bool put(const char* key, const void* buf, size_t len) override;

 private:
  Preferences prefs_;
  bool open_ = false;
};
