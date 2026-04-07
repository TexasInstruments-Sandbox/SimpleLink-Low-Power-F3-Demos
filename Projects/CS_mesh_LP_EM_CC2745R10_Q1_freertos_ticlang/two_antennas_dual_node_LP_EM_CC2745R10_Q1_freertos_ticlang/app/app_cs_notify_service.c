/******************************************************************************

@file  app_cs_notify_service.c

@brief This file contains a simple GATT service for CS completion notification.
       The key node hosts this service, and the car node writes to it to signal
       that channel sounding has completed successfully.

Group: WCS, BTS
Target Device: cc23xx

******************************************************************************

 Copyright (c) 2025-2026, Texas Instruments Incorporated
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


*****************************************************************************/

//*****************************************************************************
//! Includes
//*****************************************************************************
#include <string.h>
#include "ti/ble/stack_util/bcomdef.h"
#include "ti/ble/stack_util/icall/app/icall.h"
#include "ti/ble/host/gatt/gatt.h"
#include "ti/ble/host/gatt/gatt_uuid.h"
#include "ti/ble/host/gatt/gattservapp.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "app_cs_notify_service.h"
#include "app_key_node.h"

#include <ti/drivers/UART2.h>
#include <FreeRTOS.h>
#include <timers.h>

#include DeviceFamily_constructPath(driverlib/hapi.h)
#include DeviceFamily_constructPath(inc/hw_pmctl.h)
#include DeviceFamily_constructPath(inc/hw_types.h)

//*****************************************************************************
//! Externs
//*****************************************************************************
extern UART2_Handle uart;

//*****************************************************************************
//! Defines
//*****************************************************************************

// CS Notify Service UUID: 0xB0B0
#define CS_NOTIFY_SERVICE_UUID          0xB0B0

// CS Complete Characteristic UUID: 0xB0B1
#define CS_NOTIFY_CHAR_UUID             0xB0B1

// Position of CS Complete characteristic value in attribute table
#define CS_NOTIFY_CHAR_VALUE_POS        2

// Number of attributes in the service
#define CS_NOTIFY_NUM_ATTRS             4

//*****************************************************************************
//! Typedefs
//*****************************************************************************

//*****************************************************************************
//! Local Variables
//*****************************************************************************

// CS Notify Service UUID (raw bytes)
static const uint8_t csNotifyServUUID[ATT_UUID_SIZE] =
{
  TI_BASE_UUID_128(CS_NOTIFY_SERVICE_UUID)
};

// CS Notify Service UUID wrapped in gattAttrType_t for service declaration
static const gattAttrType_t csNotifyService =
{
  ATT_UUID_SIZE,
  csNotifyServUUID
};

// CS Complete Characteristic UUID
static const uint8_t csNotifyCharUUID[ATT_UUID_SIZE] =
{
  TI_BASE_UUID_128(CS_NOTIFY_CHAR_UUID)
};

// CS Complete Characteristic value
static uint8_t csNotifyCharValue = 0;

// CS Complete Characteristic user description
static uint8_t csNotifyCharUserDesc[] = "CS Complete";

// Characteristic properties
static uint8_t csNotifyCharProps = GATT_PROP_WRITE;

// Timer for delayed reset (100ms delay to allow write response to be sent)
static TimerHandle_t resetTimerHandle = NULL;

// Timer callback for delayed reset
static void CSNotifyService_resetTimerCB(TimerHandle_t xTimer)
{
    HWREG(PMCTL_BASE + PMCTL_O_AONRCLR1) |= PMCTL_AONRCLR1_FLAG_M;
    HapiResetDevice();
}

//*****************************************************************************
//! Local Function Prototypes
//*****************************************************************************

static bStatus_t CSNotifyService_readAttrCB(uint16_t connHandle,
                                            gattAttribute_t *pAttr,
                                            uint8_t *pValue, uint16_t *pLen,
                                            uint16_t offset, uint16_t maxLen,
                                            uint8_t method);

static bStatus_t CSNotifyService_writeAttrCB(uint16_t connHandle,
                                             gattAttribute_t *pAttr,
                                             uint8_t *pValue, uint16_t len,
                                             uint16_t offset, uint8_t method);

//*****************************************************************************
//! Profile Attributes - Table
//*****************************************************************************

static gattAttribute_t csNotifyServiceAttrTbl[CS_NOTIFY_NUM_ATTRS] =
{
  // CS Notify Service Declaration
  {
    { ATT_BT_UUID_SIZE, primaryServiceUUID }, // type
    GATT_PERMIT_READ,                         // permissions
    0,                                        // handle
    (uint8_t *)&csNotifyService               // pValue - pointer to gattAttrType_t
  },

  // CS Complete Characteristic Declaration
  {
    { ATT_BT_UUID_SIZE, characterUUID },
    GATT_PERMIT_READ,
    0,
    &csNotifyCharProps
  },

  // CS Complete Characteristic Value
  {
    { ATT_UUID_SIZE, csNotifyCharUUID },
    GATT_PERMIT_WRITE,                        // Only writable
    0,
    &csNotifyCharValue
  },

  // CS Complete Characteristic User Description
  {
    { ATT_BT_UUID_SIZE, charUserDescUUID },
    GATT_PERMIT_READ,
    0,
    csNotifyCharUserDesc
  },
};

//*****************************************************************************
//! Profile Callbacks
//*****************************************************************************

// CS Notify Service Callbacks
static const gattServiceCBs_t csNotifyServiceCBs =
{
  CSNotifyService_readAttrCB,   // Read callback
  CSNotifyService_writeAttrCB,  // Write callback
  NULL                          // Authorization callback
};

//*****************************************************************************
//! Public Functions
//*****************************************************************************

/*********************************************************************
 * @fn      CSNotifyService_addService
 *
 * @brief   Initializes the CS Notify Service by registering
 *          GATT attributes with the GATT server.
 *
 * @return  SUCCESS or error code
 */
bStatus_t CSNotifyService_addService(void)
{
  // Create the reset delay timer (100ms one-shot)
  resetTimerHandle = xTimerCreate("ResetTimer",
                                  pdMS_TO_TICKS(100),
                                  pdFALSE,  // One-shot
                                  NULL,
                                  CSNotifyService_resetTimerCB);

  // Register GATT attribute list and callbacks with GATT Server App
  return GATTServApp_RegisterService(csNotifyServiceAttrTbl,
                                     CS_NOTIFY_NUM_ATTRS,
                                     GATT_MAX_ENCRYPT_KEY_SIZE,
                                     &csNotifyServiceCBs);
}

//*****************************************************************************
//! Local Functions
//*****************************************************************************

/*********************************************************************
 * @fn      CSNotifyService_readAttrCB
 *
 * @brief   Read an attribute.
 *
 * @param   connHandle - connection message was received on
 * @param   pAttr - pointer to attribute
 * @param   pValue - pointer to data to be read
 * @param   pLen - length of data to be read
 * @param   offset - offset of the first octet to be read
 * @param   maxLen - maximum length of data to be read
 * @param   method - type of read message
 *
 * @return  SUCCESS, blePending or Failure
 */
static bStatus_t CSNotifyService_readAttrCB(uint16_t connHandle,
                                            gattAttribute_t *pAttr,
                                            uint8_t *pValue, uint16_t *pLen,
                                            uint16_t offset, uint16_t maxLen,
                                            uint8_t method)
{
  // This characteristic is write-only, so return error for reads
  return ATT_ERR_READ_NOT_PERMITTED;
}

/*********************************************************************
 * @fn      CSNotifyService_writeAttrCB
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
 * @return  SUCCESS, blePending or Failure
 */
static bStatus_t CSNotifyService_writeAttrCB(uint16_t connHandle,
                                             gattAttribute_t *pAttr,
                                             uint8_t *pValue, uint16_t len,
                                             uint16_t offset, uint8_t method)
{
  bStatus_t status = SUCCESS;

  // Check if this is the CS Complete characteristic
  if (pAttr->handle == csNotifyServiceAttrTbl[CS_NOTIFY_CHAR_VALUE_POS].handle)
  {
    // Validate length
    if (len != 1)
    {
      status = ATT_ERR_INVALID_VALUE_SIZE;
    }
    else if (offset != 0)
    {
      status = ATT_ERR_ATTR_NOT_LONG;
    }
    else
    {
      // Write the value - any value received triggers restart
      csNotifyCharValue = pValue[0];

      UART2_write(uart, "CS complete notification received!\r\n", 37, NULL);

      // Start 100ms delay timer before reset (allows write response to be sent)
      if (resetTimerHandle != NULL)
      {
          xTimerStart(resetTimerHandle, 0);
      }
    }
  }
  else
  {
    status = ATT_ERR_ATTR_NOT_FOUND;
  }

  return status;
}
