# Security

This page covers what M5-Mesh-Hello v0.1.0 protects, what it does not protect, and how to verify a release. To report a vulnerability, follow [SECURITY.md](../../SECURITY.md). Do not open a public issue.

## Threat model

| Adversary | Capability | Main concerns |
|---|---|---|
| **Remote attacker over LoRa** | Can transmit arbitrary frames on the region's frequency from a distance | Memory corruption or crashes from malformed frames, decryption bypass, forcing the device to transmit (air abuse), storage exhaustion |
| **Malicious or spoofing Meshtastic node** | Can send well-formed Meshtastic frames with any `from` node number, names and key | Impersonating other nodes, replacing a peer's key, flooding NodeInfo requests, making us spam greetings |
| **Physical access** | Holds the device or its flash chip | Reading the private key, reading saved peers and settings, installing modified firmware |
| **Supply chain** | Tampers with dependencies, CI or the published binaries | Malicious code in the build or in `bin/` |

## Protections

### Untrusted radio input

- **Every byte from the radio is untrusted.** The header parser, protobuf decoder, decryption and application logic check every length and index against fixed-size buffers. Frames longer than 255 bytes, payloads longer than 239 bytes, protobuf fields that run past the end, oversized `payload` fields and public keys that are not 32 bytes are rejected.
- **No heap allocation in the RX path.** All buffers are fixed-size, and the large decoded struct is static.
- **Fuzzing and sanitizers.** `test/test_fuzz` runs 200,000 random and mutated frames through the protocol decoders: `headerParse`, the protobuf decoders, `pkiDecrypt` and `parseFrame`. The application layer on top of them (`App::handleFrame` in `src/app/`) runs only on the device and is not fuzzed; its decision logic (node table, outbox, rate limits, settings) is unit-tested in `src/mesh/`. CI runs every native test, the fuzz test included, under AddressSanitizer and UndefinedBehaviorSanitizer.
- **Static analysis.** CodeQL (`security-extended`) runs on every push and pull request and weekly, and its results appear in the repository's Security tab. It analyses the native test build, that is the portable core in `src/mesh/` plus the tests; the device-only code in `src/app/` and `src/hw/` is not compiled there and therefore not analysed. The job fails on any finding.

### Cryptography

- DMs use **X25519 + SHA-256 + AES-256-CCM** with an 8-byte tag, as in Meshtastic. Tags are compared in **constant time**, and a failed decryption wipes the plaintext buffer, so unauthenticated data is never used.
- Weak X25519 peer keys (those giving an invalid shared secret) are rejected.
- **Secrets are wiped** with `secureZero()` after use: shared keys, raw X25519 output, key schedules, the CTR counter block, and the key record read from NVS.
- The private key is **never displayed, printed or logged**. The UI shows only the first 4 bytes of the public key.
- **Key generation uses hardware entropy.** The ESP32-S3's RF-noise entropy source is enabled (`bootloader_random_enable`) while the key is generated, because Wi-Fi and Bluetooth are off. The packet-ID generator is also seeded from hardware entropy at every boot, so CTR counter blocks and CCM nonces do not repeat across reboots, and every DM carries a fresh random `extraNonce`.
- The stored key pair is checked on load (`pub == X25519(priv)`). A corrupt record is never used half-way.

### Radio behaviour and anti-abuse

- **Wi-Fi and Bluetooth are off.** The only remote interface is LoRa.
- **Nothing is transmitted before a region is chosen.** On first boot (no valid region stored) the radio only listens until the user confirms a region. While a region change is pending, or while the Cap or radio is not working, frames stay queued.
- **The device never relays** other nodes' traffic (`CLIENT_MUTE`).
- **Automatic replies are rate-limited**, both per node and globally, so spoofed `from` numbers cannot keep the device on the air:
  - NodeInfo replies: once per 12 h per node and once per 30 s in total.
  - NAK + NodeInfo for PKI DMs we cannot decrypt: once per 10 min per node and once per 30 s in total.
  - NodeInfo requests when you DM a keyless node: once per 60 s per node.
  - Our own NodeInfo broadcasts: at most one per 30 s, plus one every 3 h.
  - ACKs for retransmissions of a DM we already acknowledged: at most 2 per DM.
- **Transmission pacing**: one frame at a time, at least 1.5 s apart, a 6-frame queue, and the region's duty-cycle airtime budget.
- **Auto-greet** is off at boot, greets each node at most once (persisted), at most one greeting per 30 s, with a queue of 10.
- **Duplicate suppression** of the last 64 `(from, id)` pairs.
- **Bounded storage**: fixed-size NVS blobs, at most 50 saved peers (slots reused), at most 500 greeted IDs. A flood of fake nodes cannot grow storage.

### Identity and keys

- **First key wins**: the first public key received for a node number is kept, and a later NodeInfo with a different (or missing) key is ignored. Keys of DM peers are persisted (up to 50), so this survives reboots. The persisted keys are also kept in a separate table in RAM, so a flood of fake nodes that pushes a saved peer out of the 100-node list cannot replace its key: the saved key is still used for DMs and still checked against every NodeInfo for that node number. When the list is full, keyless nodes that are not saved peers are dropped first.
- **Legacy DMs are rejected**: a text DM encrypted only with the public channel key is dropped, so a DM shown in a conversation was always PKI-authenticated for the sender's stored key.

### Supply chain

- Libraries and the platform are **pinned to exact versions** in `platformio.ini`. The crypto library is vendored (`lib/crypto/`, MIT).
- GitHub Actions are **pinned to full commit SHAs**, workflows run with `contents: read`, checkout does not keep credentials, and Dependabot proposes action updates.
- Release images come with **`bin/SHA256SUMS`**. CI re-verifies them on every run.

## What is NOT protected

- **The private key is readable with physical access.** Flash encryption and secure boot are off, because the firmware is installed through M5Launcher, which cannot use them. Anyone who can dump the flash can read the key pair from NVS (key `idkey`) and impersonate the node in DMs. Saved peers, the greeted list and settings are also readable. If the device is lost, treat its identity as compromised.
- **The public channel is readable by anyone.** It uses Meshtastic's well-known default key by design, so every message you send on the Canal screen, and every NodeInfo (names, public key, hardware model), is public.
- **No packet signatures (XEdDSA) in v1.** Broadcast frames, including channel texts and NodeInfo, are not authenticated. Anyone can send a channel message or NodeInfo claiming any node number. Meshtastic's default policy also accepts unsigned packets.
- **Impersonation before the key is known.** Node numbers are not bound to keys. Until we have stored a node's real key, an attacker can send a NodeInfo with that node number and their own key, and first-key-wins then keeps the attacker's key. DMs "to" that node would go to the attacker, and the real node's DMs would be rejected. This is an upstream protocol property.
- **A saved peer's key cannot be replaced.** The flip side of first-key-wins: if a contact factory-resets or erases their NVS and gets a new key, this device keeps rejecting it, and DMs with that contact stop working. v1 has no action to forget a single saved key (see the [roadmap](roadmap.md)).
- **Metadata is visible.** Node numbers, names, hardware model, packet timing, sizes and who sends to whom (the header `to`/`from` is not encrypted) are visible to any receiver. Your node ID is derived from the device's MAC address.
- **DM content is end-to-end encrypted, but delivery is not guaranteed.** There is no retry, and an attacker can jam the frequency or drop ACKs.
- **Physical-layer denial of service** (jamming) is out of scope.
- **Chat history** is in RAM only and disappears at power-off. It is not protected against someone holding the unlocked device.

## Verifying a release

The release images and their checksums are in [`bin/`](../../bin/):

```sh
cd bin
sha256sum -c SHA256SUMS
```

Both lines must say `OK`. On Windows without `sha256sum`:

```bat
certutil -hashfile M5-Mesh-Hello.bin SHA256
```

Compare the result with the line for that file in `SHA256SUMS`.

The checksums prove that the files match what was committed to the repository. To check that a binary matches the source, rebuild it (`python -m platformio run -e cardputer`) with the pinned toolchain and compare. CI also builds the firmware from source on every push and publishes the CI-built `firmware.bin` with its own checksum as a workflow artifact. Builds are not guaranteed to be bit-for-bit reproducible across machines.

## Reporting a vulnerability

Use GitHub private vulnerability reporting, as described in [SECURITY.md](../../SECURITY.md). Include the version, region and a reproducing frame (hex dump) if possible.
