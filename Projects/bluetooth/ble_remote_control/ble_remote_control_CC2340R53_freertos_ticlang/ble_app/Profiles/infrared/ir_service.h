/******************************************************************************

@file  ir_service.h

@brief This file contains the Audio Audio service definitions and prototypes.

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


#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include <ti/drivers/utils/List.h>
#include "ti/ble/stack_util/icall/app/icall_ble_api.h"

// IR Service callback function
typedef void (*irServiceCB_t)(uint8 event);

/**
 * @brief Add IR Service to GATT database
 * @return SUCCESS or error code
 */
bStatus_t IR_AddService(void);

/**
 * @brief Initialize IR Service (load IR codes from NVS)
 *
 * Call this after IR_AddService() to load IR configuration.
 */
void IR_InitService(void);

/**
 * @brief Check if a key is currently suppressed (should send BLE HID instead of IR)
 *
 * @param slot IR table slot number (0-47)
 * @return true if key is suppressed, false if should send IR
 */
bool IR_IsKeySuppressed(uint8_t slot);

/**
 * @brief Clear IR suppress list (all keys back to IR mode)
 */
void IR_ClearSuppressList(void);

/**
 * @brief Send IR key event notification to Android TV
 *
 * Notifies Android TV when an IR key is pressed or released.
 * Only call this when key is actually sent via IR (not BLE HID).
 *
 * @param slot IR table slot number (0-47)
 * @param isPressed true for KEY_DOWN, false for KEY_UP
 */
void IR_SendKeyEvent(uint8_t slot, bool isPressed);

/**
 * @brief Erase IR table and reset to defaults
 *
 * Called when pairing with new Android TV device to ensure clean slate.
 * Erases IR table from NVS and reloads cache with defaults.
 */
void IR_ResetTable(void);
