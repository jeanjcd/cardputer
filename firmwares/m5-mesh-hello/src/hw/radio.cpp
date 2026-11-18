#include "radio.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <RadioLib.h>
#include <SPI.h>

#include "utility/PI4IOE5V6408_Class.hpp"

namespace {
// Cap LoRa-1262 wiring on the Cardputer-Adv.
constexpr int kPinNss = 5, kPinDio1 = 4, kPinRst = 3, kPinBusy = 6;
constexpr int kPinSck = 40, kPinMiso = 39, kPinMosi = 14;
constexpr int kPinSdCs = 12;  // microSD shares the SPI bus; keep it deselected
constexpr uint8_t kIoeAddr = 0x43;
constexpr uint8_t kIoeAntennaPin = 0;

// Meshtastic LongFast.
constexpr float kBwKHz = 250.0f;
constexpr uint8_t kSf = 11;
constexpr uint8_t kCr = 5;  // 4/5
constexpr uint8_t kSyncWord = 0x2B;
constexpr uint16_t kPreamble = 16;
constexpr float kTcxoV = 3.0f;
constexpr float kCurrentLimitMa = 140.0f;
constexpr int8_t kMinPower = 2, kMaxPower = 22;

SPIClass loraSpi(FSPI);  // the display uses SPI3; the LoRa/SD bus gets SPI2
Module* gModule = nullptr;
SX1262* gLora = nullptr;
volatile bool gIrqFlag = false;

void IRAM_ATTR onDio1() { gIrqFlag = true; }

int8_t clampPower(int8_t p) { return p < kMinPower ? kMinPower : (p > kMaxPower ? kMaxPower : p); }
}  // namespace

bool Radio::begin(float freqMHz, int8_t powerDbm) {
  if (ready_) return true;

  pinMode(kPinSdCs, OUTPUT);
  digitalWrite(kPinSdCs, HIGH);

  // Antenna RF switch on the internal I2C IO expander (shared with keyboard and IMU).
  static m5::PI4IOE5V6408_Class ioe(kIoeAddr, 400000, &m5::In_I2C);
  capFound_ = ioe.begin();
  if (capFound_) {
    ioe.setDirection(kIoeAntennaPin, true);
    ioe.setHighImpedance(kIoeAntennaPin, false);
    ioe.digitalWrite(kIoeAntennaPin, true);
  }

  loraSpi.begin(kPinSck, kPinMiso, kPinMosi, kPinNss);
  if (!gModule) gModule = new Module(kPinNss, kPinDio1, kPinRst, kPinBusy, loraSpi);
  if (!gLora) gLora = new SX1262(gModule);

  int16_t st = gLora->begin(freqMHz, kBwKHz, kSf, kCr, kSyncWord, clampPower(powerDbm), kPreamble, kTcxoV, true);
  if (st == RADIOLIB_ERR_NONE) st = gLora->setCurrentLimit(kCurrentLimitMa);
  if (st == RADIOLIB_ERR_NONE) st = gLora->setCRC(2);  // LoRa payload CRC on
  if (st == RADIOLIB_ERR_NONE) st = gLora->explicitHeader();
  if (st == RADIOLIB_ERR_NONE) st = gLora->setRxBoostedGainMode(true);  // as Meshtastic: better sensitivity
  if (st != RADIOLIB_ERR_NONE) {
    lastError_ = st;
    return false;
  }
  gIrqFlag = false;
  gLora->setPacketReceivedAction(onDio1);
  st = gLora->startReceive();
  lastError_ = st;
  ready_ = (st == RADIOLIB_ERR_NONE);
  return ready_;
}

bool Radio::poll(uint8_t* buf, size_t& len, int16_t& rssi, float& snr) {
  size_t cap = len;
  len = 0;
  if (!ready_ || !gIrqFlag) return false;
  gIrqFlag = false;

  bool ok = false;
  size_t n = gLora->getPacketLength();
  if (buf && n > 0 && n <= kMaxFrame && n <= cap) {
    int16_t st = gLora->readData(buf, n);
    if (st == RADIOLIB_ERR_NONE) {
      len = n;
      rssi = (int16_t)gLora->getRSSI();
      snr = gLora->getSNR();
      ok = true;
    } else if (st == RADIOLIB_ERR_CRC_MISMATCH) {
      crcErrors_++;
    } else {
      lastError_ = st;
    }
  }
  // Restarting RX also clears the IRQ status (covers rejected lengths).
  int16_t st = gLora->startReceive();
  if (st != RADIOLIB_ERR_NONE) lastError_ = st;
  return ok;
}

bool Radio::send(const uint8_t* buf, size_t len) {
  if (!ready_ || !buf || len == 0 || len > kMaxFrame) return false;
  // transmit() polls DIO1 itself; detach our RX callback while it runs.
  gLora->clearPacketReceivedAction();
  int16_t st = gLora->transmit(buf, len);
  gIrqFlag = false;
  gLora->setPacketReceivedAction(onDio1);
  if (st != RADIOLIB_ERR_NONE) lastError_ = st;
  int16_t rx = gLora->startReceive();
  if (rx != RADIOLIB_ERR_NONE) lastError_ = rx;
  return st == RADIOLIB_ERR_NONE;
}

bool Radio::setFreq(float freqMHz) {
  if (!ready_) return false;
  gLora->standby();
  int16_t st = gLora->setFrequency(freqMHz);
  gIrqFlag = false;
  int16_t rx = gLora->startReceive();
  if (st != RADIOLIB_ERR_NONE) lastError_ = st;
  else if (rx != RADIOLIB_ERR_NONE) lastError_ = rx;
  return st == RADIOLIB_ERR_NONE && rx == RADIOLIB_ERR_NONE;
}

bool Radio::setPower(int8_t dbm) {
  if (!ready_) return false;
  gLora->standby();
  int16_t st = gLora->setOutputPower(clampPower(dbm));  // preserves the OCP (current limit) register
  gIrqFlag = false;
  int16_t rx = gLora->startReceive();
  if (st != RADIOLIB_ERR_NONE) lastError_ = st;
  else if (rx != RADIOLIB_ERR_NONE) lastError_ = rx;
  return st == RADIOLIB_ERR_NONE && rx == RADIOLIB_ERR_NONE;
}
