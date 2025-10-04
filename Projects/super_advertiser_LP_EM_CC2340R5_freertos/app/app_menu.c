/******************************************************************************

@file  app_menu.c

Group: WCS, BTS
Target Device: cc23xx

******************************************************************************

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

******************************************************************************


*****************************************************************************/


//*****************************************************************************
//! Includes
//*****************************************************************************
#include <string.h>
#include <ti/ble/app_util/framework/bleapputil_api.h>
#include <ti/ble/app_util/menu/menu_module.h>
#include <app_main.h>
#include <app_super_adv.h>
#include "ti_ble_config.h"


//*****************************************************************************
//! Prototypes
//*****************************************************************************
// Settings callbacks
void Menu_settingsCB(uint8 index);
void Menu_incrementCB(uint8 index);
void Menu_decrementCB(uint8 index);
// Advertising callbacks
void Menu_advertisingCB(uint8 index);
void Menu_ToggleAdvCB(uint8 index);

//*****************************************************************************
//! Globals
//*****************************************************************************

// Super advertiser parameters
extern uint8 broadcasterAdvHandle_1;
extern BLEAppUtil_AdvStart_t broadcasterStartAdvSet1;
extern SuperAdv_deviceAddressPool broadcasterDeviceAddressPool1;

uint8_t advertisingToggled = 1;

// Settings Menu
const MenuModule_Menu_t settingsMenu[] =
{
    {"Increment", &Menu_incrementCB, "Increment the number of advertising devices"},
    {"Decrement", &Menu_decrementCB, "Decrement the number of advertising devices"}
};

MENU_MODULE_MENU_OBJECT("Scanning Menu", settingsMenu);

// Settings Menu
const MenuModule_Menu_t advertisingMenu[] =
{
    {"Toggle advertising", &Menu_ToggleAdvCB, "Toggle the advertising on/off"},
};

MENU_MODULE_MENU_OBJECT("Advertising Menu", advertisingMenu);

// Main menu
const MenuModule_Menu_t mainMenu[] =
{
   {"Settings", &Menu_settingsCB, "Settings menu"},
   {"Advertising", &Menu_advertisingCB, "Advertising menu"},
};

MENU_MODULE_MENU_OBJECT("Super Advertiser Menu", mainMenu);

//*****************************************************************************
//! Functions
//*****************************************************************************

/*********************************************************************
 * @fn      Menu_settingsCB
 *
 * @brief   A callback that will be called once the Settings item in
 *          the main menu is selected.
 *          Calls MenuModule_startSubMenu to display the settings menu.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_settingsCB(uint8 index)
{
    MenuModule_startSubMenu(&settingsMenuObject);

    MenuModule_printf(APP_MENU_ADV_EVENT, 0, "Current number of advertisement client : "
                         MENU_MODULE_COLOR_BOLD MENU_MODULE_COLOR_GREEN "%d" MENU_MODULE_COLOR_RESET,
                         broadcasterDeviceAddressPool1.numAddresses);
}


/*********************************************************************
 * @fn      Menu_incrementCB
 *
 * @brief   A callback that will be called once the Increment item in
 *          the settings Menu is selected.
 *          Increments by one the amount of advertising devices.
 *          If the resulting number of advertising devices is
 *          higher than 100, wraps around to 1 devices.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_incrementCB(uint8 index)
{
    SuperAdv_incrementNumClient(&broadcasterDeviceAddressPool1);

    MenuModule_printf(APP_MENU_ADV_EVENT, 0, "Current number of advertisement client : "
                         MENU_MODULE_COLOR_BOLD MENU_MODULE_COLOR_GREEN "%d" MENU_MODULE_COLOR_RESET,
                         broadcasterDeviceAddressPool1.numAddresses);
}


/*********************************************************************
 * @fn      Menu_decrementCB
 *
 * @brief   A callback that will be called once the Decrement item in
 *          the settings Menu is selected.
 *          Decrements by one the amount of advertising devices.
 *          If the resulting number of advertising devices is
 *          lower than 1, wraps around to 100 devices.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_decrementCB(uint8 index)
{
    SuperAdv_decrementNumClient(&broadcasterDeviceAddressPool1);

    MenuModule_printf(APP_MENU_ADV_EVENT, 0, "Current number of advertisement client : "
                         MENU_MODULE_COLOR_BOLD MENU_MODULE_COLOR_GREEN "%d" MENU_MODULE_COLOR_RESET,
                         broadcasterDeviceAddressPool1.numAddresses);
}


/*********************************************************************
 * @fn      Menu_advertisingCB
 *
 * @brief   A callback that will be called once the Advertising item in
 *          the main menu is selected.
 *          Calls MenuModule_startSubMenu to display the advertising menu.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_advertisingCB(uint8 index)
{
    MenuModule_startSubMenu(&advertisingMenuObject);

    if(advertisingToggled)
    {
        MenuModule_printf(APP_MENU_ADV_EVENT, 0, "Advertising is currently : "
                         MENU_MODULE_COLOR_BOLD MENU_MODULE_COLOR_GREEN "ON" MENU_MODULE_COLOR_RESET);
    }
    else
    {
        MenuModule_printf(APP_MENU_ADV_EVENT, 0, "Advertising is currently : "
                         MENU_MODULE_COLOR_BOLD MENU_MODULE_COLOR_RED "OFF" MENU_MODULE_COLOR_RESET);
    }
}


/*********************************************************************
 * @fn      Menu_ToggleAdvCB
 *
 * @brief   A callback that will be called once the toggle item in
 *          the advertising Menu is selected.
 *          Disables the scan if it was enabled.
 *          Enables the scan if it was disabled.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_ToggleAdvCB(uint8 index)
{
    if(advertisingToggled)
    {
        advertisingToggled = 0;
        BLEAppUtil_advStop(broadcasterAdvHandle_1);
        MenuModule_printf(APP_MENU_ADV_EVENT, 0, "Advertising is currently : "
                         MENU_MODULE_COLOR_BOLD MENU_MODULE_COLOR_RED "OFF" MENU_MODULE_COLOR_RESET);
    }
    else
    {
        advertisingToggled = 1;
        BLEAppUtil_advStart(broadcasterAdvHandle_1, &broadcasterStartAdvSet1);
        MenuModule_printf(APP_MENU_ADV_EVENT, 0, "Advertising is currently : "
                         MENU_MODULE_COLOR_BOLD MENU_MODULE_COLOR_GREEN "ON" MENU_MODULE_COLOR_RESET);
    }
}

/*********************************************************************
 * @fn      Menu_start
 *
 * @brief   This function is called after stack initialization,
 *          the purpose of this function is to initialize the
 *          menu
 *
 * @return  SUCCESS, errorInfo
 */
bStatus_t Menu_start()
{
  bStatus_t status = SUCCESS;

  MenuModule_params_t params = {
    .mode = MenuModule_Mode_MENU_WITH_BUTTONS
  };

  status = MenuModule_init(&mainMenuObject, &params);

  return status;
}
