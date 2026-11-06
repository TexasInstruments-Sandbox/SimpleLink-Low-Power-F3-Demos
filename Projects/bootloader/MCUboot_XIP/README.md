# MCUboot XIP project

## Introduction

The following projects are modified to allow the device to "switch" images in a user defined deterministic way using MUCboot and two distinct images.
Found in this folder are three projects which are companion projects for the CC2340R5, and are meant to be flashed all at once to the device with UniFlash.

---

## Features


- Added code to control PMCTL register, this register keeps its value after reset
- Updated the contents the loader file in the MCUboot project to allow for image switching and handling in a deterministic way
- Updated the project(s) mcubootblinky blinky.c file on both version 1_, and 1_2 to set the PMCTL register
- **Switching between MCUboot images**: Added the ability to set a unique identifier using the PMCTL register which is read by MCUboot to determine which image to boot into

---

## Prerequisites

### Hardware Requirements

- 1x [LP-EM-CC2340R5 Launchpad](https://www.ti.com/tool/LP-EM-CC2340R5)
- 1x [LP-XDS110 Debugger](https://www.ti.com/tool/LP-XDS110ET)
- 1x USB Type-C to USB-A cable (included with LP-XDS110ET debugger)

### Software Requirements

- **SDK**: `simplelink_lowpower_f3_sdk_9_14_00_33`
- **IDE**: Code Composer Studio (CCS) `20.1.1`
- **TI CLang**: `4.0.2`
- **UniFlash flashing utility**: [UniFlash 9.2.0.5300](https://www.ti.com/tool/UNIFLASH)

---

## How to Use

### 1. **Setup**

1. Import MCUboot, MCUboot_blinky1, MCUboot_blinky2
2. Build MCUboot, MCUboot_blinky1, MCUboot_blinky2
3. Locate important files
    - mcuboot_app1.c in MCUboot project
    - loader.c in bootutil/src/loader.c in MCUboot project
    - blinky.c in 1_mcubootblinky
    - blinky.c in 2_mcubootblinky
4. Confirm each project builds

---

### 2. **Setting the switch (XIP)**

#### **MCUboot project**

The following steps detail the important setup files, and modified files needed to make MCUboot XIP switch images in a deterministic way:

1. In SysConfig under the Image Bootloaders section make sure the upgrade mode is set to `XIP`
2. In same section as step 1, make sure the Primary image and Secondary image slot addresses and size are set to the size you set and load the 1_mcubootblinky and 2_mcubootblinky project to
3. In the filepath bootutil/src/loader.c open the loader file, then reference the image below; the added code snippet allows MCUboot to check the status of the PMCTL register and then select the right image accordingly
4. Use the `if(img_cnt > 1)` section of the figure below to define how you want the device to switch images, do make sure any change you make is reflected in the MCUboot blinky projects
5. Also in loader.c from line 2635 to 2643 comment out the `MCUBOOT_DIRECT_XIP_REVERT` section, during testing this line caused the images to become erased, see the figure below as an example on what to comment out

![mcuboot_code][figure 1]


#### **1_mcubootblinky and 2_mcubootblinky**

In the 1_mcubootblinky and 2_mcubootblinky project there are two files to pay attention to:

* blinky.c
* mcuboot_blinky_cc2340r5.cmd

In the blinky.c file:

* locate `gpioButtonFxn0`, this function has been modified from the original base project to now specifically reset the value in the PMCTL register, then set the PMCTL register with a unique value, in the case of 1_mcubootblinky that is `0x3`, and in the case of 2_mcubootblinky it is `0x5`. 

In the 1_mcubootblinky project open the mcuboot_blinky_cc2340r5.cmd file we need to make sure the contents of the addresses match what MCUboot expects:

```cmd file
#define FLASH_BASE              0x6080
#define FLASH_SIZE              0x3CF80
#define RAM_BASE                0x20000000
#define RAM_SIZE                0x9000
#define CCFG_BASE               0x4E020000
#define CCFG_SIZE               0x800

MCUBOOT_HDR_BASE = 0x6000;
```

In the 2_mcubootblinky project open the mcuboot_blinky_cc2340r5.cmd file we need to make sure the contents of the addresses match what MCUboot expects:

```cmd file
#define FLASH_BASE              0x43080
#define FLASH_SIZE              0x3CF80
#define RAM_BASE                0x20000000
#define RAM_SIZE                0x9000
#define CCFG_BASE               0x4E020000
#define CCFG_SIZE               0x800

MCUBOOT_HDR_BASE = 0x6000;
```

### 3. **Running the combined project(s)**

#### UniFlash

Once the three projects have been modified and built we will need to use the flashing utility UniFlash to flash the three built files to our device simultaneously, see the steps below:

1. In the debug folder of the local MCUboot project find the .hex image (mcuboot_LP_EM_CC2340R5_nortos_ticlang.hex)
2. In the debug folder of the local 1_mcubootblinky find the .bin image (1_mcubootblinky_LP_EM_CC2340R5_nortos_ticlang.bin)
3. In the debug folder of the local 2_mcubootblinky find the .bin image (2_mcubootblinky_LP_EM_CC2340R5_nortos_ticlang.bin)
4. Open UniFlash, enter and select the CC2340R5 device, and in the program section click "Browse" in the Flash Image(s) section
5. Select the MCUboot.hex image
6. Select the 1_mcubootblinky.bin image, check the **Binary** box, and set the load address to 0x6000
7. Select the 2_mcubootblinky.bin image, check the **Binary** box, and set the load address to 0x48000 
8. Click **Load Image**, and the device should start flashing all three images to the device
9. Once UniFlash has completed flashing the XDS110's LED should stay RED, please press the reset button on the XDS110 to let the device start the programs we just flashed
10. The device will boot into MCUboot, select and load into one of the images and set the LED accordingly (1_mcubootblinky is green, 2_mcubootblinky is red)
11. Click the left button to set the PMCTL register, reset the device, and restart from step 10; in practice we should see the active LED switch each time we go through this process

## Notes

- While UniFlash is flashing please do not disconnect or reset the device
- When using the MCUboot project a majority of the files loaded are **linked files** which are the original copies of the SDK files rather than workspace copies; be careful when modifying these files or create a new local copy of the file to modify


[figure 1]:mcuboot_code.png "mcuboot_code"