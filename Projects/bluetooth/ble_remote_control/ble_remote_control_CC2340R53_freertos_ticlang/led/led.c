/******************************************************************************

@file  led.c

@brief LED control module for Google ATV Remote Control specification compliance

This file implements LED patterns for visual feedback on the BLE remote control,
following Google Android TV Remote Control PRD v1.5 specifications.

LED Color Legend:
- GREEN: BLE operations (key press, voice, pairing, success)
- RED: IR operations, errors, OTA updates
- ORANGE: IR key press (GREEN + RED simultaneously)

Supported LED Patterns:
1. Key Press Feedback:
   - BLE key: GREEN 100ms
   - IR key: ORANGE 100ms
   - Unpaired key: RED 100ms

2. Voice Interaction:
   - On-request: Static GREEN (manual control)
   - HTT: Static GREEN (manual control)
   - PTT: Blinking GREEN 500ms ON/OFF

3. BLE Status:
   - Pairing: Blinking GREEN 2Hz (250ms period)
   - Success: 2x blink GREEN 100ms ON/OFF
   - Error: 4x blink RED 50ms ON/OFF
   - OTA: Static RED

Timer-based LED Control:
- Uses ClockP periodic timer with callback for precise timing
- Supports three modes:
  * One-shot (periodic=0): Single timeout, LED turns off
  * External stop (periodic=1): Continues until audio_done/pairing_done flag
  * Fixed cycles (periodic=N): Runs for N toggle cycles then stops

Google ATV Spec Compliance: PRD v1.5 Section 3.4 (LED Behavior)

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

#include <stdint.h>
#include <ti/drivers/GPIO.h>
#include "ti_drivers_config.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include <ti/drivers/dpl/ClockP.h>
#include <led/led.h>

#define CLOCK_UNITS_MS        1000  // convert to ms

/*********************************************************************
 * LOCAL VARIABLES
 *
 * LED Timer State:
 * - led_timeout_clkHandle: ClockP handle for periodic LED timer
 * - led_reset: Flag to prevent starting timer if already active
 * - periodic: Timer mode control:
 *   * 0 = One-shot (single timeout, LED off)
 *   * 1 = External stop (continues until audio_done/pairing_done)
 *   * N = Fixed cycles (runs for N toggle cycles)
 * - periods: Counter for number of toggle cycles completed
 * - audio_done: External flag from voice module to stop PTT blinking
 * - pairing_done: External flag from pairing module to stop pairing blink
 *********************************************************************/
static ClockP_Handle led_timeout_clkHandle;
static uint8_t led_reset;
static uint8_t periodic;
static int8_t periods;
static uint8_t led_skip_cb;
extern uint8_t audio_done;
extern uint8_t pairing_done;

static void LED_timerTrigger(uint16_t time);
static void LED_timerOnStart(uint16_t time);

/*********************************************************************
 * @fn      LED_KeyPress
 *
 * @brief   Handle LED patterns for key press events
 *
 * @param   led_event - Key press event type
 *
 * @return  None
 *
 * @note    Per Google ATV PRD v1.5:
 *          - BLE key: GREEN 100ms
 *          - IR key: ORANGE (GREEN+RED) 100ms
 *          - Unpaired key: RED 100ms
 *********************************************************************/
void LED_KeyPress(LED_Event_t led_event)
{
    /* Check if timer is busy - prevent race condition */
    if (led_reset != 0)
    {
        return;  // Timer busy, ignore this LED request to prevent stuck LED
    }

    switch (led_event)
    {
        case KEY_PRESS_BLE:
            /* LED Green - BLE HID report sent */
            periodic=0; // One-shot mode: timer stops after one timeout
            GPIO_write(CONFIG_GPIO_LED_GREEN,1);
        break;

        case KEY_PRESS_IR:
            /* LED Orange - IR code transmitted (GREEN + RED) */
            periodic=0; // One-shot mode: timer stops after one timeout
            GPIO_write(CONFIG_GPIO_LED_GREEN,1);
            GPIO_write(CONFIG_GPIO_LED_RED,1);
        break;

        case KEY_PRESS_NO_PAIR:
            /* LED Red - Not paired/connected */
            periodic=0; // One-shot mode: timer stops after one timeout
            GPIO_write(CONFIG_GPIO_LED_RED,1);
        break;

        default:
        break;
    }

    /* Trigger 100ms LED On Timer */
    LED_timerTrigger(100);

}

/*********************************************************************
 * @fn      LED_Voice
 *
 * @brief   Handle LED patterns for voice interaction events
 *
 * @param   led_event - Voice event type
 *
 * @return  None
 *
 * @note    Per Google ATV PRD v1.5:
 *          - On-request: Static GREEN (host-initiated, manual control)
 *          - HTT: Static GREEN (button-initiated, manual control)
 *          - PTT: Blinking GREEN 500ms ON/OFF (button-initiated, continues until timeout)
 *
 *          Note: Current implementation uses KEY_VOICE_PTT_ON for HTT mode
 *          with blinking LED for better visibility (non-standard but acceptable).
 *          Standard HTT should use KEY_VOICE_HTT_ON with static LED.
 *********************************************************************/
void LED_Voice(LED_Event_t led_event)
{
    switch (led_event)
    {
        case KEY_VOICE_ONREQ_ON:
            /* LED Voice On-request - Static GREEN (no timer, always allowed) */
            GPIO_write(CONFIG_GPIO_LED_GREEN,1);
            /* No timer needed - manual control via KEY_VOICE_ONREQ_OFF */
        break;

        case KEY_VOICE_ONREQ_OFF:
            /* LED Voice On-request OFF - Turn off manually (always allowed) */
            GPIO_write(CONFIG_GPIO_LED_GREEN,0);
        break;

        case KEY_VOICE_HTT_ON:
            /* LED Voice HTT - Static GREEN (no timer, always allowed) */
            GPIO_write(CONFIG_GPIO_LED_GREEN,1);
            /* No timer needed - manual control via KEY_VOICE_HTT_OFF */
        break;

        case KEY_VOICE_HTT_OFF:
            /* LED Voice HTT OFF - Turn off manually (always allowed) */
            GPIO_write(CONFIG_GPIO_LED_GREEN,0);
        break;

        case KEY_VOICE_PTT_ON:
            /* Voice has priority - stop any running timer to prevent blocking */
            if (led_reset != 0)
            {
                led_skip_cb = 1;
                ClockP_stop(led_timeout_clkHandle);
                ClockP_delete(led_timeout_clkHandle);
                periods = 0;
                /* Keep led_reset=1 to block other events during transition */
            }
            else
            {
                led_reset = 1;  // Claim timer lock if not already held
            }

            /* Clear LEDs and prepare for PTT blinking */
            GPIO_write(CONFIG_GPIO_LED_GREEN, 0);
            GPIO_write(CONFIG_GPIO_LED_RED, 0);

            /* LED Voice PTT - Blinking GREEN 500ms ON/OFF */
            GPIO_write(CONFIG_GPIO_LED_GREEN,1);
            periodic=1; // External stop mode: continues until audio_done flag set

            /* Start new timer (led_reset already =1, LED_timerTrigger will handle) */
            LED_timerOnStart(500);
        break;

        default:
        break;
    }

}

/*********************************************************************
 * @fn      LED_bleStatus
 *
 * @brief   Handle LED patterns for BLE connection status and OTA events
 *
 * @param   led_event - BLE status event type
 *
 * @return  None
 *
 * @note    Per Google ATV PRD v1.5:
 *          - Pairing: Blinking GREEN 2Hz (250ms ON/OFF) until paired
 *          - OTA in progress: Static RED (manual control)
 *          - Success (pairing/OTA/factory reset): 2x blink GREEN 100ms ON/OFF
 *          - Error/Timeout: 4x blink RED 50ms ON/OFF
 *********************************************************************/
void LED_bleStatus(LED_Event_t led_event)
{
    switch (led_event)
    {
        case KEY_BLE_PAIRING:
            /* Pairing has priority - stop any running timer to prevent blocking */
            if (led_reset != 0)
            {
                led_skip_cb = 1;
                ClockP_stop(led_timeout_clkHandle);
                ClockP_delete(led_timeout_clkHandle);
                periods = 0;
                /* Keep led_reset=1 to block other events during transition */
            }
            else
            {
                led_reset = 1;  // Claim timer lock if not already held
            }

            /* Clear LEDs and prepare for pairing blink */
            GPIO_write(CONFIG_GPIO_LED_GREEN, 0);
            GPIO_write(CONFIG_GPIO_LED_RED, 0);

            /* LED Pairing - Blink at 2Hz (250ms ON/250ms OFF) */
            GPIO_write(CONFIG_GPIO_LED_GREEN,1);
            periodic=1; // External stop mode: continues until pairing_done flag set

            /* Start new timer (led_reset already =1) */
            LED_timerOnStart(250);  // 2Hz = 500ms period (250ms ON, 250ms OFF)
        break;

        case KEY_BLE_OAD_START:
            /* LED OTA in progress - Static RED (no timer, always allowed) */
            GPIO_write(CONFIG_GPIO_LED_RED,1);
            /* No timer needed - manual control via KEY_BLE_OAD_FINISH */
        break;

        case KEY_BLE_OAD_FINISH:
            /* LED OTA finish - Turn off RED LED (always allowed) */
            GPIO_write(CONFIG_GPIO_LED_RED,0);
        break;

        case KEY_SUCCESS:
            /* Check if timer is busy - prevent race condition */
            if (led_reset != 0)
            {
                return;  // Timer busy, ignore this request to prevent stuck LED
            }
            /* LED Success - 2x blink GREEN 100ms ON/OFF (total 400ms) */
            GPIO_write(CONFIG_GPIO_LED_GREEN,1);
            periodic=4; // Fixed cycles mode: 4 toggles = 2 complete ON/OFF cycles
            LED_timerTrigger(100);
        break;

        case KEY_FAIL:
            /* Check if timer is busy - prevent race condition */
            if (led_reset != 0)
            {
                return;  // Timer busy, ignore this request to prevent stuck LED
            }
            /* LED Error - 4x blink RED 50ms ON/OFF (total 400ms) */
            GPIO_write(CONFIG_GPIO_LED_RED,1);
            periodic=8; // Fixed cycles mode: 8 toggles = 4 complete ON/OFF cycles
            LED_timerTrigger(50);
        break;

        case KEY_TIMEOUT:
            /* Check if timer is busy - prevent race condition */
            if (led_reset != 0)
            {
                return;  // Timer busy, ignore this request to prevent stuck LED
            }
            /* LED Timeout - 4x blink RED 50ms ON/OFF (total 400ms) */
            GPIO_write(CONFIG_GPIO_LED_RED,1);
            periodic=8; // Fixed cycles mode: 8 toggles = 4 complete ON/OFF cycles
            LED_timerTrigger(50);
        break;

        default:
        break;
    }

}

/*********************************************************************
 * @fn      led_timeout_clkHandleCB
 *
 * @brief   LED timer callback - toggles LEDs and manages timer lifecycle
 *
 * @param   None
 *
 * @return  None
 *
 * @note    This callback implements three LED timer modes:
 *
 *          1. One-shot (periodic=0):
 *             - First call toggles LED OFF
 *             - Timer stops immediately
 *             - Used for: key press feedback (100ms ON)
 *
 *          2. External stop (periodic=1):
 *             - Toggles LED every callback (creates blink pattern)
 *             - Continues until external flag (audio_done/pairing_done) set to 1
 *             - Used for: PTT voice (500ms), pairing (250ms)
 *
 *          3. Fixed cycles (periodic=N, N>1):
 *             - Toggles LED every callback
 *             - Stops after N toggles (N/2 complete blink cycles)
 *             - Used for: success (4 toggles = 2 blinks), error (8 toggles = 4 blinks)
 *
 *          All modes clean up timer resources and reset LEDs to OFF when stopping.
 *********************************************************************/
static void led_timeout_clkHandleCB(void)
{

    /* Stale callback guard if led_reset is already 0 a previous cadllback already
       stopped the timer and cleaned up. This is a queued duplicate that arrived after
       ClokP_delete (BLE task dalayed).*/
    if (led_reset == 0)
    {
        return;
    }
    /* Stale callback guard for PTT/pairing timer transitions. The delete path sets
       led_skip_cb before ClockP_delete(). The first callback to arrive after that consumed
       this flag and exits, preventing a spurious LED toggle before the replacement timers callabck.*/
    if (led_skip_cb)
    {
        led_skip_cb = 0;
        return;
    }

    // Toggle both LEDs (only active LED will change state)
    GPIO_toggle(CONFIG_GPIO_LED_GREEN);
    GPIO_toggle(CONFIG_GPIO_LED_RED);

    // Mode 1: One-shot OR Mode 2: External stop condition met
    if ((periodic == 0) || (audio_done == 1) || (pairing_done == 1))
    {
        /* Stop and Destroy Clock instance */
        ClockP_stop(led_timeout_clkHandle);
        ClockP_delete(led_timeout_clkHandle);
        led_reset = 0;
        audio_done = 0;
        pairing_done = 0;
        GPIO_write(CONFIG_GPIO_LED_GREEN, 0);
        GPIO_write(CONFIG_GPIO_LED_RED, 0);
        return;
    }

    // Mode 3: Fixed cycles - stop after N toggles
    if ((periodic != 1) && (periods >= periodic))
    {
        /* Stop and Destroy Clock instance */
        ClockP_stop(led_timeout_clkHandle);
        ClockP_delete(led_timeout_clkHandle);
        led_reset = 0;
        periods = 0;
        GPIO_write(CONFIG_GPIO_LED_GREEN, 0);
        GPIO_write(CONFIG_GPIO_LED_RED, 0);
        return;
    }
    periods++;
}

/*********************************************************************
 * @fn      LED_timerOnStart
 *
 * @brief   Initialize and start LED periodic timer
 *
 * @param   time - Timer period in milliseconds
 *
 * @return  None
 *
 * @note    Creates a periodic ClockP timer that calls led_timeout_clkHandleCB
 *          at the specified interval. The timer is configured to start immediately.
 *********************************************************************/
static void LED_timerOnStart(uint16_t time)
{

    static ClockP_Params clockpParams;
    static ClockP_Struct led_timeout_clkStruct;
    ClockP_Params_init(&clockpParams);
    uint32_t clockTicks = (uint32_t)((time * (uint16_t)CLOCK_UNITS_MS)); // Convert ms to ticks
    clockpParams.period = clockTicks; // Config for periodic timer
    clockpParams.startFlag = true; // Start immediately
    clockpParams.arg = (uintptr_t)led_timeout_clkHandleCB;
    led_timeout_clkHandle = ClockP_construct(&led_timeout_clkStruct, (void *)BLEAppUtil_invokeFunctionNoData, clockTicks, &clockpParams);
}

/*********************************************************************
 * @fn      LED_timerTrigger
 *
 * @brief   Trigger LED timer if not already running
 *
 * @param   time - Timer period in milliseconds
 *
 * @return  None
 *
 * @note    Prevents starting a new timer if one is already active (led_reset=1).
 *          This ensures only one LED pattern runs at a time.
 *********************************************************************/
static void LED_timerTrigger(uint16_t time)
{

    /* Make sure we don't start the clock timer while already activated */
    if (led_reset == 0)
    {
        led_reset=1;
        LED_timerOnStart(time);
    }
}
