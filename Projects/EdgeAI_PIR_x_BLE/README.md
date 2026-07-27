# EdgeAI_PIR_x_BLE

## Introduction

The EdgeAI_x_BLE demo demonstrates the integration of on-device neural network inference with
Bluetooth® Low Energy (BLE)
communication on the Texas Instruments [LP-EM-CC2745R10-Q1 LaunchPad]. 
The project leverages the CC2745R10's hardware CDE neural network co-processor 
alongside its integrated 2.4 GHz BLE radio to deliver a
complete edge AI pipeline within a single chip, with no cloud connectivity required.

The device samples a PIR (Passive Infrared) sensor via ADC, performs signal processing and feature extraction
using ARM CMSIS-DSP, runs a TVM-compiled three-class classifier, and transmits the 4-byte inference result to
a connected smartphone over a BLE GATT notification — entirely on the device, in real time.

This project was built by following the SLA titled *Integrating Edge AI Inference with Wireless Connectivity
via Bluetooth Low Energy*, which demonstrates how to enrich a `basic_ble` project from the SimpleLink F3 SDK
with neural network inferencing capabilities sourced from the `edgeai_smart_pir_detection` example. Furthermore,
this project uses a pre-built CMSIS-DSP Library that is only functional with PIR sensors. If you would like to
use Edge AI inferencing capabilities with a sensor other than a PIR sensor, please refer to the 
*Manual `CMSIS-DSP` Installation* section of the 
*Integrating Edge AI Inference with Wireless Connectivity via Bluetooth Low Energy*. 

---

## Features

- **On-device neural network inference**: 3-class PIR classifier (human / background / dog) running entirely
  on the CC2745R10, with no cloud dependency
- **Hardware-accelerated inference**: CDE co-processor enabled via `m33_cde_init()`, reducing inference
  latency by 1.3–1.5x compared to software-only execution
- **Timer-paced ADC sensor acquisition**: PIR sensor sampled at ~33 Hz via LGPTimer-triggered ADC
- **CMSIS-DSP feature extraction**: Windowed FFT, spectral entropy, dominant frequency detection, kurtosis,
  zero-crossing rate, and slope change detection produce a 25×25 float feature matrix
- **BLE peripheral role**: Advertises as `"BLE x Edge AI"`, accepts connections from smartphones and gateways
- **Remote start/stop control**: Write `1` to CHAR1 to start the inference pipeline, `0` to stop
- **BLE inference notifications**: GATT Notify on CHAR4 delivers a 4-byte packed result
  `{inferred_class, human%, background%, dog%}` on every inference cycle
- **FreeRTOS + POSIX pthreads**: Feature extraction and inference run in `mainThread` (POSIX pthread);
  sensor acquisition runs in `SensorThread` (FreeRTOS DPL task); the BLE stack runs under its own
  FreeRTOS scheduler
- **Power-aware design**: All tasks block on semaphores between inference cycles, allowing FreeRTOS to enter
  low-power standby automatically when no work is pending

---

## Prerequisites

### Hardware Requirements

- 1x [LP-EM-CC2745R10-Q1 LaunchPad][launchpad] — CC2745R10 with integrated BLE radio and CDE co-processor
- 1x XDS110 Debug Probe — included with the LaunchPad
- 1x PIR sensor connected to DIO21 (BoosterPack header pin 2), or the
  [Edge AI Sensor BoosterPack (TIDA-010997)][boosterpack]
- 1x Smartphone with Bluetooth® LE support, with the [TI SimpleLink Connect app][slc-app] installed

### Software Requirements

- **SDK**: [SimpleLink F3 SDK (v9.12.00.19)][f3-sdk]
- **Edge AI Plugin**: [SimpleLink SDK Edge AI Plugin][edgeai-plugin]
- **IDE**: [Code Composer Studio (CCS) 20.4 or later][ccs]
- **Compiler**: TI Clang (v4.0.4)

> **Note**: Always verify the SimpleLink F3 SDK version against the dependency section of the
> SimpleLink SDK Edge AI Plugin release notes. The required F3 SDK version is updated with each
> new Edge AI Plugin release.

---

## How the Project Works

### System Architecture

The firmware is organized as an end-to-end inference pipeline that runs continuously once started:

```text
PIR Sensor (ADC @ ~33 Hz via LGPTimer)
         |
         v
Sensor Acquisition — sensor_app.c
  |- LGPTimer interrupt posts semaphore to pace ADC conversions
  |- Raw ADC buffer collected (128 samples)
  `- Buffer delivered to mainThread via MessageQueue
         |
         v
Feature Extraction — feature_extract.c / feature_extract_app.c
  |- Sliding window segmentation of raw samples
  |- Per-window: FFT pooling, kurtosis, ZCR, slope changes, spectral entropy, top frequencies
  `- Output: 25-element float feature vector per window, 25 windows (25x25 feature matrix)
         |
         v
TVM Neural Network Inference — tvmgen_default_run() using mod.a
  |- Input tensor:  (1, 1, 25, 25) float32
  |- Output tensor: (1, 3) float32 logits [human, background, dog]
  `- Softmax applied to convert logits to per-class probabilities (0–100%)
         |
         v
BLE Notification — app_inference_notify.c
  `- GATT_Notification() on CHAR4 (ATT handle 40) sends 4 bytes to connected central
```

### FreeRTOS Thread Structure

| Thread           | Created By        | Role                                                      |
|------------------|-------------------|-----------------------------------------------------------|
| BLE Stack tasks  | `appMain()`       | Manages BLE advertising, connections, and GATT            |
| `mainThread`     | `EdgeAIMain()`    | Drives the sensor → feature → inference → notify loop     |
| `SensorThread`   | `Sensor_init()`   | Manages ADC state machine (WAIT / START / GETDATA / STOP) |

### CDE Co-Processor Initialization

The Cortex-M33 CDE co-processor must be enabled before any TVM model call executes. This is handled in
`main_freertos.c` by `m33_cde_init()`, which writes to the co-processor access control register at
`0xE000ED88` using inline assembly. `m33_cde_init()` is the very first call inside `int main()`,
before `Board_init()`.

### BLE GATT Service

The project exposes a Simple GATT service (UUID `0xFFF0`) with two active characteristics:

| Characteristic | UUID     | Properties  | Description                                                |
|----------------|----------|-------------|------------------------------------------------------------|
| CHAR1          | `0xFFF1` | Read, Write | Start/stop control. Write `0x01` to start, `0x00` to stop.|
| CHAR4          | `0xFFF4` | Notify      | 4-byte inference result (see Notification Format below).   |

> **Note**: The GATT attribute table in `simple_gatt_profile.c` has been reduced to CHAR1 and CHAR4
> only. This produces a table of exactly 8 entries, placing CHAR4's value handle at ATT handle 40,
> which matches `#define HANDLE_CHAR_4 40` in `app_inference_notify.c`.

### Notification Format

Each CHAR4 notification carries exactly 4 bytes, corresponding to the `Sensor_InferenceResult` struct:

| Byte | Field                    | Type      | Meaning                                                    |
|------|--------------------------|-----------|------------------------------------------------------------|
| 0    | `inferred_class`         | `uint8_t` | Winning class: `0` = human, `1` = background, `2` = dog   |
| 1    | `human_probability`      | `uint8_t` | Human class probability as integer percent (0–100)         |
| 2    | `background_probability` | `uint8_t` | Background class probability as integer percent (0–100)    |
| 3    | `dog_probability`        | `uint8_t` | Dog class probability as integer percent (0–100)           |

**Example**: Receiving `0x00 0x50 0x0F 0x05` on CHAR4 means:
- Inferred class = 0 → **human**
- Human probability = 80%
- Background probability = 15%
- Dog probability = 5%

The three probability bytes will approximately sum to 100, with ±2 variance due to float-to-uint8 rounding.

---

## Peripherals and Pin Assignments

When this project is built, SysConfig generates TI-Driver configurations into `ti_drivers_config.c` and
`ti_drivers_config.h`. The `basic_ble.syscfg` file can be opened in the SysConfig GUI to view all pin
and resource assignments.

| Peripheral | Instance            | Pin     | Function                                |
|------------|---------------------|---------|-----------------------------------------|
| ADC        | `CONFIG_ADC_0`      | DIO21   | PIR sensor signal input (8-bit, ~33 Hz) |
| LGPTimer   | `CONFIG_LGPTIMER_0` | LGPT1   | ADC sampling rate timer                 |
| BLE Radio  | —                   | 2.4 GHz | BLE advertising and GATT server         |

---

## How to Use

### 1. Build and Flash

1. Import the `EdgeAI_x_BLE` project into Code Composer Studio.
2. Confirm the build configuration is set to **Release** in the Project Properties or toolbar.
   The Debug configuration does not include the CMSIS-DSP, TF-M, and Edge AI Plugin linker paths
   and will fail to build.
3. Build the project (Project → Build All).
4. Flash the resulting binary to your LP-EM-CC2745R10-Q1 LaunchPad.

### 2. Connect via Smartphone

1. Download the [TI SimpleLink Connect app][slc-app] on your smartphone.
2. Open the app and select the **Scanner** option (top-left pane on the home screen).
3. Locate your device in the scanner list — it advertises as **`BLE x Edge AI`** by default.
   If you do not see it, tap the refresh button in the top-right corner of the scanner screen.
4. Tap the arrow on the right side of your device's row to connect.

### 3. Start Inference

1. Once connected, select **TI Simple Peripheral Service** from the service list on your phone.
2. In the event that you would rather view the inference results in decimal form rather than hex form,
   you are able to switch your mode by using the *format* dropdown menu and selecting the decimal option.
3. Enable notifications on **Characteristic 4** by toggling it on.
4. Tap the **write** row under **Characteristic 1**.
5. Type `1` and press enter. This writes `0x01` to CHAR1, setting the `start` flag to `1` inside
   the peripheral's GATT attribute store.
6. The sensor task unblocks, ADC sampling begins, and inference results will appear under
   **Characteristic 4** within approximately 4 seconds.

### 4. Stop Inference

Write `0` to **Characteristic 1** at any time. The sensor task returns to its WAIT state and the
device idles until a new start command is received.

### Interpreting the Results

The information below is true regardless of the format of the inference result (either decimal or hex).

The four bytes received on Characteristic 4 map directly to the `Sensor_InferenceResult` struct:

- **Byte 0** (`inferred_class`): The predicted class index — `0` for human, `1` for background,
  `2` for dog. 
- **Bytes 1–3**: Per-class probabilities as integer percentages (0–100). They will roughly sum
  to 100. If you selected hex format, then the per-class probabilities will be in hexadecimal form,
  it will be up to you to manually convert the results to decimal form. If you selected decimal
  format, then the per-class probabilities will be in decimal form, no conversion will be needed.

---

## Project Structure

```text
EdgeAI_x_BLE/
├── main_freertos.c               # Modified: m33_cde_init(), EdgeAIMain(), pthread_create
├── smart_detection_pir.c         # Main inference thread (mainThread)
├── basic_ble.syscfg              # SysConfig: ADC, LGPTimer, device name, FreeRTOS heap
├── cmsis_compiler.h              # CMSIS compiler abstraction (project-root copy)
├── cc27xx_freertos.cmd           # Linker command file
├── ai_artifacts/
│   ├── mod.a                     # Compiled TVM neural network static library
│   └── tvmgen_default.h          # TVM-generated inference function declarations
├── sensor/
│   ├── sensor_app.c              # ADC state machine and LGPTimer-paced sampling
│   └── sensor_app.h
├── feature_extraction/
│   ├── feature_extract.c         # CMSIS-DSP signal processing (FFT, kurtosis, ZCR, etc.)
│   ├── feature_extract.h
│   ├── feature_extract_app.c     # Feature extraction pipeline orchestrator
│   └── feature_extract_app.h
├── dsp/                          # ARM CMSIS-DSP source tree (v1.16.2)
│   ├── Include/
│   ├── PrivateInclude/
│   └── Source/
├── app/
│   ├── app_inference_notify.c    # GATT_Notification sender for inference results
│   ├── app_simple_gatt.c         # Modified GATT app (charValue1=0, CHAR4 notify removed)
│   └── Profiles/
│       └── app_dev_info.c
└── common/
    └── Startup/
        └── osal_icall_ble.c
```

---

## References

- [LP-EM-CC2745R10-Q1 LaunchPad][launchpad]
- [SimpleLink F3 SDK v9.12.00.19][f3-sdk]
- [SimpleLink SDK Edge AI Plugin][edgeai-plugin]
- [TI SimpleLink Connect Mobile App][slc-app]
- [Edge AI Sensor BoosterPack TIDA-010997][boosterpack]
- [TI Neural Network Compiler User Guide][nnc-ug]
- [Code Composer Studio][ccs]

<!-- Link definitions -->
[launchpad]:     https://www.ti.com/tool/LP-EM-CC2745R10-Q1
[f3-sdk]:        https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK/9.12.00.19
[edgeai-plugin]: https://www.ti.com/tool/download/SIMPLELINK-SDK-EDGEAI-PLUGIN
[slc-app]:       https://www.ti.com/tool/SIMPLELINK-CONNECT-SW-MOBILE-APP
[boosterpack]:   https://www.ti.com/tool/TIDA-010997
[nnc-ug]:        https://software-dl.ti.com/mctools/nnc/mcu/users_guide/index.html
[ccs]:           https://www.ti.com/tool/download/CCSTUDIO
