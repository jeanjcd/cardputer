#include "ui.h"

#include <M5Cardputer.h>

static M5Canvas canvas(&M5Cardputer.Display);

void uiBegin() {
  // Current M5GFX gives the Cardputer/ADV panel (135x240) a built-in rotation_offset of 1,
  // so rotation 0 is already landscape; setRotation(1) (as older examples do) makes it portrait.
  canvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
}

void uiDraw(bool connected, bool enabled, uint32_t msUntilNext, uint32_t cycles) {
  static bool first = true;
  static bool lastConn, lastEn;
  static uint32_t lastSec, lastCycles;
  uint32_t sec = (msUntilNext + 999) / 1000;
  if (!first && connected == lastConn && enabled == lastEn && sec == lastSec && cycles == lastCycles) return;
  first = false;
  lastConn = connected; lastEn = enabled; lastSec = sec; lastCycles = cycles;

  canvas.fillSprite(BLACK);
  canvas.setTextSize(2);

  canvas.setCursor(8, 8);
  canvas.setTextColor(connected ? GREEN : ORANGE);
  canvas.print(connected ? "BT: conectado" : "BT: aguardando");

  canvas.setCursor(8, 36);
  canvas.setTextColor(enabled ? CYAN : LIGHTGREY);
  canvas.print(enabled ? "Macro: ON" : "Macro: OFF");

  canvas.setTextColor(WHITE);
  if (enabled) {
    canvas.setCursor(8, 64);
    if (!connected) canvas.print("Pausada");
    else if (sec == 0) canvas.print("Executando...");
    else canvas.printf("Proximo em %lus", (unsigned long)sec);
  }

  canvas.setCursor(8, 92);
  canvas.printf("Ciclos: %lu", (unsigned long)cycles);

  canvas.setTextSize(1);
  canvas.setTextColor(LIGHTGREY);
  canvas.setCursor(8, canvas.height() - 14);
  canvas.print("[Enter] liga/desliga   M5-Keyboard");

  canvas.pushSprite(0, 0);
}
