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
#include "GATT/app_gattutil.h"
#include "GATT/assigned_numbers/app_assigned_numbers.h"
#include "ti_ble_config.h"

#if !defined(Display_DISABLE_ALL)
//*****************************************************************************
//! Prototypes
//*****************************************************************************

extern int snprintf(char *str, size_t n, const char *format, ...);

// Scanning callbacks
void Menu_scanningCB(uint8 index);
void Menu_scanStartCB(uint8 index);
void Menu_scanStopCB(uint8 index);
// Connection callbacks
void Menu_connectionCB(uint8 index);
void Menu_connectCB(uint8 index);
void Menu_connectToDeviceCB(uint8 index);
void Menu_workWithCB(uint8 index);
void Menu_selectedDeviceCB(uint8 index);
void Menu_connPhyCB(uint8 index);
void Menu_connPhyChangeCB(uint8 index);
void Menu_paramUpdateCB(uint8 index);
void Menu_disconnectCB(uint8 index);
// Gatt callbacks
void Menu_GattServicesCB(uint8 index);
void Menu_GattServicesSelectCB(
  GATTUtil_service* GATTServices,
  uint16_t GATTServicesLength,
  void* cbContext);
void Menu_GattCharCB(uint8 index);
void Menu_GattCharSelectCB(
  GATTUtil_char* GATTChars, 
  uint16_t GATTCharsLength,
  void* cbContext);
void Menu_GattCharActionCB(uint8 index);
void Menu_GattCharReadCB(uint8 index);
void Menu_GattCharWriteCB(uint8 index);
void Menu_GattCharEnableNotifyCB(uint8 index);
void Menu_GattCharDisableNotifyCB(uint8 index);
void Menu_GattCharReadDisplayCB(GATTUtil_value value, void* cbContext);
// Memory function
static bStatus_t FreeGATTServices();
static bStatus_t FreeGATTChars();

//*****************************************************************************
//! Globals
//*****************************************************************************

// GATT services
GATTUtil_service* currentGATTServices;
uint16_t currentGATTServicesLength = 0;
MenuModule_Menu_t* GATTServicesMenu = NULL;
MenuModule_MenuObject_t* GATTServicesObject = NULL;

// GATT characteristics
GATTUtil_char* currentGATTChars;
uint16_t currentGATTCharsLength = 0;
GATTUtil_char currentGATTChar;
MenuModule_Menu_t* GATTCharsMenu;
MenuModule_MenuObject_t* GATTCharsObject;

// The current connection handle the menu is working with
static uint16 menuCurrentConnHandle;

// Scan Menu
const MenuModule_Menu_t scanningMenu[] =
{
 {"Scan", &Menu_scanStartCB, "Scan for devices"},
 {"Stop Scan", &Menu_scanStopCB, "Stop Scanning for devices"}
};

MENU_MODULE_MENU_OBJECT("Scanning Menu", scanningMenu);

// Connection Menu
const MenuModule_Menu_t connectionMenu[] =
{
 {"Connect", &Menu_connectCB, "Connect to a device"},
 {"Work with", &Menu_workWithCB, "Work with a peer device"}
};

MENU_MODULE_MENU_OBJECT("Connection Menu", connectionMenu);

// Work with menu
const MenuModule_Menu_t workWithMenu[] =
{
 {"GATT", &Menu_GattServicesCB, "Explore the GATT table"},
 {"Change conn phy", &Menu_connPhyCB, "1M, Coded or 2M"},
 {"Param update", &Menu_paramUpdateCB, "Send connection param update req"},
 {"Disconnect", &Menu_disconnectCB, "Disconnect a specific connection"}
};

MENU_MODULE_MENU_OBJECT("Work with Menu", workWithMenu);

// Phy selection menu
const MenuModule_Menu_t connPhyMenu[] =
{
 {"1 Mbps", &Menu_connPhyChangeCB, ""},
 {"2 Mbps", &Menu_connPhyChangeCB, ""},
 {"1 & 2 Mbps", &Menu_connPhyChangeCB, ""},
 {"Coded", &Menu_connPhyChangeCB, ""},
 {"1 & 2 Mbps & Coded", &Menu_connPhyChangeCB, ""},
};

MENU_MODULE_MENU_OBJECT("Set Conn PHY Preference", connPhyMenu);

// Main menu
const MenuModule_Menu_t mainMenu[] =
{
 {"Scanning", &Menu_scanningCB, "Scan menu"},
 {"Connection", &Menu_connectionCB, "Connection menu"},
};

MENU_MODULE_MENU_OBJECT("Full GATT Client Menu", mainMenu);

//*****************************************************************************
//! Functions
//*****************************************************************************

/*********************************************************************
 * @fn      Menu_scanningCB
 *
 * @brief   A callback that will be called once the Scanning item in
 *          the main menu is selected.
 *          Calls MenuModule_startSubMenu to display the scanning menu.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_scanningCB(uint8 index)
{
  MenuModule_startSubMenu(&scanningMenuObject);
}

/*********************************************************************
 * @fn      Menu_scanStartCB
 *
 * @brief   A callback that will be called once the scan item in
 *          the scanningMenu is selected.
 *          Sets the parameters needed for a scan and starts the scan.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_scanStartCB(uint8 index)
{
    bStatus_t status;
    const BLEAppUtil_ScanStart_t centralScanStartParams =
    {
        /*! Zero for continuously scanning */
        .scanPeriod     = DEFAULT_SCAN_PERIOD, /* Units of 1.28sec */

        /*! Scan Duration shall be greater than to scan interval,*/
        /*! Zero continuously scanning. */
        .scanDuration   = DEFAULT_SCAN_DURATION, /* Units of 10ms */

        /*! If non-zero, the list of advertising reports will be */
        /*! generated and come with @ref GAP_EVT_SCAN_DISABLED.  */
        .maxNumReport   = APP_MAX_NUM_OF_ADV_REPORTS
    };

    status = BLEAppUtil_scanStart(&centralScanStartParams);

    // Print the status of the scan
    MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0, "Call Status: ScanStart = "
                      MENU_MODULE_COLOR_BOLD MENU_MODULE_COLOR_RED "%d" MENU_MODULE_COLOR_RESET,
                      status);
}

/*********************************************************************
 * @fn      Menu_scanStopCB
 *
 * @brief   A callback that will be called once the stop scan item in
 *          the scanningMenu is selected.
 *          Calls BLEAppUtil_scanStop and display the returned status.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_scanStopCB(uint8 index)
{
    bStatus_t status;

    status = BLEAppUtil_scanStop();

    // Print the status of the scan
    MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0, "Call Status: ScanStop = "
                      MENU_MODULE_COLOR_BOLD MENU_MODULE_COLOR_RED "%d" MENU_MODULE_COLOR_RESET,
                      status);
}

/*********************************************************************
 * @fn      Menu_connectionCB
 *
 * @brief   A callback that will be called once the connection item in
 *          the main menu is selected.
 *          Calls MenuModule_startSubMenu to display the connection menu.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_connectionCB(uint8 index)
{
  MenuModule_startSubMenu(&connectionMenuObject);
}

/*********************************************************************
 * @fn      Menu_connectCB
 *
 * @brief   A callback that will be called once the connect item in
 *          the connectionMenu is selected.
 *          Gets the list of scanned devices and calls
 *          MenuModule_printStringList to display the list of addresses.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_connectCB(uint8 index)
{
    uint8 i = 0;
    // Create a static list for the devices addresses strings
    static char addressList[APP_MAX_NUM_OF_ADV_REPORTS][BLEAPPUTIL_ADDR_STR_SIZE] = {0};
    // Create a static menu that will contain the addresses
    static MenuModule_Menu_t peerAddr[APP_MAX_NUM_OF_ADV_REPORTS];

    // Get the scan results list
    App_scanResults *menuScanRes;
    uint8 size = Scan_getScanResList(&menuScanRes);

    // If the scan result list is empty, pring a msg
    if (size == 0)
    {
        MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0, "Call Status: No devices in scan list");
    }
    // If the list is not empty, copy the addresses and fill up the menu
    else
    {
        for(i = 0; i < size; i++)
        {
            // Convert the addresses to strings
            memcpy(addressList[i], BLEAppUtil_convertBdAddr2Str(menuScanRes[i].address), BLEAPPUTIL_ADDR_STR_SIZE);
            peerAddr[i].itemName = addressList[i];
            peerAddr[i].itemCallback = &Menu_connectToDeviceCB;
            peerAddr[i].itemHelp = "";
        }
        // Create the menu object
        MENU_MODULE_MENU_OBJECT("Addresses List", peerAddr);
        // Display the list
        MenuModule_printStringList(&peerAddrObject, size);
    }
}

/*********************************************************************
 * @fn      Menu_connectToDeviceCB
 *
 * @brief   A callback that will be called once a device address in
 *          the scan results list is selected.
 *          Gets the list of scanned devices and connect to the address
 *          in the provided index.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_connectToDeviceCB(uint8 index)
{
    bStatus_t status;

    // Get the scan results list
    App_scanResults *menuScanRes;
    uint8 size = Scan_getScanResList(&menuScanRes);

    // Set the connection parameters
    BLEAppUtil_ConnectParams_t connParams =
    {
     .peerAddrType = menuScanRes[index].addressType,
     .phys = DEFAULT_INIT_PHY,
     .timeout = 1000
    };

    // Copy the selected address
    memcpy(connParams.pPeerAddress, menuScanRes[index].address, B_ADDR_LEN);
    status = BLEAppUtil_connect(&connParams);

    // Print the status of the connect call
    MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0, "Call Status: Connect = "
                      MENU_MODULE_COLOR_BOLD MENU_MODULE_COLOR_RED "%d" MENU_MODULE_COLOR_RESET,
                      status);

    // Go back to the last menu
    MenuModule_goBack();
}

/*********************************************************************
 * @fn      Menu_workWithCB
 *
 * @brief   A callback that will be called once the work with item in
 *          the connectionMenu is selected.
 *          Gets the list of connected devices and calls
 *          MenuModule_printStringList to display the list of addresses.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_workWithCB(uint8 index)
{
    uint8 i;
    uint8 numConns = linkDB_NumActive();

    // If the connection list is empty, pring a msg
    if (numConns == 0)
    {
        MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0, "Call Status: No connected devices");
    }
    // If the list is not empty, copy the addresses and fill up the menu
    else
    {
        // Create a static list for the devices addresses strings
        static char connAddrsses[MAX_NUM_BLE_CONNS][BLEAPPUTIL_ADDR_STR_SIZE] = {0};
        // Create a static menu that will contain the addresses
        static MenuModule_Menu_t connAddrList[MAX_NUM_BLE_CONNS];
        // Get the list of connected devices
        App_connInfo * currConnList = Connection_getConnList();
        for(i = 0; i < numConns; i++)
        {
            // Convert the addresses to strings
            memcpy(connAddrsses[i], BLEAppUtil_convertBdAddr2Str(currConnList[i].peerAddress), BLEAPPUTIL_ADDR_STR_SIZE);
            connAddrList[i].itemName = connAddrsses[i];
            connAddrList[i].itemCallback = &Menu_selectedDeviceCB;
            connAddrList[i].itemHelp = "";
        }

        // Create the menu object
        MENU_MODULE_MENU_OBJECT("Connected devices List", connAddrList);
        // Display the list
        MenuModule_printStringList(&connAddrListObject, numConns);
    }
}

/*********************************************************************
 * @fn      Menu_selectedDeviceCB
 *
 * @brief   A callback that will be called once a device address in
 *          the connected devices list is selected.
 *          Go back to the last menu and display the work with menu.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_selectedDeviceCB(uint8 index)
{
    menuCurrentConnHandle = Connection_getConnhandle(index);
    // Go to the last menu
    MenuModule_goBack();
    // Display the work with menu options
    MenuModule_startSubMenu(&workWithMenuObject);
}

/*********************************************************************
 * @fn      Menu_connPhyCB
 *
 * @brief   A callback that will be called once the Change conn phy item
 *          in the workWithMenu is selected.
 *          Display the connPhy options menu.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_connPhyCB(uint8 index)
{
  MenuModule_startSubMenu(&connPhyMenuObject);
}

/*********************************************************************
 * @fn      Menu_connPhyChangeCB
 *
 * @brief   A callback that will be called once a phy item in the
 *          connPhyMenu is selected.
 *          Calls BLEAppUtil_setConnPhy to set the selected phy and
 *          go back to the work with menu.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_connPhyChangeCB(uint8 index)
{
    bStatus_t status;
    static uint8_t phy[] = {
      HCI_PHY_1_MBPS,
      HCI_PHY_2_MBPS,
      HCI_PHY_1_MBPS | HCI_PHY_2_MBPS,
      HCI_PHY_CODED,
      HCI_PHY_1_MBPS | HCI_PHY_2_MBPS | HCI_PHY_CODED
    };

    BLEAppUtil_ConnPhyParams_t phyParams =
    {
     .connHandle = menuCurrentConnHandle,
     .allPhys = 0,
     .txPhy = phy[index],
     .rxPhy = phy[index],
     .phyOpts = 0
    };

    // Set the connection phy selected in the menu
    status = BLEAppUtil_setConnPhy(&phyParams);

    // Print the status of the set conn phy call
    MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0, "Call Status: SetConnPhy = "
                      MENU_MODULE_COLOR_BOLD MENU_MODULE_COLOR_RED "%d" MENU_MODULE_COLOR_RESET,
                      status);

    // Go back to the "work with" menu
    MenuModule_goBack();
}

/*********************************************************************
 * @fn      Menu_paramUpdateCB
 *
 * @brief   A callback that will be called once the Param update item
 *          in the workWithMenu is selected.
 *          Calls BLEAppUtil_paramUpdateReq to send the parameters
 *          update request.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_paramUpdateCB(uint8 index)
{
    bStatus_t status;
    gapUpdateLinkParamReq_t pParamUpdateReq =
    {
     .connectionHandle = menuCurrentConnHandle,
     .intervalMin = 400,
     .intervalMax = 800,
     .connLatency = 0,
     .connTimeout = 600
    };

    // Send a connection param update request
    status = BLEAppUtil_paramUpdateReq(&pParamUpdateReq);

    // Print the status of the param update call
    MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0, "Call Status: ParamUpdateReq = "
                      MENU_MODULE_COLOR_BOLD MENU_MODULE_COLOR_RED "%d" MENU_MODULE_COLOR_RESET,
                      status);
}

/*********************************************************************
 * @fn      Menu_disconnectCB
 *
 * @brief   A callback that will be called once the Disconnect item
 *          in the workWithMenu is selected.
 *          Calls BLEAppUtil_disconnect to disconnect from the
 *          menuCurrentConnHandle.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_disconnectCB(uint8 index)
{
    bStatus_t status;

    // Disconnect from the selected connection
    status = BLEAppUtil_disconnect(menuCurrentConnHandle);

    // Print the status of the set conn phy call
    MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0, "Call Status: Disconnect = "
                      MENU_MODULE_COLOR_BOLD MENU_MODULE_COLOR_RED "%d" MENU_MODULE_COLOR_RESET,
                      status);

    // Go back to the "connection" menu
    MenuModule_goBack();
}

/*********************************************************************
 * @fn      Menu_GattServicesCB
 *
 * @brief   A callback that will be called once the GATT item in
 *          the Work With menu is selected.
 *          Calls GATTUtil_GetPrimaryServices with a callback to 
 *          Menu_GattServicesSelectCB to get the primary services
 *          of the GATT table.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */

void Menu_GattServicesCB(uint8 index)
{
  MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0, "GATT Status: Discovering primary services ...");

  // Get the primary services from the peripheral GATT table
  GATTUtil_GetPrimaryServices(
      menuCurrentConnHandle,
      Menu_GattServicesSelectCB,
      //GattServicesCB,
      NULL
  );
}

/*********************************************************************
 * @fn      Menu_GattServicesCB
 *
 * @brief   A callback that will be called by GATTUtil_GetPrimaryServices
 *          once the ATT requests for the primary services are
 *          completed.
 *
 * @param   GATTServices - the list of GATT services found by
 *                         GATTUtil_GetPrimaryServices
 * @param   GATTServicesLength - the length of the GATT services
 *                               list
 * @param   cbContext - pointer passed to GATTUtil_GetPrimaryServices
 *
 * @return  none
 */
void Menu_GattServicesSelectCB(
  GATTUtil_service* GATTServices, 
  uint16_t GATTServicesLength,
  void* cbContext)
{
    // If GATTServices is empty, print a message
    if (GATTServicesLength == 0)
    {
        MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0, "Call Status: No primary services discovered");
        return;
    }

    MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0, "GATT Status: Primary services successfuly discovered!");
    
    FreeGATTServices();

    // Save the GATT services list
    currentGATTServices = GATTServices;
    currentGATTServicesLength = GATTServicesLength;

    // Create a static menu that will contain the addresses
    GATTServicesMenu = ICall_malloc(sizeof(MenuModule_Menu_t) * GATTServicesLength);

    // If the list is not empty, copy the addresses and fill up the menu
    for(uint8_t i = 0; i < GATTServicesLength; i++)
    {
        // Convert the service handle to a string
        char* handleName = ICall_malloc(sizeof(char) * 20);
        snprintf(handleName, 20, "%d", GATTServices[i].startHandle);


        // Convert the addresses to strings
        GATTServicesMenu[i].itemName = handleName;
        GATTServicesMenu[i].itemCallback = &Menu_GattCharCB;
        GATTServicesMenu[i].itemHelp = (char*)GATTUtil_getServiceNameByUUID(GATTServices[i].UUID);
    }

    // Create the menu object
    GATTServicesObject = ICall_malloc(sizeof(MenuModule_MenuObject_t));
    GATTServicesObject->menuArray = GATTServicesMenu;
    GATTServicesObject->menuNumItems = GATTServicesLength;
    GATTServicesObject->menuTitle = "Primary services";

    // Display the list
    MenuModule_printStringList(GATTServicesObject, GATTServicesLength);
}

/*********************************************************************
 * @fn      Menu_GattCharCB
 *
 * @brief   A callback that will be called once the user selects a
 *          GATT service in GATT menu.
 *          Calls GATTUtil_GetCharsOfService with a callback to 
 *          Menu_GattCharSelectCB to get the characteristics of the
 *          service.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_GattCharCB(uint8 index)
{
  GATTUtil_service currentGATTService = currentGATTServices[index];

  MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0, "GATT Status: Discovering characteristics ...");

  GATTUtil_GetCharsOfService(
    menuCurrentConnHandle, 
    currentGATTService,
    Menu_GattCharSelectCB,
    NULL);
}

/*********************************************************************
 * @fn      Menu_GattCharSelectCB
 *
 * @brief   A callback that will be called by GATTUtil_GetCharsOfService
 *          once the ATT requests for the characteristics of the service
 *          are completed.
 *
 * @param   GATTChars - the list of GATT characteristics found by
 *                      GATTUtil_GetCharsOfService
 * @param   GATTCharsLength - the length of the GATT characteristics
 *                            list
 * @param   cbContext - pointer passed to GATTUtil_GetCharsOfService
 *
 * @return  none
 */
void Menu_GattCharSelectCB(
  GATTUtil_char* GATTChars, 
  uint16_t GATTCharsLength,
  void* cbContext)
{

    // If GATTChars is empty, print a message
    if (GATTCharsLength == 0)
    {
        MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0, "Call Status: No characteristics discovered");
        return;
    }

    MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0, "GATT Status: Characteristics successfuly discovered!");
  
    FreeGATTChars();

    // Save the GATT characteristic list
    currentGATTChars = GATTChars;
    currentGATTCharsLength = GATTCharsLength;

    // Create a static menu that will contain the addresses
    GATTCharsMenu = ICall_malloc(sizeof(MenuModule_Menu_t) * GATTCharsLength);

    // If the list is not empty, copy the addresses and fill up the menu
    for(uint8_t i = 0; i < GATTCharsLength; i++)
    {
        // Convert the characteristic handle to a string
        char* handleName = ICall_malloc(sizeof(char) * 20);
        snprintf(handleName, 20, "%d", GATTChars[i].handle);


        // Convert the addresses to strings
        GATTCharsMenu[i].itemName = handleName;
        GATTCharsMenu[i].itemCallback = &Menu_GattCharActionCB;
        GATTCharsMenu[i].itemHelp = (char*)GATTUtil_getCharacteristicNameByUUID(GATTChars[i].UUID);
    }

    // Create the menu object
    GATTCharsObject = ICall_malloc(sizeof(MenuModule_MenuObject_t));
    GATTCharsObject->menuArray = GATTCharsMenu;
    GATTCharsObject->menuNumItems = GATTCharsLength;
    GATTCharsObject->menuTitle = "Characteristics";

    // Display the list
    MenuModule_printStringList(GATTCharsObject, GATTCharsLength);
}

/*********************************************************************
 * @fn      Menu_GattCharActionCB
 *
 * @brief   A callback that will be called once the GATT characteristic
 *          in the GATT characterstics menu is selected.
 *          Creates a menu with 4 possible actions : Read, Write, Enable
 *          notifications and Disable notificatons.
 *
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_GattCharActionCB(uint8 index)
{
    currentGATTChar = currentGATTChars[index];

    // Create a static menu that will contain the possible actions 
    // (Read, Write, Enable notifications & Disable notifications)
    static MenuModule_Menu_t actions[4];

    // Read
    if(GATTUtil_isCharReadable(currentGATTChar))
    {
      actions[0].itemName = "Read";
      actions[0].itemHelp = "Read the value of the characteristic";
      actions[0].itemCallback = &Menu_GattCharReadCB;
    }
    else
    {
      // Disable the action
      actions[0].itemName = MENU_MODULE_COLOR_RED "Read" MENU_MODULE_COLOR_RESET;
      actions[0].itemHelp = MENU_MODULE_COLOR_RED "Read the value of the characteristic" MENU_MODULE_COLOR_RESET;
      actions[0].itemCallback = NULL;
    }

    // Write
    if(GATTUtil_isCharWritable(currentGATTChar))
    {
      actions[1].itemName = "Write";
      actions[1].itemHelp = "Write a value of the characteristic";
      actions[1].itemCallback = &Menu_GattCharWriteCB;
    }
    else
    {
      // Disable the action
      actions[1].itemName = MENU_MODULE_COLOR_RED "Write" MENU_MODULE_COLOR_RESET;
      actions[1].itemHelp = MENU_MODULE_COLOR_RED "Write a value of the characteristic" MENU_MODULE_COLOR_RESET;
      actions[1].itemCallback = NULL;
    }

    // Enable notifications
    if(GATTUtil_isCharNotif(currentGATTChar))
    {
      actions[2].itemName = "Notify enable";
      actions[2].itemHelp = "Enable notifications from this characteristic";
      actions[2].itemCallback = &Menu_GattCharEnableNotifyCB;
    }
    else
    {
      // Disable the action
      actions[2].itemName = MENU_MODULE_COLOR_RED "Notify enable" MENU_MODULE_COLOR_RESET;
      actions[2].itemHelp = MENU_MODULE_COLOR_RED "Enable notifications from this characteristic" MENU_MODULE_COLOR_RESET;
      actions[2].itemCallback = NULL;
    }

    // Disable notifications
    if(GATTUtil_isCharNotif(currentGATTChar))
    {
      actions[3].itemName = "Notify disable";
      actions[3].itemHelp = "Disable notifications from this characteristic";
      actions[3].itemCallback = &Menu_GattCharDisableNotifyCB;
    }
    else
    {
      // Disable the action
      actions[3].itemName = MENU_MODULE_COLOR_RED "Notify disable" MENU_MODULE_COLOR_RESET;
      actions[3].itemHelp = MENU_MODULE_COLOR_RED "Disable notifications from this characteristic" MENU_MODULE_COLOR_RESET;
      actions[3].itemCallback = NULL;
    }

    // Create the menu object
    MENU_MODULE_MENU_OBJECT("Actions", actions);

    // Display the list
    MenuModule_printStringList(&actionsObject, 4);
}

/*********************************************************************
 * @fn      Menu_GattCharReadCB
 *
 * @brief   A callback that will be called once the Read action
 *          in the GATT characterstics actions menu is selected.
 *          Calls GATTUtil_GetValueOfAttribute with a callback to 
 *          Menu_GattCharReadDisplayCB to get the value of the
 *          characteristic.
 * 
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_GattCharReadCB(uint8 index)
{
  GATTUtil_GetValueOfAttribute(
    menuCurrentConnHandle,
    currentGATTChar.valueHandle,
    &Menu_GattCharReadDisplayCB,
    NULL
  );

  // Go back to the last menu
  MenuModule_goBack();
}

/*********************************************************************
 * @fn      Menu_GattCharReadDisplayCB
 *
 * @brief   A callback that will be called by GATTUtil_GetValueOfAttribute
 *          once the ATT requests for the value of the characteristic
 *          are completed.
 *          Displays the first byte of the characteristic value.
 *          Frees the value after usage.
 *
 * @param   value - the value object of the GATT characteristics obtained
 *                  by GATTUtil_GetCharsOfService
 * @param   cbContext - pointer passed to GATTUtil_GetCharsOfService
 *
 * @return  none
 */
void Menu_GattCharReadDisplayCB(GATTUtil_value value, void* cbContext)
{
  MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0,
    MENU_MODULE_COLOR_GREEN "Length of data = %d | First byte read = %02x" MENU_MODULE_COLOR_RESET,
    value.len, value.pValue[0]);

  GATTUtil_freeValue(value);
}

/*********************************************************************
 * @fn      Menu_GattCharWriteCB
 *
 * @brief   A callback that will be called once the Write action
 *          in the GATT characterstics actions menu is selected.
 *          Calls GATTUtil_WriteCharValue to write a fixed sample data 
 *          to the characteristic.
 * 
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_GattCharWriteCB(uint8 index)
{
  bStatus_t status;
  attWriteReq_t req;

  // Sample data
  uint8_t charVals[4] = { 0x00, 0x02, 0x55, 0xFF };

  req.handle = currentGATTChar.valueHandle;
  req.pValue = GATT_bm_alloc(menuCurrentConnHandle, ATT_WRITE_REQ, 1, NULL);
  req.len = 1;
  req.pValue[0] = charVals[index];
  req.sig = 0;
  req.cmd = 0;

  status = GATT_WriteCharValue(menuCurrentConnHandle, &req, BLEAppUtil_getSelfEntity());
  if (status != SUCCESS)
  {
      GATT_bm_free((gattMsg_t *)&req, ATT_WRITE_REQ);
  }


  // Inform that the operation has succeeded
  MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0,
    MENU_MODULE_COLOR_GREEN "Sample data written to characteristic" MENU_MODULE_COLOR_RESET);

  // Go back to the last menu
  MenuModule_goBack();
}

/*********************************************************************
 * @fn      Menu_GattCharEnableNotifyCB
 *
 * @brief   A callback that will be called once the Enable Notifications
 *          action in the GATT characterstics actions menu is selected.
 *          Calls GATT_WriteNoRsp to write {0x01,0x00} into the 
 *          characteristic to enable notifications.
 * 
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_GattCharEnableNotifyCB(uint8 index)
{
  bStatus_t status;
  attWriteReq_t req;

  uint8 configData[2] = {0x01,0x00};
  req.pValue = GATT_bm_alloc(menuCurrentConnHandle, ATT_WRITE_REQ, 2, NULL);

  // Enable notify for outgoing data
  if (req.pValue != NULL)
  {
      req.handle = currentGATTChar.valueHandle;
      req.len = 2;
      memcpy(req.pValue, configData, 2);
      req.cmd = TRUE;
      req.sig = FALSE;
      status = GATT_WriteNoRsp(menuCurrentConnHandle, &req);
      if ( status != SUCCESS )
      {
          GATT_bm_free((gattMsg_t *)&req, ATT_WRITE_REQ);
      }
  }

  // Inform that the operation has succeeded
  MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0,
    MENU_MODULE_COLOR_GREEN "Characteristic notifications enabled" MENU_MODULE_COLOR_RESET);

  // Go back to the last menu
  MenuModule_goBack();
}

/*********************************************************************
 * @fn      Menu_GattCharDisableNotifyCB
 *
 * @brief   A callback that will be called once the Disable Notifications
 *          action in the GATT characterstics actions menu is selected.
 *          Calls GATT_WriteNoRsp to write {0x00,0x00} into the 
 *          characteristic to disable notifications.
 * 
 * @param   index - the index in the menu
 *
 * @return  none
 */
void Menu_GattCharDisableNotifyCB(uint8 index)
{
  bStatus_t status;
  attWriteReq_t req;

  uint8 configData[2] = {0x00,0x00};
  req.pValue = GATT_bm_alloc(menuCurrentConnHandle, ATT_WRITE_REQ, 2, NULL);

  // Enable notify for outgoing data
  if (req.pValue != NULL)
  {
      req.handle = currentGATTChar.valueHandle;
      req.len = 2;
      memcpy(req.pValue, configData, 2);
      req.cmd = TRUE;
      req.sig = FALSE;
      status = GATT_WriteNoRsp(menuCurrentConnHandle, &req);
      if ( status != SUCCESS )
      {
          GATT_bm_free((gattMsg_t *)&req, ATT_WRITE_REQ);
      }
  }

  // Inform that the operation has succeeded
  MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0,
    MENU_MODULE_COLOR_GREEN "Characteristic notifications disabled" MENU_MODULE_COLOR_RESET);

  // Go back to the last menu
  MenuModule_goBack();
}

#endif // #if !defined(Display_DISABLE_ALL)

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

#if !defined(Display_DISABLE_ALL)
  MenuModule_params_t params = {
    .mode = MenuModule_Mode_MENU_WITH_BUTTONS
  };

  status = MenuModule_init(&mainMenuObject, &params);
#endif // #if !defined(Display_DISABLE_ALL)
  return status;
}

/*********************************************************************
 * @fn      FreeGATTServices
 *
 * @brief   This function frees the currentGATTServices and resets their 
 *          associated metadata.
 *
 * @return  SUCCESS
 */
static bStatus_t FreeGATTServices()
{
  if(currentGATTServices)
  {
    ICall_free(currentGATTServices);
  }

  // Free the buffer for the name
  for(uint8_t i = 0; i < currentGATTServicesLength; i++)
  {
      ICall_free(GATTServicesMenu[i].itemName);
  }

  if(GATTServicesMenu)
  {
    ICall_free(GATTServicesMenu);
  }
  
  if(GATTServicesMenu)
  {
    ICall_free(GATTServicesObject);
  }

  currentGATTServices = NULL;
  GATTServicesMenu = NULL;
  GATTServicesObject = NULL;
  currentGATTServicesLength = 0;

  return SUCCESS;
}

/*********************************************************************
 * @fn      FreeGATTChars
 *
 * @brief   This function frees the currentGATTChars and resets their 
 *          associated metadata.
 *
 * @return  SUCCESS
 */
static bStatus_t FreeGATTChars()
{
  if(currentGATTChars)
  {
    ICall_free(currentGATTChars);
  }

  // Free the buffer for the name
  for(uint8_t i = 0; i < currentGATTCharsLength; i++)
  {
      ICall_free(GATTCharsMenu[i].itemName);
  }

  if(GATTCharsMenu)
  {
    ICall_free(GATTCharsMenu);
  }

  if(GATTCharsObject)
  {
    ICall_free(GATTCharsObject);
  }
  
  currentGATTChars = NULL;
  GATTCharsMenu = NULL;
  GATTCharsObject = NULL;
  currentGATTCharsLength = 0;

  return SUCCESS;
}
