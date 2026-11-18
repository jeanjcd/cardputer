# Architecture

How the firmware is organised, how a frame flows through it, what it stores and how much memory it uses. This describes v0.1.0.

## Module map

```
src/
  main.cpp        setup(): M5Cardputer.begin → App::begin → Ui::begin
                  loop():  keyboard update → Ui::handleKeys → App::tick → Ui::render
  mesh/           portable protocol core: no Arduino/ESP-IDF headers, built and tested natively
  hw/             hardware: SX1262 radio, NVS storage, persistent identity
  app/            application logic (App) and user interface (Ui)
lib/crypto/       vendored rweather/Crypto subset (AES, CTR, SHA-256, Curve25519), MIT
test/             native Unity test suites
tools/            gen_vectors.py (reference vectors), sanitize_link.py (sanitizer link flags)
```

### `src/mesh/` (portable core)

| Module | Responsibility |
|---|---|
| [`header`](../../src/mesh/header.h) | 16-byte on-air header parse/write |
| [`pb`](../../src/mesh/pb.h) | Protobuf subset: `Data`, `User`, `Routing` |
| [`crypto`](../../src/mesh/crypto.h), [`aes_ccm`](../../src/mesh/aes_ccm.h) | AES-128-CTR, X25519 + SHA-256, AES-256-CCM; [`secure_zero.h`](../../src/mesh/secure_zero.h) wipes secrets |
| [`packet`](../../src/mesh/packet.h) | Builds outgoing frames (text, DM, NodeInfo, ACK/NAK), parses and decrypts incoming frames (`parseFrame`), packet IDs, `DedupCache` |
| [`region`](../../src/mesh/region.h) | Region table, frequency formula, power clamp, channel hash, LoRa time-on-air, `AirtimeBudget` |
| [`nodedb`](../../src/mesh/nodedb.h) | Known nodes (RAM) and persisted peers (NVS), first-key-wins |
| [`chat`](../../src/mesh/chat.h) | Message history in a shared pool |
| [`greeter`](../../src/mesh/greeter.h) | Auto-greet queue, interval and persisted greeted set |
| [`outbox`](../../src/mesh/outbox.h) | Outgoing DMs, including those waiting for a key |
| [`ratelimit`](../../src/mesh/ratelimit.h) | Per-node and global limiters for automatic replies |
| [`settings`](../../src/mesh/settings.h) | User settings, validation, blob encoding, greeting composition |
| [`text`](../../src/mesh/text.h) | Bounded UTF-8 helpers, display conversion to CP437 |
| [`hwmodel`](../../src/mesh/hwmodel.h) | Hardware model ID → name and chip family |
| [`kv`](../../src/mesh/kv.h) | Abstract key/value blob store (NVS on the device, a map in tests) |

### `src/hw/` (hardware)

| Module | Responsibility |
|---|---|
| [`radio`](../../src/hw/radio.h) | PI4IOE antenna switch, SX1262 via RadioLib, interrupt-driven RX, blocking TX, retune, power |
| [`nvs_kv`](../../src/hw/nvs_kv.h) | `Kv` on ESP32 NVS (Arduino `Preferences`, namespace `meshhello`), exact-length reads |
| [`identity`](../../src/hw/identity.h) | Load or create the X25519 key pair, node number from MAC, hardware entropy |

### `src/app/` (application)

| Module | Responsibility |
|---|---|
| [`app`](../../src/app/app.h) | Owns all state. RX handling, scheduling, TX queue, duty budget, settings actions, toasts. |
| [`ui`](../../src/app/ui.h) | Four tabs on the 240 × 135 display, keyboard handling, off-screen canvas |

The UI reads state only through `App`'s const accessors and acts through its action methods (`sendText`, `cycleRegion`, `toggleAutoGreet`, ...). `App` bumps a version counter on every visible change. The UI redraws when it changes (at most every 50 ms), and at least once per second for ages and toasts.

## Data flow

```
                 SX1262 (DIO1 IRQ)
                        |
                 Radio::poll  ----------------------------- rx count, last RSSI
                        |
                 headerParse  -- drop: from == us / from == broadcast
                        |
                 DedupCache.contains(from, id)?  -- yes: re-ACK if it is a DM we ACKed; drop
                        |
                 parseFrame (decrypt: PKI if channel 0x00 and to == us, CTR if channel 0x08)
                        |
        +---------------+------------------+---------------------+
   undecodable     decoded, other port   NodeInfo / Text / Ack
   (PKI w/o key:        |                       |
    NAK 35 +       dedup.record           dedup.record, NodeDb.touch
    NodeInfo)      NodeDb.touch                 |
                                 +--------------+--------------+
                             NodeInfo         Text             Ack/NAK
                          NodeDb.applyUser  ChatStore.add     ChatStore: v / x
                          (first key wins)  ACK if want_ack
                          Greeter.onNodeWithKey
                          Outbox.onKeyArrived
                          NodeInfo reply (rate-limited)

   UI actions / timers
        |
   App::schedule (only when the TX queue is empty), in priority order:
        1. pending NodeInfo broadcast (boot, 3 h, rename, manual; 30 s spacing)
        2. Outbox.next  -> buildDirectText (PKI)
        3. Greeter.due  -> greeting DM
   ACKs and NAKs are inserted at the front of the queue (urgent); NodeInfo replies,
   NodeInfo requests and public texts are appended directly when they arise.
        |
   TX queue (6 frames)
        |
   App::transmitOne: >= 1.5 s since the last TX end, AirtimeBudget.allows(...)
        |
   Radio::send (blocking) -> back to RX; AirtimeBudget.record; message state ~ -> . or x
```

Key points:

- **No heap in the RX path.** The decoded `Inbound` struct is a static buffer, and all buffers are fixed-size with explicit bounds.
- **Dedup after parse.** A pair is recorded only once the frame was handled, so a PKI DM that arrives before the sender's key can still be read when it is retransmitted.
- **One transmission at a time.** `schedule()` refills the queue only when it is empty. `transmitOne()` sends at most one frame per loop, at least **1.5 s** after the previous one ended, and only if the region's duty budget allows it. Otherwise the head frame (and everything behind it) waits, and the status bar shows `duty limit`.
- **Blocking TX.** `Radio::send` blocks until TX done (up to about 2.2 s for 255 bytes), so keys are not handled during that time.

## Persistence (NVS)

Everything is stored in the ESP32 NVS partition under namespace **`meshhello`**, as fixed-size blobs. A read succeeds only if the stored blob has exactly the expected length. Version bytes and field validation reject anything else, and the corresponding defaults are used.

| Key | Size | Layout | Written when |
|---|---|---|---|
| `idkey` | 65 B | `[ver=1][priv 32][pub 32]` | Once, on first boot (or if the record is missing/corrupt). Validated on load: `pub` must equal X25519(`priv`, 9). |
| `settings` | 235 B | `[ver=1][region id u8][txPower i8][flags u8: bit0 custom greeting][greeting 201][longName 25][shortName 5]`; strings NUL-padded UTF-8 | Power, name or greeting change; region change (debounced 2 s or on leaving Config) |
| `greeted` | 2003 B | `[ver=1][count u16 LE][500 × node number u32 LE]` | Each time a node is greeted (auto or manual) |
| `peer0` … `peer49` | 89 B each | `[ver=1][node number u32 LE][seq u32 LE][longName 40][shortName 8][public key 32]` | When we exchange a DM with a keyed node (once per peer per boot); the least recently written slot is reused when all 50 are taken |
| `region` | 1 B | Region id | **Read only**, as a fallback from an early diagnostic build when `settings` is absent |

Region ids (persisted): `0` ANZ, `1` BR_902, `2` US, `3` EU_868, `4` RU, `5` KR, `6` UA_868, `7` MY_919, `8` SG_923, `9` PH_868, `10` PH_915, `11` IN, `12` NZ_865, `13` NP_865, `14` KZ_863, `15` JP, `16` TW, `17` TH.

Chat history, the node list (other than saved peers), unread counters, auto-greet ON/OFF and the airtime budget are **RAM only**.

## RAM budget

Approximate static sizes on the ESP32-S3 (32-bit):

| Structure | Size | Notes |
|---|---|---|
| `ChatStore` | ~21 KB | 96 slots × 224 B (200-byte text + state + peer + sequence) |
| `NodeDb` | ~13 KB | 100 entries × 128 B + 50 persisted-slot records |
| UI line buffer | ~9 KB | 210 wrapped lines × 44 B for history rendering |
| `Outbox` | ~2.2 KB | 10 × 216 B |
| `Greeter` | ~2 KB | 500 greeted IDs + 10 queued |
| TX queue | ~1.6 KB | 6 × 264 B |
| `DedupCache` | 0.5 KB | 64 × (from, id) |
| Rate limiters | ~0.9 KB | 3 × 32 slots |
| Display canvas | ~63 KB (heap) | 240 × 135 × 16 bit, allocated once at start; if allocation fails the UI draws directly to the display |

The `greeted` blob (2 KB) is assembled on the stack while it is being loaded or saved.

## Limits

| Limit | Value | Source |
|---|---|---|
| Frame size | 255 B (16 header + 239 payload) | `Radio::kMaxFrame`, `packet.cpp` |
| Text per message | 199 bytes | `Settings::kMaxGreeting`, `ChatMsg::text[200]` |
| `Data.payload` accepted | 233 B | `MeshData::payload` |
| Long / short name | 24 / 4 bytes | `Settings` |
| Nodes in RAM | 100 | `NodeDb::kMaxNodes` |
| Persisted peers | 50 | `NodeDb::kMaxPeers` |
| Messages per conversation / total | 30 / 96 | `ChatStore` |
| Outbox (queued + waiting for key) | 10 | `Outbox::kMax` |
| TX queue | 6 frames | `App::kTxQueue` |
| Greeted set / greet queue | 500 / 10 | `Greeter` |
| Dedup cache | 64 pairs | `DedupCache` |
| Re-ACK memory | 8 DMs | `App::ackFrom_` |
| Gap between our frames | 1.5 s | `kTxGapMs` |
| Auto-greet interval | 30 s | `Greeter` |
| NodeInfo period | 3 h | `kNodeInfoPeriodMs` |
| Manual / rename NodeInfo spacing | 30 s | `kManualNodeInfoMs` |
| NodeInfo request to a keyless DM target | 1 per 60 s per node | `kNodeInfoRequestMs` |
| NodeInfo replies | 1 per 12 h per node, 1 per 30 s overall | `niReplies_` |
| NAK + NodeInfo for unknown-key PKI | 1 per 10 min per node, 1 per 30 s overall | `unknownKey_` |
| Settings save debounce (region) | 2 s | `App::tick` |
| Toast duration | 2.5 s | `kToastMs` |
| RX re-arm without activity | 10 min | `kRxRearmMs` |
| Greeter retry after NVS failure | 60 s | `Greeter::kRetryBackoffMs` |
