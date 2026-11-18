# Hardware

M5-Mesh-Hello runs on one specific combination:

- **M5Stack Cardputer-Adv** (ESP32-S3, 240 × 135 display, keyboard behind a TCA8418 controller).
- **M5Stack Cap LoRa-1262**: Semtech SX1262 LoRa transceiver, rated 868–923 MHz, up to +22 dBm, RP-SMA antenna connector. It also carries an ATGM336H GNSS receiver, which v1 does not use.
- An **antenna** on the Cap's RP-SMA connector. The kit antenna is tuned for 868 MHz (see [Antenna and range](#antenna-and-range)).

The original Cardputer (non-Adv) and other LoRa modules are not supported: the pin map, the I/O expander and the antenna switch differ.

> **Never transmit without an antenna.** The firmware sends its NodeInfo within a second of booting, so screw the antenna on before powering the device.

## Pin map

From [`src/hw/radio.cpp`](../../src/hw/radio.cpp) and M5Stack's Cap LoRa-1262 documentation:

| Signal | ESP32-S3 pin | Notes |
|---|---|---|
| SX1262 NSS (chip select) | G5 | |
| SX1262 DIO1 (IRQ) | G4 | Interrupt on packet received; also polled as a level |
| SX1262 RESET | G3 | |
| SX1262 BUSY | G6 | |
| SPI SCK | G40 | Shared with the microSD slot |
| SPI MISO | G39 | Shared with the microSD slot |
| SPI MOSI | G14 | Shared with the microSD slot |
| microSD CS | G12 | Driven **high** (deselected) before the radio starts |
| I²C SDA / SCL (internal bus) | G8 / G9 | Shared by the keyboard controller, the IMU and the Cap's I/O expander |
| PI4IOE5V6408 I/O expander | I²C address `0x43` | Port **P0** drives the antenna RF switch |
| GNSS UART (ATGM336H) | ESP RX = G15, ESP TX = G13, 115200 baud | **Not used in v1** |

The LoRa/SD bus runs on the ESP32-S3's SPI2 (`FSPI`) peripheral. The display uses SPI3.

## Antenna switch (PI4IOE5V6408, P0)

The Cap's RF switch is enabled through port P0 of a PI4IOE5V6408 I/O expander on the internal I²C bus (address `0x43`, 400 kHz). At boot the firmware:

1. initialises the expander;
2. sets P0 as an output, disables its high-impedance mode and drives it **high**;
3. reads the output register back.

The Cap counts as **found** only if every step succeeds and P0 reads back high. Otherwise the status bar shows `SEM CAP LoRa`. The firmware still tries to start the SX1262, but without the switch enabled nothing useful can be sent or received. Transmit/receive switching inside the Cap is driven by the SX1262's DIO2 pin (RadioLib's default, as in M5Stack's official example).

`M5Cardputer.begin()` must run before the radio starts because it brings up the internal I²C bus. The expander shares that bus with the keyboard controller and the IMU.

## Radio configuration

| Setting | Value |
|---|---|
| TCXO | **3.0 V**, regulator in **LDO** mode (as in M5Stack's official example) |
| Over-current protection | 140 mA |
| RX gain | Boosted (as Meshtastic does, for better sensitivity) |
| Modem | LoRa, BW 250 kHz, SF 11, CR 4/5 (Meshtastic LongFast) |
| Sync word | `0x2B` (Meshtastic) |
| Preamble | 16 symbols |
| Header / CRC | Explicit header, payload CRC on |
| TX power | 2–22 dBm, clamped per region (see [frequencies.md](frequencies.md#transmit-power)) |
| Frequency | Computed from the region (see [frequencies.md](frequencies.md)) |

Reception is interrupt-driven on DIO1. As a safety net, the firmware also checks the DIO1 level on every loop, and re-arms reception after 10 minutes without any RX or TX. Transmission is blocking: the main loop, and therefore the UI, pauses until the frame has gone out (up to about 2.2 s for a maximum-length frame).

## Shared buses

- **SPI with the microSD slot.** The SD card's CS (G12) is held high so that the card ignores the bus while the radio uses it. v1 does not use the SD card at runtime. M5Launcher uses it only to install the firmware.
- **I²C with the keyboard.** The antenna switch's expander sits on the same internal I²C bus as the TCA8418 keyboard controller. The firmware writes to the expander once at boot.

## GNSS (not used in v1)

The Cap has an ATGM336H GNSS receiver on UART (ESP RX = G15, TX = G13, 115200 baud). v1 does not start the UART and sends no position. Position sharing is on the [roadmap](roadmap.md). M5Stack notes that LoRa transmissions interfere with the GNSS receiver, which a v2 will have to take into account.

## Antenna and range

The antenna supplied with the Cap is tuned for **868 MHz**. In a 902–928 MHz region it still works, with a small loss:

- 919.875 MHz (ANZ) is about 6 % above its tuned frequency, and 903.875 MHz (BR_902) about 4 % above. Away from its tuning the antenna's VSWR rises.
- At VSWR 2:1, about 11 % of the power is reflected, a loss of ≈ **0.5 dB**. At 3:1, ≈ **1.25 dB**.
- No public measurement of this antenna at 915 MHz exists. Expect a loss of about **0.5–2 dB**, which is roughly 5–20 % of range. It works for long distances, but it is not ideal.

What matters most for range, in order:

1. **Height and line of sight.** Raising the device a few metres, or moving to a window, balcony or rooftop, gains far more than any antenna change. Buildings, hills and bodies block 900 MHz signals.
2. **An antenna tuned for your band.** For 902–928 MHz regions (ANZ, BR_902, US, ...), buy an antenna tuned for **915 MHz**, not a "wideband" one. For 863–870 MHz regions (EU_868, RU, ...), the kit's 868 MHz antenna is the right one.
3. **Vertical orientation**, the same polarisation as the other nodes.
4. **Maximum allowed power.** The default setting is 22 dBm. The region may lower the effective power.

With the 868 MHz kit antenna, BR_902 is slightly closer to the antenna's tuning than ANZ, but the Brazilian community is on ANZ. See [frequencies.md](frequencies.md#antennas-per-band).

**Never transmit without an antenna attached.** Transmitting into an open connector reflects the power back into the power amplifier and can damage it.

## Installing with M5Launcher

The release images are in [`bin/`](../../bin/):

| File | Use |
|---|---|
| `M5-Mesh-Hello.bin` | The application image. Install it with **M5Launcher** from the SD card. |
| `M5-Mesh-Hello-merged.bin` | A full flash image (bootloader + partition table + boot_app0 + app) to write at offset `0x0` with esptool or a web flasher. It **replaces everything on the flash, including M5Launcher and its installed apps**. Use it only if you do not use M5Launcher. |
| `SHA256SUMS` | SHA-256 checksums of both images |

Steps:

1. Verify the download: `sha256sum -c SHA256SUMS` inside `bin/` (on Windows: `certutil -hashfile M5-Mesh-Hello.bin SHA256`, then compare). See [security.md](security.md#verifying-a-release).
2. Copy `M5-Mesh-Hello.bin` to the microSD card.
3. In M5Launcher, choose the SD card, select the file and install it.
4. Screw the antenna on, then start the app.

Partition notes:

- M5Launcher creates an application partition sized to the binary it installs. v0.1.0 is about 590 KiB (603,744 bytes). The firmware uses no SPIFFS/LittleFS partition, and everything it stores goes into **NVS** (namespace `meshhello`, see [architecture.md](architecture.md#persistence-nvs)).
- To install a newer version, first **remove the old one** in M5Launcher (**PMan → Remove**), then install the new binary. That frees the partition so that the new image gets a correctly sized one.
- Settings, the key pair, the greeted list and saved peers live in NVS and normally survive reinstalling the app. Whether M5Launcher erases NVS depends on the Launcher version and the options used.

**Do not run `pio test` on a device with M5Launcher installed.** PlatformIO's on-device test runner flashes its own image and overwrites M5Launcher and its apps. All tests in this project run natively on the PC (see [development.md](development.md)).

## Display note

With the current M5GFX, rotation 0 is already landscape (240 × 135) on the Cardputer-Adv. The firmware never calls `setRotation(1)`.
