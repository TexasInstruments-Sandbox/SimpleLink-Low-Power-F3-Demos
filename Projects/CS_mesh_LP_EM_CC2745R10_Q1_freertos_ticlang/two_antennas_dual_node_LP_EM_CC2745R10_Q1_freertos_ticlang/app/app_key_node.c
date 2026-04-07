/******************************************************************************

@file  app_key_node.c

@brief This file contains the key node functionalities for the BLE application.
       It handles the advertisement, connection and CS.

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
#include "ti_ble_config.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/host/gap/gap_scanner.h"
#include "app_key_node.h"
#include "app_peripheral_api.h"
#include "app_gatt_api.h"
#include "app_pairing_api.h"
#include "app_connection_api.h"
#include "app_cs_api.h"
#include "app_cs_transceiver_api.h"
#include "app_cs_notify_client.h"
#include "app_cs_notify_service.h"

#include "ti_drivers_config.h"
#include <ti/drivers/UART2.h>
#include <stdio.h>

//*****************************************************************************
//! Defines
//*****************************************************************************
#define PEER_NAME_LEN 8

#define KEY_NODE_INVALID_PROCEDURE_COUNTER  0xFFFFFFFF

//*****************************************************************************
//! Prototypes
//*****************************************************************************
uint8_t KeyNode_startAdvertisement(void);

#ifdef CHANNEL_SOUNDING
static void KeyNode_csEvtHandler(csEvtHdr_t *pCsEvt);
static void KeyNode_handleCsEvent(BLEAppUtil_eventHandlerType_e eventType, uint32 event, BLEAppUtil_msgHdr_t *pMsgData);
#endif // CHANNEL_SOUNDING

//*****************************************************************************
//! Globals
//*****************************************************************************
// This is the name of the peer device that the scanner will look for when the
// key node is defined with central configuration
uint8_t carNodeName[PEER_NAME_LEN] = {'C','a', 'r', ' ', 'N', 'o', 'd', 'e'};

// Procedure Counter of the current subevent being processed
uint32_t gCurrProcedureCounter = KEY_NODE_INVALID_PROCEDURE_COUNTER;

extern UART2_Handle uart;

//*****************************************************************************
//! Functions
//*****************************************************************************

/*******************************************************************************
 * This API is starts the key node application.
 *
 * Public function defined in app_key_node.h.
 */
uint8_t KeyNode_start(void)
{
    uint8_t status = SUCCESS;

    // Add the CS Notify Service so car node can write CS complete notification
    status = CSNotifyService_addService();
    if (status != SUCCESS)
    {
        char debugBuf[50];
        sprintf(debugBuf, "Failed to add CS Notify Service! err=%d\r\n", status);
        UART2_write(uart, debugBuf, strlen(debugBuf), NULL);
    }

    // Register to receive connection and pairing events
    Connection_registerEvtHandler(&KeyNode_handleConnectionEvent);

    Pairing_registerEvtHandler(&KeyNode_handlePairingEvent);
#ifdef CHANNEL_SOUNDING
    // Register to the Channel Sounding events
    ChannelSounding_registerEvtHandler(&KeyNode_handleCsEvent);
#endif // CHANNEL_SOUNDING

    // Start the Transceiver
    if( status == USUCCESS )
    {
        status = ChannelSoundingTransceiver_start();
    }

    UART2_write(uart, "Key node ready!\r\n", 18, NULL);

    return status;
}

uint8_t KeyNode_startAdvertisement(void)
{
    Peripheral_advStartCmdParams_t advParams;

    advParams.advHandle = 0;
    advParams.enableOptions = GAP_ADV_ENABLE_OPTIONS_USE_MAX;
    advParams.durationOrMaxEvents = 0;

    return Peripheral_advStart(&advParams);
}

/*******************************************************************************
 * This function handle connection events.
 *
 * Public function defined in app_key_node.h.
 */
void KeyNode_handleConnectionEvent(BLEAppUtil_eventHandlerType_e eventType, uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
    if ( pMsgData != NULL )
    {
        if ( event == BLEAPPUTIL_LINK_TERMINATED_EVENT )
        {
            UART2_write(uart, "Disconnected, restarting advertisements!\r\n", 43, NULL);
            KeyNode_startAdvertisement();
        }
        else if ( event == BLEAPPUTIL_LINK_ESTABLISHED_EVENT )
        {
            UART2_write(uart, "Connected!\r\n", 13, NULL);
            gapEstLinkReqEvent_t *pGapEstMsg = (gapEstLinkReqEvent_t *)pMsgData;

            // Call MTU exchange
            AppGATT_exchangeMTU(pGapEstMsg->connectionHandle, MAX_PDU_SIZE);
        }
    }
}

/*********************************************************************
 * @fn      KeyNode_handlePairingEvent
 *
 * @brief   This function handles pairing events raised
 *
 * @param   eventType - the type of the events @ref BLEAppUtil_eventHandlerType_e.
 * @param   event     - message event.
 * @param   pMsgData  - pointer to message data.
 *
 * @return  None
 */
void KeyNode_handlePairingEvent(BLEAppUtil_eventHandlerType_e eventType, uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
    BLEAppUtil_PairStateData_t *pPairEvt = (BLEAppUtil_PairStateData_t *)pMsgData;
    ChannelSounding_securityEnableCmdParams_t secEnableParams = {0};
    uint8_t status = SUCCESS;
    linkDBInfo_t linkInfo = {0};

    if ( pMsgData == NULL )
    {
        return;
    }

    if ( event == BLEAPPUTIL_PAIRING_STATE_COMPLETE || event == BLEAPPUTIL_PAIRING_STATE_ENCRYPTED )
    {
        UART2_write(uart, "Pairing successful!\r\n", 22, NULL);
        // Verify pairing was successful
        if ( pPairEvt->status == SUCCESS )
        {
            // Set the connection handle in the Channel Sounding Security Enable command
            secEnableParams.connHandle = pPairEvt->connHandle;

            // Get the link information
            status = linkDB_GetInfo(secEnableParams.connHandle, &linkInfo);

            // Check the link role
            if ( status == SUCCESS && linkInfo.connRole == GAP_PROFILE_CENTRAL )
            {
                // As a Central, enable Channel Sounding security
                ChannelSounding_securityEnable(&secEnableParams);
            }
        }
    }
}

/***********************************************************************
** Internal Functions
*/

#ifdef CHANNEL_SOUNDING
/*********************************************************************
 * @fn      KeyNode_handleCsEvent
 *
 * @brief   This function handles Channel Sounding events raised
 *
 * @param   eventType - the type of the events @ref BLEAppUtil_eventHandlerType_e.
 * @param   event     - message event.
 * @param   pMsgData  - pointer to message data.
 *
 * @return  None
 */
static void KeyNode_handleCsEvent(BLEAppUtil_eventHandlerType_e eventType, uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
    if ( eventType == BLEAPPUTIL_CS_TYPE && event == BLEAPPUTIL_CS_EVENT_CODE )
    {
        KeyNode_csEvtHandler((csEvtHdr_t *)pMsgData);
    }
}

/*********************************************************************
 * @fn      KeyNode_csEvtHandler
 *
 * @brief   This function handles @ref BLEAPPUTIL_CS_EVENT_CODE
 *
 * @param   pCsEvt - pointer message data
 *
 * @return  None
 */
static void KeyNode_csEvtHandler(csEvtHdr_t *pCsEvt)
{
  uint8_t opcode = pCsEvt->opcode;

  if (NULL != pCsEvt)
  {
    switch( opcode )
    {
        case CS_READ_REMOTE_SUPPORTED_CAPABILITIES_COMPLETE_EVENT:
        {
        break;
        }

        case CS_CONFIG_COMPLETE_EVENT:
        {
        break;
        }

        case CS_READ_REMOTE_FAE_TABLE_COMPLETE_EVENT:
        {
        break;
        }

        case CS_SECURITY_ENABLE_COMPLETE_EVENT:
        {
        // Assume reflector
        // call default settings
        ChannelSounding_setDefaultSettingsCmdParams_t params; // TODO
        params.connHandle = 0;              //!< Connection handle
        params.roleEnable = 3;              //!< Role enable flag
        params.csSyncAntennaSelection = 1;  //!< CS sync antenna selection
        params.maxTxPower = 10;             //!< Maximum TX power in dBm
        ChannelSounding_setDefaultSettings(&params);
        break;
        }

        case CS_PROCEDURE_ENABLE_COMPLETE_EVENT:
        {
        ChannelSounding_procEnableComplete_t *pAppProcedureEnableComplete = (ChannelSounding_procEnableComplete_t *) pCsEvt;
        ChannelSoundingTransceiver_sendResults((uint8_t*) pAppProcedureEnableComplete, sizeof(ChannelSounding_procEnableComplete_t), APP_CS_PROCEDURE_ENABLE_COMPLETE_EVENT);
        break;
        }

        case CS_SUBEVENT_RESULT:
        {
            ChannelSounding_subeventResults_t *pAppSubeventResults = (ChannelSounding_subeventResults_t *) pCsEvt;

            gCurrProcedureCounter = pAppSubeventResults->procedureCounter;

            ChannelSoundingTransceiver_sendResults((uint8_t*) pAppSubeventResults, sizeof(ChannelSounding_subeventResults_t) + pAppSubeventResults->dataLen, APP_CS_SUBEVENT_RESULT);
            
            if (pAppSubeventResults->procedureDoneStatus == CS_PROCEDURE_DONE ||
                pAppSubeventResults->procedureDoneStatus == CS_PROCEDURE_ABORTED)
            {
                // Reset the procedure counter
                gCurrProcedureCounter = KEY_NODE_INVALID_PROCEDURE_COUNTER;
            }

            break;
        }

        case CS_SUBEVENT_CONTINUE_RESULT:
        {
            ChannelSounding_subeventResultsContinue_t *pAppSubeventResultsCont = (ChannelSounding_subeventResultsContinue_t *) pCsEvt;

            // Calculate the size of the extended event
            uint16_t resultsExtSize = sizeof(ChannelSounding_subeventResultsContinueExt_t) + pAppSubeventResultsCont->dataLen;

            // Allocate memory for the extended event
            ChannelSounding_subeventResultsContinueExt_t* appSubeventResultsContExt =
                                                (ChannelSounding_subeventResultsContinueExt_t*)ICall_malloc(resultsExtSize);

            // If memory allocation was successful, fill the extended event
            if (NULL != appSubeventResultsContExt && gCurrProcedureCounter != KEY_NODE_INVALID_PROCEDURE_COUNTER)
            {
                appSubeventResultsContExt->csEvtOpcode           = pAppSubeventResultsCont->csEvtOpcode;        
                appSubeventResultsContExt->connHandle            = pAppSubeventResultsCont->connHandle;
                appSubeventResultsContExt->configID              = pAppSubeventResultsCont->configID;
                appSubeventResultsContExt->procedureCounter      = (uint16_t) gCurrProcedureCounter;   // Extended parameter
                appSubeventResultsContExt->procedureDoneStatus   = pAppSubeventResultsCont->procedureDoneStatus;
                appSubeventResultsContExt->subeventDoneStatus    = pAppSubeventResultsCont->subeventDoneStatus;
                appSubeventResultsContExt->abortReason           = pAppSubeventResultsCont->abortReason;
                appSubeventResultsContExt->numAntennaPath        = pAppSubeventResultsCont->numAntennaPath;
                appSubeventResultsContExt->numStepsReported      = pAppSubeventResultsCont->numStepsReported;
                appSubeventResultsContExt->dataLen               = pAppSubeventResultsCont->dataLen;

                // Copy subevent data
                memcpy(appSubeventResultsContExt->data, pAppSubeventResultsCont->data, pAppSubeventResultsCont->dataLen);

                // Send the event
                ChannelSoundingTransceiver_sendResults((uint8_t*) appSubeventResultsContExt, resultsExtSize, APP_CS_SUBEVENT_CONTINUE_RESULT_EXT);

                ICall_free(appSubeventResultsContExt);
            }

            if (pAppSubeventResultsCont->procedureDoneStatus == CS_PROCEDURE_DONE ||
                pAppSubeventResultsCont->procedureDoneStatus == CS_PROCEDURE_ABORTED)
            {
                // Reset the procedure counter
                gCurrProcedureCounter = KEY_NODE_INVALID_PROCEDURE_COUNTER;
            }

            break;
        }

        default:
        {
            break;
        }
    }
  }
}

#endif // CHANNEL_SOUNDING
