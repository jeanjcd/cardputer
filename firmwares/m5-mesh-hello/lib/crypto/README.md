# Vendored crypto (rweather/Crypto 0.4.0, MIT)

Subset of [rweather/arduinolibs](https://github.com/rweather/arduinolibs) `Crypto` 0.4.0
(PlatformIO `rweather/Crypto@0.4.0`): AES-128/256 (+ ESP32 hardware backend), BlockCipher,
Cipher, CTR, Hash, SHA256, Curve25519, BigNumberUtil, Crypto utils and `utility/` headers.
Vendored because the upstream package cannot build on the native test host (its `RNG.cpp`
requires `Arduino.h`). Only local change: `Curve25519::dh1()` and the `RNG.h` include were
removed (RNG is not vendored; keys are generated with an injected RNG + `Curve25519::eval`).
See `LICENSE`.
