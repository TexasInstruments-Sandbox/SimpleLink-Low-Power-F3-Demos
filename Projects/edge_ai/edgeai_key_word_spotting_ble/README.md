# Key Word Spotting — EdgeAI BLE

End-to-end reference implementation for on-device voice keyword recognition over Bluetooth Low Energy. A BLE peripheral sensor runs audio capture, MFCC feature extraction, and TVM neural network inference on-device, then streams classified results to a BLE central, which forwards them to a host PC for real-time visualization.

## System Overview

```
[CC2745R10 Peripheral Sensor]  --BLE Notify-->  [CC2340R53 Central Receiver]  --UART-->  [Host Visualization (Python)]
  Mic → MFCC → TVM Inference                     Receive + Forward Results               Real-time Dashboard
```

---

## Repository Structure

```
Key Word Spotting/
├── Peripheral BLE Sensor/
│   └── edgeai_voice_sensor_ble_LP_EM_CC2745R10_freertos_ticlang/   — Sensor firmware (CC2745R10)
└── Central BLE Receiver/
    ├── edgeai_voice_central_ble_LP_EM_CC2340R53_freertos_ticlang/  — Receiver firmware (CC2340R53)
    └── Host Visualization/                                          — Real-time Python dashboard
```

---

## Projects

| Project | Device | Role | README |
|---------|--------|------|--------|
| `edgeai_voice_sensor_ble_LP_EM_CC2745R10_freertos_ticlang` | CC2745R10 (Cortex-M33) | BLE Peripheral — audio capture, MFCC, TVM inference, BLE TX | [README](Peripheral%20BLE%20Sensor/edgeai_voice_sensor_ble_LP_EM_CC2745R10_freertos_ticlang/README.md) |
| `edgeai_voice_central_ble_LP_EM_CC2340R53_freertos_ticlang` | CC2340R53 (Cortex-M0+) | BLE Central — scan, connect, receive GATT notifications, UART forward | [README](Central%20BLE%20Receiver/edgeai_voice_central_ble_LP_EM_CC2340R53_freertos_ticlang/README.md) |

---

## EdgeAI Learning Resources

Before working with this project, developers are strongly encouraged to review the following SimpleLink Academy labs on Edge AI:

**[SimpleLink Academy — Edge AI Labs](https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AEIJm0rwIeU.2P1OBWwlaA__SIMPLELINK-ACADEMY-CC23XX__gsUPh5j__LATEST)**

| Lab | Description |
|-----|-------------|
| Getting Started with Edge AI and TI Edge AI Studio | Introduction to the Edge AI workflow on TI SimpleLink devices and the TI Edge AI Studio toolchain. |
| **Advanced Model Quantization and Compilation** | **Covers model quantization to reduce memory footprint and the use of the TI-TVM compiler to compile models targeting on-chip hardware accelerators such as the NPU available on CC27xx devices. Highly recommended.** |
| **System Integration and Wireless Connectivity** | **Covers integrating an Edge AI inference pipeline into a complete embedded system with wireless connectivity — directly applicable to this project.** |

> **Note on this reference design:** This project does **not** use TI Edge AI Studio to generate the model. Instead, it follows the **Bring Your Own Model (BYOM)** approach: the keyword recognition model was trained using a custom Python-based AI pipeline built on PyTorch. The model was then quantized and compiled externally using the TI-TVM compiler to target the CC2745R10 hardware.
>
> The steps covered in **Lab 2 (Advanced Model Quantization and Compilation)** are essential for anyone adapting this design with a new or retrained model. That lab explains in detail how to quantize your model for efficient IN8 fixed-point inference, and how to use the TI-TVM compiler to generate a deployable binary that leverages the hardware accelerators, including the NPU, available on the CC27xx device family.

---

## Host Visualization

Located in `Central BLE Receiver/Host Visualization/`. Connects to the central receiver over UART and displays a real-time inference dashboard (keyword class + probability bar chart) using Python Dash.

```bash
cd "Central BLE Receiver/Host Visualization"
pip install -r requirements.txt
python realtime_inference_voice.py
```

Dependencies: `pyserial`, `numpy`, `plotly`, `dash`.

Open the browser at `http://localhost:8050` after launching.

---

## Dependencies Summary

| Component | Device | SDK | CCS |
|-----------|--------|-----|-----|
| Peripheral Sensor | CC2745R10 | simplelink_lowpower_f3_sdk_9_12_00_19 | CCS Theia 20.2.0 |
| Central Receiver | CC2340R53 | simplelink_lowpower_f3_sdk_9_12_00_19 | CCS Theia 20.2.0 |

---

## References

- [SimpleLink Academy — Edge AI Labs](https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AEIJm0rwIeU.2P1OBWwlaA__SIMPLELINK-ACADEMY-CC23XX__gsUPh5j__LATEST)
- [BLE5-Stack User's Guide — F3](https://software-dl.ti.com/simplelink/esd/simplelink_lowpower_f3_sdk/9.12.00.19/exports/docs/ble5stack/ble_user_guide/html/ble-stack-5.x-guide/index-cc23xx.html)
- [BLE E2E Forum](https://e2e.ti.com/support/wireless-connectivity/bluetooth-group/bluetooth/f/bluetooth-forum)
