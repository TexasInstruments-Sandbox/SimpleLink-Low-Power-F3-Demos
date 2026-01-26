
/**
 * @file app_ams_parse.c
 * @brief Apple Media Service (AMS) BLE client parsing and control implementation for CC27xx.
 *
 * This file contains parsing logic, control functions, and attribute handling for interacting
 * with AMS on iOS devices over Bluetooth Low Energy.
 *
 * Features:
 * - Parses AMS remote command and entity update notifications
 * - Sends remote commands to control media playback
 * - Requests and processes player, queue, and track information
 * - Handles truncated attribute reads and additional data requests
 * - Updates local media info state for display and control
 *
 * Usage:
 * - Use AMS request functions to query player, queue, and track info after service discovery
 * - Call AMS parse functions from the GATT event handler to process incoming notifications and responses
 * - Use AMS remote command functions to control playback on the remote device
 
 =============================================================================================
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
 *****************************************************************************/


/*********************************************************************
 * INCLUDES
 */
#include <string.h>
#include "ti_drivers_config.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include <ti/display/Display.h>
#include <ti/display/DisplayUart2.h>
#include <app_main.h>
#include "app_ancs.h"


extern AMS_PlayerInfo gAmsPlayerInfo;
extern AMS_QueueInfo gAmsQueueInfo;
extern AMS_TrackInfo gAmsTrackInfo;
extern AMS_RemoteCommandSet gAmsRemoteCommands;

AMS_PlayerInfo gAmsPlayerInfo = {0};
AMS_QueueInfo gAmsQueueInfo = {0};
AMS_TrackInfo gAmsTrackInfo = {0};
AMS_RemoteCommandSet gAmsRemoteCommands = {0};

extern Display_Handle disp_handle;

static uint8_t readEntityID;
static uint8_t readEntityAttributeId;


/**
 * @brief Parse and display available AMS remote commands from a notification.
 * @param pMsg Pointer to GATT message event containing remote command data.
 */
void ams_parseRemoteUpdates(gattMsgEvent_t *pMsg)
{

    // Reset all command flags to 0
    memset(&gAmsRemoteCommands, 0, sizeof(gAmsRemoteCommands));
    uint8_t *packetData;
    uint16_t packetLen;

    // Point to the GATT Msg data
    packetData = pMsg->msg.handleValueNoti.pValue;
    packetLen = pMsg->msg.handleValueNoti.len;

    for(uint8_t i = 0; i < packetLen; i++)
    {
        uint8_t remoteCommandID = packetData[i];
        switch(remoteCommandID) {
            case 0: gAmsRemoteCommands.play = 1; break;
            case 1: gAmsRemoteCommands.pause = 1; break;
            case 2: gAmsRemoteCommands.toggle_play_pause = 1; break;
            case 3: gAmsRemoteCommands.next_track = 1; break;
            case 4: gAmsRemoteCommands.previous_track = 1; break;
            case 5: gAmsRemoteCommands.volume_up = 1; break;
            case 6: gAmsRemoteCommands.volume_down = 1; break;
            case 7: gAmsRemoteCommands.advance_repeat_mode = 1; break;
            case 8: gAmsRemoteCommands.advance_shuffle_mode = 1; break;
            case 9: gAmsRemoteCommands.skip_forward = 1; break;
            case 10: gAmsRemoteCommands.skip_backward = 1; break;
            case 11: gAmsRemoteCommands.like_track = 1; break;
            case 12: gAmsRemoteCommands.dislike_track = 1; break;
            case 13: gAmsRemoteCommands.bookmark_track = 1; break;
            default: break;
        }
    }
}
/**
 * @brief Send an AMS remote command to control media playback on the remote device.
 * @param remoteCommandID Command ID to send (see AMS remote command ID values).
 */
void ams_sendRemoteCommands(uint8_t remoteCommandID)
{
    uint8_t status;
    uint8_t cmdLen = 1;

    // Do a write
    attWriteReq_t req;

    req.pValue = GATT_bm_alloc(Ancs_connHandle, ATT_WRITE_REQ, cmdLen, NULL);
    uint8_t *requestPayload = req.pValue;
    if (req.pValue != NULL)
    {
        // Get the AMS entity update handle
        req.handle = ams_handleCache[AMS_REMOTE_COM_HDL_START];

        // Set command length.
        req.len = cmdLen;

        // add remote command ID to the payload
        *requestPayload = remoteCommandID;
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

}
/**
 * @brief Request AMS player information (name, playback info, volume) from the remote device.
 */
void ams_requestPlayerInfo()
{
    uint8_t status;
    uint8_t cmdLen = 4;

    // Do a write
    attWriteReq_t req;

    req.pValue = GATT_bm_alloc(Ancs_connHandle, ATT_WRITE_REQ, cmdLen, NULL);
    uint8_t *requestPayload = req.pValue;
    if (req.pValue != NULL)
    {
        // Get the AMS entity update handle
        req.handle = ams_handleCache[AMS_ENT_UPDT_HDL_START];

        // Set command length.
        req.len = cmdLen;

        // Set Entity ID.
        *requestPayload = ENTITY_ID_PLAYER;
        requestPayload++;

        //set player name ID attribute
        *requestPayload = PLAYER_ATTRIBUTE_ID_NAME;
        requestPayload++;

        //set playback info ID attribute
        *requestPayload = PLAYER_ATTRIBUTE_ID_PLAYBACK_INFO;
        requestPayload++;

        //set volume ID attribute
        *requestPayload = PLAYER_ATTRIBUTE_ID_VOLUME;
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
}

/**
 * @brief Request AMS queue information (index, count, shuffle, repeat) from the remote device.
 */
void ams_requestQueueInfo()
{
    uint8_t status;
    uint8_t cmdLen = 5;

    // Do a write
    attWriteReq_t req;

    req.pValue = GATT_bm_alloc(Ancs_connHandle, ATT_WRITE_REQ, cmdLen, NULL);
    uint8_t *requestPayload = req.pValue;
    if (req.pValue != NULL)
    {
        // Get the AMS entity update handle
        req.handle = ams_handleCache[AMS_ENT_UPDT_HDL_START];

        // Set command length.
        req.len = cmdLen;

        // Set Entity ID.
        *requestPayload = ENTITY_ID_QUEUE;
        requestPayload++;

        //set track ID attribute
        *requestPayload = QUEUE_ATTRIBUTE_ID_INDEX;
        requestPayload++;

        //set album ID attribute
        *requestPayload = QUEUE_ATTRIBUTE_ID_COUNT;
        requestPayload++;

        //set duration ID attribute
        *requestPayload = QUEUE_ATTRIBUTE_ID_SHUFFLE_MODE;
        requestPayload++;

        //set track title ID attribute
        *requestPayload = QUEUE_ATTRIBUTE_ID_REPEAT_MODE;
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
}

/**
 * @brief Request AMS track information (artist, album, duration, title) from the remote device.
 */
void ams_requestTrackInfo()
{
    uint8_t status;
    uint8_t cmdLen = 5;

    // Do a write
    attWriteReq_t req;

    req.pValue = GATT_bm_alloc(Ancs_connHandle, ATT_WRITE_REQ, cmdLen, NULL);
    uint8_t *requestPayload = req.pValue;
    if (req.pValue != NULL)
    {
        // Get the AMS entity update handle
        req.handle = ams_handleCache[AMS_ENT_UPDT_HDL_START];

        // Set command length.
        req.len = cmdLen;

        // Set Entity ID.
        *requestPayload = ENTITY_ID_TRACK;
        requestPayload++;

        //set track ID attribute
        *requestPayload = TRACK_ATTR_ID_ARTIST;
        requestPayload++;

        //set album ID attribute
        *requestPayload = TRACK_ATTR_ID_ALBUM;
        requestPayload++;

        //set duration ID attribute
        *requestPayload = TRACK_ATTR_ID_DURATION;
        requestPayload++;

        //set track title ID attribute
        *requestPayload = TRACK_ATTR_ID_TITLE;
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
}
/**
 * @brief Parse AMS entity update notifications and update local media info state.
 * @param pMsg Pointer to GATT message event containing entity update data.
 */
void ams_parseEntityUpdateNotifications(gattMsgEvent_t *pMsg)
{
    // Pointer to the GATT Msg data.
    uint8_t *packetData;
    uint16_t packetLen;

    char valueBuf[100];

    // Point to the GATT Msg data
    packetData = pMsg->msg.handleValueNoti.pValue;
    packetLen = pMsg->msg.handleValueNoti.len - 3; //taking off 3 bytes for entity ID, attribute ID, entity update flags
    memcpy(valueBuf, (packetData +3), packetLen);
    valueBuf[packetLen] = '\0';

    //check entity ID
    if(packetData[0] == ENTITY_ID_PLAYER)
    {
        //PlayerAttributeIDName localized name of the app
        if(packetData[1] == PLAYER_ATTRIBUTE_ID_NAME)
        {
            strncpy(gAmsPlayerInfo.playerName, valueBuf, MAX_STRING_LEN - 1);
            gAmsPlayerInfo.playerName[MAX_STRING_LEN - 1] = '\0';
        }
        else if(packetData[1] == PLAYER_ATTRIBUTE_ID_PLAYBACK_INFO)
        {
            //Playback info is in a comma separated string
            char *playbackState = strtok(valueBuf, ",");
            char *playbackRate = strtok(NULL, ",");
            char *elapsedTime = strtok(NULL, ",");

            strncpy(gAmsPlayerInfo.playbackState, playbackState, MAX_STRING_LEN - 1);
            gAmsPlayerInfo.playbackState[MAX_STRING_LEN - 1] = '\0';
            strncpy(gAmsPlayerInfo.playbackRate, playbackRate, MAX_STRING_LEN - 1);
            gAmsPlayerInfo.playbackRate[MAX_STRING_LEN - 1] = '\0';
            strncpy(gAmsPlayerInfo.elapsedTime, elapsedTime, MAX_STRING_LEN - 1);
            gAmsPlayerInfo.elapsedTime[MAX_STRING_LEN - 1] = '\0';
            
        }
        else if(packetData[1] == PLAYER_ATTRIBUTE_ID_VOLUME)
        {
            strncpy(gAmsPlayerInfo.volume, valueBuf, MAX_STRING_LEN - 1);
            gAmsPlayerInfo.volume[MAX_STRING_LEN - 1] = '\0';
        }

        /*
        NOTE:
        Using the following formula at any time, the MR can calculate from the Playback Rate and Elapsed Time how much time has elapsed during the playback of the track:
        CurrentElapsedTime = ElapsedTime + ((TimeNow – TimePlaybackInfoWasReceived) * PlaybackRate)
        This lets MRs implement and display a UI scrubber without needing to poll the MS for continuous updates.
        */
        #if ENABLE_MENU
        menu_refresh();
        #endif
    }
    else if(packetData[0] == ENTITY_ID_QUEUE)
    {
        //QueueAttributeIDIndex
        if(packetData[1] == QUEUE_ATTRIBUTE_ID_INDEX)
        {
            strncpy(gAmsQueueInfo.queueIndex, valueBuf, MAX_STRING_LEN - 1);
            gAmsQueueInfo.queueIndex[MAX_STRING_LEN - 1] = '\0';
        }
        else if(packetData[1] == QUEUE_ATTRIBUTE_ID_COUNT)
        {
            strncpy(gAmsQueueInfo.queueCount, valueBuf, MAX_STRING_LEN - 1);
            gAmsQueueInfo.queueCount[MAX_STRING_LEN - 1] = '\0';
        }
        else if(packetData[1] == QUEUE_ATTRIBUTE_ID_SHUFFLE_MODE)
        {
            if(!strcmp(valueBuf, "0"))
            {
                strncpy(gAmsQueueInfo.shuffleMode, "off", MAX_STRING_LEN - 1);
                gAmsQueueInfo.shuffleMode[MAX_STRING_LEN - 1] = '\0';
            }
            else if(!strcmp(valueBuf, "1"))
            {
                strncpy(gAmsQueueInfo.shuffleMode, "one", MAX_STRING_LEN - 1);
                gAmsQueueInfo.shuffleMode[MAX_STRING_LEN - 1] = '\0';
            }
            else if(!strcmp(valueBuf, "2"))
            {
                strncpy(gAmsQueueInfo.shuffleMode, "all", MAX_STRING_LEN - 1);
                gAmsQueueInfo.shuffleMode[MAX_STRING_LEN - 1] = '\0';
            }
        }
        else if(packetData[1] == QUEUE_ATTRIBUTE_ID_REPEAT_MODE)
        {
            if(!strcmp(valueBuf, "0"))
            {
                strncpy(gAmsQueueInfo.repeatMode, "off", MAX_STRING_LEN - 1);
                gAmsQueueInfo.repeatMode[MAX_STRING_LEN - 1] = '\0';
            }
            else if(!strcmp(valueBuf, "1"))
            {
                strncpy(gAmsQueueInfo.repeatMode, "one", MAX_STRING_LEN - 1);
                gAmsQueueInfo.repeatMode[MAX_STRING_LEN - 1] = '\0';
            }
            else if(!strcmp(valueBuf, "2"))
            {
                strncpy(gAmsQueueInfo.repeatMode, "all", MAX_STRING_LEN - 1);
                gAmsQueueInfo.repeatMode[MAX_STRING_LEN - 1] = '\0';
            }
        }
        #if ENABLE_MENU
        menu_refresh();
        #endif
    }
    else if(packetData[0] == ENTITY_ID_TRACK)
    {
        if(packetData[1] == TRACK_ATTR_ID_ARTIST)
        {
            strncpy(gAmsTrackInfo.artist, valueBuf, MAX_STRING_LEN - 1);
            gAmsTrackInfo.artist[MAX_STRING_LEN - 1] = '\0';
        }
        else if(packetData[1] == TRACK_ATTR_ID_TITLE)
        {
            strncpy(gAmsTrackInfo.title, valueBuf, MAX_STRING_LEN - 1);
            gAmsTrackInfo.title[MAX_STRING_LEN - 1] = '\0';
        }
        else if(packetData[1] == TRACK_ATTR_ID_ALBUM)
        {
            strncpy(gAmsTrackInfo.album, valueBuf, MAX_STRING_LEN - 1);
            gAmsTrackInfo.album[MAX_STRING_LEN - 1] = '\0';
        }
        else if(packetData[1] == TRACK_ATTR_ID_DURATION)
        {
            strncpy(gAmsTrackInfo.duration, valueBuf, MAX_STRING_LEN - 1);
            gAmsTrackInfo.duration[MAX_STRING_LEN - 1] = '\0';
        }
        #if ENABLE_MENU
        menu_refresh();
        #endif
    }

    //check to see if the entity update flag indicates truncated data
    //if so request more data
    //This is optional
    if(packetData[2] == ENTITY_UPDATE_FLAG_TRUNCATED)
    {
        readEntityID = packetData[0];
        readEntityAttributeId = packetData[1];
    }
}
/**
 * @brief Send a request for additional/truncated AMS entity attribute data.
 */
void ams_sendEntityAttributeRequests()
{
    uint8_t status;
    uint8_t cmdLen = 2;

    // Do a write
    attWriteReq_t req;

    req.pValue = GATT_bm_alloc(Ancs_connHandle, ATT_WRITE_REQ, cmdLen, NULL);
    uint8_t *requestPayload = req.pValue;
    if (req.pValue != NULL)
    {
        // Get the AMS entity update handle
        req.handle = ams_handleCache[AMS_ENT_ATTR_HDL_START];

        // Set command length.
        req.len = cmdLen;

        // Set Entity ID.
        *requestPayload = readEntityID;
        requestPayload++;

        //set attribute ID
        *requestPayload = readEntityAttributeId;
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
}

/**
 * @brief Initiate a read request for AMS entity attribute data from the remote device.
 */
void ams_entityAttributeReadRequest()
{
    uint8_t status;
    // Do a read
    attReadReq_t req = {0};

    // Get the AMS entity update handle
    req.handle = ams_handleCache[AMS_ENT_ATTR_HDL_START];

    // Execute the read.
    status = GATT_ReadCharValue(Ancs_connHandle, &req, ICall_getEntityId());
    if (status != SUCCESS)
    {
        Display_printf(disp_handle, 0xFF, 0, "ENTITY READ REQUEST ERROR:\t%d",status);
    }
    
}
/**
 * @brief Parse AMS entity attribute read response and display the result.
 * @param pMsg Pointer to GATT message event containing the read response.
 */
void ams_parseEntityAttributeResponses(gattMsgEvent_t *pMsg)
{
    //get the response value
    attReadRsp_t rsp = (attReadRsp_t)pMsg->msg.readRsp;
    char *val = (char *)rsp.pValue;

    if(readEntityID == ENTITY_ID_PLAYER) {
        if(readEntityAttributeId == PLAYER_ATTRIBUTE_ID_NAME) {
            strncpy(gAmsPlayerInfo.playerName, val, MAX_STRING_LEN-1);
            gAmsPlayerInfo.playerName[MAX_STRING_LEN-1] = '\0';
        } else if(readEntityAttributeId == PLAYER_ATTRIBUTE_ID_PLAYBACK_INFO) {
            // Optionally parse if needed
            strncpy(gAmsPlayerInfo.playbackState, val, MAX_STRING_LEN-1);
            gAmsPlayerInfo.playbackState[MAX_STRING_LEN-1] = '\0';
        } else if(readEntityAttributeId == PLAYER_ATTRIBUTE_ID_VOLUME) {
            strncpy(gAmsPlayerInfo.volume, val, MAX_STRING_LEN-1);
            gAmsPlayerInfo.volume[MAX_STRING_LEN-1] = '\0';
        }
    } else if(readEntityID == ENTITY_ID_QUEUE) {
        if(readEntityAttributeId == QUEUE_ATTRIBUTE_ID_INDEX) {
            strncpy(gAmsQueueInfo.queueIndex, val, MAX_STRING_LEN-1);
            gAmsQueueInfo.queueIndex[MAX_STRING_LEN-1] = '\0';
        } else if(readEntityAttributeId == QUEUE_ATTRIBUTE_ID_COUNT) {
            strncpy(gAmsQueueInfo.queueCount, val, MAX_STRING_LEN-1);
            gAmsQueueInfo.queueCount[MAX_STRING_LEN-1] = '\0';
        } else if(readEntityAttributeId == QUEUE_ATTRIBUTE_ID_SHUFFLE_MODE) {
            strncpy(gAmsQueueInfo.shuffleMode, val, MAX_STRING_LEN-1);
            gAmsQueueInfo.shuffleMode[MAX_STRING_LEN-1] = '\0';
        } else if(readEntityAttributeId == QUEUE_ATTRIBUTE_ID_REPEAT_MODE) {
            strncpy(gAmsQueueInfo.repeatMode, val, MAX_STRING_LEN-1);
            gAmsQueueInfo.repeatMode[MAX_STRING_LEN-1] = '\0';
        }
    } else if(readEntityID == ENTITY_ID_TRACK) {
        if(readEntityAttributeId == TRACK_ATTR_ID_ARTIST) {
            strncpy(gAmsTrackInfo.artist, val, MAX_STRING_LEN-1);
            gAmsTrackInfo.artist[MAX_STRING_LEN-1] = '\0';
        } else if(readEntityAttributeId == TRACK_ATTR_ID_TITLE) {
            strncpy(gAmsTrackInfo.title, val, MAX_STRING_LEN-1);
            gAmsTrackInfo.title[MAX_STRING_LEN-1] = '\0';
        } else if(readEntityAttributeId == TRACK_ATTR_ID_ALBUM) {
            strncpy(gAmsTrackInfo.album, val, MAX_STRING_LEN-1);
            gAmsTrackInfo.album[MAX_STRING_LEN-1] = '\0';
        } else if(readEntityAttributeId == TRACK_ATTR_ID_DURATION) {
            strncpy(gAmsTrackInfo.duration, val, MAX_STRING_LEN-1);
            gAmsTrackInfo.duration[MAX_STRING_LEN-1] = '\0';
        }
    } else {
        // Unknown entity, just print
        Display_printf(disp_handle, 0xFF, 0, "truncated value: %s", val);
    }

}