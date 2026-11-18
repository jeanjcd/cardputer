> 英語版からの翻訳です。内容に相違がある場合は[英語版](../en/development.md)が優先されます。

# 開発

M5-Mesh-Hello のビルド、テスト、リリースの方法です。コーディング規約、コミットのスタイル、プルリクエストのチェックリストは [CONTRIBUTING.md](../../CONTRIBUTING.md)（英語）にあります。コマンドはすべて `firmwares/m5-mesh-hello/` フォルダで実行します。

## ツールチェーン

| ツール | バージョン | 備考 |
|---|---|---|
| Python | 3.10+（CI は 3.12） | |
| PlatformIO Core | **6.2.0**（CI で固定） | `python -m pip install "platformio==6.2.0"` でインストールし、`python -m platformio ...`（`PATH` にあれば `pio ...`）として実行 |
| プラットフォーム | `espressif32@8.0.0`（Arduino） | PlatformIO がダウンロード |
| ライブラリ | M5Cardputer 1.1.1、M5Unified 0.3.0、M5GFX 0.3.0、RadioLib 7.8.1 | [`platformio.ini`](../../platformio.ini) で固定 |
| 暗号 | rweather/Crypto のサブセット | [`lib/crypto/`](../../lib/crypto/) に同梱（MIT） |
| ネイティブテスト用のホストコンパイラ | C++17 対応の GCC | **Windows**: [WinLibs](https://winlibs.com/)（`winget install BrechtSanders.WinLibs.POSIX.UCRT` を実行し、その `mingw64/bin` を `PATH` に追加）。**Linux**: `gcc`/`g++`（例: `build-essential`）。**macOS**: Xcode Command Line Tools または Homebrew の GCC。 |

Windows の Git Bash での例:

```sh
PATH="/c/Users/<you>/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin:$PATH" \
  python -m platformio test -e native
```

## ビルド

```sh
python -m platformio run -e cardputer      # -> .pio/build/cardputer/firmware.bin
```

できたファイルは M5Launcher でインストールする（SD カードにコピー）か、`python -m platformio run -e cardputer -t upload` で書き込みます。ファームウェアを実行する前にアンテナを取り付けてください。

## 環境

| 環境 | 用途 |
|---|---|
| `cardputer` | デバイスのファームウェア（ESP32-S3、Arduino） |
| `native` | PC 上のネイティブ Unity テスト。`src/mesh/`（移植性のあるコア）とテストだけをビルドします。 |
| `native_sanitize` | `native` に `-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g` を加えたもの。リンクフラグは [`tools/sanitize_link.py`](../../tools/sanitize_link.py) が追加します。 |

## ネイティブテスト

```sh
python -m platformio test -e native                       # all suites
python -m platformio test -e native -f test_region        # one suite
```

（1 行目はすべてのスイート、2 行目は 1 つのスイートだけを実行します。）

| スイート | 対象 |
|---|---|
| `test_codec` | ヘッダーと protobuf のエンコード／デコード |
| `test_crypto` | AES-CTR、X25519、SHA-256、AES-CCM を参照ベクタで検証 |
| `test_packet` | フレームの組み立てと解析、PKI、ACK/NAK、重複抑制 |
| `test_region` | djb2、全リージョンの周波数、チャンネルハッシュ、出力の制限、送信時間、デューティ予算 |
| `test_logic` | NodeDb、ChatStore、Greeter、Outbox |
| `test_app_logic` | 設定（デフォルト、blob の往復変換、検証、旧キー）、あいさつ文の生成、CP437 表示変換、テキスト編集ヘルパー、ハードウェアモデル名、レート制限 |
| `test_fuzz` | 受信経路のファジング的な堅牢性テスト（下記参照） |
| `test_docs_regions` | [`docs/en/frequencies.md`](../en/frequencies.md) のリージョン表が `src/mesh/region.*` と一致すること |

### サニタイザ (Linux/macOS のみ)

```sh
python -m platformio test -e native_sanitize
```

ASan/UBSan にはサニタイザのランタイムが必要で、Linux と macOS では GCC/Clang が提供しています。Windows の MinGW/WinLibs にはないため、この環境は Windows ではリンクできません。WSL を使うか、CI に任せてください。CI はすべての push とプルリクエストで、`ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:strict_string_checks=1` と `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1` を付けてこれを実行します。

### ファジングテスト

`test/test_fuzz` は、シードを固定した **200,000** 個の入力を `headerParse`、`pbDecodeData`/`pbDecodeUser`/`pbDecodeRoutingError`、`pkiDecrypt`、`parseFrame` に流し込みます。入力はランダムなフレームと、正しいフレームの変異（ビット反転、バイトの上書き、挿入、削除、切り詰め、追加）です。クラッシュしないこと、出力構造体の外に何も書き込まれないこと（カナリア）、受け付けた結果がすべて一貫していることを確認します。各入力はちょうどのサイズのヒープバッファに置かれるので、ASan が範囲外読み込みを検出します。反復回数を変えるには、環境変数 `M5MH_FUZZ_ITERS` を設定します。シードは固定なので、実行結果は再現可能です。

### ドキュメントのチェック

`test/test_docs_regions` は、`docs/en/frequencies.md` の `<!-- regions:begin -->` と `<!-- regions:end -->` の間の表を解析し、各行をコードと比較します: 名前と順序、バンドの端、LongFast の周波数、出力の上限、デューティサイクル、実験的フラグ。コードのリージョン表を変更した場合は、同じコミットでドキュメントを更新してください。このテストは `native` スイートで実行されるので、一致しないと CI が失敗します。テストはプロジェクトフォルダからの相対パスでファイルを探し、見つからなければ自身のソースのパスからの相対パスで探します。チェックされるのは英語版だけです。`docs/pt-BR/`、`docs/es/`、`docs/ru/`、`docs/ja/` の翻訳はマーカーなしで表を再掲しています。

### リンクのチェック

```sh
python tools/check_links.py
```

[`tools/check_links.py`](../../tools/check_links.py) は、`README*.md` と `docs/**/*.md` にあるすべての相対リンクをチェックします。リンク先のファイルやフォルダが存在すること、そして `#fragment` がリンク先の Markdown ファイルの見出しと一致すること（GitHub 形式のアンカー）を確認します。外部の `http(s)` リンクはチェックしません。CI はネイティブテストのジョブでこれを実行します。

## テストベクタの再生成

[`test/vectors.h`](../../test/vectors.h) は生成されたファイルです。手で編集しないでください。

```sh
python -m pip install cryptography
python tools/gen_vectors.py
```

生成器は決定的です。意図的に生成器を変更しない限り、出力はバイト単位で同一でなければなりません。

## デバイス上でテストを実行しない

**M5Launcher がインストールされた Cardputer に対して `pio test` を実行しないでください。** PlatformIO のデバイス上テストランナーはアプリ領域全体に独自のイメージを書き込み、M5Launcher とそれでインストールしたすべてのアプリを上書きします。このプロジェクトはネイティブでのみテストします。デバイスでの動作は、ビルドし、M5Launcher でインストールし、実地テストすることで確認します。

## CI

[`.github/workflows/m5-mesh-hello.yml`](../../../../.github/workflows/m5-mesh-hello.yml) は、このフォルダまたはワークフローに触れるすべての push とプルリクエスト、および手動実行で動きます。権限: `contents: read`。すべてのアクションは完全なコミット SHA で固定されています。

| ジョブ | 内容 |
|---|---|
| Native tests | `python tools/check_links.py`（リンクチェック）の後、ubuntu-latest で `pio test -e native`（ドキュメントのチェックを含む） |
| Native tests (ASan + UBSan) | `pio test -e native_sanitize` |
| Device build | コミットされた `bin/` のイメージを `sha256sum -c SHA256SUMS` で検証し、`pio run -e cardputer` を実行し、リリースイメージ、CI でビルドした `firmware.bin`、チェックサムを含むアーティファクトをアップロード |

[`.github/workflows/codeql-m5-mesh-hello.yml`](../../../../.github/workflows/codeql-m5-mesh-hello.yml) は、同じトリガーと毎週のスケジュールで CodeQL（`c-cpp`、`security-extended` クエリ、ネイティブテストプログラムの手動ビルド）を実行します。リポジトリは公開されているので、結果は code scanning にアップロードされ、リポジトリの **Security** タブに表示されます。SARIF アーティファクト（`codeql-results-m5-mesh-hello`）としても保存され、**検出結果が 1 件でもあればジョブは失敗します**。解析対象はネイティブテストのビルドだけで、`src/mesh/` の移植性のあるコアとテストです。`src/app/` と `src/hw/` のデバイス専用コードはそのビルドに含まれません。

[`.github/dependabot.yml`](../../../../.github/dependabot.yml) は、固定された GitHub Actions の更新を毎週提案します。

## リリース

1. [`CHANGELOG.md`](../../CHANGELOG.md) を更新します: `Unreleased` の項目を新しいバージョンと日付の下に移します。
2. ネイティブテストとデバイスのビルドを実行します。
   ```sh
   python -m platformio test -e native
   python -m platformio run -e cardputer
   ```
3. アプリケーションイメージをコピーし、統合したフルフラッシュイメージを作成します（`<framework-arduinoespressif32>` は `~/.platformio/packages/framework-arduinoespressif32` です）。
   ```sh
   cp .pio/build/cardputer/firmware.bin bin/M5-Mesh-Hello.bin
   esptool --chip esp32s3 merge-bin -o bin/M5-Mesh-Hello-merged.bin \
     0x0 .pio/build/cardputer/bootloader.bin 0x8000 .pio/build/cardputer/partitions.bin \
     0xe000 <framework-arduinoespressif32>/tools/partitions/boot_app0.bin 0x10000 .pio/build/cardputer/firmware.bin
   ```
4. チェックサムを再生成します。
   ```sh
   cd bin && sha256sum *.bin > SHA256SUMS
   ```
5. `bin/M5-Mesh-Hello.bin` を M5Launcher でインストールし、デバイスで簡単な動作確認をします（無線の初期化、ステータスバー、Config、再起動後に識別情報が変わっていないこと）。
6. コミットして push し、CI が通るのを待ちます（Device build ジョブが `SHA256SUMS` を再検証します）。
7. リリースに `m5-mesh-hello-v<version>` の形式でタグを付け（CHANGELOG のリンクはこの形式を使います）、2 つのイメージと `SHA256SUMS` を GitHub のリリースに添付します。

## コーディング規約

[CONTRIBUTING.md](../../CONTRIBUTING.md#coding-conventions)（英語）を参照してください。要約: `src/mesh/` は移植性を保つ（Arduino/ESP-IDF のヘッダーを使わない）、無線からのすべてのバイトは信頼せず範囲チェックする、受信経路でヒープを使わない、範囲チェック付きの文字列関数だけを使う、秘密情報は `secureZero()` で消去し決して表示しない、すべての公開関数に 1 行のドキュメントコメントを付ける、すべてのファイルに SPDX ヘッダーを付ける、依存関係を固定する。ドキュメントの変更は英語（`docs/en/`）から始めます。翻訳はその後に続きます。
