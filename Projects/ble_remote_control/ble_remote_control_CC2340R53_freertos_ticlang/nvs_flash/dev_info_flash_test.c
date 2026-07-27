/******************************************************************************

@file  dev_info_flash_test.c

@brief Test functions for programming NVS data during development

       DEVELOPMENT ONLY: These functions are for testing NVS programming.
       Remove or disable before production build.

       Usage:
       1. Call program_nvs_test_data() ONCE from main() or a button handler
       2. Reset device
       3. Verify Device Information Service via BLE scanner
       4. Comment out or remove the function call after testing

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

#include "dev_info_flash.h"
#include "ir_flash.h"
#include <ti/drivers/NVS.h>
#include <ti_drivers_config.h>
#include <string.h>

/*********************************************************************
 * TEST FUNCTIONS
 */

/**
 * @brief Program test firmware version to NVS
 *
 * Programs version "1.2.3" to NVS offset 0x0000
 */
void program_test_version(void)
{
    NVS_Handle nvsHandle;
    const char *version = "1.2.3";  // Change to your version
    int_fast16_t status;

    NVS_init();

    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        // Failed to open NVS
        return;
    }

    // Write version string at offset 0x0000
    status = NVS_write(nvsHandle, NVS_VERSION_OFFSET,
                      (void*)version, strlen(version) + 1,
                      NVS_WRITE_POST_VERIFY);

    NVS_close(nvsHandle);

    // Check status (0 = success)
    if (status == NVS_STATUS_SUCCESS)
    {
        // Version programmed successfully
    }
}

/**
 * @brief Program test device ID to NVS
 *
 * Programs G10 device with name "TestRemote" to NVS offset 0x0020
 */
void program_test_device_id(void)
{
    NVS_Handle nvsHandle;
    uint8_t deviceData[32];
    int_fast16_t status;

    // Build device ID data structure
    deviceData[0] = 0x01;              // Device Type: G10 (Hobb short)
    deviceData[1] = 0x02;              // Vendor Source: USB IF
    deviceData[2] = 0x24;              // Vendor ID low byte
    deviceData[3] = 0x8A;              // Vendor ID high byte (0x8A24 = Google)
    deviceData[4] = 0x82;              // Product ID low byte
    deviceData[5] = 0x66;              // Product ID high byte (0x6682 = Google ATV)
    deviceData[6] = 0x00;              // Product Version low byte
    deviceData[7] = 0x01;              // Product Version high byte (0x0100 = v1.0)
    deviceData[8] = 0x0A;              // Device Name Length: 10

    // Device Name: "TestRemote"
    const char *name = "TestRemote";
    memcpy(&deviceData[9], name, 10);

    // Fill remaining with 0xFF (unprogrammed)
    memset(&deviceData[19], 0xFF, 12);

    deviceData[31] = 0x00;             // UI Layout

    NVS_init();

    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        return;
    }

    // Write device ID at offset 0x0020
    status = NVS_write(nvsHandle, NVS_DEVICE_ID_OFFSET,
                      deviceData, sizeof(deviceData),
                      NVS_WRITE_POST_VERIFY);

    NVS_close(nvsHandle);
}

/**
 * @brief Program complete test data (version + device ID)
 *
 * Call this ONCE from main() during development testing.
 * Comment out after successful test.
 *
 * Example usage in main.c:
 *
 *   #include "nvs_flash/dev_info_flash_test.h"
 *
 *   void main() {
 *       // ... initialization code ...
 *
 *       // DEVELOPMENT ONLY: Program test NVS data
 *       // program_nvs_test_data();  // Uncomment ONCE to program, then comment out
 *
 *       // ... rest of main ...
 *   }
 */
void program_nvs_test_data(void)
{
    // Program firmware version
    program_test_version();

    // Program device identification
    program_test_device_id();

    // After calling this function once:
    // 1. Reset the device
    // 2. NVS will contain the test data
    // 3. Device Info Service will read from NVS
    // 4. Verify with BLE scanner
    // 5. Comment out this function call
}

/**
 * @brief Erase NVS data (for testing fallback behavior)
 *
 * Erases the entire NVS region to test fallback to Google defaults.
 * WARNING: This erases ALL NVS data.
 */
void erase_nvs_test_data(void)
{
    NVS_Handle nvsHandle;
    NVS_Attrs attrs;

    NVS_init();

    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        return;
    }

    // Get NVS attributes
    NVS_getAttrs(nvsHandle, &attrs);

    // Erase entire region
    NVS_erase(nvsHandle, 0, attrs.regionSize);

    NVS_close(nvsHandle);

    // After erasing:
    // - Device will use Google defaults (G10, v1.0.0)
    // - Model Number will be "Google_G10"
    // - Version will be "1.0.0"
}

/**
 * @brief Program G20 device with custom name
 *
 * Example: Program G20 (Shaw long remote) with name "RemoteB046"
 */
void program_g20_test_data(void)
{
    NVS_Handle nvsHandle;
    uint8_t deviceData[32];
    const char *version = "1.0.0";
    const char *name = "RemoteB046";

    NVS_init();
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        return;
    }

    // Program version
    NVS_write(nvsHandle, NVS_VERSION_OFFSET,
             (void*)version, strlen(version) + 1,
             NVS_WRITE_POST_VERIFY);

    // Build G20 device data
    deviceData[0] = 0x02;              // Device Type: G20 (Shaw long)
    deviceData[1] = 0x02;              // Vendor Source: USB IF
    deviceData[2] = 0x24;              // Vendor ID: 0x8A24 (Google)
    deviceData[3] = 0x8A;
    deviceData[4] = 0x82;              // Product ID: 0x6682
    deviceData[5] = 0x66;
    deviceData[6] = 0x00;              // Product Version: 0x0100
    deviceData[7] = 0x01;
    deviceData[8] = strlen(name);      // Device Name Length

    memcpy(&deviceData[9], name, strlen(name));
    memset(&deviceData[9 + strlen(name)], 0xFF, 22 - strlen(name));
    deviceData[31] = 0x00;             // UI Layout

    // Program device ID
    NVS_write(nvsHandle, NVS_DEVICE_ID_OFFSET,
             deviceData, sizeof(deviceData),
             NVS_WRITE_POST_VERIFY);

    NVS_close(nvsHandle);
}

/**
 * @brief Program wakeup key configuration to NVS
 *
 * Programs wakeUpKey1=0x0007 (bits 0,1,2 = Assistant, Home, Power wakeup)
 * and wakeUpKey2=0xFFFF (disabled). Custom format left unprogrammed.
 *
 * Fields are big-endian as read by DevInfoFlash_readWakeupConfig().
 */
void program_test_wakeup_config(void)
{
    NVS_Handle nvsHandle;
    uint8_t buffer[NVS_DEVICE_WAKEUP_SIZE];
    int_fast16_t status;

    memset(buffer, 0xFF, sizeof(buffer));

    /* wakeUpKey1 = 0x0007 (big-endian): bits 0,1,2 = Assistant, Home, Power */
    buffer[DEVICE_WAKEUP_KEY_1]     = 0x00;
    buffer[DEVICE_WAKEUP_KEY_1 + 1] = 0x07;

    /* wakeUpKey2 = 0xFFFF (big-endian): disabled */
    buffer[DEVICE_WAKEUP_KEY_2]     = 0xFF;
    buffer[DEVICE_WAKEUP_KEY_2 + 1] = 0xFF;

    /* DEVICE_WAKEUP_KEY_CUSTOM_FORMAT (31 bytes): left 0xFF (unprogrammed) */

    NVS_init();
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        return;
    }

    status = NVS_write(nvsHandle, NVS_DEVICE_WAKEUP_OFFSET,
                      buffer, sizeof(buffer),
                      NVS_WRITE_POST_VERIFY);
    NVS_close(nvsHandle);
    (void)status;
}

/**
 * @brief Program device hardware configuration to NVS
 *
 * Programs:
 * - couchModeSwitch = 0xFF (shutdown on inactivity, default)
 * - txPower         = 0x04 (4 dBm)
 * - backlightSwitch = 0x01 (enabled)
 */
void program_test_device_config(void)
{
    NVS_Handle nvsHandle;
    uint8_t buffer[NVS_DEVICE_CONFIG_SIZE];
    int_fast16_t status;

    memset(buffer, 0xFF, sizeof(buffer));

    buffer[DEVICE_COUCH_MODE_SWITCH] = 0xFF;  /* Shutdown on inactivity (default) */
    buffer[DEVICE_TX_POWER_OUTPUT]   = 0x04;  /* 4 dBm TX power */
    buffer[DEVICE_BACKLIGHT_SWITCH]  = 0x01;  /* Backlight enabled */

    NVS_init();
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        return;
    }

    status = NVS_write(nvsHandle, NVS_DEVICE_CONFIG_OFFSET,
                      buffer, sizeof(buffer),
                      NVS_WRITE_POST_VERIFY);
    NVS_close(nvsHandle);
    (void)status;
}

/**
 * @brief Program BLE configuration to NVS
 *
 * Programs:
 * - periodicWakeupSwitch   = 0xFF (disabled)
 * - periodicWakeupInterval = 10 minutes (little-endian)
 * - rpaSwitch              = 0x00 (RPA enabled)
 * - bleSwitch              = 0xFF (BLE advertising enabled)
 * - cachePowerSwitch       = 0xFF (cache power key on reconnect)
 */
void program_test_ble_config(void)
{
    NVS_Handle nvsHandle;
    uint8_t buffer[NVS_DEVICE_BLE_SIZE];
    int_fast16_t status;

    memset(buffer, 0xFF, sizeof(buffer));

    buffer[DEVICE_PERIODIC_WAKEUP_SWITCH]    = 0xFF;  /* Periodic wakeup disabled */
    buffer[DEVICE_PERIODIC_WAKEUP_INT]       = 0x0A;  /* Interval: 10 min (low byte, little-endian) */
    buffer[DEVICE_PERIODIC_WAKEUP_INT + 1]   = 0x00;  /* Interval: high byte */
    buffer[DEVICE_RPA_SWITCH]                = 0x00;  /* RPA enabled */
    buffer[DEVICE_BLE_SWITCH]                = 0xFF;  /* BLE advertising enabled */
    buffer[DEVICE_CACHE_POWER_SWITCH]        = 0xFF;  /* Cache power key enabled */

    NVS_init();
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        return;
    }

    status = NVS_write(nvsHandle, NVS_DEVICE_BLE_OFFSET,
                      buffer, sizeof(buffer),
                      NVS_WRITE_POST_VERIFY);
    NVS_close(nvsHandle);
    (void)status;
}

/**
 * @brief Program security configuration to NVS
 *
 * Programs recognizable test patterns:
 * - authKey[16]        = 0x01 .. 0x10
 * - encryptionPass[16] = 0x11 .. 0x20
 * - ecdsaKey[64]       = 0x01 .. 0x40
 */
void program_test_security_config(void)
{
    NVS_Handle nvsHandle;
    uint8_t buffer[NVS_DEVICE_SECURITY_SIZE];
    int_fast16_t status;
    uint8_t i;

    memset(buffer, 0xFF, sizeof(buffer));

    /* Auth key: 0x01 .. 0x10 */
    for (i = 0; i < 16; i++)
    {
        buffer[DEVICE_AUTH_KEY + i] = 0x01 + i;
    }

    /* Encryption password: 0x11 .. 0x20 */
    for (i = 0; i < 16; i++)
    {
        buffer[DEVICE_ENCRY_PASS + i] = 0x11 + i;
    }

    /* ECDSA key: 0x01 .. 0x40 */
    for (i = 0; i < 64; i++)
    {
        buffer[DEVICE_ECDSA_KEY + i] = 0x01 + i;
    }

    NVS_init();
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        return;
    }

    status = NVS_write(nvsHandle, NVS_DEVICE_SECURITY_OFFSET,
                      buffer, sizeof(buffer),
                      NVS_WRITE_POST_VERIFY);
    NVS_close(nvsHandle);
    (void)status;
}

/**
 * @brief Program IR table covering all four IrCodeType_t paths
 *
 * Slot layout after programming:
 *   Slots  0-9:  Samsung/generic codes     → IR_TYPE_PROGRAMMED
 *   Slot  10:    {0x00,0x00,0x00,0x00}     → IR_TYPE_DISABLED
 *   Slot  11:    {0x00,0xFF,0xFF,0xFF}     → IR_TYPE_DEFAULT (use firmware default)
 *   Slots 12-47: {0xFF,0xFF,0xFF,0xFF}     → IR_TYPE_UNPROGRAMMED
 *
 * This allows KeyRouter to exercise all code-type branches in a single boot.
 * Verify with IrFlash_getCodeType() in the debugger after reset.
 *
 * NOTE: erase_nvs_test_data() (or erase_ir_table()) must be called first.
 * NVS_write without NVS_WRITE_ERASE can only write bits 1→0; the flash
 * must already be 0xFF before calling this function.
 */
void program_test_ir_table(void)
{
    IrCodeTable_t table;
    uint8_t i;

    NVS_init();

    /* Start with all slots unprogrammed */
    for (i = 0; i < NVS_IR_NUM_ENTRIES; i++)
    {
        table.entries[i].address    = 0xFF;
        table.entries[i].address2   = 0xFF;
        table.entries[i].command    = 0xFF;
        table.entries[i].commandInv = 0xFF;
    }

    /* Slot 0: Power - Samsung (addr 0x07, cmd 0x02) */
    table.entries[IR_SLOT_POWER].address    = 0x07;
    table.entries[IR_SLOT_POWER].address2   = 0xF8;
    table.entries[IR_SLOT_POWER].command    = 0x02;
    table.entries[IR_SLOT_POWER].commandInv = 0xFD;

    /* Slot 1: Volume Up - Samsung (cmd 0x07) */
    table.entries[IR_SLOT_VOLUME_UP].address    = 0x07;
    table.entries[IR_SLOT_VOLUME_UP].address2   = 0xF8;
    table.entries[IR_SLOT_VOLUME_UP].command    = 0x07;
    table.entries[IR_SLOT_VOLUME_UP].commandInv = 0xF8;

    /* Slot 2: Volume Down - Samsung (cmd 0x0B) */
    table.entries[IR_SLOT_VOLUME_DOWN].address    = 0x07;
    table.entries[IR_SLOT_VOLUME_DOWN].address2   = 0xF8;
    table.entries[IR_SLOT_VOLUME_DOWN].command    = 0x0B;
    table.entries[IR_SLOT_VOLUME_DOWN].commandInv = 0xF4;

    /* Slot 3: Volume Mute - Samsung (cmd 0x0F) */
    table.entries[IR_SLOT_VOLUME_MUTE].address    = 0x07;
    table.entries[IR_SLOT_VOLUME_MUTE].address2   = 0xF8;
    table.entries[IR_SLOT_VOLUME_MUTE].command    = 0x0F;
    table.entries[IR_SLOT_VOLUME_MUTE].commandInv = 0xF0;

    /* Slot 4: TV Input (addr 0x88) */
    table.entries[IR_SLOT_TV_INPUT].address    = 0x88;
    table.entries[IR_SLOT_TV_INPUT].address2   = 0x77;
    table.entries[IR_SLOT_TV_INPUT].command    = 0x60;
    table.entries[IR_SLOT_TV_INPUT].commandInv = 0x9F;

    /* Slot 5: Channel Up */
    table.entries[IR_SLOT_CHANNEL_UP].address    = 0x88;
    table.entries[IR_SLOT_CHANNEL_UP].address2   = 0x77;
    table.entries[IR_SLOT_CHANNEL_UP].command    = 0x33;
    table.entries[IR_SLOT_CHANNEL_UP].commandInv = 0xCC;

    /* Slot 6: Channel Down */
    table.entries[IR_SLOT_CHANNEL_DOWN].address    = 0x88;
    table.entries[IR_SLOT_CHANNEL_DOWN].address2   = 0x77;
    table.entries[IR_SLOT_CHANNEL_DOWN].command    = 0x34;
    table.entries[IR_SLOT_CHANNEL_DOWN].commandInv = 0xCB;

    /* Slot 7: Guide */
    table.entries[IR_SLOT_GUIDE].address    = 0x88;
    table.entries[IR_SLOT_GUIDE].address2   = 0x77;
    table.entries[IR_SLOT_GUIDE].command    = 0x32;
    table.entries[IR_SLOT_GUIDE].commandInv = 0xCD;

    /* Slot 8: Info */
    table.entries[IR_SLOT_INFO].address    = 0x88;
    table.entries[IR_SLOT_INFO].address2   = 0x77;
    table.entries[IR_SLOT_INFO].command    = 0x29;
    table.entries[IR_SLOT_INFO].commandInv = 0xD6;

    /* Slot 9: Settings */
    table.entries[IR_SLOT_SETTINGS].address    = 0x88;
    table.entries[IR_SLOT_SETTINGS].address2   = 0x77;
    table.entries[IR_SLOT_SETTINGS].command    = 0x0F;
    table.entries[IR_SLOT_SETTINGS].commandInv = 0xF0;

    /* Slot 10: Disabled → IR_TYPE_DISABLED
     * IrFlash_getCodeType() must return IR_TYPE_DISABLED (not IR_TYPE_PROGRAMMED).
     * KeyRouter routes to BLE HID when connected, no-op when disconnected. */
    table.entries[10].address    = 0x00;
    table.entries[10].address2   = 0x00;
    table.entries[10].command    = 0x00;
    table.entries[10].commandInv = 0x00;

    /* Slot 11: Default sentinel → IR_TYPE_DEFAULT
     * IrFlash_getCodeType() must return IR_TYPE_DEFAULT.
     * KeyRouter falls back to hardcoded firmware default for this slot.
     * Only the address byte changes from 0xFF→0x00; other bytes stay 0xFF. */
    table.entries[11].address    = 0x00;
    table.entries[11].address2   = 0xFF;
    table.entries[11].command    = 0xFF;
    table.entries[11].commandInv = 0xFF;

    /* Slots 12-47: left as 0xFF (IR_TYPE_UNPROGRAMMED) from init loop above */

    table.isValid = true;
    IrFlash_writeTable(&table);
}

/**
 * @brief Program a single IR slot as disabled
 *
 * Writes {0x00, 0x00, 0x00, 0x00} - detected as IR_TYPE_DISABLED by
 * IrFlash_getCodeType(). KeyRouter routes to BLE HID (or no-op when
 * disconnected).
 *
 * @param slot  IR slot number (0-47). Use IR_SLOT_* constants.
 */
void program_ir_slot_disabled(uint8_t slot)
{
    IrCodeEntry_t entry = {0x00, 0x00, 0x00, 0x00};

    NVS_init();
    IrFlash_writeEntry(slot, &entry);
}

/**
 * @brief Program a single IR slot with the default sentinel
 *
 * Writes {0x00, 0xFF, 0xFF, 0xFF} - detected as IR_TYPE_DEFAULT by
 * IrFlash_getCodeType(). KeyRouter uses the hardcoded firmware default
 * for this slot.
 *
 * @param slot  IR slot number (0-47). Use IR_SLOT_* constants.
 */
void program_ir_slot_default_sentinel(uint8_t slot)
{
    IrCodeEntry_t entry = {0x00, 0xFF, 0xFF, 0xFF};

    NVS_init();
    IrFlash_writeEntry(slot, &entry);
}

/**
 * @brief Erase the IR code table (reset to firmware defaults)
 *
 * Fills the entire IR table region (0x0140, 192 bytes) with 0xFF.
 * After this, IrFlash_getCodeType() returns IR_TYPE_UNPROGRAMMED for all
 * slots and KeyRouter falls back to hardcoded Samsung defaults (slots 0-9)
 * or BLE HID (slots 10-47).
 *
 * NOTE: This internally uses NVS_write without NVS_WRITE_ERASE so it only
 * succeeds when writing to already-cleared (0xFF) flash. Use after a full
 * erase_nvs_test_data() or if the IR region is known to be unprogrammed.
 */
void erase_ir_table(void)
{
    NVS_init();
    IrFlash_eraseTable();
}

/**
 * @brief Program complete NVS test data for all configuration regions
 *
 * Programs all NVS config regions in sequence. Verify with g_rcConfig
 * after reset:
 *
 *   Region            | Offset | Expected result
 *   ------------------+--------+------------------------------------------
 *   Firmware version  | 0x0000 | g_rcConfig.firmwareVersion.versionStr = "1.2.3"
 *   Device ID         | 0x0020 | g_rcConfig.deviceId.deviceType = G10,
 *                     |        | deviceId.deviceName = "TestRemote"
 *   Wakeup config     | 0x0040 | g_rcConfig.wakeupMask.wakeUpKey1 = 0x0007,
 *                     |        | wakeupMask.wakeUpKey2 = 0xFFFF
 *   Device config     | 0x0080 | g_rcConfig.deviceConfig.txPower = 4,
 *                     |        | deviceConfig.backlightSwitch = 0x01
 *   BLE config        | 0x00A0 | g_rcConfig.bleConfig.rpaSwitch = 0x00,
 *                     |        | bleConfig.bleSwitch = 0xFF,
 *                     |        | bleConfig.cachePowerSwitch = 0xFF
 *   Security config   | 0x00C0 | g_rcConfig.securityConfig.authKey[0] = 0x01
 *   IR table          | 0x0140 | IrFlash_readTable() slots 0-9 = Samsung codes,
 *                     |        | IrFlash_getCodeType() = IR_TYPE_PROGRAMMED
 *
 * USAGE:
 *   erase_nvs_test_data();    // Erase all NVS first (required)
 *   program_full_test_data(); // Program all regions
 *   // Reset device
 *   // Verify g_rcConfig fields via debugger or BLE
 */
void program_full_test_data(void)
{
    program_test_version();
    program_test_device_id();
    program_test_wakeup_config();
    program_test_device_config();
    program_test_ble_config();
    program_test_security_config();
    program_test_ir_table();
}
