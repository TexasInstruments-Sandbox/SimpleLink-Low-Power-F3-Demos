/******************************************************************************

@file  keypad.c

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

/*********************************************************************
 * OOBE (OUT-OF-BOX EXPERIENCE) IMPLEMENTATION
 *
 * Per Google ATV specification, this file implements automatic IR-only
 * mode during Out-of-Box Experience (first boot before pairing).
 *
 * Implementation:
 * ---------------
 * All key functions check the conn_established flag to determine mode:
 *
 * - conn_established = 0 (OOBE/First Boot):
 *   → IR-Only Mode: All key presses send IR codes
 *   → LED feedback: Orange (KEY_PRESS_IR)
 *   → Duration: 10-minute advertising window for pairing
 *
 * - conn_established = 1 (After Pairing):
 *   → BLE Mode: All key presses send BLE HID reports
 *   → LED feedback: Green (KEY_PRESS_BLE)
 *   → Full functionality active
 *
 * No special OOBE mode handling required - automatic switching based
 * on BLE connection state provides seamless OOBE experience.
 *********************************************************************/

/* Include Application Drivers */
#include "voice/voice_processing.h"
#include "voice/voice_interaction.h"
#include "shutdown/power_shutdown.h"
#include "keypad/key_scan.h"
#include "keypad/key_router.h"
#include "infrared/IR.h"
#include "backlight/backlight.h"
#include "fmr/fmr.h"
#include "led/led.h"
#include "ble_hid/hiddev.h"
#include "nvs_flash/dev_info_flash.h"

// External couch mode flag from key_scan.c
extern uint8_t couch_mode_active;

/* Define Key Local Functions */
static void Hid_keyInput();
static void Hid_keyPower();
static void Hid_key1();
static void Hid_key2();
static void Hid_key3();
static void Hid_key4();
static void Hid_key5();
static void Hid_key6();
static void Hid_key7();
static void Hid_key8();
static void Hid_key9();
static void Hid_keySubtitles();
static void Hid_key0();
static void Hid_keyInfo();
static void Hid_keyRed();
static void Hid_keyGreen();
static void Hid_keyYellow();
static void Hid_keyBlue();
static void Hid_keyUserProfile();
static void Hid_keyAssitant();
static void Hid_keyDashboard();
static void Hid_keyDPadUp();
static void Hid_keyDPadLeft();
static void Hid_keyDPadCenter();
static void Hid_keyDPadRight();
static void Hid_keyDPadDown();
static void Hid_keyBack();
static void Hid_keyHome();
static void Hid_keyGuide();
static void Hid_keyFreeTV();
static void Hid_keyVolumeUp();
static void Hid_keyProgramUp();
static void Hid_keyMute();
static void Hid_keyVolumeDown();
static void Hid_keyProgramDown();
static void Hid_keyYoutube();
static void Hid_keyNetflix();
static void Hid_keyApp03();
static void Hid_keyApp04();
static void Hid_keyApp05();
static void Hid_keyApp06();
static void Hid_buttonReleased();
static void Hid_keyG21();

typedef void (*keyfunction)(void);

/* Define Global Variables */
KeypadConfig keypadConfig;
extern uint8_t conn_established;
extern uint16_t connHandle;  // Global connection handle from app_connection.c
uint8_t audio_key_enabled;  // Exported for voice_interaction.c
uint8_t audio_done;
bool pendingPowerKey = false;

/*********************************************************************
 * KEY STATE TRACKING
 *
 * Tracks the last known key state to ensure proper press→release transitions.
 *
 * Problem: With fast debounce (10ms), electrical noise can cause:
 *          - Multiple spurious release events
 *          - Missed release events (causing stuck keys)
 *          - Press events that look like releases
 *
 * Solution: Track state transitions:
 *           - IDLE: No key pressed, no KEY_NONE needed
 *           - PRESSED: Key is down, KEY_NONE must be sent on release
 *           - State changes only on actual transitions, not bounces
 *********************************************************************/
typedef enum {
    KEY_STATE_IDLE = 0,      // No key pressed (KEY_NONE already sent or initial state)
    KEY_STATE_PRESSED = 1    // Key pressed (KEY_NONE needed on release)
} KeyState_t;

static KeyState_t current_key_state = KEY_STATE_IDLE;

PDEIRGENLPF3_Transaction pdeIrTrans;
extern PDEIRLPF3_Handle pdeIrHandle;
extern PDEIRLPF3_Params IR_params;
extern uint8_t hidDevConnSecure;

/* Initialize KeyPad configuration parameters */
void Hid_initKeys(void)
{
  keypadConfig.rowPins[0] = CONFIG_KEY_ROW_1;
  keypadConfig.rowPins[1] = CONFIG_KEY_ROW_2;
  keypadConfig.rowPins[2] = CONFIG_KEY_ROW_3;
  keypadConfig.rowPins[3] = CONFIG_KEY_ROW_4;
  keypadConfig.rowPins[4] = CONFIG_KEY_ROW_5;
  keypadConfig.rowPins[5] = CONFIG_KEY_ROW_6;

  keypadConfig.colPins[0] = CONFIG_KEY_COL_1;
  keypadConfig.colPins[1] = CONFIG_KEY_COL_2;
  keypadConfig.colPins[2] = CONFIG_KEY_COL_3;
  keypadConfig.colPins[3] = CONFIG_KEY_COL_4;
  keypadConfig.colPins[4] = CONFIG_KEY_COL_5;
  keypadConfig.colPins[5] = CONFIG_KEY_COL_6;
  keypadConfig.colPins[6] = CONFIG_KEY_COL_7;

  keypadConfig.debounceDelay_us = 1000000;
  keypadConfig.max_pressed_keys = 3;
  Keypad_init();

  // Initialize key router for IR/HID routing
  KeyRouter_init();

}

/* Task for handling keypad events */
/*********************************************************************
 * @fn      processKeyPress
 *
 * @brief   Process a key press event from the queue
 *
 * @param   keys - Pointer to PressedKeys structure containing:
 *                 - key_count: Number of keys pressed simultaneously (1-3)
 *                 - pressedKeys[]: Array of (row, col) positions
 *
 * @return  None
 *
 * @note    OOBE (Out-of-Box Experience) IR-Only Mode:
 *          ============================================
 *          This function implements automatic IR-only mode during OOBE per
 *          Google ATV specification. Each key function (e.g., Hid_keyBack,
 *          Hid_keyGuide) checks conn_established flag:
 *
 *          - conn_established = 0 (OOBE/First Boot):
 *            → All keys send IR codes only
 *            → LED shows orange (KEY_PRESS_IR)
 *            → Remote operates in IR-only mode during 10-minute pairing window
 *
 *          - conn_established = 1 (After Pairing):
 *            → All keys send BLE HID reports
 *            → LED shows green (KEY_PRESS_BLE)
 *            → Full BLE functionality active
 *
 *          This provides seamless OOBE experience without special mode handling.
 *********************************************************************/
static void processKeyPress(PressedKeys *keys)
{
    /* Track that a key is now pressed - KEY_NONE must be sent on release */
    current_key_state = KEY_STATE_PRESSED;

    /* Signal to the Buzzer logic that the RC has been found */
    rc_used = 1;

    /* Update last interaction time for voice privacy timeout (1-minute rule) */
    VoiceInteraction_updateLastInteraction();

    /* OOBE IR-Only Mode Implementation:
     * Check connection status to determine IR vs BLE mode */
    if (conn_established == 1)
    {
        /* BLE Connected Mode: Send HID reports */
        /* Restart Shutdown Clock if key is pressed while in a connection */
        RestartShutdownClock();

        /* Trigger LED Key Events */
        if (hidDevConnSecure)
        {
            LED_KeyPress(KEY_PRESS_BLE);  // Green LED - BLE mode
        }
        else
        {
            LED_KeyPress(KEY_PRESS_NO_PAIR);  // Red LED - connected but not paired
        }
    }
    else
    {
        /* OOBE/IR-Only Mode: No BLE connection, all keys send IR codes */
        LED_KeyPress(KEY_PRESS_IR);  // Orange LED - IR mode
    }

    /* How many keys pressed at the same time. */
    if (keys->key_count == 1)
    {
        void* callbackKeySelection[NUM_ROWS][NUM_COLS] =
        {
            {Hid_keyPower, Hid_keyInput, Hid_keyUserProfile, Hid_keyAssitant, Hid_keyDashboard, Hid_key1, Hid_key2},
            {Hid_key3, Hid_key4, Hid_key5, Hid_key6, Hid_key7, Hid_key8, Hid_key9},
            {Hid_keySubtitles, Hid_key0, Hid_keyInfo, Hid_keyRed, Hid_keyGreen, Hid_keyYellow, Hid_keyBlue},
            {Hid_keyDPadUp, Hid_keyDPadLeft, Hid_keyDPadCenter, Hid_keyDPadRight, Hid_keyDPadDown, Hid_keyBack, Hid_keyHome}, 
            {Hid_keyGuide, Hid_keyFreeTV, Hid_keyFreeTV, Hid_keyFreeTV, Hid_keyVolumeUp, Hid_keyProgramUp, Hid_keyMute},
            {Hid_keyVolumeDown, Hid_keyProgramDown, Hid_keyYoutube, Hid_keyNetflix, Hid_keyApp03, Hid_keyApp04, Hid_keyApp05} //, Hid_keyApp06}
        };
        
        volatile keyfunction keyf = callbackKeySelection[keys->pressedKeys[0].row][keys->pressedKeys[0].col];
        /* The remote should avoid sending key events (neither BLE, nor IR, nor Assistant) while in buzz mode or couch mode. */
        if (buzz_reset == 0 && couch_mode_active == 0)
        {
            keyf();
            /* No need to activate Backlight when transmitting Audio
            TODO: Doing Backlingth with Audio ends up in Fault ISR. */
            if ((keys->pressedKeys[0].row != 0) && (keys->pressedKeys[0].col != 3))
            {
                if (!g_rcConfig.deviceConfig.isValid || g_rcConfig.deviceConfig.backlightSwitch != CONFIG_SWITCH_DISABLE)
                {
                    Backlighting_trigger();
                }
            }
        }
    }
    /* Multiple Keys pressed at same time. */
    else if (keys->key_count == 2)
    {
        /* COMBO KEY TIMING - Google ATV Spec Section 2.6:
         *
         * Spec requirement: "time gap between two key events is less or equal to 150ms to be considered as a combo"
         *
         * Current implementation: More user-friendly approach
         * - Detects combo whenever both keys are held down simultaneously
         * - Does NOT enforce 150ms timing between first and second key press
         * - If user presses Key1, then 500ms later presses Key2 (while holding Key1), combo is still detected
         *
         * Rationale:
         * - Matrix scan captures current state of ALL keys held down
         * - Users expect combo to work as long as both keys are pressed together
         * - Strict 150ms timing would require tracking individual key press timestamps
         * - Current behavior meets the intent of the spec (prevent accidental combos) while being more forgiving
         */

        /* Pairing Mode & Re-Pairing: [BACK]+[HOME] for longer than 4s. */
        if ((keys->pressedKeys[0].row == 3) && (keys->pressedKeys[0].col == 5) && (keys->pressedKeys[1].row == 3) && (keys->pressedKeys[1].col == 6))
        {
            /* Once the key combo has been pressed (no low interrupt edge), make sure it remains pressed for 4 seconds.
            Once the timer is up, the combo action will be executed in the callback function. */
            Combokey_multipressTimerStart(MODE_REPAIR);
        }
        /* Factory Reset Mode (Clear Bond Cache): [DPAD CENTER]+[MUTE] for longer than 4s. */
        if ((keys->pressedKeys[0].row == 3) && (keys->pressedKeys[0].col == 2) && (keys->pressedKeys[1].row == 4) && (keys->pressedKeys[1].col == 6))
        {
            /* Once the key combo has been pressed (no low interrupt edge), make sure it remains pressed for 4 seconds.
            Once the timer is up, the combo action will be executed in the callback function. */
            Combokey_multipressTimerStart(MODE_CLEARBOND);
        }
        /* Google Bug Report Mode: [DPAD CENTER]+[BACK] for longer than 1s (In IR mode, the sent code data is 0x96.) */
        if ((keys->pressedKeys[0].row == 3) && (keys->pressedKeys[0].col == 2) && (keys->pressedKeys[1].row == 3) && (keys->pressedKeys[1].col == 5))
        {
            /* Once the key combo has been pressed (no low interrupt edge), make sure it remains pressed for 1 seconds.
            Once the timer is up, the combo action will be executed in the callback function. */
            Combokey_multipressTimerStart(MODE_BUGREPORT);
        }
        /* Google Accessibility shortcut: [DPAD DOWN]+[BACK] for longer than 1s. */
        if ((keys->pressedKeys[0].row == 3) && (keys->pressedKeys[0].col == 4) && (keys->pressedKeys[1].row == 3) && (keys->pressedKeys[1].col == 5))
        {
            /* Once the key combo has been pressed (no low interrupt edge), make sure it remains pressed for 1 seconds.
            Once the timer is up, the combo action will be executed in the callback function. */
            Combokey_multipressTimerStart(MODE_GOOGLEACC);
        }
    }

    else if (keys->key_count == keypadConfig.max_pressed_keys)
    {
        /* Once the key combo has been pressed (no low interrupt edge), make sure it remains pressed for 30 seconds.
        Once the timer is up, the combo action will be executed in the callback function. */
        /* NVS Configured Couch mode switch. 0xFF: disconnect BLE, (RCU sleep) / 0x01: BLE continue, battery continue
        (ignore button pressing) */
        if (g_rcConfig.deviceConfig.couchModeSwitch == 0xFF)
        {
            Combokey_multipressTimerStart(MODE_COUCH);
        }
        else
        {
            /* Ignore Button Pressing */
        }
    }
}

/*********************************************************************
 * @fn      processKeyRelease
 *
 * @brief   Process a key release event
 *
 * @param   None
 *
 * @return  None
 *********************************************************************/
static void processKeyRelease(void)
{
    /* The remote should avoid sending key events (neither BLE, nor IR, nor Assistant) while in buzz mode. */
    if (buzz_reset == 0)
    {
        Hid_buttonReleased();
    }
}

/*********************************************************************
 * @fn      Hid_keypadTask
 *
 * @brief   Process key events from the queue
 *
 * @param   None
 *
 * @return  None
 *
 * @note    Queue-based implementation:
 *          - Dequeues all pending events
 *          - Processes press and release events
 *          - Re-enables interrupts after processing
 *********************************************************************/
void Hid_keypadTask(void)
{
    KeyEvent event;

    // Process all queued events (allows burst key presses to be handled)
    // Loop continues until queue is empty
    while (KeyQueue_dequeue(&event))
    {
        if (event.action_type == 1)
        {
            // Key press event
            processKeyPress(&event.keys);
            /* Missed release guard (quick tap case):
               Interrups are disable from the press ISR through the entire debounce window.
               If the key was release before debouce completed, the matrix scan found no keys 
               (key_count == 0) and no key function was disapatched - but processKeyPress() still
               set current_key_state to KEY_STATE_PRESSED (and autio_key_enabled may be set).
               Synthesize the release using the scan data already captired in the event; no GPIO
               reads required.*/
               if (event.keys.key_count == 0)
               {
                 processKeyRelease();
               }
        }
        else
        {
            // Key release event
            processKeyRelease();
        }
    }

    /* Configure keypad rows as inputs with pull-down resistor and interrupts enabled for button release detection. */
    GPIO_setConfig(CONFIG_KEY_ROW_1, GPIO_CFG_IN_PD | GPIO_CFG_HYSTERESIS_ON | GPIO_CFG_INT_BOTH_EDGES_INTERNAL | GPIO_CFG_INT_ENABLE);
    GPIO_setConfig(CONFIG_KEY_ROW_2, GPIO_CFG_IN_PD | GPIO_CFG_HYSTERESIS_ON | GPIO_CFG_INT_BOTH_EDGES_INTERNAL | GPIO_CFG_INT_ENABLE);
    GPIO_setConfig(CONFIG_KEY_ROW_3, GPIO_CFG_IN_PD | GPIO_CFG_HYSTERESIS_ON | GPIO_CFG_INT_BOTH_EDGES_INTERNAL | GPIO_CFG_INT_ENABLE);
    GPIO_setConfig(CONFIG_KEY_ROW_4, GPIO_CFG_IN_PD | GPIO_CFG_HYSTERESIS_ON | GPIO_CFG_INT_BOTH_EDGES_INTERNAL | GPIO_CFG_INT_ENABLE);
    GPIO_setConfig(CONFIG_KEY_ROW_5, GPIO_CFG_IN_PD | GPIO_CFG_HYSTERESIS_ON | GPIO_CFG_INT_BOTH_EDGES_INTERNAL | GPIO_CFG_INT_ENABLE);
    GPIO_setConfig(CONFIG_KEY_ROW_6, GPIO_CFG_IN_PD | GPIO_CFG_HYSTERESIS_ON | GPIO_CFG_INT_BOTH_EDGES_INTERNAL | GPIO_CFG_INT_ENABLE);
}

/* Prepare BLE HID Report to be send */
static void HIDAdvRemote_sendReport(uint16_t keycode)
{

  uint8_t consumer_key[4];
  consumer_key[0] = LO_UINT16(keycode);
  consumer_key[1] = HI_UINT16(keycode);
  consumer_key[2] = 0x00;
  consumer_key[3] = 0x00;
  HidDev_Report(HID_RPT_ID_CC_IN, HID_REPORT_TYPE_INPUT, 4, consumer_key);
}

/* Definition of HID Key Functionality */
static void Hid_keyAssitant()
{
    if (conn_established == 1 && audio_key_enabled == 0)
    {
        // Use HTT (Hold-to-Talk) model - audio starts on press, stops on release
        // Using blinking LED for better visibility
        // Privacy: Remote should not turn on the microphone after 1 minute from the last user interaction with the remote.
        if (VoiceInteraction_startHTT(connHandle))
        {
            LED_Voice(KEY_VOICE_PTT_ON);  // Blinking LED (500ms ON/OFF) for HTT
            audio_key_enabled = 1;
            audio_done = 0;
        }
        // If VoiceInteraction_startHTT returns false, it means:
        // - Privacy timeout exceeded (>1 min since last interaction)
        // - Already active session
        // In these cases, do nothing (no LED, no audio)
    }
}

static void Hid_keyUserProfile()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_PROFILE_SWITCH, conn_established == 1);
}

static void Hid_keyFreeTV()
{
    /* TBD */
}

static void Hid_keySettings()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_SETTINGS, conn_established == 1);
}

static void Hid_key1()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_1, conn_established == 1);
}

static void Hid_key2()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_2, conn_established == 1);
}

static void Hid_key3()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_3, conn_established == 1);
}

static void Hid_key4()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_4, conn_established == 1);
}

static void Hid_key5()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_5, conn_established == 1);
}

static void Hid_key6()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_6, conn_established == 1);
}

static void Hid_key7()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_7, conn_established == 1);
}

static void Hid_key8()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_8, conn_established == 1);
}

static void Hid_key9()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_9, conn_established == 1);
}

static void Hid_keySubtitles()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_CAPTIONS, conn_established == 1);
}

static void Hid_key0()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_0, conn_established == 1);
}

static void Hid_keyInfo()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_INFO, conn_established == 1);
}

static void Hid_keyProgramDown()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_CHANNEL_DOWN, conn_established == 1);
}

static void Hid_keyProgramUp()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_CHANNEL_UP, conn_established == 1);
}


static void Hid_keyDashboard()
{
    // Dashboard button (Settings on some remotes)
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_ALL_APPS, conn_established == 1);
}

static void Hid_keyGuide()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_GUIDE, conn_established == 1);
}

static void Hid_keyBack()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_BACK, conn_established == 1);
}

static void Hid_keyDPadCenter()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_DPAD_CENTER, conn_established == 1);
}

static void Hid_keyDPadLeft()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_DPAD_LEFT, conn_established == 1);
}

static void Hid_keyDPadRight()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_DPAD_RIGHT, conn_established == 1);
}

static void Hid_keyDPadUp()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_DPAD_UP, conn_established == 1);
}

static void Hid_keyDPadDown()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_DPAD_DOWN, conn_established == 1);
}

static void Hid_keyHome()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_HOME, conn_established == 1);
}

static void Hid_keyVolumeDown()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_VOLUME_DOWN, conn_established == 1);
}

static void Hid_keyVolumeUp()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_VOLUME_UP, conn_established == 1);
}

static void Hid_keyMute()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_VOLUME_MUTE, conn_established == 1);
}

static void Hid_keyApp05()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_F20, conn_established == 1);
}

static void Hid_keyApp06()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_F19, conn_established == 1);
}

static void Hid_keyApp04()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_F20, conn_established == 1);
}

static void Hid_keyApp03()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_F19, conn_established == 1);
}

static void Hid_keyRed()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_PROG_RED, conn_established == 1);
}

static void Hid_keyGreen()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_PROG_GREEN, conn_established == 1);
}

static void Hid_keyYellow()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_PROG_YELLOW, conn_established == 1);
}

static void Hid_keyBlue()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_PROG_BLUE, conn_established == 1);
}

static void Hid_keyYoutube()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_F17, conn_established == 1);
}

static void Hid_keyNetflix()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_F18, conn_established == 1);
}

static void Hid_buttonReleased()
{
    if (conn_established == 1)
    {
        if (audio_key_enabled == 1)
        {
            // HTT mode: Button release stops audio immediately
            VoiceInteraction_stopHTT(connHandle);
            // Note: VoiceInteraction_stopHTT() -> sendAudioStop() -> sets audio_done=1
            // The LED will stop blinking automatically via the audio_done flag
            // State will transition to IDLE after audio stops
            current_key_state = KEY_STATE_IDLE;
        }
        else
        {
            // Don't send HID report if in couch mode
            if (couch_mode_active == 0)
            {
                /*********************************************************************
                 * STATE-BASED KEY_NONE HANDLING
                 *
                 * Only send KEY_NONE on PRESSED → IDLE transition.
                 * Prevents both flooding AND missed KEY_NONE:
                 *
                 * - KEY_STATE_PRESSED: Key was down, MUST send KEY_NONE
                 * - KEY_STATE_IDLE: Already released, skip (prevents flooding)
                 *
                 * This guarantees KEY_NONE is sent exactly once per actual
                 * press→release cycle, even with bouncy/noisy release detection.
                 *********************************************************************/
                if (current_key_state == KEY_STATE_PRESSED)
                {
                    HIDAdvRemote_sendReport(KEY_NONE);
                    current_key_state = KEY_STATE_IDLE;  // Transition to idle
                }
                // Else: current_key_state == KEY_STATE_IDLE
                // Already released, this is a spurious bounce event, ignore
            }
            else
            {
                // Couch mode: don't send reports but still track state
                current_key_state = KEY_STATE_IDLE;
            }
        }
    }
}

static void Hid_keyPower()
{
    /*
     * Cache Power Key - Scenario A (Advertising/Reconnecting):
     * Device is alive and advertising/reconnecting. Power is pressed before BLE
     * reconnects. IR is sent immediately via KeyRouter. The BLE HID Power report
     * is cached here and replayed in Keypad_replayCachedPowerKey() after the
     * BLEAPPUTIL_LINK_ESTABLISHED_EVENT fires.
     *
     * Limitation - Scenario B (Shutdown Wakeup, NOT covered):
     * If Power woke the device from shutdown, that press is unrecoverable.
     * TriggerShutdown() drives all columns HIGH before entering shutdown, so
     * after wakeup only the ROW pin that fired is known — the exact column
     * (and therefore key identity) cannot be determined. The wakeup key press
     * is always lost regardless of this cache switch setting.
     */
    if (conn_established == 0 &&
        g_rcConfig.bleConfig.isValid &&
        g_rcConfig.bleConfig.cachePowerSwitch == 0xFF)
    {
        pendingPowerKey = true;
    }
    KeyRouter_handleKeyPress(IR_KEYCODE_POWER, conn_established == 1);
}

void Keypad_replayCachedPowerKey(void)
{
    /* Send Power key press then release using the HID consumer code (0x0030).
     * Uses HIDAdvRemote_sendReport directly to avoid re-triggering IR via
     * KeyRouter_handleKeyPress. Only called from app_connection.c after
     * BLEAPPUTIL_LINK_ESTABLISHED_EVENT when pendingPowerKey is set. */
    HIDAdvRemote_sendReport(0x0030); /* Power key HID consumer code */
    HIDAdvRemote_sendReport(KEY_NONE);
    pendingPowerKey = false;
}

static void Hid_keyInput()
{
    // Use key router for intelligent IR/HID routing
    KeyRouter_handleKeyPress(IR_KEYCODE_TV_INPUT, conn_established == 1);
}

static void Hid_keyG21()
{
  // TBD: Do nothing
}