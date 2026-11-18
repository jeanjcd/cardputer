// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui.h"

#include <M5Cardputer.h>
#include <stdio.h>
#include <string.h>

#include "../mesh/hwmodel.h"
#include "../mesh/text.h"

namespace {
constexpr int kW = 240;
constexpr int kCharW = 6;      // Font0 cell width
constexpr int kCols = 40;      // 240 / 6
constexpr int kLineH = 10;
constexpr int kContentY = 22;  // below status bar and tabs
constexpr int kInputY = 124;

constexpr uint16_t kBg = TFT_BLACK;
constexpr uint16_t kBarBg = 0x10A2;  // dark grey-blue
constexpr uint16_t kSel = 0x2945;    // selection background

M5Canvas canvas(&M5Cardputer.Display);
lgfx::LGFXBase* g = nullptr;  // canvas, or the display itself if the canvas could not be allocated
bool useCanvas = false;

// One wrapped history line (CP437 glyphs).
struct Line {
  char glyphs[kCols + 1];
  uint8_t n;
  uint16_t color;
};
constexpr size_t kMaxLines = ChatStore::kMaxMsgs * 7;
Line gLines[kMaxLines];

// Converts src to CP437 and copies exactly `width` cells into dst (truncated or space-padded); returns width.
size_t field(char* dst, const char* src, size_t width) {
  char tmp[kCols + 1];
  size_t n = utf8ToCp437(src, tmp, sizeof tmp);
  for (size_t i = 0; i < width; i++) dst[i] = i < n ? tmp[i] : ' ';
  return width;
}

char stateChar(ChatMsg::State s) {
  switch (s) {
    case ChatMsg::Pending: return '~';
    case ChatMsg::Sent: return '.';
    case ChatMsg::Acked: return 'v';
    case ChatMsg::Failed: return 'x';
    case ChatMsg::WaitingKey: return '?';
  }
  return ' ';
}

void formatAge(char* out, size_t cap, uint32_t ageMs) {
  uint32_t s = ageMs / 1000;
  if (s < 60) snprintf(out, cap, "%2lus", (unsigned long)s);
  else if (s < 3600) snprintf(out, cap, "%2lum", (unsigned long)(s / 60));
  else if (s < 86400) snprintf(out, cap, "%2luh", (unsigned long)(s / 3600));
  else snprintf(out, cap, "%2lud", (unsigned long)(s / 86400 > 99 ? 99 : s / 86400));
}
}  // namespace

void Ui::begin() {
  auto& d = M5Cardputer.Display;  // 240x135 landscape at rotation 0 (never setRotation(1))
  d.fillScreen(kBg);
  canvas.setColorDepth(16);
  useCanvas = canvas.createSprite(kW, 135) != nullptr;
  g = useCanvas ? static_cast<lgfx::LGFXBase*>(&canvas) : static_cast<lgfx::LGFXBase*>(&d);
  g->setFont(&fonts::Font0);
  g->setTextSize(1);
  g->setTextWrap(false);
  g->cp437(true);                           // full 256-glyph Font0 table
  g->setAttribute(lgfx::utf8_switch, 0);    // we pass CP437 glyph indices, not UTF-8
  app_.setViewing(App::kNone);
  dirty_ = true;
}

// ---------------------------------------------------------------- drawing primitives

void Ui::glyphs(int x, int y, const char* cp437, size_t n, uint16_t fg, uint16_t bg) {
  g->setTextColor(fg, bg);
  for (size_t i = 0; i < n && x + kCharW <= kW; i++, x += kCharW) g->drawChar((uint16_t)(uint8_t)cp437[i], x, y);
}

void Ui::text(int x, int y, const char* utf8, uint16_t fg, uint16_t bg, size_t maxCells) {
  char buf[kCols + 1];
  if (maxCells > kCols) maxCells = kCols;
  size_t n = utf8ToCp437(utf8, buf, maxCells + 1);
  glyphs(x, y, buf, n, fg, bg);
}

int Ui::nodeIndex(uint32_t num) const {
  const NodeDb& db = app_.nodes();
  for (size_t i = 0; i < db.count(); i++)
    if (db.at(i).num == num) return (int)i;
  return -1;
}

void Ui::render(uint32_t nowMs) {
  bool changed = app_.version() != lastVersion_;
  if (!dirty_ && !changed && (uint32_t)(nowMs - lastDrawMs_) < 1000) return;
  if (!dirty_ && (uint32_t)(nowMs - lastDrawMs_) < 50) return;  // cap redraw rate for radio-only changes
  lastVersion_ = app_.version();
  lastDrawMs_ = nowMs;
  dirty_ = false;
  draw(nowMs);
}

void Ui::draw(uint32_t nowMs) {
  g->fillScreen(kBg);
  drawStatus(nowMs);
  drawTabs();
  switch (tab_) {
    case Nodes: drawNodes(nowMs); break;
    case Chat: drawChat(); break;
    case Channel: drawChannel(); break;
    case Config: drawConfig(); break;
    default: break;
  }
  if (useCanvas) canvas.pushSprite(0, 0);
}

void Ui::drawStatus(uint32_t nowMs) {
  const char* t = app_.toast(nowMs);
  if (t) {
    g->fillRect(0, 0, kW, 10, TFT_YELLOW);
    text(1, 1, t, TFT_BLACK, TFT_YELLOW, kCols);
    return;
  }
  g->fillRect(0, 0, kW, 10, kBarBg);
  char radio[24], rssi[16], line[64];
  const RegionInfo& r = regionInfo(app_.settings().region);
  if (!app_.capFound()) snprintf(radio, sizeof radio, "SEM CAP LoRa");
  else if (!app_.radioReady()) snprintf(radio, sizeof radio, "RADIO ERRO");
  else snprintf(radio, sizeof radio, "%s %.3f", r.name, app_.freqMHz());
  if (app_.dutyLimited()) snprintf(rssi, sizeof rssi, "duty limit");
  else if (app_.haveRssi()) snprintf(rssi, sizeof rssi, "%ddBm", app_.lastRssi());
  else snprintf(rssi, sizeof rssi, "-dBm");
  const char* autoTxt = app_.autoGreet() ? "AUTO:ON" : "AUTO:OFF";
  unsigned long rx = (unsigned long)app_.rxCount();
  snprintf(line, sizeof line, "%s | %s | rx %lu | %s", radio, autoTxt, rx, rssi);
  if (strlen(line) > kCols) snprintf(line, sizeof line, "%s|%s|rx %lu|%s", radio, autoTxt, rx, rssi);
  uint16_t fg = (!app_.capFound() || !app_.radioReady() || app_.dutyLimited()) ? TFT_RED : TFT_WHITE;
  text(1, 1, line, fg, kBarBg, kCols);
}

void Ui::drawTabs() {
  bool nodeUnread = false;
  const NodeDb& db = app_.nodes();
  for (size_t i = 0; i < db.count(); i++)
    if (db.at(i).unread) nodeUnread = true;
  const char* labels[kTabs] = {nodeUnread ? "N\xC3\xB3s*" : "N\xC3\xB3s", "Conversa",
                               app_.publicUnread() ? "Canal*" : "Canal", "Config"};
  for (int i = 0; i < kTabs; i++) {
    int x0 = i * (kW / kTabs);
    bool active = i == tab_;
    uint16_t bg = active ? TFT_DARKCYAN : kBg;
    g->fillRect(x0, 11, kW / kTabs - 1, 10, bg);
    char buf[16];
    size_t n = utf8ToCp437(labels[i], buf, sizeof buf);
    int x = x0 + (kW / kTabs - (int)n * kCharW) / 2;
    glyphs(x, 12, buf, n, active ? TFT_WHITE : TFT_LIGHTGREY, bg);
  }
}

void Ui::drawNodes(uint32_t nowMs) {
  const NodeDb& db = app_.nodes();
  constexpr int kRows = 10;
  if (db.count() == 0) {
    text(0, kContentY + 2 * kLineH, "Nenhum n\xC3\xB3 ouvido ainda.", TFT_LIGHTGREY, kBg, kCols);
    text(0, kContentY + 3 * kLineH, "Aguarde pacotes da rede Meshtastic.", TFT_DARKGREY, kBg, kCols);
  } else {
    int sel = nodeIndex(nodeSel_);
    if (sel < 0) { sel = 0; nodeSel_ = db.at(0).num; }
    if ((size_t)sel < nodeTop_) nodeTop_ = (size_t)sel;
    if ((size_t)sel >= nodeTop_ + kRows) nodeTop_ = (size_t)sel - kRows + 1;
    if (nodeTop_ >= db.count()) nodeTop_ = 0;
    for (int row = 0; row < kRows && nodeTop_ + row < db.count(); row++) {
      const NodeEntry& e = db.at(nodeTop_ + row);
      char cells[kCols + 1];
      char tmp[24];
      size_t w = 0;
      if (e.user.shortName[0]) w += field(cells + w, e.user.shortName, 4);
      else { snprintf(tmp, sizeof tmp, "%04lx", (unsigned long)(e.num & 0xFFFF)); w += field(cells + w, tmp, 4); }
      cells[w++] = ' ';
      if (e.user.longName[0]) w += field(cells + w, e.user.longName, 10);
      else { snprintf(tmp, sizeof tmp, "!%08lx", (unsigned long)e.num); w += field(cells + w, tmp, 10); }
      cells[w++] = ' ';
      if (e.user.longName[0] || e.user.hwModel) hwModelName(e.user.hwModel, tmp, sizeof tmp);
      else snprintf(tmp, sizeof tmp, "-");
      w += field(cells + w, tmp, 13);
      cells[w++] = ' ';
      bool heard = e.lastHeardMs != 0;
      if (heard) snprintf(tmp, sizeof tmp, "%4d", e.rssi);
      else snprintf(tmp, sizeof tmp, "   -");
      w += field(cells + w, tmp, 4);
      cells[w++] = ' ';
      if (heard) formatAge(tmp, sizeof tmp, nowMs - e.lastHeardMs);
      else snprintf(tmp, sizeof tmp, "  -");
      w += field(cells + w, tmp, 3);
      cells[w++] = e.unread ? '*' : ' ';
      cells[w++] = app_.greeted(e.num) ? '+' : ' ';
      bool selected = (int)(nodeTop_ + row) == sel;
      uint16_t bg = selected ? kSel : kBg;
      uint16_t fg = e.unread ? TFT_YELLOW : (e.hasKey ? TFT_WHITE : TFT_DARKGREY);
      int y = kContentY + row * kLineH;
      if (selected) g->fillRect(0, y - 1, kW, kLineH, bg);
      glyphs(0, y, cells, w, fg, bg);
    }
  }
  text(0, kInputY, "Enter:conversa A:auto-sauda\xC3\xA7\xC3\xA3o Tab:tela", TFT_DARKGREY, kBg, kCols);
}

void Ui::drawHistory(uint32_t peer, int y0, int rows, int& scroll) {
  const ChatStore& cs = app_.chat();
  size_t count = cs.count(peer);
  size_t L = 0;
  char conv[Settings::kMaxGreeting + 2];
  for (size_t i = 0; i < count && L < kMaxLines; i++) {
    const ChatMsg& m = cs.at(peer, i);
    size_t n = utf8ToCp437(m.text, conv, sizeof conv);
    uint16_t color = !m.mine ? TFT_WHITE : (m.state == ChatMsg::Failed ? TFT_RED : TFT_CYAN);
    size_t pos = 0;
    bool first = true;
    do {
      Line& ln = gLines[L];
      size_t w = 0;
      if (first) {
        ln.glyphs[w++] = m.mine ? stateChar(m.state) : '<';
        ln.glyphs[w++] = m.mine ? '>' : ' ';
      } else {
        ln.glyphs[w++] = ' ';
        ln.glyphs[w++] = ' ';
      }
      while (w < kCols && pos < n) ln.glyphs[w++] = conv[pos++];
      ln.n = (uint8_t)w;
      ln.color = color;
      first = false;
      L++;
    } while (pos < n && L < kMaxLines);
  }
  if (L == 0) {
    text(0, y0, "(sem mensagens)", TFT_DARKGREY, kBg, kCols);
    return;
  }
  int maxScroll = (int)L > rows ? (int)L - rows : 0;
  if (scroll > maxScroll) scroll = maxScroll;
  int start = (int)L - rows - scroll;
  if (start < 0) start = 0;
  for (int r = 0; r < rows && start + r < (int)L; r++) {
    const Line& ln = gLines[start + r];
    glyphs(0, y0 + r * kLineH, ln.glyphs, ln.n, ln.color, kBg);
  }
  if (scroll > 0) glyphs(kW - kCharW, y0, "^", 1, TFT_YELLOW, kBg);
}

void Ui::drawInput(int y, const char* buf) {
  g->drawFastHLine(0, y - 2, kW, TFT_DARKGREY);
  char conv[Settings::kMaxGreeting + 2];
  size_t n = utf8ToCp437(buf, conv, sizeof conv);
  constexpr size_t kVisible = kCols - 3;  // "> " + text + cursor
  const char* start = n > kVisible ? conv + (n - kVisible) : conv;
  size_t shown = n > kVisible ? kVisible : n;
  glyphs(0, y, "> ", 2, TFT_GREEN, kBg);
  glyphs(2 * kCharW, y, start, shown, TFT_GREEN, kBg);
  glyphs((2 + (int)shown) * kCharW, y, "_", 1, TFT_GREEN, kBg);
}

void Ui::drawChat() {
  if (chatPeer_ == 0) {
    text(0, kContentY + 3 * kLineH, "Escolha um n\xC3\xB3 na aba N\xC3\xB3s (Enter).", TFT_LIGHTGREY, kBg, kCols);
    return;
  }
  int idx = nodeIndex(chatPeer_);
  char hdr[96], model[20];
  if (idx >= 0) {
    const NodeEntry& e = app_.nodes().at((size_t)idx);
    hwModelName(e.user.hwModel, model, sizeof model);
    const char* chip = hwChipFamily(e.user.hwModel);
    snprintf(hdr, sizeof hdr, "!%08lx %s %s %s%s", (unsigned long)chatPeer_, e.user.longName, model, chip,
             e.hasKey ? "" : " (sem chave)");
    text(0, kContentY, hdr, e.hasKey ? TFT_YELLOW : TFT_ORANGE, kBg, kCols);
  } else {
    snprintf(hdr, sizeof hdr, "!%08lx (fora da lista)", (unsigned long)chatPeer_);
    text(0, kContentY, hdr, TFT_ORANGE, kBg, kCols);
  }
  drawHistory(chatPeer_, kContentY + kLineH, 9, chatScroll_);
  drawInput(kInputY, chatInput_);
}

void Ui::drawChannel() {
  drawHistory(App::kPublic, kContentY, 10, chanScroll_);
  drawInput(kInputY, chanInput_);
}

void Ui::drawConfig() {
  const Settings& s = app_.settings();
  const RegionInfo& r = regionInfo(s.region);
  const char* labels[kCfgItems] = {"Regi\xC3\xA3o", "Pot\xC3\xAAncia", "Nome longo", "Nome curto",
                                   "Sauda\xC3\xA7\xC3\xA3o", "Enviar NodeInfo"};
  char value[Settings::kMaxGreeting + 32];
  for (int i = 0; i < kCfgItems; i++) {
    switch (i) {
      case CfgRegion:
        snprintf(value, sizeof value, "%s %.3f%s", r.name, app_.freqMHz(), r.experimental ? " (exp)" : "");
        break;
      case CfgPower:
        if (app_.effectivePower() != s.txPower)
          snprintf(value, sizeof value, "%d dBm (efetiva %d)", s.txPower, app_.effectivePower());
        else
          snprintf(value, sizeof value, "%d dBm", s.txPower);
        break;
      case CfgLong: utf8Copy(value, sizeof value, s.longName); break;
      case CfgShort: utf8Copy(value, sizeof value, s.shortName); break;
      case CfgGreeting: app_.greeting(value, sizeof value); break;
      default: value[0] = 0; break;
    }
    int y = kContentY + i * kLineH;
    bool selected = i == cfgSel_;
    uint16_t bg = selected ? kSel : kBg;
    if (selected) g->fillRect(0, y - 1, kW, kLineH, bg);
    char cells[kCols + 1];
    size_t w = field(cells, labels[i], 11);
    glyphs(0, y, cells, w, TFT_LIGHTGREY, bg);
    text(11 * kCharW, y, value, TFT_WHITE, bg, kCols - 11);
  }

  // Identity: node id and public-key fingerprint only (the private key is never shown).
  const Identity& me = app_.me();
  char line[64];
  snprintf(line, sizeof line, "ID !%08lx  chave %02x%02x%02x%02x", (unsigned long)me.nodeNum, me.pub[0], me.pub[1],
           me.pub[2], me.pub[3]);
  text(0, kContentY + 6 * kLineH + 2, line, TFT_GREEN, kBg, kCols);
  if (app_.idStatus() == IdentityStatus::CreatedNotSaved)
    text(0, kContentY + 7 * kLineH + 2, "ERRO: chave n\xC3\xA3o gravada na NVS!", TFT_RED, kBg, kCols);
  else if (app_.airtimeBudgetMs()) {
    snprintf(line, sizeof line, "Ar 1h: %lus de %lus (duty %u%%)", (unsigned long)(app_.airtimeUsedMs() / 1000),
             (unsigned long)(app_.airtimeBudgetMs() / 1000), (unsigned)r.dutyCycle);
    text(0, kContentY + 7 * kLineH + 2, line, TFT_LIGHTGREY, kBg, kCols);
  }
  if (r.experimental)
    text(0, kContentY + 8 * kLineH + 2, "! Fora de 868-923 MHz do Cap (exp)", TFT_ORANGE, kBg, kCols);

  if (editing_) {
    g->fillRect(0, kInputY - 22, kW, 135 - (kInputY - 22), kBg);
    snprintf(line, sizeof line, "%s: Enter salva, ` cancela", labels[editItem_]);
    text(0, kInputY - 12, line, TFT_YELLOW, kBg, kCols);
    drawInput(kInputY, editBuf_);
  } else {
    text(0, kInputY, "Enter:editar/acionar  ;/. move", TFT_DARKGREY, kBg, kCols);
  }
}

// ---------------------------------------------------------------- keys

void Ui::setTab(Tab t) {
  if (tab_ == Config && t != Config) app_.flushSettings(millis());
  tab_ = t;
  if (t != Config) editing_ = false;
  if (t == Chat) app_.setViewing(chatPeer_ ? chatPeer_ : App::kNone);
  else if (t == Channel) app_.setViewing(App::kPublic);
  else app_.setViewing(App::kNone);
  dirty_ = true;
}

void Ui::openChat(uint32_t peer) {
  chatPeer_ = peer;
  app_.greeting(chatInput_, sizeof chatInput_);  // pre-filled suggestion
  chatScroll_ = 0;
  setTab(Chat);
}

void Ui::handleKeys(uint32_t nowMs) {
  auto& kb = M5Cardputer.Keyboard;
  if (!kb.isChange() || !kb.isPressed()) return;
  auto ks = kb.keysState();
  dirty_ = true;
  char chars[16];
  size_t n = 0;
  for (char c : ks.word)
    if (n < sizeof chars) chars[n++] = c;

  if (ks.tab) {
    setTab((Tab)((tab_ + 1) % kTabs));
    return;
  }
  bool esc = false, submit = false;
  switch (tab_) {
    case Nodes:
      keysNodes(chars, n, ks.fn, ks.enter);
      break;
    case Chat:
      keysInput(chatInput_, sizeof chatInput_ - 1, chars, n, ks.fn, ks.del, ks.enter, chatScroll_, esc, submit);
      if (esc) setTab(Nodes);
      else if (submit && chatPeer_ && chatInput_[0] && app_.sendText(chatPeer_, chatInput_, nowMs)) {
        chatInput_[0] = 0;
        chatScroll_ = 0;
      }
      break;
    case Channel:
      keysInput(chanInput_, sizeof chanInput_ - 1, chars, n, ks.fn, ks.del, ks.enter, chanScroll_, esc, submit);
      if (esc) setTab(Nodes);
      else if (submit && chanInput_[0] && app_.sendText(App::kPublic, chanInput_, nowMs)) {
        chanInput_[0] = 0;
        chanScroll_ = 0;
      }
      break;
    case Config:
      keysConfig(chars, n, ks.fn, ks.del, ks.enter, nowMs);
      break;
    default:
      break;
  }
}

void Ui::keysNodes(const char* chars, size_t n, bool fn, bool enter) {
  (void)fn;
  const NodeDb& db = app_.nodes();
  for (size_t i = 0; i < n; i++) {
    char c = chars[i];
    if (c == 'a' || c == 'A') {
      app_.toggleAutoGreet();
    } else if ((c == ';' || c == '.') && db.count()) {
      int sel = nodeIndex(nodeSel_);
      if (sel < 0) sel = 0;
      if (c == ';' && sel > 0) sel--;
      if (c == '.' && (size_t)sel + 1 < db.count()) sel++;
      nodeSel_ = db.at((size_t)sel).num;
    }
  }
  if (enter && db.count()) {
    int sel = nodeIndex(nodeSel_);
    openChat(db.at(sel < 0 ? 0 : (size_t)sel).num);
  }
}

void Ui::keysInput(char* buf, size_t maxBytes, const char* chars, size_t n, bool fn, bool del, bool enter,
                   int& scroll, bool& esc, bool& submit) {
  for (size_t i = 0; i < n; i++) {
    char c = chars[i];
    if (c == '`') esc = true;                      // Esc key
    else if (fn && c == ';') scroll++;             // Fn+; = up
    else if (fn && c == '.') { if (scroll > 0) scroll--; }  // Fn+. = down
    else if (fn) continue;                         // other Fn combos: ignore
    else textAppendAscii(buf, kInputCap, maxBytes, c);
  }
  if (scroll > (int)kMaxLines) scroll = (int)kMaxLines;
  if (del) utf8PopBack(buf);
  if (enter) submit = true;
}

void Ui::startEdit(CfgItem item) {
  const Settings& s = app_.settings();
  editItem_ = item;
  editing_ = true;
  switch (item) {
    case CfgPower: snprintf(editBuf_, sizeof editBuf_, "%d", s.txPower); break;
    case CfgLong: utf8Copy(editBuf_, sizeof editBuf_, s.longName); break;
    case CfgShort: utf8Copy(editBuf_, sizeof editBuf_, s.shortName); break;
    case CfgGreeting: app_.greeting(editBuf_, sizeof editBuf_); break;
    default: editing_ = false; break;
  }
}

void Ui::submitEdit(uint32_t nowMs) {
  bool ok = true;
  switch (editItem_) {
    case CfgPower: {
      int v = 0;
      size_t len = strnlen(editBuf_, 3);
      ok = len >= 1 && len <= 2;
      for (size_t i = 0; ok && i < len; i++) {
        if (editBuf_[i] < '0' || editBuf_[i] > '9') ok = false;
        else v = v * 10 + (editBuf_[i] - '0');
      }
      ok = ok && app_.setTxPower(v, nowMs);
      if (!ok) app_.showToast("Pot\xC3\xAAncia: 2 a 22 dBm", nowMs);
      break;
    }
    case CfgLong:
      ok = app_.setLongName(editBuf_, nowMs);
      if (!ok) app_.showToast("Nome longo: 1 a 24 bytes", nowMs);
      break;
    case CfgShort:
      ok = app_.setShortName(editBuf_, nowMs);
      if (!ok) app_.showToast("Nome curto: 1 a 4 letras", nowMs);
      break;
    case CfgGreeting:
      ok = app_.setGreeting(editBuf_, nowMs);
      if (!ok) app_.showToast("Sauda\xC3\xA7\xC3\xA3o inv\xC3\xA1lida", nowMs);
      break;
    default:
      break;
  }
  if (ok) editing_ = false;
}

void Ui::keysConfig(const char* chars, size_t n, bool fn, bool del, bool enter, uint32_t nowMs) {
  if (editing_) {
    size_t maxBytes = editItem_ == CfgPower ? 2
                      : editItem_ == CfgLong ? Settings::kMaxLongName
                      : editItem_ == CfgShort ? Settings::kMaxShortName
                                              : Settings::kMaxGreeting;
    char filtered[16];
    size_t m = 0;
    for (size_t i = 0; i < n; i++)
      if (editItem_ != CfgPower || chars[i] == '`' || (chars[i] >= '0' && chars[i] <= '9')) filtered[m++] = chars[i];
    int dummy = 0;
    bool esc = false, submit = false;
    keysInput(editBuf_, maxBytes, filtered, m, fn, del, enter, dummy, esc, submit);
    if (esc) editing_ = false;
    else if (submit) submitEdit(nowMs);
    return;
  }
  for (size_t i = 0; i < n; i++) {
    char c = chars[i];
    if (c == ';' && cfgSel_ > 0) cfgSel_--;
    else if (c == '.' && cfgSel_ + 1 < kCfgItems) cfgSel_++;
    else if (c == '`') { setTab(Nodes); return; }
  }
  if (enter) {
    switch (cfgSel_) {
      case CfgRegion: app_.cycleRegion(nowMs); break;
      case CfgNodeInfo: app_.requestNodeInfo(nowMs); break;
      default: startEdit((CfgItem)cfgSel_); break;
    }
  }
}
