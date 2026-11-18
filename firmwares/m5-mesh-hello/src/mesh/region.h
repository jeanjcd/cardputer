#pragma once
#include <stddef.h>
#include <stdint.h>

/// Supported LoRa regions.
enum class Region { ANZ, BR_902 };

/// Default LongFast channel key (AES-128).
extern const uint8_t kDefaultKey[16];

/// DJB2 hash of a C string (Meshtastic channel-to-slot hash).
uint32_t djb2(const char* s);

/// Center frequency in MHz for a channel name, region and bandwidth in kHz.
float regionFreqMHz(Region region, const char* channelName, float bwKHz);

/// One-byte channel hash: XOR of name bytes XOR key bytes.
uint8_t channelHash(const char* name, const uint8_t* key, size_t keyLen);
