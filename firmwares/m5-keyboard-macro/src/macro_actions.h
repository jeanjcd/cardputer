#pragma once
#include <stdint.h>
#include <vector>

constexpr uint32_t kIntervalMs = 60000;
constexpr uint16_t kMoveGapMs = 5;
constexpr uint16_t kDwellMs = 300;
constexpr uint16_t kTapDownMs = 30;
constexpr uint16_t kTapUpMs = 30;
constexpr int kShiftTaps = 10;
constexpr int kPoints = 3;
constexpr uint8_t kLeftShift = 0x02;
constexpr int kHomeReports = 40;
constexpr int kMinX = 100, kMaxX = 1820;
constexpr int kMinY = 100, kMaxY = 980;

enum class ActionType { Move, KeyDown, ReleaseAll, Wait };

struct Action {
  ActionType type;
  int8_t dx;
  int8_t dy;
  uint8_t modifier;
  uint16_t waitMs;
};

using RandomFn = uint32_t (*)(uint32_t lo, uint32_t hiInclusive);

// Slam the cursor to (0,0), then walk to (x,y) in <=127 steps, then dwell.
void appendMoveTo(std::vector<Action>& out, int x, int y);

// One full macro cycle: kPoints random points, then kShiftTaps Left Shift taps.
std::vector<Action> buildCycle(RandomFn randomFn);
