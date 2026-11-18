> 英語版からの翻訳です。内容に相違がある場合は[英語版](../en/protocol.md)が優先されます。

# プロトコルリファレンス

このページでは、M5-Mesh-Hello v0.1.0 が無線で何を送信し、何を受け付けるかを正確に説明します。実装者向けのリファレンスです。ここに書かれていることはすべて [`src/mesh/`](../../src/mesh/) のコードから導いたもので、[`tools/gen_vectors.py`](../../tools/gen_vectors.py)（Python の `cryptography`）で生成した参照ベクタで検証しています。

このファームウェアは、[Meshtastic](https://meshtastic.org) プロトコルのうち、相互運用可能な小さなサブセットを実装しています。参考にしている上流のソース:

- 無線、リージョン、周波数: [`src/mesh/RadioInterface.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/RadioInterface.cpp)、[`RadioInterface.h`](https://github.com/meshtastic/firmware/blob/master/src/mesh/RadioInterface.h)
- 暗号化: [`src/mesh/CryptoEngine.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/CryptoEngine.cpp)、[`src/mesh/aes-ccm.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/aes-ccm.cpp)
- ルーティング、ACK、PKI の判断: [`src/mesh/Router.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/Router.cpp)、[`src/mesh/ReliableRouter.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/ReliableRouter.cpp)
- チャンネルとチャンネルハッシュ: [`src/mesh/Channels.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/Channels.cpp)
- ノードデータベースと鍵の扱い: [`src/mesh/NodeDB.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/NodeDB.cpp)
- Protobuf: [`meshtastic/mesh.proto`](https://github.com/meshtastic/protobufs/blob/master/meshtastic/mesh.proto)、[`meshtastic/portnums.proto`](https://github.com/meshtastic/protobufs/blob/master/meshtastic/portnums.proto)

## 物理層

LoRa、Meshtastic **LongFast**: BW 250 kHz、SF 11、CR 4/5、同期ワード `0x2B`、16 シンボルのプリアンブル、明示ヘッダー、ペイロード CRC オン。中心周波数はリージョンから計算されます。[frequencies.md](frequencies.md#周波数の計算方法) を参照してください。LoRa フレーム 1 つは最大 255 バイトで、16 バイトのヘッダーと最大 239 バイトの暗号化ペイロードからなります。

## 無線上のヘッダー (16 バイト、リトルエンディアン)

| オフセット | サイズ | フィールド | 備考 |
|---|---|---|---|
| 0 | 4 | `to` | 宛先のノード番号。`0xFFFFFFFF` = ブロードキャスト |
| 4 | 4 | `from` | 送信者のノード番号 |
| 8 | 4 | `id` | パケット ID |
| 12 | 1 | `flags` | 下記参照 |
| 13 | 1 | `channel` | チャンネルハッシュ（デフォルトチャンネルは `0x08`）、PKI の場合は `0x00` |
| 14 | 1 | `next_hop` | 0 を送信 |
| 15 | 1 | `relay_node` | 自分のノード番号の下位バイトを送信 |

`flags`:

| ビット | マスク | フィールド | 送信する値 |
|---|---|---|---|
| 0–2 | `0x07` | `hop_limit` | 3 |
| 3 | `0x08` | `want_ack` | PKI のダイレクトテキストは 1、それ以外は 0 |
| 4 | `0x10` | `via_mqtt` | 0 |
| 5–7 | `0xE0`（`>> 5`） | `hop_start` | 3 |

コード: [`header.cpp`](../../src/mesh/header.cpp)、[`packet.cpp`](../../src/mesh/packet.cpp) の `fillHeader`。

## チャンネル暗号化 (AES-128-CTR)

PKI のダイレクトメッセージを除くすべてのフレームは、デフォルトのチャンネル鍵で暗号化されます。

- **鍵**（AES-128。PSK の省略表記 `AQ==`、インデックス 1 を展開したもの）:
  `d4 f1 bb 3a 20 29 07 59 f0 bc ff ab cf 4e 69 01`
- **チャンネル名**: `LongFast`
- **チャンネルハッシュ**（ヘッダーのバイト 13）: 名前の全バイトの XOR と、鍵の全バイトの XOR を XOR したもの = **`0x08`**
- **カウンタブロック**（16 バイト）:

  | バイト | 内容 |
  |---|---|
  | 0–7 | パケット `id`（u64 リトルエンディアン。バイト 4–7 は 0） |
  | 8–11 | `from`（u32 リトルエンディアン） |
  | 12–15 | ブロックカウンタ（32 ビット**ビッグエンディアン**、0 から開始） |

- 平文はエンコードされた `Data` protobuf です。暗号化と復号は同じ XOR 演算です。

コード: [`crypto.cpp`](../../src/mesh/crypto.cpp) の `ctrCrypt`、[`region.cpp`](../../src/mesh/region.cpp) の `kDefaultKey` / `channelHash`。

## PKI ダイレクトメッセージ (X25519 + SHA-256 + AES-256-CCM)

現在の Meshtastic ファームウェアは、テキストの DM を公開鍵暗号（PKI）でのみ送信します。M5-Mesh-Hello も同様です。

1. **鍵ペア**: X25519。秘密鍵はハードウェア RNG からの 32 ランダムバイトで、RFC 7748 に従ってクランプされます（`k[0] &= 248; k[31] &= 127; k[31] |= 64`）。公開鍵 = X25519(priv, 9)。一度だけ生成され、NVS に保存されます。
2. **共有鍵**: `key = SHA-256(X25519(our_priv, peer_pub))`、32 バイト。共有秘密がすべて 0 になるなど無効になる相手の鍵（弱い点）は拒否されます。
3. **暗号方式**: AES-256-CCM（RFC 3610）、**タグ長 M = 8**、長さフィールド L = 2、**13 バイトのノンス**、関連データなし。
4. **ノンス**（13 バイト）:

   | バイト | 内容 |
   |---|---|
   | 0–3 | パケット `id`（u32 リトルエンディアン） |
   | 4–7 | `extraNonce`（u32 リトルエンディアン） |
   | 8–11 | `from`（u32 リトルエンディアン） |
   | 12 | `0x00` |

   奇妙な配置に見えますが、上流がこうしています。Meshtastic は 16 バイトのバッファを `id (u64 LE) | from (u32 LE) | 0` として組み立て、そのバイト 4–7（u64 の id の上位半分で、常に 0）に `extraNonce` を上書きします。CCM は先頭の 13 バイトを使います。実装はこれを正確に再現しなければなりません。
5. 16 バイトのヘッダーに続くペイロードの**無線上のフォーマット**: `ciphertext | tag (8 bytes) | extraNonce (4 bytes, LE)`。オーバーヘッドは 12 バイトなので、平文の `Data` の最大長は 239 − 12 = 227 バイトです。
6. **ヘッダー**: `channel = 0x00`、`to` = 受信者のノード番号、`want_ack = 1`。
7. `extraNonce` はメッセージごとに新しく生成される 32 ビットの乱数です。

受信時、`channel == 0x00` **かつ** `to` が自分のノード番号であるフレームは PKI として扱われます。送信者の公開鍵が保存されている必要があります。分からない場合、フレームは復号できません（[鍵が不明な場合の扱い](#鍵が不明な場合の扱い)を参照）。タグは定数時間で比較され、失敗した場合は平文バッファを消去してフレームを破棄します。

コード: [`crypto.cpp`](../../src/mesh/crypto.cpp) の `x25519Shared` / `pkiEncrypt` / `pkiDecrypt`、[`aes_ccm.cpp`](../../src/mesh/aes_ccm.cpp)。

## どの鍵を何に使うか

| フレーム | 暗号化 | `channel` バイト |
|---|---|---|
| 公開チャンネルへのテキスト（ブロードキャスト） | チャンネル鍵（CTR） | `0x08` |
| 1 つのノードへのテキスト（DM） | **PKI**（CCM） | `0x00` |
| NodeInfo（ブロードキャストまたは宛先指定） | チャンネル鍵 | `0x08` |
| ルーティング ACK / NAK | チャンネル鍵 | `0x08` |

受信時:

- チャンネルハッシュが `0x08` のフレームは、デフォルト鍵で復号されます。`0x00` 以外のその他のチャンネルハッシュは無視されますが、`rx` にはカウントされます。
- **旧形式の DM は拒否されます**: チャンネル鍵で暗号化され、（ブロードキャストではなく）単一のノード宛ての `TEXT_MESSAGE_APP` フレームは、公式ファームウェアと同様に破棄されます。テキストの DM は PKI でなければなりません。
- `from` が自分のノード番号または `0xFFFFFFFF` のフレームは無視されます。

## Protobuf サブセット

手書きのエンコーダ／デコーダ（[`pb.cpp`](../../src/mesh/pb.cpp)）。未知のフィールドは読み飛ばし、不正な入力や途中で切れた入力は拒否します。

**`Data`**（復号されたペイロード）:

| フィールド | 番号 | 型 | 用途 |
|---|---|---|---|
| `portnum` | 1 | varint | `1` TEXT_MESSAGE_APP、`4` NODEINFO_APP、`5` ROUTING_APP。その他はアクティビティとしてのみ認識 |
| `payload` | 2 | bytes | 233 バイト以下を受け付ける |
| `want_response` | 3 | varint (bool) | 起動時の NodeInfo と NodeInfo 要求で設定 |
| `dest` | 4 | fixed32 | デコードするが未使用 |
| `source` | 5 | fixed32 | デコードするが未使用 |
| `request_id` | 6 | fixed32 | ACK/NAK で、確認対象のパケットの ID |
| `reply_id` | 7 | fixed32 | デコードするが未使用 |

**`User`**（NODEINFO_APP のペイロード）:

| フィールド | 番号 | 型 | 送信する値 |
|---|---|---|---|
| `id` | 1 | string | 自分のノード番号の `!%08x` |
| `long_name` | 2 | string | 自分のロングネーム。UTF-8 の文字境界で 24 バイトに切り詰め |
| `short_name` | 3 | string | 自分のショートネーム（4 バイト以下） |
| `hw_model` | 5 | varint | `112`（M5STACK_CARDPUTER_ADV） |
| `role` | 7 | varint | `1`（CLIENT_MUTE） |
| `public_key` | 8 | bytes | 自分の 32 バイトの X25519 公開鍵 |

受信した `User` に `public_key` があり、それがちょうど 32 バイトでない場合、その `User` 全体を拒否します。フィールド 4（`macaddr`、非推奨）と 6（`is_licensed`）は読み飛ばします。

**`Routing`**（ROUTING_APP のペイロード）:

| フィールド | 番号 | 型 | 用途 |
|---|---|---|---|
| `error_reason` | 3 | varint (enum) | `0` NONE = ACK、それ以外 = NAK。送信するのは `35` PKI_UNKNOWN_PUBKEY |

## ACK と NAK

- **ACK の送信**: 自分宛てで `want_ack` が付いた PKI の DM すべてに対し、`request_id` = その DM のパケット ID とし、`Routing{error_reason = NONE}` をバイト `18 00` として明示的にエンコードした `ROUTING_APP` フレームを送信者に送ります。チャンネル暗号化、ホップリミット 3、`want_ack` なしです。同じ DM が再び届いた場合（こちらの ACK が失われたための再送）、もう一度 ACK を送ります。このために、最後に ACK した 8 つの `(from, id)` の組を記憶しています。
- **受信**: 自分宛てで `request_id` が 0 でない ROUTING_APP フレームは、`error_reason` がないか 0 であれば ACK です。Proto3 はゼロ値を省略するので、空の `Routing` メッセージも NONE を意味します。対応するメッセージは送信済み（`.`）から確認済み（`v`）に変わります。`error_reason` が 0 以外なら NAK で、メッセージは失敗（`x`）になります。
- DM を自動で再送することはありません。

## 鍵が不明な場合の扱い

- **鍵のないノードへの送信**: DM は保留され（状態 `?`）、want_response = 1 を付けた自分の NodeInfo をそのノードに**直接**送ります（1 ノードにつき最大 60 秒に 1 回）。鍵を含む相手の NodeInfo が届くと、保留中の DM が送信されます。DM が保留されている間、要求は最大 60 秒に 1 回繰り返されます。保留中の DM は 10 分後に失敗（`x`）になります。また、10 件の送信箱がいっぱいで、鍵のあるノードへの DM がその枠を必要とする場合は、それより早く失敗になります。
- **鍵のないノードからの PKI DM の受信**: `request_id` = そのパケット ID とした NAK `Routing{error_reason = 35 (PKI_UNKNOWN_PUBKEY)}` と、`want_response = 1` を付けた自分の NodeInfo を送ります。制限: 1 ノードにつき 10 分に 1 回、全体で 30 秒に 1 回。デコードできないフレームは重複キャッシュに**追加されない**ので、鍵が届いた後の送信者の再試行は復号できます。

## NodeInfo

- 自分の NodeInfo は、起動時（`want_response` 付き）、3 時間ごと、名前の変更後、Config 画面から要求されたときにブロードキャストします。手動と名前変更によるブロードキャストは最低 30 秒の間隔を空けます。
- `want_response` 付きの NodeInfo（ブロードキャストまたは自分宛て）を受信すると、`want_response` なしの自分の NodeInfo をそのノードに直接送って応答します。制限: 1 ノードにつき 12 時間に 1 回、全体で 30 秒に 1 回。送信者を偽装されても、送信し続けさせられることがないようにするためです。

## 最初の鍵が優先

Meshtastic の NodeDB と同様に、あるノード番号について最初に受信した 32 バイトの公開鍵が保存され、置き換えられることはありません。そのノードの後続の `User` は、**同じ**鍵を持つ場合にのみ適用されます。鍵が異なるか欠けている場合、名前も含めて更新全体が無視されます。DM の相手の鍵は（最大 50 まで）永続化されるので、このルールは再起動後も維持されます。自分の鍵も同じ理由で一度だけ生成され、変わることはありません。

受信側は、ノード番号とその鍵の対応を検証しません。ノードの鍵が分かる前であれば、誰でもそのノード番号を名乗って自分の鍵を含む NodeInfo を送ることができ、その鍵が優先されてしまいます。これは上流プロトコルの性質です。[security.md](security.md) を参照してください。

## ノード番号

自分のノード番号は、公式ファームウェアと同様に、**工場出荷時の MAC アドレスの下位 4 バイト**をビッグエンディアンで並べたものです（`mac[2] << 24 | mac[3] << 16 | mac[4] << 8 | mac[5]`）。MAC を読めない場合や、予約値（`< 4` または `0xFFFFFFFF`）になる場合は、代わりに公開鍵の FNV-1a ハッシュを使います。ノード ID の文字列は、`!` の後にこの番号を小文字の 16 進 8 桁で続けたものです。

## パケット ID

`id = (rnd << 10) | (counter & 0x3FF)`、0 にはなりません。`rnd` は、起動のたびにハードウェア RNG でシードされる xorshift64* 生成器からの 32 ビット値なので、ID（したがって CTR のカウンタブロックと CCM のノンス）が再起動をまたいで繰り返されることはありません。`counter` は ID を作るたびに増えます。上流も同じ方式を使っています。

## 重複の抑制

直近 **64** 個の `(from, id)` の組を保持するリングバッファです。組がキャッシュにあるフレームは無視されます。ただし、すでに ACK した DM の重複には再び ACK を送ります。組が記録されるのは、フレームが正常に処理された後だけです。復号やデコードに失敗したフレームは記録されません。

## ホップリミットと中継

自分が発信するすべてのフレームは、Meshtastic のデフォルトである `hop_limit = hop_start = 3` です。他のノードのフレームを**中継することはありません**。自分の NodeInfo はロール `CLIENT_MUTE` を宣言しており、ファームウェアに再ブロードキャストの経路はありません。

## v1 で実装されていないもの

XEdDSA によるパケット署名（上流のデフォルトポリシーは署名なしのパケットを受け付けます）、デフォルトのプライマリ以外のチャンネル、独自の PSK と AES-256-CTR チャンネル、LongFast 以外のプリセット、位置情報、テレメトリ、トレースルート、ストアアンドフォワード、MQTT。[roadmap.md](roadmap.md) を参照してください。
