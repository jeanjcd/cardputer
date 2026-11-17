#include "macro_actions.h"

static Action move(int8_t dx, int8_t dy) { return {ActionType::Move, dx, dy, 0, 0}; }
static Action wait(uint16_t ms) { return {ActionType::Wait, 0, 0, 0, ms}; }

static int clampStep(int remaining) {
  if (remaining > 127) return 127;
  if (remaining < -127) return -127;
  return remaining;
}

void appendMoveTo(std::vector<Action>& out, int x, int y) {
  for (int i = 0; i < kHomeReports; i++) {
    out.push_back(move(-127, -127));
    out.push_back(wait(kMoveGapMs));
  }
  int rx = x, ry = y;
  while (rx != 0 || ry != 0) {
    int sx = clampStep(rx), sy = clampStep(ry);
    out.push_back(move((int8_t)sx, (int8_t)sy));
    out.push_back(wait(kMoveGapMs));
    rx -= sx;
    ry -= sy;
  }
  out.push_back(wait(kDwellMs));
}

std::vector<Action> buildCycle(RandomFn randomFn) {
  std::vector<Action> out;
  for (int p = 0; p < kPoints; p++) {
    int x = (int)randomFn(kMinX, kMaxX);
    int y = (int)randomFn(kMinY, kMaxY);
    appendMoveTo(out, x, y);
  }
  for (int t = 0; t < kShiftTaps; t++) {
    out.push_back({ActionType::KeyDown, 0, 0, kLeftShift, 0});
    out.push_back(wait(kTapDownMs));
    out.push_back({ActionType::ReleaseAll, 0, 0, 0, 0});
    out.push_back(wait(kTapUpMs));
  }
  return out;
}
