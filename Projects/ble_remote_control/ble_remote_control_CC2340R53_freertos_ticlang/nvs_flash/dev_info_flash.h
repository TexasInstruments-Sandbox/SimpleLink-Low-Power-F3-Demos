/******************************************************************************

@file  dev_info_flash.h

@brief Device Information Flash Reading Module - Reads device identification
data from flash memory for BLE Device Information Service. Implements
Google ATV Remote Control specification compliance for device identification.

Group: WCS, BTS
Target Device: cc23xx

******************************************************************************

Copyright (c) 2022-2026, Texas Instruments Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

*  Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

*  Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

*  Neither the name of Texas Instruments Incorporated nor the names of
its contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

******************************************************************************

Copyright (c) 2022-2026, Texas Instruments Incorporated
All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

******************************************************************************
*****************************************************************************/

#ifndef DEV_INFO_FLASH_H
#define DEV_INFO_FLASH_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include <stdint.h>
#include <stdbool.h>

/*********************************************************************
 * CONSTANTS
 */

/* NVS Memory Layout - All data stored in CONFIG_NVSINTERNAL_STORAGE region */
#define NVS_VERSION_OFFSET                  0x0000   /* Offset for firmware version string (20 bytes) */
#define NVS_VERSION_MAX_LEN                 32       /* Max version string length bytes including null terminator */

/*********************************************************************************************************/
/* Device ID Information */
#define NVS_DEVICE_ID_OFFSET                0x0020   /* Offset for device identification data (32 bytes) */
#define NVS_DEVICE_ID_SIZE                  32       /* Size of device ID block */

/* Device ID Data Offsets (relative to NVS_DEVICE_ID_OFFSET) */
#define DEVICE_TYPE_OFFSET                  0        /* 1 byte (0x01-0x06) */
#define VENDOR_SOURCE_OFFSET                1        /* 1 byte (0x02 = USB IF) */
#define VENDOR_ID_OFFSET                    2        /* 2 bytes (0x8a24 = Google) */
#define PRODUCT_ID_OFFSET                   4        /* 2 bytes (0x6682 = Google ATV) */
#define PRODUCT_VER_OFFSET                  6        /* 2 bytes (0x0100/0x0101) */
#define DEVICE_NAME_LEN_OFFSET              8        /* 1 byte (max 16) */
#define DEVICE_NAME_OFFSET                  9        /* up to 10 bytes */
#define DEVICE_UI_LAYOUT_OFFSET             19       /* 1 byte (UI config) */

/*********************************************************************************************************/
/* Device Wakeup Information */
#define NVS_DEVICE_WAKEUP_OFFSET            0x0040   /* Offset for device identification data (32 bytes) */
#define NVS_DEVICE_WAKEUP_SIZE              64       /* Size of device ID block */

/* Device Wakeup Data Offsets (relative to NVS_DEVICE_WAKEUP_OFFSET) */
#define DEVICE_WAKEUP_KEY_1                 0        /* 2 bytes (mask 0xFFFF) */
#define DEVICE_WAKEUP_KEY_2                 2        /* 2 bytes (mask 0xFFFF) */
#define DEVICE_WAKEUP_KEY_CUSTOM_FORMAT     4        /* 31 bytes */

/*********************************************************************************************************/
/* Device Configuration Information */
#define NVS_DEVICE_CONFIG_OFFSET            0x0080   /* Offset for device identification data (32 bytes) */
#define NVS_DEVICE_CONFIG_SIZE              32       /* Size of device ID block */

/* Device Configuration Data Offsets (relative to NVS_DEVICE_CONFIG_OFFSET) */
#define DEVICE_COUCH_MODE_SWITCH            0        /* 1 byte (0xFF for shutdown or 0x01 for BLE continue) */
#define DEVICE_TX_POWER_OUTPUT              1        /* 1 byte (0xFF/0x23/0x1F/0x1B/0x17) */
#define DEVICE_BACKLIGHT_SWITCH             2        /* 1 byte (0x00 to disable or 0x01 to enable) */

/*********************************************************************************************************/
/* Device Bluetooth LE Information */
#define NVS_DEVICE_BLE_OFFSET               0x00A0   /* Offset for device identification data (32 bytes) */
#define NVS_DEVICE_BLE_SIZE                 32       /* Size of device ID block */

/* Device Configuration Data Offsets (relative to NVS_DEVICE_BLE_OFFSET) */
#define DEVICE_PERIODIC_WAKEUP_SWITCH       0        /* 1 byte (0x00 to disable or 0x01 to enable) */
#define DEVICE_PERIODIC_WAKEUP_INT          1        /* 2 bytes (The unit is 1 minute.) */
#define DEVICE_RPA_SWITCH                   3        /* 1 byte (0x00 to disable or 0x01 to enable) */
#define DEVICE_BLE_SWITCH                   4        /* 1 byte (0x00 to disable or 0x01 to enable) */
#define DEVICE_CACHE_POWER_SWITCH           5        /* 1 byte (0x00 to disable or 0x01 to enable) */

/*********************************************************************************************************/
/* Device Configuration Information */
#define NVS_DEVICE_SECURITY_OFFSET       0x00C0   /* Offset for device identification data (32 bytes) */
#define NVS_DEVICE_SECURITY_SIZE         128   /* Size of device ID block */

/* Device Configuration Data Offsets (relative to NVS_DEVICE_SECURITY_OFFSET) */
#define DEVICE_AUTH_KEY                  0        /* 16 byte */
#define DEVICE_ENCRY_PASS                16       /* 16 bytes (0x00 to disable or 0x01 to enable) */
#define DEVICE_ECDSA_KEY                 32       /* 64 bytes (0x00 to disable or 0x01 to enable) */

/*********************************************************************************************************/
/* Google Default Values (fallback) */
#define GOOGLE_DEFAULT_VID                  0x8a24   /* Google Vendor ID */
#define GOOGLE_DEFAULT_PID                  0x6682   /* Google ATV Remote Product ID */
#define GOOGLE_DEFAULT_VER                  0x0100   /* Product version */
#define GOOGLE_DEFAULT_FW_VER               "1.0.0"  /* Default firmware version */

/* Device Type Mappings (Google ATV Remote Control Spec) */
#define DEVICE_TYPE_G10                     0x01     /* Hobb (short) */
#define DEVICE_TYPE_G20                     0x02     /* Shaw (long) */
#define DEVICE_TYPE_G11                     0x03
#define DEVICE_TYPE_G31                     0x04
#define DEVICE_TYPE_G21                     0x05
#define DEVICE_TYPE_G91                     0x06

/* Validation Constants */
#define FLASH_UNPROGRAMMED                  0xFF     /* Empty flash byte */
#define MAX_DEVICE_NAME_LEN                 16       /* Max device name length */

#define CONFIG_SWITCH_DISABLE               0x00     /* Disable */

/*********************************************************************
 * ===================================================================
 * OFFSET 0x0020: DEVICE IDENTIFICATION DATA (32 bytes)
 * ===================================================================
 *
 * This data identifies the device to Google ATV system.
 *
 * LAYOUT (Little-Endian Format):
 * --------------------------------------------------------------------
 * Offset | Address  | Size | Field            | Example      | Description
 * --------------------------------------------------------------------
 * 0x00   | 0x40020  | 1B   | Device Type      | 0x01         | G10=0x01, G20=0x02, G11=0x03, G31=0x04, G21=0x05, G91=0x06
 * 0x01   | 0x40021  | 1B   | Vendor Source    | 0x02         | MUST be 0x02 (USB IF)
 * 0x02   | 0x40022  | 2B   | Vendor ID        | 0x24 0x8A    | MUST be 0x8A24 (Google) - little-endian
 * 0x04   | 0x40024  | 2B   | Product ID       | 0x82 0x66    | MUST be 0x6682 (Google ATV Remote) - little-endian
 * 0x06   | 0x40026  | 2B   | Product Version  | 0x00 0x01    | 0x0100=v1.0, 0x0101=v1.1 - little-endian
 * 0x08   | 0x40028  | 1B   | Device Name Len  | 0x0A         | Length of device name (0-16)
 * 0x09   | 0x40029  | 16B  | Device Name      | "RemoteB046" | ASCII string (not null-terminated)
 * 0x19   | 0x40039  | 12B  | Reserved         | 0xFF...      | Reserved for future use
 * 0x1F   | 0x4003F  | 1B   | UI Layout        | 0x00         | UI configuration byte
 * --------------------------------------------------------------------
 *
 * EXAMPLE: Programming G10 device with name "RemoteB046" and version "1.2.3"
 * --------------------------------------------------------------------
 * COMPLETE NVS DATA (52 bytes from offset 0x0000):
 *
 * Offset 0x0000 (Version String - 20 bytes):
 *   31 2E 32 2E 33 00 FF FF  FF FF FF FF FF FF FF FF
 *   FF FF FF FF
 *   ("1.2.3" + null + unused)
 *
 * Offset 0x0020 (Device ID - 32 bytes):
 *   01 02 24 8A 82 66 00 01  0A 52 65 6D 6F 74 65 42
 *   30 34 36 FF FF FF FF FF  FF FF FF FF FF FF FF 00
 *
 * Breakdown:
 *   01       = Device Type G10
 *   02       = Vendor Source (USB IF)
 *   24 8A    = Vendor ID 0x8A24 (Google)
 *   82 66    = Product ID 0x6682 (Google ATV Remote)
 *   00 01    = Product Version 1.0
 *   0A       = Device Name Length 10
 *   52...36  = "RemoteB046" (ASCII: 52=R, 65=e, 6D=m, etc.)
 *   FF...FF  = Reserved (unprogrammed)
 *   00       = UI Layout
 *
 * EXAMPLE: Programming G20 device with default name and version "1.0.0"
 * --------------------------------------------------------------------
 * Offset 0x0000 (Version String):
 *   31 2E 30 2E 30 00 FF FF  FF FF FF FF FF FF FF FF
 *   FF FF FF FF
 *   ("1.0.0" + null + unused)
 *
 * Offset 0x0020 (Device ID):
 *   02 02 24 8A 82 66 00 01  00 FF FF FF FF FF FF FF
 *   FF FF FF FF FF FF FF FF  FF FF FF FF FF FF FF 00
 *
 * Breakdown:
 *   02       = Device Type G20 (Shaw long remote)
 *   02       = Vendor Source (USB IF)
 *   24 8A    = Vendor ID 0x8A24 (Google)
 *   82 66    = Product ID 0x6682
 *   00 01    = Product Version 1.0
 *   00       = Device Name Length 0 (will use default "GoogleRemote")
 *   FF...FF  = Unused
 *   00       = UI Layout
 *
 * ===================================================================
 * PROGRAMMING METHODS
 * ===================================================================
 *
 * METHOD 1: Using TI Uniflash (Recommended for Production)
 * --------------------------------------------------------------------
 * 1. Create a single binary file containing all NVS data (52 bytes):
 *    - First 20 bytes: Firmware version string
 *    - Next 32 bytes: Device ID data
 *
 * 2. Open Uniflash and connect to CC2340R53 device
 * 3. Go to "Program" tab
 * 4. Add binary file: nvs_data.bin @ 0x40000
 * 5. Program device
 * 6. Verify checksums
 *
 * Creating the binary file (Linux/Mac):
 *   # Create version string (20 bytes, padded with 0xFF)
 *   $ printf "1.2.3\x00" > version.bin
 *   $ dd if=/dev/zero bs=1 count=14 | tr '\0' '\377' >> version.bin
 *
 *   # Create device ID data (32 bytes)
 *   $ printf "\x01\x02\x24\x8A\x82\x66\x00\x01\x0A" > device_id.bin
 *   $ printf "RemoteB046" >> device_id.bin
 *   $ dd if=/dev/zero bs=1 count=13 | tr '\0' '\377' >> device_id.bin
 *   $ printf "\x00" >> device_id.bin
 *
 *   # Combine into single file
 *   $ cat version.bin device_id.bin > nvs_data.bin
 *
 *   # Program to flash
 *   $ uniflash.sh -ccxml CC2340R5.ccxml -program nvs_data.bin -offset 0x40000 -verify
 *
 * METHOD 2: Using XDS Debug Probe + CCS
 * --------------------------------------------------------------------
 * 1. Connect XDS110 or XDS200 debug probe
 * 2. Open CCS Memory Browser (View → Memory Browser)
 * 3. Navigate to address 0x40000
 * 4. Write bytes manually:
 *    - At 0x40000: Type version string "1.2.3" (will add null terminator)
 *    - At 0x40020: Write device ID bytes
 * 5. Or use "Load Memory" to load binary file
 * 6. Disconnect debugger (data persists in flash)
 *
 * METHOD 3: Using TI Debug Server (DSLite)
 * --------------------------------------------------------------------
 *   $ DSLite load -c CC2340R5.ccxml -f nvs_data.bin -o 0x40000
 *
 * METHOD 4: Programmatic (During Development/Testing)
 * --------------------------------------------------------------------
 * Add this test function to your code (see dev_info_flash.c for full example):
 *
 *   #include <ti/drivers/NVS.h>
 *
 *   void program_nvs_test_data(void)
 *   {
 *       // Program version string
 *       const char *version = "1.2.3";
 *       NVS_Handle nvs = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
 *       NVS_write(nvs, 0x0000, version, strlen(version) + 1, NVS_WRITE_POST_VERIFY);
 *
 *       // Program device ID
 *       uint8_t deviceData[32] = {
 *           0x01, 0x02, 0x24, 0x8A, 0x82, 0x66, 0x00, 0x01,
 *           0x0A, 'R','e','m','o','t','e','B','0','4','6',
 *           0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00
 *       };
 *       NVS_write(nvs, 0x0020, deviceData, 32, NVS_WRITE_POST_VERIFY);
 *       NVS_close(nvs);
 *   }
 *
 * ===================================================================
 * FALLBACK BEHAVIOR (Unprogrammed NVS)
 * ===================================================================
 *
 * If NVS offset 0x0020 is unprogrammed (all 0xFF):
 *   - Device Type: G10 (0x01)
 *   - Vendor ID: 0x8A24 (Google)
 *   - Product ID: 0x6682 (Google ATV Remote)
 *   - Product Version: 0x0100 (v1.0)
 *   - Model Number: "Google_G10"
 *   - Device Name: "GoogleRemote"
 *
 * If NVS offset 0x0000 is unprogrammed (all 0xFF or 0x00):
 *   - Software Revision: "1.0.0"
 *   - Firmware Revision: "1.0.0"
 *
 * This ensures Google ATV compliance even without production programming.
 *
 * ===================================================================
 * VERIFICATION PROCEDURE
 * ===================================================================
 *
 * Step 1: Verify Flash Programming (Using CCS Memory Browser)
 * --------------------------------------------------------------------
 * 1. Debug your application
 * 2. Open Memory Browser (View → Memory Browser)
 * 3. Navigate to 0x40000 and verify:
 *    - Bytes look like version string (not all 0xFF)
 * 4. Navigate to 0x40020 and verify:
 *    - First byte = device type (0x01-0x06, not 0xFF)
 *    - Bytes 1-7 = PnP ID data (02 24 8A 82 66...)
 *
 * Step 2: Verify via BLE (Using nRF Connect or LightBlue)
 * --------------------------------------------------------------------
 * 1. Power on device
 * 2. Connect via BLE
 * 3. Browse to Device Information Service (UUID 0x180A)
 * 4. Read characteristics:
 *    - Model Number (0x2A24) → "Google_G10" or "Google_G20" etc.
 *    - Serial Number (0x2A25) → 12-digit hex MAC address
 *    - Firmware Revision (0x2A26) → Your programmed version
 *    - Software Revision (0x2A28) → Your programmed version
 *    - PnP ID (0x2A50) → Should be: 02 24 8A 82 66 00 01 (or your product version)
 *
 * Step 3: Verify with Google ATV
 * --------------------------------------------------------------------
 * 1. Pair with Android TV device
 * 2. Check Settings → Remotes & Accessories
 * 3. Device should be recognized as "Google Remote"
 * 4. Model should display correctly (G10, G20, etc.)
 *
 * ===================================================================
 * IMPORTANT NOTES FOR PRODUCTION
 * ===================================================================
 *
 * 1. CRITICAL: Do NOT modify VID/PID values
 *    - VID MUST be 0x8A24 (Google)
 *    - PID MUST be 0x6682 (Google ATV Remote)
 *    - Vendor Source MUST be 0x02 (USB IF)
 *
 * 2. Device Type selection:
 *    - Choose based on physical remote model
 *    - G10 = Short remote (Hobb)
 *    - G20 = Long remote (Shaw)
 *    - Contact Google for other type assignments
 *
 * 3. Device Name (optional):
 *    - Max 16 characters
 *    - ASCII only, no null terminator in flash
 *    - If length=0, defaults to "GoogleRemote"
 *    - Can include serial number for tracking
 *
 * 4. Product Version:
 *    - 0x0100 = v1.0 (initial release)
 *    - 0x0101 = v1.1 (incremental update)
 *    - Increment for hardware revisions
 *
 * 5. Firmware Version String:
 *    - Use semantic versioning (X.Y.Z)
 *    - Update with each firmware release
 *    - Max 19 characters + null terminator
 *    - Examples: "1.0.0", "2.1.5", "1.0.0-beta"
 *
 * 6. Flash Endurance:
 *    - Device ID region (0x78000): Write ONCE during production
 *    - Version region (0x40000): Can be updated during OTA
 *    - NVS has ~10,000 write cycle endurance
 *
 * 7. Security:
 *    - Consider flash read protection for production
 *    - Prevents unauthorized reading of device data
 *    - Configure via CCFG (Customer Configuration)
 *
 *********************************************************************/

/*********************************************************************
 * TYPEDEFS
 */

/**
 * Device identification data read from flash at 0x78000
 */
typedef struct {
    uint8_t  deviceType;          /* Device type (0x01-0x06) */
    uint8_t  vendorSource;        /* PnP vendor source (0x02 = USB IF) */
    uint16_t vendorId;            /* Vendor ID (0x8a24 = Google) */
    uint16_t productId;           /* Product ID (0x6682) */
    uint16_t productVersion;      /* Product version (0x0100/0x0101) */
    uint8_t  deviceNameLen;       /* Length of device name (0-16) */
    char     deviceName[17];      /* Device name (null-terminated) */
    uint8_t  deviceUiLayout;      /* UI layout config */
    bool     isValid;             /* True if read from flash successfully */
} DeviceIdData_t;

/**
 * Firmware version extracted from NVS region
 */
typedef struct {
    uint8_t  major;               /* Major version */
    uint8_t  minor;               /* Minor version */
    uint16_t revision;            /* Revision number */
    uint32_t build;               /* Build number */
    char     versionStr[20];      /* Formatted string "X.Y.Z" */
    bool     isValid;             /* True if read successfully */
} FirmwareVersion_t;

/**
 * BLE configuration read from NVS at NVS_DEVICE_BLE_OFFSET
 */
typedef struct {
    uint8_t  periodicWakeupSwitch;   /* 0xFF=disable, 0x00=enable */
    uint16_t periodicWakeupInterval; /* Interval in minutes (little-endian) */
    uint8_t  rpaSwitch;              /* 0xFF=disable, 0x00=enable */
    uint8_t  bleSwitch;              /* 0x00=disable BLE, 0xFF=enable */
    uint8_t  cachePowerSwitch;       /* 0xFF=cache power key, 0x00=don't cache */
    bool     isValid;
} BleConfig_t;

/**
 * Security configuration read from NVS at NVS_DEVICE_SECURITY_OFFSET
 */
typedef struct {
    uint8_t authKey[16];        /* Authentication key */
    uint8_t encryptionPass[16]; /* Encryption password */
    uint8_t ecdsaKey[64];       /* ECDSA public key */
    bool    isValid;
} SecurityConfig_t;

/**
 * WakeUp Key Format extracted from NVS region
 */
typedef struct {
    uint16_t wakeUpKey1;                /* Bitmask: keys that trigger wakeup with KEY ID 0x01 (0xFFFF=disabled) */
    uint16_t wakeUpKey2;                /* Bitmask: keys that trigger wakeup with KEY ID 0x02 (0xFFFF=disabled) */
    uint8_t  wakeUpKeyCustom[31];       /* Custom wakeup packet format */
    bool     isValid;
} WakeUpKeyMask_t;

/**
 * Device Configuration extracted from NVS region
 */
typedef struct {
    uint8_t  couchModeSwitch;          /* 0xFF=disconnect/sleep, 0x01=BLE continue */
    uint8_t  txPower;                  /* TX power level (0xFF=3.50dBm, 0x23=7.27dBm, ...) */
    uint8_t  backlightSwitch;          /* 0x00=disable, 0x01=enable */
    bool     isValid;
} DeviceConfig_t;


/*********************************************************************
 * API FUNCTIONS
 */

/**
 * @brief Initialize flash reading module
 *
 * Must be called once at startup before using other functions.
 */
void DevInfoFlash_init(void);

/**
 * @brief Read device identification data from flash at 0x78000
 *
 * Reads manufacturer data region and validates contents.
 * Falls back to Google default values if flash is unprogrammed.
 *
 * @param data Pointer to structure to fill with device ID data
 * @return true if data read successfully (or defaults used), false on error
 */
bool DevInfoFlash_readDeviceId(DeviceIdData_t *data);

/**
 * @brief Read firmware version from NVS region
 *
 * Reads version string from CONFIG_NVSINTERNAL_STORAGE region at 0x40000.
 * Falls back to default version "1.0.0" if NVS unprogrammed.
 *
 * @param version Pointer to structure to fill with version data
 * @return true if version read from NVS, false if using default
 */
bool DevInfoFlash_readFirmwareVersion(FirmwareVersion_t *version);

/**
 * @brief Generate model number string from device type
 *
 * Converts device type byte to Google model string (e.g., "Google_G10").
 *
 * @param deviceType Device type byte (0x01-0x06)
 * @param modelStr Buffer to store model string
 * @param maxLen Maximum length of modelStr buffer
 */
void DevInfoFlash_getModelNumber(uint8_t deviceType, char *modelStr, uint8_t maxLen);

/**
 * @brief Generate serial number from MAC address
 *
 * Formats MAC address as hex string for serial number (e.g., "A4B5C6D7E8F9").
 *
 * @param macAddr   Pointer to 6-byte MAC address
 * @param serialStr Buffer to store serial number string
 * @param maxLen    Maximum length of serialStr buffer (minimum 13)
 * @return true if serial number written successfully, false on invalid input
 */
bool DevInfoFlash_getSerialNumber(uint8_t *macAddr, char *serialStr, uint8_t maxLen);

/**
 * @brief Read wakeup key configuration from NVS
 *
 * Reads the two wakeup key bitmasks and custom wakeup packet format.
 * A value of 0xFFFF for wakeUpKey1 or wakeUpKey2 means that field is disabled.
 * Otherwise each bit in the mask corresponds to a key (see Table 3.2) that
 * will trigger a wakeup packet with KEY ID 0x01 (Key1) or 0x02 (Key2).
 *
 * @param wakeUpConfig Pointer to structure to fill with wakeup configuration
 * @return true if read successfully, false on NVS error or NULL pointer
 */
bool DevInfoFlash_readWakeupConfig(WakeUpKeyMask_t *wakeUpConfig);

/**
 * @brief Read device hardware configuration from NVS
 *
 * Reads couch mode switch, TX power output setting, and backlight switch.
 * Fields default to 0xFF (disabled/default) when unprogrammed.
 *
 * @param deviceConfig Pointer to structure to fill with device configuration
 * @return true if read successfully, false on NVS error or NULL pointer
 */
bool DevInfoFlash_readDeviceConfig(DeviceConfig_t *deviceConfig);

/**
 * @brief Read BLE configuration from NVS
 *
 * Reads periodic wakeup settings, RPA switch, BLE enable switch, and
 * cache power switch. Fields default to 0xFF (disabled/default) when unprogrammed.
 *
 * @param bleConfig Pointer to structure to fill with BLE configuration
 * @return true if read successfully, false on NVS error or NULL pointer
 */
bool DevInfoFlash_readBLEConfig(BleConfig_t *bleConfig);

/**
 * @brief Read security configuration from NVS
 *
 * Reads authentication key (16 bytes), encryption password (16 bytes),
 * and ECDSA public key (64 bytes). All fields are 0xFF when unprogrammed.
 *
 * @param securityConfig Pointer to structure to fill with security configuration
 * @return true if read successfully, false on NVS error or NULL pointer
 */
bool DevInfoFlash_readSecurityConfig(SecurityConfig_t *securityConfig);

/*********************************************************************
 * GLOBAL RUNTIME CONFIGURATION
 *********************************************************************/

/**
 * Container for all flash configuration loaded once at boot.
 * Access via g_rcConfig after DevInfoFlash_loadAll() is called.
 */
typedef struct {
    DeviceIdData_t    deviceId;
    FirmwareVersion_t firmwareVersion;
    WakeUpKeyMask_t   wakeupMask;
    DeviceConfig_t    deviceConfig;
    BleConfig_t       bleConfig;
    SecurityConfig_t  securityConfig;
    bool              loaded;           /* true after DevInfoFlash_loadAll() succeeds */
} RcConfig_t;

extern RcConfig_t g_rcConfig;

/**
 * @brief Load all flash configuration into RAM once at boot.
 *
 * Reads all NVS regions into g_rcConfig. Must be called once during
 * startup (before Peripheral_start()) after DevInfoFlash_init().
 * All other modules must access g_rcConfig directly instead of
 * calling individual read functions at runtime.
 */
void DevInfoFlash_loadAll(void);

#ifdef __cplusplus
}
#endif

#endif /* DEV_INFO_FLASH_H */
