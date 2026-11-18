# Regions and frequencies

Everything about the radio regions, frequencies, transmit power and duty cycle in M5-Mesh-Hello v0.1.0. All values come from [`src/mesh/region.h`](../../src/mesh/region.h) and [`src/mesh/region.cpp`](../../src/mesh/region.cpp). A native test, `test/test_docs_regions`, checks the table below against the code on every CI run (see [Keeping this table honest](#keeping-this-table-honest)).

> **Legal note.** Always use the region of the country where you are physically located. You are responsible for following your local radio regulations (frequency, power, duty cycle, antenna gain). This document is technical information, not legal advice.

## What a Meshtastic region is

A LoRa radio hears exactly one frequency at a time. For two Meshtastic nodes to talk to each other, they must use the same frequency and the same modem settings. Meshtastic defines a **region** for each regulatory area. A region is a frequency band (start and end), a duty-cycle limit, a power limit, and a channel spacing. A node takes its region, the channel name (here `LongFast`) and the bandwidth, and computes the actual centre frequency from them with a fixed formula. So every node in the same region with the default channel lands on the same frequency without any coordination.

M5-Mesh-Hello always uses the **LongFast** preset (bandwidth 250 kHz, spreading factor 11, coding rate 4/5, sync word `0x2B`, preamble 16 symbols, explicit header, CRC on) on the default primary channel named `LongFast`. The region is the only radio setting you choose. Change it on the **Config** screen (`Região`, Enter cycles through the list). The default is **ANZ**.

## Supported regions

These are the regions whose LongFast frequency falls inside the Cap LoRa-1262's rated band of 868–923 MHz. The **experimental** regions have a LongFast frequency in [860, 868) or (923, 925] MHz, just outside the Cap's rating. The Config screen lists them in this order.

<!-- regions:begin -->
| Region | Countries / use | Band (MHz) | LongFast (MHz) | Max power (dBm) | Duty cycle (%) | Experimental |
|---|---|---|---|---|---|---|
| ANZ | Australia, New Zealand; also used by the Brazilian community | 915.0 – 928.0 | 919.875 | 30 | 100 | no |
| BR_902 | Brazil (official Meshtastic region) | 902.0 – 907.5 | 903.875 | 30 | 100 | no |
| US | United States (and other countries using 902–928 MHz) | 902.0 – 928.0 | 906.875 | 30 | 100 | no |
| EU_868 | European Union (868 MHz SRD band) | 869.4 – 869.65 | 869.525 | 27 | 10 | no |
| RU | Russia | 868.7 – 869.2 | 869.075 | 20 | 100 | no |
| KR | South Korea | 920.0 – 923.0 | 922.875 | 23 | 100 | no |
| UA_868 | Ukraine | 868.0 – 868.6 | 868.375 | 14 | 1 | no |
| MY_919 | Malaysia | 919.0 – 924.0 | 922.875 | 27 | 100 | no |
| SG_923 | Singapore | 917.0 – 925.0 | 917.875 | 20 | 100 | no |
| PH_868 | Philippines (868 MHz) | 868.0 – 869.4 | 868.125 | 14 | 100 | no |
| PH_915 | Philippines (915 MHz) | 915.0 – 918.0 | 917.875 | 24 | 100 | no |
| IN | India | 865.0 – 867.0 | 865.875 | 30 | 100 | yes |
| NZ_865 | New Zealand (865 MHz) | 864.0 – 868.0 | 864.875 | 36 | 100 | yes |
| NP_865 | Nepal | 865.0 – 868.0 | 867.875 | 30 | 100 | yes |
| KZ_863 | Kazakhstan | 863.0 – 868.0 | 866.875 | 30 | 100 | yes |
| JP | Japan | 920.5 – 923.5 | 923.375 | 13 | 100 | yes |
| TW | Taiwan | 920.0 – 925.0 | 923.875 | 27 | 100 | yes |
| TH | Thailand | 920.0 – 925.0 | 923.875 | 27 | 10 | yes |
<!-- regions:end -->

Notes:

- A duty cycle of 100 % means the firmware applies no airtime limit.
- "Max power" is the region limit copied from Meshtastic. The Cap tops out at 22 dBm, so the power actually used is lower (see [Transmit power](#transmit-power)).
- All regions in this table have a channel spacing of 0 MHz.
- The band edges, duty cycles, power limits and spacing are copied from the `RDEF` table in Meshtastic's [`src/mesh/RadioInterface.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/RadioInterface.cpp). The country names are the usual meaning of each region code. Upstream may add or change regions in later versions.

## Regions not supported

| Region(s) | Band | Why not |
|---|---|---|
| 433 MHz regions (for example `EU_433`, `UA_433`, `MY_433`, `PH_433`, `KZ_433`) | 433 MHz | The Cap LoRa-1262's RF front end and antenna are built for 868–923 MHz. 433 MHz needs different hardware. |
| `CN` | 470–510 MHz | Same reason: far outside the Cap's band. |
| `LORA_24` | 2.4 GHz | Needs a different radio chip (SX128x). The SX1262 cannot tune there. |
| Any region whose LongFast frequency falls outside 860–925 MHz | — | Not tested and outside the margin accepted for experimental regions. |

## How the frequency is computed

The code follows Meshtastic's `RadioInterface::applyModemConfig()` exactly, including its single-precision float arithmetic ([`regionFreqMHz`](../../src/mesh/region.cpp)):

```
hash        = djb2("LongFast")                       // 130429955
numChannels = floor((freqEnd - freqStart) / (spacing + bw/1000))
slot        = hash % numChannels
freq        = freqStart + bw/2000 + slot * bw/1000   // MHz; bw in kHz (250)
```

- `djb2` is the classic string hash: start with `h = 5381`, then for every byte `h = h * 33 + byte`, as an unsigned 32-bit value. For `"LongFast"` it gives **130429955**.
- `bw/2000` is half the bandwidth in MHz (0.125 MHz), so the channel's centre sits half a bandwidth above the start of its slot.
- If `numChannels` were 0 (a band narrower than one channel), the code returns `freqStart + bw/2000`. No supported region hits this case.

### Worked example: ANZ

| Step | Value |
|---|---|
| Band | 915.0 – 928.0 MHz, spacing 0 |
| `numChannels` | floor(13.0 / (0 + 0.25)) = **52** |
| `slot` | 130429955 mod 52 = **19** |
| `freq` | 915.0 + 0.125 + 19 × 0.25 = **919.875 MHz** |

Same method for the other regions:

| Region | numChannels | slot | LongFast (MHz) |
|---|---|---|---|
| BR_902 | 22 | 7 | 903.875 |
| US | 104 | 19 | 906.875 |
| EU_868 | 1 | 0 | 869.525 |
| SG_923 | 32 | 3 | 917.875 |

### Channel hash

Every frame carries a one-byte channel hash in its header so that receivers can pick the right key. It is the XOR of all bytes of the channel name, XORed with all bytes of the key. For `"LongFast"` with the default key it is **`0x08`**. See [protocol.md](protocol.md#channel-encryption-aes-128-ctr).

## Transmit power

You set a power from 2 to 22 dBm on the Config screen (`Potência`, default 22). The radio uses the **effective power** ([`effectiveTxPower`](../../src/mesh/region.cpp)):

```
limit     = region power limit, or 17 dBm if the region's limit is 0 ("unknown", as in Meshtastic)
effective = min(user setting, limit, 22)     and at least 2 dBm
```

22 dBm is the SX1262's maximum. When the effective power is lower than your setting, the Config screen shows both, for example `22 dBm (efetiva 14)` ("effective 14") in UA_868 or PH_868, or `22 dBm (efetiva 20)` in RU and SG_923. The power limit is applied again every time you change the region.

Antenna gain is not taken into account. If your local rules limit EIRP/ERP and you use a high-gain antenna, lower the power yourself.

## Duty cycle and the airtime budget

Some regions limit the fraction of time a device may transmit: EU_868 and TH allow 10 %, UA_868 allows 1 %. In those regions the firmware keeps a rolling one-hour airtime account ([`AirtimeBudget`](../../src/mesh/region.h)) and holds frames back when the next one would exceed the budget.

- **Budget** = duty % of one hour: 10 % → **360 s per hour**, 1 % → **36 s per hour**.
- **Accounting**: 61 one-minute buckets. A frame sent during minute *M* keeps counting through minute *M + 60*, so it counts for **at least 3600 s** (up to 3660 s). This errs on the safe side.
- **Before each transmission** the firmware computes the frame's time-on-air (rounded up to whole milliseconds) and checks `used + airtime ≤ budget`. If the check fails, the frame and everything queued behind it **wait**. Nothing is dropped. The status bar shows `duty limit` in red, and the Config screen shows `Ar 1h: Xs de Ys (duty Z%)` ("airtime in the last hour: X s of Y s").
- In regions with 100 % duty cycle there is no budget and the Config line is not shown.

### Time on air

The firmware uses the Semtech SX126x formula ([`loraAirtimeUs`](../../src/mesh/region.cpp)) with the LongFast settings: SF11, BW 250 kHz, CR 4/5, preamble 16, explicit header, CRC on, low-data-rate optimisation off.

```
Tsym          = 2^SF / BW                              = 2048 / 250 kHz = 8.192 ms
preamble      = (16 + 4.25) × Tsym                     = 165.888 ms
payloadSymbols = 8 + ceil((8·L − 4·SF + 28 + 16) / (4·SF)) × CR   // L = whole frame length in bytes, CR = 5
               = 8 + ceil(8·L / 44) × 5
airtime       = preamble + payloadSymbols × Tsym
```

So every frame costs a fixed **≈ 231 ms** (L = 0), plus 5 symbols (41 ms) for every 5.5 bytes, which averages **≈ 7.4 ms per byte**:

| Frame length L (header + payload) | Time on air |
|---|---|
| 0 B (theoretical minimum) | 231 ms |
| 22 B (short public text, e.g. "hi") | 395 ms |
| 88 B (our NodeInfo with default names) | 887 ms |
| 128 B | 1.21 s |
| 200 B | 1.75 s |
| 255 B (maximum frame) | 2.16 s |

A typical frame is about **1 s**. In EU_868, 360 s per hour is roughly 400 NodeInfo-sized frames per hour, far more than this firmware sends in normal use. In UA_868 (36 s per hour), about 40 such frames per hour fit.

## Experimental regions

The M5Stack Cap LoRa-1262 is rated for **868–923 MHz**. The SX1262 chip itself tunes **150–960 MHz**, so the radio can be set to frequencies slightly outside the Cap's rating. Its RF switch, filtering and matching are designed for the rated band. Expect weaker transmit power and worse sensitivity outside it. How much worse has not been measured.

The firmware includes regions whose LongFast frequency is within [860, 868) MHz (IN, NZ_865, NP_865, KZ_863) or (923, 925] MHz (JP, TW, TH) and flags them:

- On Config, the region shows `(exp)`, for example `IN 865.875 (exp)`.
- When you select one, a toast says `Fora da faixa do Cap (exp)` ("outside the Cap's band (experimental)").
- While one is selected, Config shows `! Fora de 868-923 MHz do Cap (exp)` ("outside the Cap's 868–923 MHz").

Use an experimental region only if it is the correct region for your country, and expect reduced range.

## Brazil

Two regions are relevant in Brazil, and they do **not** hear each other:

| Region | LongFast | Status |
|---|---|---|
| **ANZ** | 919.875 MHz | What the Brazilian Meshtastic community uses today (for example [meshbrasil.com](https://meshbrasil.com)). **This firmware's default.** |
| **BR_902** | 903.875 MHz | Meshtastic's official region for Brazil (902–907.5 MHz). |

Anatel's rules for restricted-radiation (unlicensed) equipment allow spread-spectrum devices such as LoRa in **902–907.5 MHz and 915–928 MHz** within the 902–928 MHz band. The 907.5–915 MHz segment is not part of it. Both slots above fall inside permitted segments. Check the current Anatel regulation before relying on this.

If you see nobody on ANZ, ask local users which region they use, or try BR_902.

## Antennas per band

An antenna is tuned for a frequency range. Away from it, the VSWR rises and part of the power is reflected instead of radiated.

| Your region's frequency | Best antenna |
|---|---|
| 863–870 MHz (EU_868, RU, UA_868, PH_868, IN, NZ_865, NP_865, KZ_863) | 868 MHz antenna |
| 902–928 MHz (ANZ, BR_902, US, KR, MY_919, SG_923, PH_915, JP, TW, TH) | 915 MHz antenna |

The antenna supplied with the Cap is tuned for 868 MHz. In the 902–928 MHz regions it still works, with an expected loss of about **0.5–2 dB**: 919.875 MHz is about 6 % above its tuning and 903.875 MHz about 4 % above. With a VSWR of 2:1, about 11 % of the power is reflected (≈ 0.5 dB). With 3:1 the loss is ≈ 1.25 dB. That costs roughly 5–20 % of range. If you buy an antenna for a 902–928 MHz region, choose one tuned for 915 MHz rather than "wideband". With the 868 MHz antenna, BR_902 (903.875 MHz) is a little closer to its tuning than ANZ. See [hardware.md](hardware.md#antenna-and-range) for more range advice.

**Never transmit without an antenna attached.** The firmware transmits a NodeInfo right after boot.

## Keeping this table honest

The region table above sits between the HTML comments `<!-- regions:begin -->` and `<!-- regions:end -->`. The native test [`test/test_docs_regions`](../../test/test_docs_regions/test_main.cpp) reads this file, parses each table row and checks it against `regionAt(i)` in the same order: the name, the band edges, the LongFast frequency computed by `regionFreqMHz`, the power limit, the duty cycle and the experimental flag. It runs with every `pio test -e native`, and therefore in CI on every push and pull request. If you change `region.cpp`, update this table in the same commit (the column order matters to the test).
