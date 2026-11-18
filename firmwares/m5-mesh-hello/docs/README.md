# M5-Mesh-Hello documentation

**English** | [Português](pt-BR/) | [Español](es/) | [Русский](ru/) | [日本語](ja/)

The documentation exists in five languages with identical file names and structure. **English is the source of truth.** The translations carry a note saying so, and if in doubt the English version prevails.

| Language | Folder | README |
|---|---|---|
| English | [`en/`](en/) | [README.md](../README.md) |
| Português (Brasil) | [`pt-BR/`](pt-BR/) | [README.pt-BR.md](../README.pt-BR.md) |
| Español | [`es/`](es/) | [README.es.md](../README.es.md) |
| Русский | [`ru/`](ru/) | [README.ru.md](../README.ru.md) |
| 日本語 | [`ja/`](ja/) | [README.ja.md](../README.ja.md) |

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

The same documents in each language:

| Document | English | Português | Español | Русский | 日本語 |
|---|---|---|---|---|---|
| User guide | [en](en/user-guide.md) | [pt-BR](pt-BR/user-guide.md) | [es](es/user-guide.md) | [ru](ru/user-guide.md) | [ja](ja/user-guide.md) |
| Hardware | [en](en/hardware.md) | [pt-BR](pt-BR/hardware.md) | [es](es/hardware.md) | [ru](ru/hardware.md) | [ja](ja/hardware.md) |
| Regions and frequencies | [en](en/frequencies.md) | [pt-BR](pt-BR/frequencies.md) | [es](es/frequencies.md) | [ru](ru/frequencies.md) | [ja](ja/frequencies.md) |
| Protocol reference | [en](en/protocol.md) | [pt-BR](pt-BR/protocol.md) | [es](es/protocol.md) | [ru](ru/protocol.md) | [ja](ja/protocol.md) |
| Architecture | [en](en/architecture.md) | [pt-BR](pt-BR/architecture.md) | [es](es/architecture.md) | [ru](ru/architecture.md) | [ja](ja/architecture.md) |
| Development | [en](en/development.md) | [pt-BR](pt-BR/development.md) | [es](es/development.md) | [ru](ru/development.md) | [ja](ja/development.md) |
| Security | [en](en/security.md) | [pt-BR](pt-BR/security.md) | [es](es/security.md) | [ru](ru/security.md) | [ja](ja/security.md) |
| Roadmap | [en](en/roadmap.md) | [pt-BR](pt-BR/roadmap.md) | [es](es/roadmap.md) | [ru](ru/roadmap.md) | [ja](ja/roadmap.md) |

The region table in [en/frequencies.md](en/frequencies.md) is checked against the code in CI; the translated copies are not. `tools/check_links.py` checks every relative link and anchor in all languages.

Project files (English only): [README](../README.md), [CONTRIBUTING](../CONTRIBUTING.md), [SECURITY](../SECURITY.md), [CHANGELOG](../CHANGELOG.md), [LICENSE](../LICENSE) (GPL-3.0-or-later), [THIRD_PARTY_NOTICES](../THIRD_PARTY_NOTICES.md).

## Historical records

These documents record the original design and implementation plan. They are kept as written, in Portuguese, and are not translated or updated. Where they differ from the current code, the documents above and the code prevail.

- [design.md](design.md): the approved design (goals, verified hardware and protocol facts, antenna analysis).
- [plano.md](plano.md): the implementation plan, task by task.
