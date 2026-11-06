/******************************************************************************

@file  google_voice_spec.h

@brief Google Voice over BLE specification v1.1 defines and constants

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

#ifndef GOOGLE_VOICE_SPEC_H
#define GOOGLE_VOICE_SPEC_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * CONSTANTS - Google Voice over BLE spec v1.1
 *********************************************************************/

// Commands on ATVV_CHAR_TX (Write characteristic - from host to remote)
#define GVOICE_CMD_MIC_OPEN         0x04  // Open microphone
#define GVOICE_CMD_MIC_CLOSE        0x05  // Close microphone
#define GVOICE_CMD_GET_CAPS         0x0A  // Get capabilities
#define GVOICE_CMD_MIC_EXTEND       0x0B  // Extend audio session

// Commands on ATVV_CHAR_CTL (Notify characteristic - from remote to host)
#define GVOICE_CMD_AUDIO_STOP       0x00  // Audio transmission stopped
#define GVOICE_CMD_AUDIO_START      0x04  // Audio transmission started
#define GVOICE_CMD_START_SEARCH     0x08  // Start search (PTT only)
#define GVOICE_CMD_AUDIO_SYNC       0x0A  // Audio sync message
#define GVOICE_CMD_CAPS_RESP        0x0B  // Capability response
#define GVOICE_CMD_MIC_OPEN_ERROR   0x0C  // Error opening microphone

// Codec IDs (bitmask for CAPS_RESP)
#define GVOICE_CODEC_ADPCM_8KHZ     0x01  // ADPCM 8kHz/16bit
#define GVOICE_CODEC_ADPCM_16KHZ    0x02  // ADPCM 16kHz/16bit
#define GVOICE_CODEC_ADPCM_BOTH     0x03  // Both 8kHz and 16kHz (supports dynamic BW)
#define GVOICE_CODEC_LC3            0x04  // LC3 (not supported per user request)

// Assistant Interaction Models
#define GVOICE_MODEL_ON_REQUEST     0x00  // On-request model (host-triggered via MIC_OPEN)
#define GVOICE_MODEL_PTT            0x01  // Press-to-Talk model (button-triggered)
#define GVOICE_MODEL_HTT            0x03  // Hold-to-Talk model (hold button)

// AUDIO_START reason field values
#define GVOICE_REASON_MIC_OPEN      0x00  // Triggered by MIC_OPEN command
#define GVOICE_REASON_PTT           0x01  // Triggered by Assistant button (PTT)
#define GVOICE_REASON_HTT           0x03  // Triggered by Assistant button (HTT)

// AUDIO_STOP reason field values
#define GVOICE_STOP_MIC_CLOSE       0x00  // Stopped by MIC_CLOSE command
#define GVOICE_STOP_HTT_RELEASE     0x02  // HTT button released
#define GVOICE_STOP_AUDIO_START     0x04  // New AUDIO_START command coming
#define GVOICE_STOP_TIMEOUT         0x08  // Audio transfer timeout
#define GVOICE_STOP_CCC_DISABLED    0x10  // Notifications disabled on ATVV_CHAR_AUDIO
#define GVOICE_STOP_OTHER           0x80  // Other reason

// MIC_OPEN_ERROR reason field values
#define GVOICE_ERROR_TIMEOUT        0x01  // 1-minute timeout exceeded (privacy)
#define GVOICE_ERROR_BUSY           0x02  // Mic already in use
#define GVOICE_ERROR_OTHER          0xFF  // Other error

// Spec version
#define GVOICE_SPEC_VERSION_MAJOR   0x01  // Version 1.x
#define GVOICE_SPEC_VERSION_MINOR   0x01  // Version x.1
#define GVOICE_SPEC_VERSION         0x0101 // Version 1.1 (big-endian format)

// Extra configuration flags
#define GVOICE_EXTRA_CFG_DLE        0x01  // DLE supported (bit 0)

// Default frame sizes
#define GVOICE_FRAME_SIZE_DEFAULT   20    // Default ADPCM frame size (bytes)
#define GVOICE_FRAME_SIZE_RECOMMENDED 160  // Recommended for 16kHz + DLE (bytes)

// Timeouts (milliseconds)
#define GVOICE_TIMEOUT_ACTIVE_REMOTE    60000  // 1 minute - disable mic after last interaction
#define GVOICE_TIMEOUT_MAX_RECORDING    30000  // 30 seconds - maximum recording duration
#define GVOICE_TIMEOUT_AUDIO_TRANSFER   5000   // 5 seconds - no frames sent

#ifdef __cplusplus
}
#endif

#endif /* GOOGLE_VOICE_SPEC_H */
