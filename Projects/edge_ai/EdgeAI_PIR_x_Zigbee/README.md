   # EdgeAI_PIR_x_Zigbee
                                                                                                                                       
   ## Introduction

   EdgeAI PIR x Zigbee is a real-time occupancy detection system that fuses a passive infrared (PIR)
   sensor, a TVM-compiled neural network, and the Zigbee Home Automation profile into a single
   firmware image on the LP-EM-CC2745R10-Q1 LaunchPad.

   Rather than using a fixed threshold to decide occupancy, the device continuously acquires ADC
   samples from the PIR sensor, extracts a 25×25 feature matrix using CMSIS-DSP signal
   processing, and runs a three-class neural network (human / background / dog) via Apache TVM.
   The inferred occupancy state is reported to a Zigbee coordinator once per second as a standard
   ZCL Occupancy Sensing attribute, making it compatible with Zigbee HA-aware smart home platforms
   such as the Amazon Alexa, and other third-party Zigbee gateways.

   ---

   ## Features

   - **On-device AI inference**: TVM-compiled ONNX neural network with three output classes
     (human, background, dog), running entirely on-device with no cloud dependency.
   - **CMSIS-DSP signal processing**: 128 ADC samples processed into 25 time windows × 25
     signal features (FFT energy, kurtosis, zero-crossing rate, slope changes, spectral entropy).
   - **Cortex-M33 CDE co-processor support**: `m33_cde_init()` unlocks CP0 before the model
     runs, enabling hardware-accelerated TVM execution on the CC2745R10-Q1.
   - **Standard ZHA occupancy sensor**: Implements ZCL Occupancy Sensing cluster (0x0406) on
     endpoint 5, Zigbee HA profile 0x0104, device type 0x0107. Pairs natively with the Amazon Alexa without custom configuration.
   - **Fast motion detection**: Motion reported to the coordinator in 4–11 seconds after onset.
   - **1-second Zigbee poll period**: Minimizes attribute-report latency on a Zigbee End Device.
   - **Non-blocking three-thread architecture**: Sensor acquisition, ZBOSS event loop, and
     inference run on separate tasks, ensuring the cooperative Zigbee stack is never blocked.
   - **Factory reset via on-board button**: Hold BTN-1 or BTN-2 during boot to erase Zigbee
     NVRAM and enter Zigbee joining mode for pairing with a new coordinator.

   ---

   ## Hardware and Software Prerequisites

   ### Hardware Requirements

   - 1× [LP-EM-CC2745R10-Q1 LaunchPad](https://www.ti.com/tool/LP-EM-CC2745R10-Q1)
   - 1× PIR sensor module (analog output connected to Launchpad DIO21)
   - 1× Zigbee coordinator (Amazon Echo Gen 4+ with built-in Zigbee hub)
   - 1× USB cable to connect the LaunchPad to a PC for flashing

   ### Software Requirements

   - **SDK**: `simplelink_lowpower_f3_sdk@9.14.02.16`
   - **Edge AI Plugin**: `simplelink_sdk_edgeai_plugin_1_00_00_00`
   - **IDE**: Code Composer Studio (CCS) `20.1.1` or later
   - **Compiler**: TI CLang `4.0.2`
   - **TVM runtime**: Pre-compiled as `ai_artifacts/mod.a` (included in project)
   - **CMSIS-DSP**: Pre-compiled as `dsp/cmsis_dspm33f.a` (included in project)

   ---

   ## How It Works

   ### Three-Thread Architecture

   The firmware runs three concurrent execution contexts under FreeRTOS:

   - **SensorThread** (FreeRTOS TaskP) — Reads ADC samples from the PIR sensor at a fixed
     rate and places them into a shared circular queue.
   - **zboss_main_loop** (FreeRTOS TaskP) — Runs the ZBOSS cooperative Zigbee event loop,
     handling all Zigbee stack events and ZCL attribute reporting.
   - **inferenceTask** (POSIX pthread) — Dequeues sensor data, runs feature extraction,
     executes the TVM neural network, and updates the shared occupancy state.

   The inference task communicates with the Zigbee loop via two `volatile uint8_t` variables.
   Single-byte writes on Cortex-M33 are atomic, so no mutex is needed:

   ```c
   static volatile uint8_t g_inference_ready    = 0;
   static volatile uint8_t g_inferred_occupancy = 0;
   ```

   ### Inference Pipeline

   ```
   PIR sensor (ADC) → 128 raw samples
           ↓
   FeatureExtract_run()
           ↓
   25 time windows × 25 features = 625-float matrix
           ↓
   tvmgen_default_run()  — TVM model input (1,1,25,25) → output (1,3)
           ↓
   argmax(logits) → class 0 = human, class 1 = background, class 2 = dog
           ↓
   g_inferred_occupancy = (class == 0) ? 1 : 0
   ```

   Classes 1 (background) and 2 (dog) both map to `unoccupied`. Only class 0 (human) maps
   to `occupied`.

   ### Zigbee Reporting

   `report_inference_result()` is a ZBOSS scheduled alarm that fires every second within the
   ZBOSS event loop. It reads the shared occupancy state, updates the ZCL Occupancy attribute,
   marks it for reporting, and reschedules itself:

   ```c
   ZB_SCHEDULE_APP_ALARM(report_inference_result, 0, 1 * ZB_TIME_ONE_SECOND);
   ```

   Because all ZBOSS API calls happen inside the cooperative event loop context, there are no
   thread-safety concerns with the Zigbee stack itself.

   ### CDE Co-Processor Initialization

   The TVM model targets the Cortex-M33 CDE co-processor (CP0) on the CC2745R10-Q1.
   `m33_cde_init()` writes to CPACR at `0xE000ED88` to unlock CP0 before any inference runs.
   It is called as the very first statement in `main()`, before `Board_init()`.

   ---

   ## Pin Assignments

   | Signal           | BoosterPack Pin |     CC2745R10-Q1 Pin      | Resource                  | Notes                      |
   |------------------|-----------------|---------------------------|---------------------------|----------------------------|
   | PIR Sensor Input | BP.2            | DIO 21                    | ADC — CONFIG_ADC_0        | 8-bit resolution           |
   | Green LED        | BP.19           | DIO 12                    | GPIO — CONFIG_GPIO_GLED   | Zigbee status              |
   | Red LED          | BP.36           | DIO 16                    | GPIO — CONFIG_GPIO_RLED   | Error / join indicator     |
   | BTN-1            | BP.30           | DIO 20                    | GPIO — CONFIG_GPIO_BTN1   | Factory reset(hold on boot)|
   | BTN-2            | BP.40           | DIO 0                     | GPIO — CONFIG_GPIO_BTN2   | Factory reset(hold on boot)|

   ---

   ## Getting Started

   ### Hardware Setup

   1. Connect the PIR sensor analog output to DIO21 on the LP-EM-CC2745R10-Q1.
   2. Power the sensor from the 3.3 V and GND pins on the LaunchPad BoosterPack header.
   3. Connect the LaunchPad to a PC via USB.

   ### Build and Flash

   1. Open Code Composer Studio and import the `EdgeAI_x_Zigbee` project.
   2. Select the **Release** build configuration.
   3. Build the project (Project → Build All). Verify zero errors before flashing.
   4. Flash to the device via Run → Debug, or use the CCS Flash Programmer tool.

   ### Pair with a Zigbee Coordinator

   **Amazon Alexa (Echo with built-in Zigbee hub):**

   1. Open the Alexa app → Devices → (+) Add Device → Other.
   2. Alexa will enter discovery mode.
   3. Hold BTN-1 or BTN-2 on the LaunchPad during power-on to perform a factory reset
      and force the device into Zigbee joining mode.
   4. Wait for Alexa to discover the sensor (it will appear as a motion sensor named
      "PIR Occupancy", manufacturer: TI, model: PIR Occupancy).

   ### Factory Reset

   Hold **BTN-1** or **BTN-2** during power-on to erase Zigbee NVRAM. The device will
   enter Zigbee joining mode on the next boot. This is required whenever re-pairing with
   a new coordinator or after flashing firmware that changes the Zigbee configuration.

   ---

   ## Application Design Details

   ### Zigbee Profile

   | Parameter   | Value                           |
   |-------------|---------------------------------|
   | Profile     | ZHA (0x0104)                    |
   | Device Type | HA Occupancy Sensor (0x0107)    |
   | Endpoint    | 5                               |
   | Cluster     | ZCL Occupancy Sensing (0x0406)  |
   | Attribute   | Occupancy (0x0000)              |
   | Poll Period | 1000 ms                         |
   | Channels    | 11–26                           |

   ### Neural Network

   | Parameter | Value                                                     |
   |-----------|-----------------------------------------------------------|
   | Compiler  | Apache TVM                                                |
   | Input     | (1, 1, 25, 25) float32 — 25 windows × 25 DSP features    |
   | Output    | (1, 3) float32 — raw logits for three classes             |
   | Class 0   | Human → Occupied (1)                                      |
   | Class 1   | Background → Unoccupied (0)                               |
   | Class 2   | Dog → Unoccupied (0)                                      |
   | Decision  | argmax of logits — equivalent to argmax of softmax        |

   ### FreeRTOS Configuration

   | Parameter              | Value                  |
   |------------------------|------------------------|
   | Heap Size              | 0x00010000 (64 KB)     |
   | Timer Stack Size       | 0x00000100 (256 bytes) |
   | Inference Task Stack   | 1024 bytes             |
   | Inference Task Priority| 1                      |

   ---

   ## Known Limitations

   - **No-motion detection latency (~24 seconds)**: The PIR sensor's analog hold-time circuit
     keeps the output elevated for approximately 20 seconds after the last detected motion.
     The neural network correctly classifies this hold-time signal as human-class occupancy.
     The software floor for "no motion detected" is therefore approximately 24 seconds and
     cannot be reduced without replacing the PIR sensor hardware.

   - **Motion detection latency (4–11 seconds)**: Time from motion onset to coordinator
     notification. Varies depending on sensor warm-up, feature window fill, and Zigbee
     round-trip timing.

   - **Single coordinator pairing**: The device stores Zigbee NVRAM and communicates only
     with the coordinator it was paired with. A factory reset is required to join a new
     coordinator.

   ---

   ## References

   - [LP-EM-CC2745R10-Q1 Product Page](https://www.ti.com/tool/LP-EM-CC2745R10-Q1)
   - [SimpleLink Low Power F3 SDK](https://www.ti.com/tool/SIMPLELINK-LOWPOWER-F3-SDK)
   - [SimpleLink SDK Edge AI Plugin](https://www.ti.com/tool/SIMPLELINK-SDK-EDGEAI-PLUGIN)
   - [ZBOSS Zigbee Stack Documentation](https://dsr-zboss.com)
   - [ZCL Specification — Occupancy Sensing Cluster (0x0406)](https://zigbeealliance.org)
   - [Apache TVM — Open Deep Learning Compiler Stack](https://tvm.apache.org)
   - [CMSIS-DSP Library](https://arm-software.github.io/CMSIS-DSP/latest/)
   
