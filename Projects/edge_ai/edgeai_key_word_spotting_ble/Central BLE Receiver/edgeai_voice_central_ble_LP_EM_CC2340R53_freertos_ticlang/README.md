# EdgeAI Voice Keyword Recognition — BLE Central Receptor (CC2340R53)

BLE central device that scans for, connects to, and receives voice keyword inference results from the EdgeAI BLE Peripheral Sensor. Forwards raw inference results over UART to a host PC for real-time visualization.

## Dependencies

- TI Device: CC2340R53RKP (Cortex-M0+)
- TI SDK: simplelink_lowpower_f3_sdk_9_12_00_19
- TI Code Composer Studio: CCS 20.5.1
- RTOS: FreeRTOS

## System Overview

```
[Peripheral Sensor] --BLE notify--> [CC2340R53 Central] --UART 1.5Mbaud--> [Host PC / Visualization]
```

---

## Features

### BLE Scanning & Discovery

- **Scan type:** Active (sends scan requests).
- **Scan interval / window:** 500 ms / 500 ms (continuous, no idle gap).
- **Scan duration:** 10 seconds per scan batch; restarts automatically.
- **Duplicate filter:** Enabled; each peripheral address reported once per scan batch.
- **Max cached reports:** 20 advertising reports buffered.

**Peripheral identification:** The central connects to a device advertising manufacturer-specific company ID `0xAABB` and/or device name `"EdgeAI BLE"`. Service UUID filtering is enabled (`DEFAULT_DEV_DISC_BY_SVC_UUID = true`) so only matching peripherals are considered.

---

### Connection & GATT Sequence

After a matching peripheral is found, the following sequence executes automatically:

1. **Initiate connection** — `Central_connect()` with fixed 500 ms connection interval, 0 latency, 20,000 ms supervision timeout.
2. **MTU exchange** — 3 seconds after link establishment, `Exchange_gattMTU()` requests 247-byte effective MTU (client RX MTU = 251 − 4 header bytes). This enables the 17-byte inference result notification.
3. **Enable notifications** — `Enable_notification()` writes `0x0001` to CCCD handle `0x29` (CHAR4 notification descriptor).
4. **Start sensor** — `GATT_write(1)` writes `0x01` to CHAR1 handle `0x25`, triggering ADC capture to start Analog MIC data acquisition and inference on the peripheral.
5. **Receive notifications** — GATT `ATT_HANDLE_VALUE_NOTI` events deliver 17-byte inference result frames at ~1 Hz.
6. **Forward over UART** — each 17-byte frame is written verbatim to UART at 1.5 Mbaud.

**Connection parameters:**

| Parameter               | Value          |
|-------------------------|----------------|
| Initiator scan interval | 10 ms          |
| Initiator scan window   | 10 ms          |
| Connection interval     | 500 ms (fixed) |
| Connection latency      | 0              |
| Supervision timeout     | 20,000 ms      |
| ATT MTU                 | 251 bytes      |

---

### Inference Result Reception

Received GATT notifications carry the 17-byte inference result payload from the peripheral:

| Offset | Type         | Field               | Description                                     |
|--------|--------------|---------------------|-------------------------------------------------|
| 0      | `uint8_t`    | `inferred_class`    | Predicted class (0=left, 1=right, 2=up, 3=down) |
| 1–4    | `float` (LE) | `up_probability`    | Softmax score for "up"                          |
| 5–8    | `float` (LE) | `down_probability`  | Softmax score for "down"                        |
| 9–12   | `float` (LE) | `left_probability`  | Softmax score for "left"                        |
| 13–16  | `float` (LE) | `right_probability` | Softmax score for "right"                       |

On each notification:
- LED Red toggles.
- The 17-byte frame is forwarded verbatim to UART (`UART2_write`).

---

### UART Host Interface

- **Baud rate:** 1,500,000 bps.
- **Frame format:** Raw binary, 17 bytes per inference result.
- **Direction:** Central → Host PC only (no host commands expected).
- **Interface:** XDS110 onboard UART (same USB connection used for flashing/debugging).

The host PC `Host Visualization/realtime_inference_voice.py` consumes these frames.

---

### Host Visualization (Python)

**File:** `Host Visualization/realtime_inference_voice.py`

A real-time Plotly/Dash web dashboard that reads the UART stream and visualizes inference results.

**Features:**
- **Softmax probability chart:** 4 live traces (LEFT, RIGHT, UP, DOWN) over time; last 200 frames displayed.
- **Predicted class label:** Large text display of the current detected keyword.
- **CSV export:** Button saves the current raw data buffer to CSV.
- **Refresh rate:** 500 ms.

**Frame parsing:**
```python
class_id, left, right, up, down = struct.unpack('<Bffff', frame[:17])
```

**Class mapping:**
```python
{0: "LEFT", 1: "RIGHT", 2: "UP", 3: "DOWN", 4: "BACKGROUND"}
```

**Running the dashboard:**
```bash
pip install dash plotly pyserial
python realtime_inference_voice.py
```

*Note:* Make sure to change the COM port you are using inside the python script.

---

### LED Status

| Color      | Condition                                   |
|------------|---------------------------------------------|
| Green ON   | MTU exchange complete; notifications active |
| Red toggle | Inference result notification received      |

---

## BLE GATT Client Operations

The central acts as a GATT client. It does not host services; it discovers and operates the peripheral's Simple GATT Profile.

| Operation                        | Handle | Value                    | Trigger               |
|----------------------------------|--------|--------------------------|-----------------------|
| Write CHAR1 (start)              | `0x25` | `0x01`                   | After MTU exchange    |
| Write CCCD CHAR4 (notify enable) | `0x29` | `0x00, 0x01`             | After MTU exchange    |
| Receive CHAR4 notification       | `0x28` | 17-byte inference result | ~1 Hz from peripheral |

> **Note:** Handles `0x25`, `0x28`, and `0x29` are hardcoded to match the peripheral's Simple GATT Profile attribute table. If the peripheral GATT table changes, update these values in `app/app_GATT_commands.c`.

---

## Pin Configuration

Modify pin assignments in `ble_remote_control.syscfg` (SysConfig).

### GPIO Summary

| Signal    | GPIO           | Direction | Notes                            |
|-----------|----------------|-----------|----------------------------------|
| LED Green | boosterpack.40 | Output    | Notifications active indicator   |
| LED Red   | boosterpack.39 | Output    | Toggles on each inference result |
| UART TX   | boosterpack.4  | Output    | XDS110 UART → Host PC (1.5 Mbaud)|
| UART RX   | boosterpack.3  | Input     | XDS110 UART (unused for input)   | 
| Button 0  | boosterpack.13 | Input     | User button                      |
| Button 1  | boosterpack.12 | Input     | User button                      |

---

## Build Configuration

No application-specific compiler defines are required beyond SDK defaults. Standard symbols active in Release:

| Symbol                | Purpose                  |
|-----------------------|--------------------------|
| `CC23X0`              | CC2340 device family     |
| `FREERTOS`            | FreeRTOS kernel          |
| `ICALL_NO_APP_EVENTS` | Disable iCall app events |
| `NVOCMP_NVPAGES=6`    | NV storage pages         |

---

## Developing with this Reference Design

### Changing the Target Peripheral

The central identifies the peripheral by manufacturer company ID `0xAABB`. To connect to a different peripheral:

1. Update the company ID filter in `app/app_observer.c`.
2. Update the hardcoded GATT handles in `app/app_GATT_commands.c` (`0x25` for CHAR1, `0x28`/`0x29` for CHAR4/CCCD) to match the new peripheral's attribute table.
3. Update the notification payload parser in `app/app_data.c` and the host visualization script if the payload format changes.

### Changing the UART Baud Rate

Update the baud rate in log/logdebug.c (`Debug_UART_Config(void)`) and in the host Python script (`serial.Serial(port, baudrate=1500000)`).

### Adding Service Discovery (Dynamic Handles)

Currently GATT handles are hardcoded. To make the central robust to peripheral firmware changes, implement full GATT service discovery using `GATT_DiscAllPrimaryServices()` and `GATT_DiscAllChars()` in `app_GATT_commands.c`, then cache the discovered handles before enabling notifications.

---

## References

- [BLE5-Stack User's Guide](https://software-dl.ti.com/simplelink/esd/simplelink_lowpower_f3_sdk/9.12.00.19/exports/docs/ble5stack/ble_user_guide/html/ble-stack-5.x-guide/index-cc23xx.html)
- [SimpleLink Academy CC23XX](https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AEIJm0rwIeU.2P1OBWwlaA__SIMPLELINK-ACADEMY-CC23XX__gsUPh5j__LATEST)
- [BLE E2E Forum](https://e2e.ti.com/support/wireless-connectivity/bluetooth-group/bluetooth/f/bluetooth-forum)
