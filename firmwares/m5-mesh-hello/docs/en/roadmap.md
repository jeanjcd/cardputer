# Roadmap and known limitations

v0.1.0 is a deliberately small Meshtastic node. This page lists what it does not do yet, ideas for a v2, and the known limitations of v1. None of the ideas is promised. Contributions are welcome (see [CONTRIBUTING.md](../../CONTRIBUTING.md)).

## Ideas for v2

| Idea | Notes |
|---|---|
| **GPS / position** | Use the Cap's ATGM336H GNSS (UART, ESP RX G15 / TX G13) to send `POSITION_APP` and show distances to other nodes. LoRa TX interferes with the GNSS receiver, so the two must be scheduled carefully. |
| **More channels and custom PSKs** | Secondary channels, custom keys, AES-256-CTR channels (32-byte PSKs), channel names other than `LongFast`, and channel URLs/QR import. Today only the default primary channel is supported, and `ctrCrypt` is AES-128 only. |
| **Other presets** | LongSlow, MediumFast, ShortFast and the other modem presets (the frequency formula already takes the channel name and bandwidth). |
| **XEdDSA signing and verification** | Sign our packets and verify signed packets from others, as newer Meshtastic firmware does. |
| **Relay / router role** | Optionally rebroadcast other nodes' packets (hop limit handling, next-hop routing) instead of `CLIENT_MUTE`. |
| **DM retry** | Retransmit a DM a few times when no ACK arrives, as Meshtastic's reliable router does. |
| **UTF-8 font** | A display font with Portuguese/Spanish/Russian glyphs and emoji, plus non-ASCII input on the keyboard. |
| **Persistent chat history** | Keep messages across reboots (SD card or NVS, bounded). |
| **Non-blocking TX** | Interrupt-driven transmit so that the UI stays responsive during the up-to-2.2 s airtime. |
| **Telemetry / traceroute** | Show other nodes' telemetry, and answer or send traceroutes. |
| **Battery indicator** | Read the battery ADC, which v1 does not use. It needs care: the RF entropy source used for key generation shares hardware with the ADC, so the ADC must be set up after key generation. |
| **MeshCore?** | Support for the MeshCore protocol as an alternative mesh. To be explored. |
| **Identity migration** | A versioned key record migration so that a future format change never regenerates the key. |
| **Forget node key** | An action to forget one saved peer's key, so that a contact who factory-reset (new key) can be accepted again. Today a saved key can never be replaced. |

## Known limitations of v1

| Limitation | Details |
|---|---|
| **199-byte text** | Messages, greetings and the input line are limited to 199 bytes of UTF-8. |
| **Display glyphs** | The CP437 font lacks some accented letters (shown unaccented, e.g. `ã → a`) and all emoji (shown as `?`). Sent text is never altered. See the [user guide](user-guide.md#display-limitations). |
| **ASCII-only keyboard input** | Only printable ASCII can be typed. You cannot type the `` ` `` character (it is Esc). |
| **Blocking TX freezes the UI** | While a frame is transmitted (up to about 2.2 s for 255 bytes), keys are not read and the screen does not update. |
| **No key repeat** | Holding a key does not repeat it. |
| **Text-screen scrolling** | History scrolls with `Fn`+`;` / `Fn`+`.`, one line per press. |
| **No DM retry** | A DM without an ACK stays at `.`. |
| **Messages in RAM only** | 30 per conversation, 96 in total, lost at power-off. |
| **100 nodes in RAM** | When the list is full, the least recently heard keyless node that is not a saved peer is dropped first, otherwise the least recently heard node. The keys of saved DM peers (up to 50) are kept in a separate table, so they survive being dropped from the list. |
| **Saved peer keys cannot be replaced** | If a saved contact factory-resets and gets a new key, it is rejected and DMs with them stop working. There is no "forget node key" action yet. See the [user guide FAQ](user-guide.md#faq). |
| **Greeted set of 500** | After 500 greeted nodes, auto-greet stops greeting new nodes, so that nobody is greeted twice. |
| **Single channel and preset** | Default primary channel only, LongFast only. |
| **No signatures** | XEdDSA is not implemented. See [security.md](security.md#what-is-not-protected). |
| **No phone app, Wi-Fi, BLE or MQTT** | The device is configured only from its own keyboard. |
| **Physical key extraction** | Flash encryption is off under M5Launcher, so the private key in NVS is readable with physical access. |
| **Region change lost within 2 s** | A region change is saved 2 s after the last change, or when leaving Config. Switching off sooner loses it. | Nothing is transmitted until it is saved.
| **No auto-detection of the Cap at runtime** | If the Cap is missing at boot (`SEM CAP LoRa`), the firmware has to be restarted after fixing it. |
| **Experimental regions** | IN, NZ_865, NP_865, KZ_863, JP, TW, TH are outside the Cap's rated 868–923 MHz, and their performance is unmeasured. |
| **End-to-end interoperability not yet field-verified** | Radio init, TX, identity persistence and the UI are verified on the device. Exchanging DMs and channel messages with other Meshtastic nodes is implemented and covered by tests against upstream-compatible vectors, but has not yet been confirmed in the field. |
