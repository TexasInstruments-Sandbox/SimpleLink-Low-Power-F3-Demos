# Single-Wire UART Controller — LP-EM-CC2745R10-Q1 (FreeRTOS, TI Clang)

## Table of Contents

- [Introduction](#introduction)
- [Features](#features)
- [Prerequisites](#prerequisites)
  - [Hardware Requirements](#hardware-requirements)
  - [Software Requirements](#software-requirements)
- [Hardware Setup](#hardware-setup)
  - [Physical Connections](#physical-connections)
  - [Pin Configuration](#pin-configuration)
- [How to Use](#how-to-use)
  - [Build and Flash](#build-and-flash)
  - [Operating Modes](#operating-modes)
  - [Button Controls](#button-controls)
  - [Serial Terminal Output](#serial-terminal-output)
- [Protocol Overview](#protocol-overview)
  - [Frame Structure](#frame-structure)
  - [Collision Detection and Avoidance](#collision-detection-and-avoidance)
  - [Timing Parameters](#timing-parameters)
- [Project Structure](#project-structure)
- [Troubleshooting](#troubleshooting)
- [References](#references)

---

## Introduction

This project implements the **controller** side of a patent-pending Single-Wire UART communication protocol on the [CC2745R10-Q1](https://www.ti.com/product/CC2745R10-Q1) wireless microcontroller. It runs on the [LP-EM-CC2745R10-Q1](https://www.ti.com/tool/LP-EM-CC2745R10-Q1) LaunchPad under FreeRTOS.

The Single-Wire UART protocol enables reliable, half-duplex bidirectional communication over a single shared conductor using the standard UART2 peripheral in open-drain mode. The controller manages up to two addressed peripheral/tag devices (MSPM0L1117 or equivalent) and exercises five operating modes: ID assignment, broadcast, unicast, repeated unicast, and active-send receive.

This project is designed to be used together with a companion peripheral implementation running on an MSPM0L1117. Together they form a complete reference design suitable for Electronic Shelf Label (ESL) systems, distributed sensor networks, and any application requiring minimal-wire peripheral connectivity.

---

## Features

- Half-duplex single-wire UART at **921,600 baud** using open-drain TX configuration
- **Dynamic peripheral addressing** via 32-bit trace ID discovery and assignment
- **CSMA/CD collision avoidance and detection** using echo comparison and a 250 µs carrier-sense window
- **CRC-16/AUG-CCITT** data integrity on all data frames (software implementation, no hardware CRC required)
- **Five operating modes** selectable at runtime via the LaunchPad buttons
- **Peripheral wakeup pulse** (50 µs) to rouse sleeping tag devices before each transaction
- **Bounded retry logic** — up to 5 retries per command with deterministic failure reporting
- FreeRTOS task architecture with POSIX semaphore synchronisation between UART callbacks and the application task
- Debug output via XDS110 UART (UART0) at 115,200 baud

---

## Prerequisites

### Hardware Requirements

- 1x [LP-EM-CC2745R10-Q1 LaunchPad](https://www.ti.com/tool/LP-EM-CC2745R10-Q1) (controller)
- 1–2x MSPM0L1117 peripheral boards running the companion single-wire UART peripheral firmware
- 1x external pull-up resistor: **1 kΩ** (connected to VDD of the shared bus)
- USB cable for flashing and UART debug output
- Jumper wire to connect the shared bus wire between the controller and peripheral(s)

### Software Requirements

- **SDK**: SimpleLink LowPower F3 SDK `v9.14.00.39`
- **IDE**: Code Composer Studio (CCS) 20.x or later
- **Compiler**: TI Clang (included with CCS)
- **Serial terminal**: PuTTY, Tera Term, or equivalent — 115,200 baud, 8N1, no flow control

---

## Hardware Setup

### Physical Connections

All nodes share a single bus wire. Connect the TX and RX pins of every device together at a single node and tie that node to VDD through the pull-up resistor. Configure every UART TX pin as open-drain so that a transmitting node pulls the wire low without conflicting with other nodes.

```
LP-EM-CC2745R10-Q1           MSPM0L1117 (Tag 1)       MSPM0L1117 (Tag 2)
  BP.7  (UART2 TX) ─────┐     UART_TX ─────┐             UART_TX ─────┐
  BP.6  (UART2 RX) ──┐  │     UART_RX ──┐  │             UART_RX ──┐  │
                    │  └─────────────────┘  └─────────────────────┘  │
                    └──────────────────────────────────────────────────┘
                                                                       │
                              VDD ──[1 kΩ]────────────────────────┘
```

> **Note:** The pull-up value governs the RC time constant with bus capacitance. At 921,600 baud (one bit = 1.09 µs) a 1 kΩ pull-up is appropriate for typical bench-length wiring. For longer buses or higher capacitance, use 1 kΩ or lower.

### Pin Configuration

The following pins are configured by SysConfig (`singlewireuart.syscfg`) and assigned in the generated `ti_drivers_config.h`.

| Function | LaunchPad Header | GPIO | Driver Configuration |
|----------|-----------------|------|----------------------|
| Single-wire UART TX | BP.7 | GPIO3 | UART2_1 TX, open-drain |
| Single-wire UART RX | BP.6 | GPIO22 | UART2_1 RX, input |
| Button 0 — Mode select | BP.30 | GPIO20 | Active-low input, BTN-1 |
| Button 1 — Trigger action | BP.40 | GPIO0 | Active-low input, BTN-2 |
| LED RED — Status/error | BP.36 | GPIO16 | Output |
| LED GREEN — Status/activity | BP.19 | GPIO12 | Output |
| Debug serial TX/RX | XDS110 back-channel | — | UART0, 115,200 baud |

The UART2_1 TX pin is configured as open-drain by the driver using `GPIO_CFG_OUT_OD_LOW` so that it presents high impedance when idle, enabling wired-AND bus operation:

```c
GPIO_setConfigAndMux(hwAttrs->txPin,
                     GPIO_CFG_OUT_OD_LOW | GPIO_CFG_OUT_STD | GPIO_CFG_OUT_STR_HIGH,
                     hwAttrs->txPinMux);
```

---

## How to Use

### Build and Flash

1. Import the project into Code Composer Studio using **File → Import → CCS Projects**.
2. Ensure the SDK path resolves to `simplelink_lowpower_f3_sdk_9_14_00_39` in the project properties.
3. Build the project (**Project → Build All**).
4. Connect the LP-EM-CC2745R10-Q1 via USB and flash (**Run → Debug**, then **Run**).
5. Open a serial terminal and connect to the XDS110 Application/User UART COM port at **115,200 baud, 8N1**.

### Operating Modes

The controller cycles through five modes, each exercising a distinct protocol capability. The current mode is printed to the serial terminal on every state change.

| Mode | Value | Description |
|------|-------|-------------|
| `ASSIGN_ID` | 0x01 | Listen for `CMD_PLUGIN_NOTIFY` from new peripherals and respond with `CMD_ACK` to assign their trace ID |
| `REPEAT_UNICAST` | 0x02 | Continuously send unicast commands to each addressed peripheral in sequence |
| `BROADCAST` | 0x03 | Transmit a single broadcast command to all addressed peripherals simultaneously |
| `UNICAST` | 0x04 | Send one unicast command to a single specific peripheral |
| `ACTIVE_SEND` | 0x05 | Stop controller-initiated traffic and wait to receive unsolicited data from peripherals |

The recommended bring-up sequence is:

1. Power on the peripheral(s) first so they queue a `CMD_PLUGIN_NOTIFY` advertisement.
2. Start the controller in **ASSIGN_ID** mode using Button 0.
3. Once IDs are assigned (confirmed on the serial terminal), select the desired test mode.

### Button Controls

| Button | Action |
|--------|--------|
| BTN-1 (BP.30) | Advance to the next operating mode (cycles ASSIGN_ID → REPEAT_UNICAST → BROADCAST → UNICAST → ACTIVE_SEND → ASSIGN_ID) |
| BTN-2 (BP.40) | Trigger a one-shot action in the current mode (for example, send a single broadcast or unicast command) |

### Serial Terminal Output

The debug serial terminal reports each protocol event. Example output for a typical bring-up and unicast exchange:

```
Mode: ASSIGN_ID
Received PLUGIN_NOTIFY from Tag ID: 0xA1B2C3D4
Assigned ID to Tag 0
Received PLUGIN_NOTIFY from Tag ID: 0xD4C3B2A1
Assigned ID to Tag 1
Mode: UNICAST
Collision avoidance OK
TX command to Tag 0xA1B2C3D4 ...
RX ACK from Tag 0xA1B2C3D4
Unicast complete
```

If a collision is detected during the command phase the controller retries automatically (up to 5 times) and prints `CD failed!` if all retries are exhausted.

---

## Protocol Overview

### Frame Structure

Every controller-to-peripheral transaction is:

```
[WAKEUP PULSE 50 µs] [COMMAND FRAME 9 bytes] [DATA FRAME variable]
```

Every peripheral-to-controller transaction is:

```
[BREAK PREAMBLE 140 µs] [COMMAND FRAME 9 bytes] [DATA FRAME variable]
```

**Command frame (9 bytes):**

| Byte | Field | Description |
|------|-------|-------------|
| 0 | `cmdCode` | Command type (see table below) |
| 1 | `reserved[7:4]` / `cmdSeq[3:0]` | Sequence nibble used for ACK matching |
| 2–5 | `TagID` | 32-bit target device ID (little-endian); `0xFFFFFFFF` = broadcast |
| 6–7 | `dataLength` | Length of the following data frame in bytes (big-endian) |
| 8 | `checksum` | 8-bit arithmetic sum of bytes 0–7 modulo 256 |

**Command codes:**

| Code | Command | Direction |
|------|---------|-----------|
| `0x00` | `CMD_BROADCAST` | Controller → All peripherals |
| `0x3A` | `CMD_UNICASTING` | Controller → One peripheral |
| `0x5A` | `CMD_ASSIGN_ID` | Controller → Unaddressed peripheral |
| `0x35` | `CMD_ACTIVE_SEND` | Peripheral → Controller |
| `0xAA` | `CMD_PLUGIN_NOTIFY` | Peripheral → Controller (advertisement) |
| `0x55` | `CMD_ACK` | Peripheral → Controller (acknowledgment) |

**Data frame layout:**

```
[SOF 's'] [LENGTH_H] [LENGTH_L] [PAYLOAD × N] [CRC16_H] [CRC16_L] [EOF 'e']
```

The CRC-16/AUG-CCITT checksum (polynomial `0x1021`, seed `0x1D0F`) is computed over the length and payload fields. The CC2745R10-Q1 does not have a hardware CRC peripheral, so the checksum is computed in software (`crc16_aug_ccitt()`).

### Collision Detection and Avoidance

The controller applies a three-layer CSMA/CD strategy before and during each transmission:

1. **Carrier sense (pre-transmission):** The `collisionAvoidance()` function temporarily reconfigures the RX GPIO as an input, arms a falling-edge interrupt, and starts a 250 µs timer. If bus activity is detected within the window, the transmission is deferred.

2. **Echo-based collision detection (during command TX):** The controller simultaneously issues `UART2_read` and `UART2_write` for the command frame. After the write callback posts the semaphore, the received echo is compared byte-for-byte against the transmitted buffer. A mismatch indicates a collision; the controller retries up to `NUM_RETRIES = 5` times.

3. **Data-frame echo check:** The same read-while-write technique is applied to the data frame, providing end-to-end integrity verification of every transmission.

### Timing Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| Baud rate | 921,600 bps | Single-byte transfer time ≈ 10.4 µs |
| Wakeup pulse | 50 µs | Low pulse asserted by controller before each transaction |
| Carrier-sense window | 250 µs | `CA_DELAY` — bus must remain idle for this duration |
| Maximum retries | 5 | `NUM_RETRIES` — collision detection retries per frame |
| Maximum peripherals | 2 | `MAXIMUM_ID` — addressable peripheral count in this demo |
| RX ring buffer | 1,024 bytes | Configured in SysConfig for UART2_1 |
| TX ring buffer | 1,024 bytes | Configured in SysConfig for UART2_1 |

---

## Project Structure

```
uart2_singlewire_controller_LP_EM_CC2745R10_Q1_freertos_ticlang/
├── main_freertos.c          # FreeRTOS entry point; creates the main application thread
├── singlewireuart.c         # Controller application: all operating modes, UART callbacks,
│                            #   collision avoidance, wakeup pulse, CRC-16 implementation
├── singlewireuart.h         # Data structures (COMMAND_PACKET, payload types), state
│                            #   and event enumerations, buffer-size constants
├── singlewireuart.syscfg    # SysConfig source: UART2, GPIO, Button, LED, FreeRTOS config
├── lpf3_freertos.cmd        # Linker command file (FLASH / SRAM memory map)
├── targetConfigs/
│   └── CC2745R10.ccxml      # XDS110 debug probe configuration
└── Debug/                   # Build output (generated; not checked in)
    └── syscfg/              # Auto-generated driver configuration files
        ├── ti_drivers_config.c / .h
        ├── FreeRTOSConfig.h
        └── ...
```

---

## Troubleshooting

| Symptom | Likely Cause | Resolution |
|---------|--------------|------------|
| No communication — peripheral never responds | Missing or incorrect pull-up resistor | Verify 1 kΩ pull-up to VDD on the shared bus wire |
| Frame errors / garbled data on serial terminal | Baud rate mismatch | Confirm both controller and peripheral are configured for 921,600 baud, 8N1 |
| `CD failed!` printed repeatedly | Bus noise or missing pull-up causing spurious low transitions | Check pull-up value and bus wire length; increase `CA_DELAY` if needed |
| Peripheral never receives `ASSIGN_ID` acknowledgment | Controller not in `ASSIGN_ID` mode when peripheral powers on | Press BTN-1 to return to `ASSIGN_ID` mode, then power cycle the peripheral |
| `PLUGIN_NOTIFY` not seen by controller | Peripheral firmware not running or TX/RX wires crossed | Verify TX and RX pins of all nodes are wired to the same bus conductor |
| LEDs not responding during broadcast test | ID not yet assigned to any peripheral | Run `ASSIGN_ID` mode first before switching to `BROADCAST` |

---

## References

1. Application Note — *Single-Wire UART Communication Between CC2745R10-Q1 Controller and MSPM0L1117 Peripheral/Tag*
2. [CC2745R10-Q1 Product Page](https://www.ti.com/product/CC2745R10-Q1)
3. [LP-EM-CC2745R10-Q1 LaunchPad](https://www.ti.com/tool/LP-EM-CC2745R10-Q1)
4. [SimpleLink LowPower F3 SDK](https://www.ti.com/tool/SIMPLELINK-LOWPOWER-F3-SDK) — v9.14.00.39
5. [TI Drivers UART2 API Reference](https://software-dl.ti.com/simplelink/esd/simplelink_lowpower_f3_sdk/latest/exports/docs/tidrivers/doxygen/html/_u_a_r_t2_8h.html)
