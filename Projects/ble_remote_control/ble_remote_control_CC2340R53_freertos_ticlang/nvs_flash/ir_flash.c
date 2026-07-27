/******************************************************************************

@file  ir_flash.c

@brief IR Code Flash Storage Module Implementation - Manages NEC IR code
storage in NVS flash memory. Provides default codes for out-of-box
TV control.

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
#include "ir_flash.h"
#include <string.h>
#include <ti/drivers/NVS.h>
#include <ti_drivers_config.h>

/*********************************************************************
 * LOCAL VARIABLES
 */
static bool initialized = false;

/*********************************************************************
 * DEFAULT IR CODES (Hardcoded - Out of Box Functionality)
 */

/* Default NEC IR codes for Samsung TV control (Address: 0x07) - DEMO Configuration */
static const IrCodeEntry_t defaultIrCodes[NVS_IR_NUM_ENTRIES] = {
    /* Slot 0: Power - Samsung Remote Code */
    {0x07, 0xF8, 0x02, 0xFD},

    /* Slot 1: Volume Up - Samsung Remote Code */
    {0x07, 0xF8, 0x07, 0xF8},

    /* Slot 2: Volume Down - Samsung Remote Code */
    {0x07, 0xF8, 0x0B, 0xF4},

    /* Slot 3: Volume Mute - Samsung Remote Code */
    {0x07, 0xF8, 0x0F, 0xF0},

    /* Slot 4: TV Input */
    {0x88, 0x77, 0x60, 0x9F},

    /* Slot 5: Channel Up */
    {0x88, 0x77, 0x33, 0xCC},

    /* Slot 6: Channel Down */
    {0x88, 0x77, 0x34, 0xCB},

    /* Slot 7: Guide */
    {0x88, 0x77, 0x32, 0xCD},

    /* Slot 8: Info */
    {0x88, 0x77, 0x29, 0xD6},

    /* Slot 9: Settings */
    {0x88, 0x77, 0x0F, 0xF0},

    /* Slots 10-47: No defaults (will send BLE HID) */
    {0xFF, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF}
};

/*********************************************************************
 * ANDROID KEY CODE TO IR SLOT MAPPING
 */

typedef struct {
    uint16_t keyCode;      /* Android key code */
    uint8_t  slot;         /* IR table slot */
} KeyCodeMapping_t;

static const KeyCodeMapping_t keyCodeMap[] = {
    {0x0021, 0},   /* KEYCODE_POWER */
    {0x0023, 1},   /* KEYCODE_VOLUME_UP */
    {0x0024, 2},   /* KEYCODE_VOLUME_DOWN */
    {0x0025, 3},   /* KEYCODE_VOLUME_MUTE */
    {0x0060, 4},   /* KEYCODE_TV_INPUT */
    {0x0033, 5},   /* KEYCODE_CHANNEL_UP */
    {0x0034, 6},   /* KEYCODE_CHANNEL_DOWN */
    {0x0032, 7},   /* KEYCODE_GUIDE */
    {0x0029, 8},   /* KEYCODE_INFO */
    {0x000F, 9},   /* KEYCODE_SETTINGS */
    // Add more mappings as needed for slots 10-39
};

#define KEY_CODE_MAP_SIZE  (sizeof(keyCodeMap) / sizeof(KeyCodeMapping_t))

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/**
 * @brief Check if IR entry is unprogrammed (all 0xFF)
 */
static bool isEntryUnprogrammed(const IrCodeEntry_t *entry)
{
    return (entry->address == IR_FLASH_UNPROGRAMMED &&
            entry->address2 == IR_FLASH_UNPROGRAMMED &&
            entry->command == IR_FLASH_UNPROGRAMMED &&
            entry->commandInv == IR_FLASH_UNPROGRAMMED);
}

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/**
 * @brief Initialize IR flash module
 */
void IrFlash_init(void)
{
    if (!initialized)
    {
        // NVS should already be initialized by DevInfoFlash_init()
        // We just mark ourselves as initialized
        initialized = true;
    }
}

/**
 * @brief Read entire IR code table from NVS
 */
bool IrFlash_readTable(IrCodeTable_t *table)
{
    NVS_Handle nvsHandle;
    int_fast16_t status;
    uint8_t buffer[NVS_IR_TABLE_SIZE];

    if (table == NULL)
    {
        return false;
    }

    // Initialize table structure
    memset(table, 0, sizeof(IrCodeTable_t));

    // Open CONFIG_NVSINTERNAL_STORAGE region
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        // Cannot open NVS
        table->isValid = false;
        return false;
    }

    // Read IR table from NVS offset 0x0040
    status = NVS_read(nvsHandle, NVS_IR_TABLE_OFFSET, buffer, NVS_IR_TABLE_SIZE);
    NVS_close(nvsHandle);

    if (status != NVS_STATUS_SUCCESS)
    {
        // NVS read failed
        table->isValid = false;
        return false;
    }

    // Parse buffer into IR entries
    for (uint8_t i = 0; i < NVS_IR_NUM_ENTRIES; i++)
    {
        uint16_t offset = i * NVS_IR_ENTRY_SIZE;
        table->entries[i].address = buffer[offset + 0];
        table->entries[i].address2 = buffer[offset + 1];
        table->entries[i].command = buffer[offset + 2];
        table->entries[i].commandInv = buffer[offset + 3];
    }

    table->isValid = true;
    return true;
}

/**
 * @brief Read single IR code entry by slot number
 */
bool IrFlash_readEntry(uint8_t slot, IrCodeEntry_t *entry)
{
    NVS_Handle nvsHandle;
    int_fast16_t status;
    uint8_t buffer[NVS_IR_ENTRY_SIZE];
    uint16_t offset;

    if (entry == NULL || slot >= NVS_IR_NUM_ENTRIES)
    {
        return false;
    }

    // Calculate offset for this slot
    offset = NVS_IR_TABLE_OFFSET + (slot * NVS_IR_ENTRY_SIZE);

    // Open NVS
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        return false;
    }

    // Read single entry
    status = NVS_read(nvsHandle, offset, buffer, NVS_IR_ENTRY_SIZE);
    NVS_close(nvsHandle);

    if (status != NVS_STATUS_SUCCESS)
    {
        return false;
    }

    // Parse entry
    entry->address = buffer[0];
    entry->address2 = buffer[1];
    entry->command = buffer[2];
    entry->commandInv = buffer[3];

    return true;
}

/**
 * @brief Write single IR code entry to NVS
 */
bool IrFlash_writeEntry(uint8_t slot, const IrCodeEntry_t *entry)
{
    NVS_Handle nvsHandle;
    int_fast16_t status;
    uint8_t buffer[NVS_IR_ENTRY_SIZE];
    uint16_t offset;

    if (entry == NULL || slot >= NVS_IR_NUM_ENTRIES)
    {
        return false;
    }

    // Prepare buffer
    buffer[0] = entry->address;
    buffer[1] = entry->address2;
    buffer[2] = entry->command;
    buffer[3] = entry->commandInv;

    // Calculate offset
    offset = NVS_IR_TABLE_OFFSET + (slot * NVS_IR_ENTRY_SIZE);

    // Open NVS
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        return false;
    }

    // Write entry
    status = NVS_write(nvsHandle, offset, buffer, NVS_IR_ENTRY_SIZE,
                      NVS_WRITE_POST_VERIFY);
    NVS_close(nvsHandle);

    return (status == NVS_STATUS_SUCCESS);
}

/**
 * @brief Write entire IR code table to NVS
 */
bool IrFlash_writeTable(const IrCodeTable_t *table)
{
    NVS_Handle nvsHandle;
    int_fast16_t status;
    uint8_t buffer[NVS_IR_TABLE_SIZE];

    if (table == NULL)
    {
        return false;
    }

    // Pack entries into buffer
    for (uint8_t i = 0; i < NVS_IR_NUM_ENTRIES; i++)
    {
        uint16_t offset = i * NVS_IR_ENTRY_SIZE;
        buffer[offset + 0] = table->entries[i].address;
        buffer[offset + 1] = table->entries[i].address2;
        buffer[offset + 2] = table->entries[i].command;
        buffer[offset + 3] = table->entries[i].commandInv;
    }

    // Open NVS
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        return false;
    }

    // Write entire table
    status = NVS_write(nvsHandle, NVS_IR_TABLE_OFFSET, buffer, NVS_IR_TABLE_SIZE,
                      NVS_WRITE_POST_VERIFY);
    NVS_close(nvsHandle);

    return (status == NVS_STATUS_SUCCESS);
}

/**
 * @brief Get IR code type based on entry content
 */
IrCodeType_t IrFlash_getCodeType(const IrCodeEntry_t *entry)
{
    if (entry == NULL)
    {
        return IR_TYPE_UNPROGRAMMED;
    }

    // Check for unprogrammed (all 0xFF)
    if (isEntryUnprogrammed(entry))
    {
        return IR_TYPE_UNPROGRAMMED;
    }

    // Rule 0: Disabled entry (0x00 0x00 0x00 0x00) - programmed by Android TV with type=0
    if (entry->address == 0x00 &&
        entry->address2 == 0x00 &&
        entry->command == 0x00 &&
        entry->commandInv == 0x00)
    {
        return IR_TYPE_DISABLED;
    }

    // Rule 1: Explicit default (0x00 0xFF 0xFF 0xFF)
    if (entry->address == 0x00 &&
        entry->address2 == 0xFF &&
        entry->command == 0xFF &&
        entry->commandInv == 0xFF)
    {
        return IR_TYPE_DEFAULT;
    }

    // Otherwise it's a programmed code
    return IR_TYPE_PROGRAMMED;
}

/**
 * @brief Get default IR code for slot
 */
bool IrFlash_getDefaultCode(uint8_t slot, IrCodeEntry_t *entry)
{
    if (entry == NULL || slot >= NVS_IR_NUM_ENTRIES)
    {
        return false;
    }

    // Copy from default table
    memcpy(entry, &defaultIrCodes[slot], sizeof(IrCodeEntry_t));

    // Check if this slot has a default (not all 0xFF)
    return !isEntryUnprogrammed(entry);
}

/**
 * @brief Erase IR code table
 */
bool IrFlash_eraseTable(void)
{
    NVS_Handle nvsHandle;
    int_fast16_t status;
    uint8_t buffer[NVS_IR_TABLE_SIZE];

    // Fill buffer with 0xFF
    memset(buffer, 0xFF, NVS_IR_TABLE_SIZE);

    // Open NVS
    nvsHandle = NVS_open(CONFIG_NVSINTERNAL_STORAGE, NULL);
    if (nvsHandle == NULL)
    {
        return false;
    }

    // Write erased table
    status = NVS_write(nvsHandle, NVS_IR_TABLE_OFFSET, buffer, NVS_IR_TABLE_SIZE,
                      NVS_WRITE_POST_VERIFY);
    NVS_close(nvsHandle);

    return (status == NVS_STATUS_SUCCESS);
}

/**
 * @brief Convert Android key code to IR slot number
 */
uint8_t IrFlash_keyCodeToSlot(uint16_t keyCode)
{
    for (uint8_t i = 0; i < KEY_CODE_MAP_SIZE; i++)
    {
        if (keyCodeMap[i].keyCode == keyCode)
        {
            return keyCodeMap[i].slot;
        }
    }

    // Key code not found
    return 0xFF;
}

/**
 * @brief Convert IR slot number to Android key code
 */
uint16_t IrFlash_slotToKeyCode(uint8_t slot)
{
    for (uint8_t i = 0; i < KEY_CODE_MAP_SIZE; i++)
    {
        if (keyCodeMap[i].slot == slot)
        {
            return keyCodeMap[i].keyCode;
        }
    }

    // Slot not mapped
    return 0x0000;
}
