/******************************************************************************

@file  key_scan.c

@brief Keypad matrix scanning with combo key detection and couch mode support

This file implements a keypad matrix scanning system with interrupt-driven
debouncing, combo key detection, and special mode support for Google ATV remote control.

Key Features:
1. Matrix Scanning:
   - 6 rows (inputs with pull-down, interrupt-enabled)
   - 5 columns (outputs driven high one at a time)
   - Debouncing with 50ms period and configurable count threshold
   - Interrupt-driven for power efficiency

2. Combo Key Detection (per Google ATV PRD):
   - Pairing Mode: [BACK]+[HOME] for >4 seconds
   - Factory Reset: [DPAD CENTER]+[MUTE] for >4 seconds
   - Bug Report: [DPAD CENTER]+[BACK] for >1 second (sends 0x96 in IR mode)
   - Accessibility: [DPAD DOWN]+[BACK] for >1 second (sends 0xD0 in IR mode)
   - Couch Mode: Any 3 keys held for >30 seconds (stops transmission, stays connected)

3. Couch Mode:
   - Activated after 30 seconds of continuous key hold
   - Stops sending HID/IR reports to prevent accidental commands
   - Maintains BLE connection
   - Exits on key release

Integration:
- Keypad module (keypad.c): Receives scanned key events for HID/IR transmission
- LED module: Visual feedback for combo key actions
- BLE stack: GAP operations for pairing/disconnection
- Calendar module: Time sync for backlighting scheduled mode

Google ATV Spec Compliance: Android TV Remote Control PRD v1.5 Section 3.5 (Combo Keys)

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

#include "keypad/key_scan.h"
#include "keypad/key_router.h"
#include "led/led.h"
#include <ti/drivers/dpl/ClockP.h>
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ble_app/app_peripheral.h"  // For Peripheral_startPairingModeAdvertising()

/*********************************************************************
 * LOCAL VARIABLES
 *
 * Key State Tracking:
 * - keyState[NUM_ROWS][NUM_COLS]: Matrix of current key states (0=released, 1=pressed)
 *
 * External Dependencies:
 * - Hid_keypadTask(): Keypad module handler for scanned keys
 * - connHandle: BLE connection handle
 * - peripheralAdvHandle_1: BLE advertising handle
 * - conn_established: BLE connection status flag
 * - adv_finished: BLE advertising completion flag
 *
 * Debounce Timer:
 * - key_timeout_clkHandle: ClockP handle for debounce timer (50ms period)
 * - keytimer: Debounce counter (must reach KEY_COUNT_DEBOUNCE threshold)
 * - key_action_type: Key action (1=press, 0=release)
 * - process_pressedKeys: Flag indicating keys are ready to process
 *
 * Combo Key Timer:
 * - combokey_timeout_clkHandle: ClockP handle for combo key timing (50ms period)
 * - combokeytimer: Timer counter for combo key duration (in 50ms increments)
 * - combokey_running: Flag indicating combo key timer is active
 * - combokey_pressed: Current combo key mode being detected
 *
 * Special Modes:
 * - factory_reset: Flag for factory reset trigger
 * - couch_mode_active: Flag indicating couch mode is active (exported to keypad.c)
 *********************************************************************/

// Key state matrix
static uint8_t keyState[NUM_ROWS][NUM_COLS] = {0};

/*********************************************************************
 * KEY EVENT QUEUE
 *
 * Circular buffer for key events to prevent missed keys during processing.
 * - Producer: key_timeout_clkHandleCB (interrupt context) - enqueues events
 * - Consumer: Hid_keypadTask (task context) - dequeues and processes events
 *********************************************************************/
static KeyEventQueue keyQueue;

// Function prototypes
extern void Hid_keypadTask(void);  // Queue-based: no longer takes action_type parameter
extern uint16_t connHandle;
extern uint8_t peripheralAdvHandle_1;
extern uint8_t conn_established;
extern uint8_t adv_finished;
static void keys_press_callback(uint_least8_t index);
static void Key_timerStart(void);
static ClockP_Handle key_timeout_clkHandle;
static ClockP_Handle combokey_timeout_clkHandle;
static uint8_t keytimer;
static uint16_t combokeytimer;
static uint8_t combokey_running;
static uint8_t process_pressedKeys;
static combokey_mode_t combokey_pressed;
static uint16_t timer_count;
static uint8_t key_action_type;
uint8_t factory_reset;
uint8_t couch_mode_active = 0;  // Couch mode flag (exported)

/*********************************************************************
 * BLE ADVERTISING CONFIGURATION
 *
 * advSetStartParamsSet: BLE advertising parameters for pairing mode
 * - Duration: 90 seconds (9000 * 10ms)
 * - Used when entering pairing mode via combo key
 *********************************************************************/
const BLEAppUtil_AdvStart_t advSetStartParamsSet =
{
    /* Use the maximum possible value. This is the spec-defined maximum for */
    /* directed advertising and infinite advertising for all other types */
    .enableOptions         = GAP_ADV_ENABLE_OPTIONS_USE_DURATION,
    .durationOrMaxEvents   = 9000  /* Set Limited Advertisement with 90 seconds */
};

/*********************************************************************
 * @fn      Keypad_init
 *
 * @brief   Initialize keypad matrix GPIO configuration
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Configuration:
 *          - Rows: GPIO inputs with pull-down, hysteresis, and both-edge interrupts
 *          - Columns: GPIO outputs initially set high
 *          - All rows share the same interrupt callback (keys_press_callback)
 *          - Must be called during system initialization before key scanning
 *********************************************************************/
void Keypad_init(void)
{
    // Initialize GPIO
    GPIO_init();

    // Configure keypad rows as inputs with pull-down resistor and interrupts enabled
    for (int i = 0; i < NUM_ROWS; i++)
    {
        GPIO_setConfig(keypadConfig.rowPins[i], GPIO_CFG_IN_PD | GPIO_CFG_HYSTERESIS_ON | GPIO_CFG_INT_BOTH_EDGES_INTERNAL | GPIO_CFG_INT_ENABLE);
        // Set row pins to same callback
        GPIO_setCallback(keypadConfig.rowPins[i], (GPIO_CallbackFxn *)keys_press_callback);
    }

    // Configure keypad columns as outputs
    for (int i = 0; i < NUM_COLS; i++)
    {
        GPIO_setConfig(keypadConfig.colPins[i], GPIO_CFG_OUT_STD);
        // Set columns to high
        GPIO_write(keypadConfig.colPins[i], 1);
    }

    // Initialize key event queue
    KeyQueue_init();
}

/*********************************************************************
 * KEY EVENT QUEUE IMPLEMENTATION
 * Producer: interrupt (enqueue), Consumer: task (dequeue)
 *********************************************************************/

/*********************************************************************
 * @fn      KeyQueue_init
 *
 * @brief   Initialize queue to empty state
 *********************************************************************/
void KeyQueue_init(void)
{
    keyQueue.head = 0;
    keyQueue.tail = 0;
    keyQueue.count = 0;
}

/*********************************************************************
 * @fn      KeyQueue_isEmpty
 *
 * @brief   Check if queue is empty
 *
 * @return  1 if empty, 0 if has events
 *********************************************************************/
uint8_t KeyQueue_isEmpty(void)
{
    return (keyQueue.count == 0);
}

/*********************************************************************
 * @fn      KeyQueue_isFull
 *
 * @brief   Check if queue is full
 *
 * @return  1 if full, 0 if space available
 *********************************************************************/
uint8_t KeyQueue_isFull(void)
{
    return (keyQueue.count >= KEY_QUEUE_SIZE);
}

/*********************************************************************
 * @fn      KeyQueue_enqueue
 *
 * @brief   Add event to queue (producer - interrupt context)
 *
 * @param   event - Pointer to event to enqueue
 *
 * @return  1 if success, 0 if queue full (dropped)
 *********************************************************************/
uint8_t KeyQueue_enqueue(KeyEvent *event)
{
    if (KeyQueue_isFull())
    {
        return 0;  // Queue full, drop event
    }

    // Copy event to queue
    keyQueue.events[keyQueue.head] = *event;

    // Update head pointer (circular)
    keyQueue.head = (keyQueue.head + 1) % KEY_QUEUE_SIZE;
    keyQueue.count++;

    return 1;  // Success
}

/*********************************************************************
 * @fn      KeyQueue_dequeue
 *
 * @brief   Remove event from queue (consumer - task context)
 *
 * @param   event - Pointer to store dequeued event
 *
 * @return  1 if success, 0 if queue empty
 *********************************************************************/
uint8_t KeyQueue_dequeue(KeyEvent *event)
{
    if (KeyQueue_isEmpty())
    {
        return 0;  // Queue empty
    }

    // Copy event from queue
    *event = keyQueue.events[keyQueue.tail];

    // Update tail pointer (circular)
    keyQueue.tail = (keyQueue.tail + 1) % KEY_QUEUE_SIZE;
    keyQueue.count--;

    return 1;  // Success
}

/*********************************************************************
 * @fn      keys_press_callback
 *
 * @brief   GPIO interrupt callback for keypad row state changes
 *
 * @param   index - GPIO pin index that triggered interrupt
 *
 * @return  None
 *
 * @note    Called on both rising and falling edges of any row pin.
 *          Disables all row interrupts to prevent bounce during scanning.
 *          Determines if event is key press (any row high) or release (all rows low).
 *          Starts debounce timer to scan matrix after settling.
 *********************************************************************/
static void keys_press_callback(uint_least8_t index)
{

    // Disable all row interrupts to prevent bouncing during scan
    GPIO_disableInt(CONFIG_KEY_ROW_1);
    GPIO_disableInt(CONFIG_KEY_ROW_2);
    GPIO_disableInt(CONFIG_KEY_ROW_3);
    GPIO_disableInt(CONFIG_KEY_ROW_4);
    GPIO_disableInt(CONFIG_KEY_ROW_5);
    GPIO_disableInt(CONFIG_KEY_ROW_6);

    // Determine action type: press (1) if any row is high, release (0) if all rows low
    if (GPIO_read(CONFIG_KEY_ROW_1) || GPIO_read(CONFIG_KEY_ROW_2) || GPIO_read(CONFIG_KEY_ROW_3) || GPIO_read(CONFIG_KEY_ROW_4) || GPIO_read(CONFIG_KEY_ROW_5) || GPIO_read(CONFIG_KEY_ROW_6))
    {
        key_action_type = 1;  // Key press detected
    }
    else
    {
        key_action_type = 0;  // Key release detected
    }

    // Start debounce timer to scan matrix
    BLEAppUtil_invokeFunctionNoData((void *)Key_timerStart);
}

/*********************************************************************
 * @fn      key_timeout_clkHandleCB
 *
 * @brief   Debounce timer callback - scans keypad matrix
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Called every 50ms during debounce period (KEY_PERIOD_MS).
 *
 *          Matrix Scanning Algorithm:
 *          1. Set all columns to low/high-impedance
 *          2. For each column:
 *             a. Drive column high
 *             b. Read all row pins (1=pressed, 0=not pressed)
 *             c. Store results in keyState[row][col]
 *             d. Return column to low/high-impedance
 *          3. Restore all columns to high for interrupt detection
 *
 *          Debouncing:
 *          - Increments keytimer on each call
 *          - After KEY_COUNT_DEBOUNCE calls, considers keys stable
 *          - Re-enables row interrupts via Hid_keypadTask()
 *
 *          This ensures clean key detection without mechanical bounce artifacts.
 *********************************************************************/
static void key_timeout_clkHandleCB(void)
{
    process_pressedKeys = 0;

    // Phase 1: Prepare for scanning - set all columns to low/high-impedance
    for (int col = 0; col < NUM_COLS; col++)
    {
        GPIO_write(keypadConfig.colPins[col], 0);
        GPIO_setConfig(keypadConfig.colPins[col], GPIO_CFG_NO_DIR);
    }

    // Phase 2: Scan matrix - drive each column high one at a time and read rows
    for (int col = 0; col < NUM_COLS; col++)
    {
        // Drive one column high
        GPIO_setConfig(keypadConfig.colPins[col], GPIO_CFG_OUT_STD);
        GPIO_write(keypadConfig.colPins[col], 1);

        // Read all row pins for this column
        for (int row = 0; row < NUM_ROWS; row++)
        {
            uint8_t currState = GPIO_read(keypadConfig.rowPins[row]);
            keyState[row][col] = currState;  // Store key state
        }

        // Return column to low/high-impedance
        GPIO_write(keypadConfig.colPins[col], 0);
        GPIO_setConfig(keypadConfig.colPins[col], GPIO_CFG_NO_DIR);
    }

    // Phase 3: Restore columns to high for interrupt detection
    for (int col = 0; col < NUM_COLS; col++)
    {
        GPIO_setConfig(keypadConfig.colPins[col], GPIO_CFG_OUT_STD);
        GPIO_write(keypadConfig.colPins[col], 1);
    }

    // Debounce logic: wait for KEY_COUNT_DEBOUNCE stable readings
    if (keytimer >= KEY_COUNT_DEBOUNCE)
    {
        keytimer = 0;
        process_pressedKeys = 1;

        // Create key event from scanned matrix
        KeyEvent event;
        event.action_type = key_action_type;
        event.keys.key_count = 0;

        // Build PressedKeys list from keyState matrix (only for press events)
        if (key_action_type == 1)
        {
            for (int row = 0; row < NUM_ROWS; row++)
            {
                for (int col = 0; col < NUM_COLS; col++)
                {
                    if (keyState[row][col] == 1)
                    {
                        event.keys.pressedKeys[event.keys.key_count].row = row;
                        event.keys.pressedKeys[event.keys.key_count].col = col;
                        event.keys.key_count++;
                    }
                }
            }
        }

        // Enqueue event (fast, ~10 CPU cycles)
        KeyQueue_enqueue(&event);

        ClockP_stop(key_timeout_clkHandle);
        ClockP_delete(key_timeout_clkHandle);

        // Trigger keypad task to process queue (re-enables interrupts)
        Hid_keypadTask();
    }
    else
    {
        keytimer++;  // Continue debouncing
    }
}

/*********************************************************************
 * @fn      Key_timerStart
 *
 * @brief   Start debounce timer for key scanning
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Creates a 50ms periodic timer (KEY_PERIOD_MS) for debouncing.
 *          Called by keys_press_callback on any keypad interrupt.
 *********************************************************************/
static void Key_timerStart(void)
{
    static ClockP_Params clockpParams;
    static ClockP_Struct key_timeout_clkStruct;
    ClockP_Params_init(&clockpParams);
    uint32_t clockTicks = (KEY_PERIOD_MS * CLOCK_UNITS_MS); // 50ms period
    clockpParams.period = clockTicks; // Periodic timer
    clockpParams.startFlag = true; // Start immediately
    clockpParams.arg = (uintptr_t)key_timeout_clkHandleCB;
    key_timeout_clkHandle = ClockP_construct(&key_timeout_clkStruct, (void *)BLEAppUtil_invokeFunctionNoData, clockTicks, &clockpParams);
}

/*********************************************************************
 * @fn      Keypad_read
 *
 * @brief   Read pressed keys from keyState matrix
 *
 * @param   keys - Pointer to PressedKeys structure to fill with results
 *
 * @return  1 if keys are ready to process, 0 otherwise
 *
 * @note    Fills keys structure with:
 *          - key_count: Number of keys pressed simultaneously
 *          - pressedKeys[]: Array of (row, col) positions for each pressed key
 *
 *          Called by keypad module after debounce timer completes.
 *          Clears keyState array after reading to prevent duplicate events.
 *          Limits to keypadConfig.max_pressed_keys simultaneous keys.
 *********************************************************************/
uint8_t Keypad_read(PressedKeys *keys) 
{
    /* Count keys pressed at the same time. */
    uint8_t numKeysPressed = 0;

    if (process_pressedKeys == 1)
    {
        /*  Check for key presses */
        for (int row = 0; row < NUM_ROWS; row++) 
        {
            for (int col = 0; col < NUM_COLS; col++) 
            {
                if (keyState[row][col] == 1) 
                {
                    /* Key is pressed */
                    keys->pressedKeys[numKeysPressed].row = row;
                    keys->pressedKeys[numKeysPressed].col = col;
                    numKeysPressed++;
                    keyState[row][col] = 0; /* Clear the key presses */
                    if (numKeysPressed > keypadConfig.max_pressed_keys)
                    {
                        /* How many keys are pressed at the same time. */
                        keys->key_count = keypadConfig.max_pressed_keys;
                        return 1;
                    }
                }
            }
        }

        /* How many keys pressed at the same time. */
        keys->key_count = numKeysPressed;
        process_pressedKeys = 0;
        return 1;
    }

    return 0;
}

/*********************************************************************
 * @fn      combokey_timeout_clkHandleCB
 *
 * @brief   Combo key timer callback - tracks hold duration and triggers actions
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Called every 50ms while combo keys are held.
 *
 *          Combo Key Modes (per Google ATV PRD v1.5):
 *
 *          1. MODE_REPAIR ([BACK]+[HOME] >4s):
 *             - Terminates current connection OR restarts advertising
 *             - Used for pairing mode / re-pairing
 *
 *          2. MODE_CLEARBOND ([CENTER]+[MUTE] >4s):
 *             - Clears bond cache (erases all bonds)
 *             - Terminates connection
 *             - Factory reset for pairing data
 *
 *          3. MODE_BUGREPORT ([CENTER]+[BACK] >1s):
 *             - Sends 0x96 code in IR mode
 *             - Triggers Android TV bug report
 *
 *          4. MODE_GOOGLEACC ([DOWN]+[BACK] >1s):
 *             - Sends 0xD0 code in IR mode
 *             - Opens Android TV accessibility shortcuts
 *
 *          5. MODE_COUCH (any 3 keys >30s):
 *             - Enters couch mode: stops HID/IR transmission
 *             - Maintains BLE connection
 *             - Exits on key release
 *
 *          If keys are released before threshold, timer stops and no action taken.
 *********************************************************************/
static void combokey_timeout_clkHandleCB(void)
{
    combokeytimer++;
    if (key_action_type == 1)
    {
        switch (combokey_pressed)
        {
            /* Pairing Mode & Re-Pairing: [BACK]+[HOME] for longer than 4s. */
            case MODE_REPAIR:
                if (combokeytimer >= KEY_PRESS_4_SEC_COUNT)
                {
                    combokeytimer = 0;
                    combokey_running = 0;

                    if (conn_established == 1)
                    {
                        // Set flag to force undirected advertising after disconnection
                        forceUndirectedOnNextDisconnect = 1;

                        // Terminate connection - LINK_TERMINATED handler will start undirected advertising
                        GAP_TerminateLinkReq(connHandle, HCI_DISCONNECT_REMOTE_USER_TERM);
                    }
                    else
                    {
                        // Not connected - force undirected advertising for pairing mode
                        // This stops current advertising (if any) and restarts in undirected mode
                        Peripheral_startPairingModeAdvertising();
                    }
                }
                break;
            /* Factory Reset Mode (Clear Bond Cache): [DPAD CENTER]+[MUTE] for longer than 4s. */
            case MODE_CLEARBOND:
                if (combokeytimer >= KEY_PRESS_4_SEC_COUNT)
                {
                    combokeytimer = 0;
                    combokey_running = 0;

                    // Erase all bonds first (bondCount becomes 0)
                    bStatus_t status = GAPBondMgr_SetParameter(GAPBOND_ERASE_ALLBONDS, 0, NULL);

                    if (conn_established == 1) {
                        // Set flag to force undirected advertising after disconnection
                        // (redundant since bondCount=0, but explicit for clarity)
                        forceUndirectedOnNextDisconnect = 1;

                        // Connected: Terminate connection
                        // LINK_TERMINATED handler will start undirected advertising
                        GAP_TerminateLinkReq(connHandle, HCI_DISCONNECT_REMOTE_USER_TERM);
                    }
                    else {
                        // Not connected: Directly restart advertising in undirected mode
                        // Stops current advertising (if any) and restarts as undirected
                        Peripheral_startPairingModeAdvertising();
                    }

                    /* Google ATV Spec Section 2.7: Factory reset - 2x blink GREEN on success, 4x blink RED on fail.
                       However ideally this should be done when the Bonds have been actually erased */
                    if (status == SUCCESS)
                    {
                        LED_bleStatus(KEY_SUCCESS);  // 2x GREEN blinks
                    }
                    else
                    {
                        LED_bleStatus(KEY_FAIL);     // 4x RED blinks
                    }
                }
                break;
            /* Google Bug Report Mode: [DPAD CENTER]+[BACK] for longer than 1s (In IR mode, the sent code data is 0x96.) */
            case MODE_BUGREPORT:
                if (combokeytimer >= KEY_PRESS_1_SEC_COUNT)
                {
                    /* Send IR code 0x96 for Bug Report Mode */
                    KeyRouter_sendComboIrCode(0x96);

                    /* Google ATV Spec Section 2.7: Bug report - 2x blink GREEN confirmation */
                    LED_bleStatus(KEY_SUCCESS);

                    combokeytimer = 0;
                    combokey_running = 0;
                }
                break;
            /* Google Accessibility shortcut: [DPAD DOWN]+[BACK] for longer than 1s (In IR mode, the sent code data is 0xD0.) */
            case MODE_GOOGLEACC:
                if (combokeytimer >= KEY_PRESS_1_SEC_COUNT)
                {
                    /* Send IR code 0xD0 for Accessibility Mode */
                    KeyRouter_sendComboIrCode(0xD0);
                    combokeytimer = 0;
                    combokey_running = 0;
                }
                break;
            /* Couch Mode: All 3 keys held for longer than 30s */
            case MODE_COUCH:
                if (combokeytimer >= KEY_PRESS_30_SEC_COUNT)
                {
                    // Enter couch mode: stop sending HID/IR reports, stay connected
                    couch_mode_active = 1;
                    combokeytimer = 0;
                    combokey_running = 0;
                }
                break;
        }
    }
    else
    {
        /* The button(s) was released, therefore timer is terminated and no action is taken */
        ClockP_stop(combokey_timeout_clkHandle);
        ClockP_delete(combokey_timeout_clkHandle);
        combokeytimer = 0;
        combokey_running = 0;

        // If in couch mode, exit it on key release
        if (couch_mode_active == 1) {
            couch_mode_active = 0;
        }
    }

}

/*********************************************************************
 * @fn      Combokey_multipressTimerStart
 *
 * @brief   Start combo key timer for duration tracking
 *
 * @param   combokey_type - Combo key mode being detected (MODE_REPAIR, MODE_CLEARBOND, etc.)
 *
 * @return  None
 *
 * @note    Called by keypad module when combo key combination is detected.
 *          Starts 50ms periodic timer to track how long keys are held.
 *          Enables buzzer for audible feedback (optional).
 *          Only starts if not already running (prevents nested timers).
 *
 *          Timer continues until:
 *          - Duration threshold is met (action triggered)
 *          - Keys are released (timer stopped, no action)
 *********************************************************************/
void Combokey_multipressTimerStart(combokey_mode_t combokey_type)
{
    if (combokey_running == 0)
    {
        // Buzzer_open(); /* Disabled: Can interfere with LED timers */
        combokey_running = 1;
        combokey_pressed = combokey_type;
        static ClockP_Params clockpParams;
        static ClockP_Struct combokey_timeout_clkStruct;
        ClockP_Params_init(&clockpParams);
        uint32_t clockTicks = (KEY_PERIOD_MS * CLOCK_UNITS_MS); // 50ms period
        clockpParams.period = clockTicks; // Periodic timer
        clockpParams.startFlag = true; // Start immediately
        clockpParams.arg = (uintptr_t)combokey_timeout_clkHandleCB;
        combokey_timeout_clkHandle = ClockP_construct(&combokey_timeout_clkStruct, (void *)BLEAppUtil_invokeFunctionNoData, clockTicks, &clockpParams);
    }
}