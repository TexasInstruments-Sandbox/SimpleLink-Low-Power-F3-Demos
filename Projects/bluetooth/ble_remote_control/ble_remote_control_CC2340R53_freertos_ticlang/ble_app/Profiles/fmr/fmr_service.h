/******************************************************************************

@file  fmr_service.h

@brief This file contains the Audio Audio service definitions and prototypes.

Group: WCS, BTS
Target Device: cc23xx

******************************************************************************

 Copyright (c) 2022-2025, Texas Instruments Incorporated
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


#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include <ti/drivers/utils/List.h>
#include "ti/ble/stack_util/icall/app/icall_ble_api.h"
#include <fmr/pwm_buzzer.h>

// Battery Service callback function
typedef void (*fmrServiceCB_t)(uint8 event);

typedef struct __attribute__((packed))
{
   uint8_t fmr_event_type;
   uint16_t fmr_time_mode;
} FMR_service_dataformat;

extern FMR_service_dataformat FMRServiceData;

typedef enum FMR_lastEvent
{
   PEER_REQUEST          = 0x00,  /* The value that is implied by every successful write operation. */
   DURATION_ELAPSED      = 0x01,  /* Set if and only if the remaining buzz duration is counted down to 0. */
   RCU_FOUND             = 0x02,  /* Set when the user picks up the remote or presses any button while in the buzz mode. */
   DURATION_TOO_LONG     = 0x03,  /* Is set in the response to write operation when the provided remaining buzz duration is too large. */
   LOW_BATTERY           = 0x04,  /* Used when the battery is too low to activate or maintain buzz mode. Remaining buzz duration is usually adjusted to 0 at the same time */
   FMR_OTHER             = 0x7F,  /* Any other event not described in this specification. */
   FMR_INVALID_REQUEST   = 0xFF,  /* Set when received an invalid write operation from the host. */
} FMR_lastEvent_t;

bStatus_t FMR_AddService(void);

void FMR_notify(uint16_t connHandle, FMR_service_dataformat fmr_noti);
