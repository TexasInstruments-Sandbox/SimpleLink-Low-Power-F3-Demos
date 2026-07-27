/******************************************************************************

@file  backlight_service.c

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
#include <stdbool.h>
#include "ti/ble/stack_util/icall/app/icall.h"
/* This Header file contains all BLE API and icall structure definition */
#include "ti/ble/stack_util/icall/app/icall_ble_api.h"

#include "backlight_service.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "calendar/calendar.h"
#include "backlight/pwm_backlight.h"
#include <ti/drivers/dpl/ClockP.h>

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPEDEFS
 */

#define BACKLIGHTING_SERVICE_UUID    0x8B69
#define BACKLIGHTING_CHAR_UUID       0x8B6A

/*********************************************************************
 * LOCAL VARIABLES
 */

/// @brief Android Base 128-bit UUID: F000XXXX-0451-4000-B000-000000000000
#define BACKLIGHTING_BASE_UUID_128( uuid )  0x64, 0xB6, 0x17, 0xF6, 0x01, 0xAF, 0x7D, 0xBC, \
                                  0x05, 0x4F, 0x21, 0x5A, LO_UINT16( uuid ), HI_UINT16( uuid ), 0xB3, 0xA2

// BACKLIGHTING_SERVICE
static const uint8 backlightingProfileServUUID[ATT_UUID_SIZE] =
{
    BACKLIGHTING_BASE_UUID_128(BACKLIGHTING_SERVICE_UUID)
};

// BACKLIGHTING_CHAR
static const uint8 backlightingCharUUID[ATT_UUID_SIZE] =
{
    BACKLIGHTING_BASE_UUID_128(BACKLIGHTING_CHAR_UUID)
};

//----------------------------------------------------------------------//

#define BACKLIGHTING_CHAR_CTL_LEN  3 // Event: 8-bit + Time/Mode: unsigned 16-bit integer
#define BACKLIGHTING_VALUE_IDX     2 

// BACKLIGHTING_SERVICE_UUID
// BACKLIGHTING Profile Service
static const gattAttrType_t backlightingProfileService = {ATT_UUID_SIZE, backlightingProfileServUUID};

// CONTROL Characteristic Properties
static uint8 backlightingProfileControlProps = GATT_PROP_WRITE | GATT_PROP_READ | GATT_PROP_NOTIFY;
// CONTROL Characteristic Value
static uint8_t backlightingProfileControl[BACKLIGHTING_CHAR_CTL_LEN];
// CONTROL Characteristic Configuration Descriptor Value
static gattCharCfg_t *backlightingProfileControlConfig;

static backlightingServiceCB_t *backlighting_profileCBs = NULL;

Backlight_service_dataformat backlightServiceData;

// Access to backlight program mode (defined in pwm_backlight.c)
extern Backlight_program_t backlightProgramMode;

/*********************************************************************
 * TIME SYNC STATE TRACKING
 *
 * Per Google ATV spec, SET_TIME_CMD is only valid when:
 * 1. Backlight mode is set to MODE_SCHEDULE (0x0002)
 * 2. SET_TIME_CMD is received within 3 seconds of setting the mode
 *
 * This prevents spurious time sync attempts and enforces proper
 * sequencing of mode configuration followed by time sync.
 *********************************************************************/
static uint32_t lastModeSetTimestamp = 0;  // Timestamp when MODE_SCHEDULE was set
static bool waitingForTimeSync = false;     // Flag: waiting for time sync within window

#define TIME_SYNC_WINDOW_MS 3000  // 3-second window for time sync after mode set

/*********************************************************************
 * Profile Attributes - Table
 */

volatile gattAttribute_t backlightingProfileAttrTbl[] =
{
 /*--------------------type-------------------*/ /*------------permissions-------------*/ /*------------------pValue--------------------*/
   // BACKLIGHTING Primary Service
   GATT_BT_ATT( primaryServiceUUID,                 GATT_PERMIT_READ,                                   (uint8 *)&backlightingProfileService ),

   // BACKLIGHTING Characteristic Declaration
   GATT_BT_ATT( characterUUID,                      GATT_PERMIT_READ,                                    &backlightingProfileControlProps ),
   // BACKLIGHTING Characteristic Value
   GATT_ATT( backlightingCharUUID,                  GATT_PERMIT_READ | GATT_PERMIT_WRITE,                backlightingProfileControl ),
   // BACKLIGHTING Characteristic configuration
   GATT_BT_ATT( clientCharCfgUUID,                  GATT_PERMIT_READ | GATT_PERMIT_WRITE,                (uint8 *)&backlightingProfileControlConfig ),
};

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static bStatus_t Backlighting_writeAttrCB( uint16 connHandle, gattAttribute_t *pAttr, uint8 *pValue, uint16 len, uint16 offset, uint8 method );
static bStatus_t Backlighting_readAttrCB( uint16_t connHandle, gattAttribute_t *pAttr, uint8_t *pValue, uint16_t *pLen, uint16_t offset, uint16_t maxLen, uint8_t method );

/*********************************************************************
 * PROFILE CALLBACKS
 */
const gattServiceCBs_t backlighting_servCBs =
{
  Backlighting_readAttrCB,                // Read callback function pointer
  Backlighting_writeAttrCB,               // Write callback function pointer
  NULL                                    // Authorization callback function pointer
};

/*********************************************************************
* @fn      Backlighting_registerProfileCBs
*
* @brief   Registers the profile callback function. Only call
*          this function once.
*
* @param   profileCallback - pointer to profile callback functions.
*
* @return  SUCCESS or INVALIDPARAMETER
*/
bStatus_t Backlighting_registerProfileCBs( backlightingServiceCB_t *profileCallback )
{
  if ( profileCallback )
  {
    backlighting_profileCBs = profileCallback;

    return ( SUCCESS );
  }

  return ( INVALIDPARAMETER );
}

/*********************************************************************
* @fn      Backlighting_AddService
*
* @brief   This function initializes the Data Stream Server service
*          by registering GATT attributes with the GATT server.
*
* @return  SUCCESS or stack call status
*/

bStatus_t Backlighting_AddService( void )
{
  bStatus_t status = SUCCESS;

  // Allocate Audio Stream Client Characteristic Configuration table
  backlightingProfileControlConfig = (gattCharCfg_t *)ICall_malloc(sizeof(gattCharCfg_t) * MAX_NUM_BLE_CONNS);
  if ( backlightingProfileControlConfig == NULL )
  {
    return bleMemAllocError;
  }
  // Initialize Audio Stream Client Characteristic Configuration attributes
  GATTServApp_InitCharCfg( LINKDB_CONNHANDLE_INVALID, backlightingProfileControlConfig );

  // Register GATT attribute list and CBs with GATT Server
  status = GATTServApp_RegisterService( backlightingProfileAttrTbl,
                                        GATT_NUM_ATTRS( backlightingProfileAttrTbl ),
                                        GATT_MAX_ENCRYPT_KEY_SIZE,
                                        &backlighting_servCBs );
                                     
  // Return status value
  return ( status );
}

/*********************************************************************
 * @fn      Backlighting_setParameter
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
bStatus_t Backlighting_setParameter(uint8 param, void *pValue, uint16 len)
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
 * @fn      Backlighting_writeAttrCB
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


static bStatus_t Backlighting_writeAttrCB( uint16 connHandle,
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

  /* Read the UUID of the Backlight Characteristic */
  uint16_t uuid_char = BUILD_UINT16(pAttr->type.uuid[12], pAttr->type.uuid[13]);
  if (uuid_char == BACKLIGHTING_CHAR_UUID)
  {

    uint8_t event = pValue[0];
    // Parse as big-endian per Google spec: pValue[1] = MSB (high byte), pValue[2] = LSB (low byte)
    uint16_t data = (pValue[1] << 8) | pValue[2];

    // Check minimum length
    if (len < 3)
    {
      // Invalid request - send notification adding INVALID_REQUEST.
      Backlighting_sendNotification(connHandle, event, 1, 0);
      return ATT_ERR_INVALID_VALUE_SIZE;
    }

    if (event == SET_BACKLIGHT_MODE_CMD)
    {
      // Handle SET_BACKLIGHT_MODE_CMD
      backlightServiceData.mod = 1;
      backlightServiceData.backlight_event_type = event;
      backlightServiceData.backlight_time_mode = data;

      // CRITICAL: Update backlightProgramMode immediately!
      // Don't wait for key press, otherwise time command will overwrite mode data
      backlightProgramMode.mode = data;

      // If mode is SCHEDULE (0x0002), prepare for time sync
      if (data == MODE_SCHEDULE)
      {
        // Get current timestamp in milliseconds
        // ClockP_getSystemTickPeriod() returns period in microseconds
        lastModeSetTimestamp = (ClockP_getSystemTicks() * ClockP_getSystemTickPeriod()) / 1000;
        waitingForTimeSync = true;
      }
      else
      {
        waitingForTimeSync = false;
      }

      // INVALID_REQUEST notification  if the SET_BACKLIGHT_MODE_CMD parameter exceeds 0x0002
      if (data > MODE_SCHEDULE)
      {
          // Invalid Mode values adding INVALID_REQUEST.
          Backlighting_sendNotification(connHandle, event, 1, data);
          status = ATT_ERR_INVALID_VALUE;
      }
      else
      {
        // Send notification confirming mode change
        Backlighting_sendNotification(connHandle, event, 0, data);
        status = SUCCESS;
      }

    }
    else if (event == SET_TIME_CMD)
    {
      // Handle SET_TIME_CMD - only valid if within 3 seconds of MODE_SCHEDULE
      // This should also cover the case where the ATV writes first 0x01 without previously writting 0x00.
      if (waitingForTimeSync)
      {
        // Get current timestamp in milliseconds
        // ClockP_getSystemTickPeriod() returns period in microseconds
        uint32_t currentTimestamp = (ClockP_getSystemTicks() * ClockP_getSystemTickPeriod()) / 1000;
        uint32_t elapsed = currentTimestamp - lastModeSetTimestamp;

        if (elapsed <= TIME_SYNC_WINDOW_MS)
        {
          // Valid time sync - parse 0xHHMM format (big-endian)
          uint8_t hours = (data >> 8) & 0xFF;    // MSB
          uint8_t minutes = data & 0xFF;          // LSB

          // Validate hours (0-23) and minutes (0-59)
          if (hours <= 23 && minutes <= 59)
          {
            // Set time using calendar module
            Calendar_setTime(hours, minutes);

            backlightServiceData.mod = 1;
            backlightServiceData.backlight_event_type = event;
            backlightServiceData.backlight_time_mode = data;

            // Clear waiting flag
            waitingForTimeSync = false;

            // Send notification confirming time sync
            Backlighting_sendNotification(connHandle, event, 0, data);
            status = SUCCESS;
          }
          else
          {
            // Invalid time values adding INVALID_REQUEST.
            Backlighting_sendNotification(connHandle, event, 1, data);
            status = ATT_ERR_INVALID_VALUE;
          }
        }
        else
        {
          // Time sync window expired (>3 seconds) adding INVALID_REQUEST.
          waitingForTimeSync = false;
          Backlighting_sendNotification(connHandle, event, 1, data);
          status = ATT_ERR_INVALID_VALUE;
        }
      }
      else
      {
        // SET_TIME_CMD without prior MODE_SCHEDULE or outside window adding INVALID_REQUEST.
        Backlighting_sendNotification(connHandle, event, 1, data);
        status = ATT_ERR_INVALID_VALUE;
      }
    }
    else
    {
      // Unknown event type adding INVALID_REQUEST.
      Backlighting_sendNotification(connHandle, event, 1, data);
      status = ATT_ERR_INVALID_VALUE;
    }
  }

  return (status);
}

/*********************************************************************
 * @fn          Backlighting_sendNotification
 *
 * @brief       Send a notification with event and data
 *
 * @param       connHandle - connection handle
 * @param       event - event type
 * @param       data - 16-bit data value
 *
 * @return      SUCCESS or error status
 */
bStatus_t Backlighting_sendNotification(uint16_t connHandle, uint8_t event, uint8_t invalid_req, uint16_t data)
{
  uint16_t value = GATTServApp_ReadCharCfg(connHandle,
                                           backlightingProfileControlConfig);
  if (value & GATT_CLIENT_CFG_NOTIFY)
  {
    attHandleValueNoti_t noti;

    // Allocate notification buffer
    noti.pValue = GATT_bm_alloc(connHandle, ATT_HANDLE_VALUE_NOTI,
                                BACKLIGHTING_CHAR_CTL_LEN, NULL);
    if (noti.pValue != NULL)
    {
      noti.handle = backlightingProfileAttrTbl[BACKLIGHTING_VALUE_IDX].handle;

      if (invalid_req == 1)
      {
        noti.len = BACKLIGHTING_CHAR_CTL_LEN + 1;
        // Fill notification data: [Event, Data LSB, Data MSB]
        noti.pValue[0] = (uint8_t)INVALID_REQUEST;
        noti.pValue[1] = event;
        noti.pValue[2] = HI_UINT16(data);
        noti.pValue[3] = LO_UINT16(data);
      }
      else
      {
        noti.len = BACKLIGHTING_CHAR_CTL_LEN;
        // Fill notification data: [Event, Data LSB, Data MSB]
        noti.pValue[0] = event;
        noti.pValue[1] = HI_UINT16(data);
        noti.pValue[2] = LO_UINT16(data);
      }

      bStatus_t status = GATT_Notification(connHandle, &noti, FALSE);
      if (status != SUCCESS)
      {
        GATT_bm_free((gattMsg_t *)&noti, ATT_HANDLE_VALUE_NOTI);
      }
      return status;
    }
    return bleMemAllocError;
  }
  return SUCCESS; // Notifications not enabled
}

static bStatus_t Backlighting_readAttrCB( uint16_t connHandle, gattAttribute_t *pAttr,
                                       uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                       uint16_t maxLen, uint8_t method )
{
  bStatus_t status = SUCCESS;

  // If we get here, that means you've forgotten to add an if clause for a
  // characteristic value attribute in the attribute table that has READ permissions.
  *pLen = 0;
  status = ATT_ERR_ATTR_NOT_FOUND;

  return status;
}