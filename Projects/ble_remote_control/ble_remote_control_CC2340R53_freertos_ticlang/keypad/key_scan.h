/******************************************************************************

@file  key_scan.h

@brief This file contains the application main functionality

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
#include <stdbool.h>
#include <ti/drivers/GPIO.h>
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti_drivers_config.h"

/*********************************************************************
 * TIMING CONSTANTS - Balanced for Reliability
 *
 * Debouncing Configuration:
 * - KEY_PERIOD_MS: Timer period for debounce sampling (10ms)
 * - KEY_COUNT_DEBOUNCE: Number of stable samples required (3 samples)
 * - Total debounce time: 10ms × 3 = 30ms
 *
 * Rationale:
 * - 30ms debounce is reliable for mechanical switches (typical bounce: 5-20ms)
 * - Previous 50ms was too slow and had no queue (missed keys during BLE TX)
 * - Previous 15ms was too fast (spurious KEY_NONE events from bounce)
 * - 30ms + queue system = fast response + no missed keys
 *********************************************************************/
#define KEY_PERIOD_MS               10                // 10ms timer period
#define CLOCK_UNITS_MS              1000              // Convert to milliseconds
#define KEY_COUNT_DEBOUNCE          3                 // 30ms total (3 × 10ms samples)

/* Multi Key Pressing Delays */
#define KEY_PRESS_1_SEC_COUNT       1000/KEY_PERIOD_MS
#define KEY_PRESS_4_SEC_COUNT       4000/KEY_PERIOD_MS
#define KEY_PRESS_30_SEC_COUNT      30000/KEY_PERIOD_MS

typedef enum combokey_mode
{
   MODE_REPAIR     = 0x00,  /* */
   MODE_CLEARBOND  = 0x01, 
   MODE_BUGREPORT  = 0x02,  
   MODE_GOOGLEACC  = 0x03, 
   MODE_COUCH      = 0x04, 
} combokey_mode_t;

// Define keypad configuration
#define NUM_ROWS 6
#define NUM_COLS 7

#define CONFIG_KEY_COL_1                      25
#define CONFIG_KEY_COL_2                      0
#define CONFIG_KEY_COL_3                      1
#define CONFIG_KEY_COL_4                      2
#define CONFIG_KEY_COL_5                      5
#define CONFIG_KEY_COL_6                      6
#define CONFIG_KEY_COL_7                      7

#define CONFIG_KEY_ROW_1                      9
#define CONFIG_KEY_ROW_2                      10
#define CONFIG_KEY_ROW_3                      8
#define CONFIG_KEY_ROW_4                      15
#define CONFIG_KEY_ROW_5                      20
#define CONFIG_KEY_ROW_6                      19

// Define keypad key structure
typedef struct {
    uint8_t row;
    uint8_t col;
} KeypadKey;

// Define key pressed structure
typedef struct {
    KeypadKey pressedKeys[NUM_ROWS * NUM_COLS];
    uint8_t key_count;
} PressedKeys;

/*********************************************************************
 * KEY EVENT QUEUE STRUCTURES
 *
 * Queue-based buffering system to prevent missed keys during processing.
 *
 * Problem Solved:
 * - BLE HID transmission can take 10-50ms depending on connection interval
 * - LED processing, IR transmission, combo key detection add more delay
 * - Without queuing, keys pressed during this time are lost
 *
 * Solution:
 * - Interrupt handler scans matrix and enqueues event (~2ms)
 * - Re-enables interrupts immediately
 * - Main task dequeues and processes events asynchronously
 * - Up to 8 rapid key events can be buffered
 *********************************************************************/

// Key event structure for queue (includes action type)
typedef struct {
    PressedKeys keys;       // Which keys were pressed (row/col positions)
    uint8_t action_type;    // 1 = press, 0 = release
} KeyEvent;

// Key event queue (circular buffer / FIFO)
#define KEY_QUEUE_SIZE 8  // Can hold up to 8 rapid key events
typedef struct {
    KeyEvent events[KEY_QUEUE_SIZE];  // Ring buffer storage
    volatile uint8_t head;             // Write position (producer: interrupt)
    volatile uint8_t tail;             // Read position (consumer: main task)
    volatile uint8_t count;            // Number of events in queue (0-8)
} KeyEventQueue;

// Define keypad configuration structure
typedef struct {
    uint8_t rowPins[NUM_ROWS];
    uint8_t colPins[NUM_COLS];
    uint32_t debounceDelay_us;
    uint8_t max_pressed_keys;
} KeypadConfig;

extern KeypadConfig keypadConfig;
extern uint8_t peripheralAdvHandle_1;
extern uint8_t conn_established;
extern uint8_t adv_finished;

// Couch mode flag - when set, stop sending HID/IR reports but stay connected
extern uint8_t couch_mode_active;

// Initialize keypad driver
void Keypad_init(void);

// Read the currently pressed key, returns true if a key is pressed
uint8_t Keypad_read(PressedKeys *keys);

/*********************************************************************
 * KEY EVENT QUEUE API
 *
 * Thread-safety notes:
 * - Single producer (interrupt context): enqueue
 * - Single consumer (task context): dequeue
 * - No mutex needed for single-producer-single-consumer FIFO
 * - volatile keyword ensures memory visibility between interrupt and task
 *********************************************************************/

/*********************************************************************
 * @fn      KeyQueue_init
 *
 * @brief   Initialize key event queue to empty state
 *
 * @return  None
 *
 * @note    Must be called during system initialization before key scanning starts.
 *********************************************************************/
void KeyQueue_init(void);

/*********************************************************************
 * @fn      KeyQueue_isEmpty
 *
 * @brief   Check if queue has any events
 *
 * @return  1 if queue is empty, 0 if events are available
 *********************************************************************/
uint8_t KeyQueue_isEmpty(void);

/*********************************************************************
 * @fn      KeyQueue_isFull
 *
 * @brief   Check if queue is at capacity
 *
 * @return  1 if queue is full (8 events), 0 if space available
 *
 * @note    If queue fills, new events are dropped (shouldn't happen with fast processing).
 *********************************************************************/
uint8_t KeyQueue_isFull(void);

/*********************************************************************
 * @fn      KeyQueue_enqueue
 *
 * @brief   Add key event to queue (producer - called from interrupt)
 *
 * @param   event - Pointer to key event to enqueue
 *
 * @return  1 if successful, 0 if queue full (event dropped)
 *
 * @note    Called from interrupt context after matrix scan completes.
 *          Non-blocking operation (~10 CPU cycles).
 *********************************************************************/
uint8_t KeyQueue_enqueue(KeyEvent *event);

/*********************************************************************
 * @fn      KeyQueue_dequeue
 *
 * @brief   Remove and return key event from queue (consumer - called from task)
 *
 * @param   event - Pointer to store dequeued event
 *
 * @return  1 if successful, 0 if queue empty
 *
 * @note    Called from main task context (Hid_keypadTask).
 *          Processes events in FIFO order (first pressed, first processed).
 *********************************************************************/
uint8_t KeyQueue_dequeue(KeyEvent *event);

void Combokey_multipressTimerStart(combokey_mode_t combokey_type);

/* Cache Power Key - set when Power is pressed while not connected and cache is enabled */
extern bool pendingPowerKey;

/* Replay cached Power key HID report after BLE reconnection */
void Keypad_replayCachedPowerKey(void);