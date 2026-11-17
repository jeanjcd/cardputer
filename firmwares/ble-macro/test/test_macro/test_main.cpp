#include <Arduino.h>
#include <unity.h>
#include <vector>
#include <cstdlib>

#include "macro_actions.h"
#include "macro_runner.h"
#include "hid_output.h"

void setUp() {}
void tearDown() {}

// ---------- Task 1: action builder ----------

static std::vector<std::pair<uint32_t, uint32_t>> g_randomCalls;

static uint32_t fakeRandomLo(uint32_t lo, uint32_t hi) {
  g_randomCalls.push_back({lo, hi});
  return lo;
}

static std::vector<Action> movesOf(const std::vector<Action>& v) {
  std::vector<Action> out;
  for (const auto& a : v)
    if (a.type == ActionType::Move) out.push_back(a);
  return out;
}

void test_move_chunks_within_127() {
  std::vector<Action> v;
  appendMoveTo(v, 1820, 980);
  for (const auto& a : movesOf(v)) {
    TEST_ASSERT_TRUE(abs(a.dx) <= 127);
    TEST_ASSERT_TRUE(abs(a.dy) <= 127);
  }
}

void test_home_first() {
  std::vector<Action> v;
  appendMoveTo(v, 500, 500);
  auto m = movesOf(v);
  TEST_ASSERT_TRUE(m.size() > (size_t)kHomeReports);
  for (int i = 0; i < kHomeReports; i++) {
    TEST_ASSERT_EQUAL_INT(-127, m[i].dx);
    TEST_ASSERT_EQUAL_INT(-127, m[i].dy);
  }
}

void test_move_reaches_target() {
  std::vector<Action> v;
  appendMoveTo(v, 1820, 980);
  auto m = movesOf(v);
  long sx = 0, sy = 0;
  for (size_t i = kHomeReports; i < m.size(); i++) {
    sx += m[i].dx;
    sy += m[i].dy;
  }
  TEST_ASSERT_EQUAL_INT(1820, sx);
  TEST_ASSERT_EQUAL_INT(980, sy);
}

void test_cycle_shape() {
  g_randomCalls.clear();
  auto v = buildCycle(fakeRandomLo);
  int keyDowns = 0, releases = 0, dwells = 0;
  for (size_t i = 0; i < v.size(); i++) {
    const auto& a = v[i];
    if (a.type == ActionType::KeyDown) {
      keyDowns++;
      TEST_ASSERT_EQUAL_UINT8(0x02, a.modifier);
      TEST_ASSERT_TRUE(i + 3 < v.size());
      TEST_ASSERT_TRUE(v[i + 1].type == ActionType::Wait);
      TEST_ASSERT_EQUAL_UINT16(30, v[i + 1].waitMs);
      TEST_ASSERT_TRUE(v[i + 2].type == ActionType::ReleaseAll);
      TEST_ASSERT_TRUE(v[i + 3].type == ActionType::Wait);
      TEST_ASSERT_EQUAL_UINT16(30, v[i + 3].waitMs);
    }
    if (a.type == ActionType::ReleaseAll) releases++;
    if (a.type == ActionType::Wait && a.waitMs == 300) dwells++;
  }
  TEST_ASSERT_EQUAL_INT(10, keyDowns);
  TEST_ASSERT_EQUAL_INT(10, releases);
  TEST_ASSERT_EQUAL_INT(3, dwells);
}

void test_random_bounds() {
  g_randomCalls.clear();
  buildCycle(fakeRandomLo);
  int xs = 0, ys = 0;
  for (auto& c : g_randomCalls) {
    if (c.first == 100 && c.second == 1820) xs++;
    else if (c.first == 100 && c.second == 980) ys++;
    else TEST_FAIL_MESSAGE("unexpected random range");
  }
  TEST_ASSERT_EQUAL_INT(3, xs);
  TEST_ASSERT_EQUAL_INT(3, ys);
}

// ---------- Task 2: MacroRunner ----------

enum class Call { Move, KeyDown, Release };

struct FakeHid : HidOutput {
  std::vector<Call> calls;
  void mouseMove(int8_t, int8_t) override { calls.push_back(Call::Move); }
  void keyDown(uint8_t) override { calls.push_back(Call::KeyDown); }
  void releaseAll() override { calls.push_back(Call::Release); }
  bool has(Call c) const {
    for (auto x : calls) if (x == c) return true;
    return false;
  }
};

static uint32_t randomMid(uint32_t lo, uint32_t hi) { return (lo + hi) / 2; }

// Advance in 1 ms steps until pred() or limit; returns time reached.
template <typename P>
static uint32_t runUntil(MacroRunner& r, uint32_t t, uint32_t limitMs, bool connected, P pred) {
  for (uint32_t i = 0; i < limitMs; i++, t++) {
    r.update(t, connected);
    if (pred()) return t;
  }
  return t;
}

void test_first_cycle_immediate() {
  FakeHid hid;
  MacroRunner r(hid, randomMid);
  r.setEnabled(true, 0);
  r.update(0, true);
  TEST_ASSERT_TRUE(hid.has(Call::Move));
}

void test_next_cycle_after_60s() {
  FakeHid hid;
  MacroRunner r(hid, randomMid);
  r.setEnabled(true, 0);
  uint32_t t = runUntil(r, 0, 20000, true, [&] { return r.cyclesDone() == 1; });
  TEST_ASSERT_EQUAL_UINT32(1, r.cyclesDone());
  hid.calls.clear();
  for (t = t + 1; t < 60000; t++) r.update(t, true);
  TEST_ASSERT_FALSE(hid.has(Call::Move));
  r.update(60000, true);
  TEST_ASSERT_TRUE(hid.has(Call::Move));
}

void test_stop_mid_cycle_releases_keys() {
  FakeHid hid;
  MacroRunner r(hid, randomMid);
  r.setEnabled(true, 0);
  uint32_t t = runUntil(r, 0, 20000, true, [&] { return hid.has(Call::KeyDown); });
  TEST_ASSERT_TRUE(hid.has(Call::KeyDown));
  r.setEnabled(false, t);
  TEST_ASSERT_TRUE(hid.calls.back() == Call::Release);
  size_t n = hid.calls.size();
  for (uint32_t i = 1; i < 70000; i += 7) r.update(t + i, true);
  TEST_ASSERT_EQUAL(n, hid.calls.size());
}

void test_no_output_while_disconnected() {
  FakeHid hid;
  MacroRunner r(hid, randomMid);
  r.setEnabled(true, 0);
  for (uint32_t t = 0; t < 5000; t++) r.update(t, false);
  TEST_ASSERT_EQUAL(0, hid.calls.size());
  r.update(5000, true);
  TEST_ASSERT_TRUE(hid.has(Call::Move));
}

void test_reconnect_restarts_cycle() {
  FakeHid hid;
  MacroRunner r(hid, randomMid);
  r.setEnabled(true, 0);
  uint32_t t = runUntil(r, 0, 1000, true, [] { return false; });  // mid-cycle
  TEST_ASSERT_EQUAL_UINT32(0, r.cyclesDone());
  hid.calls.clear();
  r.update(t, false);
  TEST_ASSERT_TRUE(hid.has(Call::Release));
  hid.calls.clear();
  r.update(t + 10, true);
  TEST_ASSERT_TRUE(hid.has(Call::Move));
}

void test_timing_survives_rollover() {
  FakeHid hid;
  MacroRunner r(hid, randomMid);
  uint32_t start = 0xFFFFFF00u;
  r.setEnabled(true, start);
  uint32_t t = runUntil(r, start, 20000, true, [&] { return r.cyclesDone() == 1; });
  TEST_ASSERT_EQUAL_UINT32(1, r.cyclesDone());
  hid.calls.clear();
  for (t = t + 1; t != start + 60000u; t++) r.update(t, true);
  TEST_ASSERT_FALSE(hid.has(Call::Move));
  r.update(start + 60000u, true);
  TEST_ASSERT_TRUE(hid.has(Call::Move));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_move_chunks_within_127);
  RUN_TEST(test_home_first);
  RUN_TEST(test_move_reaches_target);
  RUN_TEST(test_cycle_shape);
  RUN_TEST(test_random_bounds);
  RUN_TEST(test_first_cycle_immediate);
  RUN_TEST(test_next_cycle_after_60s);
  RUN_TEST(test_stop_mid_cycle_releases_keys);
  RUN_TEST(test_no_output_while_disconnected);
  RUN_TEST(test_reconnect_restarts_cycle);
  RUN_TEST(test_timing_survives_rollover);
  UNITY_END();
}

void loop() {}
