# Security policy

## Supported versions

| Version | Supported |
|---|---|
| 0.1.x | Yes |
| < 0.1 | No |

Only the latest 0.1.x release receives security fixes.

## Reporting a vulnerability

Please report vulnerabilities **privately**. Do not open a public issue, pull
request or discussion for a security problem.

1. Open the repository on GitHub, go to the **Security** tab and choose
   **Report a vulnerability** (GitHub private vulnerability reporting).
2. Include the affected version or commit, the hardware (Cardputer-Adv + Cap
   LoRa-1262), the region setting, and steps or a frame capture that reproduce
   the problem. A proof-of-concept frame as a hex dump is ideal.
3. If you cannot use GitHub private reporting, open a public issue that says
   only "security contact requested", without details, and a maintainer will
   reach out.

## Scope

In scope:

- **Remote attacks over LoRa**: any frame received by the radio is untrusted.
  Memory-safety bugs (out-of-bounds read/write, overflow, crash, hang) in the
  header parser, protobuf decoder, decryption or application logic; ways to
  bypass PKI authentication of direct messages; key confusion or replacement
  of a peer's stored public key (first-key-wins).
- **Malicious Meshtastic nodes**: spoofed NodeInfo, flooding that defeats rate
  limits, duty-cycle budget or queue caps, persistent storage exhaustion.
- **Secrets handling**: leaking the private key or derived shared keys through
  the screen, serial output or logs; weak key generation.
- **Supply chain**: the build configuration, pinned dependencies, the vendored
  `lib/crypto`, the CI workflows and the release images in `bin/` (verify them
  with `bin/SHA256SUMS`).

Out of scope (known and documented limitations of v1):

- Reading the private key with physical access to the device: flash
  encryption is off because the firmware is installed through M5Launcher.
- Reading the public LongFast channel: it uses the well-known default key by
  design, exactly like Meshtastic.
- Missing packet signatures (XEdDSA) and other protocol-level limitations
  shared with upstream Meshtastic.
- Radio jamming or other physical-layer denial of service.

## What to expect

- Acknowledgement within **7 days**.
- An initial assessment (accepted, needs more information, or out of scope)
  within **14 days**.
- For accepted issues, a fix and a new 0.1.x release as soon as practical,
  normally within **90 days**, followed by a GitHub security advisory. Reporters
  are credited unless they ask not to be.

This is a volunteer project; timelines are best effort.

## More information

The threat model, what is and is not protected, and how to verify a release are
described in [docs/en/security.md](docs/en/security.md).
