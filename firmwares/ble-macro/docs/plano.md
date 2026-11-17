# Cardputer BLE Macro Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Firmware for the M5Stack Cardputer that pairs over BLE as keyboard + mouse and, while toggled on, every 60 s moves the cursor to 3 random screen points and taps Left Shift 10 times.

**Architecture:** A pure, hardware-free macro engine (`Action` list + non-blocking `MacroRunner` that talks to an abstract `HidOutput`) drives a NimBLE HID combo device. `main.cpp` only wires the keyboard, display and BLE to the runner. The pure logic is tested with Unity **on the device** (`pio test`), because this machine has no host C++ compiler.

**Tech Stack:** PlatformIO, Arduino framework (espressif32), `m5stack/M5Cardputer`, `h2zero/NimBLE-Arduino@^1.4.3` (1.x API: `NimBLEHIDDevice::inputReport()`, `reportMap()`).

**Spec:** `docs/superpowers/specs/2026-11-17-ble-macro-design.md`

## Global Constraints

- BLE device name: `"M5-Keyboard"`.
- Toggle key: Enter on the Cardputer keyboard.
- Cycle interval: 60000 ms; first cycle runs immediately on toggle-on.
- Per cycle: 3 random points, then 10 Left Shift taps.
- Random points inside 1920×1080 with 100 px margin: x ∈ [100, 1820], y ∈ [100, 980].
- Mouse reports: each axis clamped to ±127; 5 ms between move reports; 300 ms dwell at each point.
- Shift tap: 30 ms down, 30 ms up. Left Shift modifier bit = `0x02`.
- No macro output while BLE is disconnected.
- Not a git repo: skip commit steps unless Jean asks to `git init`.
- Use unsigned subtraction (`now - start >= delay`) for every millis() comparison.

## Review Focus

1. **Toggle off or disconnect in the middle of the Shift taps** — Shift must not stay stuck pressed on the PC; runner calls `releaseAll()` on stop/disconnect. Test: `test_stop_mid_cycle_releases_keys` (Task 2).
2. **Disconnect mid-cycle, then reconnect** — the partial cycle is dropped and the next cycle starts immediately on reconnect, not 60 s later. Test: `test_reconnect_restarts_cycle` (Task 2).
3. **Toggle on while disconnected** — nothing is sent; macro starts once connected. Test: `test_no_output_while_disconnected` (Task 2).
4. **millis() rollover** after ~49 days — timing keeps working. Test: `test_timing_survives_rollover` (Task 2).
5. **Holding Enter** — toggles once per press, not every loop. Covered by using `M5Cardputer.Keyboard.isChange()` + `isPressed()` edge in Task 4 (manual check).

---

### Task 1: Project scaffold + macro action builder

**Files:**
- Create: `platformio.ini`
- Create: `src/hid_output.h`
- Create: `src/macro_actions.h`, `src/macro_actions.cpp`
- Test: `test/test_macro/test_main.cpp`

**Interfaces:**
- Produces:
  - `struct HidOutput { virtual void mouseMove(int8_t dx, int8_t dy)=0; virtual void keyDown(uint8_t modifier)=0; virtual void releaseAll()=0; virtual ~HidOutput()=default; };`
  - `enum class ActionType { Move, KeyDown, ReleaseAll, Wait };`
  - `struct Action { ActionType type; int8_t dx; int8_t dy; uint8_t modifier; uint16_t waitMs; };`
  - `constexpr` values in `macro_actions.h`: `kIntervalMs=60000`, `kMoveGapMs=5`, `kDwellMs=300`, `kTapDownMs=30`, `kTapUpMs=30`, `kShiftTaps=10`, `kPoints=3`, `kLeftShift=0x02`, `kHomeReports=40`, `kMinX=100, kMaxX=1820, kMinY=100, kMaxY=980`.
  - `void appendMoveTo(std::vector<Action>& out, int x, int y);` — `kHomeReports` moves of (-127,-127) each followed by Wait `kMoveGapMs`, then moves toward (x,y) split into chunks ≤127 per axis, each followed by Wait `kMoveGapMs`, then Wait `kDwellMs`.
  - `std::vector<Action> buildCycle(uint32_t (*randomFn)(uint32_t lo, uint32_t hiInclusive));` — `kPoints` × `appendMoveTo(random x, random y)`, then `kShiftTaps` × [KeyDown `kLeftShift`, Wait 30, ReleaseAll, Wait 30].

`platformio.ini`:
```ini
[env:cardputer]
platform = espressif32
board = m5stack-stamps3
framework = arduino
monitor_speed = 115200
build_flags = -DARDUINO_USB_CDC_ON_BOOT=1
lib_deps =
  m5stack/M5Cardputer
  h2zero/NimBLE-Arduino@^1.4.3
test_build_src = yes
```
(`test_build_src = yes` compiles `src/` into tests; guard `setup()/loop()` in `main.cpp` with `#ifndef PIO_UNIT_TESTING`.)

- [ ] **Step 1: Install PlatformIO**: `python -m pip install -U platformio`; verify `pio --version` prints a version.
- [ ] **Step 2: Write failing tests** in `test/test_macro/test_main.cpp` (Unity, `setup()` calls `UNITY_BEGIN()`/`RUN_TEST`/`UNITY_END()` after `delay(2000)`):
  - `test_move_chunks_within_127`: `appendMoveTo(v, 1820, 980)`; every Move has `abs(dx)<=127 && abs(dy)<=127`.
  - `test_move_reaches_target`: sum of dx/dy over Moves **after** the first `kHomeReports` Moves == (1820, 980).
  - `test_home_first`: first `kHomeReports` Moves are all (-127,-127).
  - `test_cycle_shape`: with a fake `randomFn` returning `lo`, `buildCycle` has exactly 10 KeyDown with modifier `0x02`, 10 ReleaseAll, and every KeyDown is followed by Wait 30 then ReleaseAll then Wait 30; exactly 3 Waits of 300.
  - `test_random_bounds`: `buildCycle` calls `randomFn` with (100,1820) for x and (100,980) for y, 3 times each.
- [ ] **Step 3: Run** `pio test -e cardputer` with Cardputer on USB — Expected: compile FAIL (symbols missing).
- [ ] **Step 4: Implement** `hid_output.h`, `macro_actions.h/.cpp` to the signatures above. Add a stub `src/main.cpp` whose `setup()/loop()` are inside `#ifndef PIO_UNIT_TESTING`.
- [ ] **Step 5: Run** `pio test -e cardputer` — Expected: all 5 tests PASS.

### Task 2: Non-blocking MacroRunner

**Files:**
- Create: `src/macro_runner.h`, `src/macro_runner.cpp`
- Modify: `test/test_macro/test_main.cpp`

**Interfaces:**
- Consumes: `HidOutput`, `Action`, `buildCycle`, constants (Task 1).
- Produces:
  ```cpp
  class MacroRunner {
   public:
    MacroRunner(HidOutput& hid, uint32_t (*randomFn)(uint32_t, uint32_t));
    void setEnabled(bool on, uint32_t nowMs);   // off => releaseAll() if mid-cycle, clear queue
    bool enabled() const;
    void update(uint32_t nowMs, bool connected); // call every loop
    uint32_t msUntilNextCycle(uint32_t nowMs) const; // 0 while a cycle is running
    uint32_t cyclesDone() const;
  };
  ```
  Behavior: when enabled and connected and no cycle queued and (first cycle pending or `now - lastCycleStart >= kIntervalMs`) → `buildCycle`, record start. Execute actions in order; a Wait blocks only until `now - waitStart >= waitMs`; several non-Wait actions may run in one `update`. On transition to disconnected mid-cycle → `releaseAll()`, drop queue, mark "first cycle pending" so the next cycle runs on reconnect. `cyclesDone` increments when a queue fully drains.
- [ ] **Step 1: Write failing tests** with `FakeHid` recording calls:
  - `test_first_cycle_immediate`: enable at t=0, `update(0,true)` → at least one mouseMove recorded.
  - `test_next_cycle_after_60s`: drive updates in 1 ms steps until `cyclesDone()==1`; no new Move until t = 60000 from the first cycle start; Move appears at 60000.
  - `test_stop_mid_cycle_releases_keys`: run until first keyDown recorded, `setEnabled(false, t)` → last call is `releaseAll`; further updates record nothing.
  - `test_no_output_while_disconnected`: enable, `update(t,false)` for 5000 ms → no calls; then `update(t,true)` → Move recorded.
  - `test_reconnect_restarts_cycle`: mid-cycle `update(t,false)` → `releaseAll` recorded; `update(t+10,true)` → new home Moves start immediately.
  - `test_timing_survives_rollover`: enable at `t=0xFFFFFF00`, complete a cycle across the wrap, next cycle starts 60000 ms after start.
- [ ] **Step 2: Run** `pio test -e cardputer` — Expected: FAIL (MacroRunner undefined).
- [ ] **Step 3: Implement** `MacroRunner` per the signature and behavior above.
- [ ] **Step 4: Run** `pio test -e cardputer` — Expected: all Task 1 + Task 2 tests PASS.

### Task 3: NimBLE HID combo device

**Files:**
- Create: `src/ble_hid.h`, `src/ble_hid.cpp`

**Interfaces:**
- Consumes: `HidOutput` (Task 1).
- Produces: `class BleHid : public HidOutput { public: void begin(const char* name); bool isConnected() const; void mouseMove(int8_t,int8_t) override; void keyDown(uint8_t modifier) override; void releaseAll() override; };`
  - Report map with two report IDs: **1 = keyboard** (8 bytes: modifier, reserved, 6 keys) and **2 = mouse** (buttons byte, X, Y, wheel; X/Y relative, logical −127..127).
  - `NimBLEDevice::init(name)`, security bonding enabled (`setSecurityAuth(true,false,false)`), appearance `HID_KEYBOARD`, HID info/PnP set, battery 100, advertise HID service UUID; restart advertising on disconnect.
  - `keyDown` sends keyboard report `{modifier,0,0,0,0,0,0,0}`; `releaseAll` sends zero keyboard report **and** zero mouse buttons.
- [ ] **Step 1: Implement** `BleHid`.
- [ ] **Step 2: Build** `pio run -e cardputer` — Expected: `SUCCESS`.
- [ ] **Step 3: Run tests** `pio test -e cardputer` — Expected: still all PASS (no regressions).

### Task 4: main.cpp wiring + Cardputer screen

**Files:**
- Modify: `src/main.cpp`
- Create: `src/ui.h`, `src/ui.cpp`

**Interfaces:**
- Consumes: `BleHid` (Task 3), `MacroRunner` (Task 2).
- Produces: `void uiDraw(bool connected, bool enabled, uint32_t msUntilNext, uint32_t cycles);` — lines: `BT: conectado` / `BT: aguardando conexao`; `Macro: ON` / `Macro: OFF`; `Proximo em Ns` (or `Executando...` when 0, hidden when OFF); `Ciclos: N`; footer `[Enter] liga/desliga`. Redraw at most every 250 ms, using a sprite or only on change to avoid flicker.
- `setup()`: `M5Cardputer.begin(cfg, true)`, rotation 1, `ble.begin("M5-Keyboard")`. `randomFn` wraps `esp_random()` → `lo + esp_random() % (hi - lo + 1)`.
- `loop()`: `M5Cardputer.update()`; if `Keyboard.isChange() && Keyboard.isPressed()` and `keysState().enter` → `runner.setEnabled(!runner.enabled(), millis())`; `runner.update(millis(), ble.isConnected())`; `uiDraw(...)`; `delay(1)`.
- [ ] **Step 1: Implement** `ui.*` and `main.cpp`.
- [ ] **Step 2: Build + flash** `pio run -e cardputer -t upload` — Expected: `SUCCESS`; screen shows `BT: aguardando conexao`, `Macro: OFF`.
- [ ] **Step 3: Run tests** `pio test -e cardputer` — Expected: all PASS; then re-flash firmware with `pio run -t upload`.
- [ ] **Step 4: Manual check (Jean)**: pair "M5-Keyboard" on PC → screen shows conectado; press Enter → cursor jumps to 3 points, virtual keyboard shows Shift flashing 10×; countdown from 60 s; press Enter mid-taps → Shift not stuck; hold Enter → toggles only once; turn PC Bluetooth off/on → macro resumes immediately after reconnect.
