/******************************************************************************

@file  power_shutdown.c

@brief Power management and shutdown control for battery conservation

This file implements low-power shutdown mode for the remote control to maximize
battery life. The shutdown mode is triggered after extended periods of inactivity,
with different timeout thresholds for connected vs advertising states.

Power Management Strategy:
1. Connected Mode (15-minute timeout):
   - Remote is connected to Android TV host
   - After 15 minutes of inactivity, disconnects then enters shutdown
   - Any key press restarts the inactivity timer
   - Maintains BLE connection during active use

2. Advertising Mode (15-minute timeout):
   - Remote is advertising but not connected (pairing mode)
   - After 15 minutes of inactivity, enters shutdown directly
   - No BLE connection to gracefully terminate

Shutdown Mode Behavior:
- All peripherals powered down except wake-capable GPIO
- IO latches maintained to preserve keypad column state
- Wake-on-keypress: Any key press wakes from shutdown
- Row pins configured as shutdown wake sources (high-level trigger)
- Column pins driven high to enable wake detection
- Ultra-low power consumption (~1-2 µA)

Wake-up Process:
1. Hardware detects key press (row pin goes high)
2. System boots from reset
3. Application initializes normally
4. Keypad interrupt triggers key scan
5. User sees immediate response

Integration:
- Keypad module: Any key press resets inactivity timer via RestartShutdownClock()
- BLE stack: Graceful disconnection before shutdown in connected mode
- Power driver: PowerCC23X0_notify() and Power_shutdown() API

Per Google ATV spec, this implements battery-saving shutdown with wake-on-keypress
to achieve >1 year battery life under typical usage patterns.

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

#include "shutdown/power_shutdown.h"
#include <ti/drivers/Power.h>
#include "keypad/key_scan.h"
#include "infrared/IR.h"
#include "nvs_flash/dev_info_flash.h"

/*********************************************************************
 * CONSTANTS
 *********************************************************************/
#define RC_IDLE_CONN_TIMEOUT         900000  // Connected mode timeout: 15 minutes
#define CLOCK_UNITS_MS               1000   // Convert to milliseconds

/*********************************************************************
 * LOCAL VARIABLES
 *
 * Inactivity Timers:
 * - rc_timeout_clkHandle_conn: Timeout timer for connected mode
 * - rc_timeout_clkHandle_adv: Timeout timer for advertising mode
 * - connHandle: BLE connection handle (external from app_connection.c)
 *
 * These timers track user inactivity and trigger shutdown after 15 minutes.
 * Only one timer is active at a time depending on BLE state.
 *********************************************************************/
ClockP_Handle rc_timeout_clkHandle_conn;
ClockP_Handle rc_timeout_clkHandle_adv;

extern uint16_t connHandle;

/*********************************************************************
 * @fn      RestartShutdownClock
 *
 * @brief   Reset inactivity timer for connected mode
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Called by keypad module on every key press to prevent shutdown.
 *          Resets 15-minute countdown timer.
 *          Only affects connected mode timer (rc_timeout_clkHandle_conn).
 *
 *          Integration: Called from keypad.c after each key event processed.
 *********************************************************************/
void RestartShutdownClock(void)
{
    ClockP_stop(rc_timeout_clkHandle_conn);
    int32_t clockTicks = RC_IDLE_CONN_TIMEOUT * (CLOCK_UNITS_MS);
    ClockP_setTimeout(rc_timeout_clkHandle_conn, clockTicks);
    ClockP_start(rc_timeout_clkHandle_conn);
}

/*********************************************************************
 * @fn      TriggerShutdown
 *
 * @brief   Enter low-power shutdown mode with wake-on-keypress
 *
 * @param   None
 *
 * @return  None (does not return - system enters shutdown)
 *
 * @note    Shutdown Preparation Sequence:
 *          1. Drive all keypad columns HIGH (enables wake detection)
 *          2. Configure all keypad rows as:
 *             - Input with pull-down (GPIO_CFG_IN_PD)
 *             - Shutdown wake source on HIGH level (GPIO_CFG_SHUTDOWN_WAKE_HIGH)
 *          3. Clear any pending row interrupts
 *          4. Notify power driver of impending shutdown
 *          5. Enter shutdown mode (Power_shutdown)
 *
 *          Wake-up Behavior:
 *          - Any key press drives a row HIGH through column
 *          - Row wake source triggers system reset
 *          - System boots normally and resumes operation
 *
 *          Power Consumption:
 *          - Shutdown mode: ~1-2 µA
 *          - IO latches maintained to preserve column state
 *          - Only row wake detection circuits remain powered
 *********************************************************************/

/* Maps Table 3.2 wakeup mask bit positions to keypad row indices.
 * Row index corresponds to keypadConfig.rowPins[] (0=ROW_1 ... 5=ROW_6).
 * Derived from the key matrix in keypad.c (callbackKeySelection). */
static const uint8_t wakeupBitToRow[13] = {
    0,  /* Bit  0: Assistant → Row 0 (ROW_1) */
    3,  /* Bit  1: Home      → Row 3 (ROW_4) */
    0,  /* Bit  2: Power     → Row 0 (ROW_1) */
    0,  /* Bit  3: Input     → Row 0 (ROW_1) */
    5,  /* Bit  4: YouTube   → Row 5 (ROW_6) */
    5,  /* Bit  5: Netflix   → Row 5 (ROW_6) */
    5,  /* Bit  6: APP03     → Row 5 (ROW_6) */
    5,  /* Bit  7: APP04     → Row 5 (ROW_6) */
    1,  /* Bit  8: Blue      → Row 1 (ROW_2) */
    1,  /* Bit  9: Green     → Row 1 (ROW_2) */
    1,  /* Bit 10: Red       → Row 1 (ROW_2) */
    1,  /* Bit 11: Yellow    → Row 1 (ROW_2) */
    4,  /* Bit 12: FreeTV    → Row 4 (ROW_5) */
};

void TriggerShutdown(void)
{
    /* Step 1: Drive all keypad columns HIGH to enable wake detection */
    GPIO_write(CONFIG_KEY_COL_1, 1);
    GPIO_write(CONFIG_KEY_COL_2, 1);
    GPIO_write(CONFIG_KEY_COL_3, 1);
    GPIO_write(CONFIG_KEY_COL_4, 1);
    GPIO_write(CONFIG_KEY_COL_5, 1);
    GPIO_write(CONFIG_KEY_COL_6, 1);
    GPIO_write(CONFIG_KEY_COL_7, 1);

    /* Step 2: Determine which rows to enable as shutdown wake sources */
    bool rowWakeEnabled[NUM_ROWS] = {false};

    if (g_rcConfig.loaded && g_rcConfig.wakeupMask.isValid)
    {
        /* Combine both masks - treat 0xFFFF (disabled) as no contribution */
        uint16_t activeMask = 0;
        if (g_rcConfig.wakeupMask.wakeUpKey1 != 0xFFFF) activeMask |= g_rcConfig.wakeupMask.wakeUpKey1;
        if (g_rcConfig.wakeupMask.wakeUpKey2 != 0xFFFF) activeMask |= g_rcConfig.wakeupMask.wakeUpKey2;

        if (activeMask != 0)
        {
            /* Enable only rows that contain a key present in the active mask */
            for (int bit = 0; bit < 13; bit++)
            {
                if (activeMask & (1 << bit))
                {
                    rowWakeEnabled[wakeupBitToRow[bit]] = true;
                }
            }
        }
        else
        {
            /* Both masks were 0xFFFF (feature disabled) - wake from any key */
            for (int i = 0; i < NUM_ROWS; i++) rowWakeEnabled[i] = true;
        }
    }
    else
    {
        /* Config not loaded or invalid - fall back to all rows */
        for (int i = 0; i < NUM_ROWS; i++) rowWakeEnabled[i] = true;
    }

    /* Step 3: Configure rows - wake source only for rows with active wakeup keys */
    for (int i = 0; i < NUM_ROWS; i++)
    {
        uint8_t rowPin = keypadConfig.rowPins[i];
        GPIO_clearInt(rowPin);
        if (rowWakeEnabled[i])
        {
            GPIO_setConfig(rowPin, GPIO_CFG_IN_PD | GPIO_CFG_SHUTDOWN_WAKE_HIGH);
        }
        else
        {
            GPIO_setConfig(rowPin, GPIO_CFG_IN_PD);
        }
    }

    /* Step 4: Notify power driver and enter shutdown */
    int_fast16_t Status = PowerCC23X0_notify(PowerLPF3_ENTERING_SHUTDOWN);

    if (Status == Power_SOK)
    {
        /* shutdownTime is always 0 (indefinite) - CC23xx shutdown only supports
         * GPIO wakeup sources. The NVS periodicWakeupSwitch / periodicWakeupInterval
         * fields (BleConfig_t) are read and stored in g_rcConfig but not acted on,
         * as there is no RTC or timer wakeup path from full shutdown on this device. */
        Power_shutdown(0, 0);
    }
}

/*********************************************************************
 * @fn      triggerDisconnect
 *
 * @brief   Gracefully disconnect BLE connection before shutdown
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Called when connected mode inactivity timer expires.
 *          Sends BLE disconnect request with reason HCI_DISCONNECT_REMOTE_USER_TERM.
 *          After disconnection completes, app_connection.c triggers TriggerShutdown().
 *********************************************************************/
void triggerDisconnect(void)
{
    GAP_TerminateLinkReq(connHandle, HCI_DISCONNECT_REMOTE_USER_TERM);
}

/*********************************************************************
 * @fn      rc_timeout_clkHandleCB_conn
 *
 * @brief   Connected mode inactivity timer callback
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Called after 15 minutes of inactivity in connected mode.
 *          Triggers graceful BLE disconnection, which will lead to shutdown.
 *********************************************************************/
static void rc_timeout_clkHandleCB_conn(void)
{
    //triggerDisconnect()
    GAP_TerminateLinkReq(connHandle, HCI_DISCONNECT_REMOTE_USER_TERM);
}

/*********************************************************************
 * @fn      ConfigPowerShutdownConn_start
 *
 * @brief   Start connected mode inactivity timer (15 minutes)
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Called when BLE connection is established.
 *          Timer is restarted on every key press via RestartShutdownClock().
 *          One-shot timer - expires after RC_IDLE_CONN_TIMEOUT (15 min).
 *********************************************************************/
void ConfigPowerShutdownConn_start(void)
{
    ClockP_Params clockpParams;
    static ClockP_Struct rc_timeout_clkStructConn;
    ClockP_Params_init(&clockpParams);
    uint32_t clockTicks = RC_IDLE_CONN_TIMEOUT * (CLOCK_UNITS_MS); // 15 minutes
    clockpParams.period = clockTicks; // One-shot timer (period = timeout for one-shot)
    clockpParams.startFlag = true; // Start immediately
    clockpParams.arg = (uintptr_t)rc_timeout_clkHandleCB_conn;
    rc_timeout_clkHandle_conn = ClockP_construct(&rc_timeout_clkStructConn, (void *)BLEAppUtil_invokeFunctionNoData, clockTicks, &clockpParams);
}