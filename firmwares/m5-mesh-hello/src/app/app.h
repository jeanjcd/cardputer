// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "../hw/identity.h"
#include "../hw/nvs_kv.h"
#include "../hw/radio.h"
#include "../mesh/chat.h"
#include "../mesh/greeter.h"
#include "../mesh/nodedb.h"
#include "../mesh/outbox.h"
#include "../mesh/packet.h"
#include "../mesh/ratelimit.h"
#include "../mesh/settings.h"

/// The mesh application: owns radio, identity, settings and all protocol state; driven by tick() from loop().
/// The UI reads state through the const accessors and acts through the action methods.
class App {
 public:
  static constexpr uint32_t kPublic = ChatStore::kPublic;  // chat "peer" of the public channel
  static constexpr uint32_t kNone = 0;                     // nothing being viewed

  /// Brings up NVS, identity (first, it uses the hardware RNG), settings, packet-id seed, peers and the radio.
  void begin();
  /// One loop iteration: receive and handle frames, schedule NodeInfo/greetings/DMs and transmit at most one frame.
  void tick(uint32_t nowMs);

  // ---- state for the UI ----
  /// Our identity (public part used by the UI; never display priv).
  const Identity& me() const { return me_; }
  /// Current settings.
  const Settings& settings() const { return settings_; }
  /// Known nodes.
  const NodeDb& nodes() const { return db_; }
  /// Chat history (DMs by peer, public channel under kPublic).
  const ChatStore& chat() const { return chat_; }
  /// True if auto-greet is on (always off at boot).
  bool autoGreet() const { return greeter_.enabled(); }
  /// True if the node is in the persisted greeted set.
  bool greeted(uint32_t num) const { return greeter_.wasGreeted(num); }
  /// Frames received since boot.
  uint32_t rxCount() const { return rxCount_; }
  /// True once a frame has been received; lastRssi() is then valid.
  bool haveRssi() const { return haveRssi_; }
  /// RSSI of the last received frame (dBm).
  int16_t lastRssi() const { return lastRssi_; }
  /// Current centre frequency in MHz.
  float freqMHz() const;
  /// TX power actually used: min(setting, region limit, 22).
  int8_t effectivePower() const { return effectiveTxPower(settings_.region, settings_.txPower); }
  /// True while a frame is held back by the region's duty-cycle budget.
  bool dutyLimited() const { return dutyLimited_; }
  /// Airtime used in the last hour (ms) and the region budget (ms; 0 = unrestricted).
  uint32_t airtimeUsedMs() const;
  uint32_t airtimeBudgetMs() const;
  /// Radio and Cap status.
  bool radioReady() const { return radio_.ready(); }
  bool capFound() const { return radio_.capFound(); }
  /// Identity load outcome (CreatedNotSaved means the key will change on reboot).
  IdentityStatus idStatus() const { return idStatus_; }
  /// Unread public-channel messages.
  uint8_t publicUnread() const { return publicUnread_; }
  /// Bumped on every visible state change; the UI redraws when it changes.
  uint32_t version() const { return version_; }
  /// Current toast text, or nullptr when none is active.
  const char* toast(uint32_t nowMs) const;
  /// Writes the greeting that would be sent now.
  void greeting(char* out, size_t cap) const { composeGreeting(settings_, out, cap); }

  // ---- actions from the UI ----
  /// Declares which chat is on screen (a node, kPublic or kNone) and clears its unread counter.
  void setViewing(uint32_t peer);
  /// Toggles auto-greet.
  void toggleAutoGreet();
  /// Sends text (≤200 B) as a DM to `peer`, or to the public channel for kPublic; false (with a toast) if rejected.
  bool sendText(uint32_t peer, const char* text, uint32_t nowMs);
  /// Switches to the next supported region, retunes and re-applies the power limit; persisted after 2 s or on flush.
  void cycleRegion(uint32_t nowMs);
  /// Persists a pending (debounced) settings change now, e.g. when leaving the Config tab.
  void flushSettings(uint32_t nowMs);
  /// Sets TX power (2..22 dBm); false if out of range.
  bool setTxPower(int power, uint32_t nowMs);
  /// Sets our long name (1..24 B UTF-8) and announces it; false if invalid.
  bool setLongName(const char* name, uint32_t nowMs);
  /// Sets our short name (1..4 B) and announces it; false if invalid.
  bool setShortName(const char* name, uint32_t nowMs);
  /// Sets the greeting text; an empty text or the composed default reverts to the default. False if invalid.
  bool setGreeting(const char* text, uint32_t nowMs);
  /// Broadcasts our NodeInfo, at most once every 30 s (deferred otherwise).
  void requestNodeInfo(uint32_t nowMs);
  /// Shows a short message on the status bar for 2.5 s.
  void showToast(const char* text, uint32_t nowMs);

 private:
  struct TxItem {
    uint8_t frame[Radio::kMaxFrame + 1];
    uint16_t len;
    uint32_t msgId;  // chat message to update after TX (0 = none)
  };
  static constexpr size_t kTxQueue = 6;

  static const uint8_t* lookupKey(uint32_t num);
  uint32_t newPacketId();
  uint32_t random32();
  void changed() { version_++; }
  bool saveSettings(uint32_t nowMs);

  void handleFrame(const uint8_t* buf, size_t len, int16_t rssi, float snr, uint32_t nowMs);
  void onNodeInfo(const Inbound& in, uint32_t nowMs);
  void onText(const Inbound& in);
  void onAck(const Inbound& in);
  void onUnknownKeyPki(const PacketHeader& h, uint32_t nowMs);
  void keyArrived(uint32_t num);
  void persistPeerOnce(uint32_t num);

  bool enqueue(const uint8_t* frame, size_t len, uint32_t msgId, bool urgent);
  void sendAck(uint32_t to, uint32_t ackedId);
  void sendNodeInfoTo(uint32_t to, bool wantResponse);
  void schedule(uint32_t nowMs);
  void transmitOne(uint32_t nowMs);
  void setMsgState(uint32_t msgId, ChatMsg::State from1, ChatMsg::State from2, ChatMsg::State to);

  NvsKv kv_;
  Radio radio_;
  Identity me_{};
  IdentityStatus idStatus_ = IdentityStatus::Loaded;
  Settings settings_{};
  NodeDb db_{kv_};
  ChatStore chat_;
  Greeter greeter_{kv_};
  Outbox outbox_;
  DedupCache dedup_;
  ReplyLimiter niReplies_{12UL * 3600 * 1000, 30000};  // NodeInfo replies: 12 h per node, 30 s globally
  ReplyLimiter unknownKey_{10UL * 60 * 1000, 30000};   // NAK + NodeInfo to PKI senders we lack a key for
  NodeRateLimiter niRequests_;  // direct NodeInfo requests when DMing a node without key
  AirtimeBudget airtime_;
  bool dutyLimited_ = false;
  uint32_t ackFrom_[8] = {}, ackId_[8] = {};  // DMs we ACKed, to re-ACK their retransmissions
  uint8_t ackNext_ = 0;

  TxItem txq_[kTxQueue];
  size_t txHead_ = 0, txCount_ = 0;
  bool haveTx_ = false;
  uint32_t lastTxEndMs_ = 0;

  uint64_t rng_ = 0;  // xorshift64* state, seeded from hardware entropy at boot

  bool niPending_ = false, niWantResponse_ = false;
  bool haveNi_ = false, haveManualNi_ = false;
  uint32_t lastNiMs_ = 0, lastManualNiMs_ = 0;

  uint32_t persisted_[NodeDb::kMaxPeers] = {};  // peers already persisted this boot (ring when full)
  size_t persistedCount_ = 0, persistedNext_ = 0;
  bool settingsDirty_ = false;  // debounced save after region changes
  uint32_t settingsDirtyMs_ = 0;

  uint32_t viewing_ = kNone;
  uint8_t publicUnread_ = 0;
  uint32_t rxCount_ = 0;
  bool haveRssi_ = false;
  int16_t lastRssi_ = 0;
  uint32_t version_ = 0;
  char toast_[48] = "";
  uint32_t toastUntilMs_ = 0;
};
