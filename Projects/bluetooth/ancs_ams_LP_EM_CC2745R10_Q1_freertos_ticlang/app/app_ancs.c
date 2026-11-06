/**
 * @file app_ancs.c
 * @brief Apple Notification Center Service (ANCS) and Apple Media Service (AMS) BLE client implementation for CC27xx.
 *
 * This file contains the state machine, GATT event handler, and service/characteristic discovery logic
 * for interacting with ANCS and AMS on iOS devices over Bluetooth Low Energy.
 *
 * Features:
 * - Discovers ANCS and AMS services and characteristics
 * - Subscribes to notification and data source characteristics
 * - Handles incoming notifications and data updates
 * - Manages CCCD writes for enabling notifications
 * - Provides event handler for BLEAppUtil framework
 *
 * Usage:
 * - Call ANCS_start() after BLE stack initialization to register handlers
 * - The state machine will automatically discover and subscribe to required services
 * - Notification and data events are processed via ANCS_AMS_EventHandler
 *
 * Copyright (c) 2022-2025 Texas Instruments Incorporated
 * All rights reserved.

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
 */

// ========================= Section: Includes =========================
#include <string.h>
#include "ti_drivers_config.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/app_util/menu/menu_module.h"
#include <ti/display/Display.h>
#include <ti/display/DisplayUart2.h>
#include <app_main.h>
#include "app_ancs.h"

// ========================== Section: Prototypes =========================
static void ANCS_AMS_EventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData);
static void gatt_disc_char_wrapper();
static void gatt_disc_cccd();
static void gatt_write_cccd_wrapper();
static void gatt_disc_ams_wrapper();
static void gatt_write_cccd_execute();

static void delayed_AMS_Requests_wrapper(uintptr_t arg);
static void delayed_AMS_Requests(char * arg);
ClockP_Handle c_ams_req;
static ClockP_Params ams_req_params;

// ========================= Section: Type Definitions =========================
typedef enum {
    DISCOVER_ANCS_SERVICE,
    DISCOVER_AMS_SERVICE,
    DISCOVER_ANCS_CHARS,
    DISCOVER_AMS_CHARS,
    DISCOVER_ANCS_NOTI_CCCD,
    DISCOVER_ANCS_DATASRC_CCCD,
    DISCOVER_AMS_REMOTE_COM_CCCD,
    DISCOVER_AMS_ENT_UPDT_CCCD,
    ANCS_NOTI_SUB,
    ANCS_DATA_SUB,
    AMS_REMOTE_SUB,
    AMS_ENT_UPDT_SUB,
    REQ_AMS_UPDATES,
    IDLE
} ancs_state_t;

typedef struct CCCD_info_t
{
  uint16_t attrHdl;
  uint8_t cccd_conn_handle;
}CCCD_info_t;


BLEAppUtil_EventHandler_t ancsGATTHandler =
{
    .handlerType    = BLEAPPUTIL_GATT_TYPE,
    .pEventHandler  = ANCS_AMS_EventHandler,
    .eventMask      = BLEAPPUTIL_ATT_HANDLE_VALUE_NOTI        |
                      BLEAPPUTIL_ATT_FIND_BY_TYPE_VALUE_RSP   |
                      BLEAPPUTIL_ATT_READ_BY_TYPE_RSP         |
                      BLEAPPUTIL_ATT_FIND_INFO_RSP            |
                      BLEAPPUTIL_ATT_READ_RSP                 |
                      BLEAPPUTIL_ATT_WRITE_RSP                

};
// ========================= Section: Macros =========================
#ifndef SUCCESS
#define SUCCESS 0
#endif
#ifndef FAILURE
#define FAILURE 1
#endif
// ========================= Section: Globals =========================
extern Display_Handle disp_handle;

static ancs_state_t ancsState = DISCOVER_ANCS_SERVICE;

static uint16_t ancs_start_group_handle;
static uint16_t ancs_end_group_handle;
static uint16_t ams_start_group_handle;
static uint16_t ams_end_group_handle;

uint16_t Ancs_handleCache[HDL_CACHE_LEN];
uint16_t ams_handleCache[HDL_CACHE_LEN];

static uint8_t Ancs_noti_conn_handle = 0;
static uint8_t conn_handle = 0;
static uint8_t Ancs_data_conn_handle = 0;
static uint8_t ams_remote_conn_handle = 0;
static uint8_t ams_updt_conn_handle = 0;

static attWriteReq_t writeReq = { 0 };

static CCCD_info_t noti_cccd = {0};
static CCCD_info_t data_cccd = {0};
static CCCD_info_t remote_cccd = {0};
static CCCD_info_t entity_updt_cccd = {0};


// ========================= Section: Event Handler =========================
/**
 * @brief Main GATT event handler for ANCS/AMS client state machine.
 *
 * Handles service/characteristic discovery, CCCD writes, and notification/data events.
 *
 * @param event BLEAppUtil event code
 * @param pMsgData Pointer to BLEAppUtil_msgHdr_t message data
 */

static void ANCS_AMS_EventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
    gattMsgEvent_t *gattMsg = ( gattMsgEvent_t * )pMsgData;

    if(gattMsg->method == ATT_HANDLE_VALUE_NOTI)
    {
        attHandleValueNoti_t att_noti = (attHandleValueNoti_t )gattMsg->msg.handleValueNoti;
        if (att_noti.handle == Ancs_handleCache[ANCS_NOTIF_SCR_HDL_START])
        {
          Ancs_queueNewNotif(gattMsg);
        }
        else if (att_noti.handle == Ancs_handleCache[ANCS_DATA_SRC_HDL_START])
        {
          Ancs_processDataServiceNotif(gattMsg);

        }
        else if (att_noti.handle == ams_handleCache[AMS_REMOTE_COM_HDL_START])
        {
          ams_parseRemoteUpdates(gattMsg);

        }
        else if (att_noti.handle == ams_handleCache[AMS_ENT_UPDT_HDL_START])
        {
          ams_parseEntityUpdateNotifications(gattMsg);

        }
        GATT_bm_free((gattMsg_t *)&att_noti, ATT_HANDLE_VALUE_NOTI);
    }

    if(gattMsg->method == ATT_READ_RSP)
    {
      ams_parseEntityAttributeResponses(gattMsg);
    }


    // DISCOVER_ANCS_SERVICE: Discover the ANCS service group handles
    if(ancsState == DISCOVER_ANCS_SERVICE && gattMsg->method == ATT_FIND_BY_TYPE_VALUE_RSP)
    {

      Display_printf(disp_handle, 0xFF, 0, "ANCS service found");
      attFindByTypeValueRsp_t att = (attFindByTypeValueRsp_t)gattMsg->msg.findByTypeValueRsp;

      ancs_start_group_handle = BUILD_UINT16(att.pHandlesInfo[0], att.pHandlesInfo[1]);
      ancs_end_group_handle = BUILD_UINT16(att.pHandlesInfo[2], att.pHandlesInfo[3]);
      if (ancs_start_group_handle == 0 || ancs_end_group_handle == 0) {
        Display_printf(disp_handle, 0xFF, 0, "Error: Invalid ANCS service handles");
        return;
      }
      GATT_bm_free((gattMsg_t *)&att, ATT_FIND_BY_TYPE_VALUE_RSP);
      ancsState = DISCOVER_ANCS_CHARS;
      BLEAppUtil_invokeFunctionNoData(gatt_disc_char_wrapper);
    }
    // DISCOVER_AMS_SERVICE: Discover the AMS service group handles
    else if(ancsState == DISCOVER_AMS_SERVICE && gattMsg->method == ATT_FIND_BY_TYPE_VALUE_RSP)
    {
        Display_printf(disp_handle, 0xFF, 0, "AMS service found");
        attFindByTypeValueRsp_t att = (attFindByTypeValueRsp_t)gattMsg->msg.findByTypeValueRsp;

        ams_start_group_handle = BUILD_UINT16(att.pHandlesInfo[0], att.pHandlesInfo[1]);
        ams_end_group_handle = BUILD_UINT16(att.pHandlesInfo[2], att.pHandlesInfo[3]);
        if (ams_start_group_handle == 0 || ams_end_group_handle == 0) {
          Display_printf(disp_handle, 0xFF, 0, "Error: Invalid AMS service handles");
          return;
        }
        GATT_bm_free((gattMsg_t *)&att, ATT_FIND_BY_TYPE_VALUE_RSP);
        ancsState = DISCOVER_AMS_CHARS;

        BLEAppUtil_invokeFunctionNoData(gatt_disc_char_wrapper);
    }
    // DISCOVER_ANCS_CHARS: Discover all ANCS characteristics
    else if(ancsState == DISCOVER_ANCS_CHARS && gattMsg->method == ATT_READ_BY_TYPE_RSP)
    {
        attReadByTypeRsp_t att = (attReadByTypeRsp_t)gattMsg->msg.readByTypeRsp;
        uint8_t *pData = att.pDataList;
        uint16_t charStartHandle = 0;
        uint8_t properties = 0;
        uint16_t valueHandle = 0;
        uint16_t charUUID = 0;
        uint16_t charEndHandle = 0;
        for (int i = 0; i < att.numPairs; i++) {
          if ((pData + att.len) > (att.pDataList + att.numPairs * att.len)) {
            Display_printf(disp_handle, 0xFF, 0, "Error: Out of bounds in characteristic parsing");
            break;
          }
          properties = pData[2];
          charStartHandle = BUILD_UINT16(pData[3], pData[4]);
          charUUID = BUILD_UINT16(pData[5], pData[6]);
          if (i < att.numPairs - 1) {
            charEndHandle = BUILD_UINT16(pData[att.len], pData[att.len + 1] )-1;
          } else {
            charEndHandle = ancs_end_group_handle;
          }
          switch (charUUID) {
            case ANCSAPP_NOTIF_SRC_CHAR_UUID:
              Ancs_handleCache[ANCS_NOTIF_SCR_HDL_START] = charStartHandle;
              Ancs_handleCache[ANCS_NOTIF_SCR_HDL_END]   = charEndHandle;
              break;
            case ANCSAPP_CTRL_PT_CHAR_UUID:
              Ancs_handleCache[ANCS_CTRL_POINT_HDL_START] = charStartHandle;
              Ancs_handleCache[ANCS_CTRL_POINT_HDL_END]   = charEndHandle;
              break;
            case ANCSAPP_DATA_SRC_CHAR_UUID:
              Ancs_handleCache[ANCS_DATA_SRC_HDL_START] = charStartHandle;
              Ancs_handleCache[ANCS_DATA_SRC_HDL_END]   = charEndHandle;
              break;
            default:
              break;
          }
          pData += att.len;
        }
        Display_printf(disp_handle, 0xFF, 0, "ANCS characterisitics found");
        GATT_bm_free((gattMsg_t *)&att, ATT_READ_BY_TYPE_RSP);

        ancsState = DISCOVER_ANCS_NOTI_CCCD;
        BLEAppUtil_invokeFunctionNoData(gatt_disc_cccd);
    }
    // DISCOVER_ANCS_NOTI_CCCD: Discover CCCD for ANCS Notification Source characteristic
    else if(ancsState == DISCOVER_ANCS_NOTI_CCCD && gattMsg->method == ATT_FIND_INFO_RSP)
    {
        attFindInfoRsp_t rsp = (attFindInfoRsp_t)gattMsg->msg.findInfoRsp;
        uint8_t pkt_index = 0;

        if (rsp.format != 0x01) {
            GATT_bm_free((gattMsg_t *)&rsp, ATT_FIND_INFO_RSP);
            return;
        }

        Ancs_noti_conn_handle = gattMsg->connHandle;
        for (uint8_t i = 0; i < rsp.numInfo; i++) {
            if ((pkt_index + 4) > (rsp.numInfo * 4)) {
                Display_printf(disp_handle, 0xFF, 0, "Error: Out of bounds in descriptor parsing");
                break;
            }
            uint16_t cccd_handle = BUILD_UINT16(rsp.pInfo[pkt_index + 0], rsp.pInfo[pkt_index + 1]);
            uint16_t uuid = BUILD_UINT16(rsp.pInfo[pkt_index + 2], rsp.pInfo[pkt_index + 3]);
            pkt_index += 4;
            if(uuid == CCCD_UUID) {
                Ancs_handleCache[ANCS_NOTIF_SCR_HDL_CCCD] = cccd_handle;
                break;
            }
        }
        GATT_bm_free((gattMsg_t *)&rsp, ATT_FIND_INFO_RSP);
        
        ancsState = DISCOVER_ANCS_DATASRC_CCCD;
        BLEAppUtil_invokeFunctionNoData(gatt_disc_cccd);
    }
    // DISCOVER_ANCS_DATASRC_CCCD: Discover CCCD for ANCS Data Source characteristic
    else if(ancsState == DISCOVER_ANCS_DATASRC_CCCD && gattMsg->method == ATT_FIND_INFO_RSP)
    {
        attFindInfoRsp_t rsp = (attFindInfoRsp_t)gattMsg->msg.findInfoRsp;
        uint8_t pkt_index = 0;

        if (rsp.format != 0x01) {
            GATT_bm_free((gattMsg_t *)&rsp, ATT_FIND_INFO_RSP);
            return;
        }

        Ancs_data_conn_handle = gattMsg->connHandle;
        for (uint8_t i = 0; i < rsp.numInfo; i++) {
            if ((pkt_index + 4) > (rsp.numInfo * 4)) {
                Display_printf(disp_handle, 0xFF, 0, "Error: Out of bounds in descriptor parsing");
                break;
            }
            uint16_t cccd_handle = BUILD_UINT16(rsp.pInfo[pkt_index + 0], rsp.pInfo[pkt_index + 1]);
            uint16_t uuid = BUILD_UINT16(rsp.pInfo[pkt_index + 2], rsp.pInfo[pkt_index + 3]);
            pkt_index += 4;
            if(uuid == CCCD_UUID) {
                Ancs_handleCache[ANCS_DATA_SRC_HDL_CCCD] = cccd_handle;
            }
        }

        GATT_bm_free((gattMsg_t *)&rsp, ATT_FIND_INFO_RSP);

        ancsState = DISCOVER_AMS_SERVICE;
        BLEAppUtil_invokeFunctionNoData(gatt_disc_ams_wrapper);
      
    }
    // DISCOVER_AMS_CHARS: Discover all AMS characteristics
    else if(ancsState == DISCOVER_AMS_CHARS && gattMsg->method == ATT_READ_BY_TYPE_RSP)
    {
        attReadByTypeRsp_t att = (attReadByTypeRsp_t)gattMsg->msg.readByTypeRsp;

        uint8_t *pData = att.pDataList;
        uint16_t charStartHandle = 0;
        uint8_t properties = 0;
        uint16_t valueHandle = 0;
        uint16_t charUUID = 0;
        uint16_t charEndHandle = 0;

        for (int i = 0; i < att.numPairs; i++) {
            properties = pData[2];
            charStartHandle = BUILD_UINT16(pData[3], pData[4]);
            charUUID = BUILD_UINT16(pData[5], pData[6]);
            // UUID follows (length depends on 16-bit or 128-bit)
            // Store or process handles as needed
            if (i < att.numPairs - 1) {
                charEndHandle = BUILD_UINT16(pData[att.len], pData[att.len + 1] )-1;
            }
            else
            {
                charEndHandle = ams_end_group_handle;
            }
            switch (charUUID)
                {
                case AMSAPP_REMOTE_COM_CHAR_UUID:
                    ams_handleCache[AMS_REMOTE_COM_HDL_START] = charStartHandle;
                    ams_handleCache[AMS_REMOTE_COM_HDL_END]   = charEndHandle;
                    break;
                case AMSAPP_ENTITY_UPDATE_CHAR_UUID:
                    ams_handleCache[AMS_ENT_UPDT_HDL_START] = charStartHandle;
                    ams_handleCache[AMS_ENT_UPDT_HDL_END]   = charEndHandle;
                    break;
                case AMSAPP_ENTITY_ATTR_CHAR_UUID:
                    ams_handleCache[AMS_ENT_ATTR_HDL_START] = charStartHandle;
                    ams_handleCache[AMS_ENT_ATTR_HDL_END]   = charEndHandle;
                    break;

                default:
                    break;
                }

            pData += att.len; // Move to next pair
        }
        GATT_bm_free((gattMsg_t *)&att, ATT_READ_BY_TYPE_RSP);

        ancsState = DISCOVER_AMS_REMOTE_COM_CCCD;
        BLEAppUtil_invokeFunctionNoData(gatt_disc_cccd);
    }

    // DISCOVER_AMS_REMOTE_COM_CCCD: Discover CCCD for AMS Remote Command characteristic
    else if(ancsState == DISCOVER_AMS_REMOTE_COM_CCCD && gattMsg->method == ATT_FIND_INFO_RSP)
    {
        attFindInfoRsp_t rsp = (attFindInfoRsp_t)gattMsg->msg.findInfoRsp;
        uint8_t pkt_index = 0;

        if (rsp.format != 0x01)
        {
          // We can only handle packet format of 0x01.
          GATT_bm_free((gattMsg_t *)&rsp, ATT_FIND_INFO_RSP);
        }

        ams_remote_conn_handle = gattMsg->connHandle;
        for (uint8_t i = 0; i < rsp.numInfo; i++)
        {
          uint16_t cccd_handle = BUILD_UINT16(rsp.pInfo[pkt_index + 0], rsp.pInfo[pkt_index + 1]);
          uint16_t uuid = BUILD_UINT16(rsp.pInfo[pkt_index + 2], rsp.pInfo[pkt_index + 3]);

          pkt_index += 4;

          if(uuid == CCCD_UUID)
          {
            ams_handleCache[AMS_REMOTE_COM_HDL_CCCD] = cccd_handle;
            ancsState = DISCOVER_AMS_ENT_UPDT_CCCD;
            BLEAppUtil_invokeFunctionNoData(gatt_disc_cccd);      
            break;
          } 
        }

        GATT_bm_free((gattMsg_t *)&rsp, ATT_FIND_INFO_RSP);
    }
    // DISCOVER_AMS_ENT_UPDT_CCCD: Discover CCCD for AMS Entity Update characteristic
    else if(ancsState == DISCOVER_AMS_ENT_UPDT_CCCD && gattMsg->method == ATT_FIND_INFO_RSP)
    {
        attFindInfoRsp_t rsp = (attFindInfoRsp_t)gattMsg->msg.findInfoRsp;
        uint8_t pkt_index = 0;

        if (rsp.format != 0x01)
        {
          // We can only handle packet format of 0x01.
          GATT_bm_free((gattMsg_t *)&rsp, ATT_FIND_INFO_RSP);
        }

        ams_updt_conn_handle = gattMsg->connHandle;
        for (uint8_t i = 0; i < rsp.numInfo; i++)
        {
          uint16_t cccd_handle = BUILD_UINT16(rsp.pInfo[pkt_index + 0], rsp.pInfo[pkt_index + 1]);
          uint16_t uuid = BUILD_UINT16(rsp.pInfo[pkt_index + 2], rsp.pInfo[pkt_index + 3]);

          pkt_index += 4;

          if(uuid == CCCD_UUID)
          {
            ams_handleCache[AMS_ENT_UPDT_HDL_CCCD] = cccd_handle;
            ancsState = ANCS_DATA_SUB;
            ANCS_AMS_EventHandler(0, NULL);
            break;
          } 
        }

        GATT_bm_free((gattMsg_t *)&rsp, ATT_FIND_INFO_RSP);

      
    }
    // ANCS_DATA_SUB: Subscribe to ANCS Data Source notifications
    else if(ancsState == ANCS_DATA_SUB)
    {
      data_cccd.attrHdl = Ancs_handleCache[ANCS_DATA_SRC_HDL_CCCD];
      data_cccd.cccd_conn_handle = Ancs_data_conn_handle;
      BLEAppUtil_invokeFunctionNoData(gatt_write_cccd_wrapper);
      ancsState = IDLE; 
    }

    // AMS_ENT_UPDT_SUB: Subscribe to AMS Entity Update notifications
    else if(ancsState == AMS_ENT_UPDT_SUB && gattMsg->method == ATT_WRITE_RSP)
    {
      entity_updt_cccd.attrHdl = ams_handleCache[AMS_ENT_UPDT_HDL_CCCD];
      entity_updt_cccd.cccd_conn_handle = ams_updt_conn_handle;
      BLEAppUtil_invokeFunctionNoData(gatt_write_cccd_wrapper);
    }
    // AMS_REMOTE_SUB: Subscribe to AMS Remote Command notifications
    else if(ancsState == AMS_REMOTE_SUB && gattMsg->method == ATT_WRITE_RSP)
    {
      remote_cccd.attrHdl = ams_handleCache[AMS_REMOTE_COM_HDL_CCCD];
      remote_cccd.cccd_conn_handle = ams_remote_conn_handle;
      BLEAppUtil_invokeFunctionNoData(gatt_write_cccd_wrapper);
    }
    // ANCS_NOTI_SUB: Subscribe to ANCS Notification Source notifications
    else if(ancsState == ANCS_NOTI_SUB && gattMsg->method == ATT_WRITE_RSP)
    {
      noti_cccd.attrHdl = Ancs_handleCache[ANCS_NOTIF_SCR_HDL_CCCD];
      noti_cccd.cccd_conn_handle = Ancs_noti_conn_handle;
      BLEAppUtil_invokeFunctionNoData(gatt_write_cccd_wrapper);
    }
    // REQ_AMS_UPDATES: Request AMS track and player info
    else if(ancsState == REQ_AMS_UPDATES)
    {
      // after subscribing to ANCS notifications there is a flood of preexisting notifications
      // to avoid overloading the system we wait 2 seconds before requesting AMS information
      ClockP_Params_init(&ams_req_params);
      ams_req_params.startFlag = false;
      ams_req_params.period = 0;

      c_ams_req = ClockP_create(delayed_AMS_Requests_wrapper, 2000000, &ams_req_params);
      ClockP_start(c_ams_req);
      
      ancsState = IDLE;
    }
}



static void delayed_AMS_Requests_wrapper(uintptr_t arg)
{
    BLEAppUtil_invokeFunctionNoData(delayed_AMS_Requests);
}

static void delayed_AMS_Requests(char* arg)
{
    Display_printf(disp_handle, 0xFF, 0, "Executing delayed AMS requests");
    ams_requestTrackInfo();
    ClockP_usleep(200000); // 200ms between requests
    ams_requestPlayerInfo();
}

// ======================== Section: GATT Helper Functions =========================

/**
 * @brief Prepare and send a CCCD write for notification/data/AMS characteristics.
 *
 * This function prepares a GATT write request to enable notifications for ANCS and AMS characteristics.
 * It allocates the required buffer, sets the value to enable notifications, and triggers the write execution.
 * The target characteristic is selected based on the current client state.
 *
 * @note Uses a short delay to ensure BLE stack readiness before writing.
 *
 * @return void
 */
static void gatt_write_cccd_wrapper()
{
  uint8_t val[2] = {0x01, 0x00};
  writeReq.cmd = 0;
  writeReq.len = 2;
  writeReq.sig = 0;

  if(ancsState == ANCS_DATA_SUB|| ancsState == IDLE)
  {
    writeReq.handle = data_cccd.attrHdl;
    writeReq.pValue = GATT_bm_alloc(data_cccd.cccd_conn_handle, ATT_WRITE_REQ, writeReq.len, NULL);
  }
  else if(ancsState == AMS_REMOTE_SUB)
  {
    writeReq.handle = remote_cccd.attrHdl;
    writeReq.pValue = GATT_bm_alloc(remote_cccd.cccd_conn_handle, ATT_WRITE_REQ, writeReq.len, NULL);
  }
  else if(ancsState == AMS_ENT_UPDT_SUB)
  {
    writeReq.handle = entity_updt_cccd.attrHdl;
    writeReq.pValue = GATT_bm_alloc(entity_updt_cccd.cccd_conn_handle, ATT_WRITE_REQ, writeReq.len, NULL);
  }
  else if(ancsState == ANCS_NOTI_SUB)
  {
    writeReq.handle = noti_cccd.attrHdl;
    writeReq.pValue = GATT_bm_alloc(noti_cccd.cccd_conn_handle, ATT_WRITE_REQ, writeReq.len, NULL);
  }

  if (writeReq.pValue) 
  {
    writeReq.pValue[0] = 0x01;   
    writeReq.pValue[1] = 0x00;
    BLEAppUtil_invokeFunctionNoData(gatt_write_cccd_execute);
  } 
}

/**
 * @brief Execute the CCCD write prepared by gatt_write_cccd_wrapper().
 *
 * This function sends a GATT write request to enable notifications for ANCS and AMS characteristics.
 * It advances the client state machine to subscribe to each required characteristic in sequence.
 * If the write fails, it frees the allocated buffer and prints an error message.
 *
 * @note Uses a short delay to ensure BLE stack readiness before writing.
 *
 * @return void
 */
static void gatt_write_cccd_execute()
{
    
  ClockP_usleep(100000);
  bStatus_t stat;

  if(ancsState == ANCS_DATA_SUB|| ancsState == IDLE)
  {
    stat = GATT_WriteCharValue(data_cccd.cccd_conn_handle, &writeReq, BLEAppUtil_getSelfEntity());
    ancsState = AMS_REMOTE_SUB;
  }
  else if(ancsState == AMS_REMOTE_SUB)
  {
    stat = GATT_WriteCharValue(remote_cccd.cccd_conn_handle, &writeReq, BLEAppUtil_getSelfEntity());
    ancsState = AMS_ENT_UPDT_SUB;
  }
  else if(ancsState == AMS_ENT_UPDT_SUB)
  {
    stat = GATT_WriteCharValue(entity_updt_cccd.cccd_conn_handle, &writeReq, BLEAppUtil_getSelfEntity());
    ancsState = ANCS_NOTI_SUB;
  }
  else if(ancsState == ANCS_NOTI_SUB)
  {
    stat = GATT_WriteCharValue(noti_cccd.cccd_conn_handle, &writeReq, BLEAppUtil_getSelfEntity());
    ancsState = REQ_AMS_UPDATES;
  }
    
  if (stat != SUCCESS) {
    GATT_bm_free((gattMsg_t *)&writeReq, ATT_WRITE_REQ);
    Display_printf(disp_handle, 0xFF, 0, "cccd write failed %d", stat);
  }
}


/**
 * @brief Discover all characteristics for ANCS or AMS service.
 *
 * This function initiates a GATT characteristic discovery for either the ANCS or AMS service,
 * depending on the current state. It is called during the client state machine to enumerate
 * all characteristics within the discovered service group.
 *
 * @note Uses a short delay to ensure BLE stack readiness before discovery.
 *
 * @return void
 */
static void gatt_disc_char_wrapper()
{
  ClockP_usleep(100000);
  if(ancsState == DISCOVER_ANCS_CHARS)
  {
    GATT_DiscAllChars(conn_handle, ancs_start_group_handle, ancs_end_group_handle, BLEAppUtil_getSelfEntity());
  }
  else if(ancsState == DISCOVER_AMS_CHARS)
  {
    GATT_DiscAllChars(conn_handle, ams_start_group_handle, ams_end_group_handle, BLEAppUtil_getSelfEntity());
  }
}

/**
 * @brief Discover all CCCD (Client Characteristic Configuration Descriptor) handles for ANCS and AMS characteristics.
 *
 * This function initiates GATT descriptor discovery for the relevant characteristic based on the current state.
 * It is called during service/characteristic discovery to locate CCCD handles required for enabling notifications.
 *
 * @note Uses a short delay to ensure BLE stack readiness before discovery.
 *
 * @return void
 */
static void gatt_disc_cccd()
{
  //delay needed for stack to work
  ClockP_usleep(100000);

  if(ancsState == DISCOVER_ANCS_NOTI_CCCD)
  {
    GATT_DiscAllCharDescs(conn_handle, Ancs_handleCache[ANCS_NOTIF_SCR_HDL_START], Ancs_handleCache[ANCS_NOTIF_SCR_HDL_END], BLEAppUtil_getSelfEntity());
  }
  else if(ancsState == DISCOVER_ANCS_DATASRC_CCCD)
  {
    GATT_DiscAllCharDescs(conn_handle, Ancs_handleCache[ANCS_DATA_SRC_HDL_START], Ancs_handleCache[ANCS_DATA_SRC_HDL_END], BLEAppUtil_getSelfEntity());
  }
  else if(ancsState == DISCOVER_AMS_REMOTE_COM_CCCD)
  {
    GATT_DiscAllCharDescs(conn_handle, ams_handleCache[AMS_REMOTE_COM_HDL_START], ams_handleCache[AMS_REMOTE_COM_HDL_END], BLEAppUtil_getSelfEntity());
  }
  else if(ancsState == DISCOVER_AMS_ENT_UPDT_CCCD)
  {
    GATT_DiscAllCharDescs(conn_handle, ams_handleCache[AMS_ENT_UPDT_HDL_START], ams_handleCache[AMS_ENT_UPDT_HDL_END], BLEAppUtil_getSelfEntity());
  }
}


/**
 * @brief Discover the AMS (Apple Media Service) primary service by UUID.
 *
 * This function initiates a GATT service discovery for the AMS service using its 128-bit UUID.
 * It is called during the ANCS/AMS client state machine to locate the AMS service on the connected device.
 *
 * @note Uses a short delay to ensure BLE stack readiness before discovery.
 *
 * @return void
 */
static void gatt_disc_ams_wrapper()
{
  ClockP_usleep(100000);
  bStatus_t status;
  uint8_t uuid[16] = {AMSAPP_AMS_SVC_UUID};
  Display_printf(disp_handle, 0xFF, 0, "Searching for AMS service");
  status = GATT_DiscPrimaryServiceByUUID(conn_handle, uuid, sizeof(uuid), BLEAppUtil_getSelfEntity());
  if(status != SUCCESS)
  {
    Display_printf(disp_handle, 0xFF, 0, "primary service discovery failed");
  }
}

/*********************************************************************
 * @fn      ANCS_start
 *
 * @brief   Initialize and start the ANCS/AMS client. Registers the GATT event handler
 *          for Apple Notification Center Service and Apple Media Service BLE client.
 *
 * @return  bStatus_t - SUCCESS if handler registration succeeded, error code otherwise.
 */
bStatus_t ANCS_start( void )
{
  bStatus_t status = SUCCESS;

  // Register the handlers
  status = BLEAppUtil_registerEventHandler( &ancsGATTHandler );

  // Return status value
  return( status );
}

/*********************************************************************
 * @fn      ANCS_resetState
 *
 * @brief   Reset the ANCS/AMS state machine to restart discovery 
 *          and subscription process on reconnection
 *
 * @return  void
 */
void ANCS_resetState(void)
{
    // Reset state machine to start discovery process
    ancsState = DISCOVER_ANCS_SERVICE;
    
    // Clear handle caches
    memset(Ancs_handleCache, 0, sizeof(Ancs_handleCache));
    memset(ams_handleCache, 0, sizeof(ams_handleCache));
    
    // Clear CCCD info structures 
    memset(&noti_cccd, 0, sizeof(noti_cccd));
    memset(&data_cccd, 0, sizeof(data_cccd));
    memset(&remote_cccd, 0, sizeof(remote_cccd));
    memset(&entity_updt_cccd, 0, sizeof(entity_updt_cccd));
    
    // Trigger service discovery after a short delay
    BLEAppUtil_invokeFunctionNoData(gatt_disc_service_wrapper);
}

void gatt_disc_service_wrapper()
{
  //delay needed for stack to finish pending operations
  ClockP_usleep(500000);
  bStatus_t status;
  uint8_t uuid[16] = {ANCSAPP_ANCS_SVC_UUID};
  Display_printf(disp_handle, 0xFF, 0, "Searching for ANCS service");
  status = GATT_DiscPrimaryServiceByUUID(conn_handle, uuid, sizeof(uuid), BLEAppUtil_getSelfEntity());
  if(status != SUCCESS)
  {
    Display_printf(disp_handle, 0xFF, 0, "primary service discovery failed");
  }
}