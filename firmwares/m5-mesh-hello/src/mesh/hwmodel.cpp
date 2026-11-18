// SPDX-License-Identifier: GPL-3.0-or-later
#include "hwmodel.h"
#include <stdio.h>

namespace {
struct Model { uint16_t id; const char* name; const char* chip; };

constexpr const char* kS3 = "ESP32-S3";
constexpr const char* kEsp32 = "ESP32";
constexpr const char* kNrf = "nRF52";
constexpr const char* kRp = "RP2040";

// Subset of meshtastic HardwareModel (mesh.proto); names shortened to fit the node list.
const Model kModels[] = {
    {1, "TLORA_V2", kEsp32},
    {3, "TLORA_V2_1_1P6", kEsp32},
    {4, "TBEAM", kEsp32},
    {5, "HELTEC_V2_0", kEsp32},
    {7, "T_ECHO", kNrf},
    {9, "RAK4631", kNrf},
    {10, "HELTEC_V2_1", kEsp32},
    {12, "TBEAM_S3", kS3},
    {13, "RAK11200", kEsp32},
    {15, "TLORA_V2_1_1P8", kEsp32},
    {16, "TLORA_T3_S3", kS3},
    {18, "NANO_G2_ULTRA", kNrf},
    {25, "STATION_G1", kEsp32},
    {26, "RAK11310", kRp},
    {30, "RP2040_LORA", kRp},
    {31, "STATION_G2", kS3},
    {39, "DIY_V1", kEsp32},
    {43, "HELTEC_V3", kS3},
    {44, "HELTEC_WSL_V3", kS3},
    {47, "RPI_PICO", kRp},
    {48, "HELTEC_TRACKER", kS3},
    {49, "HELTEC_PAPER", kS3},
    {50, "T_DECK", kS3},
    {51, "T_WATCH_S3", kS3},
    {66, "VISION_MASTER", kS3},
    {67, "VISION_MASTER", kS3},
    {68, "VISION_MASTER", kS3},
    {69, "HELTEC_T114", kNrf},
    {70, "SENSECAP_IND", kS3},
    {71, "TRACKER_T1000_E", kNrf},
    {81, "XIAO_S3", kS3},
    {84, "WISMESH_TAP", kNrf},
    {88, "XIAO_NRF52", kNrf},
    {89, "THINKNODE_M1", kNrf},
    {110, "HELTEC_V4", kS3},
    {112, "CARDPUTER_ADV", kS3},
    {255, "PRIVATE_HW", ""},
};

const Model* find(uint32_t model) {
  for (const Model& m : kModels)
    if (m.id == model) return &m;
  return nullptr;
}
}  // namespace

void hwModelName(uint32_t model, char* out, size_t cap) {
  if (!out || cap == 0) return;
  const Model* m = find(model);
  if (m) snprintf(out, cap, "%s", m->name);
  else snprintf(out, cap, "HW#%lu", (unsigned long)model);
}

const char* hwChipFamily(uint32_t model) {
  const Model* m = find(model);
  return m ? m->chip : "";
}
