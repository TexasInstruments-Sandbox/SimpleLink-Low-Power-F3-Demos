/******************************************************************************

@file  hiddev.c

@brief This file contains the HID main functionalities.

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

/**** Include Libraries ****/

#include <string.h>

/* Include BLE Libraries */
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/app_util/menu/menu_module.h"
#include "ti/ble/app_util/common/util.h"
#include "ble_app/app_peripheral.h"

/* Include BLE Profiles and Services */
#include "ble_app/Profiles/batmon/battservice.h"
#include <ble_app/Profiles/audio_stream/audio_stream_profile.h>
#include "hiddev.h"
#include <ble_app/app_main.h>
#include "ble_app/Profiles/hid/hid_service.h"
#include "keypad/keypad.h"

/**** Define Constants ****/

#define reportQEmpty()                        (firstQIdx == lastQIdx)

#define HID_DEV_DATA_LEN                      9
#ifdef HID_DEV_RPT_QUEUE_LEN
  #define HID_DEV_REPORT_Q_SIZE               (HID_DEV_RPT_QUEUE_LEN+1)
#else
  #define HID_DEV_REPORT_Q_SIZE               (10+1)
#endif

#ifndef MIN
#define MIN(n,m)   (((n) < (m)) ? (n) : (m))
#endif

/**** Define Structs ****/

typedef struct
{
  appEvtHdr_t hdr; // Event header
  uint8_t *pData;  // Event data
} hidDevEvt_t;

typedef struct
{
 uint8_t id;
 uint8_t type;
 uint8_t len;
 uint8_t data[HID_DEV_DATA_LEN];
} hidDevReport_t;


/**** Define Local Functions ****/

extern void Hid_initKeys();
// Task events and processing functions.
static void HidDev_clock(char *pData);
// Process reconnection delay
static void HidDev_reportReady(char *pData);
// HID reports.
static hidRptMap_t *HidDev_reportByHandle(uint16_t handle);
static hidRptMap_t *HidDev_reportById(uint8_t id, uint8_t type);
static hidRptMap_t *HidDev_reportByCccdHandle(uint16_t handle);
static void HidDev_enqueueReport(uint8_t id, uint8_t type, uint8_t len, uint8_t *pData);
static hidDevReport_t *HidDev_dequeueReport(void);
static void HidDev_sendReport(uint8_t id, uint8_t type, uint8_t len, uint8_t *pData);
static uint8_t HidDev_sendNoti(uint16_t handle, uint8_t len, uint8_t *pData);
static void Hid_Send_Report(char *pData);

/**** Define Globals ****/

static Clock_Struct idleTimeoutClock;

static hidRptMap_t *pHidDevRptTbl;

static uint8_t hidDevRptTblLen;

static hidDevCB_t *pHidDevCB;

static hidDevCfg_t *pHidDevCfg;

static uint8_t firstQIdx = 0;
static uint8_t lastQIdx = 0;
static hidDevReport_t hidDevReportQ[HID_DEV_REPORT_Q_SIZE];

// Last report sent out
static hidDevReport_t lastReport = { 0 };

// State when HID reports are ready to be sent out
static Clock_Struct reportReadyClock;

// GAP State
static gaprole_States_t hidDevGapState = GAPROLE_INIT;

// TRUE if connection is secure
uint8_t hidDevConnSecure = FALSE;

extern uint16_t connHandle;

// GAP connection handle
static uint16_t gapConnHandle;

// TRUE if pairing in progress
uint8_t hidDevPairingStarted = FALSE;

// Pairing state
static uint8_t hidDevGapBondPairingState = HID_GAPBOND_PAIRING_STATE_NONE;

// State when HID reports are ready to be sent out
static volatile uint8_t hidDevReportReadyState = TRUE;

// Application GAP Role and GAP Bond states
static gaprole_States_t harGapRoleState = GAPROLE_INIT;

static uint8_t harGapBondState = HID_GAPBOND_PAIRING_STATE_NONE;

static void HIDAdvRemote_handleEventCB(uint8_t evt)
{

  uint32_t hwiKey;

  switch(evt)
  {
    case(HID_DEV_GAPROLE_STATE_CHANGE_EVT):
      // Update application GAP Role state
      HidDev_GetParameter(HIDDEV_GAPROLE_STATE, &harGapRoleState);
      break;

    case(HID_DEV_GAPBOND_STATE_CHANGE_EVT):
      // Update application GAP Bond pairing state
      HidDev_GetParameter(HIDDEV_GAPBOND_STATE, &harGapBondState);

      break;

    default:
      // Do nothing
      break;
  }

  return;
}

// HID Dev callbacks
static hidDevCB_t hidAdvRemoteHidCBs =
{
  (void *)HIDAdvRemote_handleEventCB,
};

// HID Dev configuration
static hidDevCfg_t hidAdvRemoteCfg =
{
  DEFAULT_HID_IDLE_TIMEOUT,   // Idle timeout
  HID_KBD_FLAGS               // HID feature flags
};

bStatus_t HidDev_start(void)
{
  bStatus_t status = SUCCESS;
  uint8_t  *devAddr = NULL;

  Batt_AddService();

  Util_constructClock(&reportReadyClock, (void *)BLEAppUtil_invokeFunctionNoData,
                      HID_REPORT_READY_TIME, 0, false, (uint32)HidDev_reportReady);

  // Add HID keyboard service
  status = HidKbdCC_AddService();
  // Register for HID Dev callback
  HidDev_Register(&hidAdvRemoteCfg, &hidAdvRemoteHidCBs);

  Hid_initKeys();

  return status;
}

static void HidDev_reportReady( char *pData )
{
    // Allow reports to be sent
    hidDevReportReadyState = TRUE;
    // If there are reports in the queue
     if (!reportQEmpty())
     {
         // If connection is secure
         if (hidDevConnSecure && hidDevReportReadyState)
         {
           hidDevReport_t *pReport = HidDev_dequeueReport();

           if (pReport != NULL)
           {
             // Send report.
             HidDev_sendReport(pReport->id, pReport->type, pReport->len,
                               pReport->data);
           }

           // If there is another report in the queue
           if (!reportQEmpty())
           {
             Util_rescheduleClock(&reportReadyClock,1);

           }
         }

     }
}

void HidDev_Register(hidDevCfg_t *pCfg, hidDevCB_t *pCBs)
{
  pHidDevCB = pCBs;
  pHidDevCfg = pCfg;

  // If configured and not zero, create the idle timeout clock.
  if ((pHidDevCfg != NULL) && (pHidDevCfg->idleTimeout != 0))
  {
    Util_constructClock(&idleTimeoutClock, (void *)BLEAppUtil_invokeFunctionNoData,
                        pHidDevCfg->idleTimeout, 0, false, (uint32)HidDev_clock);

  }
}

void HidDev_RegisterReports(uint8_t numReports, hidRptMap_t *pRpt)
{
  pHidDevRptTbl = pRpt;
  hidDevRptTblLen = numReports;
}

void HidDev_Report(uint8_t id, uint8_t type, uint8_t len, uint8_t *pData)
{
  // Validate length of report
  if ( len > HID_DEV_DATA_LEN )
  {
    return;
  }
  // If connection is secure
  if (hidDevConnSecure)
  {
    // Make sure there're no pending reports.
    if (reportQEmpty())
    {
      // Send report.
      HidDev_sendReport(id, type, len, pData);
      return;
    }
  }
  HidDev_enqueueReport(id, type, len, pData);
}

void HidDev_Close(void)
{
  uint8_t param;

  // If connected then disconnect.
  if (hidDevGapState == GAPROLE_CONNECTED)
  {
    BLEAppUtil_disconnect(gapConnHandle);
  }
}

bStatus_t HidDev_GetParameter(uint8_t param, void *pValue)
{
  bStatus_t ret = SUCCESS;

  switch (param)
  {
    case HIDDEV_GAPROLE_STATE:
      *((uint8_t*)pValue) = hidDevGapState;
      break;

    case HIDDEV_GAPBOND_STATE:
      *((uint8_t*)pValue) = hidDevGapBondPairingState;
      break;

    default:
      ret = INVALIDPARAMETER;
      break;
  }

  return (ret);
}

static hidRptMap_t *HidDev_reportByCccdHandle(uint16_t handle)
{
  uint8_t i;
  hidRptMap_t *p = pHidDevRptTbl;

  for (i = hidDevRptTblLen; i > 0; i--, p++)
  {
    if ((p->pCccdAttr != NULL) && (p->pCccdAttr->handle == handle))
    {
      return p;
    }
  }

  return NULL;
}

static hidRptMap_t *HidDev_reportById(uint8_t id, uint8_t type)
{
  uint8_t i;
  hidRptMap_t *p = pHidDevRptTbl;

  for (i = hidDevRptTblLen; i > 0; i--, p++)
  {
    if (p->id == id && p->type == type && p->mode == hidProtocolMode)
    {
      return p;
    }
  }

  return NULL;
}

static void HidDev_sendReport(uint8_t id, uint8_t type, uint8_t len, uint8_t *pData)
{
  hidRptMap_t *pRpt;
  // Get ATT handle for report.
  if ((pRpt = HidDev_reportById(id, type)) != NULL)
  {
    uint8_t value = GATTServApp_ReadCharCfg(gapConnHandle, GATT_CCC_TBL(pRpt->pCccdAttr->pValue));
    if (HidDev_sendNoti(pRpt->handle, len, pData) == SUCCESS)
    {
      // Save the report just sent out
      lastReport.id = id;
      lastReport.type = type;
      lastReport.len = len;
      memcpy(lastReport.data, pData, len);
    }
  }
}

static uint8_t HidDev_sendNoti(uint16_t handle, uint8_t len, uint8_t *pData)
{
  uint8_t status;
  attHandleValueNoti_t noti;


  noti.pValue = GATT_bm_alloc(gapConnHandle, ATT_HANDLE_VALUE_NOTI, len, NULL);
  if (noti.pValue != NULL)
  {
    noti.handle = handle;
    noti.len = len;
    memcpy(noti.pValue, pData, len);

    // Send notification
    status = GATT_Notification(gapConnHandle, &noti, FALSE);
    if (status != SUCCESS)
    {
      GATT_bm_free((gattMsg_t *)&noti, ATT_HANDLE_VALUE_NOTI);
    }
  }
  else
  {
    status = bleMemAllocError;
  }

  return status;
}

static void HidDev_enqueueReport(uint8_t id, uint8_t type, uint8_t len,
                                 uint8_t *pData)
{
  uint8_t bondCnt = 0;
  VOID GAPBondMgr_GetParameter(GAPBOND_BOND_COUNT, &bondCnt);
  // Enqueue only if bonded.
  if (bondCnt > 0)
  {
    // Update last index.
    lastQIdx = (lastQIdx + 1) % HID_DEV_REPORT_Q_SIZE;

    if (lastQIdx == firstQIdx)
    {
      // Queue overflow; discard oldest report.
      firstQIdx = (firstQIdx + 1) % HID_DEV_REPORT_Q_SIZE;
    }

    // Save report.
    hidDevReportQ[lastQIdx].id = id;
    hidDevReportQ[lastQIdx].type = type;
    hidDevReportQ[lastQIdx].len = len;
    memcpy(hidDevReportQ[lastQIdx].data, pData, len);

    if (hidDevConnSecure)
    {
      // Notify our task to send out pending reports.
        BLEAppUtil_invokeFunctionNoData(Hid_Send_Report);

    }
  }
}

static hidDevReport_t *HidDev_dequeueReport(void)
{
  if (reportQEmpty())
  {
    return NULL;
  }

  // Update first index.
  firstQIdx = (firstQIdx + 1) % HID_DEV_REPORT_Q_SIZE;

  return (&(hidDevReportQ[firstQIdx]));
}

static void HidDev_clock(char *pData)
{
    uint16_t connHandle=0;
    uint8_t index=0;
    if (hidDevGapState == GAPROLE_CONNECTED)
    {
        // If pairing in progress then restart timer.
        if (hidDevPairingStarted)
        {
        }
        // Else disconnect and don't allow reports to be sent
        else
        {
          hidDevReportReadyState = FALSE;

          for(index =0 ; index< MAX_NUM_BLE_CONNS ; index++)
          {
              connHandle = Connection_getConnhandle(index);
              if(MAX_NUM_BLE_CONNS != connHandle)
              {
                  break;
              }
          }
          BLEAppUtil_disconnect(connHandle);

        }
    }

}

bStatus_t HidDev_ReadAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                            uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                            uint16_t maxLen, uint8_t method)
{
  bStatus_t   status = SUCCESS;
  hidRptMap_t *pRpt;

  uint16_t uuid = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);

  // Only report map is long.
  if (offset > 0 && uuid != REPORT_MAP_UUID)
  {
    return (ATT_ERR_ATTR_NOT_LONG);
  }

  if (uuid == REPORT_UUID ||
      uuid == BOOT_KEY_INPUT_UUID ||
      uuid == BOOT_KEY_OUTPUT_UUID ||
      uuid == BOOT_MOUSE_INPUT_UUID)
  {
    // Find report ID in table.
    if ((pRpt = HidDev_reportByHandle(pAttr->handle)) != NULL)
    {
      // Execute report callback.
      status  = (*pHidDevCB->reportCB)(pRpt->id, pRpt->type, uuid,
                                        HID_DEV_OPER_READ, pLen, pValue);
    }
    else
    {
      *pLen = 0;
    }
  }
  else if (uuid == REPORT_MAP_UUID)
  {
    // If the value offset of the Read Blob Request is greater than the
    // length of the attribute value, an Error Response shall be sent with
    // the error code Invalid Offset.
    if (offset > hidReportMapLen)
    {
      status = ATT_ERR_INVALID_OFFSET;
    }
    else
    {
      // Determine read length.
      *pLen = MIN(maxLen, (hidReportMapLen - offset));

      // Copy data.
      memcpy(pValue, pAttr->pValue + offset, *pLen);
    }
  }
  else if (uuid == HID_INFORMATION_UUID)
  {
    *pLen = HID_INFORMATION_LEN;
    memcpy(pValue, pAttr->pValue, HID_INFORMATION_LEN);
  }
  else if (uuid == GATT_REPORT_REF_UUID)
  {
    *pLen = HID_REPORT_REF_LEN;
    memcpy(pValue, pAttr->pValue, HID_REPORT_REF_LEN);
  }
  else if (uuid == PROTOCOL_MODE_UUID)
  {
    *pLen = HID_PROTOCOL_MODE_LEN;
    pValue[0] = pAttr->pValue[0];
  }
  else if (uuid == GATT_EXT_REPORT_REF_UUID)
  {
    *pLen = HID_EXT_REPORT_REF_LEN;
    memcpy(pValue, pAttr->pValue, HID_EXT_REPORT_REF_LEN);
  }
  // Restart idle timer.
  if (status == SUCCESS)
  {
#if IDLE_TIMER_EN
          HidDev_StartIdleTimer();
#endif
  }

  return (status);
}

bStatus_t HidDev_WriteAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                             uint8_t *pValue, uint16_t len, uint16_t offset,
                             uint8_t method)
{
  bStatus_t   status = SUCCESS;
  hidRptMap_t *pRpt;

  // Make sure it's not a blob operation (no attributes in the profile are long).
  if (offset > 0)
  {
    return (ATT_ERR_ATTR_NOT_LONG);
  }

  uint16_t uuid = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);

  if (uuid == REPORT_UUID || uuid == BOOT_KEY_OUTPUT_UUID)
  {
    // Find report ID in table.
    if ((pRpt = HidDev_reportByHandle(pAttr->handle)) != NULL)
    {
      // Execute report callback.
      status  = (*pHidDevCB->reportCB)(pRpt->id, pRpt->type, uuid,
                                        HID_DEV_OPER_WRITE, &len, pValue);
    }
  }
  else if (uuid == HID_CTRL_PT_UUID)
  {
    // Validate length and value range.
    if (len == 1)
    {
      if (pValue[0] == HID_CMD_SUSPEND ||  pValue[0] == HID_CMD_EXIT_SUSPEND)
      {
        // Execute HID app event callback.
        (*pHidDevCB->evtCB)((pValue[0] == HID_CMD_SUSPEND) ?
                             HID_DEV_SUSPEND_EVT : HID_DEV_EXIT_SUSPEND_EVT);
      }
      else
      {
        status = ATT_ERR_INVALID_VALUE;
      }
    }
    else
    {
      status = ATT_ERR_INVALID_VALUE_SIZE;
    }
  }
  else if (uuid == GATT_CLIENT_CHAR_CFG_UUID)
  {
    status = GATTServApp_ProcessCCCWriteReq(connHandle, pAttr, pValue, len,
                                            offset, GATT_CLIENT_CFG_NOTIFY);
    if (status == SUCCESS)
    {
      uint16_t charCfg = BUILD_UINT16(pValue[0], pValue[1]);

      // Find report ID in table.
      if ((pRpt = HidDev_reportByCccdHandle(pAttr->handle)) != NULL)
      {
        // Execute report callback.
        (*pHidDevCB->reportCB)(pRpt->id, pRpt->type, uuid,
                               (charCfg == GATT_CLIENT_CFG_NOTIFY) ?
                               HID_DEV_OPER_ENABLE : HID_DEV_OPER_DISABLE,
                               &len, pValue);
      }
    }
  }
  else if (uuid == PROTOCOL_MODE_UUID)
  {
    if (len == HID_PROTOCOL_MODE_LEN)
    {
      if (pValue[0] == HID_PROTOCOL_MODE_BOOT ||
          pValue[0] == HID_PROTOCOL_MODE_REPORT)
      {
        pAttr->pValue[0] = pValue[0];

        // Execute HID app event callback.
        (*pHidDevCB->evtCB)((pValue[0] == HID_PROTOCOL_MODE_BOOT) ?
                            HID_DEV_SET_BOOT_EVT : HID_DEV_SET_REPORT_EVT);
      }
      else
      {
        status = ATT_ERR_INVALID_VALUE;
      }
    }
    else
    {
      status = ATT_ERR_INVALID_VALUE_SIZE;
    }
  }

  // Restart idle timer.
  if (status == SUCCESS)
  {
#if IDLE_TIMER_EN
          HidDev_StartIdleTimer();
#endif
  }
  return (status);
}

void HidDev_StartIdleTimer(void)
{
  if ((pHidDevCfg != NULL) && (pHidDevCfg->idleTimeout > 0))
  {
    Util_startClock(&idleTimeoutClock);
  }

}

void HidDev_StopIdleTimer(void)
{
  if ((pHidDevCfg != NULL) && (pHidDevCfg->idleTimeout > 0))
  {
    Util_stopClock(&idleTimeoutClock);
  }
}

static hidRptMap_t *HidDev_reportByHandle(uint16_t handle)
{
  uint8_t i;
  hidRptMap_t *p = pHidDevRptTbl;

  for (i = hidDevRptTblLen; i > 0; i--, p++)
  {
    if (p->handle == handle && p->mode == hidProtocolMode)
    {
      return p;
    }
  }

  return NULL;
}

static void Hid_Send_Report(char *pData)
{
    // If connection is secure
    if (hidDevConnSecure && hidDevReportReadyState)
    {
      hidDevReport_t *pReport = HidDev_dequeueReport();
      if (pReport != NULL)
      {
        // Send report.
        HidDev_sendReport(pReport->id, pReport->type, pReport->len, pReport->data);
      }
      // If there is another report in the queue
      if (!reportQEmpty())
      {
        BLEAppUtil_invokeFunctionNoData(Hid_Send_Report);
      }
    }
}