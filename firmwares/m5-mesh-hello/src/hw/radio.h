// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>

/// SX1262 on the M5 Cap LoRa-1262 (Cardputer-Adv), Meshtastic LongFast modem settings, interrupt-driven RX.
class Radio {
 public:
  static constexpr size_t kMaxFrame = 255;

  /// Enables the antenna switch (PI4IOE5V6408 P0), initialises the SX1262 and starts RX. Call after M5Cardputer.begin().
  bool begin(float freqMHz, int8_t powerDbm);
  /// Non-blocking: if a packet arrived (IRQ flag or DIO1 high), copies it into buf (capacity passed in `len`, length
  /// returned in `len`) with RSSI/SNR. Re-arms RX if nothing was received or sent for 10 minutes.
  bool poll(uint8_t* buf, size_t& len, int16_t& rssi, float& snr);
  /// Blocking transmit of 1..255 bytes, then back to RX; returns true if the radio reported TX done.
  bool send(const uint8_t* buf, size_t len);
  /// Retunes to freqMHz and resumes RX; returns false on a radio error.
  bool setFreq(float freqMHz);
  /// Sets TX power (clamped to 2..22 dBm) and resumes RX; returns false on a radio error.
  bool setPower(int8_t dbm);

  /// True if the radio initialised successfully.
  bool ready() const { return ready_; }
  /// True if the Cap's IO expander answered and the antenna switch output was configured and reads back HIGH.
  bool capFound() const { return capFound_; }
  /// Number of received packets dropped for a CRC/header error.
  uint32_t crcErrors() const { return crcErrors_; }
  /// Last RadioLib status code (0 = no error).
  int16_t lastError() const { return lastError_; }

 private:
  void markActivity();
  bool ready_ = false;
  uint32_t lastActivityMs_ = 0;
  bool capFound_ = false;
  uint32_t crcErrors_ = 0;
  int16_t lastError_ = 0;
};
