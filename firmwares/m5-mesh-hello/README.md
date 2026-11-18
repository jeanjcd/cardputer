**English** | [Português](README.pt-BR.md) | [Español](README.es.md) | [Русский](README.ru.md)

# M5-Mesh-Hello

A small, self-contained [Meshtastic](https://meshtastic.org)-compatible node for the **M5Stack Cardputer-Adv** with the **Cap LoRa-1262**. It joins the Meshtastic network as an ordinary node (default name **4stro-031** / **4S31**), lists the nodes around you, chats with them through encrypted direct messages, reads and writes the public LongFast channel, and can greet every new node **exactly once**. No phone is needed.

This is not the official Meshtastic firmware. It is a minimal implementation written from scratch (RadioLib + protocol + cryptography) with its own keyboard UI, small enough to install next to other apps with M5Launcher.

## Features

- Meshtastic **LongFast** on the default public channel (AES-128-CTR, default key).
- **Encrypted direct messages** (PKI: X25519 + SHA-256 + AES-256-CCM) with delivery ACKs.
- **Node list** with names, hardware model, RSSI, last-heard time, unread and greeted markers.
- **Public channel** chat.
- **Auto-greet**: off at every boot. When on, it sends your greeting once per node, ever (persisted list), at most one every 30 s.
- **18 regions** (11 within the Cap's rated band plus 7 experimental), with the TX power clamped to each region's limit and an hourly airtime budget for duty-cycle regions.
- Editable long/short name, greeting text and TX power. The key pair is generated once from hardware entropy and never changes.
- Never relays other nodes' traffic (`CLIENT_MUTE`). Wi-Fi and Bluetooth stay off.
- Portable protocol core with native unit tests, a fuzz test, ASan/UBSan and CodeQL in CI.

## Screens

```
+----------------------------------------+   +----------------------------------------+
|ANZ 919.875 | AUTO:OFF | rx 42 | -97dBm |   |ANZ 919.875 | AUTO:ON | rx 57 | -101dBm |
| Nós*  | Conversa |  Canal  |  Config   |   |  Nós  | Conversa |  Canal  |  Config   |
|BASE Base Stati TBEAM          -97  2m* |   |!9c2ea3f1 Base Station TBEAM ESP32      |
|JOAO Joao Movel HELTEC_V3     -112 15s +|   |v>Saudaçoes de 4stro-031! ? (Cardputer-A|
|a3f1 !9c2ea3f1  -                -   -  |   |  dv)                                   |
|                                        |   |< Oi! Bem-vindo a rede                  |
|                                        |   |                                        |
|Enter:conversa A:auto-saudaçao Tab:tela |   |> _                                     |
+----------------------------------------+   +----------------------------------------+
        Nós (node list)                               Conversa (direct messages)

+----------------------------------------+   +----------------------------------------+
|ANZ 919.875 | AUTO:OFF | rx 42 | -97dBm |   |ANZ 919.875 | AUTO:OFF | rx 42 | -97dBm |
|  Nós  | Conversa |  Canal  |  Config   |   |  Nós  | Conversa |  Canal  |  Config   |
|< JOAO: bom dia pessoal                 |   |Regiao     ANZ 919.875                  |
|.>alguem em Sao Paulo?                  |   |Potência   22 dBm                       |
|                                        |   |Nome longo 4stro-031                    |
|                                        |   |Nome curto 4S31                         |
|                                        |   |Saudaçao   Saudaçoes de 4stro-031! ? (Ca|
|                                        |   |Enviar NodeInfo                         |
|                                        |   |ID !a1b2c3d4  chave 1a2b3c4d            |
|> _                                     |   |Enter:editar/acionar  ;/. move          |
+----------------------------------------+   +----------------------------------------+
        Canal (public channel)                        Config
```

`Tab` switches screens, `;`/`.` move, `Enter` opens/sends/edits, `` ` `` goes back, `A` on Nós toggles auto-greet. The full guide is in [docs/en/user-guide.md](docs/en/user-guide.md).

## Hardware

- **M5Stack Cardputer-Adv** (the original Cardputer is not supported).
- **M5Stack Cap LoRa-1262** (SX1262, rated 868–923 MHz, up to 22 dBm).
- **An antenna** on the Cap's RP-SMA connector. The kit antenna is tuned for 868 MHz and works in 902–928 MHz regions with roughly 0.5–2 dB loss. For those regions a 915 MHz antenna is better.

> **Always screw the antenna on before powering the device.** The firmware transmits its NodeInfo right after boot. Transmitting without an antenna can damage the radio.

Details: [docs/en/hardware.md](docs/en/hardware.md).

## Quick start

1. **Verify** the release images in [`bin/`](bin/):
   ```sh
   cd bin && sha256sum -c SHA256SUMS
   ```
2. Copy `bin/M5-Mesh-Hello.bin` to the microSD card and install it with **M5Launcher**. If an older version is installed, remove it first (**PMan → Remove**).
   (`bin/M5-Mesh-Hello-merged.bin` is a full-flash image for esptool at `0x0`. Flashing it **erases M5Launcher and every app installed through it**. Use it only if you do not use M5Launcher.)
3. Attach the antenna and start the app. On first boot it creates its key pair, announces itself and starts listening.
4. On **Config**, check the **region** (`Região`): every node must use the same one. The default is ANZ (919.875 MHz).
5. Wait for nodes to appear on **Nós**, select one, press `Enter` and send the pre-filled greeting. Or press `A` to turn on auto-greet.

## Regions

The default is **ANZ** (919.875 MHz), which the Brazilian Meshtastic community uses. Brazil's official Meshtastic region is BR_902 (903.875 MHz). The firmware also supports US, EU_868, RU, KR, UA_868, MY_919, SG_923, PH_868 and PH_915, plus the experimental IN, NZ_865, NP_865, KZ_863, JP, TW and TH (slightly outside the Cap's rated band). **Always use the region of the country you are in.** The full table, the frequency formula, power limits and duty cycle are in [docs/en/frequencies.md](docs/en/frequencies.md).

## Documentation

| | |
|---|---|
| [User guide](docs/en/user-guide.md) | Screens, keys, settings, auto-greet, troubleshooting, FAQ |
| [Hardware](docs/en/hardware.md) | Pin map, antenna, range, M5Launcher |
| [Regions and frequencies](docs/en/frequencies.md) | Regions, formula, power, duty cycle, Brazil |
| [Protocol](docs/en/protocol.md) | Wire format and crypto, for implementers |
| [Architecture](docs/en/architecture.md) | Modules, data flow, storage, limits |
| [Development](docs/en/development.md) | Build, tests, CI, releases |
| [Security](docs/en/security.md) | Threat model and limits |
| [Roadmap](docs/en/roadmap.md) | Ideas and known limitations |

All languages: [docs/README.md](docs/README.md). Contributing: [CONTRIBUTING.md](CONTRIBUTING.md). Security reports: [SECURITY.md](SECURITY.md). Changes: [CHANGELOG.md](CHANGELOG.md).

## Status

**v0.1.0.** Field-tested on a Cardputer-Adv + Cap LoRa-1262: radio initialisation, transmission, identity persistence across reboots and the whole UI (screens, navigation, auto-greet toggle) work. **End-to-end exchange with other Meshtastic nodes (receiving their NodeInfo, DMs and ACKs in the field) has not been verified yet**, because no node was within range during testing. The protocol is implemented after the upstream firmware and tested against reference vectors, but treat interoperability as unconfirmed until it has been reported working. Reports are welcome.

## License

[GPL-3.0-or-later](LICENSE). Third-party components and their licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
