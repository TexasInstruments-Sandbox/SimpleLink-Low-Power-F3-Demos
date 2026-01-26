# RF Packet IEEE test

This IEEE 802.15.4 RF Packet RX/TX example illustrates how to do simple packet 
transmission using the RCL driver over the IEEE PHY. It includes both TX and 
RX configurations as well as automatic ACKs based on short addressing.  This 
is intended to be a starting point for IEEE Proprietary RF development.

## RF Packet IEEE Features

This project implements the following functionality:

* RF Packet RX/TX using the IEEE 802.15.4 PHY
* Selectable configuration between RX and TX modes
* Demonstration of Auto-ACK mode 

## Dependencies

This project depends on the following software components:
* [SimpleLink Low Power F3 SDK] v9.12.00.19
* [Code Composer Studio] v20.2.0
* [TI CLANG Compiler] v4.0.2.LTS
* [SysConfig] v1.23.2

## Hardware Requirements

This project depends on the use of the following hardware:

* 2x [LP-EM-CC2340R5] - CC2340R5 LaunchPad™ development kit for SimpleLink™ Low 
Energy MCU
* 2x [LP-XDS110ET] - XDS110ET LaunchPad™ development kit debugger with 
EnergyTrace™ software

## Board Specific Settings

The following contains default definitions from within `rfPacket_ieee_test.c`
and can be modified as needed:

1. `FREQUENCY`: frequency is 2435 MHz (channel 17)
2. `PAN_ID`: PAN ID is 0xF00D
3. `SHORT_ADDRESS_RX`: short address of the RX device is 0xABCD
4. `SHORT_ADDRESS_TX`: short address of the TX device is 0x1234
5. `PKT_LEN`: the length of the test packets transmitted is 38

## Example Usage

Build the example once with the `exampleMode` equal to `EXAMPLE_MODE_TX`, and 
flash it on a LaunchPad.  Then change the `exampleMode` equal to 
`EXAMPLE_MODE_RX` and re-build/flash this on a second LaunchPad.  The line of 
code to change between the two modes is shown below from 
`rfPacket_ieee_test.c`.

```c
volatile uint32_t exampleMode = EXAMPLE_MODE_TX;
```

Once both LaunchPads are programmed, start the TX device followed by the RX
device.  Every second the Tx LaunchPad will toggle the green LED, indicating a 
packet has been transmitted, and the RX LaunchPad will consecutively toggle its
green LED to indicate reception of the packet.  A 
[Packet Sniffer](https://www.ti.com/tool/PACKET-SNIFFER) can be used to verify 
the packets being transmitted over-the-air as well as the RX ACKs.  Note that 
further TX device handling of the ACK has not been implemented and will require 
additional application development.

## Conclusion

This project demonstrates how to implement Proprietary RF packet RX and TX
using the IEEE PHY.  The frequency, PAN ID, packet length, and short addresses
can be configured.  The Auto-ACK mode is demonstrated.  For more information
about the IEEE 802.15.4 Rx and Tx Command Handler, visit the 
[RCL User's Guide](https://dev.ti.com/tirex/explore/content/simplelink_lowpower_f3_sdk_9_12_00_19/docs/rcl/html/ieee_rxtx_handler.html).

<!-- Definition of inline references -->
[LP-EM-CC2340R5]: https://www.ti.com/tool/LP-EM-CC2340R5
[LP-XDS110ET]: https://www.ti.com/tool/LP-XDS110ET

[SimpleLink Low Power F3 SDK]: https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK/9.12.00.19
[Code Composer Studio]: https://www.ti.com/tool/download/CCSTUDIO/20.2.0
[TI CLANG Compiler]: https://www.ti.com/tool/download/ARM-CGT-CLANG/4.0.2.LTS
[SysConfig]: https://www.ti.com/tool/download/SYSCONFIG/1.23.2.4156
