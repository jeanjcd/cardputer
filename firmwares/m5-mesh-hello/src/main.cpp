#ifndef PIO_UNIT_TESTING
// Temporary diagnostic firmware (Task 6): radio bring-up, identity and frame decoding on screen.
// Task 7 replaces this file with the real application UI.
#include <Arduino.h>
#include <M5Cardputer.h>
#include <esp_random.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hw/identity.h"
#include "hw/nvs_kv.h"
#include "hw/radio.h"
#include "mesh/header.h"
#include "mesh/packet.h"
#include "mesh/region.h"

namespace {
constexpr const char* kChannelName = "LongFast";
constexpr float kBwKHz = 250.0f;
constexpr int8_t kTxPowerDbm = 22;
constexpr uint32_t kAutoNodeInfoMs = 5000;
constexpr uint32_t kBroadcast = 0xFFFFFFFFu;
constexpr int kLineH = 12;
constexpr int kCols = 40;  // 240 px / 6 px per glyph (Font0, size 1)

NvsKv kv;
Radio radio;
Identity me;
IdentityStatus idStatus = IdentityStatus::Loaded;
Region region = Region::ANZ;
DedupCache dedup;

// Diagnostic state shown on screen.
uint32_t rxFrames = 0, rxDups = 0;
int16_t lastRssi = 0;
float lastSnr = 0;
bool haveFrame = false;
uint8_t lastChannel = 0;
uint32_t lastFrom = 0;
char lastKind[24] = "-";
char lastName[40] = "-";
char txStatus[24] = "-";
uint32_t txCount = 0;
bool autoSent = false;
bool dirty = true;
uint32_t shownCrcErrors = 0;

// No node database in the diagnostic build: PKI direct messages cannot be decrypted.
const uint8_t* noPeerKey(uint32_t) { return nullptr; }

float currentFreq() { return regionFreqMHz(region, kChannelName, kBwKHz); }
const char* regionName(Region r) { return r == Region::ANZ ? "ANZ" : "BR_902"; }

// Copies a UTF-8 string for display, replacing non-printable/non-ASCII bytes with '?' (display only).
void displayCopy(char* dst, size_t cap, const char* src) {
  size_t i = 0;
  for (; src[i] && i + 1 < cap; i++) {
    unsigned char c = (unsigned char)src[i];
    dst[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
  }
  dst[i] = '\0';
}

void drawLine(int row, uint16_t color, const char* fmt, ...) {
  char text[kCols + 1];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(text, sizeof text, fmt, ap);
  va_end(ap);
  char padded[kCols + 1];
  snprintf(padded, sizeof padded, "%-40s", text);  // overwrite the whole row: no flicker, no clear
  auto& d = M5Cardputer.Display;
  d.setTextColor(color, TFT_BLACK);
  d.setCursor(0, row * kLineH + 2);
  d.print(padded);
}

void redraw() {
  dirty = false;
  const char* idText = idStatus == IdentityStatus::Loaded    ? "key loaded"
                       : idStatus == IdentityStatus::Created ? "key new"
                                                             : "KEY NOT SAVED";
  drawLine(0, TFT_CYAN, "M5-Mesh-Hello diag");
  drawLine(1, TFT_WHITE, "Region %s  %.3f MHz", regionName(region), currentFreq());
  drawLine(2, TFT_WHITE, "Me !%08lx %s  %s", (unsigned long)me.nodeNum, me.shortName, idText);
  if (!radio.capFound())
    drawLine(3, TFT_RED, "Cap LoRa-1262 not found");
  else if (!radio.ready())
    drawLine(3, TFT_RED, "Cap OK  radio ERR %d", radio.lastError());
  else
    drawLine(3, TFT_GREEN, "Cap OK  radio OK");
  drawLine(4, TFT_WHITE, "RX %lu  CRC err %lu  dup %lu", (unsigned long)rxFrames,
           (unsigned long)radio.crcErrors(), (unsigned long)rxDups);
  if (haveFrame) {
    drawLine(5, TFT_WHITE, "RSSI %d dBm  SNR %.1f dB", lastRssi, lastSnr);
    drawLine(6, TFT_WHITE, "ch 0x%02X from !%08lx", lastChannel, (unsigned long)lastFrom);
  } else {
    drawLine(5, TFT_DARKGREY, "RSSI -  SNR -");
    drawLine(6, TFT_DARKGREY, "ch -  from -");
  }
  drawLine(7, TFT_WHITE, "kind %s", lastKind);
  drawLine(8, TFT_YELLOW, "name %s", lastName);
  drawLine(9, TFT_WHITE, "TX %s (%lu sent)", txStatus, (unsigned long)txCount);
  drawLine(10, TFT_DARKGREY, "[r] region  [n] send NodeInfo");
}

void sendNodeInfo() {
  uint8_t frame[Radio::kMaxFrame + 1];
  size_t n = buildNodeInfo(me, nextPacketId(esp_random()), kBroadcast, true, frame);
  bool ok = n > 0 && radio.send(frame, n);
  if (ok) txCount++;
  snprintf(txStatus, sizeof txStatus, "%s", ok ? "ok" : "fail");
  dirty = true;
}

void toggleRegion() {
  region = (region == Region::ANZ) ? Region::BR_902 : Region::ANZ;
  uint8_t v = (uint8_t)region;
  kv.put("region", &v, 1);
  radio.setFreq(currentFreq());
  dirty = true;
}

void handleFrame(const uint8_t* buf, size_t len, int16_t rssi, float snr) {
  rxFrames++;
  haveFrame = true;
  lastRssi = rssi;
  lastSnr = snr;
  dirty = true;
  PacketHeader h;
  if (!headerParse(buf, len, h)) {
    snprintf(lastKind, sizeof lastKind, "short (%u B)", (unsigned)len);
    return;
  }
  lastChannel = h.channel;
  lastFrom = h.from;
  if (dedup.seen(h.from, h.id)) {
    rxDups++;
    snprintf(lastKind, sizeof lastKind, "duplicate");
    return;
  }
  static Inbound in;  // large; keep it off the loop task stack
  bool ok = parseFrame(me, buf, len, noPeerKey, in);
  if (ok && in.kind == InKind::NodeInfo) {
    snprintf(lastKind, sizeof lastKind, "NodeInfo");
    displayCopy(lastName, sizeof lastName, in.user.longName);
  } else if (ok && in.kind == InKind::Text) {
    snprintf(lastKind, sizeof lastKind, "Text (%u B)", (unsigned)in.data.payloadLen);
  } else if (ok && in.kind == InKind::Ack) {
    snprintf(lastKind, sizeof lastKind, "Ack");
  } else if (in.kind == InKind::Other) {
    snprintf(lastKind, sizeof lastKind, "port %lu", (unsigned long)in.data.portnum);
  } else {
    snprintf(lastKind, sizeof lastKind, "not decoded");
  }
}

void handleKeys() {
  auto& kb = M5Cardputer.Keyboard;
  if (!kb.isChange() || !kb.isPressed()) return;
  auto ks = kb.keysState();
  for (char c : ks.word) {
    if (c == 'r' || c == 'R') toggleRegion();
    else if (c == 'n' || c == 'N') sendNodeInfo();
  }
}
}  // namespace

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);  // brings up the internal I2C used by the Cap's IO expander
  auto& d = M5Cardputer.Display;  // 240x135 landscape at rotation 0 (never setRotation(1))
  d.fillScreen(TFT_BLACK);
  d.setFont(&fonts::Font0);
  d.setTextSize(1);

  kv.begin();
  idStatus = loadOrCreateIdentity(kv, me);
  uint8_t v = 0;
  if (kv.get("region", &v, 1) && v <= (uint8_t)Region::BR_902) region = (Region)v;

  radio.begin(currentFreq(), kTxPowerDbm);
  redraw();
}

void loop() {
  M5Cardputer.update();
  handleKeys();

  uint8_t buf[Radio::kMaxFrame + 1];
  size_t len = sizeof buf;
  int16_t rssi = 0;
  float snr = 0;
  if (radio.poll(buf, len, rssi, snr)) handleFrame(buf, len, rssi, snr);

  if (!autoSent && millis() >= kAutoNodeInfoMs) {
    autoSent = true;
    sendNodeInfo();
  }
  if (radio.crcErrors() != shownCrcErrors) {
    shownCrcErrors = radio.crcErrors();
    dirty = true;
  }
  if (dirty) redraw();
  delay(1);
}
#endif
