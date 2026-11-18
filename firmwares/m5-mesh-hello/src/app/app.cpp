// SPDX-License-Identifier: GPL-3.0-or-later
#include "app.h"

#include <Arduino.h>
#include <esp_timer.h>
#include <stdio.h>
#include <string.h>

#include "../mesh/text.h"

namespace {
constexpr const char* kChannelName = "LongFast";
constexpr float kBwKHz = 250.0f;
constexpr uint32_t kBroadcast = 0xFFFFFFFFu;
constexpr uint32_t kTxGapMs = 1500;                        // airtime courtesy between our frames
constexpr uint32_t kNodeInfoPeriodMs = 3UL * 3600 * 1000;  // periodic NodeInfo broadcast
constexpr uint32_t kManualNodeInfoMs = 30000;              // manual/rename NodeInfo broadcasts
constexpr uint32_t kNodeInfoRequestMs = 60000;             // direct NodeInfo requests per node
constexpr uint32_t kToastMs = 2500;

App* gApp = nullptr;

uint64_t uptimeMs() { return (uint64_t)(esp_timer_get_time() / 1000); }
}  // namespace

const uint8_t* App::lookupKey(uint32_t num) { return gApp ? gApp->db_.pubKey(num) : nullptr; }

void App::begin() {
  gApp = this;
  kv_.begin();
  // Identity first: it may use the hardware RNG (bootloader_random_*), which must not overlap ADC use.
  idStatus_ = loadOrCreateIdentity(kv_, me_);
  uint8_t seed[8];
  hwRandomBytes(seed, sizeof seed);  // packet ids/nonces must not repeat across reboots
  for (size_t i = 0; i < sizeof seed; i++) rng_ = (rng_ << 8) | seed[i];
  if (rng_ == 0) rng_ = 0x9E3779B97F4A7C15ULL;

  regionPrompt_ = settingsNeedRegionPrompt(settingsLoad(kv_, settings_));
  utf8Copy(me_.longName, sizeof me_.longName, settings_.longName);
  utf8Copy(me_.shortName, sizeof me_.shortName, settings_.shortName);

  db_.load();
  greeter_.load();  // auto-greet stays off at every boot

  radio_.begin(freqMHz(), effectivePower());  // RX only until a region is confirmed (see txHeld)
  if (!regionPrompt_) {
    niPending_ = true;  // boot announcement asks neighbours for their NodeInfo
    niWantResponse_ = true;
  }
  if (idStatus_ == IdentityStatus::CreatedNotSaved) showToast("ERRO: chave nao gravada!", millis());
  else if (idStatus_ == IdentityStatus::Created) showToast("Nova chave gerada", millis());
  changed();
}

float App::freqMHz() const { return regionFreqMHz(settings_.region, kChannelName, kBwKHz); }

uint32_t App::airtimeUsedMs() const { return airtime_.usedMs(uptimeMs()); }

uint32_t App::airtimeBudgetMs() const {
  uint8_t d = regionInfo(settings_.region).dutyCycle;
  return d >= 100 ? 0 : (uint32_t)d * 36000;
}

uint32_t App::random32() {
  rng_ ^= rng_ >> 12;  // xorshift64*
  rng_ ^= rng_ << 25;
  rng_ ^= rng_ >> 27;
  return (uint32_t)((rng_ * 2685821657736338717ULL) >> 32);
}

uint32_t App::newPacketId() { return nextPacketId(random32()); }

void App::showToast(const char* text, uint32_t nowMs) {
  utf8Copy(toast_, sizeof toast_, text);
  toastUntilMs_ = nowMs + kToastMs;
  changed();
}

const char* App::toast(uint32_t nowMs) const {
  return (toast_[0] && (int32_t)(toastUntilMs_ - nowMs) > 0) ? toast_ : nullptr;
}

bool App::saveSettings(uint32_t nowMs) {
  settingsDirty_ = false;  // the whole struct is written, including any pending region change
  bool ok = settingsSave(kv_, settings_);
  showToast(ok ? "Config salva" : "ERRO ao gravar config", nowMs);
  return ok;
}

// ---------------------------------------------------------------- receive

void App::tick(uint32_t nowMs) {
  uint8_t buf[Radio::kMaxFrame + 1];
  size_t len = sizeof buf;
  int16_t rssi = 0;
  float snr = 0;
  if (radio_.poll(buf, len, rssi, snr)) handleFrame(buf, len, rssi, snr, nowMs);
  if (settingsDirty_ && (uint32_t)(nowMs - settingsDirtyMs_) >= 2000) flushSettings(nowMs);
  serviceHeld(nowMs);
  schedule(nowMs);
  transmitOne(nowMs);
}

void App::handleFrame(const uint8_t* buf, size_t len, int16_t rssi, float snr, uint32_t nowMs) {
  rxCount_++;
  haveRssi_ = true;
  lastRssi_ = rssi;
  changed();

  PacketHeader h;
  if (!headerParse(buf, len, h) || h.from == me_.nodeNum || h.from == kBroadcast) return;
  if (dedup_.contains(h.from, h.id)) {
    // Retransmission of a DM we already ACKed: our ACK was probably lost, send it again.
    if (h.to == me_.nodeNum && h.wantAck && acked_.allowReAck(h.from, h.id)) sendAck(h.from, h.id);
    return;
  }

  static Inbound in;  // large: keep it off the loop task stack (no heap in the RX path)
  bool ok = parseFrame(me_, buf, len, lookupKey, in);
  if (!ok && in.kind != InKind::Other) {
    // Undecodable frames are NOT recorded in the dedup cache, so a retransmission can still be read later
    // (e.g. a PKI DM that arrived before the sender's key).
    if (h.channel == 0x00 && h.to == me_.nodeNum && !db_.pubKey(h.from)) onUnknownKeyPki(h, nowMs);
    return;
  }
  dedup_.record(h.from, h.id);  // only frames we fully handled
  db_.touch(h.from, rssi, snr, nowMs);
  if (!ok) return;  // decoded but uninteresting (position, telemetry...): activity only

  switch (in.kind) {
    case InKind::NodeInfo: onNodeInfo(in, nowMs); break;
    case InKind::Text: onText(in); break;
    case InKind::Ack: onAck(in); break;
    default: break;
  }
}

void App::onNodeInfo(const Inbound& in, uint32_t nowMs) {
  uint32_t from = in.h.from;
  bool hadKey = db_.pubKey(from) != nullptr;
  bool ok = db_.applyUser(from, in.user);  // first key wins
  if (ok && db_.pubKey(from)) {
    greeter_.onNodeWithKey(from);
    if (!hadKey) keyArrived(from);
  }
  // Replies are throttled per node (12 h) and globally (30 s) so spoofed senders cannot keep us on the air.
  if (in.data.wantResponse && (in.direct || in.h.to == kBroadcast) && niReplies_.allow(from, nowMs))
    sendNodeInfoTo(from, false);
}

void App::onUnknownKeyPki(const PacketHeader& h, uint32_t nowMs) {
  // Tell the sender we cannot decrypt (PKI_UNKNOWN_PUBKEY) and send our NodeInfo asking for theirs, so the key
  // exchange completes and its next retry decrypts. Rate-limited per node (10 min) and globally (30 s).
  if (!unknownKey_.allow(h.from, nowMs)) return;
  uint8_t f[Radio::kMaxFrame + 1];
  size_t n = buildRoutingError(me_, newPacketId(), h.from, h.id, kRoutingPkiUnknownPubkey, f);
  if (n) enqueue(f, n, 0, true);
  sendNodeInfoTo(h.from, true);
}

void App::keyArrived(uint32_t num) {
  outbox_.onKeyArrived(num);
  size_t n = chat_.count(num);
  for (size_t i = 0; i < n; i++) {
    const ChatMsg& m = chat_.at(num, i);
    if (m.mine && m.state == ChatMsg::WaitingKey) {
      ChatMsg* p = chat_.findById(m.id);
      if (p) p->state = ChatMsg::Pending;
    }
  }
}

void App::onText(const Inbound& in) {
  char text[sizeof in.data.payload + 1];
  size_t n = in.data.payloadLen < sizeof in.data.payload ? in.data.payloadLen : sizeof in.data.payload;
  memcpy(text, in.data.payload, n);
  text[n] = 0;
  uint32_t from = in.h.from;
  ChatMsg m;
  memset(&m, 0, sizeof m);
  m.mine = false;
  m.id = in.h.id;
  m.state = ChatMsg::Sent;

  if (in.pki && in.direct) {
    m.setText(text);
    chat_.add(from, m);
    if (viewing_ != from) {
      NodeEntry* e = db_.get(from);
      if (e && e->unread < 255) e->unread++;
    }
    if (in.h.wantAck) {
      sendAck(from, in.h.id);
      acked_.record(from, in.h.id);
    }
    persistPeerOnce(from);
  } else if (in.h.to == kBroadcast) {
    // Public channel: ChatMsg has no sender field, so prefix the stored copy with the sender's short name.
    char who[8];
    const NodeEntry* e = db_.get(from);
    if (e && e->user.shortName[0]) utf8Copy(who, sizeof who, e->user.shortName);
    else snprintf(who, sizeof who, "%04lx", (unsigned long)(from & 0xFFFF));
    char line[sizeof m.text];
    size_t w = 0;
    for (const char* p = who; *p && w + 1 < sizeof line; p++) line[w++] = *p;
    for (const char* p = ": "; *p && w + 1 < sizeof line; p++) line[w++] = *p;
    line[w] = 0;
    utf8Copy(line + w, sizeof line - w, text);
    m.setText(line);
    chat_.add(kPublic, m);
    if (viewing_ != kPublic && publicUnread_ < 255) publicUnread_++;
  }
}

void App::onAck(const Inbound& in) {
  if (in.routingError == 0) {
    chat_.markAcked(in.ackedId);
  } else {
    setMsgState(in.ackedId, ChatMsg::Sent, ChatMsg::Pending, ChatMsg::Failed);  // NAK
  }
}

void App::setMsgState(uint32_t msgId, ChatMsg::State from1, ChatMsg::State from2, ChatMsg::State to) {
  ChatMsg* m = chat_.findById(msgId);
  if (m && m->mine && (m->state == from1 || m->state == from2)) {
    m->state = to;
    changed();
  }
}

void App::persistPeerOnce(uint32_t num) {
  for (size_t i = 0; i < persistedCount_; i++)
    if (persisted_[i] == num) return;
  if (!db_.pubKey(num)) return;
  db_.persistPeer(num);
  if (persistedCount_ < NodeDb::kMaxPeers) {
    persisted_[persistedCount_++] = num;
  } else {  // full: replace the oldest entry so each peer is written once, not on every message
    persisted_[persistedNext_] = num;
    persistedNext_ = (persistedNext_ + 1) % NodeDb::kMaxPeers;
  }
}

// ---------------------------------------------------------------- transmit

bool App::txHeld() const {
  return !txMayTransmit(!regionPrompt_, settingsDirty_, radio_.ready(), radio_.capFound());
}

bool App::enqueue(const uint8_t* frame, size_t len, uint32_t msgId, bool urgent) {
  if (regionPrompt_) return false;  // first boot: no region confirmed, so nothing may even be queued for TX
  if (len == 0 || len > Radio::kMaxFrame || txCount_ >= kTxQueue) return false;
  size_t slot;
  if (urgent) {
    txHead_ = (txHead_ + kTxQueue - 1) % kTxQueue;
    slot = txHead_;
  } else {
    slot = (txHead_ + txCount_) % kTxQueue;
  }
  memcpy(txq_[slot].frame, frame, len);
  txq_[slot].len = (uint16_t)len;
  txq_[slot].msgId = msgId;
  txCount_++;
  return true;
}

void App::sendAck(uint32_t to, uint32_t ackedId) {
  uint8_t f[Radio::kMaxFrame + 1];
  size_t n = buildAck(me_, newPacketId(), to, ackedId, f);
  if (n) enqueue(f, n, 0, true);
}

void App::sendNodeInfoTo(uint32_t to, bool wantResponse) {
  uint8_t f[Radio::kMaxFrame + 1];
  size_t n = buildNodeInfo(me_, newPacketId(), to, wantResponse, f);
  if (n) enqueue(f, n, 0, false);
}

// Held DMs (waiting for the peer's key): expire after 10 min (marked failed) and, meanwhile, re-ask each such node
// for its NodeInfo at most once per kNodeInfoRequestMs.
void App::serviceHeld(uint32_t nowMs) {
  uint32_t id;
  while (outbox_.expireOne(nowMs, &id)) setMsgState(id, ChatMsg::WaitingKey, ChatMsg::WaitingKey, ChatMsg::Failed);
  if (txHeld() || txCount_ >= kTxQueue) return;
  uint32_t peers[Outbox::kMax];
  size_t n = outbox_.heldPeers(peers, Outbox::kMax);
  for (size_t i = 0; i < n && txCount_ < kTxQueue; i++)
    if (niRequests_.allow(peers[i], nowMs, kNodeInfoRequestMs)) sendNodeInfoTo(peers[i], true);
}

void App::schedule(uint32_t nowMs) {
  if (haveNi_ && (uint32_t)(nowMs - lastNiMs_) >= kNodeInfoPeriodMs && !niPending_) {
    niPending_ = true;
    niWantResponse_ = false;
  }
  if (txCount_ != 0) return;  // one frame at a time: refill only when the queue is empty

  if (niPending_ && (!haveManualNi_ || (uint32_t)(nowMs - lastManualNiMs_) >= kManualNodeInfoMs)) {
    uint8_t f[Radio::kMaxFrame + 1];
    size_t n = buildNodeInfo(me_, newPacketId(), kBroadcast, niWantResponse_, f);
    if (n && enqueue(f, n, 0, false)) {
      niPending_ = false;
      haveNi_ = haveManualNi_ = true;
      lastNiMs_ = lastManualNiMs_ = nowMs;
    }
    return;
  }

  OutItem item;
  if (outbox_.next(item)) {
    const uint8_t* key = db_.pubKey(item.to);
    uint8_t f[Radio::kMaxFrame + 1];
    size_t n = key ? buildDirectText(me_, item.chatId, item.to, key, random32(), item.text, f) : 0;
    if (n && enqueue(f, n, item.chatId, false)) persistPeerOnce(item.to);
    else setMsgState(item.chatId, ChatMsg::Pending, ChatMsg::WaitingKey, ChatMsg::Failed);
    return;
  }

  uint32_t num;
  if (greeter_.due(nowMs, num)) {
    const uint8_t* key = db_.pubKey(num);
    if (!key) return;  // evicted meanwhile; it stays marked greeted (never twice beats once)
    char g[Settings::kMaxGreeting + 1];
    composeGreeting(settings_, g, sizeof g);
    uint32_t id = newPacketId();
    uint8_t f[Radio::kMaxFrame + 1];
    size_t n = buildDirectText(me_, id, num, key, random32(), g, f);
    if (!n || !enqueue(f, n, id, false)) return;
    ChatMsg m;
    memset(&m, 0, sizeof m);
    m.mine = true;
    m.id = id;
    m.state = ChatMsg::Pending;
    m.setText(g);
    chat_.add(num, m);
    persistPeerOnce(num);
    changed();
  }
}

void App::transmitOne(uint32_t nowMs) {
  if (txCount_ == 0) {
    if (dutyLimited_) { dutyLimited_ = false; changed(); }
    return;
  }
  // No region confirmed, region being changed, or no working radio/Cap: keep frames queued (no airtime charged, DMs
  // not failed); the status bar shows the reason.
  if (txHeld()) return;
  if (haveTx_ && (uint32_t)(nowMs - lastTxEndMs_) < kTxGapMs) return;
  TxItem& t = txq_[txHead_];
  uint32_t airMs = (longFastAirtimeUs(t.len) + 999) / 1000;
  uint8_t duty = regionInfo(settings_.region).dutyCycle;
  bool allowed = airtime_.allows(uptimeMs(), airMs, duty);
  if (allowed != !dutyLimited_) { dutyLimited_ = !allowed; changed(); }
  if (!allowed) return;  // hold the frame (and everything behind it) until the budget frees up

  bool ok = radio_.send(t.frame, t.len);
  airtime_.record(uptimeMs(), airMs);
  lastTxEndMs_ = millis();
  haveTx_ = true;
  uint32_t msgId = t.msgId;
  txHead_ = (txHead_ + 1) % kTxQueue;
  txCount_--;
  if (msgId) setMsgState(msgId, ChatMsg::Pending, ChatMsg::Pending, ok ? ChatMsg::Sent : ChatMsg::Failed);
  if (!ok) showToast("Falha no TX", lastTxEndMs_);
  changed();
}

// ---------------------------------------------------------------- UI actions

void App::setViewing(uint32_t peer) {
  viewing_ = peer;
  if (peer == kPublic) {
    publicUnread_ = 0;
  } else if (peer != kNone) {
    NodeEntry* e = db_.get(peer);
    if (e) e->unread = 0;
  }
  changed();
}

void App::toggleAutoGreet() {
  bool on = !greeter_.enabled();
  greeter_.setEnabled(on);
  if (on) {
    // Also greet keyed nodes already heard this boot (newest first; the greeter caps its queue at 10).
    for (size_t i = 0; i < db_.count(); i++) {
      const NodeEntry& e = db_.at(i);
      if (e.hasKey && e.lastHeardMs != 0) greeter_.onNodeWithKey(e.num);
    }
  }
  changed();
}

bool App::sendText(uint32_t peer, const char* text, uint32_t nowMs) {
  if (!text || !text[0]) return false;
  if (strnlen(text, Settings::kMaxGreeting + 1) > Settings::kMaxGreeting) {
    showToast("Texto longo demais", nowMs);
    return false;
  }
  ChatMsg m;
  memset(&m, 0, sizeof m);
  m.mine = true;

  if (peer == kPublic) {
    uint32_t id = newPacketId();
    uint8_t f[Radio::kMaxFrame + 1];
    size_t n = buildBroadcastText(me_, id, text, f);
    if (!n || !enqueue(f, n, id, false)) {
      showToast("Fila de TX cheia", nowMs);
      return false;
    }
    m.id = id;
    m.state = ChatMsg::Pending;
    m.setText(text);
    chat_.add(kPublic, m);
    changed();
    return true;
  }
  if (peer == kNone || peer == kBroadcast || peer == me_.nodeNum) return false;

  bool keyKnown = db_.pubKey(peer) != nullptr;
  OutItem item;
  memset(&item, 0, sizeof item);
  item.to = peer;
  utf8Copy(item.text, sizeof item.text, text);
  item.chatId = newPacketId();
  uint32_t evicted = 0;
  if (!outbox_.queueDirect(item, keyKnown, nowMs, &evicted)) {
    showToast("Fila de mensagens cheia", nowMs);
    return false;
  }
  if (evicted) setMsgState(evicted, ChatMsg::WaitingKey, ChatMsg::WaitingKey, ChatMsg::Failed);  // oldest held DM gave way
  m.id = item.chatId;
  m.state = keyKnown ? ChatMsg::Pending : ChatMsg::WaitingKey;
  m.setText(text);
  chat_.add(peer, m);
  if (!keyKnown) {
    // Ask the node for its NodeInfo (and give it ours); the DM waits for the key.
    if (niRequests_.allow(peer, nowMs, kNodeInfoRequestMs)) sendNodeInfoTo(peer, true);
    showToast("Aguardando chave do no", nowMs);
  }
  char g[Settings::kMaxGreeting + 1];
  composeGreeting(settings_, g, sizeof g);
  if (strcmp(g, text) == 0) greeter_.markGreeted(peer);  // a manual greeting counts: never greet twice
  changed();
  return true;
}

void App::retune(uint32_t nowMs) {
  bool freqOk = radio_.setFreq(freqMHz());
  bool powerOk = radio_.setPower(effectivePower());  // always re-apply the region's power limit
  changed();
  if (!freqOk || !powerOk) showToast("ERRO no radio", nowMs);
  else if (regionInfo(settings_.region).experimental) showToast("Fora da faixa do Cap (exp)", nowMs);
}

void App::cycleRegion(uint32_t nowMs) {
  if (regionPrompt_) return;
  settings_.region = regionNext(settings_.region);
  settingsDirty_ = true;  // debounced: no NVS write on every Enter, and TX is held until the region settles
  settingsDirtyMs_ = nowMs;
  retune(nowMs);
}

void App::stepPromptRegion(int dir, uint32_t nowMs) {
  if (!regionPrompt_) return;
  settings_.region = dir < 0 ? regionPrev(settings_.region) : regionNext(settings_.region);
  retune(nowMs);  // RX only: enqueue() refuses everything while the prompt is open
}

void App::confirmPromptRegion(uint32_t nowMs) {
  if (!regionPrompt_) return;
  regionPrompt_ = false;
  saveSettings(nowMs);  // a failed save is shown; the prompt returns on the next boot
  niPending_ = true;    // boot announcement, now on the confirmed region
  niWantResponse_ = true;
  changed();
}

void App::flushSettings(uint32_t nowMs) {
  if (!settingsDirty_) return;
  settingsDirty_ = false;
  saveSettings(nowMs);
}

bool App::setTxPower(int power, uint32_t nowMs) {
  if (!validTxPower(power)) return false;
  settings_.txPower = (int8_t)power;
  bool ok = radio_.setPower(effectivePower());
  saveSettings(nowMs);
  if (!ok) showToast("ERRO no radio", nowMs);
  return true;
}

bool App::setLongName(const char* name, uint32_t nowMs) {
  if (!validLongName(name)) return false;
  utf8Copy(settings_.longName, sizeof settings_.longName, name);
  utf8Copy(me_.longName, sizeof me_.longName, name);
  saveSettings(nowMs);
  requestNodeInfo(nowMs);
  return true;
}

bool App::setShortName(const char* name, uint32_t nowMs) {
  if (!validShortName(name)) return false;
  utf8Copy(settings_.shortName, sizeof settings_.shortName, name);
  utf8Copy(me_.shortName, sizeof me_.shortName, name);
  saveSettings(nowMs);
  requestNodeInfo(nowMs);
  return true;
}

bool App::setGreeting(const char* text, uint32_t nowMs) {
  Settings def = settings_;
  def.customGreeting = false;
  char composed[Settings::kMaxGreeting + 1];
  composeGreeting(def, composed, sizeof composed);
  if (!text || !text[0] || strcmp(text, composed) == 0) {
    settings_.customGreeting = false;
    memset(settings_.greeting, 0, sizeof settings_.greeting);
  } else {
    if (!validGreeting(text)) return false;
    settings_.customGreeting = true;
    utf8Copy(settings_.greeting, sizeof settings_.greeting, text);
  }
  saveSettings(nowMs);
  return true;
}

void App::requestNodeInfo(uint32_t nowMs) {
  niPending_ = true;
  niWantResponse_ = false;
  if (haveManualNi_ && (uint32_t)(nowMs - lastManualNiMs_) < kManualNodeInfoMs) {
    char t[40];
    unsigned left = (unsigned)((kManualNodeInfoMs - (nowMs - lastManualNiMs_) + 999) / 1000);
    snprintf(t, sizeof t, "NodeInfo em %us", left);
    showToast(t, nowMs);
  }
  changed();
}
