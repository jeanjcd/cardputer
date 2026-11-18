# M5-Mesh-Hello documentation

**English** | [Português](pt-BR/) | [Español](es/) | [Русский](ru/)

The documentation exists in four languages with identical file names and structure. **English is the source of truth.** The translations carry a note saying so, and if in doubt the English version prevails.

| Language | Folder |
|---|---|
| English | [`en/`](en/) |
| Português (Brasil) | [`pt-BR/`](pt-BR/) |
| Español | [`es/`](es/) |
| Русский | [`ru/`](ru/) |

## Documents

| Document | Contents |
|---|---|
| [User guide](en/user-guide.md) | Screens, keys, Config options, auto-greet, markers, when the device transmits, why no nodes appear, troubleshooting, FAQ |
| [Hardware](en/hardware.md) | Cardputer-Adv + Cap LoRa-1262 pin map, antenna switch, TCXO, shared buses, antenna and range, installing with M5Launcher |
| [Regions and frequencies](en/frequencies.md) | Supported and unsupported regions, frequency formula, power clamp, duty cycle and airtime, Brazil, antennas per band |
| [Protocol reference](en/protocol.md) | Wire format, channel and PKI encryption, protobuf subset, ACK/NAK, first-key-wins |
| [Architecture](en/architecture.md) | Module map, data flow, NVS persistence, RAM budget, limits |
| [Development](en/development.md) | Toolchain, build, native and sanitizer tests, fuzzing, CI, releases |
| [Security](en/security.md) | Threat model, protections, what is not protected, verifying a release |
| [Roadmap](en/roadmap.md) | v2 ideas and known limitations |

Project files (English only): [README](../README.md), [CONTRIBUTING](../CONTRIBUTING.md), [SECURITY](../SECURITY.md), [CHANGELOG](../CHANGELOG.md), [LICENSE](../LICENSE) (GPL-3.0-or-later), [THIRD_PARTY_NOTICES](../THIRD_PARTY_NOTICES.md).

## Historical records

These documents record the original design and implementation plan. They are kept as written, in Portuguese, and are not translated or updated. Where they differ from the current code, the documents above and the code prevail.

- [design.md](design.md): the approved design (goals, verified hardware and protocol facts, antenna analysis).
- [plano.md](plano.md): the implementation plan, task by task.
