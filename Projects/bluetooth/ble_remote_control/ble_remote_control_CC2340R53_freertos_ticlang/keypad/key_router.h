/******************************************************************************************

@file key_router.h
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

#ifndef KEY_ROUTER_H
#define KEY_ROUTER_H

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
 * API FUNCTIONS
 */

/**
 * @brief Initialize key router module
 *
 * Call once at startup after IR service is initialized.
 */
void KeyRouter_init(void);

/**
 * @brief Handle key press - routes to IR or BLE HID
 *
 * Routing logic:
 * 1. Map Android key code to IR slot
 * 2. Check if key is suppressed → BLE HID
 * 3. Check if key has IR code (programmed or default) → IR
 * 4. Otherwise → BLE HID
 *
 * @param androidKeyCode Android key code (e.g., 0x0021 for Power)
 * @param isConnected true if BLE connected, false otherwise
 * @return true if key was handled, false otherwise
 */
bool KeyRouter_handleKeyPress(uint16_t androidKeyCode, bool isConnected);

/**
 * @brief Handle key release - routes to IR or BLE HID
 *
 * @param androidKeyCode Android key code
 * @param isConnected true if BLE connected
 * @return true if key was handled, false otherwise
 */
bool KeyRouter_handleKeyRelease(uint16_t androidKeyCode, bool isConnected);

/**
 * @brief Reload IR table cache from NVS
 *
 * Call this after Android TV programs new IR codes via BLE.
 * This is the only flash read after initialization - keeps key
 * press handling fast by using RAM cache.
 */
void KeyRouter_reloadCache(void);

/**
 * @brief Send special combo key IR code
 *
 * Sends special IR codes for combo key actions per Google ATV spec:
 * - Bug Report Mode: 0x96 (DPAD CENTER + BACK for >1s)
 * - Accessibility Mode: 0xD0 (DPAD DOWN + BACK for >1s)
 *
 * These codes use the default NEC address (0x88) with the special command byte.
 *
 * @param commandCode IR command code (0x96 for bug report, 0xD0 for accessibility)
 */
void KeyRouter_sendComboIrCode(uint8_t commandCode);

#ifdef __cplusplus
}
#endif

#endif /* KEY_ROUTER_H */
