/**
 * @file app_media_menu.c
 * @brief Media information display and menu logic for ANCS/AMS BLE client on CC27xx.
 *
 * This file provides functions to display media player information and notifications
 * received from iOS devices via the Apple Notification Center Service (ANCS) and
 * Apple Media Service (AMS) over Bluetooth Low Energy.
 *
 * Features:
 * - Displays player, track, and playback state information in a formatted menu
 * - Updates media info fields based on AMS entity update notifications
 * - Shows all current notifications from the ANCS notification queue
 * - Provides a clear and user-friendly output for serial/UART display
 *
 * Usage:
 * - Call print_media_info() to update the display with the latest media and notification info
 * - Ensure mediaMenuValues is updated by AMS/ANCS event handlers before calling
 *
Copyright (c) 2022-2025, Texas Instruments Incorporated
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
 */

#include <string.h>
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/app_util/menu/menu_module.h"
#include <app_main.h>
#include "app_ancs.h"

extern Display_Handle disp_handle;
#define MEDIA_MENU_HEADER_LINE 0
#define MEDIA_MENU_PLAYER_INFO_START_LINE 1
#define MEDIA_MENU_PLAYER_STATE_LINE 2
#define MEDIA_MENU_ARTIST_LINE 3
#define MEDIA_MENU_TITLE_LINE 4
#define MEDIA_MENU_ALBUM_LINE 5
#define MEDIA_MENU_DURATION_LINE 6
#define MEDIA_MENU_SEPARATOR_LINE 7
#define MEDIA_MENU_NOTIFICATIONS_START_LINE 8

static void printANCSNotifications();
static void print_media_info();

/**
 * @brief Print the current media player information and all notifications.
 *
 * This function clears the display and prints a formatted menu showing the current
 * player, state, artist, title, album, and duration, as well as all notifications
 * in the ANCS notification queue.
 */


void menu_refresh()
{
    print_media_info();
    printANCSNotifications();
}


void print_media_info()
{
    char mediaMenu[] = "========================= Media Info =========================";
    char mediaMenuSeparator[] = "==============================================================";

    Display_printf(disp_handle, 0, 0, "\033[2J"); // Clear screen
    Display_printf(disp_handle, 0xFF, 0, "%s", mediaMenu);

    // Interpret playback state
    const char *stateStr = gAmsPlayerInfo.playbackState;
    if (!strcmp(gAmsPlayerInfo.playbackState, "0"))
        stateStr = "paused";
    else if (!strcmp(gAmsPlayerInfo.playbackState, "1"))
        stateStr = "playing";
    else if (!strcmp(gAmsPlayerInfo.playbackState, "2"))
        stateStr = "rewinding";
    else if (!strcmp(gAmsPlayerInfo.playbackState, "3"))
        stateStr = "fast forwarding";

    Display_printf(disp_handle, 0xFF, 0, " Player: %s", gAmsPlayerInfo.playerName);
    Display_printf(disp_handle, 0xFF, 0, " Player State: %s", stateStr);
    Display_printf(disp_handle, 0xFF, 0, " Artist: %s", gAmsTrackInfo.artist);
    Display_printf(disp_handle, 0xFF, 0, " Title: %s", gAmsTrackInfo.title);
    Display_printf(disp_handle, 0xFF, 0, " Album: %s", gAmsTrackInfo.album);
    Display_printf(disp_handle, 0xFF, 0, " Duration: %s", gAmsTrackInfo.duration);
    Display_printf(disp_handle, 0xFF, 0, "%s", mediaMenuSeparator);
}


void printAMSPlayerInfo(void)
{
    Display_printf(disp_handle, 0xFF, 0, "AMS Player Info:");
    Display_printf(disp_handle, 0xFF, 0, "  Name: %s", gAmsPlayerInfo.playerName);
    Display_printf(disp_handle, 0xFF, 0, "  Playback State: %s", gAmsPlayerInfo.playbackState);
    Display_printf(disp_handle, 0xFF, 0, "  Playback Rate: %s", gAmsPlayerInfo.playbackRate);
    Display_printf(disp_handle, 0xFF, 0, "  Elapsed Time: %s", gAmsPlayerInfo.elapsedTime);
}

void printAMSTrackInfo(void)
{
    Display_printf(disp_handle, 0xFF, 0, "AMS Track Info:");
    Display_printf(disp_handle, 0xFF, 0, "  Artist: %s", gAmsTrackInfo.artist);
    Display_printf(disp_handle, 0xFF, 0, "  Title: %s", gAmsTrackInfo.title);
    Display_printf(disp_handle, 0xFF, 0, "  Album: %s", gAmsTrackInfo.album);
    Display_printf(disp_handle, 0xFF, 0, "  Duration: %s", gAmsTrackInfo.duration);
}

void printAMSQueueInfo(void)
{
    Display_printf(disp_handle, 0xFF, 0, "AMS Queue Info:");
    Display_printf(disp_handle, 0xFF, 0, "  Index: %s", gAmsQueueInfo.queueIndex);
    Display_printf(disp_handle, 0xFF, 0, "  Count: %s", gAmsQueueInfo.queueCount);
    Display_printf(disp_handle, 0xFF, 0, "  Shuffle Mode: %s", gAmsQueueInfo.shuffleMode);
    Display_printf(disp_handle, 0xFF, 0, "  Repeat Mode: %s", gAmsQueueInfo.repeatMode);
}

void printANCSNotifications(void)
{
    notifQueueNode_t *pNode = Ancs_getNotifQueueFront();
    int count = 0;
    Display_printf(disp_handle, 0xFF, 0, "---- Notification Queue ----");
    while (pNode) {
        Display_printf(disp_handle, 0xFF, 0, "Category: %s", pNode->notifInfo.category);
        Display_printf(disp_handle, 0xFF, 0, "AppID: %s", pNode->notifInfo.appID);
        Display_printf(disp_handle, 0xFF, 0, "App Name: %s", pNode->notifInfo.appName);
        Display_printf(disp_handle, 0xFF, 0, "Title: %s", pNode->notifInfo.title);
        Display_printf(disp_handle, 0xFF, 0, "Message: %s", pNode->notifInfo.message);
        Display_printf(disp_handle, 0xFF, 0, "Date: %s", pNode->notifInfo.date);
        Display_printf(disp_handle, 0xFF, 0, "----------------------------");
        pNode = pNode->pNext;
        count++;
    }
    if (count == 0) {
        Display_printf(disp_handle, 0xFF, 0, "No notifications.");
    }
}

void printAMSRemoteCommands(void)
{
    Display_printf(disp_handle, 0xFF, 0, "AMS Remote Commands:");
    if (gAmsRemoteCommands.play) Display_printf(disp_handle, 0xFF, 0, "  Play");
    if (gAmsRemoteCommands.pause) Display_printf(disp_handle, 0xFF, 0, "  Pause");
    if (gAmsRemoteCommands.toggle_play_pause) Display_printf(disp_handle, 0xFF, 0, "  Toggle Play/Pause");
    if (gAmsRemoteCommands.next_track) Display_printf(disp_handle, 0xFF, 0, "  Next Track");
    if (gAmsRemoteCommands.previous_track) Display_printf(disp_handle, 0xFF, 0, "  Previous Track");
    if (gAmsRemoteCommands.volume_up) Display_printf(disp_handle, 0xFF, 0, "  Volume Up");
    if (gAmsRemoteCommands.volume_down) Display_printf(disp_handle, 0xFF, 0, "  Volume Down");
    if (gAmsRemoteCommands.advance_repeat_mode) Display_printf(disp_handle, 0xFF, 0, "  Advance Repeat Mode");
    if (gAmsRemoteCommands.advance_shuffle_mode) Display_printf(disp_handle, 0xFF, 0, "  Advance Shuffle Mode");
    if (gAmsRemoteCommands.skip_forward) Display_printf(disp_handle, 0xFF, 0, "  Skip Forward");
    if (gAmsRemoteCommands.skip_backward) Display_printf(disp_handle, 0xFF, 0, "  Skip Backward");
    if (gAmsRemoteCommands.like_track) Display_printf(disp_handle, 0xFF, 0, "  Like Track");
    if (gAmsRemoteCommands.dislike_track) Display_printf(disp_handle, 0xFF, 0, "  Dislike Track");
    if (gAmsRemoteCommands.bookmark_track) Display_printf(disp_handle, 0xFF, 0, "  Bookmark Track");
}