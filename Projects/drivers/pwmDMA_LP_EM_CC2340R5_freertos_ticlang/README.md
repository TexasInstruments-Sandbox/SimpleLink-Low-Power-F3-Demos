# PWM DMA Control for CC2340R5

---

Example Summary
---------------
This example demonstrates the use of Direct Memory Access (DMA) for controlling 
PWM duty cycle changes on the Texas Instruments CC2340R5 platform. The application 
shows how to automatically update PWM duty cycles from a predefined buffer without 
CPU intervention, creating smooth LED brightness transitions or other PWM-based 
effects. The example also includes ADC sampling that is triggered by timer events. 
See [figure 1] with a TRANSFER_SIZE_IN_ONE_BURST of 64 (for 64 unique duty cycles 
across a sequence) and pwmPeriod of 50000 (producing a 1 kHz frequency).

![pwmdma][figure 1]

Features
--------
* PWM output through a LGPT which triggers ADC samples
* ADC values stored in a buffer through one DMA
* A second DMA in ping-pong format to transfer values into the PWM LGPT channel compare register
* Using LGPT to trigger DMA buffer location swap and re-load

Hardware Requirements
--------------------
* Texas Instruments [LP-EM-CC2340R5] LaunchPad
* [LP-XDS110ET] Debug probe

Software Requirements
--------------------
* [Code Composer Studio] v20.2
* [SimpleLink Low Power F3 SDK] v9.11
* [SysConfig] v1.23.2
* [TI CLANG Compiler] v4.0.3.LTS

Board Specific Settings
-----------------------
The example demonstrates DMA-based PWM control and ADC sampling triggered by timer 
events. Pin assignments and configurations are defined in the table below.

### Pin Map

| Function | Pin | Configuration | Description |
|----------|-----|---------------|-------------|
| **PWM Output** |||
| CONFIG_PWM_0 | DIO0 | LGPT3 CH2 | PWM output for duty cycle control |
| **ADC Inputs** |||
| CONFIG_ADC_CHANNEL_0 | DIO7 | ADC | ADC input for sampling |
| **User Interface** |||
| CONFIG_GPIO_RLED | DIO14 | GPIO Output | Red LED to indicate timer events |
| CONFIG_GPIO_GLED | GPIO15 | GPIO Output | Green LED to indicate ADC samples |
| **Timers** |||
| CONFIG_LGPTIMER_0 | LGPT3 | Timer | PWM timer with DMA access |
| CONFIG_LGPTIMER_1 | LGPT1 | Timer | Timer for controlling DMA transfers |

Example Usage
-------------
When you run this example:

1. The PWM output on DIO0 will automatically change its duty cycle in a 
fade-in/fade-out pattern
2. The green LED will toggle each time an ADC sample buffer is filled
3. The red LED will toggle each time the LGPT1 timer reaches its target value
4. The DMA controller will automatically update the PWM duty cycle without CPU 
intervention, and use a ping-pong buffer format so that it reloads one of two 
buffers at the completion of each sequence

Application Design Details
--------------------------
This example consists of a main thread that initializes the peripherals and a 
series of callback functions that handle peripheral events:

### Main Thread
The main thread:
1. Initializes the PWM peripheral
2. Prepares a buffer of duty cycle values that create a fade-in/fade-out effect
3. Configures LGPT1 timer to control DMA transfers
4. Sets up DMA channel for transferring data from the duty cycle buffer to the 
PWM compare register
5. Configures LGPT3 timer to trigger ADC sampling
6. Initializes ADC in timer trigger mode
7. Starts PWM output and enters an infinite loop

### DMA Configuration
The DMA controller is configured to:
1. Transfer data from a predefined duty cycle buffer to PWM compare register
2. Use ping-pong mode to continuously update PWM compare values without interruption
3. Automatically switch between primary and alternate control structures
4. Trigger on LGPT3 timer zero events

### ADC Configuration
The ADC is configured to:
1. Sample when triggered by LGPT3 halfway through duty cycle of each period
2. Use a callback function to process sampled data
3. Convert raw ADC values to microvolts
4. Toggle the green LED when a buffer of samples is completed

### Timer Configuration
The application uses two timers:
1. LGPT3 (CONFIG_LGPTIMER_0): Configured as a PWM timer and used to trigger ADC sampling
2. LGPT1 (CONFIG_LGPTIMER_1): Used to control the DMA transfer rate and reconfigure 
DMA control structures

### DMA Control Features
The DMA control system includes:
1. **Ping-pong buffer mode**: Allows continuous operation by switching between 
two buffer configurations
2. **Timer-based triggering**: DMA transfers occur automatically based on timer events

### Configuration Constants
- TRANSFER_SIZE_IN_ONE_BURST: 512 - Size of the duty cycle buffer, representing 
the numpber of PWM timer periods per sequence
- ADC_SAMPLE_SIZE: 64 - Number of ADC samples in each buffer
- DMA_CHANNEL: 6 - DMA channel used for transfers
- PWM_PERIOD: 50000 - PWM period, multiply by 20 to get units of nanoseconds (ex 50000*20 = 1 ms)

### Custom TI Drivers
Two customized TI Drivers are included for the operation of this 
- PWMTimerLPF3.c: changes the mode to up/down and modified the channel action 
to clear on compare and toggle on periodic
- ADCBufLPF3.c: altered to trigger on a target event instead of continuously 
via software command

[LP-EM-CC2340R5]: https://www.ti.com/tool/LP-EM-CC2340R5
[LP-XDS110ET]: https://www.ti.com/tool/LP-XDS110ET
[Code Composer Studio]: https://www.ti.com/tool/download/CCSTUDIO/20.2.0
[SimpleLink Low Power F3 SDK]: https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK/9.11.00.18
[TI CLANG Compiler]: https://www.ti.com/tool/download/ARM-CGT-CLANG/4.0.3.LTS
[SysConfig]: https://www.ti.com/tool/download/SYSCONFIG/1.23.2.4156

[figure 1]:pwmDMA.png "PWM DMA"