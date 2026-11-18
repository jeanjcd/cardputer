# M5-Mesh-Hello Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Cardputer-Adv + Cap LoRa-1262 firmware that joins the Meshtastic LongFast channel as "4stro-031", lists nearby nodes, chats via PKI DMs and the public channel, and optionally auto-greets each new node once.

**Architecture:** Pure, hardware-free core (`mesh/`: protobuf mini-codec, packet header, crypto, packet builder/parser, node DB, greeter, message store) tested **natively on the PC** with Unity (`pio test -e native`). Hardware edges (`hw/`: SX1262 via RadioLib + PI4IOE antenna switch, NVS store, M5 keyboard/display UI) are thin and verified by build + field test. Device-side `pio test` is NOT used: it overwrites M5Launcher and installed apps.

**Tech Stack:** PlatformIO (espressif32 8.0.0, Arduino core 3.3.x, board `m5stack-stamps3`), `m5stack/M5Cardputer`, `jgromes/RadioLib@^7`, `rweather/Crypto@^0.4.0` (AES-128/256, CTR, SHA256, Curve25519 — portable, also builds natively; Meshtastic uses it), Unity. Host compiler: WinLibs GCC (winget `BrechtSanders.WinLibs.POSIX.UCRT`). Python `cryptography` for reference vectors.

**Spec:** `firmwares/m5-mesh-hello/docs/design.md`

## Global Constraints

- Node: long name `4stro-031`, short name `4S31`, `hw_model = 112`, role `CLIENT_MUTE (1)`, node num = last 4 MAC bytes (`mac[2]<<24|mac[3]<<16|mac[4]<<8|mac[5]`).
- Radio: LongFast BW 250 kHz, SF 11, CR 5 (4/5), sync word `0x2B`, preamble 16, CRC on, TCXO 3.0 V, `setCurrentLimit(140)`, power default 22 dBm (range 2–22).
- Frequencies: ANZ **919.875 MHz** (default), BR_902 **903.875 MHz**; formula `freqStart + bw/2000 + (djb2(name) % floor((end-start)/(bw/1000))) * bw/1000`.
- Pins: NSS G5, DIO1 G4, RST G3, BUSY G6, SCK G40, MOSI G14, MISO G39; antenna switch = PI4IOE5V6408 @0x43 (M5 `In_I2C`) pin P0 HIGH.
- Default channel key `d4f1bb3a20290759f0bcffabcf4e6901`, channel hash `0x08`.
- Header 16 B LE `to,from,id,flags,channel,next_hop,relay_node`; flags hop_limit `&0x07`, want_ack `0x08`, hop_start `<<5`; broadcast `0xFFFFFFFF`; max payload 239 B.
- CTR nonce: `id (u64 LE) | from (u32 LE) | 00000000`. PKI: key = SHA256(X25519(priv, peerPub)); AES-256-CCM tag 8, nonce 13 = `id_low32 LE | extraNonce LE | from LE | 00`; wire = `cipher | tag8 | extraNonce4`; header channel = 0x00.
- Text DMs use PKI only; NODEINFO/ROUTING DMs and all broadcasts use the channel key.
- Hop limit 3 (hop_start = 3) on everything we originate; want_ack on DMs.
- Greeting default `"Saudações de 4stro-031! 👋 (Cardputer-Adv)"` sent as UTF-8 exactly as in the spec. On our own screen, render text with a UTF-8-capable M5GFX font for accents (e.g. `fonts::efontJA_12`/`lgfxJapanGothic_12` if flash allows, else replace non-ASCII with `?` for display only — never alter what is sent). Auto-greet OFF at boot, ≥30 000 ms between greetings, queue ≤10, greeted set persisted ≤500, never greet the same node twice.
- Our keypair generated once, persisted, never regenerated. First-key-wins for peers.
- Limits: node DB 100 in RAM (evict oldest), 50 peers persisted (name+key), 30 msgs/conversation, dedup 64 `(from,id)`.
- Screen 240×135 landscape at rotation 0 (never `setRotation(1)`).
- No Claude attribution in commits/PRs. Commits as `jeanjcd`.
- **Security (Jean's requirement, applies to every task):** every byte from the radio is untrusted — bounds-check everything, no dynamic allocation in the RX/parse path, never read/write past buffers, no `strcpy/sprintf` (use bounded variants), fixed-size buffers with explicit caps; secrets (private key, derived shared keys) are wiped with a non-elidable zeroize after use and never printed/logged/displayed (Config screen shows only the public key); constant-time tag comparison; key generation uses true hardware entropy (`bootloader_random_enable()` around `esp_fill_random()` since Wi-Fi/BT are off); dependencies pinned to exact versions; no network stack (Wi-Fi/BLE stay off) so LoRa is the only remote input.
- **Open-source ready:** code, identifiers and code comments in **English**; the folder `firmwares/m5-mesh-hello/` must be self-contained (builds and tests from inside it) so it can later move to its own public repo. `src/mesh/` must not include Arduino/M5 headers (portable protocol core). Public headers get a one-line doc comment per function.

## Review Focus

1. **Inbound garbage** (truncated packet <16 B, payload >239, bad protobuf, unknown portnum, CCM auth fail) must be dropped silently, never crash. Test `test_parse_rejects_garbage` (Task 4).
2. **Our own packets echoed back by relays** must not appear as new messages or nodes. Test `test_ignores_own_and_duplicate` (Task 4).
3. **NodeInfo with a different key for a known node** must not replace the stored key. Test `test_first_key_wins` (Task 5).
4. **Auto-greet with many nodes appearing at once** must respect queue ≤10 and 30 s spacing, and a node greeted before reboot is never greeted again. Tests `test_greeter_rate_and_queue`, `test_greeter_persisted` (Task 5).
5. **Sending a DM to a node without a known key** must not send a legacy DM; it sends a NodeInfo request and holds the message as pending, releasing it when the key arrives. Test `test_dm_waits_for_key` (Task 5).

---

### Task 1: Native test toolchain, project scaffold, reference vectors, radio math

**Files:**
- Create: `firmwares/m5-mesh-hello/platformio.ini`
- Create: `firmwares/m5-mesh-hello/tools/gen_vectors.py`
- Create: `firmwares/m5-mesh-hello/test/vectors.h` (generated, committed)
- Create: `firmwares/m5-mesh-hello/src/mesh/region.h`, `region.cpp`
- Create: `firmwares/m5-mesh-hello/src/main.cpp` (stub, guarded by `#ifndef PIO_UNIT_TESTING`)
- Test: `firmwares/m5-mesh-hello/test/test_region/test_main.cpp`

**Interfaces:**
- Produces: `uint32_t djb2(const char*)`; `enum class Region { ANZ, BR_902 }`; `float regionFreqMHz(Region, const char* channelName, float bwKHz)`; `uint8_t channelHash(const char* name, const uint8_t* key, size_t keyLen)`; `extern const uint8_t kDefaultKey[16]`.
- `platformio.ini`: `[env:cardputer]` (as M5-Keyboard-Macro, no ports pinned in the test path, libs M5Cardputer, RadioLib, Crypto, `build_src_filter = +<*>`), `[env:native]` (`platform = native`, `lib_deps = rweather/Crypto`, `build_src_filter = +<mesh/>`, `test_build_src = yes`, `build_flags = -std=gnu++17`).
- `gen_vectors.py` (Python `cryptography`, fixed seeds) writes C arrays for: CTR (key=default, id=0x12345678, from=0xA1B2C3D4, 40-byte plaintext); X25519 (two fixed private keys → pubs + shared); SHA256(shared); CCM (key=that hash, id, from, extraNonce=0xCAFEBABE, 30-byte plaintext → cipher+tag).

- [ ] **Step 1:** `winget install BrechtSanders.WinLibs.POSIX.UCRT`, add its `mingw64\bin` to PATH for the session; verify `g++ --version`. `python -m pip install cryptography`.
- [ ] **Step 2:** Write `gen_vectors.py`, run it → `test/vectors.h`.
- [ ] **Step 3: Failing tests** `test_region/test_main.cpp`: `test_djb2_longfast` (==130429955), `test_freq_anz` (919.875 ±0.001), `test_freq_br902` (903.875 ±0.001), `test_channel_hash_default` (==0x08).
- [ ] **Step 4:** `python -m platformio test -e native -f test_region` → FAIL (missing symbols).
- [ ] **Step 5:** Implement `region.*`.
- [ ] **Step 6:** Re-run → 4/4 PASS. Commit.

### Task 2: Protobuf mini-codec + on-air header

**Files:** Create `src/mesh/pb.h/.cpp`, `src/mesh/header.h/.cpp`; Test `test/test_codec/test_main.cpp`

**Interfaces:**
- Produces:
  ```cpp
  struct PacketHeader { uint32_t to, from, id; uint8_t hopLimit, hopStart; bool wantAck, viaMqtt; uint8_t channel, nextHop, relayNode; };
  bool headerParse(const uint8_t* buf, size_t len, PacketHeader& out);   // false if len < 16
  void headerWrite(const PacketHeader& h, uint8_t out[16]);
  struct MeshData { uint32_t portnum=0; uint8_t payload[233]; size_t payloadLen=0; bool wantResponse=false;
                    uint32_t dest=0, source=0, requestId=0, replyId=0; };
  struct MeshUser { char id[16]; char longName[40]; char shortName[8]; uint32_t hwModel=0; uint32_t role=0;
                    uint8_t publicKey[32]; size_t publicKeyLen=0; };
  size_t pbEncodeData(const MeshData&, uint8_t* out, size_t cap);   // 0 on overflow
  bool   pbDecodeData(const uint8_t*, size_t, MeshData&);           // skips unknown fields; false on malformed
  size_t pbEncodeUser(const MeshUser&, uint8_t* out, size_t cap);
  bool   pbDecodeUser(const uint8_t*, size_t, MeshUser&);
  size_t pbEncodeRoutingAck(uint8_t* out, size_t cap);              // Routing{error_reason=NONE} → may be empty-ish; encode field 3 = 0 explicitly
  bool   pbDecodeRoutingError(const uint8_t*, size_t, uint32_t& err);
  ```
  Field numbers/types exactly per design (Data dest/source/request_id/reply_id are `fixed32`; User strings; hw_model/role varint; public_key bytes).
- [ ] **Step 1: Failing tests:** `test_header_roundtrip` (flags byte for hopLimit=3, hopStart=3, wantAck=1 == 0x6B); `test_header_short_rejected` (len 15 → false); `test_data_roundtrip_text`; `test_data_known_bytes` (portnum=1,payload "hi" encodes to `08 01 12 02 68 69`); `test_data_skips_unknown_field` (append field 15 varint → still decodes); `test_data_malformed` (truncated length → false); `test_user_roundtrip_with_key`; `test_routing_ack_roundtrip`.
- [ ] **Step 2:** `pio test -e native -f test_codec` → FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** → all PASS. Commit.

### Task 3: Crypto (channel CTR + PKI)

**Files:** Create `src/mesh/crypto.h/.cpp`, `src/mesh/aes_ccm.h/.cpp`; Test `test/test_crypto/test_main.cpp`

**Interfaces:**
- Produces:
  ```cpp
  void ctrCrypt(const uint8_t key[16], uint32_t id, uint32_t from, uint8_t* buf, size_t len);  // in place, enc==dec
  void x25519Keypair(uint8_t priv[32], uint8_t pub[32]);        // uses RNG; priv clamped
  bool x25519Shared(const uint8_t priv[32], const uint8_t peerPub[32], uint8_t outKey[32]); // = SHA256(dh)
  // returns wire length (len+12) or 0
  size_t pkiEncrypt(const uint8_t key[32], uint32_t id, uint32_t from, uint32_t extraNonce,
                    const uint8_t* plain, size_t len, uint8_t* out);
  // returns plain length or -1 on auth failure / too short
  int pkiDecrypt(const uint8_t key[32], uint32_t id, uint32_t from, const uint8_t* wire, size_t wireLen, uint8_t* out);
  ```
  AES-CCM implemented on rweather `AES256` (CBC-MAC + CTR, L=2, M=8, no AAD), mirroring Meshtastic `aes-ccm.cpp`. Curve25519 via rweather `Curve25519::dh2`. RNG on device = `esp_random()`; native = `std::random_device` (only keypair needs it; tests use vector keys).
- [ ] **Step 1: Failing tests** against `vectors.h`: `test_ctr_vector`, `test_ctr_roundtrip`, `test_x25519_vector` (shared from both sides equal and == vector), `test_ccm_vector` (cipher+tag == vector, wire = cipher|tag|extraNonce LE), `test_ccm_rejects_tamper` (flip 1 bit → -1), `test_ccm_rejects_short` (wireLen < 12 → -1).
- [ ] **Step 2:** → FAIL. **Step 3:** Implement. **Step 4:** → all PASS. Commit.

### Task 4: Packet builder / parser (wire ⇄ events)

**Files:** Create `src/mesh/packet.h/.cpp`; Test `test/test_packet/test_main.cpp`

**Interfaces:**
- Consumes: Tasks 1–3.
- Produces:
  ```cpp
  struct Identity { uint32_t nodeNum; uint8_t priv[32], pub[32]; char longName[40], shortName[8]; };
  uint32_t nextPacketId(uint32_t rnd);   // (rnd<<10) | (counter++ & 0x3FF), never 0
  // Each returns frame length (header+payload) or 0.
  size_t buildBroadcastText(const Identity&, uint32_t id, const char* text, uint8_t* out);
  size_t buildNodeInfo(const Identity&, uint32_t id, uint32_t to, bool wantResponse, uint8_t* out);   // channel key
  size_t buildAck(const Identity&, uint32_t id, uint32_t to, uint32_t ackedId, uint8_t* out);           // ROUTING, channel key
  size_t buildDirectText(const Identity&, uint32_t id, uint32_t to, const uint8_t peerPub[32],
                         uint32_t extraNonce, const char* text, uint8_t* out);                         // PKI
  enum class InKind { None, Text, NodeInfo, Ack, Other };
  struct Inbound { InKind kind; PacketHeader h; bool direct; bool pki; MeshData data; MeshUser user; uint32_t ackedId; };
  // peerPubLookup returns pointer to stored key or nullptr
  bool parseFrame(const Identity&, const uint8_t* frame, size_t len,
                  const uint8_t* (*peerPubLookup)(uint32_t nodeNum), Inbound& out);
  class DedupCache { public: bool seen(uint32_t from, uint32_t id); };   // 64 entries, ring
  ```
  Parse rules: drop if `h.from == me` or `from == 0xFFFFFFFF`; channel byte 0x00 + to==me + key known → PKI; channel 0x08 → CTR with default key; anything else → `InKind::Other` only if diagnostics (still returns false for app logic). Text DMs arriving via channel key are rejected (mirror firmware).
- [ ] **Step 1: Failing tests** using two Identities A and B built from vector keys:
  `test_text_broadcast_roundtrip` (A builds, B parses: kind Text, !direct, text equal, hopLimit 3, channel 0x08);
  `test_nodeinfo_roundtrip` (user fields incl. hw_model 112, role 1, 32-byte key);
  `test_direct_text_roundtrip_pki` (A→B with B.pub; B parses with lookup→A.pub: kind Text, direct, pki, wantAck);
  `test_ack_roundtrip`;
  `test_parse_rejects_garbage` (len 10; len 300; valid header + random payload; PKI with wrong key) → false, no crash;
  `test_ignores_own_and_duplicate` (A parses own frame → false; DedupCache second call true);
  `test_legacy_text_dm_rejected` (text with to=B over channel key → false);
  `test_packet_id_nonzero_and_counter`.
- [ ] **Step 2:** → FAIL. **Step 3:** Implement. **Step 4:** → all PASS. Commit.

### Task 5: Node DB, conversations, greeter, outbox (pure logic + KV store interface)

**Files:** Create `src/mesh/kv.h` (interface), `src/mesh/nodedb.h/.cpp`, `src/mesh/chat.h/.cpp`, `src/mesh/greeter.h/.cpp`, `src/mesh/outbox.h/.cpp`; Test `test/test_logic/test_main.cpp`

**Interfaces:**
- Consumes: Task 4 types.
- Produces:
  ```cpp
  struct Kv { virtual bool get(const char* key, void* buf, size_t len)=0; virtual bool put(const char* key, const void* buf, size_t len)=0; virtual ~Kv()=default; };
  struct NodeEntry { uint32_t num; MeshUser user; bool hasKey; int16_t rssi; float snr; uint32_t lastHeardMs; uint8_t unread; bool greeted; };
  class NodeDb { public: explicit NodeDb(Kv&); void load();
    NodeEntry& touch(uint32_t num, int16_t rssi, float snr, uint32_t nowMs);   // creates/evicts oldest (cap 100)
    bool applyUser(uint32_t num, const MeshUser&);   // first-key-wins: false if key differs from stored
    const uint8_t* pubKey(uint32_t num) const;        // nullptr if none
    void persistPeer(uint32_t num);                   // name+key into Kv (cap 50)
    size_t count() const; const NodeEntry& at(size_t i) const;  // sorted by lastHeard desc
  };
  struct ChatMsg { bool mine; char text[200]; uint32_t id; enum { Pending, Sent, Acked, Failed, WaitingKey } state; };
  class ChatStore { public: void add(uint32_t peer /*0xFFFFFFFF = public*/, const ChatMsg&); // cap 30/peer
    void markAcked(uint32_t packetId); ChatMsg* findById(uint32_t id); /* + iteration accessors */ };
  class Greeter { public: Greeter(Kv&, uint32_t intervalMs=30000, size_t queueCap=10, size_t persistCap=500);
    void load(); void setEnabled(bool); bool enabled() const;
    void onNodeWithKey(uint32_t num);                 // enqueue if enabled && !greeted && !queued
    bool due(uint32_t nowMs, uint32_t& outNum);       // pops one if interval elapsed since last; marks greeted+persists
    bool wasGreeted(uint32_t num) const; };
  struct OutItem { uint32_t to; char text[200]; uint32_t chatId; };
  class Outbox { public: void queueDirect(const OutItem&, bool keyKnown);  // !keyKnown → held as WaitingKey, caller sends NodeInfo request
    void onKeyArrived(uint32_t num);                   // releases held items for num
    bool next(OutItem& out); };
  ```
- [ ] **Step 1: Failing tests** (with `FakeKv` map):
  `test_touch_and_evict_oldest` (101 nodes → 100, oldest gone);
  `test_first_key_wins`;
  `test_peer_persist_reload` (persist, new NodeDb on same Kv, load → key present);
  `test_chat_cap_and_ack`;
  `test_greeter_disabled_ignores`;
  `test_greeter_once_per_node` (same num twice → greeted once);
  `test_greeter_rate_and_queue` (15 nodes at t=0 → only 10 queued; due at t=0 yes, t=29999 no, t=30000 yes);
  `test_greeter_persisted` (greet, reload from Kv, onNodeWithKey same num → not queued);
  `test_dm_waits_for_key` (queueDirect keyKnown=false → next() false; onKeyArrived → next() returns it).
- [ ] **Step 2:** → FAIL. **Step 3:** Implement. **Step 4:** → all PASS (full native suite green). Commit.

### Task 6: Radio + NVS + identity on the device (diagnostic milestone)

**Files:** Create `src/hw/radio.h/.cpp`, `src/hw/nvs_kv.h/.cpp`, `src/hw/identity.h/.cpp`; Modify `src/main.cpp`

**Interfaces:**
- Consumes: `Kv`, `Identity`, region functions.
- Produces:
  ```cpp
  class Radio { public: bool begin(float freqMHz, int8_t powerDbm);   // PI4IOE P0 high, SX1262 init per Global Constraints, startReceive
    bool poll(uint8_t* buf, size_t& len, int16_t& rssi, float& snr);  // non-blocking, after DIO1 flag
    bool send(const uint8_t* buf, size_t len);                         // blocking TX then back to RX
    void setFreq(float); void setPower(int8_t); };
  class NvsKv : public Kv { /* Preferences, namespace "meshhello"; keys ≤15 chars */ };
  void loadOrCreateIdentity(Kv&, Identity&);  // keypair once (key "idkey"), nodeNum from esp_efuse MAC
  ```
- [ ] **Step 1:** Implement; `main.cpp` temporary diag screen: freq, our `!nodeid`, count of frames, last RSSI/SNR, last channel byte, last decoded sender long name.
- [ ] **Step 2:** `python -m platformio run -e cardputer` → SUCCESS; size of `firmware.bin` < 1.5 MB.
- [ ] **Step 3:** `python -m platformio test -e native` → full suite PASS (regression).
- [ ] **Step 4: Field check (Jean):** copy `.bin` to SD, install via Launcher. Expected: screen shows `919.875`, own ID; with a Meshtastic node in range, frame counter rises and names decode. If counter stays 0 for ~10 min, try BR_902 (key in diag) and report.

### Task 7: Application wiring + UI + delivery

**Files:** Create `src/app/app.h/.cpp`, `src/app/ui.h/.cpp`, `src/app/settings.h/.cpp`; Modify `src/main.cpp`; Create `firmwares/m5-mesh-hello/README.md`, `bin/M5-Mesh-Hello.bin`, `bin/M5-Mesh-Hello-merged.bin`; Modify `firmwares/README.md`, root `README.md`

**Interfaces:**
- Consumes: everything above.
- Behavior (`App::loop(nowMs)`): poll radio → dedup → parseFrame → NodeDb.touch/applyUser (→ Greeter.onNodeWithKey, Outbox.onKeyArrived) → ChatStore; send ACK for PKI DMs to us with wantAck; if a NodeInfo had wantResponse and was to us or broadcast, reply with our NodeInfo directly (at most once per 12 h per node); boot: broadcast NodeInfo wantResponse=true; every 3 h broadcast NodeInfo; Greeter.due → buildDirectText(greeting) (if key) ; Outbox.next → send; one TX at a time.
- Settings (Kv): region, power, greeting text, **long name (≤24 bytes UTF-8, default "4stro-031") and short name (≤4 chars, default "4S31")** — editable on the Config screen with the keyboard; on change, update Identity and immediately broadcast our NodeInfo (key and node num never change).
- UI per design: tabs **Nos / Conversa / Canal / Config**; Tab cycles, `;` `.` move, Enter open/send, Esc (`` ` ``) back, `A` toggles auto-greet on Nos; status bar `ANZ 919.875 | AUTO:ON | rx 42 | -97dBm`; node row ≤40 chars at size 1: `4S31 4stro-031  CARDPUTER_ADV  -97 2m ✓●` (use ASCII markers `*`=unread, `+`=greeted). Chat input pre-filled with greeting.
- `hwModelName(uint32_t)` table for common models incl. 112, 43, 9, 7, 4, 110, 71; chip family string for those (ESP32-S3 / nRF52 / ESP32).
- [ ] **Step 1:** Implement app/ui/settings.
- [ ] **Step 2:** Build → SUCCESS; native suite PASS.
- [ ] **Step 3:** Produce `bin/` (app + merged), write README (uso, região, antena, limitações, compilar), update indexes.
- [ ] **Step 4: Field test (Jean):** install via Launcher; see nodes list; DM a node using the official app on the other side (or ask a community member) → message arrives, reply shows in chat, ✓ on ACK; auto-greet ON greets a new node once; reboot → not greeted again.
- [ ] **Step 5:** Commit, push branch, PR, merge (authorized).

### Task 8: Open-source packaging + CI

**Files:** Create `firmwares/m5-mesh-hello/LICENSE` (**GPL-3.0**, decided by Jean), `CONTRIBUTING.md`, `CHANGELOG.md` (0.1.0), bilingual `README.md` (EN first, then PT-BR; short: what it is, screenshot/ASCII of screens, quick start, links into docs/), and a **structured documentation set** in `docs/` (English, with a `docs/README.md` index; PT-BR summary at the top of the index):
- `docs/user-guide.md` — install via M5Launcher, first boot, screens and keys, auto-greet rules, settings (region/power/greeting), troubleshooting (no nodes heard, which region, antenna).
- `docs/hardware.md` — Cardputer-Adv + Cap LoRa-1262 pin map, PI4IOE antenna switch, TCXO, shared SPI/I²C, antenna and range advice (868 vs 915 MHz antenna analysis from design.md).
- `docs/protocol.md` — wire format, frequency computation, channel CTR, PKI (X25519/SHA-256/AES-CCM, nonce layouts), protobuf subset, ACKs, first-key-wins — a reference for implementers, with source links to Meshtastic code/docs.
- `docs/architecture.md` — module map (`src/mesh`, `src/hw`, `src/app`), data flow RX→parse→NodeDb/Chat/Greeter→Outbox→TX, persistence keys (NVS), limits.
- `docs/development.md` — toolchain (PlatformIO, WinLibs/GCC for native tests), build, native tests, regenerating vectors, CI, release `.bin` steps, coding conventions.
- `docs/security.md` — threat model (remote attacker over LoRa, malicious node, physical access, supply chain), what is protected and what is NOT (private key in NVS is readable with physical access because flash encryption is off under M5Launcher; public channel is readable by anyone by design; no signatures in v1), mitigations, how to verify a release (`SHA256SUMS`).
- `SECURITY.md` (folder root) — supported versions and how to report a vulnerability privately (GitHub private vulnerability reporting).
- `docs/roadmap.md` — v2 ideas (GPS/position, more channels, signatures, relay), known limitations.
- Keep `docs/design.md` and `docs/plano.md` as historical design records (linked from the index). Create `.github/workflows/m5-mesh-hello.yml` at repo root (on push/PR touching the folder, `permissions: contents: read`, actions pinned to full commit SHAs): setup Python, `pip install platformio==<pinned>`, `pio test -e native`, **`pio test -e native_sanitize`** (new env on Linux CI: `-fsanitize=address,undefined -fno-sanitize-recover=all -g`), `pio run -e cardputer`, generate `SHA256SUMS` for the `.bin` files, upload artifacts. Add `.github/dependabot.yml` (github-actions ecosystem, weekly). Add a **fuzz-style test** `test/test_fuzz/` (native): 200k iterations of seeded random and mutated valid frames through `parseFrame`/`pbDecode*`/`headerParse` asserting no crash and no out-of-bounds (run under the sanitizer env in CI). Add a CodeQL workflow for C/C++ (build-mode manual using the native env).

- [ ] **Step 1:** Write files; workflow uses `working-directory: firmwares/m5-mesh-hello`.
- [ ] **Step 2:** Validate locally: `python -m platformio test -e native` PASS and `python -m platformio run -e cardputer` SUCCESS from inside the folder.
- [ ] **Step 3:** Push; check the Actions run with `gh run watch` → success.
- [ ] **Step 4:** Commit, PR, merge.
