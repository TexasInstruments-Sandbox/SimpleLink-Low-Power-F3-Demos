
/**
 * @file app_parse.c
 * @brief ANCS and AMS notification and attribute parsing logic for CC27xx BLE client.
 *
 * This file contains functions for parsing, storing, and displaying notification and attribute
 * data received from iOS devices via the Apple Notification Center Service (ANCS) and
 * Apple Media Service (AMS) over Bluetooth Low Energy.
 *
 * Features:
 * - Parses and manages ANCS notification queue and attributes
 * - Handles AMS entity and attribute updates for media info
 * - Provides utility functions for notification queue management and display
 * - Integrates with BLEAppUtil and Display framework for output
 *
 * Usage:
 * - Call parsing and queue functions from the GATT event handler to process incoming data
 * - Use print functions to display notification and media info on the serial/UART display
 ====================================================================================
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


/*********************************************************************
 * INCLUDES
 */
#include <string.h>
#include <stdio.h>
#include "ti_drivers_config.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include <ti/display/Display.h>
#include <ti/display/DisplayUart2.h>
#include <app_main.h>
#include "app_ancs.h"


/*********************************************************************
 * CONSTANTS
 */

#define NOTIF_ATTR_REQUEST_METADATA_LENGTH        0x08
#define DATA_BUFFER_SIZE                          100

#define REQUESTED_ATTR_ID_APPID                   0x01
#define REQUESTED_ATTR_ID_DS                      0x02
#define REQUESTED_ATTR_ID_TITLE                   0x04
#define REQUESTED_ATTR_ID_SUBTITLE                0x08
#define REQUESTED_ATTR_ID_MESSAGE                 0x10
#define REQUESTED_ATTR_ID_MESSAGE_SIZE            0x20
#define REQUESTED_ATTR_ID_DATE                    0x40


#define ATTR_APPID_REQUEST_SIZE                   0
#define ATTR_TITLE_REQUEST_SIZE                   20
#define ATTR_SUBTITLE_REQUEST_SIZE                30
#define ATTR_MESSAGE_REQUEST_SIZE                 DATA_BUFFER_SIZE - 1
#define ATTR_MESSAGE_SIZE_REQUEST_SIZE            0
#define ATTR_DATE_REQUEST_SIZE                    0
#define MAX_NOTIF_QUEUE_SIZE                      2

/*********************************************************************
 * TYPEDEFS
 */




/*********************************************************************
 * GLOBAL VARIABLES
 */
// Stores the connection handle to the iPhone.
uint16_t Ancs_connHandle;

// Stores the state of the Data Source GATT notification processing function (Ancs_processDataServiceNotif()).
uint8_t notifAttrPktProcessState;

// Stores the state of the Data Source GATT notification processing function (Ancs_processAppAttr()).
uint8_t appAttrPktProcessState;

extern Display_Handle disp_handle;
/*********************************************************************
 * LOCAL VARIABLES
 */

// Used to stored the retrieved attribute data.
static uint8_t  dataBuf[DATA_BUFFER_SIZE] = { '\0' };

// Stores the length of the retrieved attribute data.
static uint16_t dataLen = 0x0000;

// Stores the notification ID of the head of the queue.
static uint8_t currentNotifUID[ANCS_NOTIF_UID_LENGTH] = { 0x00 };
static uint8_t incomingCallUID[ANCS_NOTIF_UID_LENGTH] = { 0x00 };

// Holds the value for whether or not there is an incomingCall type notification in the queue.
static bool haveIncomingCall = FALSE;

// Points to the head of the queue.
static notifQueueNode_t *pNotifQueueFront = NULL;
static notifQueueNode_t *pNotifQueueTail = NULL;
static notifQueueNode_t *pCurrentProcessingNode = NULL;


/*********************************************************************
 * LOCAL FUNCTIONS
 */

// Basic link-list structure queue functions.
static void Ancs_findAndRemoveFromQueue(uint8_t *pNotifUID);
static notifQueueNode_t* Ancs_findNotifInQueue(uint8_t *pNotifUID);
       void Ancs_popAllNotifsFromQueue(void);
static void Ancs_popNotifFromQueue(void);
static void Ancs_pushNotifToQueue(uint8_t categoryID, uint8_t *pNotifUID);
static bool Ancs_queueEmpty(void);
static uint8_t Ancs_queueSize(void);

// Functions used to process incoming GATT notifications from the
// Notification Source and Data Source, and request additional data.
       void Ancs_acceptIncomingCall(void);
       void Ancs_declineIncomingCall(void);
static uint8_t Ancs_getNotifAttr(uint8_t *pNotificationUID, uint8_t attributeID, uint16_t len);
static uint8_t Ancs_getAppAttr(uint8_t *appID, uint8_t attributeID);
static void Ancs_handleNotifAttrRsp(uint8_t *pNotificationUID);
static uint8_t Ancs_performNegativeAction(uint8_t *notifUID);
static uint8_t Ancs_performPositiveAction(uint8_t *notifUID);
static void Ancs_processAppAttr(gattMsgEvent_t *pMsg);
       void Ancs_processDataServiceNotif(gattMsgEvent_t *pMsg);
static void Ancs_processNotifications(void);
static void Ancs_processNotificationServiceNotif(notifQueueNode_t *pNotif);
static void Ancs_printNotifDate(uint8_t*  dataBuf);
       void Ancs_queueNewNotif(gattMsgEvent_t *pMsg);
       uint8_t Ancs_subsDataSrc(void);
       uint8_t Ancs_subsNotifSrc(void);

static notifQueueNode_t *Ancs_findNextUnprocessedNode(void);
/*********************************************************************
 * QUEUE FUNCTIONS (In alphabetical order)
 */



/*********************************************************************
 * @fn      Ancs_findAndRemoveFromQueue
 *
 * @brief   Find a specific notification and remove it from the queue
 *
 * @param   pNotifUID  - notification UID
 *
 * @return  none
 */
static void Ancs_findAndRemoveFromQueue(uint8_t *pNotifUID)
{
  if (pNotifQueueFront == NULL)
    return;
    
  notifQueueNode_t *pSearch;
  notifQueueNode_t *pSearchLast;
  pSearch = pNotifQueueFront;
  pSearchLast = NULL;

  uint8_t notifUID[ANCS_NOTIF_UID_LENGTH];
  VOID memcpy(notifUID, pNotifUID, ANCS_NOTIF_UID_LENGTH);

  // Find the node to remove
  while (pSearch != NULL && (memcmp(notifUID, pSearch->notifData.notificationUID, ANCS_NOTIF_UID_LENGTH) != 0))
  {
    pSearchLast = pSearch;
    pSearch = pSearch->pNext;
  }

  if (pSearch != NULL) // Found the node to remove
  {
    // If removing the current processing node, clear it
    if (pSearch == pCurrentProcessingNode)
    {
      pCurrentProcessingNode = NULL;
    }

    if (pSearchLast == NULL) // Removing front node
    {
      pNotifQueueFront = pSearch->pNext;
      if (pNotifQueueFront == NULL) // Was the only node
      {
        pNotifQueueTail = NULL;
      }
    }
    else // Removing middle or tail node
    {
      pSearchLast->pNext = pSearch->pNext;
      if (pSearch == pNotifQueueTail) // Removing tail node
      {
        pNotifQueueTail = pSearchLast;
      }
    }
    
    ICall_free(pSearch);
    Ancs_queueSize();
  }

  return;
}

/*********************************************************************
 * @fn      Ancs_findNotifInQueue
 *
 * @brief   Find a Notification & Category in the existing list
 *
 * @param   pNotifUID  - notification UID
 *
 * @return  none
 */
static notifQueueNode_t* Ancs_findNotifInQueue(uint8_t *pNotifUID)
{
  notifQueueNode_t *pSearch;
  uint8_t notifUID[ANCS_NOTIF_UID_LENGTH];

  VOID memcpy(notifUID, pNotifUID, ANCS_NOTIF_UID_LENGTH);

  pSearch = pNotifQueueFront;
  while ((memcmp(notifUID, pSearch->notifData.notificationUID, ANCS_NOTIF_UID_LENGTH) != 0) && pSearch != NULL)
  {
    pSearch = pSearch->pNext;
  }

  if (pSearch == NULL) // Not in the list
    return NULL;

  else
    return pSearch;
}

/*********************************************************************
 * @fn      Ancs_popAllNotifsFromQueue
 *
 * @brief   Clear the queue of all notifications
 *
 * @param   none
 *
 * @return  none
 */
void Ancs_popAllNotifsFromQueue(void)
{
  notifQueueNode_t *pSearch;
  notifQueueNode_t *pDelete;

  pSearch = pNotifQueueFront;

  while (pSearch != NULL)
  {
    pDelete = pSearch;
    pSearch = pSearch->pNext;
    ICall_free(pDelete);
    pDelete = NULL;
  }
  
  pNotifQueueFront = NULL;
  pNotifQueueTail = NULL;
  pCurrentProcessingNode = NULL;

  return;
}

/*********************************************************************
 * @fn      Ancs_popNotifFromQueue
 *
 * @brief   Remove the front element (oldest) and move front pointer
 *
 * @param   none
 *
 * @return  none
 */
static void Ancs_popNotifFromQueue(void)
{
  if (pNotifQueueFront == NULL)
    return;

  notifQueueNode_t *pFrontOld = pNotifQueueFront;

  // If removing the current processing node, clear it
  if (pFrontOld == pCurrentProcessingNode)
  {
    pCurrentProcessingNode = NULL;
  }

  pNotifQueueFront = pNotifQueueFront->pNext;
  
  // If we just removed the last element, update tail
  if (pNotifQueueFront == NULL)
  {
    pNotifQueueTail = NULL;
  }
  
  ICall_free(pFrontOld);
  Ancs_queueSize();

  return;
}

/*********************************************************************
 * @fn      Ancs_pushNotifToQueue
 *
 * @brief   Add a Notification to end of list and start processing immediately
 *
 * @param   categoryID - category ID of the notification
 * @param   pNotifUID  - notification UID
 *
 * @return  none
 */
static void Ancs_pushNotifToQueue(uint8_t categoryID, uint8_t *pNotifUID)
{
  notifQueueNode_t *pNew;

  pNew = ICall_malloc(sizeof(notifQueueNode_t));
  if (pNew == NULL)
    return;

  // Store categoryID and notification ID.
  pNew->notifData.categoryID = categoryID;
  VOID memcpy(pNew->notifData.notificationUID, pNotifUID, ANCS_NOTIF_UID_LENGTH);
  pNew->notifData.currentState = NOTI_ATTR_ID_BEGIN;
  pNew->notifData.requestedAttrs = 0;
  pNew->pNext = NULL;

  if (pNotifQueueFront == NULL) // Empty list
  {
    pNotifQueueFront = pNew;
    pNotifQueueTail = pNew;
  }
  else  // Add to the tail (newest)
  {
    pNotifQueueTail->pNext = pNew;
    pNotifQueueTail = pNew;
  }
  Ancs_queueSize();

  // Only start processing if nothing is currently being processed
  if (pCurrentProcessingNode == NULL)
  {
    pCurrentProcessingNode = pNew;
    Ancs_processNotificationServiceNotif(pNew);
  }

  return;
}

/*********************************************************************
 * @fn      Ancs_queueEmpty
 *
 * @brief   Indicate if the notification queue is empty or not.
 *
 * @param   none
 *
 * @return  bool - Return TRUE if pNotifQueueFront equals NULL, FALSE else.
 */
static bool Ancs_queueEmpty(void)
{
  return (pNotifQueueFront == NULL);
}

/*********************************************************************
 * @fn      Ancs_queueSize
 *
 * @brief   Print the current size of the notification queue.
 *
 * @param   none
 *
 * @return  uint8_t - Number of notifications in the queue.
 */
static uint8_t Ancs_queueSize(void)
{
  uint8_t notifCount = 0;
  notifQueueNode_t *pCount;
  pCount = pNotifQueueFront;

  while(pCount != NULL)
  {
    notifCount++;
    pCount = pCount->pNext;
  }
  // Display_printf(disp_handle, 0xFF, 0, "Total Notifications:\t%d", notifCount);

  return notifCount;
}

/*********************************************************************
 * NOTIFICATION FUNCTIONS (In alphabetical order)
 */

/*********************************************************************
 * @fn      Ancs_acceptIncomingCall
 *
 * @brief   Accept an incoming phone call.
 *
 * @param   none
 *
 * @return  none
 */
void Ancs_acceptIncomingCall(void)
{
  if(haveIncomingCall == TRUE)
  {
    Ancs_performPositiveAction(incomingCallUID);
    haveIncomingCall = FALSE;
    Display_printf(disp_handle, 0xFF, 0, "Accepted Incoming Call");
  }
}


/*********************************************************************
 * @fn      Ancs_declineIncomingCall
 *
 * @brief   Reject an incoming phone call.
 *
 * @param   none
 *
 * @return  none
 */
void Ancs_declineIncomingCall(void)
{
  if(haveIncomingCall == TRUE)
  {
      Ancs_performNegativeAction(incomingCallUID);
      haveIncomingCall = FALSE;
    Display_printf(disp_handle, 0xFF, 0, "Declined Incoming Call");
  }
}

/*********************************************************************
 * @fn      Ancs_getNotifAttr
 *
 * @brief   Get notification attributes.
 *
 * @param   pNotificationUID - notification's ID.
 *
 * @param   attributeID - attribute's ID.
 *
 * @return  uint8_t SUCCESS/FAILURE
 */
static uint8_t Ancs_getNotifAttr(uint8_t *pNotificationUID, uint8_t attributeID, uint16_t len)
{
  uint8_t status;
  uint8_t cmdLen = 8;
  if (len == 0)
    cmdLen = 6;

  // Do a write
  attWriteReq_t req;

  req.pValue = GATT_bm_alloc(Ancs_connHandle, ATT_WRITE_REQ, cmdLen, NULL);
  uint8_t *requestPayload = req.pValue;
  if (req.pValue != NULL)
  {
    // Get the ANCS control point handle.
    req.handle = Ancs_handleCache[ANCS_CTRL_POINT_HDL_START];

    // Set command length.
    req.len = cmdLen;

    // Set Command ID.
    *requestPayload = COMMAND_ID_GET_NOTIFICATION_ATTRIBUTES;
    requestPayload++;

    // Set NotificationUID
    VOID memcpy(requestPayload, pNotificationUID, ANCS_NOTIF_UID_LENGTH);
    requestPayload += ANCS_NOTIF_UID_LENGTH;

    // Set attributeID
    *requestPayload = attributeID;
    requestPayload++;

    // Set length to desired max length to be retrieved.
    *requestPayload = LO_UINT16(len);
    requestPayload++;
    *requestPayload = HI_UINT16(len);

    // Signature and command must be set to zero.
    req.sig = 0;
    req.cmd = 0;

    // Execute the write.
    status = GATT_WriteCharValue(Ancs_connHandle, &req, ICall_getEntityId());
    // Display_printf(disp_handle, 0xFF, 0, "attempting to get notification attributes");
    if ( status == blePending)
    {
        //delay for ble stack is no longer pending
        ClockP_usleep(100000);
        //send again
        status = GATT_WriteCharValue(Ancs_connHandle, &req, ICall_getEntityId());
    }
    else if (status != SUCCESS)
    {
        // If it fails free the message.
        Display_printf(disp_handle, 0xFF, 0, "CP WRITE ERROR:\t%d",status);
        GATT_bm_free((gattMsg_t *) &req, ATT_WRITE_REQ);
    }
  }

  return status;
}

/*********************************************************************
 * @fn      Ancs_getAppAttr
 *
 * @brief   Get application attributes.
 *
 * @param   appID - applciation's ID
 *
 * @param   attributeID - attribute's ID.
 *
 * @return  uint8_t SUCCESS/FAILURE
 */
static uint8_t Ancs_getAppAttr(uint8_t *appID, uint8_t attributeID)
{
  uint8_t status;

  uint8_t *lenCheck = appID;
  uint8_t appIDLen = 0;

  while(*lenCheck != '\0')
  {
    lenCheck++;
    appIDLen++;
  }

  // Add 1 for the NULL terminator.
  appIDLen++;


  // 1 for Command ID, Length of the AppID, 1 for the AttrID
  uint8_t cmdLen = 1 + appIDLen + 1;

  // Do a write
  attWriteReq_t req;

  req.pValue = GATT_bm_alloc(Ancs_connHandle, ATT_WRITE_REQ, cmdLen, NULL);
  uint8_t *requestPayload = req.pValue;
  if (req.pValue != NULL)
  {
    // Get the ANCS control point handle.
    req.handle = Ancs_handleCache[ANCS_CTRL_POINT_HDL_START];

    // Set command length.
    req.len = cmdLen;

    // Set Command ID.
    *requestPayload = COMMAND_ID_GET_APP_ATTRIBUTES;
    requestPayload++;

    // Set AppID
    VOID memcpy(requestPayload, appID, appIDLen);
    requestPayload += appIDLen;

    // Set attributeID
    *requestPayload = attributeID;
    requestPayload++;

    // Signature and command must be set to zero.
    req.sig = 0;
    req.cmd = 0;

    // Execute the write.
    status = GATT_WriteCharValue(Ancs_connHandle, &req, ICall_getEntityId());
    if ( status == blePending)
    {
        //delay for ble stack is no longer pending
        ClockP_usleep(100000);
        //send again
        status = GATT_WriteCharValue(Ancs_connHandle, &req, ICall_getEntityId());
    }
    else if (status != SUCCESS)
    {
        // If it fails free the message.
        Display_printf(disp_handle, 0xFF, 0, "CP WRITE ERROR:\t%d",status);
        GATT_bm_free((gattMsg_t *) &req, ATT_WRITE_REQ);
    }
  }

  return status;
}

/*********************************************************************
 * @fn      Ancs_handleNotifAttrRsp
 *
 * @brief   Handle response value of Notification Attributes from iOS device.
 *
 * @param   attrID - attributes ID.
 *
 * @return  uint8_t SUCCESS/FAILURE
 */
static void Ancs_handleNotifAttrRsp(uint8_t *pNotificationUID)
{
  // Will store the queue node with the passed notification UID.
  notifQueueNode_t *pNode;

  // Retrieve the notification with the passed UID.
  pNode = Ancs_findNotifInQueue(pNotificationUID);

  if (pNode == NULL) return;

  switch ( pNode->notifData.currentState )
  {
  // The initial state is used to kick-off the state machine and
  // immediately proceed to the AppID state (hence the missing break).
  case NOTI_ATTR_ID_BEGIN:
    pNode->notifData.currentState = NOTI_ATTR_ID_APPID;

  // If the AppID request flag hasn't been set, request the AppID attribute
  // data and set the AppID request flag.
  case NOTI_ATTR_ID_APPID:
    if( !(pNode->notifData.requestedAttrs & REQUESTED_ATTR_ID_APPID) )
    {
      Ancs_getNotifAttr(pNotificationUID, NOTIFICATION_ATTRIBUTE_ID_APP_IDENTIFIER, ATTR_APPID_REQUEST_SIZE);
      pNode->notifData.requestedAttrs |= REQUESTED_ATTR_ID_APPID;
    }
    break;

  case APP_ATTR_ID_DN:
    if( !(pNode->notifData.requestedAttrs & REQUESTED_ATTR_ID_DS) )
    {
      Ancs_getAppAttr(dataBuf, APP_ATTRIBUTE_ID_DISPLAY_NAME);
      pNode->notifData.requestedAttrs |= REQUESTED_ATTR_ID_DS;
    }
    break;

  // If the Title request flag hasn't been set, request the Title attribute
  // data and set the Title request flag.
  case NOTI_ATTR_ID_TITLE:
    if( !(pNode->notifData.requestedAttrs & REQUESTED_ATTR_ID_TITLE) )
    {
      Ancs_getNotifAttr(pNotificationUID, NOTIFICATION_ATTRIBUTE_ID_TITLE, ATTR_TITLE_REQUEST_SIZE);
      pNode->notifData.requestedAttrs |= REQUESTED_ATTR_ID_TITLE;
    }
    break;

  // If the Subtitle request flag hasn't been set, request the Subtitle attribute
  // data and set the Subtitle request flag.
  case NOTI_ATTR_ID_SUBTITLE:
    if( !(pNode->notifData.requestedAttrs & REQUESTED_ATTR_ID_SUBTITLE) )
    {
      Ancs_getNotifAttr(pNotificationUID, NOTIFICATION_ATTRIBUTE_ID_SUBTITLE, ATTR_SUBTITLE_REQUEST_SIZE);
      pNode->notifData.requestedAttrs |= REQUESTED_ATTR_ID_SUBTITLE;
    }
    break;

  // If the Message request flag hasn't been set, request the Message attribute
  // data and set the Message request flag.
  case NOTI_ATTR_ID_MESSAGE:
    if( !(pNode->notifData.requestedAttrs & REQUESTED_ATTR_ID_MESSAGE) )
    {
      Ancs_getNotifAttr(pNotificationUID, NOTIFICATION_ATTRIBUTE_ID_MESSAGE, ATTR_MESSAGE_REQUEST_SIZE);
      pNode->notifData.requestedAttrs |= REQUESTED_ATTR_ID_MESSAGE;
    }
    break;

  // If the Message Size request flag hasn't been set, request the Message Size attribute
  // data and set the Message Size request flag.
  case NOTI_ATTR_ID_MESSAGE_SIZE:
    if( !(pNode->notifData.requestedAttrs & REQUESTED_ATTR_ID_MESSAGE_SIZE) )
    {
      Ancs_getNotifAttr(pNotificationUID, NOTIFICATION_ATTRIBUTE_ID_MESSAGE_SIZE, ATTR_MESSAGE_SIZE_REQUEST_SIZE);
      pNode->notifData.requestedAttrs |= REQUESTED_ATTR_ID_MESSAGE_SIZE;
    }
    break;

  // If the Date request flag hasn't been set, request the Date attribute
  // data and set the Date request flag.
  case NOTI_ATTR_ID_DATE:
    if( !(pNode->notifData.requestedAttrs & REQUESTED_ATTR_ID_DATE) )
    {
      Ancs_getNotifAttr(pNotificationUID, NOTIFICATION_ATTRIBUTE_ID_DATE, ATTR_DATE_REQUEST_SIZE);
      pNode->notifData.requestedAttrs |= REQUESTED_ATTR_ID_DATE;
    }
    break;

  // End state, do nothing but signify all requests have been made.
  case NOTI_ATTR_ID_END:
    break;

  default:
    break;

  }
  return;
}


/*********************************************************************
 * @fn      Ancs_processAppAttr
 *
 * @brief   Extract and reassemble the retrieved data from the Data Source notification (App attributes)
 *
 * @param   pMsg - GATT message.
 *
 * @return  none
 */
static void Ancs_processAppAttr(gattMsgEvent_t *pMsg)
{
  // Pointer to the GATT Msg data.
  uint8_t *packetData;

  // The variable that will keep track of the ANCS attribute
  // currently being processed.
  static uint8_t AttrID;

  // The variable will keep track of the current index the data
  // buffer will be written to if more than one packet is needed.
  static uint8_t currentDataBufWriteIndex;

  // Point to the GATT Msg data
  packetData = pMsg->msg.handleValueNoti.pValue;

  // Check if this is the first retrieved packet for potentially
  // a set of packets to be sent by the ANCS Data Service.
  switch(notifAttrPktProcessState)
  {
    case NOTI_ATTR_FIRST_PKT:
    {
      // Tracks the metadata length of the first packet.
      uint8_t responseLen = 0;

      // Initialize the data buffer write index to zero, as this is
      // the first packet.
      currentDataBufWriteIndex = 0;

      // Ensure Command ID is equal to zero as stated in the spec.
      if (*packetData != COMMAND_ID_GET_APP_ATTRIBUTES)
          return;
      packetData++;
      responseLen++;

      // Skip the appID.
      while(*packetData != '\0')
      {
        packetData++;
        responseLen++;
      }

      // Skip the NULL terminator.
      packetData++;
      responseLen++;

      // Store the ANCS attribute ID of the retrieved attribute.
      AttrID = *packetData;
      packetData++;
      responseLen++;

      // Store the 2-byte length of the data that is being retrieved.
      dataLen = BUILD_UINT16(*packetData, *(packetData + 1));

      // Check if the length is zero, if so the notification does not
      // have the specified attribute as stated in the ANCS spec.
      if ( dataLen == 0 )
      {
        if (AttrID == APP_ATTRIBUTE_ID_DISPLAY_NAME)
          Display_printf(disp_handle, 0xFF, 0, "* App Name:\tNot available");
        // pNotifQueueFront->notifData.currentState++;
        pCurrentProcessingNode->notifData.currentState++;
        Ancs_processNotifications();

        return;
      }

      // Move the pointer to the data portion.
      packetData += 2;
      responseLen += 2;

      // Clear the data buffer in preparation for the new data.
      VOID memset(dataBuf, '\0', DATA_BUFFER_SIZE);

      // If the data length specified in the first ANCS Data Service notification
      // is greater than the number of bytes that was sent in the
      // first data packet(total GATT msg length - request metadata), the data will be split into multiple packets.
      if (dataLen > pMsg->msg.handleValueNoti.len - responseLen)
      {
        // Copy the number of bytes that were sent in the
        // first packet to the data buffer, then set the
        // data buffer write index, and set the state from first packet
        // to continued packet.
        VOID memcpy(dataBuf, packetData, (pMsg->msg.handleValueNoti.len - responseLen));
        currentDataBufWriteIndex = (pMsg->msg.handleValueNoti.len - responseLen);
        appAttrPktProcessState = NOTI_ATTR_CONTINUE_PKT;

        // Subtract the number of data bytes contained in the first packet
        // from the total number of expected data bytes.
        dataLen -= (pMsg->msg.handleValueNoti.len - responseLen);
        return;
      }
      else
      {
        // In this case all the ANCS attribute data was contained in
        // the first packet so the data is copied, and both the index and
        // length are reset.
        VOID memcpy(dataBuf, packetData, dataLen);
        currentDataBufWriteIndex = 0;
        dataLen = 0x0000;
      }
    }

    case NOTI_ATTR_CONTINUE_PKT:
    {
      if (dataLen > 0)
      {
        // Copy all the data from the notification packet to the data buffer
        // starting from the current data buffer write index.
        VOID memcpy(dataBuf + currentDataBufWriteIndex, pMsg->msg.handleValueNoti.pValue,
                                                   pMsg->msg.handleValueNoti.len);
        // Subtract the number of data bytes contained in the packet from
        // the total, and increase the data buffer write index by that amount.
        dataLen -= pMsg->msg.handleValueNoti.len;
        currentDataBufWriteIndex += pMsg->msg.handleValueNoti.len;

        // Checks if this is the last the continued packet.
        if (dataLen == 0x0000)
        {
          // If so reset the write index and the state.
          currentDataBufWriteIndex = 0;
          notifAttrPktProcessState = NOTI_ATTR_FIRST_PKT;
        }
      }
    }
    break;

    default:
    break;
  }

  // Now we have real data, to display it on LCD for demo now,
  // customer needs to change it from here to deal with data
  if (dataLen == 0)
  {
    pCurrentProcessingNode->notifData.currentState++;

    if (AttrID == APP_ATTRIBUTE_ID_DISPLAY_NAME)
      //  Display_printf(disp_handle, 0xFF, 0, "* App Name:\t%s", (char* )dataBuf);
      strncpy(pCurrentProcessingNode->notifInfo.appName, (char *)dataBuf, sizeof(pCurrentProcessingNode->notifInfo.appName)-1);
      pCurrentProcessingNode->notifInfo.appName[sizeof(pCurrentProcessingNode->notifInfo.appName)-1] = '\0';
  }
  // Check if the dataLen variable was overflowed if packets got mismatched.
  // This may occur if rapid connecting and disconnecting to the device is performed.
  else if(dataLen > DATA_BUFFER_SIZE - 1)
  {
     Display_printf(disp_handle, 0xFF, 0, "* App Name:\tDATA CORRUPTED");
    Ancs_findAndRemoveFromQueue(currentNotifUID);
    notifAttrPktProcessState = NOTI_ATTR_FIRST_PKT;
  }
  // Continue processing the current notification.
  Ancs_processNotifications();
}

/*********************************************************************
 * @fn      Ancs_processDataServiceNotif
 *
 * @brief   Extract and reassemble the retrieved data from the Data Source notifications
 *
 * @param   pMsg - GATT message.
 *
 * @return  none
 */
void Ancs_processDataServiceNotif(gattMsgEvent_t *pMsg)
{
  // Pointer to the GATT Msg data.
  uint8_t *packetData;

  // The variable that will keep track of the ANCS attribute
  // currently being processed.
  static uint8_t AttrID;

  // The variable will keep track of the current index the data
  // buffer will be written to if more than one packet is needed.
  static uint8_t  currentDataBufWriteIndex;

  // Point to the GATT Msg data
  packetData = pMsg->msg.handleValueNoti.pValue;

  // Check if this is the first retrieved packet for potentially
  // a set of packets to be sent by the ANCS Data Service.
  switch(notifAttrPktProcessState)
  {
    case NOTI_ATTR_FIRST_PKT:
    {
      // Initialize the data buffer write index to zero, as this is
      // the first packet.
      currentDataBufWriteIndex = 0;

      // Ensure Command ID is equal to zero as stated in the spec.
      if (*packetData != COMMAND_ID_GET_NOTIFICATION_ATTRIBUTES)
      {
        if(*packetData == COMMAND_ID_GET_APP_ATTRIBUTES)
        {
          Ancs_processAppAttr(pMsg);
        }
        return;
      }
      packetData++;

      // Copy the ANCS notification UID so it may be used by
      // to perform a positive or negative action is desired.
      VOID memcpy(currentNotifUID, packetData, ANCS_NOTIF_UID_LENGTH);

      packetData += ANCS_NOTIF_UID_LENGTH;

      // Store the ANCS attribute ID of the retrieved attribute.
      AttrID = *packetData;
      packetData++;

      // Store the 2-byte length of the data that is being retrieved.
      dataLen = BUILD_UINT16(*packetData, *(packetData + 1));

      // Check if the length is zero, if so the notification does not
      // have the specified attribute as stated in the ANCS spec.
      if ( dataLen == 0 )
      {
        if (AttrID == NOTIFICATION_ATTRIBUTE_ID_APP_IDENTIFIER)
          Display_printf(disp_handle, 0xFF, 0, "* AppID:\tNot available");
        else if (AttrID == NOTIFICATION_ATTRIBUTE_ID_TITLE)
          Display_printf(disp_handle, 0xFF, 0, "* Title:\tNot available");
        else if (AttrID == NOTIFICATION_ATTRIBUTE_ID_MESSAGE)
        {
          Display_printf(disp_handle, 0xFF, 0, "* Message:\tNot available");
        }
        pCurrentProcessingNode->notifData.currentState++;

        Ancs_processNotifications();

        return;
      }
      // Move the pointer to the data portion.
      packetData += 2;

      // Clear the data buffer in preparation for the new data.
      VOID memset(dataBuf, '\0', DATA_BUFFER_SIZE);

      // If the data length specified in the first ANCS Data Service notification
      // is greater than the number of bytes that was sent in the
      // first data packet(total GATT msg length - request metadata), the data will be split into multiple packets.
      if (dataLen > pMsg->msg.handleValueNoti.len - NOTIF_ATTR_REQUEST_METADATA_LENGTH)
      {
        // Copy the number of bytes that were sent in the
        // first packet to the data buffer, then set the
        // data buffer write index, and set the state from first packet
        // to continued packet.
        VOID memcpy(dataBuf, packetData, (pMsg->msg.handleValueNoti.len - NOTIF_ATTR_REQUEST_METADATA_LENGTH));
        currentDataBufWriteIndex = (pMsg->msg.handleValueNoti.len - NOTIF_ATTR_REQUEST_METADATA_LENGTH);
        notifAttrPktProcessState = NOTI_ATTR_CONTINUE_PKT;

        // Subtract the number of data bytes contained in the first packet
        // from the total number of expected data bytes.
        dataLen -= (pMsg->msg.handleValueNoti.len - NOTIF_ATTR_REQUEST_METADATA_LENGTH);
        return;
      }
      else
      {
        // In this case all the ANCS attribute data was contained in
        // the first packet so the data is copied, and both the index and
        // length are reset.
        VOID memcpy(dataBuf, packetData, dataLen);
        currentDataBufWriteIndex = 0;
        dataLen = 0x0000;
      }
    }
    break;

    // Check if the is a continued data packet.
    case NOTI_ATTR_CONTINUE_PKT:
    {
      if (dataLen > 0)
      {
        // Copy all the data from the notification packet to the data buffer
        // starting from the current data buffer write index.
        VOID memcpy(dataBuf + currentDataBufWriteIndex, pMsg->msg.handleValueNoti.pValue,
                                                   pMsg->msg.handleValueNoti.len);
        // Subtract the number of data bytes contained in the packet from
        // the total, and increase the data buffer write index by that amount.
        dataLen -= pMsg->msg.handleValueNoti.len;
        currentDataBufWriteIndex += pMsg->msg.handleValueNoti.len;

        // Checks if this is the last the continued packet.
        if (dataLen == 0x0000)
        {
          // If so reset the write index and the state.
          currentDataBufWriteIndex = 0;
          notifAttrPktProcessState = NOTI_ATTR_FIRST_PKT;
        }
      }
    }
    break;

    default:
    break;
  }

  if (dataLen == 0)
  {
    pCurrentProcessingNode->notifData.currentState++;
    if (AttrID == NOTIFICATION_ATTRIBUTE_ID_APP_IDENTIFIER) {
      strncpy(pCurrentProcessingNode->notifInfo.appID, (char *)dataBuf, sizeof(pCurrentProcessingNode->notifInfo.appID)-1);
      pCurrentProcessingNode->notifInfo.appID[sizeof(pCurrentProcessingNode->notifInfo.appID)-1] = '\0';
    }
    else if (AttrID == NOTIFICATION_ATTRIBUTE_ID_TITLE) {
      strncpy(pCurrentProcessingNode->notifInfo.title, (char *)dataBuf, sizeof(pCurrentProcessingNode->notifInfo.title)-1);
      pCurrentProcessingNode->notifInfo.title[sizeof(pCurrentProcessingNode->notifInfo.title)-1] = '\0';
    }
    else if (AttrID == NOTIFICATION_ATTRIBUTE_ID_MESSAGE) {
      strncpy(pCurrentProcessingNode->notifInfo.message, (char *)dataBuf, sizeof(pCurrentProcessingNode->notifInfo.message)-1);
      pCurrentProcessingNode->notifInfo.message[sizeof(pCurrentProcessingNode->notifInfo.message)-1] = '\0';
    }

    else if (AttrID == NOTIFICATION_ATTRIBUTE_ID_DATE)
      Ancs_printNotifDate(dataBuf);
  }
  // Check if the dataLen variable was overflowed if packets got mismatched.
  // This may occur if rapid connecting and disconnecting to the device is performed.
  else if(dataLen > DATA_BUFFER_SIZE - 1)
  {
    Display_printf(disp_handle, 0xFF,  0, "* AppID:\tDATA CORRUPTED");
    Display_printf(disp_handle, 0xFF,  0, "* App Name:\tDATA CORRUPTED");
    Display_printf(disp_handle, 0xFF,  0, "* Title:\tDATA CORRUPTED");
    Display_printf(disp_handle, 0xFF,  0, "* Message:\tDATA CORRUPTED");
    Display_printf(disp_handle, 0xFF, 0, "* Date:\t\tDATA CORRUPTED");
    Ancs_findAndRemoveFromQueue(currentNotifUID);
    notifAttrPktProcessState = NOTI_ATTR_FIRST_PKT;
  }
  // Continue processing the current notification.
  Ancs_processNotifications();

  return;
}

/*********************************************************************
 * @fn      Ancs_processNotifications
 *
 * @brief   Continue processing the current notification
 *
 * @param   none
 *
 * @return  none
 */
static void Ancs_processNotifications(void)
{
  // Only process if we have a current node being processed
  if (pCurrentProcessingNode == NULL)
  {
    // No current processing node, check if there are unprocessed notifications
    pCurrentProcessingNode = Ancs_findNextUnprocessedNode();
    if (pCurrentProcessingNode != NULL)
    {
      Ancs_processNotificationServiceNotif(pCurrentProcessingNode);
    }
    return;
  }

  // Check if current node is finished
  if (pCurrentProcessingNode->notifData.currentState == NOTI_ATTR_ID_END)
  {
    Display_printf(disp_handle, 0xFF, 0, "Notification processing complete");
    
    // Remove oldest notification (head) if queue is too big
    if (Ancs_queueSize() > MAX_NOTIF_QUEUE_SIZE)
    {
      // Remove the head (oldest notification)
      Ancs_popNotifFromQueue();
    }
    
    // Mark current processing as complete
    pCurrentProcessingNode = NULL;

    #if ENABLE_MENU
    menu_refresh();
    #endif
    // Look for next unprocessed notification
    pCurrentProcessingNode = Ancs_findNextUnprocessedNode();
    if (pCurrentProcessingNode != NULL)
    {
      Ancs_processNotificationServiceNotif(pCurrentProcessingNode);
    }
  }
  else
  {
    // Continue processing the same node
    Ancs_processNotificationServiceNotif(pCurrentProcessingNode);
  }

  return;
}

/*********************************************************************
 * @fn      Ancs_findNextUnprocessedNode
 *
 * @brief   Find the next node that hasn't been fully processed
 *
 * @param   none
 *
 * @return  notifQueueNode_t* - Next unprocessed node, or NULL if none found
 */
static notifQueueNode_t *Ancs_findNextUnprocessedNode(void)
{
  notifQueueNode_t *pSearch = pNotifQueueFront;
  
  // Find first node that's not fully processed
  while (pSearch != NULL)
  {
    if (pSearch->notifData.currentState != NOTI_ATTR_ID_END)
    {
      return pSearch; // Found an unprocessed node
    }
    pSearch = pSearch->pNext;
  }
  
  return NULL; // No unprocessed nodes found
}

/*********************************************************************
 * @fn      Ancs_processNotificationServiceNotif
 *
 * @brief   Process ANCS Notification Service notifications
 *
 * @param   pMsg - GATT message.
 *
 * @return  none
 */
static void Ancs_processNotificationServiceNotif(notifQueueNode_t *pNotif)
{

  // If the notification is in its initial state, display its category.
  if ( pCurrentProcessingNode->notifData.currentState == NOTI_ATTR_ID_BEGIN )
  {
    switch (pNotif->notifData.categoryID)
    {
    case CATEGORY_ID_OTHER:
      strncpy(pNotif->notifInfo.category, "Other", sizeof(pNotif->notifInfo.category)-1);
      pNotif->notifInfo.category[sizeof(pNotif->notifInfo.category)-1] = '\0';
      break;

    case CATEGORY_ID_INCOMING_CALL:
      strncpy(pNotif->notifInfo.category, "IncomingCall", sizeof(pNotif->notifInfo.category)-1);
      pNotif->notifInfo.category[sizeof(pNotif->notifInfo.category)-1] = '\0';
      haveIncomingCall = TRUE;
      memcpy(incomingCallUID, pNotif->notifData.notificationUID, ANCS_NOTIF_UID_LENGTH);
      break;

    case CATEGORY_ID_MISSED_CALL:
      strncpy(pNotif->notifInfo.category, "MissedCall", sizeof(pNotif->notifInfo.category)-1);
      pNotif->notifInfo.category[sizeof(pNotif->notifInfo.category)-1] = '\0';
      haveIncomingCall = FALSE;
      break;

    case CATEGORY_ID_VOICEMAIL:
      strncpy(pNotif->notifInfo.category, "Voicemail", sizeof(pNotif->notifInfo.category)-1);
      pNotif->notifInfo.category[sizeof(pNotif->notifInfo.category)-1] = '\0';
      break;

    case CATEGORY_ID_SOCIAL:
      strncpy(pNotif->notifInfo.category, "Social", sizeof(pNotif->notifInfo.category)-1);
      pNotif->notifInfo.category[sizeof(pNotif->notifInfo.category)-1] = '\0';
      break;

    case CATEGORY_ID_SCHEDULE:
      strncpy(pNotif->notifInfo.category, "Schedule", sizeof(pNotif->notifInfo.category)-1);
      pNotif->notifInfo.category[sizeof(pNotif->notifInfo.category)-1] = '\0';
      break;

    case CATEGORY_ID_EMAIL:
      strncpy(pNotif->notifInfo.category, "Email", sizeof(pNotif->notifInfo.category)-1);
      pNotif->notifInfo.category[sizeof(pNotif->notifInfo.category)-1] = '\0';
      break;

    case CATEGORY_ID_NEWS:
      strncpy(pNotif->notifInfo.category, "News", sizeof(pNotif->notifInfo.category)-1);
      pNotif->notifInfo.category[sizeof(pNotif->notifInfo.category)-1] = '\0';
      break;

    case CATEGORY_ID_HEALTH_AND_FITNESS:
      strncpy(pNotif->notifInfo.category, "Health And Fitness", sizeof(pNotif->notifInfo.category)-1);
      pNotif->notifInfo.category[sizeof(pNotif->notifInfo.category)-1] = '\0';
      break;

    case CATEGORY_ID_BUSINESS_AND_FINANCE:
      strncpy(pNotif->notifInfo.category, "Business And Finance", sizeof(pNotif->notifInfo.category)-1);
      pNotif->notifInfo.category[sizeof(pNotif->notifInfo.category)-1] = '\0';
      break;

    case CATEGORY_ID_LOCATION:
      strncpy(pNotif->notifInfo.category, "Location", sizeof(pNotif->notifInfo.category)-1);
      pNotif->notifInfo.category[sizeof(pNotif->notifInfo.category)-1] = '\0';
      break;

    case CATEGORY_ID_ENTERTAINMENT:
      strncpy(pNotif->notifInfo.category, "Entertainment", sizeof(pNotif->notifInfo.category)-1);
      pNotif->notifInfo.category[sizeof(pNotif->notifInfo.category)-1] = '\0';
      break;

    default:
      break;

    }
  }

  // Move to the attribute retrieval state machine.
  Ancs_handleNotifAttrRsp(pNotif->notifData.notificationUID);

  return;
}

/*********************************************************************
 * @fn      Ancs_performNegativeAction
 *
 * @brief   Performs a negative action on the notification with the passed UID
 *
 * @param   notifUID - A pointer to a four byte array that contains a notification's UID
 *
 * @return  status - Returns the status of the GATT write
 */

static uint8_t Ancs_performNegativeAction(uint8_t* notifUID)
{
  // This will store the return value.
  uint8_t status;

  // Declare the GATT write request.
  attWriteReq_t req;

  // Allocate the memory for the request.
  req.pValue = GATT_bm_alloc(Ancs_connHandle, ATT_WRITE_REQ, PERFORM_NOTIFICATION_ACTION_LENGTH, NULL);

  // If the allocation was not successful, set status to FAILURE.
  if (req.pValue == NULL)
    status = FAILURE;

  // If not, proceed with the GATT request.
  else
  {
    // Create a pointer to the request's data portion.
    uint8_t *requestPayload = req.pValue;

    // Set the handle to the Control Point's start handle stored in the handle cache.
    req.handle = Ancs_handleCache[ANCS_CTRL_POINT_HDL_START];

    // Set the write length of the GATT write.
    req.len = PERFORM_NOTIFICATION_ACTION_LENGTH;

    // Set the command ID to perform an action on the notification.
    *requestPayload = COMMAND_ID_PERFORM_NOTIFICATION_ACTION;
    requestPayload++;

    // Copy the ANCS notification UID to the request.
    VOID memcpy(requestPayload, notifUID, ANCS_NOTIF_UID_LENGTH);
    requestPayload += ANCS_NOTIF_UID_LENGTH;

    // Set the action type to negative.
    *requestPayload = ACTION_ID_NEGATIVE;

    // Signature and command must be set to zero.
    req.sig = 0;
    req.cmd = 0;

    status = GATT_WriteCharValue(Ancs_connHandle, &req, ICall_getEntityId());
    // If the GATT write is unsuccessful, free the allocated memory and set the status to FAILURE.
    if (status != SUCCESS)
      GATT_bm_free((gattMsg_t *) &req, ATT_WRITE_REQ);
  }

  return status;
}

/*********************************************************************
 * @fn      Ancs_performPositiveAction
 *
 * @brief   Performs a positive action on the notification with the passed UID
 *
 * @param   notifUID - A pointer to a four byte array that contains a notification's UID
 *
 * @return  status - Returns the status of the GATT write
 */

static uint8_t Ancs_performPositiveAction(uint8_t *notifUID)
{
  // This will store the return value.
  uint8_t status;

  // Declare the GATT write request.
  attWriteReq_t req;

  // Allocate the memory for the request.
  req.pValue = GATT_bm_alloc(Ancs_connHandle, ATT_WRITE_REQ, PERFORM_NOTIFICATION_ACTION_LENGTH, NULL);

  // If the allocation was not successful, set status to FAILURE.
  if (req.pValue == NULL)
    status = FAILURE;

  // If not, proceed with the GATT request.
  else
  {
    // Create a pointer to the request's data portion.
    uint8_t *requestPayload = req.pValue;

    // Set the handle to the Control Point's start handle stored in the handle cache.
    req.handle = Ancs_handleCache[ANCS_CTRL_POINT_HDL_START];

    // Set the write length of the GATT write.
    req.len = PERFORM_NOTIFICATION_ACTION_LENGTH;

    // Set the command ID to perform an action on the notification.
    *requestPayload = COMMAND_ID_PERFORM_NOTIFICATION_ACTION;
    requestPayload++;

    // Copy the ANCS notification UID to the request.
    VOID memcpy(requestPayload, notifUID, ANCS_NOTIF_UID_LENGTH);
    requestPayload += ANCS_NOTIF_UID_LENGTH;

    // Set the action type to positive.
    *requestPayload = ACTION_ID_POSITIVE;

    // Signature and command must be set to zero.
    req.sig = 0;
    req.cmd = 0;

    status = GATT_WriteCharValue(Ancs_connHandle, &req, ICall_getEntityId());
    // If the GATT write is unsuccessful, free the allocated memory and set the status to FAILURE.
    if (status != SUCCESS)
      GATT_bm_free((gattMsg_t *) &req, ATT_WRITE_REQ);
  }

  return status;
}

/*********************************************************************
 * @fn      Ancs_printNotifDate
 *
 * @brief   Processes the date data and prints it in a more user friendly format.
 *
 * @param   dataBuf - Pointer to the Data buffer the data is stored in.
 *
 * @return  none
 */
static void Ancs_printNotifDate(uint8_t *dataBuf)
{
  if(dataBuf[12] == '\0')
    return;

  char year[5]   = {'\0'};
  char month[3]  = {'\0'};
  char day[3]    = {'\0'};
  char hour[3]   = {'\0'};
  char minute[3] = {'\0'};
  char second[3] = {'\0'};

  memcpy(year,   dataBuf,      4);
  memcpy(month,  dataBuf + 4,  2);
  memcpy(day,    dataBuf + 6,  2);
  memcpy(hour,   dataBuf + 9,  2);
  memcpy(minute, dataBuf + 11, 2);
  memcpy(second, dataBuf + 13, 2);

  uint8_t num;
  char time[14] = {'\0'};
  time[2] = ':';
  memcpy(time + 3, minute, 2);
  time[5] = ':';
  memcpy(time + 6, second, 2);
  num = 10 * (hour[0] - '0') + (hour[1] - '0');

  if (num > 12)
  {
   num -= 12;
   memcpy(time + 8, " PM", 3);
  }
  else
   memcpy(time + 8, " AM", 3);

  if (num < 10)
  {
    time[0] = '0';
    time[1] = (char) (num + '0');
  }
  else
  {
    time[0] = '1';
    time[1] = (char) ((num % 10) + '0');
  }

  if(memcmp(month,"01", 2) == 0)
    snprintf(pCurrentProcessingNode->notifInfo.date, sizeof(pCurrentProcessingNode->notifInfo.date), "January %s, %s at %s", day, year, time);
  else if(memcmp(month,"02", 2) == 0)
    snprintf(pCurrentProcessingNode->notifInfo.date, sizeof(pCurrentProcessingNode->notifInfo.date), "February %s, %s at %s", day, year, time);
  else if(memcmp(month,"03", 2) == 0)
    snprintf(pCurrentProcessingNode->notifInfo.date, sizeof(pCurrentProcessingNode->notifInfo.date), "March %s, %s at %s", day, year, time);
  else if(memcmp(month,"04", 2) == 0)
    snprintf(pCurrentProcessingNode->notifInfo.date, sizeof(pCurrentProcessingNode->notifInfo.date), "April %s, %s at %s", day, year, time);
  else if(memcmp(month,"05", 2) == 0)
    snprintf(pCurrentProcessingNode->notifInfo.date, sizeof(pCurrentProcessingNode->notifInfo.date), "May %s, %s at %s", day, year, time);
  else if(memcmp(month,"06", 2) == 0)
    snprintf(pCurrentProcessingNode->notifInfo.date, sizeof(pCurrentProcessingNode->notifInfo.date), "June %s, %s at %s", day, year, time);
  else if(memcmp(month,"07", 2) == 0)
    snprintf(pCurrentProcessingNode->notifInfo.date, sizeof(pCurrentProcessingNode->notifInfo.date), "July %s, %s at %s", day, year, time);
  else if(memcmp(month,"08", 2) == 0)
    snprintf(pCurrentProcessingNode->notifInfo.date, sizeof(pCurrentProcessingNode->notifInfo.date), "August %s, %s at %s", day, year, time);
  else if(memcmp(month,"09", 2) == 0)
    snprintf(pCurrentProcessingNode->notifInfo.date, sizeof(pCurrentProcessingNode->notifInfo.date), "September %s, %s at %s", day, year, time);
  else if(memcmp(month,"10", 2) == 0)
    snprintf(pCurrentProcessingNode->notifInfo.date, sizeof(pCurrentProcessingNode->notifInfo.date), "October %s, %s at %s", day, year, time);
  else if(memcmp(month,"11", 2) == 0)
    snprintf(pCurrentProcessingNode->notifInfo.date, sizeof(pCurrentProcessingNode->notifInfo.date), "November %s, %s at %s", day, year, time);
  else if(memcmp(month,"12", 2) == 0)
    snprintf(pCurrentProcessingNode->notifInfo.date, sizeof(pCurrentProcessingNode->notifInfo.date), "December %s, %s at %s", day, year, time);

  else
    Display_printf(disp_handle, 0xFF, 0, "* Date:\t\t%s/%s/%s at %s",  month, day, year, time);

  
  return;
}

/*********************************************************************
 * @fn      Ancs_queueNewNotif
 *
 * @brief   Extract data from the GATT notification and push it to the queue.
 *
 * @param   pMsg - GATT message.
 *
 * @return  none
 */
void Ancs_queueNewNotif(gattMsgEvent_t *pMsg)
{

  uint8_t len = pMsg->msg.handleValueNoti.len;
  if (len != 8)
  {
    Display_printf(disp_handle, 0xFF, 0, "");
    Display_printf(disp_handle, 0xFF, 0, "Error evt len");
    return;
  }

  // Create pointer to GATT notification data.
  uint8_t *packetData = pMsg->msg.handleValueNoti.pValue;

  // Store the ANCS notification's eventID
  uint8_t eventID = packetData[0];

  // Store the ANCS notification's eventFlag
#ifdef IGNORE_PREEXISTING_NOTIFICATIONS
  uint8_t eventFlag = packetData[1];
#endif

  // Store the ANCS notification's categoryID
  uint8_t categoryID = packetData[2];

  // Notification UID from packetData[4] to packetData[7]
  uint8_t* pNotificationUID = packetData + ANCS_NOTIF_UID_LENGTH;


#ifdef IGNORE_PREEXISTING_NOTIFICATIONS
  if (eventFlag & EVENT_FLAG_PREEXISTING)
    return;
#endif

  if (eventID == EVENT_ID_NOTIFICATION_ADDED)
  {
    // If it is not in the search list, add it.
    Ancs_pushNotifToQueue(categoryID, pNotificationUID);
  }
  else if (eventID == EVENT_ID_NOTIFICATION_REMOVED)
  {
    if(memcmp(pNotificationUID, currentNotifUID, 4))
      Ancs_findAndRemoveFromQueue(pNotificationUID);
  }

  // Move to the attribute retrieval process.
  Ancs_processNotifications();

  return;
}

notifQueueNode_t *Ancs_getNotifQueueFront(void)
{
  return pNotifQueueFront;
}


/*********************************************************************
 *********************************************************************/