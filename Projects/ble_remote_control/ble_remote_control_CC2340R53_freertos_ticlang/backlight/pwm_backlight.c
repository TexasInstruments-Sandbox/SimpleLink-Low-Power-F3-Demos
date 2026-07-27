/******************************************************************************

@file  pwm_backlight.c

@brief PWM-based keypad backlight control with scheduled mode and anti-gaming protection

This file implements keypad backlighting using PWM (Pulse Width Modulation) with
support for Google's GATT Service Definition for RCU Backlighting Configuration v1.0.

Backlight Modes (per Google spec):
1. Never (0x0000): Backlight disabled
2. Standard (0x0001): Backlight always active on key press
3. Scheduled (0x0002): Backlight only active during nighttime (18:00-08:00)

PWM Fade Sequence:
- Phase 1 (0-0.5s): Fade in from 0% to 45% duty cycle
- Phase 2 (0.5-2.5s): Hold at 45% maximum brightness
- Phase 3 (2.5-5.0s): Fade out from 45% to 0%
- Total duration: 5 seconds

Extension Behavior:
- If key pressed during fade (steps 1-2), advance to step 2 (max brightness)
- Extends overall illumination time for better user experience

Anti-Gaming Protection:
- If >25 key presses within 15 seconds: disable backlight for 2 minutes
- Exceptions: pause >1 minute OR 2 minutes elapsed
- Prevents battery drain from excessive button mashing

Integration:
- Calendar module: Time sync and nighttime detection (18:00-08:00)
- Backlight service: Mode configuration and time sync via BLE GATT
- Keypad module: Triggers backlight on key press events

Google ATV Spec Compliance: GATT Service Definition for RCU Backlighting Configuration v1.0

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
#include <stdbool.h>
#include <ti_drivers_config.h>
#include <ti/drivers/PWM.h>
#include <ti/drivers/GPIO.h>
#include "pwm_backlight.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include <ti/drivers/dpl/ClockP.h>
#include <ble_app/Profiles/backlight/backlight_service.h>
#include "calendar/calendar.h"

/*********************************************************************
 * CONSTANTS
 *********************************************************************/
#define STEP_TIMEOUT_MS       100    // PWM update interval: 100ms per step
#define CLOCK_UNITS_MS        1000   // Convert to milliseconds
#define N_KEY_PRESS_GAMER     25     // Gaming threshold: 25 key presses
#define KEY_TIME_LIM_COUNT    150    // Gaming window: 15 seconds (15000ms / 100ms)
#define TWO_MIN_COUNT         1200   // Gaming penalty: 2 minutes (120000ms / 100ms)
#define ONE_MIN_COUNT         600    // Gaming pause detection: 1 minute (60000ms / 100ms)

/*********************************************************************
 * LOCAL VARIABLES
 *
 * PWM Control:
 * - step_timeout_clkHandle: ClockP handle for 100ms periodic PWM updates
 * - pwm: PWM driver handle for backlight control
 * - pwmParams: PWM configuration parameters (1kHz, fractional duty cycle)
 * - dutyValue: Current PWM duty cycle value (0 to PWM_DUTY_FRACTION_MAX)
 * - dut_val: Current duty cycle percentage (0-45%)
 *
 * Fade State Machine:
 * - steptimecount: Current step in fade sequence (0-50 = 5 seconds total)
 * - click_reset: Flag indicating if backlight timer is active
 *
 * Anti-Gaming Protection:
 * - gamertimecount: Time counter for 15-second window
 * - gamerkeypress: Number of key presses in current window
 * - twomin_trigger: Flag indicating 2-minute penalty is active
 * - twomin_count: Counter for 2-minute penalty duration
 *
 * BLE Service Integration:
 * - backlightServiceData: BLE GATT service data (mode, time sync)
 * - backlightProgramMode: Active backlight mode configuration
 *********************************************************************/
 
static ClockP_Handle step_timeout_clkHandle;
static PWM_Handle pwm;
static PWM_Params pwmParams;
static uint32_t dutyValue;
static float dut_val;
static uint16_t steptimecount;
static uint16_t gamertimecount;
static uint16_t gamerkeypress;
static uint16_t twomin_count;
static uint8_t twomin_trigger;
static uint8_t click_reset;

/* Global BLE Service Variable */
extern Backlight_service_dataformat backlightServiceData;
Backlight_program_t backlightProgramMode;

/*********************************************************************
 * @fn      Backlighting_start
 *
 * @brief   Initialize PWM driver and parameters for backlight control
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Configures PWM for 1kHz frequency with fractional duty cycle.
 *          Must be called during system initialization before Backlighting_open().
 *********************************************************************/
void Backlighting_start(void)
{
    // Initialize the PWM driver
    PWM_init();
    // Initialize the PWM parameters
    PWM_Params_init(&pwmParams);
    pwmParams.idleLevel = PWM_IDLE_LOW;      // Output low when PWM is not running
    pwmParams.periodUnits = PWM_PERIOD_HZ;   // Period is in Hz
    pwmParams.periodValue = 1e3;             // 1kHz PWM frequency
    pwmParams.dutyUnits = PWM_DUTY_FRACTION; // Duty is in fractional percentage
    pwmParams.dutyValue = 0;                 // 0% initial duty cycle
}

/*********************************************************************
 * @fn      Backlighting_open
 *
 * @brief   Open PWM instance and enable backlight boost converter
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Enables boost converter (CONFIG_BOOST_EN) and starts PWM at 0%.
 *          Called at the start of each backlight activation.
 *********************************************************************/
void Backlighting_open(void)
{

    GPIO_write(CONFIG_BOOST_EN, 1);  // Enable boost converter for backlight
    // Open the PWM instance
    pwm = PWM_open(CONFIG_PWM_1, &pwmParams);
    if (pwm == NULL) {
        while (1);  // Fatal error - halt system
    }

    PWM_start(pwm); // Start PWM with 0% duty cycle
    dut_val = 0;
    dutyValue = (uint32_t) (((uint64_t) PWM_DUTY_FRACTION_MAX * dut_val) / 100);
    PWM_setDuty(pwm, dutyValue);  // Set duty cycle to 0%
}

/*********************************************************************
 * @fn      Backlighting_close
 *
 * @brief   Close PWM instance and disable backlight boost converter
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Disables boost converter and closes PWM to save power.
 *          Called at the end of each backlight deactivation.
 *********************************************************************/
void Backlighting_close(void)
{
    GPIO_write(CONFIG_BOOST_EN, 0);  // Disable boost converter
    PWM_close(pwm);                  // Close PWM instance
}

/*********************************************************************
 * @fn      Backlighting_process
 *
 * @brief   Process PWM backlight fade sequence and anti-gaming protection
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Called every 100ms by step_timeout_clkHandleCB timer callback.
 *
 *          PWM Fade Sequence (5 seconds total):
 *          - Steps 0-4 (0-0.5s): Fade in from 0% to 45% duty cycle
 *          - Steps 5-24 (0.5-2.5s): Hold at 45% maximum brightness
 *          - Steps 25-49 (2.5-5.0s): Fade out from 45% to 0%
 *          - Step 50: Clean up and stop timer
 *
 *          Anti-Gaming Protection:
 *          - If >25 key presses within 15 seconds (gamerkeypress >= 25):
 *            * Enter 2-minute penalty mode (twomin_trigger=1)
 *            * Force backlight to 0% for 2 minutes
 *            * Exit early if user pauses >1 minute AND <2 key presses
 *          - Prevents battery drain from excessive button mashing
 *********************************************************************/
void Backlighting_process(void)
{

    /* Anti-Gaming Protection Mode:
     * If >25 clicks in 15 seconds, disable backlight for 2 minutes
     * (or until user pauses constant clicking for more than 1 minute).
     * The timer has already been triggered and periodic timeout is executing. */
    if (twomin_trigger == 1)
    {
        /* 2-minute penalty mode: backlight forced to 0% */
        if (twomin_count <= TWO_MIN_COUNT)
        {
            /* Early exit condition: User pauses constant clicking for >1 minute */
            if ((twomin_count >= ONE_MIN_COUNT) && (gamerkeypress <= 1))
            {
                // Reset all variables and return to normal operation
                Backlighting_close();
                ClockP_stop(step_timeout_clkHandle);
                ClockP_delete(step_timeout_clkHandle);
                steptimecount = 0;
                dut_val = 0;
                click_reset = 0;
                twomin_trigger = 0;
                twomin_count = 0;
                gamerkeypress = 0;
                gamertimecount = 0;
            }
            twomin_count++;
            dut_val = 0;
            dutyValue = (uint32_t)((PWM_DUTY_FRACTION_MAX * dut_val) / 100);
            PWM_setDuty(pwm, dutyValue);  // Force 0% duty cycle
        }
        else
        {
            // 2-minute penalty complete - reset and return to normal
            Backlighting_close();
            ClockP_stop(step_timeout_clkHandle);
            ClockP_delete(step_timeout_clkHandle);
            steptimecount = 0;
            dut_val = 0;
            click_reset = 0;
            twomin_trigger = 0;
            twomin_count = 0;
            gamerkeypress = 0;
            gamertimecount = 0;
        }
    }
    else
    {
        /* Normal PWM Fade Sequence */
        steptimecount++;
        gamertimecount++;

        /* Phase 1: Fade in (Steps 0-4, 0-0.5s) */
        if (steptimecount < 5)
        {
            dut_val = 0;
            dutyValue = (uint32_t)((PWM_DUTY_FRACTION_MAX * dut_val) / 100);
            PWM_setDuty(pwm, dutyValue);  // Start at 0%
        }
        /* Phase 2: Ramp up to maximum brightness (Steps 5-19, 0.5-2.0s) */
        else if ((steptimecount >= 5) && (steptimecount < 20))
        {
            dut_val = dut_val + 2.6;  // Increment by 2.6% per step (15 steps * 2.6% ≈ 39%)
            dutyValue = (uint32_t)((PWM_DUTY_FRACTION_MAX * dut_val) / 100);
            PWM_setDuty(pwm, dutyValue);  // Ramp up to ~45% at step 20
        }
        /* Phase 2 continued: Hold at maximum (Steps 20-24, 2.0-2.5s) */
        // No change, holds at ~45% duty cycle

        /* Phase 3: Fade out (Steps 25-49, 2.5-5.0s) */
        else if ((steptimecount >= 25) && (steptimecount < 50))
        {
            dut_val = dut_val - 1.5;  // Decrement by 1.5% per step (25 steps * 1.5% ≈ 37.5%)
            dutyValue = (uint32_t)((PWM_DUTY_FRACTION_MAX * dut_val) / 100);
            PWM_setDuty(pwm, dutyValue);  // Fade down to ~0% at step 50
        }
        /* Phase 4: Clean up (Step 50) */
        else if (steptimecount == 50)
        {
            // Backlight sequence complete - clean up and stop timer
            Backlighting_close();
            ClockP_stop(step_timeout_clkHandle);
            ClockP_delete(step_timeout_clkHandle);
            steptimecount = 0;
            gamertimecount = 0;
            dut_val = 0;
            click_reset = 0;
        }
    }
}

/*********************************************************************
 * @fn      step_timeout_clkHandleCB
 *
 * @brief   Timer callback for PWM backlight processing
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Called every 100ms to update PWM duty cycle for fade sequence.
 *          Invokes Backlighting_process() in BLEAppUtil context.
 *********************************************************************/
static void step_timeout_clkHandleCB(void)
{
    BLEAppUtil_invokeFunctionNoData((InvokeFromBLEAppUtilContext_t*)Backlighting_process);
}

static ClockP_Struct timeout_clkStruct;

/*********************************************************************
 * @fn      BacklightingStep_start
 *
 * @brief   Start periodic timer for backlight PWM updates
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Creates a 100ms periodic timer that drives the fade sequence.
 *          Called when backlight is first triggered.
 *********************************************************************/
void BacklightingStep_start(void)
{
    static ClockP_Params clockpParams;
    ClockP_Params_init(&clockpParams);
    uint32_t clockTicks = (STEP_TIMEOUT_MS * CLOCK_UNITS_MS); // 100ms per step
    clockpParams.period = clockTicks; // Periodic timer
    clockpParams.startFlag = true; // Start immediately
    clockpParams.arg = (uintptr_t)step_timeout_clkHandleCB;
    step_timeout_clkHandle = ClockP_construct(&timeout_clkStruct, (void *)BLEAppUtil_invokeFunctionNoData, clockTicks, &clockpParams);
}

/*********************************************************************
 * @fn      BacklightingMode_program
 *
 * @brief   Process backlight mode configuration from BLE GATT service
 *
 * @param   None
 *
 * @return  Current backlight mode (MODE_NEVER, MODE_ALWAYS, MODE_SCHEDULE)
 *
 * @note    Reads backlightServiceData from BLE GATT writes and updates
 *          local backlightProgramMode configuration.
 *
 *          Handles two commands:
 *          1. SET_BACKLIGHT_MODE_CMD (0x00): Sets backlight mode
 *             - MODE_NEVER (0x0000): Backlight disabled
 *             - MODE_ALWAYS (0x0001): Backlight always enabled
 *             - MODE_SCHEDULE (0x0002): Backlight only during nighttime
 *
 *          2. SET_TIME_CMD (0x01): Time sync for scheduled mode
 *             - Only processed if mode is MODE_SCHEDULE
 *             - Time sync is handled by backlight_service.c via Calendar_setTime()
 *             - Stores time reference for future use
 *
 *          Integration: Called by Backlighting_trigger() before each activation.
 *********************************************************************/
uint16_t BacklightingMode_program(void)
{
    if (backlightServiceData.mod == 1)
    {
        /* Command 1: SET_BACKLIGHT_MODE_CMD (0x00) - Set backlight mode */
        if ((backlightServiceData.backlight_event_type == SET_BACKLIGHT_MODE_CMD))
        {
            /* Mode 0x0000: Backlight disabled (Never) */
            if (backlightServiceData.backlight_time_mode == MODE_NEVER)
            {
                backlightProgramMode.mode = MODE_NEVER;
            }
            /* Mode 0x0001: Backlight enabled (Standard/Always) */
            else if (backlightServiceData.backlight_time_mode == MODE_ALWAYS)
            {
                backlightProgramMode.mode = MODE_ALWAYS;
            }
            /* Mode 0x0002: Backlight enabled only during nighttime (Scheduled) */
            else if (backlightServiceData.backlight_time_mode == MODE_SCHEDULE)
            {
                backlightProgramMode.mode = MODE_SCHEDULE;
            }
        }
        /* Command 2: SET_TIME_CMD (0x01) - Time sync for scheduled mode */
        /* If mode is Schedule and SET_TIME_CMD received, time sync has already
         * been handled in backlight_service.c via Calendar_setTime().
         * This just stores the time reference for potential future use. */
        else if ((backlightProgramMode.mode == MODE_SCHEDULE) && (backlightServiceData.backlight_event_type == SET_TIME_CMD))
        {
            // Time sync already handled by backlight_service.c -> Calendar_setTime()
            // Store time reference (0xHHMM format) if needed for diagnostics
            backlightProgramMode.timeRef = backlightServiceData.backlight_time_mode;
        }

        // Clear modification flag after processing
        backlightServiceData.mod = 0;
    }

    return (backlightProgramMode.mode);
}

/*********************************************************************
 * @fn      Backlighting_trigger
 *
 * @brief   Trigger backlight activation on key press (main entry point)
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Called by keypad module on every key press event.
 *
 *          Mode-based activation logic:
 *          - MODE_NEVER (0x0000): Always disabled, no backlight
 *          - MODE_ALWAYS (0x0001): Always enabled on key press
 *          - MODE_SCHEDULE (0x0002): Only enabled during nighttime (18:00-08:00)
 *
 *          Extension behavior:
 *          - If timer already running (click_reset=1) and in Phase 1-2:
 *            Jump to Phase 2 (max brightness) to extend illumination
 *
 *          Anti-gaming protection:
 *          - Tracks key press rate: gamerkeypress incremented on each call
 *          - If >25 presses in 15 seconds: trigger 2-minute penalty
 *          - Resets gamer counters when threshold exceeded
 *
 *          Integration:
 *          - Calendar_isNighttime(): Checks current time for scheduled mode
 *          - BacklightingMode_program(): Reads mode from BLE GATT service
 *********************************************************************/
void Backlighting_trigger(void)
{
    // Read and process backlight mode from BLE GATT service
    uint16_t mode = BacklightingMode_program();

    // Determine if backlight should be enabled based on current mode
    bool enabled = false;
    if (mode == MODE_NEVER)
    {
        enabled = false;  // Never mode: always disabled
    }
    else if (mode == MODE_ALWAYS)
    {
        enabled = true;  // Standard mode: always enabled
    }
    else if (mode == MODE_SCHEDULE)
    {
        // Scheduled mode: only enable during nighttime (18:00-08:00)
        enabled = Calendar_isNighttime();
    }

    if (enabled)
    {
        /* First key press: Initialize backlight and start PWM fade sequence */
        if (click_reset == 0)
        {
            click_reset=1;
            Backlighting_open();        // Enable boost converter and PWM
            BacklightingStep_start();   // Start 100ms periodic timer
        }
        else
        {
            /* Subsequent key press: Extend backlight illumination
             * If in Phase 1-2 (steps 0-19), jump to Phase 2 at max brightness
             * to extend overall illumination time */
            if ((steptimecount >= 5) && (twomin_trigger == 0))
            {
                steptimecount = 20;  // Jump to Phase 2 hold period
                dut_val = 45;        // Set to max brightness
                dutyValue = (uint32_t)(((uint64_t) PWM_DUTY_FRACTION_MAX * dut_val) / 100);
                PWM_setDuty(pwm, dutyValue);
            }

            /* Anti-gaming protection: Prevent battery drain from excessive use
             * If >25 key presses within 15 seconds:
             *   - Trigger 2-minute penalty mode
             *   - Force backlight to 0% for 2 minutes
             *   - Exit early if user pauses >1 minute */
            gamerkeypress++;
            if ((gamertimecount >= KEY_TIME_LIM_COUNT) && (gamerkeypress >= N_KEY_PRESS_GAMER))
            {
                twomin_trigger = 1;  // Activate 2-minute penalty mode
                twomin_count = 1;    // Start penalty counter (restart if already in penalty)
                gamerkeypress = 0;   // Reset gamer key press counter
                gamertimecount = 0;  // Reset gamer time counter
            }
        }
    }
}
