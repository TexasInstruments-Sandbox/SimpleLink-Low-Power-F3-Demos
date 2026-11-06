/******************************************************************************

@file  app_peripheral.c

@brief This example file demonstrates how to activate the peripheral role with
the help of BLEAppUtil APIs.

Two structures are used for event handling, one for connection events and one
for advertising events.
In each, eventMask is used to specify the events that will be received
and handled.
In addition, fill the BLEAppUtil_AdvInit_t structure with variables generated
by the Sysconfig.

In the events handler functions, write what actions are done after each event.
In this example, after a connection is made, activation is performed for
re-advertising up to the maximum connections.

In the Peripheral_start() function at the bottom of the file, registration,
initialization and activation are done using the BLEAppUtil API functions,
using the structures defined in the file.

More details on the functions and structures can be seen next to the usage.

Group: WCS, BTS
Target Device: cc23xx

******************************************************************************

Copyright (c) 2022-2026, Texas Instruments Incorporated
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

Copyright (c) 2022-2026, Texas Instruments Incorporated
All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

******************************************************************************
*****************************************************************************/

#if defined( HOST_CONFIG ) && ( HOST_CONFIG & ( PERIPHERAL_CFG ) )

//*****************************************************************************
//! Includes
//*****************************************************************************
#include "ti_ble_config.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/app_util/menu/menu_module.h"
#include "ti/ble/host/gapbondmgr/gapbondmgr.h"
#include <ble_app/app_main.h>
#include <keypad/key_scan.h>

//*****************************************************************************
//! Prototypes
//******************************************************s***********************
void Peripheral_AdvEventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData);
void Peripheral_GAPConnEventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData);

//*****************************************************************************
//! External declarations
//*****************************************************************************

//*****************************************************************************
//! Globals
//*****************************************************************************

BLEAppUtil_EventHandler_t peripheralConnHandler =
{
    .handlerType    = BLEAPPUTIL_GAP_CONN_TYPE,
    .pEventHandler  = Peripheral_GAPConnEventHandler,
    .eventMask      = BLEAPPUTIL_LINK_ESTABLISHED_EVENT |
                      BLEAPPUTIL_LINK_PARAM_UPDATE_REQ_EVENT |
                      BLEAPPUTIL_LINK_TERMINATED_EVENT,
};

BLEAppUtil_EventHandler_t peripheralAdvHandler =
{
    .handlerType    = BLEAPPUTIL_GAP_ADV_TYPE,
    .pEventHandler  = Peripheral_AdvEventHandler,
    .eventMask      = BLEAPPUTIL_ADV_START_AFTER_ENABLE |
                      BLEAPPUTIL_ADV_END_AFTER_DISABLE
};

//! Store handle needed for each advertise set
uint8_t peripheralAdvHandle_1;
uint8_t adv_finished;

//! OOBE Auto-restart: Track advertising restart for 10-minute requirement
//! BLE stack limitation: max 32767 units (5.46 min) per period
//! Auto-restart once to reach ~10.9 minutes total
static uint8_t advRestartCount = 0;
static uint8_t advStoppedByConnection = 0;  // Flag: TRUE if adv stopped due to connection
#define ADV_MAX_RESTARTS 1  // Restart once (total 2 periods ≈ 10.9 min)

//! Advertising Mode State Machine: Directed vs Undirected
typedef enum {
    ADV_MODE_UNDIRECTED,    // For OOBE, pairing mode, factory reset
    ADV_MODE_DIRECTED       // For reconnection with existing bonds
} AdvMode_t;

static AdvMode_t currentAdvMode = ADV_MODE_UNDIRECTED;

//! Flag to force undirected advertising after next disconnection
//! Set by MODE_REPAIR combo key handler in key_scan.c
uint8_t forceUndirectedOnNextDisconnect = 0;

//! Store last connected peer device address for bond reading
static uint8_t lastPeerDeviceAddr[B_ADDR_LEN] = {0};
static GAP_Peer_Addr_Types_t lastPeerAddrType = PEER_ADDRTYPE_PUBLIC_OR_PUBLIC_ID;

//! Parameters for storing bonding information
static gapBondRec_t savedBondRec;
static gapBondNvRecord_t savedNvRecord = {
    .pBondRec = &savedBondRec,
    .pLocalLtk = NULL,
    .pDevLtk = NULL,
    .pIRK = NULL,
    .pSRK = NULL,
    .pSignCount = NULL,
    .pCharCfg = NULL
};
static gapBondNvRecord_t* pSavedBondRec = &savedNvRecord;
static uint8_t pIdentifier;
static uint8_t readMode;

//! Advertise param, needed for each advertise set, Generate by Sysconfig
const BLEAppUtil_AdvInit_t advSetInitParamsSet_1 =
{
    /* Advertise data and length */
    .advDataLen        = sizeof(advData1),
    .advData           = advData1,

    /* Scan respond data and length */
    .scanRespDataLen   = sizeof(scanResData1),
    .scanRespData      = scanResData1,

    .advParam          = &advParams1
};

/* OOBE Advertising Parameters - Google ATV Spec Compliance
 *
 * Google ATV Spec Requirements:
 * - First boot (no bonds): 10 minutes advertising for initial pairing
 * - Subsequent boots (has bonds): Shorter duration acceptable
 *
 * BLE Stack Limitation:
 * - durationOrMaxEvents field is signed int16_t (not uint16_t as documented)
 * - Maximum safe value: 32767 units = 327.67s ≈ 5.46 minutes
 * - Values >32767 cause advertising failure (observed: 60000 fails, 32767 works)
 *
 * Solution: Auto-restart advertising (implemented in Peripheral_AdvEventHandler)
 * - First boot: 32767 units with auto-restart = ~10.92 min total ✓
 * - Normal boot: 9000 units (90 sec), no auto-restart
 *
 * Note: durationOrMaxEvents is set dynamically in Peripheral_start() based on bond count
 */
BLEAppUtil_AdvStart_t advSetStartParamsSet_1 =
{
    .enableOptions         = GAP_ADV_ENABLE_OPTIONS_USE_DURATION,
    .durationOrMaxEvents   = (uint16_t)32767  // Default: first boot duration (adjusted at runtime)
};

//*****************************************************************************
//! Functions
//*****************************************************************************

/*********************************************************************
 * @fn      Peripheral_getLastBondedDevice
 *
 * @brief   Get the address of the most recently connected/bonded device
 *          Reads from NV by address if available, otherwise by index
 *
 * @param   pAddr - Pointer to store peer address (6 bytes)
 * @param   pAddrType - Pointer to store address type
 * @param   bondCount - Number of bonds (pass from caller to avoid redundant API calls)
 *
 * @return  TRUE if bond found, FALSE if no bonds exist
 */
static uint8_t Peripheral_getLastBondedDevice(uint8_t *pAddr,
                                              GAP_Peer_Addr_Types_t *pAddrType,
                                              uint8_t bondCount)
{
    if (bondCount == 0) {
        return FALSE;  // No bonds exist
    }

    uint8_t readStatus = FAILURE;

    // Always read by index (most recent bond = index 0)
    readMode = GAPBOND_READ_BY_IDX;
    pIdentifier = 0;
    readStatus = GapBondMgr_readBondFromNV(readMode,
                                            &pIdentifier,  // Pass as pointer
                                            PEER_ADDRTYPE_RANDOM_OR_RANDOM_ID,
                                            pSavedBondRec);

    // If successful, store this address for future use
    if (readStatus == SUCCESS) {
        memcpy(lastPeerDeviceAddr, pSavedBondRec->pBondRec->addr, B_ADDR_LEN);
        lastPeerAddrType = pSavedBondRec->pBondRec->addrType;
    }

    if (readStatus == SUCCESS) {
        memcpy(pAddr, pSavedBondRec->pBondRec->addr, B_ADDR_LEN);
        *pAddrType = pSavedBondRec->pBondRec->addrType;
        return TRUE;
    }

    return FALSE;
}

/*********************************************************************
 * @fn      Peripheral_selectAdvMode
 *
 * @brief   Configure advertising parameters based on mode (directed vs undirected)
 *          Modifies advSetInitParamsSet_1.advParam directly
 *
 * @param   forceUndirected - TRUE to force undirected mode (pairing/reset scenarios)
 * @param   bondCount - Number of existing bonds (pass to avoid redundant API calls)
 *
 * @return  none
 */
static void Peripheral_selectAdvMode(uint8_t forceUndirected, uint8_t bondCount)
{
    // Force undirected for pairing/factory reset scenarios
    if (forceUndirected) {
        currentAdvMode = ADV_MODE_UNDIRECTED;
        // Configure undirected advertising
        advSetInitParamsSet_1.advParam->eventProps = GAP_ADV_PROP_CONNECTABLE | GAP_ADV_PROP_LEGACY | GAP_ADV_PROP_SCANNABLE;
        return;
    }

    // Check if bonds exist for directed advertising
    uint8_t peerAddr[B_ADDR_LEN];
    GAP_Peer_Addr_Types_t peerAddrType;

    if (Peripheral_getLastBondedDevice(peerAddr, &peerAddrType, bondCount)) 
    {
        // Bonds exist - use directed advertising
        currentAdvMode = ADV_MODE_DIRECTED;

        // Configure directed advertising
        advSetInitParamsSet_1.advParam->eventProps = GAP_ADV_PROP_CONNECTABLE | GAP_ADV_PROP_DIRECTED | GAP_ADV_PROP_LEGACY;
        memcpy(advSetInitParamsSet_1.advParam->peerAddr, peerAddr, B_ADDR_LEN);
        advSetInitParamsSet_1.advParam->peerAddrType = peerAddrType;
        return;
    }

    // No bonds - use undirected
    currentAdvMode = ADV_MODE_UNDIRECTED;
    advSetInitParamsSet_1.advParam->eventProps = GAP_ADV_PROP_CONNECTABLE | GAP_ADV_PROP_LEGACY | GAP_ADV_PROP_SCANNABLE;
}

/*********************************************************************
 * @fn      Peripheral_AdvEventHandler
 *
 * @brief   Handle advertising events including auto-restart for OOBE
 *
 * @details OOBE Advertising Strategy (Google ATV spec: 10 minutes):
 *          - BLE stack limitation: max 32767 units (5.46 min) per period
 *          - Solution: Auto-restart advertising once after timeout
 *          - Period 1: 32767 units (327.67s ≈ 5.46 min)
 *          - Period 2: 32767 units (327.67s ≈ 5.46 min)
 *          - Total: 655.34s ≈ 10.92 minutes ✓ (meets 10-min spec)
 *
 * @param   event - message event.
 * @param   pMsgData - pointer to message data.
 *
 * @return  none
 */
void Peripheral_AdvEventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
    switch(event)
    {
        case BLEAPPUTIL_ADV_START_AFTER_ENABLE:
        {
            adv_finished = 0;
            // Note: Don't reset advRestartCount here - it's initialized in Peripheral_start()
            // Resetting here would cause infinite auto-restart loop
            break;
        }

        case BLEAPPUTIL_ADV_END_AFTER_DISABLE:
        {
            adv_finished = 1;

            /* OOBE Auto-restart: If advertising ended due to timeout (not connection),
             * and we haven't exceeded restart limit, restart advertising to meet
             * Google ATV spec's 10-minute requirement.
             *
             * This handles the BLE stack's signed int16_t limitation (max 32767).
             *
             * Don't auto-restart if:
             * - Advertising stopped due to connection (connection handler will restart if needed)
             * - Already exceeded max restart count
             */
            if (!advStoppedByConnection && advRestartCount < ADV_MAX_RESTARTS)
            {
                advRestartCount++;
                // Restart advertising for another 5.46 minutes
                BLEAppUtil_advStart(peripheralAdvHandle_1, &advSetStartParamsSet_1);
            }

            // Clear the connection flag for next cycle
            advStoppedByConnection = 0;
            break;
        }

        default:
        {
            break;
        }
    }
}

/*********************************************************************
 * @fn      Peripheral_GAPConnEventHandler
 *
 * @brief   The purpose of this function is to handle connection related
 *          events that rise from the GAP and were registered in
 *          @ref BLEAppUtil_registerEventHandler
 *
 * @param   event - message event.
 * @param   pMsgData - pointer to message data.
 *
 * @return  none
 */
void Peripheral_GAPConnEventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
    switch(event)
    {
        case BLEAPPUTIL_LINK_ESTABLISHED_EVENT:
        {
            /* Set flag to prevent OOBE auto-restart when advertising stops due to connection */
            advStoppedByConnection = 1;

            /* Save peer device address for later bond reading */
            gapEstLinkReqEvent_t *gapEstMsg = (gapEstLinkReqEvent_t *)pMsgData;
            if (gapEstMsg->hdr.status == SUCCESS)
            {
                // Copy peer's address
                memcpy(lastPeerDeviceAddr, gapEstMsg->devAddr, B_ADDR_LEN);
                // Copy peer's address type (mask to get only address type bits)
                lastPeerAddrType = (GAP_Peer_Addr_Types_t)(gapEstMsg->devAddrType & MASK_ADDRTYPE_ID);
            }

            /* Check if we reach the maximum allowed number of connections */
            if(linkDB_NumActive() < linkDB_NumConns())
            {
                /* Start advertising since there is room for more connections */
                BLEAppUtil_advStart(peripheralAdvHandle_1, &advSetStartParamsSet_1);
            }
            else
            {
                /* Stop advertising since there is no room for more connections */
                BLEAppUtil_advStop(peripheralAdvHandle_1);
            }
            break;
        }

        case BLEAPPUTIL_LINK_TERMINATED_EVENT:
        {
            // When connection terminates (by Central), restart advertising
            // Check if combo key requested forced undirected mode

            // Get bond count
            uint8_t bondCount = 0;
            GAPBondMgr_GetParameter(GAPBOND_BOND_COUNT, &bondCount);

            // Check if MODE_REPAIR requested forced undirected advertising
            uint8_t forceUndirected = forceUndirectedOnNextDisconnect;
            forceUndirectedOnNextDisconnect = 0;  // Clear flag after reading

            // Select mode: force undirected if requested, otherwise auto-select
            // This modifies advParams1 in place
            Peripheral_selectAdvMode(TRUE, bondCount); // CHANGE TO FALSE 

            // Set duration based on mode
            if (currentAdvMode == ADV_MODE_DIRECTED) {
                advSetStartParamsSet_1.durationOrMaxEvents = 9000;  // Infinite for directed
                advRestartCount = ADV_MAX_RESTARTS;  // Disable auto-restart for directed
            } else {
                advSetStartParamsSet_1.durationOrMaxEvents = 9000;  // 90s for undirected
                advRestartCount = 0;  // Enable auto-restart if needed
            }

            // Reload advertising set to update all parameters including peer address
            BLEAppUtil_initAdvSet(&peripheralAdvHandle_1, &advSetInitParamsSet_1);

            // Restart advertising
            BLEAppUtil_advStart(peripheralAdvHandle_1, &advSetStartParamsSet_1);
            break;
        }

        default:
        {
            break;
        }
    }
}

/*********************************************************************
 * @fn      Peripheral_startPairingModeAdvertising
 *
 * @brief   Start advertising in undirected mode for pairing/re-pairing
 *          Called by combo key handlers for MODE_REPAIR and MODE_CLEARBOND
 *
 * @note    This function:
 *          - Stops current advertising (if any)
 *          - Forces undirected advertising mode
 *          - Starts 90-second advertising window for pairing
 *          - Safe to call anytime, regardless of current advertising state
 *
 * @return  none
 */
void Peripheral_startPairingModeAdvertising(void)
{
    // Stop current advertising if active (safe to call even if already stopped)
    BLEAppUtil_advStop(peripheralAdvHandle_1);

    // Get bond count (should be 0 after factory reset, may have bonds for repair mode)
    uint8_t bondCount = 0;
    GAPBondMgr_GetParameter(GAPBOND_BOND_COUNT, &bondCount);

    // Force undirected mode (TRUE = force undirected, even if bonds exist)
    // This modifies advParams1 in place
    Peripheral_selectAdvMode(TRUE, bondCount);

    // Reload advertising set to update all parameters
    BLEAppUtil_initAdvSet(&peripheralAdvHandle_1, &advSetInitParamsSet_1);

    // Use pairing mode duration (90 seconds as defined in key_scan.c line 173)
    BLEAppUtil_AdvStart_t pairingAdvParams = {
        .enableOptions = GAP_ADV_ENABLE_OPTIONS_USE_DURATION,
        .durationOrMaxEvents = 9000  // 90 seconds
    };

    // Start advertising in undirected mode
    BLEAppUtil_advStart(peripheralAdvHandle_1, &pairingAdvParams);
}

/*********************************************************************
 * @fn      Peripheral_start
 *
 * @brief   This function is called after stack initialization,
 *          the purpose of this function is to initialize and
 *          register the specific events handlers of the peripheral
 *          application module
 *
 * @return  SUCCESS, errorInfo
 */
bStatus_t Peripheral_start()
{
    bStatus_t status = SUCCESS;

    status = BLEAppUtil_registerEventHandler(&peripheralConnHandler);
    if(status != SUCCESS)
    {
        // Return status value
        return(status);
    }

    status = BLEAppUtil_registerEventHandler(&peripheralAdvHandler);
    if(status != SUCCESS)
    {
        return(status);
    }

    /* OOBE & Directed Advertising: Adjust advertising mode and duration based on bond count
     * - First boot (no bonds): Undirected advertising, 32767 units with auto-restart = ~10.9 min total
     * - Normal boot (has bonds): Directed advertising to last bonded device, infinite duration
     *
     * Google ATV spec requires 10 minutes on first boot for pairing,
     * but directed advertising is used after successful pairing for faster reconnection.
     */
    uint8_t bondCount = 0;
    GAPBondMgr_GetParameter(GAPBOND_BOND_COUNT, &bondCount);

    // Select advertising parameters based on bond count
    // This modifies advParams1 in place
    if (bondCount == 0)
    {
        // First boot: Force undirected advertising (OOBE)
        Peripheral_selectAdvMode(TRUE, bondCount);  // Force undirected
        advSetStartParamsSet_1.durationOrMaxEvents = 32767;  // ~10.9 min with auto-restart
        advRestartCount = 0;  // Enable auto-restart (allow 1 restart)

        // Reload advertising set to update all parameters including peer address
        BLEAppUtil_initAdvSet(&peripheralAdvHandle_1, &advSetInitParamsSet_1);
    }
    else
    {
        // Has bonds: Use directed advertising to last bonded device
        Peripheral_selectAdvMode(TRUE, bondCount);  // Auto-select based on bonds // CHANGE TO FALSE 
        advSetStartParamsSet_1.durationOrMaxEvents = 9000;  // only for 90 seconds.
        advRestartCount = ADV_MAX_RESTARTS;  // Disable auto-restart

        // Reload advertising set to update all parameters including peer address
        BLEAppUtil_initAdvSet(&peripheralAdvHandle_1, &advSetInitParamsSet_1);

    }

    // Start advertising
    status = BLEAppUtil_advStart(peripheralAdvHandle_1, &advSetStartParamsSet_1);
    if(status != SUCCESS)
    {
        // Return status value
        return(status);
    }

    // Return status value
    return(status);
}

#endif // ( HOST_CONFIG & ( PERIPHERAL_CFG ) )
