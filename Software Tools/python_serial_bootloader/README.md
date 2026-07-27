TI F3 Serial Bootloader Host
==========================================

This folder contains a Python script that communicates with the ROM Serial Bootloader (SBL) of the Texas Instruments CC23xx and CC27xx SoCs (System on Chips).
It can be used to erase, program, and verify the flash of those SoCs with a simple USB to serial converter.

### Requirements

To communicate with the UART port of the SoC you need a USB to serial converter:
* The ones we recommend are the [LP-XDS110ET](https://www.ti.com/tool/LP-XDS110ET) or [LP-XDS110](https://www.ti.com/tool/LP-XDS110).

### Dependencies

To install all dependencies, run:

```
pip install pyserial intelhex
```

| Package | Required | Purpose |
|---------|----------|---------|
| [pyserial](https://pypi.org/project/pyserial/) | Yes | Serial port communication |
| [intelhex](https://pypi.org/project/intelhex/) | Yes (for `.hex` files) | Intel Hex file parsing |
| [python-magic](https://pypi.org/project/python-magic/) | No | Stronger firmware file type detection. Falls back to extension-based detection if not installed. **Note:** on Windows, a broken `libmagic` DLL installation will crash the script. The script detects this automatically and falls back safely. |

### Supported Devices

To list all supported device IDs:

```
python ti-python-sbl-f3.py --get-devices
```

The `-d` argument is required and must match a supported device ID (case insensitive). Entering the wrong device ID may cause a UART lockup. A common error in that case is: `"Unreasonable chip id: "`

Tested hardware:
- LP-XDS110ET + LP-EM-CC2745R10-Q1
- LP-XDS110ET + LP-EM-CC2340R5
- LP-XDS110ET + CC2340R53

### Entering Bootloader Mode when using the LP-EM-CC2745R10-Q1

Before running the script, enable ROM SBL mode by inputting the configuration settings below into your syscfg file, particularly in the location: TI DEVICES -> Boot Configuration. Then, program the device using SWD. Alternatively, performing a mass erase will enable the bootloader on boot automatically.

.. list-table:: Device Configuration - CC2745R10-Q1 Development Platform
   :widths: 50 50
   :header-rows: 0

   * - Application Vector Table
     - resetVectors
   * - Bootloader Configuration
     - Default FCFG bootloader, with CCFG settings
   * - Enable Serial Bootloader
     - ☑
   * - Serial IO Configuration Index
     - 0
   * - Enable Pin Trigger
     - ☑
   * - Pin Trigger DIO
     - 21
   * - Pin Trigger Level
     - LOW

:::{admonition} Pin Trigger DIO Discrepancy
:class: Warning
Notice that the Pin Trigger DIO for the CC2745R10-Q1 is Pin 21, this is a different Pin than the one utilized in the configuration of the CC2340R5. Furthermore, notice that the Serial IO Configuration Index is 0, this is also a different parameter than the one utilized in the configuration of the CC2340R5.
:::

1. Via a jumper, hold the bootloader backdoor pin at the configured pin trigger level (in our case it is LOW)
2. Reset or power-cycle the board
3. Run the script in the command line interface within 120 seconds of completing step 2
  
There are two .hex files provided in this software tool to assist you with visually confirming that the serial bootloader is indeed functioning as expected. "CC2745R10-Q1GreenBlinky.hex" pulses the greed LED on the CC2745R10-Q1, "CC2745R10-Q1RedBlinky.hex" pulses the red LED on the CC2745R10-Q1. 

**Blank devices** (no valid firmware) enter the bootloader automatically on reset — no pin hold needed.

A common error when the device is not in bootloader mode:
```
Timeout waiting for ACK/NACK after 'Synch (0x55 0x55)'
```

On Windows, specify the serial port explicitly with `-p`. To find your port, check Device Manager — the XDS110 Application/User UART is the correct port for flashing (not the Auxiliary Data Port).

### Entering Bootloader Mode when using the LP-EM-CC2340R5

Before running the script, enable ROM SBL mode by inputting the configuration settings below into your syscfg file, particularly in the location: TI DEVICES -> Boot Configuration. Then, program the device using SWD. Alternatively, performing a mass erase will enable the bootloader on boot automatically.

.. list-table:: Device Configuration - CC2340R5 Development Platform
   :widths: 50 50
   :header-rows: 0

   * - Application Vector Table
     - resetVectors
   * - Bootloader Configuration
     - Default FCFG bootloader, with CCFG settings
   * - Enable Serial Bootloader
     - ☑
   * - Serial IO Configuration Index
     - 2
   * - Enable Pin Trigger
     - ☑
   * - Pin Trigger DIO
     - 23
   * - Pin Trigger Level
     - LOW

:::{admonition} Pin Trigger DIO Discrepancy
:class: Warning
Notice that the Pin Trigger DIO for the CC2340R5 is Pin 23, this is a different Pin than the one utilized in the configuration of the CC2745R10-Q1. Furthermore, notice that the Serial IO Configuration Index is 2, this is also a different parameter than the one utilized in the configuration of the CC2745R10-Q1. 
:::

1. Via a jumper, hold the bootloader backdoor pin at the configured pin trigger level (in our case it is LOW)
2. Reset or power-cycle the board
3. Run the script in the command line interface within 120 seconds of completing step 2
  
There are two .hex files provided in this software tool to assist you with visually confirming that the serial bootloader is indeed functioning as expected. "CC2340R5GreenBlinky.hex" pulses the greed LED on the CC2340R5, "CC2340R5RedBlinky.hex" pulses the red LED on the CC2340R5. 

**Blank devices** (no valid firmware) enter the bootloader automatically on reset — no pin hold needed.

A common error when the device is not in bootloader mode:
```
Timeout waiting for ACK/NACK after 'Synch (0x55 0x55)'
```

On Windows, specify the serial port explicitly with `-p`. To find your port, check Device Manager — the XDS110 Application/User UART is the correct port for flashing (not the Auxiliary Data Port).

### Firmware Files

The script supports both raw binary (`.bin`) and Intel Hex (`.hex`) files.

**Intel Hex files** (`.hex`, `.ihx`, `.ihex`) are detected automatically by file extension. Multi-segment hex files (e.g. Zephyr images with separate application and CCFG sections) are handled correctly — each segment is written at its address as defined in the hex file, so no manual address specification is needed.

**Binary files** require an explicit start address via `-a` if the default (flash start `0x0`) is not correct.

### Usage

```
python ti-python-sbl-f3.py [-h] [-q] [-V] [-e] [-w] [-W] [-v] [-d device] [-p port] [-b baud] [-a addr] [file]

  -h, --help       This help
  -q               Quiet
  -V               Verbose
  -e               Mass erase
  -w               Write
  -W, --erase-write  Write after erasing (avoids full mass erase; recommended)
  -v               Verify (CRC32 check)
  -d device        Device ID (required). See --get-devices for valid values
  -p port          Serial port (default: auto-detect, unreliable on Windows)
  -b baud          Baud rate (default: 500000)
  -a addr          Target start address (hex or decimal). Not needed for .hex files
  --get-devices    Print supported device IDs
  --version        Print script version
```

### Example Usage

**Flash a Zephyr hex file (CC2340R5 / CC2340R53 on COM6):**
```
python ti-python-sbl-f3.py -d CC2340R5 -p COM6 -b 115200 -W zephyr.hex
```

**Flash a Zephyr hex file if file is in different directory (CC2340R5 / CC2340R53 on COM6):** 
```
python ti-python-sbl-f3.py -d CC2340R5 -p COM6 -b 115200 -W "C:\{YOUR PATH NAME}\zephyr.hex
```

**Flash multiple binaries at specific addresses (CC2745R10Q1):**
```
python ti-python-sbl-f3.py -d CC2745R10Q1 -p COM6 -b 115200 -e --multi-address-binaries "0x0000,app-no-ccfg.bin;0x4E020000,ccfg.bin;0x4e040000,scfg.bin"
```

**Expected output for a successful hex flash:**
```
Hex file contains 5 segment(s):
  0x00000000  140 bytes
  0x00000090  202102 bytes
  ...
  0x4E020000  2048 bytes
Connecting to target...
Erase done
  [100.0%] ... bytes  ~9000 B/s  total Xs
  Write done 0x00000000 (Xs)
  ...
  Write done 0x4E020000 (Xs)
```

### UART Console After Flashing

After the script completes and resets the device, open a serial terminal on the same port at the baud rate configured in your firmware (typically 115200 for Zephyr). The flashing script releases the port on exit, so the terminal can connect immediately after.

### How It Works

#### Synch and chip identification

The script opens the serial port and sends a two-byte synch sequence (`0x55 0x55`) to the device. The ROM bootloader responds with an ACK, confirming it is active. The script then issues a `GET_PART_ID` command and reads a 4-byte version word. Bytes 2 and 3 of that word form the chip ID, which is looked up in an internal table to identify the device family (e.g. CC2340 or CC2745). If the ID is unrecognised the script aborts — this is why `-d` must match the actual hardware.

#### Intel Hex segment handling

A `.hex` file encodes data at multiple non-contiguous addresses. A Zephyr image, for example, typically contains:

```
0x00000000  ~140 bytes   (vector table / header)
0x00000090  ~200 KB      (application code)
0x000316xx  small sections
0x4E020000  2048 bytes   (CCFG)
```

The script calls `IntelHex.segments()` to get the list of contiguous data ranges and converts each one independently. Each segment is then written to the device at its exact start address. The CCFG at `0x4E020000` is written as a separate operation rather than being buried at the end of a huge flat buffer.

#### 4-byte alignment padding

The CC23xx/CC27xx ROM bootloader requires all write operations to be a multiple of 4 bytes (word-aligned). Hex segments often have sizes that are not naturally aligned (e.g. 202102 bytes). Before writing, the script pads any segment whose length is not a multiple of 4 by appending `0xFF` bytes:

```
padding = (4 - len(data) % 4) % 4
data += b'\xff' * padding
```

`0xFF` is the erased flash value, so the padding bytes have no effect on the programmed content.

#### Write loop and 0xFF skipping

Each segment is sent to the device using a two-command sequence per packet:

1. `CMD_DOWNLOAD` — tells the bootloader the target address and total byte count for the upcoming transfer
2. `CMD_SEND_DATA` — sends up to 248 bytes of payload (the protocol maximum is 252 payload bytes; 248 is used to stay safely within the 255-byte packet limit including the 3-byte header)

The script walks the data in 248-byte chunks. Any chunk that is entirely `0xFF` is skipped entirely — no serial traffic is generated for it. When a non-`0xFF` chunk follows a skipped region, a fresh `CMD_DOWNLOAD` is issued to re-anchor the address before the next `CMD_SEND_DATA`. This means sparse regions (gaps between hex segments that were collapsed into a single buffer in older versions) cost nothing.

After each `CMD_SEND_DATA` the bootloader returns a status byte. `0x40` is success; any other value is an error (e.g. `0x43` = flash write failure).

#### Reset

After all segments are written the script issues `CMD_RESET`, which causes the bootloader to hand control to the application. The UART port is released at this point and a serial terminal can connect immediately.

### Remarks

* Programming multiple SoCs simultaneously is not supported.
* The `-W` flag (erase-write) is recommended over `-w` for most use cases — it avoids a full mass erase and only erases the pages being written.
* At 115200 baud, a typical 200 KB image takes 20-30 seconds. At 500000 baud (default), 5-10 seconds.

If you found a bug or improved some part of the code, please submit an issue or pull request.

##### Authors
Texas Instruments Incorporated (c) 2026

[python]: http://www.python.org/download/ "Python Download"
