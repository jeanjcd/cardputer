#pragma once
#include "hid_output.h"

class NimBLEHIDDevice;
class NimBLECharacteristic;

// BLE HID combo: report ID 1 = keyboard, report ID 2 = relative mouse.
class BleHid : public HidOutput {
 public:
  void begin(const char* name);
  bool isConnected() const;

  void mouseMove(int8_t dx, int8_t dy) override;
  void keyDown(uint8_t modifier) override;
  void releaseAll() override;

 private:
  NimBLEHIDDevice* hid_ = nullptr;
  NimBLECharacteristic* keyboard_ = nullptr;
  NimBLECharacteristic* mouse_ = nullptr;
};
