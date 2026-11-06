/******************************************************************************************

@file key_router.c
@brief Key Press Routing Module Implementation
 
Implements intelligent routing between IR and BLE HID based on
programmed IR codes and suppress list.

*******************************************************************************************

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
#include "key_router.h"
#include <nvs_flash/ir_flash.h>
#include <ble_app/Profiles/infrared/ir_service.h>
#include <infrared/IR.h>
#include <ble_hid/hiddev.h>
#include <unistd.h>

/*********************************************************************
 * EXTERNAL VARIABLES
 */
extern PDEIRLPF3_Handle pdeIrHandle;
extern PDEIRLPF3_Params IR_params;

/*********************************************************************
 * LOCAL VARIABLES
 */
static bool initialized = false;
static PDEIRGENLPF3_Transaction pdeIrTrans;

/* CACHED IR TABLE (avoids slow NVS reads on every key press) */
static IrCodeTable_t cachedIrTable;
static bool cacheValid = false;

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/**
 * @brief Convert Android key code to BLE HID key code
 *
 * Android TV uses Android keycodes for IR programming, but BLE HID
 * uses HID Consumer Control codes. This function maps between them.
 */
static uint16_t androidKeyToHidKey(uint16_t androidKey)
{
    // Map IR keycodes (from IR.h) to BLE HID keycodes (from hiddev.h)
    switch (androidKey)
    {
        // TV Control Keys
        case 0x21: return 0x0030;  // IR_KEYCODE_POWER -> KEYCODE_POWER
        case 0x23: return 0x00E9;  // IR_KEYCODE_VOLUME_UP -> KEYCODE_VOLUME_UP
        case 0x24: return 0x00EA;  // IR_KEYCODE_VOLUME_DOWN -> KEYCODE_VOLUME_DOWN
        case 0x25: return 0x00E2;  // IR_KEYCODE_VOLUME_MUTE -> KEYCODE_VOLUME_MUTE
        case 0x60: return 0x01BB;  // IR_KEYCODE_TV_INPUT -> KEYCODE_TV_INPUT
        case 0x33: return 0x009C;  // IR_KEYCODE_CHANNEL_UP -> KEYCODE_CHANNEL_UP
        case 0x34: return 0x009D;  // IR_KEYCODE_CHANNEL_DOWN -> KEYCODE_CHANNEL_DOWN
        case 0x32: return 0x008D;  // IR_KEYCODE_GUIDE -> KEYCODE_GUIDE
        case 0x0F: return 0x008D;  // IR_KEYCODE_SETTINGS -> KEYCODE_GUIDE (map to guide)
        case 0x59: return 0x019C;  // IR_KEYCODE_PROFILE_SWITCH -> KEYCODE_PROFILE_SWITCH

        // Navigation Keys
        case 0x48: return 0x0224;  // IR_KEYCODE_BACK -> KEYCODE_BACK
        case 0x47: return 0x0223;  // IR_KEYCODE_HOME -> KEYCODE_HOME
        case 0x15: return 0x0042;  // IR_KEYCODE_DPAD_UP -> KEYCODE_DPAD_UP
        case 0x16: return 0x0043;  // IR_KEYCODE_DPAD_DOWN -> KEYCODE_DAPD_DOWN (typo in original)
        case 0x17: return 0x0044;  // IR_KEYCODE_DPAD_LEFT -> KEYCODE_DPAD_LEFT
        case 0x18: return 0x0045;  // IR_KEYCODE_DPAD_RIGHT -> KEYCODE_DPAD_RIGHT
        case 0x19: return 0x0041;  // IR_KEYCODE_DPAD_CENTER -> KEYCODE_DPAD_CENTER

        // App Shortcut Keys
        case 0x63: return 0x0077;  // IR_KEYCODE_F17 (YouTube) -> KEYCODE_F17
        case 0x64: return 0x0078;  // IR_KEYCODE_F18 (Netflix) -> KEYCODE_F18
        case 0x67: return 0x0079;  // IR_KEYCODE_F19 (App03) -> KEYCODE_F19
        case 0x68: return 0x007A;  // IR_KEYCODE_F20 (App04) -> KEYCODE_F20

        default: return androidKey;  // Pass through if no mapping
    }
}

/* Prepare BLE HID Report to be send */
static void HIDAdvRemote_sendReport(uint16_t keycode)
{

  uint8_t consumer_key[4];
  consumer_key[0] = LO_UINT16(keycode);
  consumer_key[1] = HI_UINT16(keycode);
  consumer_key[2] = 0x00;
  consumer_key[3] = 0x00;
  HidDev_Report(HID_RPT_ID_CC_IN, HID_REPORT_TYPE_INPUT, 4, consumer_key);
}

/**
 * @brief Send IR code via IR transmitter
 */
static void sendIrCode(IrCodeEntry_t *entry, uint8_t slot)
{
    if (entry == NULL)
    {
        return;
    }

    // Notify Android TV about KEY_DOWN event
    extern void IR_SendKeyEvent(uint8_t slot, bool isPressed);
    IR_SendKeyEvent(slot, true);  // KEY_DOWN

    // Set up IR transaction (NEC format)
    pdeIrTrans.customCode = entry->address;
    pdeIrTrans.dataCode = entry->command;

    // Open IR driver
    pdeIrHandle = PDEIRLPF3_open(PDEIRLPF3_CONFIG_0, &IR_params);

    // Transmit IR code
    PDEIRLPF3_transfer(pdeIrHandle, &pdeIrTrans);

    // Wait for transmission to complete (125ms typical for NEC)
    usleep(125 * 1000);

    // Stop and close
    PDEIRLPF3_stop(pdeIrHandle);
    PDEIRLPF3_close(pdeIrHandle);

    // Notify Android TV about KEY_UP event
    IR_SendKeyEvent(slot, false);  // KEY_UP
}

/**
 * @brief Send BLE HID report
 */
static void sendBleHid(uint16_t androidKeyCode)
{
    // Convert Android keycode to HID consumer control code
    uint16_t hidKeyCode = androidKeyToHidKey(androidKeyCode);

    // Send HID report with proper keycode
    HIDAdvRemote_sendReport(hidKeyCode);
}

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/**
 * @brief Initialize key router
 */
void KeyRouter_init(void)
{
    if (!initialized)
    {
        // IR flash should already be initialized by IR service

        // Load IR table into RAM cache (one-time read at startup)
        if (IrFlash_readTable(&cachedIrTable))
        {
            cacheValid = true;
        }
        else
        {
            // Flash read failed, cache will be invalid
            // Key presses will use defaults
            cacheValid = false;
        }

        initialized = true;
    }
}

/**
 * @brief Handle key press with routing logic
 *
 * ROUTING FLOW (Google ATV Remote Control Spec):
 * ================================================
 *
 * WHEN NOT CONNECTED (Out-of-Box):
 * ---------------------------------
 * 1. User presses Power button
 * 2. Check if key has IR code (default or programmed)
 * 3. If YES → Send IR to TV
 * 4. If NO → Ignore (no BLE connection)
 *
 * WHEN CONNECTED (After Pairing):
 * --------------------------------
 * SCENARIO A: Key has IR code, NOT suppressed
 *   1. User presses Volume Up
 *   2. Check suppress list → NOT suppressed
 *   3. Check IR table → HAS IR code (Samsung TV)
 *   4. → Send IR directly to TV (TV volume changes)
 *   5. → Send IR_KEY_EVENT notification to Android TV
 *   6. Android TV does nothing (TV already handled it)
 *
 * SCENARIO B: Key has IR code, IS suppressed (HDMI-CEC active)
 *   1. User presses Volume Up
 *   2. Check suppress list → IS suppressed
 *   3. → Send BLE HID to Android TV (NOT IR!)
 *   4. Android TV controls TV volume via HDMI-CEC
 *
 * SCENARIO C: Key has NO IR code (navigation keys)
 *   1. User presses D-Pad Up
 *   2. Check IR table → NO mapping
 *   3. → Send BLE HID to Android TV
 *   4. Android TV handles navigation
 *
 * KEY PRINCIPLE: Only ONE action per key press
 * - Either IR (to TV directly)
 * - OR BLE HID (to Android TV)
 * - NEVER BOTH!
 *
 * @param androidKeyCode Android key code (e.g., 0x21 for Power)
 * @param isConnected true if BLE connected to Android TV
 * @return true if key was handled, false otherwise
 */
bool KeyRouter_handleKeyPress(uint16_t androidKeyCode, bool isConnected)
{
    // Step 1: Map Android key code to IR slot
    uint8_t slot = IrFlash_keyCodeToSlot(androidKeyCode);

    if (slot == 0xFF)
    {
        // Key not mappable to IR (e.g., navigation keys like D-Pad, Home, Back)
        // These keys always send BLE HID when connected
        if (isConnected)
        {
            sendBleHid(androidKeyCode);
            return true;
        }
        // Not connected and no IR mapping → ignore
        return false;
    }

    // Step 2: If connected, check suppress list FIRST
    // (This is how Android TV disables IR when HDMI-CEC is available)
    if (isConnected && IR_IsKeySuppressed(slot))
    {
        // Key is suppressed → send BLE HID instead of IR
        // Use case: TV supports HDMI-CEC, Android TV wants to control via CEC
        sendBleHid(androidKeyCode);
        return true;
    }

    // Step 3: Read IR code from RAM cache (fast!)
    if (!cacheValid || slot >= NVS_IR_NUM_ENTRIES)
    {
        // Cache invalid or slot out of range
        // Fallback: send BLE HID if connected, ignore otherwise
        if (isConnected)
        {
            sendBleHid(androidKeyCode);
            return true;
        }
        return false;
    }

    IrCodeEntry_t entry = cachedIrTable.entries[slot];

    // Step 4: Determine code type and route accordingly
    IrCodeType_t codeType = IrFlash_getCodeType(&entry);

    switch (codeType)
    {
        case IR_TYPE_UNPROGRAMMED:
        case IR_TYPE_DEFAULT:
            // Flash unprogrammed or explicitly set to default
            // Try to get default IR code
            if (IrFlash_getDefaultCode(slot, &entry))
            {
                // Send IR code (works both connected and disconnected)
                // When connected: Android TV gets notified via IR_SendKeyEvent
                sendIrCode(&entry, slot);
                return true;
            }
            else
            {
                // No default code available for this key
                // Fallback: send BLE HID if connected
                if (isConnected)
                {
                    sendBleHid(androidKeyCode);
                    return true;
                }
                return false;
            }

        case IR_TYPE_PROGRAMMED:
            // Android TV programmed a custom IR code (e.g., Samsung TV codes)
            // Send IR directly to TV
            // When connected: Android TV gets notified via IR_SendKeyEvent
            sendIrCode(&entry, slot);
            return true;

        case IR_TYPE_DISABLED:
            // Key explicitly disabled in IR table
            // Fallback: send BLE HID if connected
            if (isConnected)
            {
                sendBleHid(androidKeyCode);
                return true;
            }
            return false;

        default:
            // Unknown code type
            if (isConnected)
            {
                sendBleHid(androidKeyCode);
            }
            return false;
    }
}

/**
 * @brief Handle key release
 *
 * For now, key release handling is minimal since:
 * - IR codes are sent once on press (no repeat in basic mode)
 * - BLE HID release is handled by sending KEY_NONE separately
 */
bool KeyRouter_handleKeyRelease(uint16_t androidKeyCode, bool isConnected)
{
    // Check if this key would have sent BLE HID
    uint8_t slot = IrFlash_keyCodeToSlot(androidKeyCode);

    if (slot == 0xFF)
    {
        // Key not mappable to IR
        return isConnected;  // Release only matters if connected
    }

    if (isConnected && IR_IsKeySuppressed(slot))
    {
        // Key was suppressed, BLE HID was sent
        return true;
    }

    // Check if key has IR code (use cache)
    if (cacheValid && slot < NVS_IR_NUM_ENTRIES)
    {
        IrCodeEntry_t entry = cachedIrTable.entries[slot];
        IrCodeType_t codeType = IrFlash_getCodeType(&entry);

        // If IR was sent, no BLE release needed
        if (codeType == IR_TYPE_PROGRAMMED ||
            codeType == IR_TYPE_DEFAULT ||
            codeType == IR_TYPE_UNPROGRAMMED)
        {
            return true;
        }
    }

    // Otherwise, BLE HID release handling happens elsewhere
    return isConnected;
}

/**
 * @brief Reload IR table cache from NVS
 *
 * Call this after Android TV programs new IR codes to refresh the cache.
 * This is the ONLY time we read from flash after initialization.
 */
void KeyRouter_reloadCache(void)
{
    if (IrFlash_readTable(&cachedIrTable))
    {
        cacheValid = true;
    }
    else
    {
        cacheValid = false;
    }
}

/**
 * @brief Send special combo key IR code
 *
 * This function sends special IR codes for combo key actions per Google ATV spec:
 * - Bug Report Mode: 0x96 (DPAD CENTER + BACK for >1s)
 * - Accessibility Mode: 0xD0 (DPAD DOWN + BACK for >1s)
 *
 * These codes use the default NEC address (0x88) with the special command byte.
 *
 * NEC IR Format:
 * - Byte 0: Address (0x88 - default NEC address)
 * - Byte 1: Address inverse (0x77 = ~0x88)
 * - Byte 2: Command (0x96 or 0xD0 - "code data" per spec)
 * - Byte 3: Command inverse (~0x96 or ~0xD0)
 *
 * @param commandCode IR command code (0x96 for bug report, 0xD0 for accessibility)
 */
void KeyRouter_sendComboIrCode(uint8_t commandCode)
{
    // Create IR code entry with default address and special command
    IrCodeEntry_t comboEntry;
    comboEntry.address = IR_DEFAULT_NEC_ADDR;     // 0x88 - default NEC address
    comboEntry.address2 = ~IR_DEFAULT_NEC_ADDR;   // 0x77 - address inverse
    comboEntry.command = commandCode;              // 0x96 or 0xD0
    comboEntry.commandInv = ~commandCode;          // Command inverse (0x69 or 0x2F)

    // Set up IR transaction (NEC format)
    pdeIrTrans.customCode = comboEntry.address;
    pdeIrTrans.dataCode = comboEntry.command;

    // Open IR driver
    pdeIrHandle = PDEIRLPF3_open(PDEIRLPF3_CONFIG_0, &IR_params);

    // Transmit IR code
    PDEIRLPF3_transfer(pdeIrHandle, &pdeIrTrans);

    // Wait for transmission to complete (125ms typical for NEC)
    usleep(125 * 1000);

    // Stop and close
    PDEIRLPF3_stop(pdeIrHandle);
    PDEIRLPF3_close(pdeIrHandle);

    // Note: We don't call IR_SendKeyEvent() for combo keys
    // These are special codes that don't correspond to regular key slots
}
