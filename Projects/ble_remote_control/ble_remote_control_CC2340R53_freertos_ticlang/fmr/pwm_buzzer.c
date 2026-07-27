/******************************************************************************

@file  pwm_buzzer.c

@brief Find My Remote (FMR) buzzer control using PWM

This file implements the buzzer control for Find My Remote feature, allowing
the Android TV host to trigger an audible alert to help users locate a lost remote.

Per Google's Find My Remote over BLE v1.0 specification, the FMR service enables:
- Host-initiated buzzing via PEER_REQUEST event
- Configurable buzzing duration (0-15 minutes in 100ms increments)
- User-initiated stop via button press (RCU_FOUND event)
- Automatic stop when duration elapses (DURATION_ELAPSED event)
- Battery-aware operation (buzzer only enabled when battery sufficient)

Buzzer Configuration:
- PWM frequency: 1kHz
- Duty cycle: 37%
- Update interval: 100ms (for duration tracking and notification updates)
- Timer-based control with periodic callbacks

FMR Event Flow:
1. Host sends PEER_REQUEST (0x00) with duration in 100ms units
2. Remote starts buzzer and sends PEER_REQUEST notifications every 100ms
3. Buzzing continues until:
   - User presses any button → sends RCU_FOUND (0x01) notification
   - Duration elapses → sends DURATION_ELAPSED (0x02) notification
   - Host sends stop command (duration=0x0000)

Integration:
- FMR service (fmr_service.c): GATT characteristic for remote control
- Keypad module: Detects button press to trigger RCU_FOUND event
- Battery service: Prevents buzzer operation when battery low

Google ATV Spec Compliance: Find My Remote over BLE v1.0

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

/* Include PWM Driver library */ 
#include <ti_drivers_config.h>
#include <ti/drivers/PWM.h>
#include "pwm_buzzer.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include <ble_app/Profiles/fmr/fmr_service.h>
#include <ti/drivers/dpl/ClockP.h>

/*********************************************************************
 * CONSTANTS
 *********************************************************************/
#define BUZZ_TIMEOUT_MS       100   // Buzzer update interval: 100ms per count
#define CLOCK_UNITS_MS        1000  // Convert to milliseconds

/*********************************************************************
 * LOCAL VARIABLES
 *
 * PWM Control:
 * - buzz_timeout_clkHandle: ClockP handle for 100ms periodic buzzer updates
 * - pwm: PWM driver handle for buzzer control
 * - pwmParams: PWM configuration parameters (1kHz, 37% duty cycle)
 * - dutyValue: Current PWM duty cycle value
 *
 * FMR State:
 * - buzz_count: Current buzzing duration counter (in 100ms units)
 * - rc_used: Flag indicating user pressed button (RCU_FOUND event)
 * - buzz_reset: Flag indicating if buzzer timer is active
 *
 * BLE Service Integration:
 * - FMRServiceData: External data from FMR GATT service (duration, events)
 * - buzzerProgramMode: Local FMR data for notifications back to host
 *********************************************************************/
static ClockP_Handle buzz_timeout_clkHandle;
static PWM_Handle pwm;
static PWM_Params pwmParams;
uint16_t buzz_count;           // Exported: Remaining buzz duration (100ms units)
static uint32_t dutyValue;
uint8_t rc_used;               // Exported: Set by keypad when button pressed
uint8_t buzz_reset;            // Exported: Buzzer active flag

extern FMR_service_dataformat FMRServiceData;
FMR_service_dataformat buzzerProgramMode;

/*********************************************************************
 * @fn      Buzzer_start
 *
 * @brief   Initialize PWM driver and parameters for buzzer control
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Configures PWM for 1kHz frequency with fractional duty cycle.
 *          Must be called during system initialization before Buzzer_open().
 *          Called by FMR_start() in fmr.c.
 *********************************************************************/
void Buzzer_start(void)
{
    // Initialize the PWM driver
    PWM_init();
    // Initialize the PWM parameters
    PWM_Params_init(&pwmParams);
    pwmParams.idleLevel = PWM_IDLE_LOW;      // Output low when PWM is not running
    pwmParams.periodUnits = PWM_PERIOD_HZ;   // Period is in Hz
    pwmParams.periodValue = 1e3;             // 1kHz buzzer frequency
    pwmParams.dutyUnits = PWM_DUTY_FRACTION; // Duty is in fractional percentage
    pwmParams.dutyValue = 0;                 // 0% initial duty cycle
}

/*********************************************************************
 * @fn      Buzzer_open
 *
 * @brief   Open PWM instance and start buzzer tone
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Starts PWM at 37% duty cycle for audible tone.
 *          Called when FMR buzzing is triggered.
 *********************************************************************/
static void Buzzer_open(void)
{
    // Open the PWM instance
    pwm = PWM_open(CONFIG_PWM_0, &pwmParams);
    if (pwm == NULL) {
        while (1);  // Fatal error - halt system
    }

    PWM_start(pwm); // Start PWM
    dutyValue = (uint32_t) (((uint64_t) PWM_DUTY_FRACTION_MAX * 37) / 100);
    PWM_setDuty(pwm, dutyValue);  // Set duty cycle to 37% for audible tone
}

/*********************************************************************
 * @fn      Buzzer_close
 *
 * @brief   Close PWM instance and stop buzzer
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Called when buzzing stops (user found, duration elapsed, or host stop).
 *********************************************************************/
static void Buzzer_close(void)
{
    PWM_close(pwm);
}

/*********************************************************************
 * @fn      buzz_timeout_clkHandleCB
 *
 * @brief   Buzzer timer callback - tracks duration and sends FMR notifications
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Called every 100ms while buzzer is active.
 *
 *          FMR Event Handling (per Google Find My Remote v1.0):
 *
 *          1. RCU_FOUND (0x01): User pressed any button
 *             - rc_used flag set by keypad module
 *             - Stops buzzer immediately
 *             - Notifies host with RCU_FOUND event
 *
 *          2. DURATION_ELAPSED (0x02): Buzzing duration completed
 *             - buzz_count reached configured duration
 *             - Stops buzzer automatically
 *             - Notifies host with DURATION_ELAPSED event
 *
 *          3. PEER_REQUEST (0x00): Buzzing in progress (default)
 *             - Increments buzz_count every 100ms
 *             - Sends periodic PEER_REQUEST notifications with elapsed time
 *             - Continues until condition 1 or 2 met
 *
 *          Duration Format:
 *          - FMRServiceData.fmr_time_mode: Total duration in 100ms units
 *          - Example: 0x012C = 300 units = 30 seconds
 *          - Range: 0x0000 (stop) to 0x23F0 (15 minutes = 9000 units)
 *
 *          Host can stop buzzing at any time by writing duration=0x0000.
 *********************************************************************/
static void buzz_timeout_clkHandleCB(void)
{
    /* Event 1: User pressed button → RCU_FOUND */
    if (rc_used==1)
    {
        buzzerProgramMode.fmr_event_type = RCU_FOUND;
        buzzerProgramMode.fmr_time_mode = FMRServiceData.fmr_time_mode;
        FMR_notify(0, buzzerProgramMode);
        Buzzer_close();
        ClockP_stop(buzz_timeout_clkHandle);
        ClockP_delete(buzz_timeout_clkHandle);
        buzz_count = 0;
        buzz_reset = 0;
        rc_used = 0;
        return;
    }

    /* Event 2: Duration elapsed OR host sent stop command (duration=0) → DURATION_ELAPSED */
    if (buzz_count == 0)
    {
        buzzerProgramMode.fmr_event_type = DURATION_ELAPSED;
        buzzerProgramMode.fmr_time_mode = buzz_count; // notify the counter counted down to zero.
        FMR_notify(0, buzzerProgramMode);
        Buzzer_close();
        ClockP_stop(buzz_timeout_clkHandle);
        ClockP_delete(buzz_timeout_clkHandle);
        buzz_count = 0;
        buzz_reset = 0;
        rc_used = 0;
        return;
    }

    /* Event 3: Buzzing in progress → PEER_REQUEST with elapsed time */
    buzz_count--;
    buzzerProgramMode.fmr_event_type = PEER_REQUEST;
    buzzerProgramMode.fmr_time_mode = (uint16_t)buzz_count;
    //FMR_notify(0, buzzerProgramMode);
}

/*********************************************************************
 * @fn      Buzzing_start
 *
 * @brief   Start periodic timer for buzzer duration tracking
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Creates a 100ms periodic timer for FMR event notifications.
 *          Called when buzzing is first triggered.
 *********************************************************************/
static void Buzzing_start(void)
{
    static ClockP_Params clockpParams;
    static ClockP_Struct fmr_timeout_clkStruct;
    ClockP_Params_init(&clockpParams);
    uint32_t clockTicks = (BUZZ_TIMEOUT_MS * CLOCK_UNITS_MS); // 100ms period
    clockpParams.period = clockTicks; // Periodic timer
    clockpParams.startFlag = true; // Start immediately
    clockpParams.arg = (uintptr_t)buzz_timeout_clkHandleCB;
    buzz_timeout_clkHandle = ClockP_construct(&fmr_timeout_clkStruct, (void *)BLEAppUtil_invokeFunctionNoData, clockTicks, &clockpParams);
}

/*********************************************************************
 * @fn      Buzzing_trigger
 *
 * @brief   Trigger FMR buzzing (main entry point)
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Called by FMR service when host writes PEER_REQUEST event.
 *          Only starts buzzer if not already active (buzz_reset=0).
 *          Prevents multiple simultaneous buzzing timers.
 *********************************************************************/
void Buzzing_trigger(void)
{
    /* Start buzzer only if not already active */
    if (buzz_reset == 0)
    {
        buzz_reset=1;
        Buzzer_open();    // Start PWM buzzer tone
        buzz_count = FMRServiceData.fmr_time_mode;
        Buzzing_start();  // Start 100ms periodic timer
    }
}
