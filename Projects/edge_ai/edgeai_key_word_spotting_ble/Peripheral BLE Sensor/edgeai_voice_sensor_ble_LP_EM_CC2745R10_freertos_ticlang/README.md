# EdgeAI Voice Keyword Recognition — BLE Peripheral Sensor (CC27xx)

On-device voice keyword recognition reference design for the CC2745/CC2755. Captures audio via ADC, extracts MFCC features, runs a TVM neural network on-device, and transmits inference results to a connected BLE central over GATT notifications.

## Dependencies

- TI Device: CC2745/CC2755 (Cortex-M33)
- TI SDK: simplelink_lowpower_f3_sdk_9_12_00_19
- TI Code Composer Studio: CCS 20.5.1
- RTOS: FreeRTOS

## System Overview

```
[Microphone] → ADC (16 kHz) → MFCC Feature Extraction → TVM Inference → BLE Notification → [Central]
```

---

## Features

### Audio Acquisition

- **Interface:** ADCBuf driver in continuous ping-pong mode.
- **Sample rate:** 16 kHz, 12-bit resolution (normalized to 0–2048 range).
- **DMA:** Channel DMA_CH3; two 512-sample ping-pong buffers (`sampleBufferOne` / `sampleBufferTwo`).
- **Frame size:** 512 samples = 32 ms per frame.
- **Collection window:** 16,000 samples (1 second) assembled from 31 frames before inference.
- **ADC callback** (`adcBufCallback`) posts each filled 512-sample buffer to a 4-element message queue consumed by the inference task.

**Sensor state machine (`sensor_app.c`):**

| State | Description |
|-------|-------------|
| `SENSOR_STATE_WAIT` | Idle; waits for BLE trigger semaphore |
| `SENSOR_STATE_START` | Opens ADCBuf at 16 kHz, starts DMA |
| `SENSOR_STATE_GETDATA` | Drains frame queue, feeds pipeline |
| `SENSOR_STATE_STOP` | Closes ADCBuf, resets pipeline |

Triggered by a BLE write to CHAR1 (`0x01` = start, `0x00` = stop).

---

### MFCC Feature Extraction

**File:** `feature_extraction/mfcc.c`, `feature_extract_app.c`

Full MFCC pipeline per 512-sample frame:

1. **Hann windowing** — precomputed window applied to raw PCM (`hann.h`).
2. **FFT** — 1024-point real FFT via ARM CMSIS-DSP `arm_rfft_fast_f32()`.
3. **Power spectrum** — complex magnitude via `arm_cmplx_mag_f32()`, 513 bins.
4. **Mel filterbank** — 40 triangular mel-scale filters applied to power spectrum (precomputed in `mel_fb.h`).
5. **Log power (dB)** — Librosa-compatible `power_to_db()` with `top_db = 80.0`.
6. **DCT** — precomputed DCT matrix (`dct_mat.h`) reduces 40 mel bands to 13 MFCC coefficients.

**MFCC parameters:**

| Parameter | Value |
|-----------|-------|
| Sample rate | 16,000 Hz |
| FFT size | 1024 |
| Window length | 1024 samples |
| Hop length | 512 samples (50% overlap) |
| Mel bands | 40 |
| MFCC coefficients | 13 |
| Processing time | ~3.5 ms per frame |

The feature buffer accumulates 60 frames × 13 coefficients = 780 float values using a sliding window (shift 512 samples left, append new MFCC row).

---

### TVM Neural Network Inference

**Files:** `voice_keyword_recognition.c`, `ai_artifacts/` (TVM compiled model)

**Model:** TVM-compiled CNN (`tvmgen_default`) trained on Google Speech Commands subset.

**Input tensor:** `voice_input[1][60][13]` — 1-second MFCC feature map (batch × frames × coefficients).

**Output tensor:** `voice_output[1][4]` — softmax probabilities for 4 classes.

| Index | Keyword |
|-------|---------|
| 0 | left |
| 1 | right |
| 2 | up |
| 3 | down |

**Inference loop:**
1. Collect 60 MFCC frames from the feature extraction pipeline.
2. Copy feature buffer into TVM input: `voice_input[1][60][13]`.
3. Execute: `tvmgen_default_run(&nn_inputs, &nn_outputs)`.
4. Argmax: `FeatureExtract_findClass()` returns the predicted class index.
5. Schedule BLE notification: `BLEAppUtil_invokeFunctionNoData(InferenceNotify)`.

**Inference performance:** ~11.5 ms (software path, Cortex-M33 at full speed).

**Important!:** If the device is not advertising, check that the HSM firmware has been flashed! Download latest version of UNIFLASH,
connect to the device, go to `Settings & Utilities` and scroll down to HSM info. Click Read HSM System Information. If it comes back
empty scroll to the next section and enter the HSM path in the SDK: `\bin\hsm\cc27xx10_hsm_fw_v3.0.0.bin` and click on Program HSM image.

---

### BLE Peripheral & Advertising

- **Device name:** `"EdgeAI BLE"`
- **Advertising interval:** 100 ms (160 × 0.625 ms).
- **Advertising type:** Connectable, legacy, scannable.
- **Manufacturer-specific data:** Company ID `0xAABB` in both advertisement and scan response.
- **Connection interval:** 500–1000 ms requested (400–800 × 1.25 ms).
- **Supervision timeout:** 6,000 ms.
- **Max MTU:** 251 bytes.
- **Max bonds:** Configurable via GAP Bond Manager.

---

## BLE GATT Profile

Service uses the TI Simple GATT Profile with a proprietary UUID.

| Characteristic | Handle | Properties | Size | Description |
|----------------|--------|------------|------|-------------|
| CHAR1 (Control) | `0x25` | Read, Write | 1 byte | `0x01` starts ADC + inference; `0x00` stops. Written by central after connection. |
| CHAR4 (Result) | `0x28` | Notify | 17 bytes | Inference result notification (see payload below). |

**CHAR4 notification payload (17 bytes, packed):**

| Offset | Type | Field | Description |
|--------|------|-------|-------------|
| 0 | `uint8_t` | `inferred_class` | Predicted class index (0=left, 1=right, 2=up, 3=down) |
| 1–4 | `float` | `left_probability` | Softmax probability for "left" |
| 5–8 | `float` | `right_probability` | Softmax probability for "right" |
| 9–12 | `float` | `up_probability` | Softmax probability for "up" |
| 13–16 | `float` | `down_probability` | Softmax probability for "down" |

Notification rate: ~1 per second (one inference per 1-second audio window).

---

## Pin Configuration

Modify pin assignments in `edgeai_voice_sensor_ble.syscfg` (SysConfig).

### GPIO Summary

| Signal | GPIO | Direction | Notes |
|--------|------|-----------|-------|
| LED Green | boosterpack.19 | Output | Idle / ready status |
| LED Red | boosterpack.36 | Output | Active during feature extraction and inference |
| ADC IN | boosterpack.10 (DIO28)| Analog Input | Microphone / audio source (CONFIG_ADCBUF_0) |
| UART TX | boosterpack.2 | Output | Debug console |
| UART RX | boosterpack.6 | Input | Debug console |
| Button 0 | boosterpack.30 | Input | User button |
| Button 1 | boosterpack.40 | Input | User button |

---

## Developing with this Reference Design

### Project Structure

```
ble_app/              — BLE peripheral application layer
  Profiles/           — GATT service (app_simple_gatt.c, app_dev_info.c)
  app_main.c          — Application entry, task init
  app_peripheral.c    — Advertising setup and management
  app_connection.c    — Connection and parameter update
  app_data.c          — GATT write handler (CHAR1 trigger)
  app_inference_notify.c — Builds and sends CHAR4 notification
sensor_app/           — ADC state machine (sensor_app.c)
feature_extraction/   — MFCC pipeline (mfcc.c, feature_extract_app.c)
  cmsisdsp/           — ARM CMSIS-DSP FFT/DCT support
ai_artifacts/         — TVM compiled model (tvmgen_default)
voice_keyword_recognition.c — Inference loop (main ML entry point)
common/               — BLEAppUtil framework, iCall, SDK drivers
```

### Changing Keywords or Retraining the Model

1. Retrain or fine-tune a model on new keywords using a TVM-compatible framework (e.g., TensorFlow Lite → TVM compilation).
2. Replace the artifacts in `ai_artifacts/` with the newly compiled TVM model.
3. Update the output label array in `voice_keyword_recognition.c` to match the new class count and names.
4. If the number of classes changes, update the `CHAR4` notification payload size and the `Sensor_InferenceResult` struct in `sensor_app.h`.

### Changing MFCC Parameters

All MFCC parameters are defined in `feature_extraction/mfcc.h`. If you change `N_FFT`, `N_MELS`, or `N_MFCC`, the precomputed tables in `hann.h`, `mel_fb.h`, and `dct_mat.h` **must be regenerated** to match. Use a Python script (e.g., librosa) to generate new header files with updated constants.

### Changing the Audio Source

- **ADC channel:** Change `CONFIG_ADCBUF_0` pin in SysConfig.
- **Sample rate:** Change `SENSOR_SAMPLE_RATE` in `sensor_app.h` and regenerate MFCC tables accordingly.

### Tuning Inference Throughput

- Reduce `N_MAX_FRAMES` (currently 60) to shorten the audio window and increase notification rate, at the cost of accuracy.
- Reduce `HOP_LENGTH` (currently 512) for finer temporal resolution, which increases MFCC compute time.
- The TVM model in `ai_artifacts/` can be recompiled with different optimization targets (e.g., enabling the CDE custom accelerator) to reduce the ~11.5 ms inference time.

---

## References

- [BLE5-Stack User's Guide](https://software-dl.ti.com/simplelink/esd/simplelink_lowpower_f3_sdk/9.12.00.19/exports/docs/ble5stack/ble_user_guide/html/ble-stack-5.x-guide/index-cc23xx.html)
- [SimpleLink Academy CC27XX](https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AEIJm0rwIeU.2P1OBWwlaA__SIMPLELINK-ACADEMY-CC23XX__gsUPh5j__LATEST)
- [Getting Started with Edge AI and TI Edge AI Studio](https://dev.ti.com/tirex/content/simplelink_academy_for_low_power_f3_sdk_9_14_00_03/_build_simplelink_academy_for_low_power_f3_sdk_9_14_00_03/source/edgeai/edgeai_01_getting_started.html)
- [TI-TVM Documentation](https://software-dl.ti.com/mctools/nnc/mcu/users_guide/index.html)
- [ARM CMSIS-DSP](https://arm-software.github.io/CMSIS-DSP/latest/)
- [BLE E2E Forum](https://e2e.ti.com/support/wireless-connectivity/bluetooth-group/bluetooth/f/bluetooth-forum)
