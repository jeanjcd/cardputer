> 英語版からの翻訳です。内容に相違がある場合は[英語版](../en/architecture.md)が優先されます。

# アーキテクチャ

ファームウェアの構成、フレームの流れ、保存するもの、メモリ使用量について説明します。対象は v0.1.0 です。

## モジュール構成

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

（`mesh/` は移植性のあるプロトコルコアで、Arduino/ESP-IDF のヘッダーを使わず、ネイティブでビルド・テストされます。`hw/` はハードウェア（SX1262 無線、NVS ストレージ、永続的な識別情報）、`app/` はアプリケーションロジック（App）とユーザーインターフェース（Ui）です。`lib/crypto/` は同梱した rweather/Crypto のサブセット（MIT）、`test/` はネイティブの Unity テストスイート、`tools/` は補助スクリプトです。）

### `src/mesh/` (移植性のあるコア)

| モジュール | 役割 |
|---|---|
| [`header`](../../src/mesh/header.h) | 16 バイトの無線上のヘッダーの解析／書き込み |
| [`pb`](../../src/mesh/pb.h) | Protobuf サブセット: `Data`、`User`、`Routing` |
| [`crypto`](../../src/mesh/crypto.h)、[`aes_ccm`](../../src/mesh/aes_ccm.h) | AES-128-CTR、X25519 + SHA-256、AES-256-CCM。[`secure_zero.h`](../../src/mesh/secure_zero.h) が秘密情報を消去 |
| [`packet`](../../src/mesh/packet.h) | 送信フレーム（テキスト、DM、NodeInfo、ACK/NAK）の組み立て、受信フレームの解析と復号（`parseFrame`）、パケット ID、`DedupCache` |
| [`region`](../../src/mesh/region.h) | リージョン表、周波数の計算式、出力の制限、チャンネルハッシュ、LoRa の送信時間、`AirtimeBudget` |
| [`nodedb`](../../src/mesh/nodedb.h) | 既知のノード（RAM）と永続化された相手（NVS）、最初の鍵が優先 |
| [`chat`](../../src/mesh/chat.h) | 共有プール内のメッセージ履歴 |
| [`greeter`](../../src/mesh/greeter.h) | 自動あいさつのキュー、間隔、永続化されたあいさつ済みの集合 |
| [`outbox`](../../src/mesh/outbox.h) | 送信する DM（鍵待ちのものを含む） |
| [`ratelimit`](../../src/mesh/ratelimit.h) | 自動応答のためのノードごと・全体のレート制限 |
| [`settings`](../../src/mesh/settings.h) | ユーザー設定、検証、blob のエンコード、あいさつ文の生成 |
| [`text`](../../src/mesh/text.h) | 範囲チェック付きの UTF-8 ヘルパー、CP437 への表示用変換 |
| [`hwmodel`](../../src/mesh/hwmodel.h) | ハードウェアモデル ID → 名前とチップファミリー |
| [`kv`](../../src/mesh/kv.h) | 抽象的なキー／値の blob ストア（デバイスでは NVS、テストではマップ） |

### `src/hw/` (ハードウェア)

| モジュール | 役割 |
|---|---|
| [`radio`](../../src/hw/radio.h) | PI4IOE アンテナスイッチ、RadioLib による SX1262、割り込み駆動の受信、ブロッキング送信、周波数の切り替え、出力 |
| [`nvs_kv`](../../src/hw/nvs_kv.h) | ESP32 NVS 上の `Kv`（Arduino の `Preferences`、名前空間 `meshhello`）、長さが完全に一致する場合のみ読み込み |
| [`identity`](../../src/hw/identity.h) | X25519 鍵ペアの読み込みまたは作成、MAC からのノード番号、ハードウェアエントロピー |

### `src/app/` (アプリケーション)

| モジュール | 役割 |
|---|---|
| [`app`](../../src/app/app.h) | すべての状態を保持。受信処理、スケジューリング、送信キュー、デューティ予算、設定の操作、トースト。 |
| [`ui`](../../src/app/ui.h) | 240 × 135 ディスプレイ上の 4 つのタブ、キーボード処理、オフスクリーンキャンバス |

UI は `App` の const アクセサを通じてのみ状態を読み、アクションメソッド（`sendText`、`cycleRegion`、`toggleAutoGreet` など）を通じて操作します。`App` は表示に関わる変更があるたびにバージョンカウンタを増やします。UI はそれが変わったときに再描画し（最大で 50 ms ごと）、経過時間やトーストのために最低でも 1 秒に 1 回は再描画します。

## データの流れ

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

（図の要点: 受信したフレームはヘッダー解析、重複チェック、復号の順に処理され、NodeInfo・テキスト・ACK/NAK に応じて振り分けられます。送信側では、送信キューが空のときだけ `App::schedule` が優先順位（1. 保留中の NodeInfo ブロードキャスト、2. 送信箱の DM、3. 自動あいさつ）に従ってキューを補充します。ACK と NAK はキューの先頭に、NodeInfo の応答・要求と公開テキストは発生時に直接末尾に追加されます。）

要点:

- **受信経路でヒープを使いません。** デコードした `Inbound` 構造体は静的バッファで、すべてのバッファは固定サイズで明示的な範囲チェックがあります。
- **重複の記録は解析の後。** フレームが処理された後にのみ組が記録されるので、送信者の鍵より先に届いた PKI の DM も、再送されたときには読めます。
- **一度に 1 つの送信。** `schedule()` はキューが空のときだけ補充します。`transmitOne()` はループごとに最大 1 フレームを、前のフレームが終わってから最低 **1.5 秒**後に、かつリージョンのデューティ予算が許す場合にのみ送信します。そうでなければ先頭のフレーム（とその後ろのすべて）は待機し、ステータスバーに `duty limit` が表示されます。
- **ブロッキング送信。** `Radio::send` は送信完了までブロックするので（255 バイトで最大約 2.2 秒）、その間キーは処理されません。

## 永続化 (NVS)

すべては ESP32 の NVS パーティションに、名前空間 **`meshhello`** で固定サイズの blob として保存されます。読み込みが成功するのは、保存された blob の長さが期待どおりの場合だけです。それ以外はバージョンバイトとフィールドの検証で拒否され、対応するデフォルト値が使われます。

| キー | サイズ | レイアウト | 書き込むタイミング |
|---|---|---|---|
| `idkey` | 65 B | `[ver=1][priv 32][pub 32]` | 初回起動時に一度だけ（またはレコードが欠けている／壊れている場合）。読み込み時に検証: `pub` は X25519(`priv`, 9) と一致しなければならない。 |
| `settings` | 235 B | `[ver=1][region id u8][txPower i8][flags u8: bit0 custom greeting][greeting 201][longName 25][shortName 5]`。文字列は NUL 埋めの UTF-8 | 出力、名前、あいさつ文の変更時。リージョンの変更時（2 秒のデバウンス後、または Config を離れたとき） |
| `greeted` | 2003 B | `[ver=1][count u16 LE][500 × node number u32 LE]` | ノードにあいさつするたび（自動または手動） |
| `peer0` … `peer49` | 各 89 B | `[ver=1][node number u32 LE][seq u32 LE][longName 40][shortName 8][public key 32]` | 鍵のあるノードと DM をやり取りしたとき（起動ごとに相手 1 人につき 1 回）。50 枠がすべて埋まっている場合は、最も古く書き込まれた枠を再利用 |
| `region` | 1 B | リージョン ID | **読み込みのみ**。`settings` がない場合の、初期の診断ビルドからのフォールバック |

リージョン ID（永続化される値）: `0` ANZ、`1` BR_902、`2` US、`3` EU_868、`4` RU、`5` KR、`6` UA_868、`7` MY_919、`8` SG_923、`9` PH_868、`10` PH_915、`11` IN、`12` NZ_865、`13` NP_865、`14` KZ_863、`15` JP、`16` TW、`17` TH。

チャット履歴、ノード一覧（保存された相手以外）、未読カウンタ、自動あいさつのオン／オフ、送信時間の予算は **RAM のみ**です。

## RAM の使用量

ESP32-S3（32 ビット）でのおおよその静的サイズ:

| 構造体 | サイズ | 備考 |
|---|---|---|
| `ChatStore` | ~21 KB | 96 スロット × 224 B（200 バイトのテキスト + 状態 + 相手 + シーケンス） |
| `NodeDb` | ~15 KB | 100 エントリ × 128 B + 50 個の永続化スロットのレコード（永続化された鍵は RAM にも固定: 50 × 32 B） |
| UI の行バッファ | ~9 KB | 履歴描画用に 210 行（折り返し後） × 44 B |
| `Outbox` | ~2.2 KB | 10 × 216 B |
| `Greeter` | ~2 KB | あいさつ済み ID 500 個 + キュー 10 個 |
| 送信キュー | ~1.6 KB | 6 × 264 B |
| `DedupCache` | 0.5 KB | 64 × (from, id) |
| レート制限 | ~0.9 KB | 3 × 32 スロット |
| ディスプレイのキャンバス | ~63 KB（ヒープ） | 240 × 135 × 16 ビット、起動時に一度だけ確保。確保に失敗した場合、UI はディスプレイに直接描画する |

`greeted` の blob（2 KB）は、読み込みや保存の間スタック上で組み立てられます。

## 制限値

| 制限 | 値 | ソース |
|---|---|---|
| フレームサイズ | 255 B（ヘッダー 16 + ペイロード 239） | `Radio::kMaxFrame`、`packet.cpp` |
| メッセージあたりのテキスト | 199 バイト | `Settings::kMaxGreeting`、`ChatMsg::text[200]` |
| 受け付ける `Data.payload` | 233 B | `MeshData::payload` |
| ロングネーム / ショートネーム | 24 / 4 バイト | `Settings` |
| RAM 上のノード | 100 | `NodeDb::kMaxNodes` |
| 永続化される相手 | 50 | `NodeDb::kMaxPeers` |
| 会話ごと / 合計のメッセージ | 30 / 96 | `ChatStore` |
| 送信箱（キュー内 + 鍵待ち） | 10 | `Outbox::kMax` |
| 送信キュー | 6 フレーム | `App::kTxQueue` |
| あいさつ済みの集合 / あいさつのキュー | 500 / 10 | `Greeter` |
| 重複キャッシュ | 64 組 | `DedupCache` |
| 再 ACK の記憶 | 8 件の DM、それぞれ最大 2 回の再 ACK | `AckMemory` |
| 自分のフレーム間の間隔 | 1.5 秒 | `kTxGapMs` |
| 自動あいさつの間隔 | 30 秒 | `Greeter` |
| NodeInfo の周期 | 3 時間 | `kNodeInfoPeriodMs` |
| 手動 / 名前変更時の NodeInfo の間隔 | 30 秒 | `kManualNodeInfoMs` |
| 鍵のない DM 宛先への NodeInfo 要求 | 1 ノードにつき 60 秒に 1 回 | `kNodeInfoRequestMs` |
| NodeInfo の応答 | 1 ノードにつき 12 時間に 1 回、全体で 30 秒に 1 回 | `niReplies_` |
| 鍵不明の PKI に対する NAK + NodeInfo | 1 ノードにつき 10 分に 1 回、全体で 30 秒に 1 回 | `unknownKey_` |
| 設定保存のデバウンス（リージョン） | 2 秒 | `App::tick` |
| トーストの表示時間 | 2.5 秒 | `kToastMs` |
| 無活動時の受信の再設定 | 10 分 | `kRxRearmMs` |
| NVS 失敗後の Greeter の再試行 | 60 秒 | `Greeter::kRetryBackoffMs` |
