// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "app.h"

/// Four-tab keyboard UI on the 240x135 screen (rotation 0): Nos / Conversa / Canal / Config.
/// Text is drawn with Font0 in CP437 mode (display-only conversion of UTF-8; sent text is never altered).
class Ui {
 public:
  explicit Ui(App& app) : app_(app) {}
  /// Sets up the display and the off-screen canvas. Call after App::begin().
  void begin();
  /// Handles keyboard changes (call after M5Cardputer.update()).
  void handleKeys(uint32_t nowMs);
  /// Redraws if anything changed (and at least once per second for ages/toasts).
  void render(uint32_t nowMs);

 private:
  enum Tab : uint8_t { Nodes, Chat, Channel, Config, kTabs };
  enum CfgItem : uint8_t { CfgRegion, CfgPower, CfgLong, CfgShort, CfgGreeting, CfgNodeInfo, kCfgItems };
  static constexpr size_t kInputCap = 200;  // 199 bytes of text + NUL (chat/outbox buffers)

  void setTab(Tab t);
  void openChat(uint32_t peer);
  void keysNodes(const char* chars, size_t n, bool fn, bool enter);
  void keysInput(char* buf, size_t maxBytes, const char* chars, size_t n, bool fn, bool del, bool enter, int& scroll,
                 bool& esc, bool& submit);
  void keysConfig(const char* chars, size_t n, bool fn, bool del, bool enter, uint32_t nowMs);
  void startEdit(CfgItem item);
  void submitEdit(uint32_t nowMs);

  void draw(uint32_t nowMs);
  void drawStatus(uint32_t nowMs);
  void drawTabs();
  void drawNodes(uint32_t nowMs);
  void drawHistory(uint32_t peer, int y0, int rows, int& scroll);
  void drawInput(int y, const char* buf);
  void drawChat();
  void drawChannel();
  void drawConfig();
  void drawRegionPrompt();
  void keysRegionPrompt(const char* chars, size_t n, bool enter, uint32_t nowMs);

  void text(int x, int y, const char* utf8, uint16_t fg, uint16_t bg, size_t maxCells);
  void glyphs(int x, int y, const char* cp437, size_t n, uint16_t fg, uint16_t bg);
  int nodeIndex(uint32_t num) const;

  App& app_;
  Tab tab_ = Nodes;
  uint32_t nodeSel_ = 0;  // selected node number on the Nodes tab
  size_t nodeTop_ = 0;
  uint32_t chatPeer_ = 0;
  char chatInput_[kInputCap] = "";
  char chanInput_[kInputCap] = "";
  int chatScroll_ = 0, chanScroll_ = 0;  // lines scrolled up from the newest
  uint8_t cfgSel_ = 0;
  bool editing_ = false;
  CfgItem editItem_ = CfgRegion;
  char editBuf_[kInputCap] = "";
  bool dirty_ = true;
  uint32_t lastVersion_ = 0xFFFFFFFF;
  uint32_t lastDrawMs_ = 0;
};
