# Voice over BLE

End-to-end reference implementation for wireless audio streaming over Bluetooth Low Energy. Covers the full pipeline from microphone capture on an embedded transmitter to audio playback on a receiver, with a PC-side tool for recording and analysis.

## System Overview

```
[CC2340R53 Transmitter] --BLE--> [CC26X2R1 Receiver] --I2S--> [CC3200AUDBOOST] ---> [Speaker / Headphones]
                                         |
                                       USB/UART
                                         |
                                  [PC Audio Recording Tool]
```

---

## Repository Structure

```
Voice over BLE/
├── Audio Transmitter/
│   ├── voice_over_ble_LP_EM_CC2340R53_freertos_ticlang/   — Transmitter firmware (CC2340R53)
│   └── Pre-compiled binaries/                              — Ready-to-flash hex files
└── Audio Receiver/
    ├── simple_audio_receiver_CC26X2R1_LAUNCHXL_tirtos7_ticlang/  — Receiver firmware (CC26X2R1)
    ├── Audio Recording PC Tool/                            — PC-side recording and analysis utility
    └── Pre-compiled binaries/                              — Ready-to-flash hex files
```

---

## Quick Project Setup

1.  Flash the simple_audio_receiver_ADPCM_CC26X2R1.hex into the CC26X2R1 Launchpad.
2.  Flash the voice_over_ble_LP_EM_CC2340R53_freertos_ticlang_AMIC_ADPCM.hex or voice_over_ble_LP_EM_CC2340R53_freertos_ticlang_DMIC_ADPCM into the CC2340R53 Launchpad.
3.	Place the AudioBoosterPack on top of the CC26X2R1 Launchpad.
4.	Power-cycle both devices.
5.	Power the Receiver first.
6.	Power the CC2340R53 Launchpad.
7.	Wait until both LEDs (RED and GREEN) on the Receiver are ON, signaling a connection between both devices.
8.	Press the Left Button on the CC2340R53 Launchpad to start the audio transmission. The Right Button is used to stop the transmission.
9.	To verify the audio streaming you can plug an audio output to the top audio plug as seen in the photo below (green audio jack).
10.	Start the Audio Tool Recording always after pressing the Left Button on the CC2340R53 Launchpad.

---
## Projects

| Project | Device | Role | README |
|---------|--------|------|--------|
| `voice_over_ble_LP_EM_CC2340R53_freertos_ticlang` | CC2340R53 | BLE Peripheral — mic capture, encoding, BLE TX | [README](Audio%20Transmitter/voice_over_ble_LP_EM_CC2340R53_freertos_ticlang/README.md) |
| `simple_audio_receiver_CC26X2R1_LAUNCHXL_tirtos7_ticlang` | CC26X2R1 + CC3200AUDBOOST | BLE Central — BLE RX, decoding, I2S audio output | [README](Audio%20Receiver/simple_audio_receiver_CC26X2R1_LAUNCHXL_tirtos7_ticlang/README.md) |

---

## Pre-compiled Binaries

Ready-to-flash hex files requiring no compilation. Flash using UniFlash and XDS110.

**Audio Transmitter** (`Audio Transmitter/Pre-compiled binaries/`):

| File | Configuration |
|------|---------------|
| `voice_over_ble_LP_EM_CC2340R53_freertos_ticlang_DMIC_ADPCM.hex` | Digital mic (DMIC) + ADPCM |
| `voice_over_ble_LP_EM_CC2340R53_freertos_ticlang_AMIC_ADPCM.hex` | Analog mic (AMIC) + ADPCM |

**Audio Receiver** (`Audio Receiver/Pre-compiled binaries/`):

| File | Configuration |
|------|---------------|
| `simple_audio_receiver_ADPCM_CC26X2R1.hex` | ADPCM decoder |

---

## PC Audio Recording Tool

Located in `Audio Receiver/Audio Recording PC Tool/`. Connects to the receiver over USB/UART (3 Mbps) and records or visualizes the decoded audio stream in real-time.

**Pre-built executable (Windows):**
Run `TI Audio Recording Tool/TIAudioRecordingTool.exe` — no installation required.

*Note: Make sure you use Python > 3.10*

**Python source:**
```bash
cd "Audio Receiver/Audio Recording PC Tool/TI Audio Recording Tool for Developers"
pip install -r requirements.txt
python TIAudioRecordingTool.py
```

Dependencies: `pyserial`, `numpy`, `matplotlib`.

---

## Dependencies Summary

| Component | Device | SDK | CCS |
|-----------|--------|-----|-----|
| Transmitter | CC2340R53 | simplelink_lowpower_f3_sdk_9_14_02_16 | CCS Theia 20.5.0 |
| Receiver | CC26X2R1 | simplelink_cc13xx_cc26xx_sdk_6_30_01_03 | CCS 12.0.0 |

---

## References

- [CC3200AUDBOOST Audio Boosterpack](https://www.ti.com/tool/CC3200AUDBOOST)
- [CC26X2R1 LaunchPad](https://www.ti.com/tool/LAUNCHXL-CC26X2R1)
- [BLE5-Stack User's Guide — F3](https://software-dl.ti.com/simplelink/esd/simplelink_lowpower_f3_sdk/9.14.02.16/exports/docs/ble5stack/ble_user_guide/html/ble-stack-5.x-guide/index-cc23xx.html)
- [BLE5-Stack User's Guide — CC13XX/CC26XX](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/6.30.01.03/exports/docs/ble5stack/ble_user_guide/html/ble-stack-5.x-guide/index-cc13xx_cc26xx.html)
- [BLE E2E Forum](https://e2e.ti.com/support/wireless-connectivity/bluetooth-group/bluetooth/f/bluetooth-forum)
