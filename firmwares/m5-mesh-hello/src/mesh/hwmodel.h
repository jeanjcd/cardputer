#pragma once
#include <stddef.h>
#include <stdint.h>

/// Writes a short name for a Meshtastic HardwareModel value (e.g. 112 -> "CARDPUTER_ADV"); unknown -> "HW#<n>".
void hwModelName(uint32_t model, char* out, size_t cap);

/// Chip family for known models ("ESP32-S3", "ESP32", "nRF52", "RP2040"); "" if unknown.
const char* hwChipFamily(uint32_t model);
