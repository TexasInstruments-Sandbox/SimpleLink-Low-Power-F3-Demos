/******************************************************************************

@file  ir_service.c

@brief This file contains the Data Stream service sample GATT service
for use with the BLE sample application.

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

#include <string.h>
#include "ti/ble/stack_util/icall/app/icall.h"
/* This Header file contains all BLE API and icall structure definition */
#include "ti/ble/stack_util/icall/app/icall_ble_api.h"

#include "ir_service.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"

#include <infrared/IR.h>
#include <nvs_flash/ir_flash.h>
#include <keypad/key_router.h>

/*********************************************************************
 * CONSTANTS
 */

/* Custom ATT Error Code (per Google IR over BLE spec) */
#define ATT_ERR_WRITE_REQ_REJECTED  0xFC   /* Write Request Rejected */

/*********************************************************************
 * TYPEDEFS
 */

#define IR_SERVICE_UUID     0XC0BF
#define IR_CONTROL_UUID     0XC1BF
#define IR_KEY_UUID         0XC2BF
#define IR_IRCODE_UUID      0XC3BF
#define IR_IRSUPPRESS_UUID  0XC4BF
#define IR_KEYEVENT_UUID    0XC5BF

/*********************************************************************
 * LOCAL VARIABLES
 */

/// @brief Android Base 128-bit UUID: F000XXXX-0451-4000-B000-000000000000
#define IR_BASE_UUID_128( uuid )  0x64, 0xB6, 0x17, 0xF6, 0x01, 0xAF, 0x7D, 0xBC, \
                                  0x05, 0x4F, 0x21, 0x5A, LO_UINT16( uuid ), HI_UINT16( uuid ), 0x43, 0xD3

// IR_SERVICE
static const uint8 irProfileServUUID[ATT_UUID_SIZE] =
{
    IR_BASE_UUID_128(IR_SERVICE_UUID)
};

// IR_PROG_CONTROL
static const uint8 irProfileControlUUID[ATT_UUID_SIZE] =
{
    IR_BASE_UUID_128(IR_CONTROL_UUID)
};

// IR_KEY_ID
static const uint8 irProfileKeyUUID[ATT_UUID_SIZE] =
{
    IR_BASE_UUID_128(IR_KEY_UUID)
};

// IR_CODE
static const uint8 irProfileIRCodeUUID[ATT_UUID_SIZE] =
{
    IR_BASE_UUID_128(IR_IRCODE_UUID)
};

// IR_SUPPRESS
static const uint8 irProfileIRSuppressUUID[ATT_UUID_SIZE] =
{
    IR_BASE_UUID_128(IR_IRSUPPRESS_UUID)
};

// IR_KEY_EVENT
static const uint8 irProfileKeyEventUUID[ATT_UUID_SIZE] =
{
    IR_BASE_UUID_128(IR_KEYEVENT_UUID)
};

//----------------------------------------------------------------------//

/* Last Event: 8-bit enum + Remaining Buzz Duration: unsigned 16-bit integer */
#define IR_CHAR_CTL_LEN  3
#define IR_VALUE_IDX     2

// IR_SERVICE_UUID
// IR Profile Service
static const gattAttrType_t irProfileService = {ATT_UUID_SIZE, irProfileServUUID};

// IR_PROG_CONTROL Characteristic Properties
static uint8 irProfileControlProps = GATT_PROP_WRITE;
// IR_PROG_CONTROL Characteristic Value
static uint8_t irProfileControl[IR_CHAR_CTL_LEN];
// IR_PROG_CONTROL Characteristic Configuration Descriptor Value
static gattCharCfg_t *irProfileControlConfig;

// IR_KEY_ID Characteristic Properties
static uint8 irProfileKeyIDProps = GATT_PROP_WRITE;
// IR_KEY_ID Characteristic Value
static uint8_t irProfileKeyID[IR_CHAR_CTL_LEN];
// IR_KEY_ID Characteristic Configuration Descriptor Value
static gattCharCfg_t *irProfileKeyIDConfig;

// IR_CODE Characteristic Properties
static uint8 irProfileIRCodeProps = GATT_PROP_WRITE;
// IR_CODE Characteristic Value
static uint8_t irProfileIRCode[IR_CHAR_CTL_LEN];
// IR_CODE Characteristic Configuration Descriptor Value
static gattCharCfg_t *irProfileIRCodeConfig;

// IR_SUPPRESS Characteristic Properties
static uint8 irProfileIRSuppressProps = GATT_PROP_WRITE;
// IR_SUPPRESS Characteristic Value
static uint8_t irProfileIRSuppress[IR_CHAR_CTL_LEN];
// IR_SUPPRESS Characteristic Configuration Descriptor Value
static gattCharCfg_t *irProfileIRSuppressConfig;

// IR_KEY_EVENT Characteristic Properties
static uint8 irProfileKeyEventProps = GATT_PROP_NOTIFY;
// IR_KEY_EVENT Characteristic Value
static uint8_t irProfileKeyEvent[IR_CHAR_CTL_LEN];
// IR_KEY_EVENT Characteristic User Description
static uint8 irProfileKeyEventUserDesc[] = "IR Key Event";
// IR_KEY_EVENT Characteristic Configuration Descriptor Value
static gattCharCfg_t *irProfileKeyEventConfig;

static irServiceCB_t *ir_profileCBs = NULL;

/*********************************************************************
 * IR PROGRAMMING SESSION STATE
 */

/* Programming session state */
typedef enum {
    IR_SESSION_IDLE,           /* No active session */
    IR_SESSION_STARTED,        /* Session started, waiting for key/code */
    IR_SESSION_KEY_RECEIVED,   /* Key ID received, waiting for IR code */
    IR_SESSION_ERROR          /* Error occurred, reject further writes */
} IrSessionState_t;

static IrSessionState_t sessionState = IR_SESSION_IDLE;
static uint16_t currentKeyCode = 0x0000;     /* Current key being programmed */
static uint8_t currentSlot = 0xFF;           /* IR table slot for current key */
static uint8_t irCodeBuffer[256];            /* Buffer for IR code data */
static uint16_t irCodeBufferLen = 0;         /* Current length of IR code */
static IrCodeTable_t pendingTable;           /* Temporary table during programming */
static uint8_t suppressList[40];             /* List of suppressed key codes */
static uint8_t suppressListLen = 0;          /* Number of suppressed keys */

/*********************************************************************
 * Profile Attributes - Table
 */

static gattAttribute_t irProfileAttrTbl[] =
{
 /*--------------------type-------------------*/ /*------------permissions-------------*/ /*------------------pValue--------------------*/
   // IR Primary Service
   GATT_BT_ATT( primaryServiceUUID,                GATT_PERMIT_READ,                                   (uint8 *)&irProfileService ),

   // IR Characteristic Declaration
   GATT_BT_ATT( characterUUID,                     GATT_PERMIT_READ,                                    &irProfileControlProps ),
   // IR Characteristic Value
   GATT_ATT( irProfileControlUUID,                 GATT_PERMIT_READ | GATT_PERMIT_WRITE,                irProfileControl ),
   // IR Characteristic configuration
   GATT_BT_ATT( clientCharCfgUUID,                 GATT_PERMIT_READ | GATT_PERMIT_WRITE,                (uint8 *)&irProfileControlConfig ),

   // IR Characteristic Declaration
   GATT_BT_ATT( characterUUID,                     GATT_PERMIT_READ,                                    &irProfileKeyIDProps ),
   // IR Characteristic Value
   GATT_ATT( irProfileKeyUUID,                     GATT_PERMIT_READ | GATT_PERMIT_WRITE,                irProfileKeyID ),
   // IR Characteristic configuration
   GATT_BT_ATT( clientCharCfgUUID,                 GATT_PERMIT_READ | GATT_PERMIT_WRITE,                (uint8 *)&irProfileKeyIDConfig ),
   
   // IR Characteristic Declaration
   GATT_BT_ATT( characterUUID,                     GATT_PERMIT_READ,                                    &irProfileIRCodeProps ),
   // IR Characteristic Value
   GATT_ATT( irProfileIRCodeUUID,                  GATT_PERMIT_READ | GATT_PERMIT_WRITE,                irProfileIRCode ),
   // IR Characteristic configuration
   GATT_BT_ATT( clientCharCfgUUID,                 GATT_PERMIT_READ | GATT_PERMIT_WRITE,                (uint8 *)&irProfileIRCodeConfig ),

   // IR Characteristic Declaration
   GATT_BT_ATT( characterUUID,                     GATT_PERMIT_READ,                                    &irProfileIRSuppressProps ),
   // IR Characteristic Value
   GATT_ATT( irProfileIRSuppressUUID,              GATT_PERMIT_READ | GATT_PERMIT_WRITE,                irProfileIRSuppress ),
   // IR Characteristic configuration
   GATT_BT_ATT( clientCharCfgUUID,                 GATT_PERMIT_READ | GATT_PERMIT_WRITE,                (uint8 *)&irProfileIRSuppressConfig ),

   // IR_KEY_EVENT Characteristic Declaration (NOTIFY - for key press/release events)
   GATT_BT_ATT( characterUUID,                     GATT_PERMIT_READ,                                    &irProfileKeyEventProps ),
   // IR_KEY_EVENT Characteristic Value (notifications only, no writes)
   GATT_ATT( irProfileKeyEventUUID,                GATT_PERMIT_READ,                                    irProfileKeyEvent ),
   // IR_KEY_EVENT Client Characteristic Configuration (for enabling notifications)
   GATT_BT_ATT( clientCharCfgUUID,                 GATT_PERMIT_READ | GATT_PERMIT_WRITE,                (uint8 *)&irProfileKeyEventConfig ),
};

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static bStatus_t IR_writeAttrCB( uint16 connHandle, gattAttribute_t *pAttr, uint8 *pValue, uint16 len, uint16 offset, uint8 method );
static bStatus_t IR_readAttrCB( uint16_t connHandle, gattAttribute_t *pAttr, uint8_t *pValue, uint16_t *pLen, uint16_t offset, uint16_t maxLen, uint8_t method );

/*********************************************************************
 * PROFILE CALLBACKS
 */
const gattServiceCBs_t ir_servCBs =
{
  IR_readAttrCB,                 // Read callback function pointer
  IR_writeAttrCB,                // Write callback function pointer
  NULL                           // Authorization callback function pointer
};

/*********************************************************************
* @fn      IR_registerProfileCBs
*
* @brief   Registers the profile callback function. Only call
*          this function once.
*
* @param   profileCallback - pointer to profile callback functions.
*
* @return  SUCCESS or INVALIDPARAMETER
*/
bStatus_t IR_registerProfileCBs( irServiceCB_t *profileCallback )
{
  if ( profileCallback )
  {
    ir_profileCBs = profileCallback;

    return ( SUCCESS );
  }

  return ( INVALIDPARAMETER );
}

/*********************************************************************
* @fn      IR_AddService
*
* @brief   This function initializes the Data Stream Server service
*          by registering GATT attributes with the GATT server.
*
* @return  SUCCESS or stack call status
*/

bStatus_t IR_AddService( void )
{
  bStatus_t status = SUCCESS;

  // Allocate Audio Stream Client Characteristic Configuration table
  irProfileControlConfig = (gattCharCfg_t *)ICall_malloc(sizeof(gattCharCfg_t) * MAX_NUM_BLE_CONNS);
  if ( irProfileControlConfig == NULL )
  {
    return bleMemAllocError;
  }
  // Initialize Audio Stream Client Characteristic Configuration attributes
  GATTServApp_InitCharCfg( LINKDB_CONNHANDLE_INVALID, irProfileControlConfig );

  // Register GATT attribute list and CBs with GATT Server
  status = GATTServApp_RegisterService( irProfileAttrTbl,
                                        GATT_NUM_ATTRS( irProfileAttrTbl ),
                                        GATT_MAX_ENCRYPT_KEY_SIZE,
                                        &ir_servCBs );

  // Return status value
  return ( status );
}

/*********************************************************************
 * @fn      IR_setParameter
 *
 * @brief   Set a Data Stream Service parameter.
 *
 * @param   param - Characteristic UUID
 * @param   pValue - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 * @param   len - length of data to write
 *
 * @return  SUCCESS or stack call status
 */
bStatus_t IR_setParameter(uint8 param, void *pValue, uint16 len)
{
  bStatus_t status = SUCCESS;

  // Verify input parameters
  if ( pValue == NULL )
  {
    return ( INVALIDPARAMETER );
  }

  // Return status value
  return ( status );
}

/*********************************************************************
 * @fn      IR_writeAttrCB
 *
 * @brief   Validate attribute data prior to a write operation
 *
 * @param   connHandle - connection message was received on
 * @param   pAttr - pointer to attribute
 * @param   pValue - pointer to data to be written
 * @param   len - length of data
 * @param   offset - offset of the first octet to be written
 * @param   method - type of write message
 *
 * @return  SUCCESS or stack call status
 */

volatile uint16_t uuid_char;

/*********************************************************************
 * LOCAL HELPER FUNCTIONS
 */

/**
 * @brief Reset IR programming session
 */
static void resetProgrammingSession(void)
{
    sessionState = IR_SESSION_IDLE;
    currentKeyCode = 0x0000;
    currentSlot = 0xFF;
    irCodeBufferLen = 0;
    memset(irCodeBuffer, 0, sizeof(irCodeBuffer));
}

/**
 * @brief Process IR_PROG_CONTROL write (START/FINISH session)
 */
static bStatus_t processControlWrite(uint8_t *pValue, uint16_t len)
{
    if (len < 1)
    {
        return ATT_ERR_INVALID_VALUE_SIZE;
    }

    uint8_t command = pValue[0];

    if (command == 0x01)  /* START session */
    {
        // Initialize new session
        resetProgrammingSession();
        sessionState = IR_SESSION_STARTED;

        // Load current IR table from NVS
        if (!IrFlash_readTable(&pendingTable))
        {
            // If read fails, initialize with defaults
            memset(&pendingTable, 0xFF, sizeof(pendingTable));
            pendingTable.isValid = false;
        }

        return SUCCESS;
    }
    else if (command == 0x00)  /* FINISH session */
    {
        if (sessionState != IR_SESSION_STARTED && sessionState != IR_SESSION_KEY_RECEIVED)
        {
            // No active session
            return ATT_ERR_WRITE_REQ_REJECTED;
        }

        // Write pending table to NVS
        if (!IrFlash_writeTable(&pendingTable))
        {
            sessionState = IR_SESSION_ERROR;
            return ATT_ERR_WRITE_REQ_REJECTED;
        }

        // Reload key router cache so new IR codes take effect immediately
        KeyRouter_reloadCache();

        // Session complete
        resetProgrammingSession();
        return SUCCESS;
    }

    return ATT_ERR_INVALID_VALUE;
}

/**
 * @brief Process IR_KEY_ID write (specify which key to program)
 */
static bStatus_t processKeyIdWrite(uint8_t *pValue, uint16_t len)
{
    if (sessionState == IR_SESSION_ERROR)
    {
        return ATT_ERR_WRITE_REQ_REJECTED;
    }

    if (sessionState != IR_SESSION_STARTED && sessionState != IR_SESSION_KEY_RECEIVED)
    {
        // Must START session first
        sessionState = IR_SESSION_ERROR;
        return ATT_ERR_WRITE_REQ_REJECTED;
    }

    if (len != 2)
    {
        sessionState = IR_SESSION_ERROR;
        return ATT_ERR_INVALID_VALUE_SIZE;
    }

    // Parse Android key code (little-endian)
    uint16_t keyCode = (pValue[1] << 8) | pValue[0];

    // Check if key code is <= previous (must be ascending order)
    if (keyCode <= currentKeyCode && currentKeyCode != 0x0000)
    {
        sessionState = IR_SESSION_ERROR;
        return ATT_ERR_WRITE_REQ_REJECTED;
    }

    // Map key code to IR slot
    uint8_t slot = IrFlash_keyCodeToSlot(keyCode);
    if (slot == 0xFF)
    {
        // Key not supported, ignore silently (per spec)
        return SUCCESS;
    }

    // Ready to receive IR code for this key
    currentKeyCode = keyCode;
    currentSlot = slot;
    irCodeBufferLen = 0;
    sessionState = IR_SESSION_KEY_RECEIVED;

    return SUCCESS;
}

/**
 * @brief Process IR_CODE write (IR code data, may be multiple writes)
 */
static bStatus_t processIrCodeWrite(uint8_t *pValue, uint16_t len)
{
    if (sessionState == IR_SESSION_ERROR)
    {
        return ATT_ERR_WRITE_REQ_REJECTED;
    }

    if (sessionState != IR_SESSION_KEY_RECEIVED)
    {
        // Must write KEY_ID first
        sessionState = IR_SESSION_ERROR;
        return ATT_ERR_WRITE_REQ_REJECTED;
    }

    // Append data to IR code buffer
    if (irCodeBufferLen + len > sizeof(irCodeBuffer))
    {
        sessionState = IR_SESSION_ERROR;
        return ATT_ERR_INVALID_VALUE_SIZE;
    }

    memcpy(&irCodeBuffer[irCodeBufferLen], pValue, len);
    irCodeBufferLen += len;

    // Parse IR code format and store in pending table
    // For now, we support Type 1 (NEC format with 4 bytes minimum)
    if (irCodeBufferLen >= 4)
    {
        uint8_t type = irCodeBuffer[0];

        if (type == 0x00)  /* Type 0: Disabled */
        {
            // Store as all 0x00 to disable
            pendingTable.entries[currentSlot].address = 0x00;
            pendingTable.entries[currentSlot].address2 = 0x00;
            pendingTable.entries[currentSlot].command = 0x00;
            pendingTable.entries[currentSlot].commandInv = 0x00;
        }
        else if (type == 0x01)  /* Type 1: NEC IR code */
        {
            // Parse NEC format: [Type][DutyCycle][Carrier...][Data...]
            // Simplified: Extract address and command from data section
            // Full parsing would require understanding carrier and timing pairs

            // For basic NEC: Last 4 bytes should be [Addr][Addr_Inv][Cmd][Cmd_Inv]
            if (irCodeBufferLen >= 8)
            {
                uint16_t dataStart = irCodeBufferLen - 4;
                pendingTable.entries[currentSlot].address = irCodeBuffer[dataStart];
                pendingTable.entries[currentSlot].address2 = irCodeBuffer[dataStart + 1];
                pendingTable.entries[currentSlot].command = irCodeBuffer[dataStart + 2];
                pendingTable.entries[currentSlot].commandInv = irCodeBuffer[dataStart + 3];
            }
        }

        // Mark table as valid
        pendingTable.isValid = true;

        // Return to STARTED state to accept next key
        sessionState = IR_SESSION_STARTED;
        irCodeBufferLen = 0;
    }

    return SUCCESS;
}

/**
 * @brief Process IR_SUPPRESS write (temporarily switch IR keys to BLE HID)
 */
static bStatus_t processSuppressWrite(uint8_t *pValue, uint16_t len)
{
    // Store list of key codes to suppress.
    // Each entry is 2 bytes (Android key code) per spec.
    if (len % 2 != 0)
    {
        return ATT_ERR_INVALID_VALUE_SIZE;
    }

    suppressListLen = len / 2;
    if (suppressListLen > sizeof(suppressList))
    {
        return ATT_ERR_INVALID_VALUE_SIZE;
    }

    // Parse key codes
    for (uint8_t i = 0; i < suppressListLen; i++)
    {
        uint16_t keyCode = (pValue[i*2+1] << 8) | pValue[i*2];
        uint8_t slot = IrFlash_keyCodeToSlot(keyCode);
        suppressList[i] = slot;
    }

    return SUCCESS;
}

/**
 * @brief Write attribute callback
 */
static bStatus_t IR_writeAttrCB( uint16 connHandle,
                                        gattAttribute_t *pAttr,
                                        uint8 *pValue, uint16 len,
                                        uint16 offset, uint8 method )
{
  bStatus_t status = SUCCESS;

  uint16_t uuid = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);

  switch (uuid)
  {
    case GATT_CLIENT_CHAR_CFG_UUID:
      status = GATTServApp_ProcessCCCWriteReq(connHandle, pAttr, pValue, len,
                                               offset, GATT_CLIENT_CFG_NOTIFY);
      if (status == SUCCESS)
      {
        uint16_t charCfg = BUILD_UINT16(pValue[0], pValue[1]);
      }
      break;

    default:
      status = ATT_ERR_ATTR_NOT_FOUND;
      break;
  }

  /* Read the UUID of the IR Characteristic */
  uuid_char = BUILD_UINT16(pAttr->type.uuid[12], pAttr->type.uuid[13]);

  // Trigger buzzer feedback
  //Buzzing_trigger();

  // Handle IR programming characteristics
  if (uuid_char == IR_CONTROL_UUID)
  {
      status = processControlWrite(pValue, len);
  }
  else if (uuid_char == IR_KEY_UUID)
  {
      status = processKeyIdWrite(pValue, len);
  }
  else if (uuid_char == IR_IRCODE_UUID)
  {
      status = processIrCodeWrite(pValue, len);
  }
  else if (uuid_char == IR_IRSUPPRESS_UUID)
  {
      status = processSuppressWrite(pValue, len);
  }

  return (status);
}

/*********************************************************************
 * @fn          IR_notify
 *
 * @brief       Send a notification of the level state characteristic.
 *
 * @param       connHandle - linkDB item
 *
 * @return      None.
 */
static void IR_notify(uint16_t connHandle)
{
  uint16_t value = GATTServApp_ReadCharCfg(connHandle,
                                           irProfileControlConfig);
  if (value & GATT_CLIENT_CFG_NOTIFY)
  {
    attHandleValueNoti_t noti;

    noti.pValue = GATT_bm_alloc(connHandle, ATT_HANDLE_VALUE_NOTI,
                                IR_CHAR_CTL_LEN, NULL);
    if (noti.pValue != NULL)
    {
      noti.handle = irProfileAttrTbl[IR_VALUE_IDX].handle;
      noti.len = IR_CHAR_CTL_LEN;
      noti.pValue[0] = 0;

      if (GATT_Notification(connHandle, &noti, FALSE) != SUCCESS)
      {
        GATT_bm_free((gattMsg_t *)&noti, ATT_HANDLE_VALUE_NOTI);
      }
    }
  }
}

static bStatus_t IR_readAttrCB( uint16_t connHandle, gattAttribute_t *pAttr,
                                       uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                       uint16_t maxLen, uint8_t method )
{
  bStatus_t status = SUCCESS;

  // Make sure it's not a blob operation (no attributes in the profile are long)
  if (offset > 0)
  {
    return (ATT_ERR_ATTR_NOT_LONG);
  }

  uint16_t uuid = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);

  return status;
}

/*********************************************************************
 * PUBLIC API FUNCTIONS
 */

/**
 * @brief Initialize IR Service
 */
void IR_InitService(void)
{
    // Initialize IR flash module
    IrFlash_init();

    // Reset session state
    resetProgrammingSession();

    // Clear suppress list
    suppressListLen = 0;
    memset(suppressList, 0xFF, sizeof(suppressList));
}

/**
 * @brief Check if a key is suppressed
 */
bool IR_IsKeySuppressed(uint8_t slot)
{
    // Check if slot is in suppress list
    for (uint8_t i = 0; i < suppressListLen; i++)
    {
        if (suppressList[i] == slot)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Clear suppress list
 */
void IR_ClearSuppressList(void)
{
    suppressListLen = 0;
    memset(suppressList, 0xFF, sizeof(suppressList));
}

/**
 * @brief Send IR key event notification
 *
 * Notifies Android TV when IR key is pressed or released.
 * Format: [slot, event_type] where event_type: 0=KEY_UP, 1=KEY_DOWN
 */
void IR_SendKeyEvent(uint8_t slot, bool isPressed)
{
    // Get connection handle (assuming single connection for now)
    // In production, iterate through all connections
    extern uint16_t connHandle;  // From main app

    if (connHandle == LINKDB_CONNHANDLE_INVALID)
    {
        return;  // Not connected, no one to notify
    }

    // Check if client has enabled notifications
    uint16_t value = GATTServApp_ReadCharCfg(connHandle, irProfileKeyEventConfig);
    if (!(value & GATT_CLIENT_CFG_NOTIFY))
    {
        return;  // Client hasn't enabled notifications
    }

    // Prepare notification
    attHandleValueNoti_t noti;
    uint8_t notifData[2];

    notifData[0] = slot;                    // IR slot number (0-47)
    notifData[1] = isPressed ? 0x01 : 0x00; // 1=KEY_DOWN, 0=KEY_UP

    noti.pValue = GATT_bm_alloc(connHandle, ATT_HANDLE_VALUE_NOTI, 2, NULL);
    if (noti.pValue != NULL)
    {
        // Find IR_KEY_EVENT characteristic handle
        // It's the last characteristic in the table
        noti.handle = irProfileAttrTbl[GATT_NUM_ATTRS(irProfileAttrTbl) - 2].handle;
        noti.len = 2;
        memcpy(noti.pValue, notifData, 2);

        if (GATT_Notification(connHandle, &noti, FALSE) != SUCCESS)
        {
            GATT_bm_free((gattMsg_t *)&noti, ATT_HANDLE_VALUE_NOTI);
        }
    }
}

/**
 * @brief Erase IR table and reset to defaults
 *
 * Called when pairing with new device to ensure clean slate.
 */
void IR_ResetTable(void)
{
    // Erase IR table from NVS
    IrFlash_eraseTable();

    // Reset programming session
    resetProgrammingSession();

    // Clear suppress list
    IR_ClearSuppressList();

    // Notify KeyRouter to reload cache
    extern void KeyRouter_reloadCache(void);
    KeyRouter_reloadCache();
}