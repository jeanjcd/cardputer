#ifndef PIO_UNIT_TESTING
#include <Arduino.h>
#include <M5Cardputer.h>
#include <esp_random.h>

#include "ble_hid.h"
#include "macro_runner.h"
#include "ui.h"

static uint32_t hwRandom(uint32_t lo, uint32_t hi) { return lo + esp_random() % (hi - lo + 1); }

static BleHid ble;
static MacroRunner runner(ble, hwRandom);

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  uiBegin();
  ble.begin("M5-Keyboard");
}

void loop() {
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed() &&
      M5Cardputer.Keyboard.keysState().enter) {
    runner.setEnabled(!runner.enabled(), millis());
  }

  uint32_t now = millis();
  bool connected = ble.isConnected();
  runner.update(now, connected);
  uiDraw(connected, runner.enabled(), runner.msUntilNextCycle(now), runner.cyclesDone());
  delay(1);
}
#endif
