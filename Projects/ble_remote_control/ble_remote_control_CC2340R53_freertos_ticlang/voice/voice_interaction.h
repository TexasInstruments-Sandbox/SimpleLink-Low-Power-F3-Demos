/******************************************************************************

@file  voice_interaction.h

@brief Voice interaction model state machine for Google Voice over BLE spec v1.1
       Handles On-request, Press-to-Talk (PTT), and Hold-to-Talk (HTT) models

Group: WCS, BTS
Target Device: cc23xx

******************************************************************************

Copyright (c) 2022-2024, Texas Instruments Incorporated
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

Copyright (c) 2022-2025, Texas Instruments Incorporated
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

#ifndef VOICE_INTERACTION_H
#define VOICE_INTERACTION_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 *********************************************************************/
#include <stdint.h>
#include <stdbool.h>

/*********************************************************************
 * CONSTANTS
 *********************************************************************/

// Voice interaction states
typedef enum {
    VOICE_STATE_IDLE = 0,           // No audio session active
    VOICE_STATE_ON_REQUEST,         // On-request model active (MIC_OPEN)
    VOICE_STATE_PTT,                // Press-to-Talk active
    VOICE_STATE_HTT                 // Hold-to-Talk active
} VoiceState_t;

// Voice interaction model type
typedef enum {
    VOICE_MODEL_ON_REQUEST = 0x00,  // Host-triggered (MIC_OPEN)
    VOICE_MODEL_PTT = 0x01,         // Press-to-Talk
    VOICE_MODEL_HTT = 0x03          // Hold-to-Talk
} VoiceModel_t;

/*********************************************************************
 * TYPEDEFS
 *********************************************************************/

// Voice session info
typedef struct {
    VoiceState_t state;             // Current state
    VoiceModel_t model;             // Current interaction model
    uint8_t streamId;               // Current stream ID (0x01-0x80, rolls over)
    uint32_t startTime;             // Session start time (milliseconds)
    uint32_t lastInteractionTime;   // Last user interaction time
    bool isActive;                  // Is audio currently streaming
} VoiceSession_t;

/*********************************************************************
 * API FUNCTIONS
 *********************************************************************/

/*********************************************************************
 * @fn      VoiceInteraction_init
 *
 * @brief   Initialize the voice interaction state machine
 *
 * @return  None
 */
extern void VoiceInteraction_init(void);

/*********************************************************************
 * @fn      VoiceInteraction_handleMicOpen
 *
 * @brief   Handle MIC_OPEN command from host (On-request model)
 *
 * @param   connHandle - Connection handle
 * @param   payload - MIC_OPEN payload (1 byte)
 * @param   len - Payload length
 *
 * @return  true if accepted, false if rejected
 */
extern bool VoiceInteraction_handleMicOpen(uint16_t connHandle, uint8_t *payload, uint16_t len);

/*********************************************************************
 * @fn      VoiceInteraction_handleMicClose
 *
 * @brief   Handle MIC_CLOSE command from host
 *
 * @param   connHandle - Connection handle
 * @param   payload - MIC_CLOSE payload (1 byte)
 * @param   len - Payload length
 *
 * @return  true if closed, false if error
 */
extern bool VoiceInteraction_handleMicClose(uint16_t connHandle, uint8_t *payload, uint16_t len);

/*********************************************************************
 * @fn      VoiceInteraction_handleMicExtend
 *
 * @brief   Handle MIC_EXTEND command from host
 *
 * @param   connHandle - Connection handle
 * @param   payload - MIC_EXTEND payload (1 byte)
 * @param   len - Payload length
 *
 * @return  true if extended, false if error
 */
extern bool VoiceInteraction_handleMicExtend(uint16_t connHandle, uint8_t *payload, uint16_t len);

/*********************************************************************
 * @fn      VoiceInteraction_startPTT
 *
 * @brief   Start Press-to-Talk session (Assistant button pressed)
 *
 * @param   connHandle - Connection handle
 *
 * @return  true if started, false if error
 */
extern bool VoiceInteraction_startPTT(uint16_t connHandle);

/*********************************************************************
 * @fn      VoiceInteraction_startHTT
 *
 * @brief   Start Hold-to-Talk session (Assistant button pressed)
 *
 * @param   connHandle - Connection handle
 *
 * @return  true if started, false if error
 */
extern bool VoiceInteraction_startHTT(uint16_t connHandle);

/*********************************************************************
 * @fn      VoiceInteraction_stopHTT
 *
 * @brief   Stop Hold-to-Talk session (Assistant button released)
 *
 * @param   connHandle - Connection handle
 *
 * @return  true if stopped, false if error
 */
extern bool VoiceInteraction_stopHTT(uint16_t connHandle);

/*********************************************************************
 * @fn      VoiceInteraction_updateLastInteraction
 *
 * @brief   Update last user interaction timestamp (for privacy timeout)
 *
 * @return  None
 */
extern void VoiceInteraction_updateLastInteraction(void);

/*********************************************************************
 * @fn      VoiceInteraction_checkTimeout
 *
 * @brief   Check for timeouts (1-min privacy, 30s max recording)
 *          Should be called periodically (e.g., every 1 second)
 *
 * @param   connHandle - Connection handle
 *
 * @return  None
 */
extern void VoiceInteraction_checkTimeout(uint16_t connHandle);

/*********************************************************************
 * @fn      VoiceInteraction_getCurrentState
 *
 * @brief   Get current voice interaction state
 *
 * @return  Current state
 */
extern VoiceState_t VoiceInteraction_getCurrentState(void);

/*********************************************************************
 * @fn      VoiceInteraction_isActive
 *
 * @brief   Check if audio session is currently active
 *
 * @return  true if active, false if idle
 */
extern bool VoiceInteraction_isActive(void);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_INTERACTION_H */
