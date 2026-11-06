/******************************************************************************

@file  ir_flash.h

@brief IR Code Flash Storage Module - Manages NEC IR code override table
storage in NVS flash memory. Supports programming IR codes via BLE
and provides default codes for out-of-box use.

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

#ifndef IR_FLASH_H
#define IR_FLASH_H

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

/* NVS Region Configuration (shares CONFIG_NVSINTERNAL_STORAGE with device info) */
#define NVS_IR_TABLE_OFFSET       0x0140   /* Offset within NVS for IR table */
#define NVS_IR_TABLE_SIZE         192      /* Total size: 48 entries × 4 bytes */
#define NVS_IR_NUM_ENTRIES        48       /* Number of IR code slots */
#define NVS_IR_ENTRY_SIZE         4        /* Bytes per entry */

/* IR Code Entry Format (4 bytes per key) */
#define IR_ENTRY_ADDR_OFFSET      0        /* NEC Address byte */
#define IR_ENTRY_ADDR2_OFFSET     1        /* Address byte 2 or inverse */
#define IR_ENTRY_CMD_OFFSET       2        /* NEC Command byte */
#define IR_ENTRY_CMDINV_OFFSET    3        /* Command inverse or custom */

/* Special Values */
#define IR_FLASH_UNPROGRAMMED     0xFF     /* Unprogrammed flash byte */
#define IR_DEFAULT_NEC_ADDR       0x88     /* Default NEC address for generic TV */

/* Android Key Code to IR Slot Mapping (40 programmable keys) */
#define IR_SLOT_POWER             0        /* KEYCODE_POWER (0x0021) */
#define IR_SLOT_VOLUME_UP         1        /* KEYCODE_VOLUME_UP (0x0023) */
#define IR_SLOT_VOLUME_DOWN       2        /* KEYCODE_VOLUME_DOWN (0x0024) */
#define IR_SLOT_VOLUME_MUTE       3        /* KEYCODE_VOLUME_MUTE (0x0025) */
#define IR_SLOT_TV_INPUT          4        /* KEYCODE_TV_INPUT (0x0060) */
#define IR_SLOT_CHANNEL_UP        5        /* KEYCODE_CHANNEL_UP (0x0033) */
#define IR_SLOT_CHANNEL_DOWN      6        /* KEYCODE_CHANNEL_DOWN (0x0034) */
#define IR_SLOT_GUIDE             7        /* KEYCODE_GUIDE (0x0032) */
#define IR_SLOT_INFO              8        /* KEYCODE_INFO (0x0029) */
#define IR_SLOT_SETTINGS          9        /* KEYCODE_SETTINGS (0x000F) */
// Slots 10-39: Additional programmable keys
// Slots 40-47: Reserved for future use

/*********************************************************************
 * TYPEDEFS
 */

/**
 * IR code entry (4 bytes - NEC format)
 */
typedef struct {
    uint8_t address;       /* NEC address byte */
    uint8_t address2;      /* Address byte 2 or address inverse */
    uint8_t command;       /* NEC command byte */
    uint8_t commandInv;    /* Command inverse or custom byte */
} IrCodeEntry_t;

/**
 * IR code table (48 entries)
 */
typedef struct {
    IrCodeEntry_t entries[NVS_IR_NUM_ENTRIES];
    bool isValid;          /* True if table read successfully */
} IrCodeTable_t;

/**
 * IR code type based on flash content
 */
typedef enum {
    IR_TYPE_UNPROGRAMMED,  /* All 0xFF - use defaults */
    IR_TYPE_DEFAULT,       /* 0x00 0xFF 0xFF 0xFF - explicit default */
    IR_TYPE_PROGRAMMED,    /* Custom IR code */
    IR_TYPE_DISABLED       /* No IR for this key */
} IrCodeType_t;

/*********************************************************************
 * PRODUCTION PROGRAMMING GUIDE
 *********************************************************************
 *
 * IR CODE OVERRIDE TABLE - NVS Region
 *
 * Location: -CONFIG_NVSINTERNAL_STORAGE region at offset 0x0040
 * Size: 192 bytes (48 slots × 4 bytes)
 * Format: NEC IR codes
 *
 * ===================================================================
 * MEMORY LAYOUT
 * ===================================================================
 *
 * Offset | Slot | Android Key Code | Default Key
 * -------+------+------------------+------------------------
 * 0x0040 |  0   | 0x0021           | Power
 * 0x0044 |  1   | 0x0023           | Volume Up
 * 0x0048 |  2   | 0x0024           | Volume Down
 * 0x004C |  3   | 0x0025           | Volume Mute
 * 0x0050 |  4   | 0x0060           | TV Input
 * 0x0054 |  5   | 0x0033           | Channel Up
 * 0x0058 |  6   | 0x0034           | Channel Down
 * 0x005C |  7   | 0x0032           | Guide
 * 0x0060 |  8   | 0x0029           | Info
 * 0x0064 |  9   | 0x000F           | Settings
 * ...    | ...  | ...              | ...
 * 0x00EC | 47   | Reserved         | Reserved
 *
 * ===================================================================
 * NEC IR CODE FORMAT (4 bytes per entry)
 * ===================================================================
 *
 * Byte 0: Address        (e.g., 0x88 for generic NEC TV)
 * Byte 1: Address Inv    (e.g., 0x77 = ~0x88)
 * Byte 2: Command        (e.g., 0x21 for Power)
 * Byte 3: Command Inv    (e.g., 0xDE = ~0x21)
 *
 * Example: Power button for generic NEC TV
 * ┌──────┬──────┬──────┬──────┐
 * │ 0x88 │ 0x77 │ 0x21 │ 0xDE │
 * └──────┴──────┴──────┴──────┘
 *
 * ===================================================================
 * SPECIAL VALUES (Google Spec Table 3.4)
 * ===================================================================
 *
 * Rule 0: Use firmware defaults
 * ┌──────┬──────┬──────┬──────┐
 * │ 0x00 │ 0xFF │ 0xFF │ 0xFF │
 * └──────┴──────┴──────┴──────┘
 * Result: 0x88 ~0x88 default_cmd ~default_cmd
 *
 * Rule 1: Override command only (keep default address 0x88)
 * ┌──────┬──────┬──────┬──────┐
 * │ 0xFF │ 0xFF │ 0x21 │ 0xFF │
 * └──────┴──────┴──────┴──────┘
 * Result: 0x88 ~0x88 0x21 ~0x21
 *
 * Rule 2: Override address and command
 * ┌──────┬──────┬──────┬──────┐
 * │ 0x07 │ 0xFF │ 0x02 │ 0xFF │
 * └──────┴──────┴──────┴──────┘
 * Result: 0x07 ~0x07 0x02 ~0x02
 *
 * Rule 3: Full custom format
 * ┌──────┬──────┬──────┬──────┐
 * │ addr │ addr2│ cmd  │ 0xFF │
 * └──────┴──────┴──────┴──────┘
 * Result: addr addr2 cmd ~cmd
 *
 * Unprogrammed (all 0xFF): Use firmware defaults
 * ┌──────┬──────┬──────┬──────┐
 * │ 0xFF │ 0xFF │ 0xFF │ 0xFF │
 * └──────┴──────┴──────┴──────┘
 * Result: Use hardcoded default IR codes
 *
 * ===================================================================
 * DEFAULT IR CODES (Built into Firmware)
 * ===================================================================
 *
 * When NVS is unprogrammed, these codes are used:
 *
 * Power:       0x88 0x77 0x21 0xDE
 * Volume Up:   0x88 0x77 0x23 0xDC
 * Volume Down: 0x88 0x77 0x24 0xDB
 * Mute:        0x88 0x77 0x25 0xDA
 * TV Input:    0x88 0x77 0x60 0x9F
 * Channel Up:  0x88 0x77 0x33 0xCC
 * Channel Down:0x88 0x77 0x34 0xCB
 *
 * These provide basic TV control out of the box.
 *
 * ===================================================================
 * PROGRAMMING METHODS
 * ===================================================================
 *
 * METHOD 1: Via BLE (Runtime - Android TV Setup)
 * --------------------------------------------------------------------
 * Android TV programs IR codes during pairing/setup:
 * 1. User selects TV brand in Android TV settings
 * 2. Android TV writes to IR GATT Service characteristics
 * 3. Remote stores codes in NVS at offset 0x0040
 * 4. Codes persist across reboots
 *
 * METHOD 2: Via NVS API (Development/Testing)
 * --------------------------------------------------------------------
 * Use test functions in ir_flash_test.c (see that file for examples)
 *
 * METHOD 3: Via Uniflash (Production)
 * --------------------------------------------------------------------
 * Program entire NVS region including IR table:
 *   $ uniflash.sh -ccxml CC2340R5.ccxml \
 *       -program nvs_data.bin -offset 0x40000
 *
 * Binary format:
 *   [0x0000-0x0013]: Version string (20 bytes)
 *   [0x0020-0x003F]: Device ID (32 bytes)
 *   [0x0040-0x00EF]: IR table (192 bytes) ← IR codes here
 *
 * ===================================================================
 * VERIFICATION
 * ===================================================================
 *
 * 1. Check NVS contents via CCS Memory Browser:
 *    - Address 0x40040: Should show IR codes (not all 0xFF)
 *    - First entry (Power): Should be 0x88 0x77 0x21 0xDE or custom
 *
 * 2. Test with IR receiver:
 *    - Press Power button
 *    - Verify correct NEC code transmitted
 *    - Check via oscilloscope or IR decoder
 *
 * 3. Test Android TV programming:
 *    - Pair with Android TV
 *    - Follow TV brand setup
 *    - Verify IR codes work with actual TV
 *
 *********************************************************************/

/*********************************************************************
 * API FUNCTIONS
 */

/**
 * @brief Initialize IR flash module
 *
 * Must be called once at startup.
 * Note: NVS_init() should already be called by DevInfoFlash_init()
 */
void IrFlash_init(void);

/**
 * @brief Read entire IR code table from NVS
 *
 * Reads all 48 IR code entries from NVS offset 0x0040.
 * If NVS is unprogrammed, table->isValid will be false.
 *
 * @param table Pointer to structure to fill with IR codes
 * @return true if table read successfully, false on error
 */
bool IrFlash_readTable(IrCodeTable_t *table);

/**
 * @brief Read single IR code entry by slot number
 *
 * @param slot IR slot number (0-47)
 * @param entry Pointer to structure to fill with IR code
 * @return true if entry read successfully, false on error
 */
bool IrFlash_readEntry(uint8_t slot, IrCodeEntry_t *entry);

/**
 * @brief Write single IR code entry to NVS
 *
 * Programs one IR code entry to NVS at specified slot.
 *
 * @param slot IR slot number (0-47)
 * @param entry Pointer to IR code entry to write
 * @return true if write successful, false on error
 */
bool IrFlash_writeEntry(uint8_t slot, const IrCodeEntry_t *entry);

/**
 * @brief Write entire IR code table to NVS
 *
 * Programs all 48 IR code entries to NVS in one operation.
 *
 * @param table Pointer to IR code table to write
 * @return true if write successful, false on error
 */
bool IrFlash_writeTable(const IrCodeTable_t *table);

/**
 * @brief Get IR code type (unprogrammed, default, programmed, disabled)
 *
 * Determines type based on entry content per Google spec Table 3.4.
 *
 * @param entry Pointer to IR code entry
 * @return IR code type enumeration
 */
IrCodeType_t IrFlash_getCodeType(const IrCodeEntry_t *entry);

/**
 * @brief Get default IR code for slot
 *
 * Returns hardcoded firmware default IR code for given slot.
 * Used when NVS is unprogrammed or entry type is IR_TYPE_DEFAULT.
 *
 * @param slot IR slot number (0-47)
 * @param entry Pointer to structure to fill with default code
 * @return true if slot has a default code, false if no default
 */
bool IrFlash_getDefaultCode(uint8_t slot, IrCodeEntry_t *entry);

/**
 * @brief Erase IR code table (reset to unprogrammed state)
 *
 * Fills entire IR table region with 0xFF.
 * After this, remote will use firmware defaults.
 *
 * @return true if erase successful, false on error
 */
bool IrFlash_eraseTable(void);

/**
 * @brief Convert Android key code to IR slot number
 *
 * Maps Android key codes to IR table slots.
 *
 * @param keyCode Android key code (e.g., 0x0021 for Power)
 * @return IR slot number (0-47), or 0xFF if key not mappable
 */
uint8_t IrFlash_keyCodeToSlot(uint16_t keyCode);

/**
 * @brief Convert IR slot number to Android key code
 *
 * @param slot IR slot number (0-47)
 * @return Android key code, or 0x0000 if slot not mapped
 */
uint16_t IrFlash_slotToKeyCode(uint8_t slot);

#ifdef __cplusplus
}
#endif

#endif /* IR_FLASH_H */
