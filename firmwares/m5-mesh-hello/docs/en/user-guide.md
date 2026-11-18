# User guide

This guide covers M5-Mesh-Hello v0.1.0 on the M5Stack **Cardputer-Adv** with the **Cap LoRa-1262**. It explains every screen, key, setting and status message.

The on-screen text is in Portuguese. This guide quotes it exactly as displayed, followed by the English meaning in parentheses. The display has no accented glyphs for some letters, so a few strings look unaccented on the device (see [Display limitations](#display-limitations)).

Contents:

- [Before you start](#before-you-start)
- [First boot](#first-boot)
- [The screen layout](#the-screen-layout)
- [Keys](#keys)
- [Nós (Nodes) screen](#nós-nodes-screen)
- [Conversa (Chat) screen](#conversa-chat-screen)
- [Canal (Channel) screen](#canal-channel-screen)
- [Config screen](#config-screen)
- [Auto-greet](#auto-greet)
- [Message markers](#message-markers)
- [When the device listens and when it transmits](#when-the-device-listens-and-when-it-transmits)
- [Why no nodes appear](#why-no-nodes-appear)
- [Troubleshooting: every status and message](#troubleshooting-every-status-and-message)
- [Display limitations](#display-limitations)
- [Limits](#limits)
- [FAQ](#faq)

## Before you start

1. **Screw the antenna onto the Cap before powering on.** The firmware transmits a NodeInfo announcement within a second of booting. Transmitting without an antenna can damage the radio.
2. Install the firmware with M5Launcher (see the [README quick start](../../README.md#quick-start) and [hardware.md](hardware.md#installing-with-m5launcher)).
3. Know which region the Meshtastic users near you use. In Brazil the community uses **ANZ** (the default). See [frequencies.md](frequencies.md).

## First boot

- On the very first boot the device generates an X25519 key pair from the hardware random number generator and stores it in flash (NVS). The key pair **never changes** afterwards. Meshtastic nodes remember the first public key they hear for each node ("first key wins"), so a new key would stop them from sending you direct messages.
- Your **node ID** (`!xxxxxxxx`) comes from the chip's factory MAC address and is also permanent.
- The default names are **4stro-031** (long) and **4S31** (short). You can change them on the Config screen.
- The device starts in the region saved last time (ANZ on a fresh install), broadcasts its NodeInfo asking neighbours for theirs, and then listens.
- **Auto-greet is OFF** after every boot.

## The screen layout

The 240 × 135 display shows 40 columns of text (strings appear as the font renders them, for example `auto-saudaçao`):

```
+----------------------------------------+
|ANZ 919.875 | AUTO:OFF | rx 42 | -97dBm |  <- status bar (or a yellow toast)
| Nós*  | Conversa |  Canal  |  Config   |  <- tabs (* = something unread)
|BASE Base Stati TBEAM          -97  2m* |
|JOAO Joao Movel HELTEC_V3     -112 15s +|  <- one node per line
|a3f1 !9c2ea3f1  -                -   -  |
|                                        |
|Enter:conversa A:auto-saudaçao Tab:tela |  <- key hint / text input
+----------------------------------------+
```

### Status bar

`<region> <frequency> | AUTO:ON or AUTO:OFF | rx <frames received> | <RSSI of the last frame>`

- **Region and frequency** in MHz, for example `ANZ 919.875`. If the radio is not working, this field shows `SEM CAP LoRa` or `RADIO ERRO` instead (see [Troubleshooting](#troubleshooting-every-status-and-message)).
- **`AUTO:ON` / `AUTO:OFF`**: auto-greet state.
- **`rx N`**: LoRa frames received since boot. It counts every frame the radio decodes, including ones the firmware cannot read (other channels, other people's direct messages). If it goes up, the radio is hearing something.
- **RSSI** of the last received frame, for example `-97dBm`. It shows `-dBm` until the first frame arrives, and `duty limit` while the region's airtime budget holds transmissions back.
- If the line does not fit in 40 columns, the spaces around `|` are dropped.
- The bar turns **red** when the Cap is missing, the radio failed, or the duty limit is active.

**Toasts** are short messages that replace the status bar for 2.5 seconds on a yellow background, for example `Config salva` ("settings saved").

### Tabs

`Nós` (Nodes), `Conversa` (Chat), `Canal` (Channel), `Config`. The active tab is highlighted. `Nós` becomes `Nós*` when any node has unread direct messages, and `Canal` becomes `Canal*` when there are unread public-channel messages.

## Keys

| Key | Where | Action |
|---|---|---|
| `Tab` | everywhere | Next screen: Nós → Conversa → Canal → Config → Nós |
| `;` | Nós, Config | Move the selection up (the `;` key has the up arrow) |
| `.` | Nós, Config | Move the selection down (the `.` key has the down arrow) |
| `Enter` | Nós | Open the conversation with the selected node |
| `Enter` | Conversa, Canal | Send the typed text |
| `Enter` | Config | Edit the selected item, or run it (`Região`, `Enviar NodeInfo`); while editing: save |
| `` ` `` (Esc) | Conversa, Canal, Config | Go back to Nós; while editing a Config item: cancel the edit |
| `Fn` + `;` | Conversa, Canal | Scroll the history up (older messages) |
| `Fn` + `.` | Conversa, Canal | Scroll the history down (newer messages) |
| `del` (Backspace) | text input, Config editing | Delete the last character |
| `A` (or `a`) | Nós only | Turn auto-greet on or off |
| letters, digits, symbols | text input | Type (printable ASCII only) |

Notes:

- Keys do not auto-repeat. Press once per character or step.
- On the text screens, `;` and `.` without `Fn` are typed as text, and you cannot type the `` ` `` character because it is the Esc key. Other `Fn` combinations are ignored.
- While you edit `Potência` (power), only digits are accepted.

## Nós (Nodes) screen

The list of nodes heard since boot, plus the peers saved from earlier sessions. The most recently heard node is at the top. Ten rows are visible, and the list scrolls with the selection.

Each row, left to right:

| Column | Width | Content |
|---|---|---|
| Short name | 4 | The node's short name, or the last 4 hex digits of its node number if its NodeInfo has not arrived yet |
| Long name | 10 | The long name (cut to 10 characters), or `!xxxxxxxx` if unknown |
| Hardware model | 13 | For example `TBEAM`, `HELTEC_V3`, `CARDPUTER_ADV`; `HW#<n>` for models not in the firmware's table; `-` if nothing is known yet |
| RSSI | 4 | Signal strength of the last frame heard from it, in dBm; `-` if not heard since boot |
| Age | 3 | Time since it was last heard: `15s`, `2m`, `1h`, `3d`; `-` if not heard since boot |
| `*` | 1 | **Unread** direct messages from this node |
| `+` | 1 | Already **greeted** (by auto-greet or by sending the greeting by hand); never auto-greeted again |

Colours: **yellow** = unread messages; **white** = its public key is known (you can send it a DM right away); **dark grey** = no key yet (a DM to it waits for the key, see the `?` marker).

Peers loaded from flash at boot show `-` for RSSI and age until they are heard again.

When the list is empty it shows `Nenhum nó ouvido ainda.` ("No node heard yet.") and `Aguarde pacotes da rede Meshtastic.` ("Wait for packets from the Meshtastic network."). See [Why no nodes appear](#why-no-nodes-appear).

Keys: `;`/`.` select, `Enter` opens the chat, `A` toggles auto-greet. The hint line reads `Enter:conversa A:auto-saudação Tab:tela` ("Enter: chat, A: auto-greeting, Tab: screen").

## Conversa (Chat) screen

The direct-message (DM) conversation with one node. Open it with `Enter` on the Nós screen.

- With no node chosen it shows `Escolha um nó na aba Nós (Enter).` ("Choose a node on the Nós tab (Enter).").
- **Header line**: `!<node id> <long name> <model> <chip family>`, for example `!9c2ea3f1 Base Station TBEAM ESP32`. It ends with `(sem chave)` ("no key") in orange if the node's public key is not known yet. If the node has dropped out of the node list (more than 100 nodes heard), the header shows `!<node id> (fora da lista)` ("not in the list").
- **History**: 9 lines, oldest at the top, long messages wrapped. Messages you received start with `<`. Your own messages start with a state marker and `>` (see [Message markers](#message-markers)). Received text is white, your text is cyan, and failed messages are red. A yellow `^` in the top-right corner means you have scrolled up. An empty conversation shows `(sem mensagens)` ("no messages").
- **Input line** at the bottom: `> text_`. When you open a chat, it is **pre-filled with your greeting text** as a suggestion. Delete it with `del` or just send it.
- `Enter` sends the text as an encrypted DM (PKI) and clears the input. `` ` `` goes back to Nós. `Fn`+`;` / `Fn`+`.` scroll.

Sending the greeting exactly as pre-filled counts as greeting that node: it is marked `+` and auto-greet will never greet it.

Opening a chat clears that node's unread mark.

## Canal (Channel) screen

The public channel: the default Meshtastic primary channel (`LongFast`, default key), which every node with default settings can read.

- 10 lines of history. Received messages are stored as `<short name>: <text>`, or `<last 4 hex digits of the node id>: <text>` if the sender's name is unknown.
- `Enter` sends the input line to everybody on the channel. Channel messages are not acknowledged, so your messages stay at `.` once sent.
- `` ` `` goes back to Nós. `Fn`+`;` / `Fn`+`.` scroll. Viewing the screen clears the `Canal*` mark.

## Config screen

Six items. Select one with `;`/`.` and press `Enter`. The hint line reads `Enter:editar/acionar  ;/. move` ("Enter: edit/run, ;/. move"). While you edit, the prompt reads `<item>: Enter salva, ` cancela` ("Enter saves, ` cancels") above an input line.

| Item (on screen) | Meaning | What `Enter` does |
|---|---|---|
| `Região` | Region | Switches to the next region in the list (wraps around) |
| `Potência` | TX power | Edit the power in dBm |
| `Nome longo` | Long name | Edit the long name |
| `Nome curto` | Short name | Edit the short name |
| `Saudação` | Greeting | Edit the greeting text |
| `Enviar NodeInfo` | Send NodeInfo | Broadcasts your NodeInfo now |

### Região (Region)

Shows the region name and the LongFast frequency, for example `ANZ 919.875`. Each `Enter` moves to the next region in the order of the [region table](frequencies.md#supported-regions) and **retunes the radio immediately**. It also re-applies the region's power limit. The new region is saved 2 seconds after the last change, or when you leave the Config screen (toast `Config salva`). If you switch off within those 2 seconds, the change is lost.

- Experimental regions show `(exp)`, for example `IN 865.875 (exp)`. When you select one, the toast `Fora da faixa do Cap (exp)` ("outside the Cap's band (experimental)") appears. While one is selected, the screen shows the warning `! Fora de 868-923 MHz do Cap (exp)` ("outside the Cap's 868–923 MHz"). These frequencies are outside the Cap's rated band, so range may be worse. Use one only if it is your country's region.
- In regions with a duty-cycle limit (EU_868, UA_868, TH), an extra line shows the airtime used in the last hour: `Ar 1h: 12s de 360s (duty 10%)` ("airtime 1 h: 12 s of 360 s").
- All nodes must be in the same region to hear each other. See [frequencies.md](frequencies.md).

### Potência (TX power)

The transmit power you want, 2–22 dBm (default 22). Type 1 or 2 digits and press `Enter`. An invalid value shows the toast `Potência: 2 a 22 dBm` ("power: 2 to 22 dBm") and keeps the editor open.

The radio uses the **effective power** = min(your setting, region limit, 22 dBm). When the effective power is lower, the item shows both, for example `22 dBm (efetiva 14)` ("effective 14"). The change applies immediately and is saved (`Config salva`).

### Nome longo / Nome curto (long / short name)

Your node's names as other Meshtastic users see them. Long name: 1–24 bytes. Short name: 1–4 bytes (ASCII letters/digits recommended). Only ASCII can be typed on the keyboard. Invalid input shows `Nome longo: 1 a 24 bytes` or `Nome curto: 1 a 4 letras` ("short name: 1 to 4 letters"; the limit is actually 1–4 bytes).

After you save a name, the device **broadcasts a new NodeInfo** so other nodes learn it. Broadcasts are spaced at least 30 s apart; if the last one was less than 30 s ago, the new one goes out when the 30 s are up. **Your node ID and your key stay the same**: other nodes keep recognising you and can still send you DMs. Only the displayed name changes. Other nodes update the name when they receive the new NodeInfo.

### Saudação (greeting)

The text used by auto-greet and pre-filled in every new chat, up to 199 bytes.

- By default it is composed from your long name: `Saudações de <long name>! 👋 (Cardputer-Adv)` ("Greetings from <long name>! 👋"). It follows your long name when you rename the node.
- Edit it to set your own text, which is then sent exactly as you typed it.
- Save an **empty** text (or the exact default text) to go back to the composed default.
- Invalid text shows `Saudação inválida` ("invalid greeting").

The editor shows the greeting with display substitutions (for example `?` for the emoji), but the stored text is unchanged unless you edit it. If you edit the default greeting, the emoji is kept as long as you do not delete it.

### Enviar NodeInfo (send NodeInfo)

Broadcasts your NodeInfo (names and public key) on the public channel right away. Use it so that nearby nodes learn your key, for example before someone tries to send you a DM.

Broadcasts are limited to **one every 30 seconds**. This limit is shared with the boot announcement and with name changes. If you press `Enter` sooner, the toast `NodeInfo em Ns` ("NodeInfo in N s") appears and the broadcast goes out automatically when the time is up. Pressing again does not queue a second one.

### Identity line

Below the items, in green: `ID !xxxxxxxx  chave aabbccdd`.

- `ID !xxxxxxxx` is your node ID. This is how others see you in their node lists.
- `chave aabbccdd` ("key") is a fingerprint of your **public** key: its first 4 bytes in hex. It lets you check that your key did not change after a reboot or update. The private key is never shown.
- If the key could not be saved to flash, this area shows `ERRO: chave não gravada na NVS!` ("ERROR: key not saved to NVS!") in red. See [Troubleshooting](#troubleshooting-every-status-and-message).

Leaving the Config screen (`Tab` or `` ` ``) saves a pending region change. Pressing `Tab` while editing an item discards the edit.

## Auto-greet

Auto-greet sends your greeting as a DM to each new node, **once per node, ever**.

- **Turn it on or off with `A` on the Nós screen.** The status bar shows `AUTO:ON` or `AUTO:OFF`. There is no Config item for it.
- It is **OFF after every boot**, by design, so the device never starts greeting people on its own.
- It only greets nodes whose **public key is known**, that is, nodes whose NodeInfo with a key has been received. A DM needs the key. Nodes shown in grey are not greeted until their key arrives.
- When you turn it on, the nodes with a key already heard since boot are queued too, newest first. After that, a node is queued when its NodeInfo arrives.
- At most **one greeting every 30 seconds**. The very first greeting is not delayed.
- The queue holds at most **10 nodes**. Nodes that arrive while it is full are not queued at that moment. They are queued again the next time their NodeInfo is heard (nodes typically re-announce every 3 hours).
- **Never twice**: the greeted list is stored in flash (up to 500 node IDs) and survives reboots and updates. Sending the exact greeting by hand from the chat screen also adds the node to the list. When the list reaches 500 nodes, auto-greet stops greeting new nodes rather than forget old ones.
- Greeted nodes show `+` in the node list.
- Turning auto-greet off clears the queue. Nodes already greeted stay greeted.

Greetings appear in each node's conversation like any DM you send, with the usual [markers](#message-markers).

## Message markers

Your own messages start with a state marker followed by `>`. Received messages start with `<`.

| Marker | State | Meaning |
|---|---|---|
| `~` | queued | Waiting in the transmit queue (or for the duty-cycle budget) |
| `.` | sent | The radio transmitted it. For a DM, the acknowledgement has not arrived (yet). Public channel messages stay `.` because the channel has no acknowledgements. |
| `v` | acknowledged | The recipient's ACK came back: the DM was delivered and decrypted |
| `?` | waiting for key | The recipient's public key is unknown. The DM is held until the key arrives (see below). |
| `x` | failed | The transmission failed (`Falha no TX`: the radio reported an error while sending, or was not ready), the recipient answered with an error (NAK), or the key was missing at send time |

**The `?` state.** A DM can only be encrypted with the recipient's public key, which arrives in its NodeInfo. When you send to a node without a key:

1. The message is stored with `?` and the toast `Aguardando chave do no` ("waiting for the node's key") appears.
2. The device sends **its own NodeInfo directly to that node, asking for the node's NodeInfo in return** (at most once per minute per node).
3. When the node's NodeInfo arrives, every `?` message to it changes to `~` and is sent.

If the node is out of range or does not answer, the message stays at `?`. Resending or pressing `Enviar NodeInfo` can help once the node is reachable.

**No automatic retry.** If a DM stays at `.` (no ACK), the firmware does not send it again. Type it again if needed.

## When the device listens and when it transmits

**The radio is always listening** unless it is transmitting one of its own frames. It never relays other nodes' packets: the device announces itself with the role `CLIENT_MUTE` and does not rebroadcast anything it hears. All its own frames use hop limit 3, so other nodes in the mesh may relay them, as Meshtastic normally does.

It transmits only in these cases:

| When | What | Limit |
|---|---|---|
| At boot | NodeInfo broadcast **with want_response** (neighbours answer with their NodeInfo) | Once per boot |
| Every 3 hours | NodeInfo broadcast | 3 h after the previous NodeInfo broadcast |
| You change the long or short name | NodeInfo broadcast | At most one every 30 s (shared with boot and "Enviar NodeInfo") |
| You select `Enviar NodeInfo` | NodeInfo broadcast | At most one every 30 s |
| You send a message | Public text (Canal) or encrypted DM with want_ack (Conversa) | When you press Enter |
| You send a DM to a node without a key | NodeInfo sent directly to that node with want_response | At most once per minute per node |
| Auto-greet is ON | Greeting DM to a newly keyed node | One every 30 s, once per node ever |
| You receive a DM that asks for an ACK | ACK to the sender (and again if the sender retransmits the same DM) | Once per DM received |
| A node asks for our NodeInfo (want_response) | NodeInfo to that node | At most once every 30 s in total **and** once every 12 h per node |
| A node sends us an encrypted DM but we lack its key | NAK (`PKI_UNKNOWN_PUBKEY`) plus our NodeInfo with want_response, so its next attempt can be decrypted | At most once every 30 s in total and once every 10 min per node |

Rules that apply to every transmission:

- **One frame at a time**, with at least **1.5 s** between the end of one frame and the start of the next. At most 6 frames wait in the transmit queue.
- Each LoRa frame occupies the air for about **0.4–2.2 s** depending on its length, typically about **1 s** (see [time on air](frequencies.md#time-on-air)).
- In duty-cycle regions, frames wait for the [airtime budget](frequencies.md#duty-cycle-and-the-airtime-budget).
- Wi-Fi and Bluetooth are never turned on.
- While a frame is being transmitted the device does not react to keys (up to about 2.2 s).

## Why no nodes appear

The node list fills up only when the device hears Meshtastic frames it can decode. If `rx` stays at 0, nothing is being received at all. If `rx` rises but the list stays empty, frames arrive that cannot be decoded (another channel or key) or that are other people's direct messages.

Check, in this order:

1. **Same region and frequency.** Every node must use the same region. In Brazil the community uses **ANZ (919.875 MHz)**, and the official region is **BR_902 (903.875 MHz)**. The two do not hear each other. Ask local users, or try the other region and wait. See [frequencies.md](frequencies.md).
2. **Same preset.** This firmware only speaks **LongFast**, the Meshtastic default. Nodes set to another preset (LongSlow, MediumFast, ShortFast...) use different modem settings and are invisible.
3. **Default public channel.** Node names, public keys and channel messages are decoded only on the **default primary channel** (`LongFast` with the default key `AQ==`). Nodes on a private channel or custom key are heard (`rx` rises), but nothing can be decoded. DMs between other nodes are encrypted for them, and you cannot read them either.
4. **Range, height and antenna.** LoRa range depends mostly on height and line of sight. Indoors, at ground level and surrounded by buildings, you may hear nobody. Go to a window, a balcony or a rooftop. Check that the antenna is screwed on and vertical. See [hardware.md](hardware.md#antenna-and-range).
5. **Give it time.** Meshtastic nodes are quiet most of the time. With default settings a node sends telemetry about every **30 minutes**, its NodeInfo about every **3 hours**, and its position periodically if it has GPS and position sharing on. The boot announcement asks neighbours to answer with their NodeInfo, but only nodes that hear it reply. Leave the device on for an hour or more in a good spot.
6. **Announce yourself.** Use `Enviar NodeInfo` so that others learn about you. Remember the 30 s limit.

A node appears in the list as soon as any decodable frame from it is heard: telemetry, position, text or NodeInfo. It shows its name and model, and turns white, only after its NodeInfo arrives.

## Troubleshooting: every status and message

### Status bar

| Shown | Meaning | What to do |
|---|---|---|
| `SEM CAP LoRa` (red) | "No LoRa Cap": the Cap's I/O expander did not answer at boot, or its antenna-switch output could not be set | Power off, check that the Cap is fully seated on the Cardputer-Adv connector, and power on again. The firmware does not retry while running. |
| `RADIO ERRO` (red) | "Radio error": the Cap was found, but the SX1262 did not initialise | Power-cycle. If it persists, the radio module may be faulty or another firmware left it in a bad state. Reflash and try again. |
| `duty limit` (red) | The region's airtime budget for the last hour is used up, and frames are waiting | Nothing is lost. Frames go out as the budget frees up, within the hour. Config shows `Ar 1h: ...`. Only EU_868, UA_868 and TH have a budget. |
| `-dBm` | No frame received since boot | See [Why no nodes appear](#why-no-nodes-appear). |

When the radio is not working, attempts to transmit also produce the toast `Falha no TX` (the radio is not ready).

### Toasts (yellow, 2.5 s)

| Toast | Meaning | What to do |
|---|---|---|
| `Config salva` | "Settings saved" to flash | Nothing. |
| `ERRO ao gravar config` | "Error saving settings": the flash write failed | The setting is active until reboot but may be lost. Try again. If it keeps failing, the NVS partition may be full or damaged. |
| `ERRO: chave nao gravada!` (at boot) | "Error: key not saved": a new key pair was generated but could not be stored | The key will change at the next boot, and nodes that stored the old key will reject the new one. Check flash/NVS (reinstall). Also shown on Config as `ERRO: chave não gravada na NVS!`. |
| `ERRO no radio` | "Radio error" while changing region or power | Power-cycle. The radio may not be on the shown frequency. |
| `Fora da faixa do Cap (exp)` | "Outside the Cap's band (experimental)": you selected an experimental region | Fine if it is your country's region. Expect reduced range. |
| `Falha no TX` | "TX failed": the radio reported an error while sending, or the radio is not ready (e.g. Cap not detected) | The message is marked `x`. Check the status bar for `SEM CAP LoRa` / `RADIO ERRO` and power-cycle. |
| `Texto longo demais` | "Text too long" (more than 199 bytes) | Shorten the message. The keyboard input already stops at 199 bytes. |
| `Fila de TX cheia` | "TX queue full" (6 frames waiting) when sending to the channel | Wait a few seconds and send again. In duty-cycle regions the budget may be holding frames. |
| `Fila de mensagens cheia` | "Message queue full": 10 DMs are already waiting (for example several `?` messages) | Wait for keys to arrive or for messages to go out. The DM was not stored. |
| `Aguardando chave do no` | "Waiting for the node's key": DM stored with `?`, NodeInfo request sent | Wait for the node's NodeInfo. See [the `?` state](#message-markers). |
| `NodeInfo em Ns` | "NodeInfo in N s": a NodeInfo broadcast was requested within 30 s of the previous one | Nothing. It goes out automatically. |
| `Potência: 2 a 22 dBm` | "Power: 2 to 22 dBm": invalid value | Type a number from 2 to 22. |
| `Nome longo: 1 a 24 bytes` | Long name empty or too long | Type 1–24 bytes. |
| `Nome curto: 1 a 4 letras` | "Short name: 1 to 4 letters": empty or too long (the limit is 1–4 bytes) | Type 1–4 bytes (ASCII letters/digits recommended). |
| `Saudação inválida` | "Invalid greeting" (for example control characters) | Retype it. Save an empty text to restore the default. |

### Other situations

| Situation | Explanation |
|---|---|
| A DM stays at `?` | The node's key has not arrived. See [the `?` state](#message-markers). |
| A DM stays at `.` | It was transmitted but no ACK came back. The recipient may be out of range, or the ACK was lost. There is no automatic retry. |
| A DM shows `x` right after `?` turned into `~` | The node's key disappeared before sending (for example the node was evicted from a full list). Send again. |
| Grey node without name | Frames from it were heard, but its NodeInfo was not. Wait, or open a chat and send a DM: that requests its NodeInfo. |
| Header shows `(fora da lista)` | That node was evicted from the 100-node list. Its messages remain until they are pushed out. |
| Keys ignored for a moment | A frame is being transmitted (up to about 2.2 s). |
| Messages gone after reboot | Chat history lives only in RAM. Names and keys of DM peers (up to 50), the greeted list and the settings are kept. |

## Display limitations

The screen uses a built-in 8-bit font (code page 437). Text is converted **only for display**:

- Letters that exist in the font are shown accented, for example `é ç ô á ü ñ`.
- Accented letters without a glyph are shown as the base letter, for example `ã → a`, `õ → o`, `À → A`, `Ê → E`.
- Curly quotes and dashes become `'`, `"` and `-`.
- Emoji and any other character without a glyph are shown as `?`. Invisible joiners and variation selectors are dropped.

**What is sent and stored is never changed.** Messages go out exactly as typed (UTF-8), and received messages are kept intact. The keyboard itself types only printable ASCII.

## Limits

| Item | Limit |
|---|---|
| Message text | **199 bytes** (UTF-8; accented letters take 2 bytes, emoji 4) |
| Long name / short name | 1–24 bytes / 1–4 bytes (ASCII letters/digits recommended) |
| Greeting | 199 bytes |
| Messages per conversation | 30 (oldest dropped); 96 in total across all conversations; RAM only |
| Nodes in the list | 100 (the least recently heard is dropped) |
| Peers saved in flash (name + key) | 50 (nodes you exchanged DMs with) |
| Greeted list | 500 nodes |
| DMs waiting (queued or `?`) | 10 |
| Transmit queue | 6 frames |
| Auto-greet queue | 10 nodes |
| TX power | 2–22 dBm, clamped to the region limit |
| DM retries | None |

## FAQ

**Does it pick up signals when someone transmits?**
Yes, provided the sender is on the same region/frequency and the LongFast preset and is within range. The radio listens all the time except during its own short transmissions. Every frame received increments `rx` and updates the RSSI on the status bar, even when the firmware cannot decode it. Only frames on the default public channel, plus DMs addressed to you, are decoded into nodes and messages.

**Is my device transmitting right now?**
Only in the situations listed in [When the device listens and when it transmits](#when-the-device-listens-and-when-it-transmits): the boot NodeInfo, every 3 hours, after a name change or `Enviar NodeInfo`, your own messages, auto-greet (if ON), ACKs, and rate-limited replies to NodeInfo requests. It never relays other people's traffic. There is no TX indicator. While a frame is being sent, the keyboard briefly stops responding. In duty-cycle regions, Config shows the airtime used in the last hour.

**What is region ANZ?**
ANZ is the Meshtastic region for Australia and New Zealand (915–928 MHz). The LongFast frequency there is 919.875 MHz. The Brazilian Meshtastic community uses ANZ, so it is this firmware's default. Brazil's official Meshtastic region is BR_902 (903.875 MHz). Use the region where you are; see [frequencies.md](frequencies.md#brazil).

**Where is auto-greet ON/OFF?**
On the **Nós** screen: press `A`. The status bar shows `AUTO:ON` or `AUTO:OFF`. It is always OFF after a reboot. There is no Config item for it.

**How do I force sending my NodeInfo?**
Config → `Enviar NodeInfo` → `Enter`. It goes out immediately, or after the remaining part of the 30 s limit (toast `NodeInfo em Ns`).

**Can I rename the node?**
Yes. Config → `Nome longo` (1–24 bytes) and `Nome curto` (1–4 bytes, ASCII letters/digits recommended). The new names are saved and announced with a fresh NodeInfo. Your node ID and key do not change, so other nodes still recognise you and can keep sending you DMs.

**Can I use it with my phone or the Meshtastic app?**
No. Bluetooth and Wi-Fi are off, and the device is configured only from its own keyboard. It can talk over LoRa to phones connected to other Meshtastic nodes.

**Will my messages be read by others?**
Public channel messages are readable by anyone with default Meshtastic settings, by design. DMs are end-to-end encrypted with the recipient's public key. See [security.md](security.md).

**Does it relay messages for other people?**
No. It is a `CLIENT_MUTE` node and never rebroadcasts other nodes' packets.

**Why does the greeting show a `?` on screen?**
The 👋 emoji has no glyph in the screen font. It is sent correctly. See [Display limitations](#display-limitations).
