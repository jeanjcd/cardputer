# Development

How to build, test and release M5-Mesh-Hello. Coding conventions, commit style and the pull-request checklist are in [CONTRIBUTING.md](../../CONTRIBUTING.md). All commands run from the `firmwares/m5-mesh-hello/` folder.

## Toolchain

| Tool | Version | Notes |
|---|---|---|
| Python | 3.10+ (CI uses 3.12) | |
| PlatformIO Core | **6.2.0** (pinned in CI) | `python -m pip install "platformio==6.2.0"`, then run it as `python -m platformio ...` (or `pio ...` if it is on `PATH`) |
| Platform | `espressif32@8.0.0` (Arduino) | Downloaded by PlatformIO |
| Libraries | M5Cardputer 1.1.1, M5Unified 0.3.0, M5GFX 0.3.0, RadioLib 7.8.1 | Pinned in [`platformio.ini`](../../platformio.ini) |
| Crypto | rweather/Crypto subset | Vendored in [`lib/crypto/`](../../lib/crypto/) (MIT) |
| Host compiler for native tests | GCC with C++17 | **Windows**: [WinLibs](https://winlibs.com/) (`winget install BrechtSanders.WinLibs.POSIX.UCRT`, put its `mingw64/bin` on `PATH`). **Linux**: `gcc`/`g++` (e.g. `build-essential`). **macOS**: Xcode Command Line Tools or Homebrew GCC. |

On Windows with Git Bash, for example:

```sh
PATH="/c/Users/<you>/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin:$PATH" \
  python -m platformio test -e native
```

## Build

```sh
python -m platformio run -e cardputer      # -> .pio/build/cardputer/firmware.bin
```

Install the result with M5Launcher (copy it to the SD card) or flash it with `python -m platformio run -e cardputer -t upload`. Attach the antenna before the firmware runs.

## Environments

| Environment | Purpose |
|---|---|
| `cardputer` | Device firmware (ESP32-S3, Arduino) |
| `native` | Native Unity tests on the PC. Builds only `src/mesh/` (the portable core) plus the test. |
| `native_sanitize` | `native` with `-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g`. Link flags are added by [`tools/sanitize_link.py`](../../tools/sanitize_link.py). |

## Native tests

```sh
python -m platformio test -e native                       # all suites
python -m platformio test -e native -f test_region        # one suite
```

| Suite | Covers |
|---|---|
| `test_codec` | Header and protobuf encode/decode |
| `test_crypto` | AES-CTR, X25519, SHA-256, AES-CCM against reference vectors |
| `test_packet` | Frame building and parsing, PKI, ACK/NAK, dedup |
| `test_region` | djb2, frequencies of every region, channel hash, power clamp, airtime, duty budget |
| `test_logic` | NodeDb, ChatStore, Greeter, Outbox |
| `test_app_logic` | Settings (defaults, blob round-trip, validation, legacy key), greeting composition, CP437 display conversion, text editing helpers, hardware model names, rate limiters |
| `test_fuzz` | Fuzz-style robustness of the receive path (see below) |
| `test_docs_regions` | The region table in [`docs/en/frequencies.md`](frequencies.md) matches `src/mesh/region.*` |

### Sanitizers (Linux/macOS only)

```sh
python -m platformio test -e native_sanitize
```

ASan/UBSan need the sanitizer runtimes, which GCC/Clang provide on Linux and macOS. MinGW/WinLibs on Windows has none, so this environment does not link there: use WSL, or rely on CI, which runs it on every push and pull request with `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:strict_string_checks=1` and `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`.

### Fuzz test

`test/test_fuzz` feeds **200,000** seeded inputs through `headerParse`, `pbDecodeData`/`pbDecodeUser`/`pbDecodeRoutingError`, `pkiDecrypt` and `parseFrame`. The inputs are random frames and mutations of valid ones: bit flips, byte overwrites, inserts, deletes, truncation, appends. It checks that nothing crashes, nothing is written outside the output structs (canaries) and every accepted result is consistent. Each input sits in an exactly-sized heap buffer, so ASan catches over-reads. Set `M5MH_FUZZ_ITERS` in the environment to change the iteration count. The seed is fixed, so runs are reproducible.

### Docs check

`test/test_docs_regions` parses the table between `<!-- regions:begin -->` and `<!-- regions:end -->` in `docs/en/frequencies.md` and compares every row with the code: name and order, band edges, LongFast frequency, power limit, duty cycle and experimental flag. If you change the region table in code, update the doc in the same commit. The test runs in the `native` suite, so CI fails when they disagree. The test looks for the file relative to the project folder, with a fallback relative to its own source path. Only the English file is checked; the translations in `docs/pt-BR/`, `docs/es/` and `docs/ru/` reproduce the table without the markers.

### Link check

```sh
python tools/check_links.py
```

[`tools/check_links.py`](../../tools/check_links.py) checks every relative link in `README*.md` and `docs/**/*.md`: the target file or folder must exist, and a `#fragment` must match a heading in the target Markdown file (GitHub-style anchors). External `http(s)` links are not checked. CI runs it in the native-tests job.

## Regenerating test vectors

[`test/vectors.h`](../../test/vectors.h) is generated. Do not edit it by hand.

```sh
python -m pip install cryptography
python tools/gen_vectors.py
```

The generator is deterministic. The output must be byte-identical unless you changed the generator on purpose.

## Do not run tests on the device

**Never run `pio test` against the Cardputer when M5Launcher is installed.** PlatformIO's on-device test runner flashes its own image over the whole app area, which overwrites M5Launcher and every app installed through it. This project tests only natively. Device behaviour is verified by building, installing through M5Launcher and field testing.

## CI

[`.github/workflows/m5-mesh-hello.yml`](../../../../.github/workflows/m5-mesh-hello.yml) runs on every push and pull request that touches this folder or the workflow, and on manual dispatch. Permissions: `contents: read`. All actions are pinned to full commit SHAs.

| Job | What it does |
|---|---|
| Native tests | `python tools/check_links.py` (link check), then `pio test -e native` on ubuntu-latest (includes the docs check) |
| Native tests (ASan + UBSan) | `pio test -e native_sanitize` |
| Device build | Verifies the committed `bin/` images with `sha256sum -c SHA256SUMS`, runs `pio run -e cardputer`, and uploads an artifact with the release images, the CI-built `firmware.bin` and checksums |

[`.github/workflows/codeql-m5-mesh-hello.yml`](../../../../.github/workflows/codeql-m5-mesh-hello.yml) runs CodeQL (`c-cpp`, `security-extended` queries, manual build of the native test programs) on the same triggers and weekly. The repository is public, so results are uploaded to code scanning and appear in the repository's **Security** tab. They are also kept as a SARIF artifact (`codeql-results-m5-mesh-hello`), and **the job fails if there is any finding**. Only the native test build is analysed: the portable core in `src/mesh/` and the tests. The device-only code in `src/app/` and `src/hw/` is not part of that build.

[`.github/dependabot.yml`](../../../../.github/dependabot.yml) proposes weekly updates for the pinned GitHub Actions.

## Releasing

1. Update [`CHANGELOG.md`](../../CHANGELOG.md): move the `Unreleased` entries under a new version and date.
2. Run the native tests and the device build:
   ```sh
   python -m platformio test -e native
   python -m platformio run -e cardputer
   ```
3. Copy the application image and build the merged full-flash image (`<framework-arduinoespressif32>` is `~/.platformio/packages/framework-arduinoespressif32`):
   ```sh
   cp .pio/build/cardputer/firmware.bin bin/M5-Mesh-Hello.bin
   esptool --chip esp32s3 merge-bin -o bin/M5-Mesh-Hello-merged.bin \
     0x0 .pio/build/cardputer/bootloader.bin 0x8000 .pio/build/cardputer/partitions.bin \
     0xe000 <framework-arduinoespressif32>/tools/partitions/boot_app0.bin 0x10000 .pio/build/cardputer/firmware.bin
   ```
4. Regenerate the checksums:
   ```sh
   cd bin && sha256sum *.bin > SHA256SUMS
   ```
5. Install `bin/M5-Mesh-Hello.bin` through M5Launcher and smoke-test it on the device (radio init, status bar, Config, identity unchanged after reboot).
6. Commit, push, and wait for CI to pass (the device-build job re-verifies `SHA256SUMS`).
7. Tag the release as `m5-mesh-hello-v<version>` (the CHANGELOG links use this format) and attach the two images and `SHA256SUMS` to the GitHub release.

## Coding conventions

See [CONTRIBUTING.md](../../CONTRIBUTING.md#coding-conventions). In short: `src/mesh/` stays portable (no Arduino/ESP-IDF headers), every byte from the radio is untrusted and bounds-checked, there is no heap in the RX path, only bounded string functions are used, secrets are wiped with `secureZero()` and never shown, every public function has a one-line doc comment, every file has an SPDX header, and dependencies are pinned. Documentation changes start in English (`docs/en/`). Translations follow.
