/******************************************************************************

@file  voice_interaction.c

@brief Voice interaction model state machine implementation for Google Voice over BLE v1.1

This file implements the voice interaction state machine for three assistant
interaction models defined in Google Voice over BLE specification v1.1:
- On-request: Host-initiated voice capture via MIC_OPEN command
- Press-to-Talk (PTT): Button press starts recording, continues until timeout
- Hold-to-Talk (HTT): Button press starts recording, stops on button release

The state machine manages:
- Voice session lifecycle (IDLE -> ON_REQUEST/PTT/HTT -> IDLE)
- Stream ID assignment (0x00 for On-request, 0x01-0x80 auto-increment for PTT/HTT)
- Privacy timeouts (1-minute inactivity timeout, 30-second max recording timeout)
- AUDIO_START/AUDIO_STOP messaging on ATVV_CHAR_CTL characteristic
- Integration with LED control for visual feedback
- Integration with keypad module for button handling

Key Google ATV Spec Compliance:
- Spec: Google Voice over BLE v1.1
- Capabilities: ADPCM 8kHz/16kHz, HTT/PTT/On-request models
- Timeouts: 60s inactivity, 30s max recording
- Stream ID: 0x00 (On-request), 0x01-0x80 (PTT/HTT with rollover)

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

/*********************************************************************
 * INCLUDES
 *********************************************************************/
#include <string.h>
#include "voice_interaction.h"
#include "voice_processing.h"
#include "google_voice_spec.h"
#include "led/led.h"
#include <ti/drivers/dpl/ClockP.h>

/*********************************************************************
 * EXTERNAL FUNCTIONS AND VARIABLES
 *
 * These functions and variables are provided by other modules:
 * - Audio_sendBufferOverBLE: Sends data over ATVV_CHAR_CTL/ATVV_CHAR_DATA
 * - ClockP_*: System clock functions for timestamp management
 * - audio_done: LED control flag - set to 1 to stop LED blinking
 * - audio_key_enabled: Audio key state flag - prevents multiple simultaneous sessions
 *********************************************************************/
extern void Audio_sendBufferOverBLE(uint8_t *pData, uint16_t len, uint8_t charType);
extern uint32_t ClockP_getSystemTicks(void);
extern uint32_t ClockP_getSystemTickPeriod(void);
extern uint8_t audio_done;  // LED stop flag from keypad/led modules
extern uint8_t audio_key_enabled;  // Audio key state from keypad
uint8_t stop_audio = 0;

/*********************************************************************
 * LOCAL VARIABLES
 *
 * Voice session state tracking:
 * - state: Current voice interaction state (IDLE/ON_REQUEST/PTT/HTT)
 * - model: Active interaction model (determines LED behavior and timeout handling)
 * - streamId: Audio stream identifier (0x00 for On-request, 0x01-0x80 for PTT/HTT)
 * - startTime: Timestamp when current session started (for 30s max recording timeout)
 * - lastInteractionTime: Timestamp of last user interaction (for 1-min privacy timeout)
 * - isActive: Boolean flag indicating if audio streaming is active
 *********************************************************************/
static VoiceSession_t voiceSession = {
    .state = VOICE_STATE_IDLE,
    .model = VOICE_MODEL_ON_REQUEST,
    .streamId = 0,
    .startTime = 0,
    .lastInteractionTime = 0,
    .isActive = false
};

/*********************************************************************
 * LOCAL FUNCTIONS
 *********************************************************************/
static void sendAudioStart(uint16_t connHandle, uint8_t reason);
static void sendAudioStop(uint16_t connHandle, uint8_t reason);
static uint32_t getSystemTimeMs(void);

/*********************************************************************
 * @fn      getSystemTimeMs
 *
 * @brief   Get current system time in milliseconds
 *
 * @return  System time in milliseconds
 *********************************************************************/
static uint32_t getSystemTimeMs(void)
{
    uint32_t ticks = ClockP_getSystemTicks();
    uint32_t tickPeriodUs = ClockP_getSystemTickPeriod();
    return (ticks * tickPeriodUs) / 1000;  // Convert to milliseconds
}

/*********************************************************************
 * @fn      sendAudioStart
 *
 * @brief   Send AUDIO_START message on ATVV_CHAR_CTL
 *
 * @param   connHandle - Connection handle
 * @param   reason - AUDIO_START reason (GVOICE_REASON_*)
 *
 * @return  None
 *********************************************************************/
static void sendAudioStart(uint16_t connHandle, uint8_t reason)
{
    // AUDIO_START format: CMD(1) + reason(1) + codec(1) + streamId(1) = 4 bytes minimum
    // + optional codec config (3 bytes for ADPCM)
    uint8_t audioStartMsg[4] = {
        GVOICE_CMD_AUDIO_START,         // 0x04
        reason,                         // 0x00=MIC_OPEN, 0x01=PTT, 0x03=HTT
        GVOICE_CODEC_ADPCM_16KHZ,       // Using 16kHz by default (can switch dynamically)
        voiceSession.streamId           // Stream ID
    };

    // TODO: Add ADPCM codec config (pred value + step index) if needed
    // For now, send basic 4-byte message

    // Send on ATVV_CHAR_CTL (charType=2)
    Audio_sendBufferOverBLE(audioStartMsg, sizeof(audioStartMsg), 2);

    stop_audio = 0;

    // Enable audio hardware
    Audio_enable();

    voiceSession.isActive = true;
    voiceSession.startTime = getSystemTimeMs();
}

/*********************************************************************
 * @fn      sendAudioStop
 *
 * @brief   Send AUDIO_STOP message on ATVV_CHAR_CTL
 *
 * @param   connHandle - Connection handle
 * @param   reason - AUDIO_STOP reason (GVOICE_STOP_*)
 *
 * @return  None
 *********************************************************************/
static void sendAudioStop(uint16_t connHandle, uint8_t reason)
{
    // AUDIO_STOP format: CMD(1) + reason(1) = 2 bytes
    uint8_t audioStopMsg[2] = {
        GVOICE_CMD_AUDIO_STOP,          // 0x00
        reason                          // Stop reason
    };

    stop_audio = 1;

    // Disable audio hardware
    Audio_disable();

    // Send on ATVV_CHAR_CTL (charType=2)
    Audio_sendBufferOverBLE(audioStopMsg, sizeof(audioStopMsg), 2);

    voiceSession.isActive = false;

    // Stop LED blinking and reset audio key state
    audio_done = 1;
    audio_key_enabled = 0;
}

/*********************************************************************
 * PUBLIC FUNCTIONS
 *********************************************************************/

/*********************************************************************
 * @fn      VoiceInteraction_init
 *
 * @brief   Initialize the voice interaction state machine
 *********************************************************************/
void VoiceInteraction_init(void)
{
    memset(&voiceSession, 0, sizeof(VoiceSession_t));
    voiceSession.state = VOICE_STATE_IDLE;
    voiceSession.model = VOICE_MODEL_ON_REQUEST;
    voiceSession.streamId = 0;  // Will be incremented to 1 on first PTT/HTT session
    voiceSession.lastInteractionTime = getSystemTimeMs();
}

/*********************************************************************
 * @fn      VoiceInteraction_handleMicOpen
 *
 * @brief   Handle MIC_OPEN command from host (On-request model)
 *********************************************************************/
bool VoiceInteraction_handleMicOpen(uint16_t connHandle, uint8_t *payload, uint16_t len)
{
    // Check privacy timeout (1 minute since last interaction)
    uint32_t currentTime = getSystemTimeMs();
    uint32_t timeSinceLastInteraction = currentTime - voiceSession.lastInteractionTime;

    if (timeSinceLastInteraction > GVOICE_TIMEOUT_ACTIVE_REMOTE) {
        // Send MIC_OPEN_ERROR - timeout exceeded
        uint8_t errorMsg[2] = {
            GVOICE_CMD_MIC_OPEN_ERROR,  // 0x0C
            GVOICE_ERROR_TIMEOUT        // 0x01
        };
        Audio_sendBufferOverBLE(errorMsg, sizeof(errorMsg), 2);
        return false;
    }

    // Check if already active
    if (voiceSession.isActive) {
        // Send MIC_OPEN_ERROR - busy
        uint8_t errorMsg[2] = {
            GVOICE_CMD_MIC_OPEN_ERROR,  // 0x0C
            GVOICE_ERROR_BUSY           // 0x02
        };
        Audio_sendBufferOverBLE(errorMsg, sizeof(errorMsg), 2);
        return false;
    }

    // Start On-request session
    voiceSession.state = VOICE_STATE_ON_REQUEST;
    voiceSession.model = VOICE_MODEL_ON_REQUEST;
    voiceSession.streamId = 0;  // On-request always uses stream ID 0

    sendAudioStart(connHandle, GVOICE_REASON_MIC_OPEN);

    return true;
}

/*********************************************************************
 * @fn      VoiceInteraction_handleMicClose
 *
 * @brief   Handle MIC_CLOSE command from host
 *********************************************************************/
bool VoiceInteraction_handleMicClose(uint16_t connHandle, uint8_t *payload, uint16_t len)
{
    // Only valid for On-request model
    if (voiceSession.state != VOICE_STATE_ON_REQUEST) {
        return false;
    }

    // Stop the session
    sendAudioStop(connHandle, GVOICE_STOP_MIC_CLOSE);

    voiceSession.state = VOICE_STATE_IDLE;
    voiceSession.isActive = false;

    return true;
}

/*********************************************************************
 * @fn      VoiceInteraction_handleMicExtend
 *
 * @brief   Handle MIC_EXTEND command from host
 *********************************************************************/
bool VoiceInteraction_handleMicExtend(uint16_t connHandle, uint8_t *payload, uint16_t len)
{
    // Only valid for PTT/HTT models
    if (voiceSession.state == VOICE_STATE_PTT || voiceSession.state == VOICE_STATE_HTT) {
        // Reset the start time to extend the session
        voiceSession.startTime = getSystemTimeMs();
        return true;
    }

    return false;
}

/*********************************************************************
 * @fn      VoiceInteraction_startPTT
 *
 * @brief   Start Press-to-Talk session (Assistant button pressed)
 *********************************************************************/
bool VoiceInteraction_startPTT(uint16_t connHandle)
{
    // Check privacy timeout
    uint32_t currentTime = getSystemTimeMs();
    uint32_t timeSinceLastInteraction = currentTime - voiceSession.lastInteractionTime;

    if (timeSinceLastInteraction > GVOICE_TIMEOUT_ACTIVE_REMOTE) {
        return false;  // Reject - privacy timeout
    }

    // Check if already active
    if (voiceSession.isActive) {
        return false;  // Reject - already active
    }

    // Increment stream ID (1-0x80, rolls over)
    voiceSession.streamId++;
    if (voiceSession.streamId > 0x80) {
        voiceSession.streamId = 1;
    }

    voiceSession.state = VOICE_STATE_PTT;
    voiceSession.model = VOICE_MODEL_PTT;

    sendAudioStart(connHandle, GVOICE_REASON_PTT);

    // Update last interaction
    voiceSession.lastInteractionTime = currentTime;

    return true;
}

/*********************************************************************
 * @fn      VoiceInteraction_startHTT
 *
 * @brief   Start Hold-to-Talk session (Assistant button pressed)
 *********************************************************************/
bool VoiceInteraction_startHTT(uint16_t connHandle)
{
    // Check privacy timeout
    uint32_t currentTime = getSystemTimeMs();
    uint32_t timeSinceLastInteraction = currentTime - voiceSession.lastInteractionTime;

    if (timeSinceLastInteraction > GVOICE_TIMEOUT_ACTIVE_REMOTE) {
        return false;  // Reject - privacy timeout
    }

    // Check if already active
    if (voiceSession.isActive) {
        return false;  // Reject - already active
    }

    // Increment stream ID (1-0x80, rolls over)
    voiceSession.streamId++;
    if (voiceSession.streamId > 0x80) {
        voiceSession.streamId = 1;
    }

    voiceSession.state = VOICE_STATE_HTT;
    voiceSession.model = VOICE_MODEL_HTT;

    sendAudioStart(connHandle, GVOICE_REASON_HTT);

    // Update last interaction
    voiceSession.lastInteractionTime = currentTime;

    return true;
}

/*********************************************************************
 * @fn      VoiceInteraction_stopHTT
 *
 * @brief   Stop Hold-to-Talk session (Assistant button released)
 *********************************************************************/
bool VoiceInteraction_stopHTT(uint16_t connHandle)
{
    // Only valid for HTT model
    if (voiceSession.state != VOICE_STATE_HTT) {
        return false;
    }

    // Stop the session
    sendAudioStop(connHandle, GVOICE_STOP_HTT_RELEASE);

    voiceSession.state = VOICE_STATE_IDLE;
    voiceSession.isActive = false;

    return true;
}

/*********************************************************************
 * @fn      VoiceInteraction_updateLastInteraction
 *
 * @brief   Update last user interaction timestamp
 *********************************************************************/
void VoiceInteraction_updateLastInteraction(void)
{
    voiceSession.lastInteractionTime = getSystemTimeMs();
}

/*********************************************************************
 * @fn      VoiceInteraction_checkTimeout
 *
 * @brief   Check for timeouts (1-min privacy, 30s max recording)
 *********************************************************************/
void VoiceInteraction_checkTimeout(uint16_t connHandle)
{
    if (!voiceSession.isActive)
    {
        return;  // No active session
    }

    uint32_t currentTime = getSystemTimeMs();
    uint32_t sessionDuration = currentTime - voiceSession.startTime;

    // Check 30-second max recording timeout
    if (sessionDuration > GVOICE_TIMEOUT_MAX_RECORDING)
    {
        sendAudioStop(connHandle, GVOICE_STOP_TIMEOUT);
        voiceSession.state = VOICE_STATE_IDLE;
        // Note: sendAudioStop() now handles setting audio_done and audio_key_enabled
    }
}

/*********************************************************************
 * @fn      VoiceInteraction_getCurrentState
 *
 * @brief   Get current voice interaction state
 *********************************************************************/
VoiceState_t VoiceInteraction_getCurrentState(void)
{
    return voiceSession.state;
}

/*********************************************************************
 * @fn      VoiceInteraction_isActive
 *
 * @brief   Check if audio session is currently active
 *********************************************************************/
bool VoiceInteraction_isActive(void)
{
    return voiceSession.isActive;
}
