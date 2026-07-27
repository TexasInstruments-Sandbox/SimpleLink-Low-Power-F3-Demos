/******************************************************************************

@file  fmr_service.c

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
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include <ti/drivers/BatteryMonitor.h>
#include "fmr_service.h"

// Access to buzz_count from pwm_buzzer.c (remaining buzz duration in 100ms units)
extern uint16_t buzz_count;

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPEDEFS
 */

#define FMR_SERVICE_UUID    1
#define FMR_CHAR_UUID       2

#define FMR_LOW_BAT         30

/*********************************************************************
 * LOCAL VARIABLES
 */

/// @brief Android Base 128-bit UUID: F000XXXX-0451-4000-B000-000000000000
#define FMR_BASE_UUID_128( uuid )  0x64, 0xB6, 0x17, 0xF6, 0x01, 0xAF, 0x7D, 0xBC, \
                                  0x05, 0x4F, 0x21, 0x5A, LO_UINT16( uuid ), HI_UINT16( uuid ), 0x03, 0x18

static uint16_t heapHeadroom = 0;

// Data queue to hold the outgoing data
static List_List streamOutQueue;

// FMR_SERVICE
static const uint8 fmrProfileServUUID[ATT_UUID_SIZE] =
{
    FMR_BASE_UUID_128(FMR_SERVICE_UUID)
};

// FMR_CHAR
static const uint8 fmrCharUUID[ATT_UUID_SIZE] =
{
    FMR_BASE_UUID_128(FMR_CHAR_UUID)
};

//----------------------------------------------------------------------//

/* Last Event: 8-bit enum + Remaining Buzz Duration: unsigned 16-bit integer */
#define FMR_CHAR_CTL_LEN  3
#define FMR_VALUE_IDX     2

// FMR_SERVICE_UUID
// FMR Profile Service
static const gattAttrType_t fmrProfileService = {ATT_UUID_SIZE, fmrProfileServUUID};

// CONTROL Characteristic Properties
static uint8 fmrProfileControlProps = GATT_PROP_WRITE | GATT_PROP_READ | GATT_PROP_NOTIFY;
// CONTROL Characteristic Value
static uint8_t fmrProfileControl[FMR_CHAR_CTL_LEN];
// CONTROL Characteristic Configuration Descriptor Value
static gattCharCfg_t *fmrProfileControlConfig;

static fmrServiceCB_t *fmr_profileCBs = NULL;

FMR_service_dataformat FMRServiceData;

/*********************************************************************
 * Profile Attributes - Table
 */

static gattAttribute_t fmrProfileAttrTbl[] =
{
 /*--------------------type-------------------*/ /*------------permissions-------------*/ /*------------------pValue--------------------*/
   // FMR Primary Service
   GATT_BT_ATT( primaryServiceUUID,                 GATT_PERMIT_READ,                                   (uint8 *)&fmrProfileService ),

   // FMR Characteristic Declaration
   GATT_BT_ATT( characterUUID,                      GATT_PERMIT_READ,                                    &fmrProfileControlProps ),
   // FMR Characteristic Value
   GATT_ATT( fmrCharUUID,                           GATT_PERMIT_READ | GATT_PERMIT_WRITE,                fmrProfileControl ),
   // FMR Characteristic configuration
   GATT_BT_ATT( clientCharCfgUUID,                  GATT_PERMIT_READ | GATT_PERMIT_WRITE,                (uint8 *)&fmrProfileControlConfig ),
};

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static bStatus_t FMR_writeAttrCB( uint16 connHandle, gattAttribute_t *pAttr, uint8 *pValue, uint16 len, uint16 offset, uint8 method );
static bStatus_t FMR_readAttrCB( uint16_t connHandle, gattAttribute_t *pAttr, uint8_t *pValue, uint16_t *pLen, uint16_t offset, uint16_t maxLen, uint8_t method );

/*********************************************************************
 * PROFILE CALLBACKS
 */
const gattServiceCBs_t fmr_servCBs =
{
  FMR_readAttrCB,                 // Read callback function pointer
  FMR_writeAttrCB,                // Write callback function pointer
  NULL                           // Authorization callback function pointer
};

/*********************************************************************
* @fn      FMR_registerProfileCBs
*
* @brief   Registers the profile callback function. Only call
*          this function once.
*
* @param   profileCallback - pointer to profile callback functions.
*
* @return  SUCCESS or INVALIDPARAMETER
*/
bStatus_t FMR_registerProfileCBs( fmrServiceCB_t *profileCallback )
{
  if ( profileCallback )
  {
    fmr_profileCBs = profileCallback;

    return ( SUCCESS );
  }

  return ( INVALIDPARAMETER );
}

/*********************************************************************
* @fn      FMR_AddService
*
* @brief   This function initializes the Data Stream Server service
*          by registering GATT attributes with the GATT server.
*
* @return  SUCCESS or stack call status
*/

bStatus_t FMR_AddService( void )
{
  bStatus_t status = SUCCESS;

  // Allocate Audio Stream Client Characteristic Configuration table
  fmrProfileControlConfig = (gattCharCfg_t *)ICall_malloc(sizeof(gattCharCfg_t) * MAX_NUM_BLE_CONNS);
  if ( fmrProfileControlConfig == NULL )
  {
    return bleMemAllocError;
  }
  // Initialize Audio Stream Client Characteristic Configuration attributes
  GATTServApp_InitCharCfg( LINKDB_CONNHANDLE_INVALID, fmrProfileControlConfig );

  // Register GATT attribute list and CBs with GATT Server
  status = GATTServApp_RegisterService( fmrProfileAttrTbl,
                                        GATT_NUM_ATTRS( fmrProfileAttrTbl ),
                                        GATT_MAX_ENCRYPT_KEY_SIZE,
                                        &fmr_servCBs );

  // Return status value
  return ( status );
}

/*********************************************************************
 * @fn      FMR_setParameter
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
bStatus_t FMR_setParameter(uint8 param, void *pValue, uint16 len)
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
 * @fn      FMR_writeAttrCB
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

static bStatus_t FMR_writeAttrCB( uint16 connHandle,
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

  status = SUCCESS;
  /* Read the UUID of the FMR characteristic */
  uuid_char = BUILD_UINT16(pAttr->type.uuid[12], pAttr->type.uuid[13]);

  if (uuid_char == FMR_CHAR_UUID)
  {
    /* Read the values of the FMR characteristic */
    FMRServiceData.fmr_event_type = pValue[0];

    /* If the ATV writes [0x00, 0x01, 0x2C] or any other value between 0 to 15 min, then the RC should buzz for X defined seconds. */
    if (FMRServiceData.fmr_event_type == PEER_REQUEST)
    {
      /* If the Mode is valid, then read the time value */
      FMRServiceData.fmr_time_mode  = BUILD_UINT16(pValue[2], pValue[1]);

      uint16_t currentVoltage =0;
      currentVoltage = BatteryMonitor_getVoltage();
      uint16_t batterylevel = ((currentVoltage* 100) / 3273);

      if (batterylevel >= FMR_LOW_BAT)
      {
        /* If the ATV attemps to buzz for 1h or more, then the Duration is ceiled to 15 min */
        if (FMRServiceData.fmr_time_mode >= 0x8CA0) // 1h
        {
          /* RCU notifies: [0x03, 0x23, 0x28], meaning that the duration is ceiled to 15 min */
          FMRServiceData.fmr_event_type = DURATION_TOO_LONG;
          FMRServiceData.fmr_time_mode = 0x2328; // 15 min
        }
        FMR_notify(0, FMRServiceData);
        Buzzing_trigger();
      }
      /* If the RC has Low battery, then dont buzz and notify 
      RCU notifies: [0x04, 0x00, 0x00] // Low battery
      */
      else
      {
        FMRServiceData.fmr_event_type = LOW_BATTERY;
        FMRServiceData.fmr_time_mode = 0x00;
        FMR_notify(0, FMRServiceData);
      }
    }
    /* If the ATV receives an invalid request */
    else
    {
      FMRServiceData.fmr_event_type = FMR_INVALID_REQUEST;
      FMR_notify(0, FMRServiceData);
    } 
  }

  return (status);
}

/*********************************************************************
 * @fn          FMR_notify
 *
 * @brief       Send a notification of the level state characteristic.
 *
 * @param       connHandle - linkDB item
 *
 * @return      None.
 */
void FMR_notify(uint16_t connHandle, FMR_service_dataformat fmr_noti)
{
  uint16_t value = GATTServApp_ReadCharCfg(connHandle,
                                           fmrProfileControlConfig);
  if (value & GATT_CLIENT_CFG_NOTIFY)
  {
    attHandleValueNoti_t noti;

    noti.pValue = GATT_bm_alloc(connHandle, ATT_HANDLE_VALUE_NOTI,
                                FMR_CHAR_CTL_LEN, NULL);
    if (noti.pValue != NULL)
    {
      noti.handle = fmrProfileAttrTbl[FMR_VALUE_IDX].handle;
      noti.len = sizeof(fmr_noti);
      noti.pValue[0] = fmr_noti.fmr_event_type;
      noti.pValue[1] = HI_UINT16(fmr_noti.fmr_time_mode);
      noti.pValue[2] = LO_UINT16(fmr_noti.fmr_time_mode);
      if (GATT_Notification(connHandle, &noti, FALSE) != SUCCESS)
      {
        GATT_bm_free((gattMsg_t *)&noti, ATT_HANDLE_VALUE_NOTI);
      }
    }
  }
}

static bStatus_t FMR_readAttrCB( uint16_t connHandle, gattAttribute_t *pAttr,
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

  /* Reading the current state of the FMR Clock
   * Per Google Find My Remote v1.0 spec:
   *
   * ATV writes: [0x00, 0x01, 0x2C] // Buzz for 30 seconds (300 * 100ms = 30s)
   * RCU notifies: [0x00, 0x01, 0x2C]
   * … 10 seconds later …
   * ATV reads: [0x00, 0x00, 0xC8] // 20 seconds remaining (200 * 100ms = 20s)
   *
   * Format: [Last Event Type, Duration MSB, Duration LSB]
   * - Last Event Type: Most recent FMR event (PEER_REQUEST, RCU_FOUND, DURATION_ELAPSED, etc.)
   * - Duration: Remaining buzz duration in 100ms units (big-endian)
   */

  // Check if this is the FMR characteristic
  uint16_t uuid_char = BUILD_UINT16(pAttr->type.uuid[12], pAttr->type.uuid[13]);
  if (uuid_char == FMR_CHAR_UUID)
  {
    // Return 3 bytes: [Event, Duration MSB, Duration LSB]
    if (maxLen < FMR_CHAR_CTL_LEN)
    {
      return ATT_ERR_INVALID_VALUE_SIZE;
    }

    // Byte 0: Last event type from FMR service data
    pValue[0] = FMRServiceData.fmr_event_type;

    // Bytes 1-2: Remaining buzz duration (big-endian)
    // buzz_count is in 100ms units, decremented every 100ms by buzzer timer
    pValue[1] = HI_UINT16(buzz_count);  // MSB
    pValue[2] = LO_UINT16(buzz_count);  // LSB

    *pLen = FMR_CHAR_CTL_LEN;
    status = SUCCESS;
  }
  else
  {
    *pLen = 0;
    status = ATT_ERR_ATTR_NOT_FOUND;
  }

  return status;
}