// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PIO_UNIT_TESTING
// M5-Mesh-Hello: minimal Meshtastic node for the Cardputer-Adv + Cap LoRa-1262.
#include <Arduino.h>
#include <M5Cardputer.h>

#include "app/app.h"
#include "app/ui.h"

namespace {
App app;
Ui ui(app);
}  // namespace

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);  // brings up the internal I2C used by the keyboard and the Cap's IO expander
  app.begin();                   // identity (hardware RNG) first; the battery ADC is not used in v1
  ui.begin();
}

void loop() {
  M5Cardputer.update();
  ui.handleKeys(millis());
  app.tick(millis());
  ui.render(millis());
  delay(1);
}
#endif
