/******************************************************************************

@file  app_ranging_client.c

@brief This file contains the implementation of the Ranging Requester
       (RREQ) APIs and functionality.

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

#ifdef RANGING_CLIENT
//*****************************************************************************
//! Includes
//*****************************************************************************
#include "ti_ble_config.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/app_util/framework/bleapputil_timers.h"
#include "app_ranging_client_api.h"
#include "app_cs_api.h"

//*****************************************************************************
//! Defines
//*****************************************************************************

// Timeout for waiting for control point response in milliseconds (On-Demand mode only)
#define RREQ_TIMEOUT_CONTROL_POINT_RSP_MS   RREQ_MAX_TIMEOUT_CONTROL_POINT_RSP_MS
// Timeout for data ready event in milliseconds (On-Demand mode only)
#define RREQ_TIMEOUT_DATA_READY_MS          RREQ_MAX_TIMEOUT_DATA_READY_MS
// Timeout for first segment event in milliseconds (On-Demand & Real-Time)
#define RREQ_TIMEOUT_FIRST_SEGMENT_MS       RREQ_MAX_TIMEOUT_FIRST_SEGMENT_MS
// Timeout for next segment event in milliseconds (On-Demand & Real-Time)
#define RREQ_TIMEOUT_NEXT_SEGMENT_MS        RREQ_MAX_TIMEOUT_NEXT_SEGMENT_MS

//*****************************************************************************
//! Prototypes
//*****************************************************************************
static void AppRREQ_dataReadyHandler(uint16_t connHandle, uint16_t rangingCount);
static void AppRREQ_statusHandle(uint16_t connHandle, RREQClientStatus_e statusCode, uint8_t statusDataLen, uint8_t* statusData);
static void AppRREQ_completeEventHandler(uint16_t connHandle, uint16_t rangingCount, uint8_t status, RangingDBClient_procedureSegmentsReader_t segmentsReader);
static void AppRREQ_timerCB(BLEAppUtil_timerHandle timerHandle, BLEAppUtil_timerTermReason_e reason, void *pData);
static void AppRREQ_CSEventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData);
static bool AppRREQ_extEvtHandler(AppRREQEventType_e event, BLEAppUtil_msgHdr_t *pMsgData);

// Contains the inputs for the enable API to be passed to the timer
typedef struct
{
  uint16_t connHandle;
  RREQEnableModeType_e enableMode;
}AppRREQ_enableData_t;

//*****************************************************************************
//! Globals
//*****************************************************************************

// The configuration of the Ranging Requester
const RREQConfig_t reqConfig =
{
  .onDemandSubConfig.onDemandSubType     = RREQ_PREFER_NOTIFY,  // Notification for on-demand data
  .onDemandSubConfig.controlPointSubType = RREQ_PREFER_NOTIFY,  // Notification for control point commands
  .onDemandSubConfig.dataReadySubType    = RREQ_PREFER_NOTIFY,  // Notification for data ready events
  .onDemandSubConfig.overwrittenSubType  = RREQ_PREFER_NOTIFY,  // Notification for data ready events
  .realTimeSubConfig.realTimeSubType     = RREQ_PREFER_NOTIFY,  // Real-time subscription type
  .timeoutConfig.timeOutControlPointRsp  = RREQ_TIMEOUT_CONTROL_POINT_RSP_MS, // Timeout for control point response; On-Demand mode only
  .timeoutConfig.timeOutDataReady        = RREQ_TIMEOUT_DATA_READY_MS,        // Timeout for data ready event; On-Demand mode only
  .timeoutConfig.timeOutFirstSegment     = RREQ_TIMEOUT_FIRST_SEGMENT_MS,     // Timeout for receiving first segment
  .timeoutConfig.timeOutNextSegment      = RREQ_TIMEOUT_NEXT_SEGMENT_MS,      // Timeout for next segment in milliseconds
};

// The call back of the Ranging Requester
const RREQCallbacks_t gRREQDataReadyCB =
{
  .pDataReadyCallback = AppRREQ_dataReadyHandler,              // Callback for data ready events
  .pDataCompleteEventCallback = AppRREQ_completeEventHandler,  // Callback for complete events
  .pStatusCallback = AppRREQ_statusHandle,                     // Callback for status events
};

// Events handlers structure for Channel Sounding
BLEAppUtil_EventHandler_t gAppRREQCSHandler =
{
    .handlerType    = BLEAPPUTIL_CS_TYPE,
    .pEventHandler  = AppRREQ_CSEventHandler,
    .eventMask      = BLEAPPUTIL_CS_EVENT_CODE
};

// The external event handler for RREQ events
static AppRREQ_eventHandler_t gExtEvtHandler = NULL;

// The external event handler for data complete events
static AppRREQ_dataEventHandler_t gDataCompleteEvtHandler = NULL;

// Timer handle for the RREQ Enable
BLEAppUtil_timerHandle gEnableTimerHandle = BLEAPPUTIL_TIMER_INVALID_HANDLE;

//*****************************************************************************
//! Public Functions
//*****************************************************************************

/*******************************************************************************
 * Public function defined in app_ranging_client_api.h.
 */
uint8_t AppRREQ_start(void)
{
  uint8_t status = USUCCESS;

  // Start the ranging requester
  status = RREQ_Start(&gRREQDataReadyCB ,&reqConfig);

  if (status == SUCCESS )
  {
    // Register to the Channel Sounding events
    BLEAppUtil_registerEventHandler(&gAppRREQCSHandler);
  }

  return status;
}

/*******************************************************************************
 * Public function defined in app_ranging_client_api.h.
 */
uint8_t AppRREQ_enable(uint16_t connHandle, RREQEnableModeType_e enableMode)
{
  uint8_t status = SUCCESS;

  // Call the ranging client enable API
  status = RREQ_Enable(connHandle, enableMode);
  if(status == blePending)
  {
    linkDBInfo_t connInfo = {0};
    // Get the connection information
    status = linkDB_GetInfo(connHandle, &connInfo);

    // Check if the connection is valid
    if ( (status != bleTimeout) && (status != bleNotConnected))
    {
      // Allocate memory for the enable data
      AppRREQ_enableData_t *pEnableData = (AppRREQ_enableData_t *)BLEAppUtil_malloc(sizeof(AppRREQ_enableData_t));
      if(pEnableData != NULL)
      {
        // Fill the enable data structure
        pEnableData->connHandle = connHandle;
        pEnableData->enableMode = enableMode;

        if (gEnableTimerHandle == BLEAPPUTIL_TIMER_INVALID_HANDLE)
        {
          // Start the timer for the enable process
          gEnableTimerHandle = BLEAppUtil_startTimer(AppRREQ_timerCB, connInfo.connInterval, false, pEnableData);
        }

        // Update the status to pending
        status = blePending;
      }
      else
      {
        // Failed to allocate memory, handle the error
        status = bleMemAllocError;
      }
    }
  }

  return status;
}

/*******************************************************************************
 * Public function defined in app_ranging_client_api.h.
 */
uint8_t AppRREQ_disable(uint16_t connHandle)
{
  return RREQ_Disable(connHandle);
}

/*******************************************************************************
 * Public function defined in app_ranging_client_api.h.
 */
uint8_t AppRREQ_abort(uint16_t connHandle)
{
  return RREQ_Abort(connHandle);
}

/*******************************************************************************
 * Public function defined in app_ranging_client_api.h.
 */
uint8_t AppRREQ_GetRangingData(uint16_t connHandle, uint16_t rangingCount)
{
  return RREQ_GetRangingData(connHandle, rangingCount);
}

/*******************************************************************************
 * Public function defined in app_ranging_client_api.h.
 */
void AppRREQ_registerEvtHandler(AppRREQ_eventHandler_t fEventHandler)
{
  gExtEvtHandler = fEventHandler;
}

/*******************************************************************************
 * Public function defined in app_ranging_client_api.h.
 */
void AppRREQ_registerDataEvtHandler(AppRREQ_dataEventHandler_t pDataEventHandler)
{
  gDataCompleteEvtHandler = pDataEventHandler;
}

/*******************************************************************************
 * Public function defined in app_ranging_client_api.h.
 */
uint8_t AppRREQ_configureCharRegistration(uint16_t connHandle, uint16_t charUUID, uint8_t subscribeMode)
{
  return RREQ_ConfigureCharRegistration(connHandle, charUUID, (RREQConfigSubType_e) subscribeMode);
}

/*******************************************************************************
 * Public function defined in app_ranging_client_api.h.
 */
uint8_t AppRREQ_procedureStarted(uint16_t connHandle, uint16_t procedureCounter)
{
    // Send the subevent results to the RAS profile
    return RREQ_ProcedureStarted(connHandle, procedureCounter);
}

/*********************************************************************
 * @fn      AppRREQ_CSEventHandler
 *
 * @brief   The purpose of this function is to handle CS events
 *
 * input parameters
 *
 * @param   event - message event.
 * @param   pMsgData - pointer to message data.
 *
 * output parameters
 *
 * @param   None
 *
 * @return  none
 */
static void AppRREQ_CSEventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
    csEvtHdr_t *pCsEvt = ( csEvtHdr_t * )pMsgData;
    uint8_t opcode = pCsEvt->opcode;

    switch( opcode )
    {
      case CS_SUBEVENT_RESULT:
      {
        ChannelSounding_subeventResults_t* subeventResultsEvt = (ChannelSounding_subeventResults_t *) pCsEvt;

        // Notify the profile that a new procedure has started
        // Note: that if the next subevent will be part of the same procedure, the API
        //       will return @ref bleAlreadyInRequestedMode and won't take any action,
        //       we rely on it
        RREQ_ProcedureStarted(subeventResultsEvt->connHandle, subeventResultsEvt->procedureCounter);

        break;
      }

      default:
      {
        break;
      }
    }
}

//*****************************************************************************
//! Internal Functions
//*****************************************************************************

/*********************************************************************
 * @fn      AppRREQ_extEvtHandler
 *
 * @brief   The purpose of this function is to forward the event to the external
 *          event handler that registered to handle the events.
 *
 * input parameters
 *
 * @param   event     - RREQ message event.
 * @param   pMsgData  - pointer to message data.
 *
 * output parameters
 *
 * @param   None
 *
 * @return  true if the event was forwarded to the external handler, false otherwise.
 */
static bool AppRREQ_extEvtHandler(AppRREQEventType_e event, BLEAppUtil_msgHdr_t *pMsgData)
{
  bool reVal = false;

  // Send the event to the upper layer if its handle exists
  if (gExtEvtHandler != NULL)
  {
    gExtEvtHandler( event, pMsgData);
    reVal = true;
  }

  return reVal;
}

/*********************************************************************
 * @fn      AppRREQ_dataReadyHandler
 *
 * @brief   Callback function triggered when the RAS server is ready to send data.
 *          This function is called when the RAS server indicates that it has data
 *          available for the client. It triggers the RREQ module to start reading
 *          the data from the server.
 *
 * input parameters
 *
 * @param connHandle -  The connection handle associated with the data.
 * @param rangingCount  - CS procedure counter.
 *
 * output parameters
 *
 * @param   None
 *
 * @return None
 */
static void AppRREQ_dataReadyHandler(uint16_t connHandle, uint16_t rangingCount)
{
  AppRREQDataReady_t dataReadyInfo;
  dataReadyInfo.connHandle = connHandle;
  dataReadyInfo.rangingCount = rangingCount;
  bool isEventForwarded;

  isEventForwarded = AppRREQ_extEvtHandler(APP_RREQ_EVENT_DATA_READY, (BLEAppUtil_msgHdr_t *)&dataReadyInfo);

  if (isEventForwarded == false)
  {
    // if the event was not forwarded, trigger the data reading process internally as default behavior
    RREQ_GetRangingData(connHandle, rangingCount);
  }
}

/*********************************************************************
 * @fn      AppRREQ_statusHandle
 *
 * @brief   Callback function triggered when an error occurs in the RAS client.
 *          This function is called when an error occurs during the RAS client operation.
 *          It currently does not perform any specific action but can be extended in the future.
 *
 * input parameters
 *
 * @param   connHandle - The connection handle associated with the error.
 * @param   statusCode - The status code indicating the type of event.
 * @param   statusDataLen - length of statusData
 * @param   statusData - Additional data related to the error, if any.
 *
 * output parameters
 *
 * @param    None
 *
 * @return   None
 */
static void AppRREQ_statusHandle(uint16_t connHandle, RREQClientStatus_e statusCode, uint8_t statusDataLen, uint8_t* statusData)
{
  AppRREQStatus_t AppRREQStatus;
  CS_readRemoteCapCmdParams_t pParams;

  switch(statusCode)
  {
    case RREQ_TIMEOUT_CONTROL_POINT_RSP:
      // Timeout occurred while waiting for control point response. Relevant for On-Demand mode only.
      // Happens after successful @ref RREQ_Abort or @ref RREQ_GetRangingData calls
    case RREQ_TIMEOUT_DATA_READY:
      // Timeout occurred while waiting for data ready event. Relevant for On-Demand mode only.
      // Happens between successful @ref RREQ_procedureStarted and receiving Data-Ready notification from server
    case RREQ_TIMEOUT_SEGMENTS:
      // Timeout occurred while waiting for the first or next segment(s) of the ranging data.
      // If RAS mode is set to On-Demand, the profile has already called RREQ_Abort internally, therefore expect abort events.
      // If RAS mode is set to Real-Time, the profile will automatically call RREQ_ConfigureCharRegistration to disable Real-Time mode,
      // therefore expect @ref RREQ_ConfigureCharRegistration events.
    case RREQ_INIT_FAILED:
      // Failed to perform initialization procedure,
      // call to @ref RREQ_Disable in order to cleanup the profile
      // or @ref RREQ_Enable to retry and continue from the last state.
    case RREQ_INIT_DONE:
#ifdef Embpapst_EmbCS
      pParams.connHandle = connHandle;
      CS_ReadRemoteSupportedCapabilities(&pParams);
      break;
#endif
      // Initialization done successfully, ready to start ranging procedures
    case RREQ_CHAR_CONFIGURATION_DONE:
      // Characteristic configuration completed successfully
    case RREQ_CHAR_CONFIGURATION_FAILED:
      // Characteristic configuration failed
    case RREQ_CHAR_CONFIGURATION_REJECTED:
      // Characteristic configuration rejected by server by sending @ref ATT_ERROR_RSP.
      // In a case we tried to unregister, the profile will consider the characteristic as unregistered.
    case RREQ_ABORTED_SUCCESSFULLY:
      // Procedure aborted successfully
    case RREQ_ABORTED_UNSUCCESSFULLY:
      // Procedure aborted unsuccessfully
    case RREQ_SERVER_BUSY:
      // Server is busy, cannot process the request
    case RREQ_PROCEDURE_NOT_COMPLETED:
      // Procedure not completed successfully
    case RREQ_NO_RECORDS:
      // No records found for the request
    case RREQ_DATA_OVERWRITTEN:
      // Data has been overwritten
    default:
      // Unknown status code
    break;
  }

  // Prepare the error status structure
  AppRREQStatus.connHandle    = connHandle;
  AppRREQStatus.statusCode    = (uint8_t) statusCode;
  AppRREQStatus.statusData    = statusData;
  AppRREQStatus.statusDataLen = statusDataLen;

  // Send the event to the upper layer
  AppRREQ_extEvtHandler(APP_RREQ_EVENT_STATUS, (BLEAppUtil_msgHdr_t *)&AppRREQStatus);
}

/*********************************************************************
 * @fn      AppRREQ_completeEventHandler
 *
 * @brief   Handles the completion of an application request event.
 *          This function is triggered when the RREQ module completes the
 *          data reading process. It forwards the completion event to the
 *          registered external event handler, if available.
 *
 * input parameters
 *
 * @param connHandle - The connection handle associated with the event.
 * @param rangingCount - The CS procedure counter of ranging data available.
 * @param status - The status of the event (SUCCESS or error).
 * @param segmentsReader - The segments reader for the ranging data.
 *
 * output parameters
 *
 * @param   None
 *
 * @return   None
 */
static void AppRREQ_completeEventHandler(uint16_t connHandle, uint16_t rangingCount, uint8_t status, RangingDBClient_procedureSegmentsReader_t segmentsReader)
{
  // Send the event to the upper layer if its handle exists
  if (gDataCompleteEvtHandler != NULL)
  {
    gDataCompleteEvtHandler(connHandle, rangingCount, status, segmentsReader);
  }
  else
  {
    // No external handler registered, free the segments reader if needed
    if (status == SUCCESS)
    {
        RangingDBClient_freeSegmentsReader(&segmentsReader);
    }
  }
}

/*********************************************************************
 * @fn      AppRREQ_timerCB
 *
 * @brief   Timer callback function for the RREQ enable process.
 *          This function is called when the timer expires.
 *          It checks the reason for the timer expiration and
 *          performs the necessary actions.
 *
 * input parameters
 *
 * @param   timerHandle - The handle of the timer that expired.
 * @param   reason - The reason for the timer expiration.
 * @param   pData - Pointer to the data associated with the timer.
 *
 * output parameters
 *
 * @param   None
 *
 * @return None
 */
static void AppRREQ_timerCB(BLEAppUtil_timerHandle timerHandle, BLEAppUtil_timerTermReason_e reason, void *pData)
{
  if (reason == BLEAPPUTIL_TIMER_TIMEOUT)
  {
    if (pData != NULL)
    {
      // Fill the enable data structure
      AppRREQ_enableData_t enableData;
      enableData.connHandle = ((AppRREQ_enableData_t *)pData)->connHandle;
      enableData.enableMode = ((AppRREQ_enableData_t *)pData)->enableMode;

      // Free the allocated memory for the enable data
      BLEAppUtil_free(pData);

      // Reset the timer handle
      gEnableTimerHandle = BLEAPPUTIL_TIMER_INVALID_HANDLE;

      // Call the RREQ enable function
      AppRREQ_enable(enableData.connHandle, enableData.enableMode);
    }
  }
  else
  {
    // Reset the timer handle
    gEnableTimerHandle = BLEAPPUTIL_TIMER_INVALID_HANDLE;
  }
}

#endif // RANGING_CLIENT
