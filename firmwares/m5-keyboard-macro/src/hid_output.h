#pragma once
#include <stdint.h>

// Abstract HID sink: implemented by BleHid on the device and by fakes in tests.
struct HidOutput {
  virtual void mouseMove(int8_t dx, int8_t dy) = 0;
  virtual void keyDown(uint8_t modifier) = 0;
  virtual void releaseAll() = 0;
  virtual ~HidOutput() = default;
};
