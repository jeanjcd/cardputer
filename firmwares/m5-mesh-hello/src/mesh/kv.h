// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>

/// Abstract persistent key-value blob store (ESP32 NVS in firmware, a map in tests). Keys are at most 15 chars.
struct Kv {
  /// Reads the blob under `key` into buf; true only if a blob of exactly `len` bytes is stored (never writes past len).
  virtual bool get(const char* key, void* buf, size_t len) = 0;
  /// Stores `len` bytes under `key`, replacing any previous blob; returns true on success.
  virtual bool put(const char* key, const void* buf, size_t len) = 0;
  virtual ~Kv() = default;
};
