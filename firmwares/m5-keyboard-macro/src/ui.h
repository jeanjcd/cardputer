#pragma once
#include <stdint.h>

void uiBegin();
// Redraws only when something visible changed.
void uiDraw(bool connected, bool enabled, uint32_t msUntilNext, uint32_t cycles);
