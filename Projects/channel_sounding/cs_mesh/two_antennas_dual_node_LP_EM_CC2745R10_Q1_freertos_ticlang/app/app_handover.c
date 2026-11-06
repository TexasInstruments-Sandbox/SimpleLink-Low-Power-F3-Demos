/******************************************************************************

@file  app_handover.c

@brief This example file demonstrates how to activate the handover with
the help of BLEAppUtil APIs.

More details on the functions and structures can be seen next to the usage.

Group: WCS, BTS
Target Device: cc23xx

******************************************************************************

 Copyright (c) 2024-2026, Texas Instruments Incorporated
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

#if defined( HOST_CONFIG ) && ( HOST_CONFIG & ( PERIPHERAL_CFG ) )
#ifdef CONNECTION_HANDOVER

//*****************************************************************************
//! Includes
//*****************************************************************************
#include <string.h>
#include <stdarg.h>
#include "ti/ble/stack_util/comdef.h"
#include "ti/ble/host/handover/handover.h"
#include "app_handover_api.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "app_main.h"

//*****************************************************************************
//! Prototypes
//*****************************************************************************
static void Handover_extEvtHandler(BLEAppUtil_eventHandlerType_e eventType, uint32 event, BLEAppUtil_msgHdr_t *pMsgData);
void Handover_EventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData);

//*****************************************************************************
//! Globals
//*****************************************************************************

//! The external event handler
// *****************************************************************//
// ADD YOUR EVENT HANDLER BY CALLING @ref Handover_registerEvtHandler
//*****************************************************************//
static ExtCtrl_eventHandler_t gExtEvtHandler = NULL;

// Events handlers struct, contains the handlers and event masks
BLEAppUtil_EventHandler_t handoverHandler =
{
  .handlerType    = BLEAPPUTIL_HANDOVER_TYPE,
  .pEventHandler  = Handover_EventHandler,
  .eventMask      = BLEAPPUTIL_HANDOVER_START_SERVING_EVENT_CODE   |
                    BLEAPPUTIL_HANDOVER_START_CANDIDATE_EVENT_CODE
};

// Serving node parameters
handoverSNParams_t gSnParams;

// Serving node parameters
handoverCNParams_t gCnParams;

//*****************************************************************************
//! Functions
//*****************************************************************************

/*********************************************************************
 * @fn      Handover_startServingNode
 *
 * @brief   This function will handle the start of the handover process
 *          on the serving node side. This includes, getting the needed
 *          buffer size from the stack and registering to module's CB
 *
 * @param   snParams - Serving node start parameters
 *
 * @return  SUCCESS, FAILURE, bleMemAllocError, bleIncorrectMode
 *          INVALIDPARAMETER
 */
bStatus_t Handover_startServingNode(Handover_snParams_t snParams)
{
  uint8_t status = SUCCESS;

  status = BLEAppUtil_registerSNCB();

  if ( status == SUCCESS )
  {
    // Initialize the serving node parameters
    status = Handover_InitSNParams(&gSnParams);

    // Fill the given parameters
    gSnParams.connHandle = snParams.connHandle;
    gSnParams.handoverSnMode = snParams.snMode;

    // Set the minimum and maximum GATT handles
    gSnParams.minGattHandle = snParams.minGattHandle;
    gSnParams.maxGattHandle = snParams.maxGattHandle;

    if ( status == SUCCESS )
    {
      // Get the stack data size needed
      gSnParams.handoverDataSize = Handover_GetSNDataSize(&gSnParams);

      if ( gSnParams.handoverDataSize != 0 )
      {
        // Allocate the data buffer
        gSnParams.pHandoverData = (uint8 *) ICall_malloc(gSnParams.handoverDataSize);
        if ( gSnParams.pHandoverData == NULL )
        {
            // Allocation failed
            status = bleMemAllocError;
        }
        else
        {
          status = Handover_StartSN(&gSnParams);
          if ( status != SUCCESS )
          {
            // The stack returned an error code
            // Release the allocated data
            ICall_free(gSnParams.pHandoverData);
            gSnParams.pHandoverData = NULL;
          }
        }
      }
      else
      {
        status = FAILURE;
      }
    }
  }

  return status;
}

/*********************************************************************
 * @fn      Handover_startCandidateNode
 *
 * @brief   This function will handle the start of the handover process
 *          on the candidate node side. This includes registering to the
 *          module's CB
 *
 * @param   cnParams - Candidate node start parameters
 *
 * @return  none
 */
bStatus_t Handover_startCandidateNode(Handover_cnParams_t cnParams)
{
  bStatus_t status = SUCCESS;

  // Register to receive CN CB
  status = BLEAppUtil_registerCNCB();

  if ( status == SUCCESS )
  {
    status = Handover_InitCNParams(&gCnParams);
    gCnParams.pHandoverData = cnParams.pHandoverData;
    gCnParams.timeDeltaInUs = cnParams.timeDelta;
    gCnParams.timeDeltaMaxErrInUs = cnParams.timeDeltaErr;
    gCnParams.maxFailedConnEvents = cnParams.maxFailedConnEvents;
    gCnParams.txBurstRatio = cnParams.txBurstRatio;
    if ( status == SUCCESS )
    {
      // Start the candidate node
      status = Handover_StartCN(&gCnParams);
    }
  }

  return status;
}

/*********************************************************************
 * @fn      Handover_closeServingNode
 *
 * @brief   The function will close the serving node side
 *
 * @param   closeSn - handover close SN parameters
 *
 * @return  status
 */
bStatus_t Handover_closeServingNode(Handover_closeSnParams_t closeSn)
{
  return Handover_CloseSN(&gSnParams, closeSn.handoverStatus);
}

/*********************************************************************
 * @fn      Handover_EventHandler
 *
 * @brief   The purpose of this function is to handle handover events
 *
 * @param   event    - message event.
 * @param   pMsgData - pointer to message data.
 *
 * @return  none
 */
void Handover_EventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
  if (pMsgData != NULL)
  {
    switch (event)
    {
      case BLEAPPUTIL_HANDOVER_START_SERVING_EVENT_CODE:
      {
        uint8_t *pStat = (uint8_t *)pMsgData;
        uint32_t status = INVALID_32BIT_STATUS;
        uint16_t connHandle = INVALID_CONN_HANDLE;
        uint32_t totSize = 0;
        uint8_t *pSnEvent = NULL;

        if ( gSnParams.pHandoverData != NULL )
        {
          connHandle = BUILD_UINT16(pStat[0], pStat[1]);
          status = BUILD_UINT32(pStat[2], pStat[3], pStat[4], pStat[5]);

          // If status is success - send data
          if ( status == SUCCESS )
          {
            // The size of the connection handle (uint16_t), status (uint32_t) and the data size (uint32_t)
            totSize = sizeof(uint16_t) + 2*sizeof(uint32_t) + gSnParams.handoverDataSize;
          }
          else // Status != success
          {
            // Connection handle and status
            totSize = sizeof(uint16_t) + sizeof(uint32_t);
          }
        }
        else
        {
          // The handover buffer is NULL, the event size needed is for the status only
          // Connection handle and status
          totSize = sizeof(uint16_t) + sizeof(uint32_t);
        }

        // Allocate the event
        pSnEvent = (uint8_t *) ICall_malloc(totSize);

        if ( pSnEvent != NULL )
        {
          uint8_t *pEvt = pSnEvent;

          // Copy the connection handle
          memcpy(pEvt, &connHandle, sizeof(uint16_t));
          pEvt += sizeof(uint16_t);

          // Copy the status
          memcpy(pEvt, &status, sizeof(uint32_t));
          pEvt += sizeof(uint32_t);

          if ( status == SUCCESS )
          {
            // Copy the data length
            memcpy(pEvt, &gSnParams.handoverDataSize, sizeof(uint32_t));
            pEvt += sizeof(uint32_t);

            // Copy the handover data
            memcpy(pEvt, gSnParams.pHandoverData, gSnParams.handoverDataSize);
          }
        }

        Handover_extEvtHandler(BLEAPPUTIL_HANDOVER_TYPE, event, (BLEAppUtil_msgHdr_t *)pSnEvent);

        ICall_free(pSnEvent);

        // Done with the handover. Reset the parameters
        if ( gSnParams.pHandoverData != NULL )
        {
          ICall_free(gSnParams.pHandoverData);
          gSnParams.pHandoverData = NULL;
        }
        gSnParams.handoverDataSize = HANDOVER_INVALID_DATA_SIZE;

        break;
      }

      case BLEAPPUTIL_HANDOVER_START_CANDIDATE_EVENT_CODE:
      {
          uint8_t *pStat = (uint8_t *)pMsgData;
          uint32_t status = INVALID_32BIT_STATUS;
          uint16_t connHandle = INVALID_CONN_HANDLE;
          uint32_t totSize = 0;
          uint8_t *pCnEvent = NULL;

          connHandle = BUILD_UINT16(pStat[0], pStat[1]);
          status = BUILD_UINT32(pStat[2], pStat[3], pStat[4], pStat[5]);

          // If status is success - send data

          // The size of the connection handle (uint16_t) and status (uint32_t)
          totSize = sizeof(uint16_t) + sizeof(uint32_t);

          // Allocate the event
          pCnEvent = (uint8_t *) ICall_malloc(totSize);

          if ( pCnEvent != NULL )
          {
            uint8_t *pEvt = pCnEvent;

            // Copy the connection handle
            memcpy(pEvt, &connHandle, sizeof(uint16_t));
            pEvt += sizeof(uint16_t);

            // Copy the status
            memcpy(pEvt, &status, sizeof(uint32_t));
            pEvt += sizeof(uint32_t);

            Handover_extEvtHandler(BLEAPPUTIL_HANDOVER_TYPE, event, (BLEAppUtil_msgHdr_t *)pCnEvent);
            ICall_free(pCnEvent);
          }

          break;
      }

      default:
          break;
    }
  }
}

/*********************************************************************
 * @fn      Handover_extEvtHandler
 *
 * @brief   The purpose of this function is to forward the event to the external
 *          event handler that registered to handle the events.
 *
 * @param   eventType - the event type of the event @ref BLEAppUtil_eventHandlerType_e
 * @param   event     - message event.
 * @param   pMsgData  - pointer to message data.
 *
 * @return  none
 */
static void Handover_extEvtHandler(BLEAppUtil_eventHandlerType_e eventType, uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
  if ( gExtEvtHandler != NULL )
  {
    gExtEvtHandler(eventType, event, pMsgData);
  }
}

/*********************************************************************
 * @fn      Handover_registerEvtHandler
 *
 * @brief   This function is called to register the external event handler
 *          function.
 *
 * @return  None
 */
void Handover_registerEvtHandler(ExtCtrl_eventHandler_t fEventHandler)
{
  gExtEvtHandler = fEventHandler;
}

/*********************************************************************
 * @fn      Handover_start
 *
 * @brief   This function is called after stack initialization,
 *          the purpose of this function is to initialize
 *          register the specific events handlers of the handover
 *          application module and initiate the parser module.
 *
 * @param   none
 *
 * @return  SUCCESS, errorInfo
 */
bStatus_t Handover_start(void)
{
  // Register to both the Serving node and the candidate node callbacks
  return BLEAppUtil_registerEventHandler(&handoverHandler);
}

#endif // CONNECTION_HANDOVER
#endif // ( HOST_CONFIG & ( PERIPHERAL_CFG ) )
