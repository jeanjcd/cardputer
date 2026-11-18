# Contributing to M5-Mesh-Hello

Thanks for your interest. Bug reports, fixes, tests, documentation and
translations are all welcome. For security problems, follow
[SECURITY.md](SECURITY.md) instead of opening a public issue.

By contributing you agree that your work is licensed under the project's
license, GPL-3.0-or-later (see [LICENSE](LICENSE)).

## Development setup

Everything builds and tests from inside this folder.
The full developer documentation (toolchain, test suites, CI, release steps)
is in [docs/en/development.md](docs/en/development.md); the architecture is
described in [docs/en/architecture.md](docs/en/architecture.md).

1. **Python 3.10+** and **PlatformIO Core** (the CI pins `platformio==6.2.0`):

   ```sh
   python -m pip install "platformio==6.2.0"
   ```

2. **A host C++ compiler** for the native tests (GCC with C++17):
   - **Windows**: [WinLibs](https://winlibs.com/) (GCC + MinGW-w64, UCRT), for
     example `winget install BrechtSanders.WinLibs.POSIX.UCRT`, then put its
     `mingw64/bin` on `PATH`.
   - **Linux**: `gcc`/`g++` from your distribution (e.g. `build-essential`).
   - **macOS**: Xcode Command Line Tools (`xcode-select --install`) or GCC from
     Homebrew.

3. The ESP32 toolchain, framework and libraries for the device build are
   downloaded by PlatformIO on first use. Library versions are pinned in
   `platformio.ini`; the crypto library is vendored in `lib/crypto/`.

## Build

```sh
python -m platformio run -e cardputer      # .pio/build/cardputer/firmware.bin
```

Flashing: `python -m platformio run -e cardputer -t upload`, or install a
release image from `bin/` with M5Launcher. Always attach the antenna before the
radio transmits.

## Native tests

```sh
python -m platformio test -e native                 # all suites
python -m platformio test -e native -f test_fuzz    # a single suite
```

The suites cover the header and protobuf codecs, crypto (against reference
vectors), packet building/parsing, regions and airtime, the application logic,
and a fuzz-style robustness test (`test/test_fuzz`, 200,000 seeded iterations;
set `M5MH_FUZZ_ITERS` to change the count).

### Sanitizers

```sh
python -m platformio test -e native_sanitize
```

`native_sanitize` builds the native tests with
`-fsanitize=address,undefined -fno-sanitize-recover=all`. It needs a compiler
with the ASan/UBSan runtimes: GCC or Clang on Linux or macOS. MinGW/WinLibs on
Windows has no ASan runtime, so on Windows use WSL or rely on the CI job, which
runs it on Linux for every push and pull request.

## Regenerating test vectors

`test/vectors.h` is generated; do not edit it by hand.

```sh
python -m pip install cryptography
python tools/gen_vectors.py
```

The generator is deterministic; the output must be byte-identical unless you
changed the generator on purpose.

## Release images

`bin/M5-Mesh-Hello.bin` is a copy of `.pio/build/cardputer/firmware.bin`. The
merged image (full flash image written at `0x0`) is built with esptool, where
`<framework-arduinoespressif32>` is `~/.platformio/packages/framework-arduinoespressif32`:

```sh
cp .pio/build/cardputer/firmware.bin bin/M5-Mesh-Hello.bin
esptool --chip esp32s3 merge-bin -o bin/M5-Mesh-Hello-merged.bin \
  0x0 .pio/build/cardputer/bootloader.bin 0x8000 .pio/build/cardputer/partitions.bin \
  0xe000 <framework-arduinoespressif32>/tools/partitions/boot_app0.bin 0x10000 .pio/build/cardputer/firmware.bin
```

After updating the images, regenerate the checksums:

```sh
cd bin && sha256sum *.bin > SHA256SUMS
```

## Coding conventions

- **English** for code, identifiers, comments and commit messages.
- **`src/mesh/` is portable**: no Arduino, M5 or ESP-IDF headers. It must build
  and run natively. Hardware code lives in `src/hw/`, the app/UI in `src/app/`.
- **Every byte from the radio is untrusted.** Bounds-check every length and
  index, use fixed-size buffers with explicit caps, and never read or write
  past a buffer.
- **No dynamic allocation in the RX/parse path** (no `malloc`/`new`/`String`).
- **Bounded string operations only**: no `strcpy`, `strcat`, `sprintf`; use
  `snprintf`, explicit lengths and the helpers in `src/mesh/text.h`.
- **Secrets** (private key, shared keys, key material on the stack) are wiped
  with `secureZero()` from `src/mesh/secure_zero.h` after use and are never
  printed, logged or displayed. Compare tags in constant time.
- **One-line doc comment** for every function in a public header.
- New source files start with `// SPDX-License-Identifier: GPL-3.0-or-later`.
- Add or update native tests for any change to `src/mesh/`. New parsing code
  should also be exercised by `test/test_fuzz`.
- Keep dependencies pinned to exact versions.

## Commit style

- One logical change per commit, imperative mood, short summary line
  (about 72 characters), prefixed with the component, for example:
  `m5-mesh-hello: reject NodeInfo with an empty long name`.
- Explain the reason in the body when it is not obvious.
- Do not commit build output (`.pio/`) or editor settings (`.vscode/`).

## Pull request checklist

- [ ] `python -m platformio test -e native` passes.
- [ ] `python -m platformio run -e cardputer` succeeds.
- [ ] New or changed behaviour in `src/mesh/` has tests.
- [ ] No new heap allocation or unbounded string operation in the RX path.
- [ ] Secrets are zeroized and never logged.
- [ ] Documentation (English) and `CHANGELOG.md` (`Unreleased`) are updated
      when user-visible behaviour changes.
- [ ] If `bin/` changed, `bin/SHA256SUMS` is regenerated.
- [ ] CI is green (native tests, sanitizers, device build).

## Translations

Documentation is published in English, Português (Brasil), Español and
Русский. **English is the source of truth**: change the English document
first, then the translations (or open the PR with English only and say which
translations need an update). Corrections and new translations are welcome.
Do not translate code, commands, identifiers, register or field names, or
numbers.
