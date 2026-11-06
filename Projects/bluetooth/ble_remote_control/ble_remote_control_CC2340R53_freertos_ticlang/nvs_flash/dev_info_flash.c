/******************************************************************************

@file  dev_info_flash.c

@brief Device Information Flash Reading Module Implementation - Implements
flash reading functions for BLE Device Information Service.

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

/*********************************************************************
 * INCLUDES
 */
#include "dev_info_flash.h"
#include <string.h>
#include <stdio.h>
#include <ti/drivers/NVS.h>
#include <ti_drivers_config.h>

/*********************************************************************
 * LOCAL VARIABLES
 */
static bool initialized = false;

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/**
 * @brief Check if flash data is unprogrammed (all 0xFF)
 */
static bool isFlashUnprogrammed(uint8_t *data, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++)
    {
        if (data[i] != FLASH_UNPROGRAMMED)
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief Validate device type byte
 */
static bool isValidDeviceType(uint8_t deviceType)
{
    return (deviceType >= DEVICE_TYPE_G10 && deviceType <= DEVICE_TYPE_G91);
}

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/**
 * @brief Initialize flash reading module
 */
void DevInfoFlash_init(void)
{
    if (!initialized)
    {
        // Initialize NVS driver
        NVS_init();

        initialized = true;
    }
}

/**
 * @brief Read device identification data from NVS at offset 0x0020
 */
bool DevInfoFlash_readDeviceId(DeviceIdData_t *data)
{
    NVS_Handle nvsHandle = NULL;
    int_fast16_t status;
    uint8_t buffer[NVS_DEVICE_ID_SIZE];

    if (data == NULL)
    {
        return false;
    }

    // Initialize data structure
    memset(data, 0, sizeof(DeviceIdData_t));

    // Open CONFIG_NVSINTERNAL_STORAGE region
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        // Cannot open NVS, use Google defaults
        data->vendorSource = 0x02;
        data->vendorId = GOOGLE_DEFAULT_VID;
        data->productId = GOOGLE_DEFAULT_PID;
        data->productVersion = GOOGLE_DEFAULT_VER;
        data->deviceType = DEVICE_TYPE_G10;
        strcpy(data->deviceName, "GoogleRemote");
        data->isValid = true;
        return true;
    }

    // Read device ID block from NVS offset 0x0020
    memset(buffer, 0, sizeof(buffer));
    status = NVS_read(nvsHandle, NVS_DEVICE_ID_OFFSET, buffer, NVS_DEVICE_ID_SIZE);
    NVS_close(nvsHandle);

    if (status != NVS_STATUS_SUCCESS)
    {
        // NVS read failed, use Google defaults
        data->vendorSource = 0x02;
        data->vendorId = GOOGLE_DEFAULT_VID;
        data->productId = GOOGLE_DEFAULT_PID;
        data->productVersion = GOOGLE_DEFAULT_VER;
        data->deviceType = DEVICE_TYPE_G10;
        strcpy(data->deviceName, "GoogleRemote");
        data->isValid = true;
        return true;
    }

    // Check if NVS is unprogrammed (all 0xFF)
    if (isFlashUnprogrammed(buffer, 8))
    {
        // NVS unprogrammed, use Google defaults
        data->vendorSource = 0x02;
        data->vendorId = GOOGLE_DEFAULT_VID;
        data->productId = GOOGLE_DEFAULT_PID;
        data->productVersion = GOOGLE_DEFAULT_VER;
        data->deviceType = DEVICE_TYPE_G10;
        strcpy(data->deviceName, "GoogleRemote");
        data->isValid = true;
        return true;
    }

    // Parse NVS data (little-endian format)
    data->deviceType = buffer[DEVICE_TYPE_OFFSET];
    data->vendorSource = buffer[VENDOR_SOURCE_OFFSET];
    data->vendorId = (buffer[VENDOR_ID_OFFSET + 1] << 8) | buffer[VENDOR_ID_OFFSET];
    data->productId = (buffer[PRODUCT_ID_OFFSET + 1] << 8) | buffer[PRODUCT_ID_OFFSET];
    data->productVersion = (buffer[PRODUCT_VER_OFFSET + 1] << 8) | buffer[PRODUCT_VER_OFFSET];
    data->deviceNameLen = buffer[DEVICE_NAME_LEN_OFFSET];

    // Validate device type
    if (!isValidDeviceType(data->deviceType))
    {
        data->deviceType = DEVICE_TYPE_G10;  // Default to G10
    }

    // Read device name if present
    if (data->deviceNameLen > 0 && data->deviceNameLen <= MAX_DEVICE_NAME_LEN)
    {
        memcpy(data->deviceName, &buffer[DEVICE_NAME_OFFSET], data->deviceNameLen);
        data->deviceName[data->deviceNameLen] = '\0';
    }
    else
    {
        strcpy(data->deviceName, "GoogleRemote");
        data->deviceNameLen = strlen(data->deviceName);
    }

    // Read UI layout
    data->deviceUiLayout = buffer[DEVICE_UI_LAYOUT_OFFSET];

    data->isValid = true;
    return true;
}

/**
 * @brief Read firmware version from NVS region
 */
bool DevInfoFlash_readFirmwareVersion(FirmwareVersion_t *version)
{
    NVS_Handle nvsHandle = NULL;
    int_fast16_t status;
    char versionStr[NVS_VERSION_MAX_LEN];

    if (version == NULL)
    {
        return false;
    }

    // Initialize version structure with defaults
    memset(version, 0, sizeof(FirmwareVersion_t));
    strcpy(version->versionStr, GOOGLE_DEFAULT_FW_VER);
    version->isValid = false;

    // Open CONFIG_NVSINTERNAL_STORAGE region
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        // Cannot open NVS, use default
        return false;
    }

    // Read version string from offset 0
    memset(versionStr, 0, sizeof(versionStr));
    status = NVS_read(nvsHandle, NVS_VERSION_OFFSET, versionStr, NVS_VERSION_MAX_LEN);
    NVS_close(nvsHandle);

    if (status == NVS_STATUS_SUCCESS &&
        versionStr[0] != 0xFF &&
        versionStr[0] != 0x00)
    {
        // Valid version string found in NVS
        strncpy(version->versionStr, versionStr, sizeof(version->versionStr) - 1);
        version->versionStr[sizeof(version->versionStr) - 1] = '\0';

        // Parse version string (format: "X.Y.Z")
        sscanf(version->versionStr, "%hhu.%hhu.%hu",
               &version->major, &version->minor, &version->revision);

        version->isValid = true;
        return true;
    }

    // No valid version in NVS, default already set
    return false;
}

/**
 * @brief Generate model number string from device type
 */
void DevInfoFlash_getModelNumber(uint8_t deviceType, char *modelStr, uint8_t maxLen)
{
    const char *models[] = {"G10", "G20", "G11", "G31", "G21", "G91"};

    if (modelStr == NULL || maxLen < 10)
    {
        return;
    }
    else {
        switch (deviceType)
        {
            case 0x01:
            memcpy(modelStr, &models[0], sizeof(models[0]));
            break;
            case 0x02:
            memcpy(modelStr, &models[1], sizeof(models[1]));
            break;
            case 0x03:
            memcpy(modelStr, &models[2], sizeof(models[2]));
            break;
            case 0x04:
            memcpy(modelStr, &models[3], sizeof(models[3]));
            break;
            case 0x05:
            memcpy(modelStr, &models[4], sizeof(models[4]));
            break;
            case 0x06:
            memcpy(modelStr, &models[5], sizeof(models[5]));
            break;
            default:
                break;
        }
    }

}

/**
 * @brief Generate serial number from MAC address
 */
bool DevInfoFlash_getSerialNumber(uint8_t *macAddr, char *serialStr, uint8_t maxLen)
{
    if (macAddr == NULL || serialStr == NULL || maxLen < 13)
    {
        return false;
    }
    return false;
}

/**
 * @brief Read Device Wakeup Configuration
 */
bool DevInfoFlash_readWakeupConfig(WakeUpKeyMask_t *wakeUpConfig)
{
    NVS_Handle nvsHandle = NULL;
    int_fast16_t status;
    uint8_t wakeupConfigBuffer[NVS_DEVICE_WAKEUP_SIZE];

    if (wakeUpConfig == NULL)
    {
        return false;
    }

    memset(wakeUpConfig, 0, sizeof(WakeUpKeyMask_t));

    // Open CONFIG_NVSINTERNAL_STORAGE region
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        return false;
    }

    memset(wakeupConfigBuffer, 0, sizeof(wakeupConfigBuffer));
    status = NVS_read(nvsHandle, NVS_DEVICE_WAKEUP_OFFSET, wakeupConfigBuffer, NVS_DEVICE_WAKEUP_SIZE);
    NVS_close(nvsHandle);

    if (status == NVS_STATUS_SUCCESS)
    {
        /* Big-endian 2-byte fields */
        wakeUpConfig->wakeUpKey1 = (wakeupConfigBuffer[DEVICE_WAKEUP_KEY_1]     << 8) |
                                    wakeupConfigBuffer[DEVICE_WAKEUP_KEY_1 + 1];
        wakeUpConfig->wakeUpKey2 = (wakeupConfigBuffer[DEVICE_WAKEUP_KEY_2]     << 8) |
                                    wakeupConfigBuffer[DEVICE_WAKEUP_KEY_2 + 1];
        memcpy(wakeUpConfig->wakeUpKeyCustom, &wakeupConfigBuffer[DEVICE_WAKEUP_KEY_CUSTOM_FORMAT], 31);
        wakeUpConfig->isValid = true;
        return true;
    }

    return false;
}

/**
 * @brief Read Device Configuration
 */
bool DevInfoFlash_readDeviceConfig(DeviceConfig_t *deviceConfig)
{
    NVS_Handle nvsHandle = NULL;
    int_fast16_t status;
    uint8_t deviceConfigBuffer[NVS_DEVICE_CONFIG_SIZE];

    if (deviceConfig == NULL)
    {
        return false;
    }

    memset(deviceConfig, 0, sizeof(DeviceConfig_t));

    // Open CONFIG_NVSINTERNAL_STORAGE region
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        return false;
    }

    memset(deviceConfigBuffer, 0, sizeof(deviceConfigBuffer));
    status = NVS_read(nvsHandle, NVS_DEVICE_CONFIG_OFFSET, deviceConfigBuffer, NVS_DEVICE_CONFIG_SIZE);
    NVS_close(nvsHandle);

    if (status == NVS_STATUS_SUCCESS)
    {
        deviceConfig->couchModeSwitch = deviceConfigBuffer[DEVICE_COUCH_MODE_SWITCH];
        deviceConfig->txPower         = deviceConfigBuffer[DEVICE_TX_POWER_OUTPUT];
        deviceConfig->backlightSwitch = deviceConfigBuffer[DEVICE_BACKLIGHT_SWITCH];
        deviceConfig->isValid = true;
        return true;
    }

    return false;
}

/**
 * @brief Read BLE Device Configuration
 */
bool DevInfoFlash_readBLEConfig(BleConfig_t *bleConfig)
{
    NVS_Handle nvsHandle = NULL;
    int_fast16_t status;
    uint8_t bleConfigBuffer[NVS_DEVICE_BLE_SIZE];

    if (bleConfig == NULL)
    {
        return false;
    }

    memset(bleConfig, 0, sizeof(BleConfig_t));

    // Open CONFIG_NVSINTERNAL_STORAGE region
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        return false;
    }

    memset(bleConfigBuffer, 0, sizeof(bleConfigBuffer));
    status = NVS_read(nvsHandle, NVS_DEVICE_BLE_OFFSET, bleConfigBuffer, NVS_DEVICE_BLE_SIZE);
    NVS_close(nvsHandle);

    if (status == NVS_STATUS_SUCCESS)
    {
        bleConfig->periodicWakeupSwitch   = bleConfigBuffer[DEVICE_PERIODIC_WAKEUP_SWITCH];
        bleConfig->periodicWakeupInterval = (bleConfigBuffer[DEVICE_PERIODIC_WAKEUP_INT + 1] << 8) |
                                             bleConfigBuffer[DEVICE_PERIODIC_WAKEUP_INT];
        bleConfig->rpaSwitch        = bleConfigBuffer[DEVICE_RPA_SWITCH];
        bleConfig->bleSwitch        = bleConfigBuffer[DEVICE_BLE_SWITCH];
        bleConfig->cachePowerSwitch = bleConfigBuffer[DEVICE_CACHE_POWER_SWITCH];
        bleConfig->isValid = true;
        return true;
    }

    return false;
}

/*********************************************************************
 * GLOBAL RUNTIME CONFIGURATION
 *********************************************************************/

RcConfig_t g_rcConfig = {0};

/**
 * @brief Load all flash configuration into RAM once at boot
 */
void DevInfoFlash_loadAll(void)
{
    DevInfoFlash_init();

    DevInfoFlash_readDeviceId(&g_rcConfig.deviceId);
    DevInfoFlash_readFirmwareVersion(&g_rcConfig.firmwareVersion);
    DevInfoFlash_readWakeupConfig(&g_rcConfig.wakeupMask);
    DevInfoFlash_readDeviceConfig(&g_rcConfig.deviceConfig);
    DevInfoFlash_readBLEConfig(&g_rcConfig.bleConfig);
    DevInfoFlash_readSecurityConfig(&g_rcConfig.securityConfig);

    g_rcConfig.loaded = true;
}

/**
 * @brief Read Security Device Configuration
 */
bool DevInfoFlash_readSecurityConfig(SecurityConfig_t *securityConfig)
{
    NVS_Handle nvsHandle = NULL;
    int_fast16_t status;
    uint8_t securityConfigBuffer[NVS_DEVICE_SECURITY_SIZE];

    if (securityConfig == NULL)
    {
        return false;
    }

    memset(securityConfig, 0, sizeof(SecurityConfig_t));

    // Open CONFIG_NVSINTERNAL_STORAGE region
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        return false;
    }

    memset(securityConfigBuffer, 0, sizeof(securityConfigBuffer));
    status = NVS_read(nvsHandle, NVS_DEVICE_SECURITY_OFFSET, securityConfigBuffer, NVS_DEVICE_SECURITY_SIZE);
    NVS_close(nvsHandle);

    if (status == NVS_STATUS_SUCCESS)
    {
        memcpy(securityConfig->authKey,        &securityConfigBuffer[DEVICE_AUTH_KEY],   16);
        memcpy(securityConfig->encryptionPass, &securityConfigBuffer[DEVICE_ENCRY_PASS], 16);
        memcpy(securityConfig->ecdsaKey,       &securityConfigBuffer[DEVICE_ECDSA_KEY],  64);
        securityConfig->isValid = true;
        return true;
    }

    return false;
}

/* =========================================================================
 * ALTERNATIVE so that MCUBOOT read from NVS set Address: Read ECDSA public key from NVS flash region at runtime
 * =========================================================================
 *
 * Instead of compiling the EC-P256 public key into the bootloader binary,
 * the key can be stored in a known NVS flash region and loaded at boot time.
 * This allows updating the verification key without reflashing MCUboot.
 *
 * NVS memory map (project-specific):
 *   Flash base for NVS region : 0x70000  (CONFIG_NVSINTERNAL_STORAGE)
 *   Security config offset    : 0x00C0
 *   ecdsaKey field offset     : +0x20    (after authKey[16] + encryptionPass[16])
 *   Absolute flash address    : 0x700E0
 *
 * Key format in NVS (64 bytes):
 *   Bytes  0-31  : X coordinate of EC-P256 public key (big-endian)
 *   Bytes 32-63  : Y coordinate of EC-P256 public key (big-endian)
 *
 * Assembled DER format (91 bytes total) = 27-byte header + 64-byte X,Y:
 *   Header : SEQUENCE { AlgorithmIdentifier { ecPublicKey, P-256 } BIT STRING }
 *   Data   : 0x04 || X[32] || Y[32]  (uncompressed point)
 *
 * CC23xx flash is memory-mapped: the region at 0x700E0 can be read directly
 * via a pointer (no NVS driver needed for reads). Writes still require the
 * flash controller (NVS driver or driverlib FlashProgram/FlashSectorErase).
 *
 * Unprogrammed flash (all 0xFF) is detected as a sentinel and the compiled-in
 * root_pub_der[] above is used as a fallback.
 *
 * USAGE:
 *   1. Call keys_init() from mcuboot_app main, before boot_go().
 *   2. The bootutil_keys[] table already points to root_pub_der / root_pub_der_len.
 *      keys_init() overwrites root_pub_der_from_nvs[] and redirects the table
 *      pointers when a valid NVS key is found.
 *
 * SECURITY WARNING:
 *   Storing the verification key in writable flash weakens the security model:
 *   an attacker who can write to 0x700E0 can substitute their own key.
 *   Only use this approach if the NVS region is protected (e.g., by a hardware
 *   write-protect fuse or a separate secure element controlling flash access).
 *
 * =========================================================================

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Absolute flash address of ecdsaKey[64] in NVS security config region
#define NVS_ECDSA_KEY_ADDR  0x700E0U
#define NVS_ECDSA_KEY_LEN   64U     // raw X,Y bytes (no DER wrapping)
#define EC256_DER_HDR_LEN   27U     // fixed header for EC-P256 SubjectPublicKeyInfo
#define EC256_DER_TOTAL_LEN 91U     // 27 header + 1 uncompressed marker + 32 X + 32 Y

// Fixed 27-byte DER header for EC-P256 SubjectPublicKeyInfo (RFC 5480).
// Identical to the first 27 bytes of root_pub_der[] above.
static const unsigned char ec_p256_der_header[EC256_DER_HDR_LEN] = {
    0x30, 0x59,                                     // SEQUENCE (89 bytes)
    0x30, 0x13,                                     // SEQUENCE (19 bytes) — AlgorithmIdentifier
    0x06, 0x07,                                     // OID (7 bytes) — ecPublicKey
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
    0x06, 0x08,                                     // OID (8 bytes) — P-256 curve
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
    0x03, 0x42, 0x00,                               // BIT STRING (66 bytes, 0 unused bits)
    0x04                                            // uncompressed point marker
};

// Buffer for the assembled DER key (filled by keys_init() if NVS key present)
static unsigned char         root_pub_der_from_nvs[EC256_DER_TOTAL_LEN];
static unsigned int          root_pub_der_from_nvs_len = EC256_DER_TOTAL_LEN;

// Checks whether a 64-byte buffer is all 0xFF (unprogrammed flash)
static bool is_flash_unprogrammed(const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        if (buf[i] != 0xFF)
        {
            return false;
        }
    }
    return true;
}

// Call this once before boot_go().
// If a valid EC-P256 key is found in NVS it is assembled into DER format and
// the bootutil_keys[] table is updated to point to it.
void keys_init(void)
{
    // Direct pointer read — CC23xx flash is memory-mapped, no driver needed
    const uint8_t *nvs_key_ptr = (const uint8_t *)NVS_ECDSA_KEY_ADDR;

    // Sentinel check: unprogrammed flash is all 0xFF
    if (is_flash_unprogrammed(nvs_key_ptr, NVS_ECDSA_KEY_LEN))
    {
        // No key programmed in NVS — keep the compiled-in root_pub_der[]
        return;
    }

    // Assemble full DER: 27-byte header || 64-byte X,Y
    memcpy(root_pub_der_from_nvs,                  ec_p256_der_header, EC256_DER_HDR_LEN);
    memcpy(root_pub_der_from_nvs + EC256_DER_HDR_LEN, nvs_key_ptr,    NVS_ECDSA_KEY_LEN);

    // Redirect the verification key table to the NVS-sourced key
    // (bootutil_keys is declared const in the linker but lives in RAM after init)
    bootutil_keys[0].key = root_pub_der_from_nvs;
    bootutil_keys[0].len = &root_pub_der_from_nvs_len;
}

 * ========================================================================= */