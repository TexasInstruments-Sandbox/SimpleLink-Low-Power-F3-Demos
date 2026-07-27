# Voice over BLE — CC2340R53

BLE audio streaming reference project for the CC2340R53. Captures audio from a digital or analog microphone, encodes it with ADPCM or mSBC, and streams it over BLE to a central device. Intended as a development starting point for voice-over-BLE applications.

## Dependencies

- TI Device: CC2340R53
- TI SDK: simplelink_lowpower_f3_sdk_9_14_02_16
- TI Code Composer Studio: CCS Theia 20.5.0
- SysConfig: 1.23.2
- Compiler: TI CLANG 4.0.4.LTS

---

## Features

### Audio over BLE

Full audio pipeline: **mic capture → decimation → filtering → codec → BLE notification**

**DMIC (default):**
- Interface: SPI at ~1 MHz clock, DMA-driven.
- Raw input: 1-bit PDM stream from digital microphone.
- `pdm2pcm16k` performs 64× decimation from PDM to 16-bit PCM at 16 kHz.
- Filter path: `USE_DIGITAL_FILTER_1` (CIC Decimator + high-pass biquad, DC removal) + `USE_DIGITAL_FILTER_2` (biquad EQ, response compensation).

**AMIC (optional):**
- Interface: ADC with DMA, sampled at 16 kHz, 16-bit output.
- Bypasses PDM decimation; enters filter chain directly after ADC DMA callback.
- Filter path: `USE_ANALOG_FILTER` (anti-aliasing). Do not combine with digital filters.
- Supports oversampling: 8× input rate before decimation, improving SNR via `USE_CIC_FILTER` (CIC decimator, order 4, scale 8⁴). Only available with ADPCM; not supported with mSBC.

**Filter Chain:**

| Stage | Define | Mic Path | Description |
|-------|--------|----------|-------------|
| Digital 1 | `USE_DIGITAL_FILTER_1` | DMIC only | CIC Decimator + High-pass biquad; removes DC offset and noise below ~80 Hz. |
| Digital 2 | `USE_DIGITAL_FILTER_2` | DMIC only | Biquad EQ; compensates for DMIC frequency response droop at higher frequencies. |\
| Analog 1 | `USE_ANALOG_FILTER` | AMIC only | Anti-aliasing at ADC input level. Do not combine with digital filters. |
| Analog 2 | `USE_CIC_FILTER` | AMIC only | Cascaded Integrator-Comb (CIC) decimator, order 4, scale 8⁴. Required when `OVERSAMPLING` is enabled. |

**Codec:**

| Codec | Frame | Output | Compression |
|-------|-------|--------|-------------|
| ADPCM (default) | 32 samples (2 ms) | ~20 bytes/notification | 4:1 (16-bit → 4-bit delta) |
| mSBC | 120 samples (7.5 ms) | 57 bytes/notification | ~2.7:1 |

Each ADPCM notification includes a 4-byte header (predicted value + step index) for state recovery at the receiver.

### BLE Connection

- Peripheral + Central + Broadcaster + Observer roles enabled.
- Device name: `Basic BLE project`
- Default TX power: 8 dBm.
- Initial connection interval: 1000 ms.
- After start command (control byte `0x0A`): interval drops to 7.5 ms (6 × 1.25 ms), latency 0, supervision timeout 6 s. A 3-second delay after connection precedes the parameter update request.
- Advertising includes the custom 128-bit service UUID and the 16-bit UUID `0xFFF0`.

### Button Control

| Button | Action |
|--------|--------|
| Button 0 (bp.13) | Start audio streaming |
| Button 1 (bp.12) | Stop audio streaming |

### LED Status

| LED | GPIO | State |
|-----|------|-------|
| Green | GPIO 15 (bp.40) | Connected / streaming |
| Red | GPIO 14 (bp.39) | Unconnected / idle |

---

## BLE GATT Profile

The Audio Stream service uses a 16-bit UUID base.

| Characteristic | UUID | Properties | Size | Description |
|----------------|------|------------|------|-------------|
| DataIn | `0xC0C1` | Write No Rsp | 255 bytes | Central → Peripheral: control commands. Write `0x0A` to trigger connection parameter update to 7.5 ms interval and begin streaming. |
| DataOut | `0xC0C2` | Notify | 255 bytes | Peripheral → Central: compressed audio frames. Each notification carries one ADPCM or mSBC frame plus header. |

Service UUID: `0xC0C0`

---

## Pin Configuration

| Signal | GPIO | Board Pin | Direction | Notes |
|--------|------|-----------|-----------|-------|
| LED Green | GPIO 15 | bp.40 | Output | Active high |
| LED Red | GPIO 14 | bp.39 | Output | Active high |
| MIC_EN | GPIO 8 | bp.36 | Output | DMIC power enable |
| SPI CLK | GPIO 18 | bp.7 | Output | DMIC PDM clock |
| SPI PICO | GPIO 19 | bp.17 | Output | SPI data out |
| SPI POCI | GPIO 21 | bp.37 | Input | SPI data in (PDM data) |
| ADC CH0 | GPIO 7 | bp.28 | Input | AMIC analog input |
| UART TX | GPIO 20 | bp.4 | Output | Debug UART |
| UART RX | GPIO 22 | bp.3 | Input | Debug UART |
| Button 0 | — | bp.13 | Input | Start streaming |
| Button 1 | — | bp.12 | Input | Stop streaming |

---

## Build Configuration

Set these symbols in **Project Properties → Build → Compiler → Predefined Symbols**.

| Symbol | Options | Notes |
|--------|---------|-------|
| Microphone | `USE_DMIC` or `USE_AMIC` | Do not enable both |
| Codec | `USE_ADPCM_CODEC` or `USE_MSBC_CODEC` | Do not enable both |
| DMIC filters | `USE_DIGITAL_FILTER_1`, `USE_DIGITAL_FILTER_2` | DMIC path only; do not combine with analog filter |
| AMIC filters | `USE_ANALOG_FILTER`, `USE_CIC_FILTER` | AMIC path only; do not combine with digital filters |
| Oversampling | `OVERSAMPLING` | AMIC + ADPCM only; not compatible with mSBC |

Default (as shipped): `USE_DMIC` + `USE_ADPCM_CODEC` + `USE_DIGITAL_FILTER_1` + `USE_DIGITAL_FILTER_2`

---

## NVS

- Base address: `0x7D000`
- Size: 12 KB (6 NV pages, `NVOCMP_NVPAGES=6`)
- Driver: `NVSLPF30` (internal flash)

---

## Developing with this Reference Design

### Project Structure

```
app/
  Profiles/
    audio_stream/       — BLE Audio Stream GATT service (DataIn/DataOut characteristics)
  peripheral/           — BLE peripheral role, advertising, connection management
common/
  BLEAppUtil/           — iCall/BLEAppUtil framework wrappers
  NV/                   — NVS read/write utilities
voice/
  voice_processing.c    — Pipeline entry: mic capture → filter → codec → BLE TX
  adpcm/                — IMA ADPCM encoder/decoder
  sbc/                  — mSBC codec
  filter/               — CIC and biquad filter implementations
```

### Changing the Microphone

Switch `USE_DMIC` ↔ `USE_AMIC` in compiler defines. For AMIC, verify the ADC channel and pin in SysConfig match the hardware. The AMIC path skips PDM decimation and enters the filter chain after the ADC DMA callback. Use `USE_ANALOG_FILTER` + `USE_CIC_FILTER` + `OVERSAMPLING` for AMIC; do not use `USE_DIGITAL_FILTER_1/2` with AMIC. `OVERSAMPLING` is only compatible with ADPCM — disable it when using mSBC.

### Changing the Codec

Switch `USE_ADPCM_CODEC` / `USE_MSBC_CODEC` in compiler defines. Codec selection is resolved at compile time in `voice/voice_processing.c`. To add a new codec, add encoder logic under `voice/` and a corresponding `#ifdef` branch in `voice_processing.c`.

### Adjusting Connection Speed

The connection parameter update to 7.5 ms is triggered by the control command `0x0A` written to DataIn. The 3-second pre-update delay is in `audio_stream_profile.c` (`rc_audio_parmupdate_clkHandleCB`). Adjust the clock timeout there if a different ramp-up delay is needed.

### Tuning Audio Quality

- Improve SNR (AMIC + ADPCM): enable `OVERSAMPLING` + `USE_CIC_FILTER` for 8× oversampled input with CIC decimation.
- Reduce latency: decrease DMA buffer count in `voice_processing.c`.
- Adjust EQ (DMIC): modify biquad coefficients in `voice/filter/` for `USE_DIGITAL_FILTER_2` to match a different DMIC's frequency response.

---

## References

- [BLE5-Stack User's Guide](https://software-dl.ti.com/simplelink/esd/simplelink_lowpower_f3_sdk/9.14.02.16/exports/docs/ble5stack/ble_user_guide/html/ble-stack-5.x-guide/index-cc23xx.html)
- [SimpleLink Academy CC23XX](https://dev.ti.com/tirex/explore/node?node=A__AEaxXmSXZjp24G7-XUfwSQ__SIMPLELINK-ACADEMY-CC23XX__gsUPh5j__LATEST)
- [BLE E2E Forum](https://e2e.ti.com/support/wireless-connectivity/bluetooth-group/bluetooth/f/bluetooth-forum)
