# BLE Remote Control Firmware — CC2340R53

Full-featured Android TV BLE remote control reference design for the CC2340R53.

## Dependencies

- TI Device: CC2340R53
- TI SDK: simplelink_lowpower_f3_sdk_9_12_00_19
- TI Code Composer Studio: CCS Theia 20.5.0

## Supported RC Boards

| Board | Status |
|-------|--------|
| G21   | Supported (current branch) |

---

## Features

### BLE Connection, Pairing & Bonding

Advertising strategy adapts based on bond state:

- **OOBE (first boot, no bonds):** Undirected advertising runs in two 5.46-minute windows (auto-restart once) for a total of ~10.9 minutes.
- **Normal boot (existing bond):** Directed advertising targets the saved peer address for 90 seconds. Directed advertising reconnects significantly faster than undirected.
- **Pairing mode:** Triggered by holding `[BACK]+[HOME]` for >4 seconds. Restarts undirected advertising and resets the IR code table.

Connection parameters are requested immediately after link establishment: 11.25 ms interval, 0 latency (for audio streaming). During an active connection without audio, the peripheral applies a slave latency of 10 events (~112 ms standby) to save power.

---

### BLE HID

Implements the Android TV HID profile. A key press is routed through `key_router` which decides between IR transmission and BLE HID based on:

1. The suppress list (keys forced to BLE regardless of IR programming).
2. Whether an IR code is programmed for that key (force IR).
3. Default: BLE HID.

HID reports use Android-spec consumer control key codes (e.g., DPAD, Volume, Media). Special combo keys are detected and handled separately (see [Keypad Matrix](#keypad-matrix)).

---

### Audio over BLE

Full pipeline: **PDM capture → decimation → filtering → codec → BLE notification**

**DMIC (Digital Microphone):**
- PDM data fetched via SPI at ~1 MHz, DMA-driven with a 6-buffer circular queue.
- `pdm2pcm16k` decimates 1-bit PDM to 16-bit PCM at 16 kHz.
- Biquad filter compensates for DMIC frequency response droop.

**AMIC (Analog Microphone):**
- ADC/DMA driven at 16 kHz, 16-bit integer.
- Optional analog or digital filter stage.

**Codecs:**

| Codec | Frame | Compressed Size | Notes |
|-------|-------|-----------------|-------|
| ADPCM | 32 samples (2 ms) | ~20 bytes/notification | Default; required for Android TV |
| mSBC  | 120 samples (7.5 ms) | 57 bytes/notification | Higher quality; not supported by Android TV receivers |

Audio is transmitted via BLE notifications. MTU is negotiated to 255 bytes; actual payload per notification is ~20–60 bytes depending on codec. Audio start/stop uses dedicated Android-spec characteristic values within the Audio Stream GATT service.

Quality verified: SNR > 45 dB, THD < 1% at 1 kHz, range ≥10 m line-of-sight.

---

### Infrared (IR)

NEC standard driver using LGPT0 PWM at 38 kHz carrier on the IR TX pin.

- Android TV programs IR codes into the RC via a BLE GATT IR service.
- Codes are stored in NVS flash and loaded into a RAM cache at boot via `KeyRouter_reloadCache()`.
- On first pairing (bond count = 0), the IR code table is reset to allow a clean programming session.
- Key routing: if a key has a programmed IR code and is not on the BLE suppress list, it transmits IR instead of BLE HID.

---

### OAD (Over-the-Air Download)

Implements OAD with BLE security (authentication + encryption).

**Memory layout:**

| Region | Address | Size | Purpose |
|--------|---------|------|---------|
| Application | `0x00006000` | ~228 KB | Main firmware image |
| OAD image slot | `0x0003B000` | ~212 KB | Incoming firmware image |
| NVS storage | `0x00070000` | ~52 KB | Device config, IR codes |
| Internal NVS | `0x0007D000` | 12 KB | BLE stack NVS (bonds, etc.) |

OAD requires ≥30% battery before proceeding. During download, connection parameters switch to 7.5–15 ms interval with 0 latency for maximum throughput. The image is validated (header + signature) before the device resets and boots the new image via MCUBoot.

Total download time achieved < 30 seconds. However this depends on the OAD target configuration and workflow.

**MCUBoot secure boot & image signing:**

MCUBoot runs in **overwrite mode** with **ECDSA P-256** signature verification enabled. On every boot, MCUBoot verifies the application image signature against the public key compiled into the bootloader before transferring execution. An image with an invalid or missing signature will not boot.

| What | Where |
|------|-------|
| Public key (compiled into MCUBoot) | `keys.c` — linked from `$SDK/source/third_party/mcuboot/boot/ti/source/mcuboot_app/keys.c` |
| Default private key (TI sample) | `$SDK/source/third_party/mcuboot/root-ec-p256.pem` |
| Signing tool | `$SDK/tools/common/mcuboot/imgtool` |

The RC project post-build step (Release configuration) automatically strips the CCFG section, then calls `imgtool sign` to produce a signed binary:

```
imgtool sign
  --header-size 0x100
  --align 4
  --slot-size 0x35000
  --version <x.y.z>
  --pad-header --pad
  --key root-ec-p256.pem
  <input>_noheader.bin  <output>_vX.bin
```

**To use your own key pair for production:**

1. Generate a new EC P-256 key pair:
   ```
   imgtool keygen -k my-private-key.pem -t ecdsa-p256
   ```
2. Extract the public key into `keys.c` format:
   ```
   imgtool getpub -k my-private-key.pem >> keys.c
   ```
3. Replace the public key array in `$SDK/source/third_party/mcuboot/boot/ti/source/mcuboot_app/keys.c` with the output from step 2, then **rebuild and reflash MCUBoot**.
4. Update the `--key` argument in the RC project post-build step to point to `my-private-key.pem`.
5. Keep the private key secure — any image signed with it will be accepted by the bootloader.

---

### Device Information & NVS Configuration

Device identity and runtime configuration are stored in internal NVS at `0x70000` and loaded at boot by `DevInfoFlash_loadAll()`.

**Device ID region (`+0x20`, 32 bytes):**

| Field | Value |
|-------|-------|
| Vendor Source | `0x02` (USB IF) |
| Vendor ID | `0x8A24` (Google) |
| Product ID | `0x6682` (Google ATV Remote) |
| Device Type | `0x03`=G11, `0x05`=G21 (others: G10, G20, G31, G91) |
| Device Name | Up to 10 ASCII chars (e.g., `"GoogleRemote"`) |

Device type drives the Model Number and PnP ID exposed in the GATT Device Information Service. If unprogrammed (`0xFF`), defaults fall back to G10 / `"GoogleRemote"`.

**Device config region (`+0x80`):** TX power, backlight enable, couch mode behavior on inactivity.

**BLE config region (`+0xA0`):** Periodic wakeup interval, Resolvable Private Address (RPA) enable, BLE on/off switch, connection parameter cache.

---

### Find My Remote (FMR)

Buzzer driven via LGPT2 PWM. The Android TV writes a 3-byte payload to the FMR GATT characteristic:

- Byte 0: Event type.
- Bytes 1–2: Buzz duration (uint16, little-endian, in 100 ms units, max 15 minutes per spec).

Buzzing stops automatically if battery drops below 30%.

---

### Battery Monitoring

ADC channel 0 samples the battery rail. The Battery Monitor driver converts the reading to a percentage, which is exposed via the standard BLE Battery Service (UUID `0x180F`). The OAD and FMR modules both gate their operation on this value (threshold: 30%).

---

### LED Status

| Color | Condition |
|-------|-----------|
| Orange | No BLE connection (IR-only mode) |
| Red | BLE connected, not paired/bonded |
| Green | BLE connected and bonded |

---

### Combo Key Customization

Combo keys are defined in `keypad/key_scan.h` (`combokey_mode_t`). Current combos:

| Combo | Keys | Hold Time | Action |
|-------|------|-----------|--------|
| Re-pair | `[BACK]` + `[HOME]` | >4 s | Enter pairing mode, reset IR table |
| Factory reset | `[CENTER]` + `[MUTE]` | >4 s | Clear all bonds |
| Bug report | `[CENTER]` + `[BACK]` | >1 s | Send IR code 0x96 |
| Accessibility | `[DOWN]` + `[BACK]` | >1 s | Send IR code 0xD0 |
| Couch mode | Any 3 keys | >30 s | Stop BLE transmission |

---

### Power Optimization

- **Inactivity shutdown:** 15-minute timer in both connected and advertising states. On expiry, the device disconnects (if connected) and enters `Power_shutdown()`. Current in shutdown: ~1–2 µA.
- **Wake-on-keypress:** All 6 keypad row pins are configured as shutdown wake sources (high-level trigger). Pressing any key drives a row high through its column, causing a system reset.
- **Connection latency:** 10 skipped events × 11.25 ms ≈ 112 ms standby between events when idle.
- **Advertising window:** 90 seconds per window. First-boot OOBE auto-restarts once for ~10.9 minutes total.

---

## BLE GATT Profiles

All services use either standard Bluetooth SIG UUIDs or a 128-bit Android base UUID: `64B617F6-01AF-7DBC-054F-215A-XXXX-YYYY`.

| Service | UUID | Description |
|---------|------|-------------|
| Audio Stream | `...0001-5EAB` (128-bit) | Audio streaming and control |
| HID | `0x1812` | Human Interface Device |
| Battery | `0x180F` | Battery level |
| Device Information | `0x180A` | Device identity and version |
| OAD | TI base `0xFFC0` (128-bit) | Over-the-air firmware update |
| Infrared | `...C0BF-43D3` (128-bit) | IR code programming |
| Find My Remote | `...0001-1803` (128-bit) | Buzzer/finder control |
| Backlight | `...8B69-B3A2` (128-bit) | Backlight mode control |

**Audio Stream characteristics:**

| Characteristic | UUID suffix | Properties | Description |
|----------------|-------------|------------|-------------|
| TX | `0x0002` | Write No Rsp | Android TV → RC commands (8 bytes) |
| RX (audio) | `0x0003` | Notify | RC → TV audio data stream (up to 255 bytes) |
| Control | `0x0004` | Notify | RC → TV state/control events (up to 21 bytes) |

**HID characteristics (UUID `0x1812`):**

| Characteristic | UUID | Properties | Description |
|----------------|------|------------|-------------|
| HID Information | `0x2A4A` | Read | HID version and capability flags |
| Report Map | `0x2A4B` | Read | HID descriptor (keyboard + consumer control) |
| Boot Keyboard Input | `0x2A22` | Read, Notify | 6-key rollover keyboard report |
| Boot Keyboard Output | `0x2A32` | Write No Rsp | LED output (Caps Lock, etc.) |
| Protocol Mode | `0x2A4E` | Read, Write No Rsp | Report mode vs Boot mode |
| Control Point | `0x2A4C` | Write No Rsp | Suspend control |
| Report (Input) | `0x2A4D` | Read, Notify | Report ID 1: Keyboard, ID 2: Consumer control |

**Device Information characteristics (UUID `0x180A`, all Read-only):**

| Characteristic | UUID | Default / Source |
|----------------|------|-----------------|
| Model Number | `0x2A24` | Loaded from NVS (e.g., `"Google_G21"`) |
| Serial Number | `0x2A25` | Loaded from NVS |
| Firmware Revision | `0x2A26` | Loaded from NVS |
| Hardware Revision | `0x2A27` | Loaded from NVS |
| Software Revision | `0x2A28` | Loaded from NVS |
| Manufacturer Name | `0x2A29` | Loaded from NVS |
| PnP ID | `0x2A50` | Vendor `0x8A24` (Google), Product `0x6682` |

**OAD characteristics:**

| Characteristic | UUID | Properties | Description |
|----------------|------|------------|-------------|
| Image Identify | `0xFFC1` | Write, Write No Rsp, Notify | Image metadata exchange to start OAD |
| Image Block | `0xFFC2` | Write, Write No Rsp, Notify | Firmware block transfer |
| Extended Control | `0xFFC5` | Write No Rsp, Notify | OAD control commands and status responses |

**Infrared characteristics:**

| Characteristic | UUID suffix | Properties | Description |
|----------------|-------------|------------|-------------|
| IR Control | `0xC1BF` | Write | Programming control commands |
| IR Key | `0xC2BF` | Write | Key ID for IR code assignment |
| IR Code | `0xC3BF` | Write | IR code payload (NEC format) |
| IR Suppress | `0xC4BF` | Write | Force key to BLE HID (bypass IR) |
| IR Key Event | `0xC5BF` | Notify | Key press/release event notification |

**Find My Remote characteristics:**

| Characteristic | UUID suffix | Properties | Description |
|----------------|-------------|------------|-------------|
| FMR Control | `0x0002` | Read, Write, Notify | Byte 0: event type; Bytes 1–2: buzz duration (uint16 LE, 100 ms units) |

FMR event types: `PEER_REQUEST`, `DURATION_ELAPSED`, `RCU_FOUND`, `DURATION_TOO_LONG`, `LOW_BATTERY`.

**Backlight characteristics:**

| Characteristic | UUID suffix | Properties | Description |
|----------------|-------------|------------|-------------|
| Backlight Control | `0x8B6A` | Read, Write, Notify | Byte 0: mode; Byte 1: event type; Bytes 2–3: time/mode parameter (uint16 LE) |

Backlight mode is applied immediately on write. A `SET_TIME_CMD` allows the Android TV to sync a 3-second activation window after the mode is set.

---

## Pin Configuration

Modify pin assignments in `ble_remote_control.syscfg` (SysConfig) and verify `key_scan.h` for the keypad matrix. The table below reflects the G21 board layout.

### GPIO Summary

| Signal | GPIO | Direction | Notes |
|--------|------|-----------|-------|
| LED Green | 13 | Output | Status LED |
| LED Red | 14 | Output | Status / OAD LED |
| BOOST_EN | 24 | Output | Boost regulator enable |
| MIC_DTM | 21 | Output | Microphone power control |
| IR TX | 11 | Output (PWM) | LGPT0 CH0, 38 kHz carrier - Only IRGEN Out Pin!|
| SPI CLK | 18 | Output | DMIC / external flash SPI |
| SPI MOSI | 13 | Output | SPI data out |
| SPI MISO | 12 | Input | SPI data in |
| UART TX | 20 | Output | Debug console |
| UART RX | 22 | Input | Debug console |
| Buzzer | 3 | Output (PWM) | LGPT2 CH1, FMR buzzer |

### Keypad Matrix (6 × 7)

| Role | GPIOs |
|------|-------|
| Rows (inputs, interrupt) | 9, 10, 8, 15, 20, 19 |
| Columns (outputs) | 25, 0, 1, 2, 5, 6, 7 |

Rows are configured with pull-down and hysteresis. Scanning drives each column high in sequence and reads all rows. Debounce: 3 samples × 10 ms = 30 ms. Row pins double as shutdown wake sources.

> To adapt to a different board: update GPIO numbers in SysConfig and update `ROW_PINS[]` / `COL_PINS[]` in `keypad/key_scan.h`.

### DIO21 — Bootloader Backdoor Warning

> **WARNING:** DIO21 is the CC2340R53 ROM bootloader backdoor pin. If DIO21 is held **LOW** during power-on or reset, the device enters the ROM serial bootloader and will not execute application code. This results in the device appearing unresponsive (stuck in ROM boot loop).
>
> In this design DIO21 is used as `MIC_DTM` (microphone power control). Ensure that the external circuit connected to DIO21 **does not pull it low at boot**. If your board layout drives DIO21 low during startup, the pin trigger or bootloader must be disabled in SysConfig.

### SysConfig — Custom Board Configuration

Open `ble_remote_control.syscfg` in CCS SysConfig to change the following for a custom board:

| Setting | SysConfig Location | Current | Options / Notes |
|---------|-------------------|---------|-----------------|
| Voltage regulator | CCFG → `voltageRegulator` | `GLDO` | `GLDO` (low-dropout, simpler BOM) or `DCDC` (higher efficiency at higher current loads) |
| LF clock source | CCFG → `srcClkLF` | `LF RCOSC` | `LF RCOSC` (internal, no external component) or `LF XOSC` (external 32.768 kHz crystal, better frequency accuracy for BLE timing) |
| Bootloader pin trigger | CCFG → `pinTriggerEnabled` | `false` | Set `false` in production to prevent accidental ROM bootloader entry via DIO21 |
---

## Build Configuration

Set these symbols in CCS **Project Properties → Build → Compiler → Predefined Symbols**.

| Symbol | Options | Notes |
|--------|---------|-------|
| Microphone | `USE_DMIC` or `USE_AMIC` | Do not enable both |
| Codec | `USE_ADPCM_CODEC` or `USE_MSBC_CODEC` | Android TV requires ADPCM |
| Filter | `USE_DIGITAL_FILTER_1` + `USE_DIGITAL_FILTER_2` or `USE_ANALOG_FILTER` | Do not mix digital and analog |

Default (release): `USE_DMIC` + `USE_ADPCM_CODEC` + `USE_DIGITAL_FILTER_1` + `USE_DIGITAL_FILTER_2`.

---

## Developing with this Reference Design

### Project Structure

```
ble_app/            — Application layer (connection, peripheral, OAD, audio app logic)
  Profiles/         — GATT profiles (audio_stream, hid, batmon, dev_info, ir, fmr, backlight, oad)
voice/              — Audio pipeline (voice_processing.c, adpcm/, sbc/, filter/)
keypad/             — Key scan, debounce, combo detection, key routing
infrared/           — NEC IR driver, LGPT timer, IR GATT service
nvs_flash/          — Device info and IR code NVS read/write
shutdown/           — Power shutdown and wake-on-keypress
led/                — LED state machine
backlight/          — Backlight PWM control
fmr/                — Find My Remote buzzer service
common/             — BLEAppUtil framework, iCall, SDK drivers
```

### Adding or Modifying Keys

1. Update the keypad matrix size in `keypad/key_scan.h` (`NUM_ROWS`, `NUM_COLS`).
2. Update `ROW_PINS[]` and `COL_PINS[]` for the new GPIOs and reconfigure them in SysConfig.
3. Add new key mappings in `keypad/key_router.c` (`keyRouterTable[]`).
4. Add corresponding HID key codes in `ble_app/Profiles/hid/hid_service.c`.

### Changing BLE Connection Parameters

Edit the parameter structs in `ble_app/app_connection.c`. Audio streaming requires a connection interval ≤ 20 units (11.25 ms). Increasing latency saves power but increases HID key latency.

### Changing the Audio Codec or Microphone

Switch compiler defines (see [Build Configuration](#build-configuration)). The audio pipeline entry point is `voice/voice_processing.c`; codec-specific logic is in `voice/adpcm/` and `voice/sbc/`.

### Programming Device Identity (NVS)

Device type, name, VID/PID, and runtime config are stored in NVS at `0x70000`. Program via:

- **UniFlash:** Load a binary blob to address `0x70000`.
- **CCS Memory Browser:** Write bytes directly while debugging.

### OAD Image Generation

The project uses MCUBoot as the bootloader (`mcuboot_LP_EM_CC2340R53_nortos_ticlang`). Build the application in Release, then use the post-build step to sign and package the image. The signed `.bin` is the OAD payload distributed to Android TV for wireless update.

---

## References

- [BLE5-Stack User's Guide](https://software-dl.ti.com/simplelink/esd/simplelink_lowpower_f3_sdk/9.12.00.19/exports/docs/ble5stack/ble_user_guide/html/ble-stack-5.x-guide/index-cc23xx.html)
- [SimpleLink Academy CC23XX](https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AEIJm0rwIeU.2P1OBWwlaA__SIMPLELINK-ACADEMY-CC23XX__gsUPh5j__LATEST)
- [BLE E2E Forum](https://e2e.ti.com/support/wireless-connectivity/bluetooth-group/bluetooth/f/bluetooth-forum)
