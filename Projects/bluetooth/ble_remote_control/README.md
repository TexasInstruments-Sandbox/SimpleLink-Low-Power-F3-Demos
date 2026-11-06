# BLE Remote Control — CC2340R53

SW reference design for an Android TV BLE remote control with Voice over BLE, built on the TI SimpleLink Low Power F3 platform. This workspace contains the RC application firmware and the MCUBoot secure bootloader required for OAD support.

---

## Repository Structure

```
BLE Remote Control/
├── ble_remote_control_CC2340R53_freertos_ticlang/   — Full RC firmware
└── mcuboot_LP_EM_CC2340R53_nortos_ticlang/          — MCUBoot secure bootloader (OAD)
```

---

## Projects

| Project | Description | README |
|---------|-------------|--------|
| `ble_remote_control_CC2340R53_freertos_ticlang` | Full-featured Android TV BLE RC firmware with Voice over BLE (DMIC/AMIC, ADPCM/mSBC), HID, IR, OAD, FMR, Backlight | [README](ble_remote_control_CC2340R53_freertos_ticlang/README.md) |
| `mcuboot_LP_EM_CC2340R53_nortos_ticlang` | MCUBoot secure bootloader — ECDSA P-256 image verification, overwrite-mode OAD slot management | [README](mcuboot_LP_EM_CC2340R53_nortos_ticlang/README.md) |

---

## RC Firmware Features

The RC application is built from self-contained feature modules. Each module lives in its own folder and can be used independently in other projects — all internal dependencies are limited to the folder itself plus the common BLE framework.

| Folder | Feature | Description |
|--------|---------|-------------|
| `voice/` | **Audio over BLE** | Full audio pipeline: DMIC (SPI/PDM/DMA) or AMIC (ADC/DMA) → CIC decimator → biquad filters → ADPCM or mSBC codec → BLE notification. 16 kHz, 16-bit. |
| `infrared/` | **IR Transmitter** | NEC standard IR driver using LGPT PWM timer (inverted channel). Android TV key database stored in NVS. Operates independently of BLE. |
| `keypad/` | **Keypad Matrix** | 6×7 matrix scanner with debounce, wake-on-keypress from shutdown, and combo key detection. |
| `backlight/` | **Backlight Control** | PWM-based backlight driver with configurable brightness and auto-off timeout. |
| `fmr/` | **Find My Remote (FMR)** | PWM buzzer driver triggered by BLE command. Plays alert tone to locate the remote. |
| `led/` | **LED Control** | LED state machine: Orange (IR-only mode), Red (BLE unpaired), Green (BLE bonded). |
| `shutdown/` | **Power Shutdown** | Inactivity timer triggering deep shutdown. Configurable timeout. Wake via keypress. |
| `calendar/` | **Calendar / RTC** | Software RTC for timestamping and scheduled operations. |
| `nvs_flash/` | **NVS Extended Storage** | Extended NVS wrapper for storing device identity, IR key database, and bonding-related persistent data. |
| `ble_hid/` | **BLE HID** | HID device profile implementation for Android TV key codes. |
| `Util/SwUpdate/` | **OAD / SW Update** | Flash interface and slot management for MCUBoot-based over-the-air firmware updates. |
| `ble_app/Profiles/` | **BLE GATT Profiles** | Individual GATT service implementations: Audio Stream, Backlight, Battery Monitor, Device Info, FMR, HID, IR, OAD. Each profile folder is self-contained. |

> Each folder is self-contained: to reuse a module in another project, copy the folder, add the source files to the build, and wire the initialization call into your application's startup. No cross-module dependencies exist outside of the common BLE framework (`common/`) and the BLE application layer (`ble_app/`).

---

## Flash Layout

Both images must be flashed together. They occupy non-overlapping regions of the CC2340R53 internal flash (512 KB total).

| Region | Address | Size | Image |
|--------|---------|------|-------|
| MCUBoot | `0x00000000` | 24 KB | `mcuboot_LP_EM_CC2340R53_nortos_ticlang.hex` |
| RC Application | `0x00006000` | 212 KB | `ble_remote_control_CC2340R53_freertos_ticlang.hex` |
| OAD Candidate | `0x0003B000` | 212 KB | Written over-the-air during OAD update |

> MCUBoot runs first on every reset, verifies the RC application signature, then boots it. The RC application image must include a valid MCUBoot header and ECDSA P-256 signature — images built from source with the default post-build step are already signed with the TI sample key.

---

## Quick Start — Flash Pre-built Images

1. Open **UniFlash** and select device **CC2340R53**.
2. Connect the board via XDS110.
3. Add both `.hex` files (MCUBoot first, then RC application).
4. Click **Load Image**.
5. Power-cycle the board.

Pre-built MCUBoot binary: `mcuboot_LP_EM_CC2340R53_nortos_ticlang/Debug/mcuboot_LP_EM_CC2340R53_nortos_ticlang.hex`

Pre-built RC application binary: `ble_remote_control_CC2340R53_freertos_ticlang/Release/ble_remote_control_CC2340R53_freertos_ticlang.hex`

The `Release/` folder also contains versioned signed images (`_v1.bin`, `_v2.bin`) for OAD update testing, and the unsigned `_noheader.bin` for custom signing with a replacement key.

---

## Dependencies

| Component | SDK | CCS |
|-----------|-----|-----|
| RC Application | simplelink_lowpower_f3_sdk_9_12_00_19 | CCS Theia 20.5.0 |
| MCUBoot | simplelink_lowpower_f3_sdk_9_12_00_19 | CCS Theia 20.5.0 |

---

## References

- [BLE5-Stack User's Guide](https://software-dl.ti.com/simplelink/esd/simplelink_lowpower_f3_sdk/9.12.00.19/exports/docs/ble5stack/ble_user_guide/html/ble-stack-5.x-guide/index-cc23xx.html)
- [SimpleLink Academy CC23XX](https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AEIJm0rwIeU.2P1OBWwlaA__SIMPLELINK-ACADEMY-CC23XX__gsUPh5j__LATEST)
- [MCUBoot Documentation](https://docs.mcuboot.com/)
- [UniFlash](https://www.ti.com/tool/UNIFLASH)
- [BLE E2E Forum](https://e2e.ti.com/support/wireless-connectivity/bluetooth-group/bluetooth/f/bluetooth-forum)
