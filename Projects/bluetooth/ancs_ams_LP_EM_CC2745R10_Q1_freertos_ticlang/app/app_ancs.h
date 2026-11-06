
/******************************************************************************

 @file       app_ancs.h

 @brief This file contains the ANCS and AMS Application sample interface for use
   with the CC27xx Bluetooth Low Energy Protocol Stack.

 Group: CMCU, SCS
 Target Device: CC27xx

 Copyright (c) 2017-2017, Texas Instruments Incorporated
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
 *****************************************************************************/

#ifndef ANCS_H
#define ANCS_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * CONSTANTS
 */

// ANCS handle cache indices.
enum
{
  ANCS_NOTIF_SCR_HDL_START,         // ANCS Notification Source characteristic start handle.
  ANCS_NOTIF_SCR_HDL_END,           // ANCS Notification Source characteristic end handle.
  ANCS_NOTIF_SCR_HDL_CCCD,          // ANCS Notification Source CCCD handle.

  ANCS_CTRL_POINT_HDL_START,        // ANCS Control Point characteristic start handle.
  ANCS_CTRL_POINT_HDL_END,          // ANCS Control Point characteristic end handle.

  ANCS_DATA_SRC_HDL_START,          // ANCS Data Source characteristic start handle.
  ANCS_DATA_SRC_HDL_END,            // ANCS Data Source characteristic end handle.
  ANCS_DATA_SRC_HDL_CCCD,           // ANCS Data Source CCCD handle.
};

enum
{
  AMS_REMOTE_COM_HDL_START,         // ANCS Notification Source characteristic start handle.
  AMS_REMOTE_COM_HDL_END,           // ANCS Notification Source characteristic end handle.
  AMS_REMOTE_COM_HDL_CCCD,          // ANCS Notification Source CCCD handle.

  AMS_ENT_UPDT_HDL_START,        // ANCS Control Point characteristic start handle.
  AMS_ENT_UPDT_HDL_END,          // ANCS Control Point characteristic end handle.
  AMS_ENT_UPDT_HDL_CCCD,

  AMS_ENT_ATTR_HDL_START,          // ANCS Data Source characteristic start handle.
  AMS_ENT_ATTR_HDL_END,            // ANCS Data Source characteristic end handle.
};

// Cache array length.
#define HDL_CACHE_LEN            8

// States for the notification attribute retrieval state machine (Ancs_handleNotifAttrRsp()).
enum
{
  NOTI_ATTR_ID_BEGIN,                 // ANCS notification attribute initial retrieval state.
  NOTI_ATTR_ID_APPID,                 // ANCS notification attribute AppID retrieval state.
  APP_ATTR_ID_DN,                     // ANCS application attribute display name retrieval state.
  NOTI_ATTR_ID_TITLE,                 // ANCS notification attribute Title retrieval state.
  NOTI_ATTR_ID_SUBTITLE,              // ANCS notification attribute Subtitle retrieval state.
  NOTI_ATTR_ID_MESSAGE,               // ANCS notification attribute Message retrieval state.
  NOTI_ATTR_ID_MESSAGE_SIZE,          // ANCS notification attribute Message Size retrieval state.
  NOTI_ATTR_ID_DATE,                  // ANCS notification attribute Date retrieval state.
  NOTI_ATTR_ID_END                    // ANCS notification attribute final retrieval state.
};

// States for processing Data Source packets.
enum
{
  NOTI_ATTR_FIRST_PKT,                // Initial retrieved Data Source packet processing state.
  NOTI_ATTR_CONTINUE_PKT,             // Post-Initial retrieved Data Source packet processing state.
};


/*********************************************************************
 * MACROS
 */

//Ignore preexisting notifications when re/connecting to device
/*--------------------------NOTE------------------------------------------
// If not defined, all notifications including preexisting ones will be processed.
// this can lead to overloading the system and crashes if there are too many preexisting notifications.
// it is recommended to define this macro to ignore preexisting notifications.
*/
#define IGNORE_PREEXISTING_NOTIFICATIONS 1

// Number of bytes required to store an ANCS notification UID
#define ANCS_NOTIF_UID_LENGTH                                 4

// CommandID Values
#define COMMAND_ID_GET_NOTIFICATION_ATTRIBUTES                0x00
#define COMMAND_ID_GET_APP_ATTRIBUTES                         0x01
#define COMMAND_ID_PERFORM_NOTIFICATION_ACTION                0x02

#define ACTION_ID_POSITIVE                                    0
#define ACTION_ID_NEGATIVE                                    1

// Notification AttributeID Values
#define NOTIFICATION_ATTRIBUTE_ID_APP_IDENTIFIER              0
#define NOTIFICATION_ATTRIBUTE_ID_TITLE                       1
#define NOTIFICATION_ATTRIBUTE_ID_SUBTITLE                    2
#define NOTIFICATION_ATTRIBUTE_ID_MESSAGE                     3
#define NOTIFICATION_ATTRIBUTE_ID_MESSAGE_SIZE                4
#define NOTIFICATION_ATTRIBUTE_ID_DATE                        5
#define NOTIFICATION_ATTRIBUTE_ID_POSITIVE_ACTION_LABEL       6
#define NOTIFICATION_ATTRIBUTE_ID_NEGATIVE_ACTION_LABEL       7

// EventID Values
#define EVENT_ID_NOTIFICATION_ADDED                           0
#define EVENT_ID_NOTIFICATION_MODIFIED                        1
#define EVENT_ID_NOTIFICATION_REMOVED                         2

// EventFlags
#define EVENT_FLAG_SILENT                                     0x01
#define EVENT_FLAG_IMPORTANT                                  0x02
#define EVENT_FLAG_PREEXISTING                                0x04
#define EVENT_FLAG_POSITIVE_ACTION                            0x08
#define EVENT_FLAG_NEGATIVE_ACTION                            0x10

// CategoryID Values
#define CATEGORY_ID_OTHER                                     0
#define CATEGORY_ID_INCOMING_CALL                             1
#define CATEGORY_ID_MISSED_CALL                               2
#define CATEGORY_ID_VOICEMAIL                                 3
#define CATEGORY_ID_SOCIAL                                    4
#define CATEGORY_ID_SCHEDULE                                  5
#define CATEGORY_ID_EMAIL                                     6
#define CATEGORY_ID_NEWS                                      7
#define CATEGORY_ID_HEALTH_AND_FITNESS                        8
#define CATEGORY_ID_BUSINESS_AND_FINANCE                      9
#define CATEGORY_ID_LOCATION                                  10
#define CATEGORY_ID_ENTERTAINMENT                             11


// Define ANCS Client Flags
#define CLIENT_NONE                                           0x00
#define CLIENT_IMPORTANT_ALERT                                0x01
#define CLIENT_POSITIVE_ACT                                   0x02
#define CLIENT_NEG_ACT                                        0x04

// Error Codes received from Control Point
#define UNKNOWN_COMMAND                                       0xA0
#define INVALID_COMMAND                                       0xA1
#define INVALID_PARAMETER                                     0xA2
#define ACTION_FAILED                                         0xA3

// AppAttributeID values
#define APP_ATTRIBUTE_ID_DISPLAY_NAME                         0x00

// ANCS Control Point action length.
#define PERFORM_NOTIFICATION_ACTION_LENGTH                    6

//AMS remote command ID values
#define REMOTE_CMDID_PLAY 0
#define REMOTE_CMDID_PAUSE 1
#define REMOTE_CMDID_TOGGLE_PLAY_PAUSE 2
#define REMOTE_CMDID_NEXT_TRACK 3
#define REMOTE_CMDID_PREVIOUS_TRACK 4
#define REMOTE_CMDID_VOLUME_UP 5
#define REMOTE_CMDID_VOLUME_DOWN 6
#define REMOTE_CMDID_ADVANCE_REPEAT_MODE 7
#define REMOTE_CMDID_ADVANCE_SHUFFLE_MODE 8
#define REMOTE_CMDID_SKIP_FORWARD 9
#define REMOTE_CMDID_SKIP_BACKWARD 10
#define REMOTE_CMDID_LIKE_TRACK 11
#define REMOTE_CMDID_DISLIKE_TRACK 12
#define REMOTE_CMDID_BOOKMARK_TRACK 13

// Entity ID values
#define ENTITY_ID_PLAYER 0
#define ENTITY_ID_QUEUE 1
#define ENTITY_ID_TRACK 2

// Entity update truncated flag
#define ENTITY_UPDATE_FLAG_TRUNCATED  (1 << 0)

// Player attribute ID values
#define PLAYER_ATTRIBUTE_ID_NAME 0
#define PLAYER_ATTRIBUTE_ID_PLAYBACK_INFO 1
#define PLAYER_ATTRIBUTE_ID_VOLUME 2

// Queue attribute ID values
#define QUEUE_ATTRIBUTE_ID_INDEX 0
#define QUEUE_ATTRIBUTE_ID_COUNT 1
#define QUEUE_ATTRIBUTE_ID_SHUFFLE_MODE 2
#define QUEUE_ATTRIBUTE_ID_REPEAT_MODE 3

// shuffle mode values
#define SHUFFLE_MODE_OFF 0
#define SHUFFLE_MODE_ONE 1
#define SHUFFLE_MODE_ALL 2

// repeat mode values
#define REPEAT_MODE_OFF 0
#define REPEAT_MODE_ONE 1
#define REPEAT_MODE_ALL 2

// track attribute ID values
#define TRACK_ATTR_ID_ARTIST 0
#define TRACK_ATTR_ID_ALBUM 1
#define TRACK_ATTR_ID_TITLE 2
#define TRACK_ATTR_ID_DURATION 3

#define CCCD_UUID           0x2902
// ANCS: 7905F431-B5CE-4E99-A40F-4B1E122D00D0
#define ANCSAPP_ANCS_SVC_UUID 0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4, 0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79
// Notification Source: UUID 9FBF120D-6301-42D9-8C58-25E699A21DBD (notifiable)
#define ANCSAPP_NOTIF_SRC_CHAR_UUID                   0x1DBD
// Control point: UUID 69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9 (writable with response)
#define ANCSAPP_CTRL_PT_CHAR_UUID                     0xD9D9
// Data Source: UUID 22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB (notifiable)
#define ANCSAPP_DATA_SRC_CHAR_UUID                    0x7BFB

// AMS: 89D3502B-0F36-433A-8EF4-C502AD55F8DC
#define AMSAPP_AMS_SVC_UUID 0xDC, 0xF8, 0x55, 0xAD, 0x02, 0xC5, 0xF4, 0x8E, 0x3A, 0x43, 0x36, 0x0F, 0x2B, 0x50, 0xD3, 0x89
// Remote Command: UUID 9B3C81D8-57B1-4A8A-B8DF-0E56F7CA51C2
#define AMSAPP_REMOTE_COM_CHAR_UUID 0x51C2
// Entity Update: UUID 2F7CABCE-808D-411F-9A0C-BB92BA96C102
#define AMSAPP_ENTITY_UPDATE_CHAR_UUID 0xC102
// Entity Attribute: UUID C6B2F38C-23AB-46D8-A6AB-A3A870BBD5D7
#define AMSAPP_ENTITY_ATTR_CHAR_UUID 0xD5D7

// Enable menu display
#define ENABLE_MENU 1

/*********************************************************************
 * GLOBAL
 */
// The iPhones Connection handle.
extern uint16_t Ancs_connHandle;

// The ANCS handle cache.
extern uint16_t Ancs_handleCache[HDL_CACHE_LEN];
extern uint16_t ams_handleCache[HDL_CACHE_LEN];


#define MAX_STRING_LEN 64

// Struct to represent AMS remote command availability
typedef struct {
    uint8_t play;
    uint8_t pause;
    uint8_t toggle_play_pause;
    uint8_t next_track;
    uint8_t previous_track;
    uint8_t volume_up;
    uint8_t volume_down;
    uint8_t advance_repeat_mode;
    uint8_t advance_shuffle_mode;
    uint8_t skip_forward;
    uint8_t skip_backward;
    uint8_t like_track;
    uint8_t dislike_track;
    uint8_t bookmark_track;
} AMS_RemoteCommandSet;

typedef struct {
    char playerName[MAX_STRING_LEN];
    char playbackState[MAX_STRING_LEN];
    char playbackRate[MAX_STRING_LEN];
    char elapsedTime[MAX_STRING_LEN];
    char volume[MAX_STRING_LEN];
} AMS_PlayerInfo;

typedef struct {
    char queueIndex[MAX_STRING_LEN];
    char queueCount[MAX_STRING_LEN];
    char shuffleMode[MAX_STRING_LEN];
    char repeatMode[MAX_STRING_LEN];
} AMS_QueueInfo;

typedef struct {
    char artist[MAX_STRING_LEN];
    char title[MAX_STRING_LEN];
    char album[MAX_STRING_LEN];
    char duration[MAX_STRING_LEN];
} AMS_TrackInfo;

extern AMS_PlayerInfo gAmsPlayerInfo;
extern AMS_QueueInfo gAmsQueueInfo;
extern AMS_TrackInfo gAmsTrackInfo;
extern AMS_RemoteCommandSet gAmsRemoteCommands;


typedef struct
{
  uint8_t categoryID;
  uint8_t notificationUID[4];
  uint8_t currentState;
  uint8_t requestedAttrs;
} notifQueueData_t;

typedef struct
{
  char category[24];
  char appID[32];
  char appName[32];
  char title[64];
  char message[128];
  char date[40];
} notifQueueInfo_t;

typedef struct notifQueueNode_t
{
  notifQueueData_t notifData;
  notifQueueInfo_t notifInfo;
  struct notifQueueNode_t *pNext;
} notifQueueNode_t;

// Stores Data Service notification processing state.
extern uint8_t notifAttrPktProcessState;

// Stores Data Service app attribute processing state
extern uint8_t appAttrPktProcessState;
/*********************************************************************
 * FUNCTIONS
 */

// ANCS service discovery functions.
extern uint8_t Ancs_subsNotifSrc(void);
extern uint8_t Ancs_subsDataSrc(void);

// ANCS notification handling functions.
extern void Ancs_processDataServiceNotif(gattMsgEvent_t *pMsg);
extern void Ancs_queueNewNotif(gattMsgEvent_t *pMsg);
extern void Ancs_popAllNotifsFromQueue(void);
extern void Ancs_acceptIncomingCall(void);
extern void Ancs_declineIncomingCall(void);
extern notifQueueNode_t *Ancs_getNotifQueueFront(void);

/**
 * @brief Parse incoming AMS Remote Command notifications.
 * @param pMsg Pointer to GATT message event.
 */
extern void ams_parseRemoteUpdates(gattMsgEvent_t *pMsg);

/**
 * @brief Send AMS Remote Command to control media playback.
 * @param remoteCommandID Command ID to send (see AMS remote command ID values).
 */
extern void ams_sendRemoteCommands(uint8_t remoteCommandID);

/**
 * @brief Parse AMS Entity Update notifications.
 * @param pMsg Pointer to GATT message event.
 */
extern void ams_parseEntityUpdateNotifications(gattMsgEvent_t *pMsg);

/**
 * @brief Request AMS Entity Attribute values from the remote device.
 */
extern void ams_entityAttributeReadRequest();

/**
 * @brief Parse AMS Entity Attribute response data.
 * @param pMsg Pointer to GATT message event.
 */
extern void ams_parseEntityAttributeResponses(gattMsgEvent_t *pMsg);

/**
 * @brief Request AMS Player information from the remote device.
 */
extern void ams_requestPlayerInfo(void);

/**
 * @brief Request AMS Queue information from the remote device.
 */
extern void ams_requestQueueInfo(void);

/**
 * @brief Request AMS Track information from the remote device.
 */
extern void ams_requestTrackInfo(void);

/*********************************************************************
 * @fn      ANCS_resetState
 *
 * @brief   Reset ANCS/AMS state machine for reconnection
 *
 * @return  void
 */
void ANCS_resetState(void);


/**
 * @brief Print all notifications currently stored in the ANCS notification queue.
 */
extern void printAllNotifInQueue(void);



/**
 * @brief Wrapper function for the GATT service discovery.
 *
 * This function is used to start the service discovery procedure.
 *
 * @return void
 */
extern void gatt_disc_service_wrapper();


/*********************************************************************
 *********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* ANCS_H */