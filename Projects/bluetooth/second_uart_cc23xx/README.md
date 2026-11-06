# Software UART Inside BLE Example on CC2340

## Introduction

 The purpose of this project is to demonstrate how to implement a second UART inside a BLE example, which is controlled completely by software. The CC2340 devices have a single HW UART peripheral on-board. For most applications, a single UART is enough but in cases where a second UART is necessary and a SW solution is acceptable, then this project is worth considering. This same implementation could be used to interact with other RF stacks as well!

The project utilizes a GPIO for receiving the UART Rx packets, and the SPI PICO pin for transmitting the UART Tx packets.

The example is structured as a “UART Echo” example where the task reads the request size, then outputs it to the serial terminal. Please note that to run this project successfully, the project’s optimization level should be set to Z, however, the approach1.c file should be excluded from this optimization and set to optimization level 0. The project currently uses 115200 baud rate, however, it has been tested with different baud rates and is able to go as low as 38400, therefore the ability of dynamically changing baud rates could also be added if needed.



## Prerequisites

### Hardware Requirements

- 1x [LP-EM-CC2340R5 Launchpad](https://www.ti.com/tool/LP-EM-CC2340R5)
- 1x [LP-XDS110 Debugger](https://www.ti.com/tool/LP-XDS110ET)
- 1x USB Type-C to USB-A cable (included with LP-XDS110ET debugger)

### Software Requirements

- **SDK**: `simplelink_lowpower_f3_sdk_8_40_02_01`
- **IDE**: Code Composer Studio (CCS) 20.1.1 or higher
- **TI CLang**: `4.0.2`


## Overview

### How the Example Works

**UART Rx**: To achieve this, a GPIO bit-bang was implemented, where a callback was set in place to alert the system of the GPIO going low, indicating the start of the UART data packet. Once the first start bit is received, all hardware interrupts are temporarily disabled, to ensure that the device processes all the UART packets without any interruptions. An LGPTimer instance is then called, where the appropriate delay was put in place to accurately sample the UART packets. However, since all interrupts were disabled, the counter register was being read to identify the timeout. The device reads and processes 8 bytes at a time (chunk_size macro), then restores hardware interrupts in order to tend to all the other device needs as well as to reset the timer and avoid any drift.  
 
**UART Tx**: To achieve this, the SPI driver was used, where the bit size was set to 10, and the frame format was set to SPI_POL0_PHA1 (this is required to have a continuous clock, and with that continuous data transmission). Due to the length of time the SPI driver takes to release power constraints, an extra byte of all 0s will be sent to ensure that the UART on the receiving end does not interpret the release of the power constraint to be another UART packet.

### Using UART API Commands

**API Commands**: The example relies on 2 main functions swUARTRead() and swUARTWrite() :

The swUARTRead() function takes in 1 argument, which is size (uint8_t), so that the device knows how many characters to expect.

The swUARTWrite() function takes in 2 arguments, uint8_t *buffer, and uint8_t size.

**Here's an example of how to use the UART API commands:**
```c
// This code snippet reads 100 bytes of UART packets then echoes those characters
while(1)
{
    swUARTRead(100); // Has to be read to mainThreadBuffer
    swUARTWrite(mainThreadBuffer, 100);
}
```


### Changing the Baud Rate

Adjusting the Baud Rate is possible, but will require some modifications and calibrations:

- Adjusting the **TICKS_PER_8p75US** macro to reflect the correct period of the baud rate being used. This may need some fine tuning

- Adjusting the **PHASE_TICKS** macro to add the appropriate phase to the timer. This may need some fine tuning

- Adjusting the bit rate of the SPI driver

### Known Limitations

- The accuracy of this UART will rely on the BLE parameters set. For example, this project was stress tested with the advertising interval set to 1000 ms, and reported an error rate of 0.04% .

- The current implmentation only supports between 1 to 255 bytes. Sending and recieving more bytes may be possible, however it has not been tested.