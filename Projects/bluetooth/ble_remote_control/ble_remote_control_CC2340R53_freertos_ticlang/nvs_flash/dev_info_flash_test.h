/******************************************************************************

@file  dev_info_flash_test.h

@brief Test function declarations for NVS programming

       DEVELOPMENT ONLY: Include this header only during testing.

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

#ifndef DEV_INFO_FLASH_TEST_H
#define DEV_INFO_FLASH_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Program test firmware version to NVS
 */
void program_test_version(void);

/**
 * @brief Program test device ID to NVS
 */
void program_test_device_id(void);

/**
 * @brief Program complete test data (version + device ID)
 *
 * Programs:
 * - Version: "1.2.3"
 * - Device: G10 (Hobb short)
 * - Name: "TestRemote"
 */
void program_nvs_test_data(void);

/**
 * @brief Program G20 device with custom name
 *
 * Programs:
 * - Version: "1.0.0"
 * - Device: G20 (Shaw long)
 * - Name: "RemoteB046"
 */
void program_g20_test_data(void);

/**
 * @brief Erase NVS data (test fallback behavior)
 *
 * WARNING: Erases entire NVS region
 */
void erase_nvs_test_data(void);

/* -----------------------------------------------------------------------
 * Extended config region tests (0x0040 - 0x0140)
 * ----------------------------------------------------------------------- */

/**
 * @brief Program wakeup key configuration to NVS (offset 0x0040)
 *
 * Programs wakeUpKey1=0x0007 (Assistant/Home/Power), wakeUpKey2=0xFFFF (disabled).
 */
void program_test_wakeup_config(void);

/**
 * @brief Program device hardware configuration to NVS (offset 0x0080)
 *
 * Programs: couchModeSwitch=0xFF, txPower=4dBm, backlightSwitch=enabled.
 */
void program_test_device_config(void);

/**
 * @brief Program BLE configuration to NVS (offset 0x00A0)
 *
 * Programs: periodicWakeup=disabled, rpaSwitch=enabled, bleSwitch=enabled,
 *           cachePowerSwitch=enabled.
 */
void program_test_ble_config(void);

/**
 * @brief Program security configuration to NVS (offset 0x00C0)
 *
 * Programs test key patterns: authKey=0x01..0x10, encryptionPass=0x11..0x20,
 * ecdsaKey=0x01..0x40.
 */
void program_test_security_config(void);

/* -----------------------------------------------------------------------
 * IR table tests (offset 0x0140, 192 bytes, 48 slots)
 * ----------------------------------------------------------------------- */

/**
 * @brief Program IR table covering all four IrCodeType_t paths
 *
 * Slot layout:
 *   Slots  0-9:  Samsung codes  → IR_TYPE_PROGRAMMED
 *   Slot  10:    {0x00,0x00,0x00,0x00}  → IR_TYPE_DISABLED
 *   Slot  11:    {0x00,0xFF,0xFF,0xFF}  → IR_TYPE_DEFAULT
 *   Slots 12-47: {0xFF,0xFF,0xFF,0xFF}  → IR_TYPE_UNPROGRAMMED
 *
 * Requires erase_nvs_test_data() or erase_ir_table() to be called first.
 */
void program_test_ir_table(void);

/**
 * @brief Program a single IR slot as disabled
 *
 * Writes {0x00, 0x00, 0x00, 0x00} → IrFlash_getCodeType() returns IR_TYPE_DISABLED.
 * KeyRouter routes to BLE HID (connected) or no-op (disconnected).
 *
 * @param slot  IR slot number (0-47). Use IR_SLOT_* constants from ir_flash.h.
 */
void program_ir_slot_disabled(uint8_t slot);

/**
 * @brief Program a single IR slot with the default sentinel
 *
 * Writes {0x00, 0xFF, 0xFF, 0xFF} → IrFlash_getCodeType() returns IR_TYPE_DEFAULT.
 * KeyRouter uses the hardcoded firmware default for this key.
 *
 * @param slot  IR slot number (0-47). Use IR_SLOT_* constants from ir_flash.h.
 */
void program_ir_slot_default_sentinel(uint8_t slot);

/**
 * @brief Erase the IR code table (reset to firmware defaults)
 *
 * Fills IR region (0x0140, 192 bytes) with 0xFF. Only valid when flash is
 * already cleared; use after erase_nvs_test_data() for reliable operation.
 */
void erase_ir_table(void);

/* -----------------------------------------------------------------------
 * Master test function
 * ----------------------------------------------------------------------- */

/**
 * @brief Program complete NVS test data for all configuration regions
 *
 * Programs all regions: version, device ID, wakeup, device config, BLE config,
 * security config, and IR table (Samsung defaults for slots 0-9).
 *
 * USAGE:
 *   erase_nvs_test_data();    // Must be called first
 *   program_full_test_data(); // Programs all regions
 *   // Reset device, verify g_rcConfig fields
 */
void program_full_test_data(void);

#ifdef __cplusplus
}
#endif

#endif /* DEV_INFO_FLASH_TEST_H */
