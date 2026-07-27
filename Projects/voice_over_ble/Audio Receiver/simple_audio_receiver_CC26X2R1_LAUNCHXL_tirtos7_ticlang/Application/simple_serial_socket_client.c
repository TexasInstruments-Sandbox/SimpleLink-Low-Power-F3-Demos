/******************************************************************************

 @file       simple_serial_socket_client.c

 @brief This file contains the Simple Serial Socket client sample application
 for use  with the CC1352 / CC26X2 Bluetooth Low Energy Protocol Stack.

 Group: CMCU, LPRF
 Target Device: CC1352, CC26X2

 ******************************************************************************

 Copyright (c) 2018-2021, Texas Instruments Incorporated
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

 ******************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include <stdbool.h>
#include <string.h>

#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Queue.h>

#include <ti/drivers/UART2.h>
#include <ti/drivers/uart2/UART2CC26X2.h>
#include <ti/drivers/utils/List.h>

#include <ti_drivers_config.h>
#include "ti_ble_config.h"

#include <icall.h>
/* This Header file contains all BLE API and icall structure definition */
#include "icall_ble_api.h"
#include "ble_user_config.h"
#include "util.h"
#include <ti/display/Display.h>
#include <ti/drivers/apps/LED.h>

#include "simple_serial_socket_client.h"
#include "simple_stream_profile_client.h"
#include "simple_service_discovery.h"

#include "audio_player.h"

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

// Application events
#define SSSC_EVT_KEY_CHANGE                     0x01
#define SSSC_EVT_SCAN_ENABLED                   0x02
#define SSSC_EVT_SCAN_DISABLED                  0x03
#define SSSC_EVT_ADV_REPORT                     0x04
#define SSSC_EVT_INSUFFICIENT_MEM               0x05
#define SSSC_EVT_CONN                           0x06
#define SSSC_EVT_OUTGOING_DATA                  0x07

// Simple Serial Socket Client Task Events
#define SSSC_ICALL_EVT                         ICALL_MSG_EVENT_ID // Event_Id_31
#define SSSC_QUEUE_EVT                         UTIL_QUEUE_EVENT_ID // Event_Id_30
#define SSSC_START_DISCOVERY_EVT               Event_Id_00
#define SSSC_NOTI_ENABLE_EVT                   Event_Id_01

#define SSSC_ALL_EVENTS                        (SSSC_ICALL_EVT           | \
                                               SSSC_QUEUE_EVT            | \
                                               SSSC_START_DISCOVERY_EVT  | \
                                               SSSC_NOTI_ENABLE_EVT)

// Maximum number of scan responses
#define DEFAULT_MAX_SCAN_RES                  30

// Default service discovery timer delay in ms
#define DEFAULT_SVC_DISCOVERY_DELAY           1000

// Task configuration
#define SSSC_TASK_PRIORITY                     1

#ifndef SSSC_TASK_STACK_SIZE
#define SSSC_TASK_STACK_SIZE                   2000
#endif

// Set the register cause to the registration bit-mask
#define CONNECTION_EVENT_REGISTER_BIT_SET(RegisterCause) (connectionEventRegisterCauseBitMap |= RegisterCause )
// Remove the register cause from the registration bit-mask
#define CONNECTION_EVENT_REGISTER_BIT_REMOVE(RegisterCause) (connectionEventRegisterCauseBitMap &= (~RegisterCause) )
// Gets whether the current App is registered to the receive connection events
#define CONNECTION_EVENT_IS_REGISTERED (connectionEventRegisterCauseBitMap > 0)
// Gets whether the RegisterCause was registered to recieve connection event
#define CONNECTION_EVENT_REGISTRATION_CAUSE(RegisterCause) (connectionEventRegisterCauseBitMap & RegisterCause )

// UART read buffer size
#define UART_MAX_READ_SIZE    (10) //256 --> reduced for Audio

#ifndef CONFIG_LEDCOUNT
#define CONFIG_LEDCOUNT        2
#else
#define CONFIG_LED2            2
#endif

#define SSSS_DEVICE_IDENTIFIER_LENGTH 2
#define SSSC_DEVICE_IDENTIFIER_NULL   "xx"
#define SSSC_DEVICE_IDENTIFIER_LENGTH 3
#define SSSC_DEVICE_IDENTIFIER_ALL    "ALL"

// Application states
enum
{
    BLE_STATE_IDLE,
    BLE_STATE_CONNECTING,
    BLE_STATE_CONNECTED,
    BLE_STATE_CONNECTED_DISCOVERY_TO_BE_TRIGERED,
    BLE_STATE_CONNECTED_NOTIFICATIONS_TO_BE_ENABLED,
    BLE_STATE_DISCONNECTING
};

/*********************************************************************
 * TYPEDEFS
 */

// App event passed from profiles.
typedef struct
{
    appEvtHdr_t hdr;    // event header.
    uint8_t *pData;  // event data
    uint16_t arg0;    // Generic argument
} ssscEvt_t;

// UART Write list element
typedef struct
{
    List_Elem elem;
    uint8_t len;
    uint8_t buffer[];
} uartMsg_t;

// Connected device information
typedef struct
{
  uint16_t connHandle;        // Connection Handle
  uint16_t charHandle;        // Characteristic Handle
  Bool discoveryDone;         // Service discovery status
  uint8_t  addr[B_ADDR_LEN];  // Peer Device Address
  uint8_t  state;             // Connection state
  char  deviceIdentifier[SSSS_DEVICE_IDENTIFIER_LENGTH];  // Peer device local name
  Clock_Struct *startDiscClock;// Clock for service discovery
  Clock_Struct *startNotiEnableClock; // Clock for notification enabling

} connRec_t;


// The following typedef is used for registration to connection event
typedef enum
{
    NOT_REGISTER = 0, 
	FOR_STREAM = 1
} connectionEventRegisterCause_u;

/*********************************************************************
 * GLOBAL VARIABLES
 */

// Display Interface
Display_Handle dispHandle = NULL;
LED_Handle ledHandle[2];

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

// Number of connected devices
static uint8_t numConn = 0;

// List of connections
connRec_t connList[MAX_NUM_BLE_CONNS];

// Handle the registration and un-registration for the connection event, since only one can be registered.
static uint32_t connectionEventRegisterCauseBitMap = NOT_REGISTER; //see connectionEventRegisterCause_u

// Entity ID globally used to check for source and/or destination of messages
static ICall_EntityID selfEntity;

// Event globally used to post local events and pend on system and
// local events.
static ICall_SyncHandle syncEvent;

// Queue object used for app messages
static Queue_Struct appMsg;
static Queue_Handle appMsgQueue;

// Task configuration
Task_Struct ssscTask;
Char ssscTaskStack[SSSC_TASK_STACK_SIZE];

// Scanning state
static bool scanningStarted = FALSE;

// Maximum PDU size (default = 27 octets)
static uint16_t ssscMaxPduSize;

// Server manufacturer data: 0x0D, 0x00 (TI) : 0xC0, 0x00, 0xFF, 0xEE (COFFEE)
static const uint8_t serverManufData[6] = { 0x0D, 0x00, 0xC0, 0xFF, 0xEE };

// UART Interface
static UART2_Handle uartHandle = NULL;

// UART read buffer
static uint8_t uartReadBuffer[UART_MAX_READ_SIZE] = { 0 };

// UART write list
static List_List uartWriteList;
static uartMsg_t *uartCurrentMsg;
static uint8_t uartWriteActive = 0;

// UART local cleaned copy of the receive buffer
static uint16_t localUartBufferLength = 0;
static uint8_t localUartBuffer[UART_MAX_READ_SIZE] = { 0 };

/*********************************************************************
 * LOCAL FUNCTIONS
 */
//static void uartReadCallback(UART2_Handle handle, void *rxBuf, size_t size, void *userArg, int_fast16_t status);
//static void uartWriteCallback(UART2_Handle handle, void *rxBuf, size_t size, void *userArg, int_fast16_t status);

static void SimpleSerialSocketClient_init(void);
static void SimpleSerialSocketClient_taskFxn(UArg a0, UArg a1);

static void SimpleSerialSocketClient_processStackMsg(ICall_Hdr *pMsg);
static void SimpleSerialSocketClient_processAppMsg(ssscEvt_t *pMsg);
static void SimpleSerialSocketClient_processGapMsg(gapEventHdr_t *pMsg);
static void SimpleSerialSocketClient_processGATTMsg(gattMsgEvent_t *pMsg);
static void SimpleSerialSocketClient_processConnEvt(
        Gap_ConnEventRpt_t *pReport);
static void SimpleSerialSocketClient_processGATTDiscEvent(gattMsgEvent_t *pMsg);

static void SimpleSerialSocketClient_startDiscovery(void);
static bool SimpleSerialSocketClient_findSerialStreamServer(
        const uint8_t *uuid, const uint8_t *pManData, uint8_t manDataLen,
        uint8_t *pData, uint8_t dataLen);
static void SimpleSerialSocketClient_genericClockHandler(UArg a0);
static void SimpleSerialSocketClient_connEvtCB(Gap_ConnEventRpt_t *pReport);
static uint8_t SimpleSerialSocketClient_enqueueMsg(uint8_t event,
                                                   uint8_t status,
                                                   uint8_t *pData,
                                                   uint16_t arg0);
static void SimpleSerialSocketClient_scanCb(uint32_t evt, void *msg,
                                            uintptr_t arg);
static uint8_t SimpleSerialSocketClient_addConnInfo(uint16_t connHandle, uint8_t *pAddr);
static uint8_t SimpleSerialSocketClient_removeConnInfo(uint16_t connHandle);

/*********************************************************************
* PUBLIC FUNCTIONS
*/
uint8_t SimpleSerialSocketClient_getConnIndex(uint16_t connHandle);

/*********************************************************************
 * EXTERN FUNCTIONS
 */
extern void AssertHandler(uint8 assertCause, uint8 assertSubcause);

/*********************************************************************
 * PROFILE CALLBACKS
 */

/*********************************************************************
 * DRIVER CALLBACKS
 */

/*********************************************************************
 * @fn      uartReadCallback
 *
 * @brief   UART read callback, notify the application that we have new data to handle
 *
 * @param   handle  - UART handle
 *          *rxBuf  - buffer containing the incoming data.
 *          size    - length of the data buffer.
 *
 * @return  None.
 */
static void uartReadCallback(UART2_Handle handle, void *rxBuf, size_t size, void *userArg, int_fast16_t status)
{

    char ch;
    Bool eolDetected = FALSE;

    // This code is to emulate a UART return on new line with echoing of each character
    if(UART_MAX_READ_SIZE < (size + localUartBufferLength))
    {
        // Overflow
        localUartBufferLength = 0;
        return;
    }

    int i;
    for(i=0; i <= size; i++)
    {
        ch = (char)(uartReadBuffer[i]);

        if((ch == '\r') || (ch =='\n'))
        {
            // End of line detected
            eolDetected = TRUE;
            break;
        }
        else
        {
            localUartBuffer[localUartBufferLength] = ch;
            localUartBufferLength++;
            //UART2_write(handle, &ch, 1, NULL); //echo the received character
        }

    }

    if(eolDetected)
    {
        // Pass the number of read bytes using the arg0 field
        SimpleSerialSocketClient_enqueueMsg(SSSC_EVT_OUTGOING_DATA, 0, 0,
                                            localUartBufferLength);
        localUartBufferLength = 0;
    }
}

/*********************************************************************
 * @fn      uartWriteCallback
 *
 * @brief   Process more data if available in the UART write list
 *
 * @param   handle  - UART handle
 *          *txBuf  - buffer containing the written data.
 *          size    - length of the data buffer written.
 *
 * @return  None.
 */
static void uartWriteCallback(UART2_Handle handle, void *txBuf, size_t size, void *userArg, int_fast16_t status)
{
    // Free the last printed buffer
    if (NULL != uartCurrentMsg)
    {
        ICall_free(uartCurrentMsg);
        uartCurrentMsg = NULL;
    }

    // If the list is not empty, start another write
    if (!List_empty(&uartWriteList))
    {
        uartCurrentMsg = (uartMsg_t*) List_get(&uartWriteList);
        //UART2_write(handle, uartCurrentMsg->buffer, uartCurrentMsg->len, NULL);
    }
    else
    {
        uartWriteActive = 0;
    }
}

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      SimpleSerialSocketClient_RegistertToAllConnectionEvent()
 *
 * @brief   register to receive connection events for all the connection
 *
 * @param connectionEventRegisterCause represents the reason for registration
 *
 * @return @ref SUCCESS
 *
 */
bStatus_t SimpleSerialSocketClient_RegisterToAllConnectionEvent(
        connectionEventRegisterCause_u connectionEventRegisterCause)
{
    bStatus_t status = SUCCESS;

    // in case  there is no registration for the connection event, make the registration
    if (!CONNECTION_EVENT_IS_REGISTERED)
    {
        status = Gap_RegisterConnEventCb(SimpleSerialSocketClient_connEvtCB,
                                         GAP_CB_REGISTER,
                                         LINKDB_CONNHANDLE_ALL);
    }
    if (status == SUCCESS)
    {
        //add the reason bit to the bitamap.
        CONNECTION_EVENT_REGISTER_BIT_SET(connectionEventRegisterCause);
    }

    return (status);
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_UnRegistertToAllConnectionEvent()
 *
 * @brief   Unregister connection events
 *
 * @param connectionEventRegisterCause represents the reason for registration
 *
 * @return @ref SUCCESS
 *
 */
bStatus_t SimpleSerialSocketClient_UnRegisterToAllConnectionEvent(
        connectionEventRegisterCause_u connectionEventRegisterCause)
{
    bStatus_t status = SUCCESS;

    CONNECTION_EVENT_REGISTER_BIT_REMOVE(connectionEventRegisterCause);
    // in case  there is no more registration for the connection event than unregister
    if (!CONNECTION_EVENT_IS_REGISTERED)
    {
        Gap_RegisterConnEventCb(SimpleSerialSocketClient_connEvtCB,
                                GAP_CB_UNREGISTER, LINKDB_CONNHANDLE_ALL);
    }

    return (status);
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_createTask
 *
 * @brief   Task creation function for the Simple Serial Socket Client.
 *
 * @param   none
 *
 * @return  none
 */
void SimpleSerialSocketClient_createTask(void)
{
    Task_Params taskParams;

    // Configure task
    Task_Params_init(&taskParams);
    taskParams.stack = ssscTaskStack;
    taskParams.stackSize = SSSC_TASK_STACK_SIZE;
    taskParams.priority = SSSC_TASK_PRIORITY;

    Task_construct(&ssscTask, SimpleSerialSocketClient_taskFxn, &taskParams,
                   NULL);
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_Init
 *
 * @brief   Initialization function for the Simple Serial Socket Client App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notification).
 *
 * @param   none
 *
 * @return  none
 */
static void SimpleSerialSocketClient_init(void)
{
    // ******************************************************************
    // N0 STACK API CALLS CAN OCCUR BEFORE THIS CALL TO ICall_registerApp
    // ******************************************************************
    // Register the current thread as an ICall dispatcher application
    // so that the application can send and receive messages.
    ICall_registerApp(&selfEntity, &syncEvent);

    // Create an RTOS queue for message from profile to be sent to app.
    appMsgQueue = Util_constructQueue(&appMsg);

    // Initialize internal data
    int i;
    for (i = 0; i < MAX_NUM_BLE_CONNS; i++)
    {
      connList[i].connHandle = LINKDB_CONNHANDLE_INVALID;
      connList[i].startDiscClock = NULL;
      connList[i].startNotiEnableClock = NULL;
      connList[i].state = BLE_STATE_IDLE;
      connList[i].discoveryDone = FALSE;
      memcpy(connList[i].deviceIdentifier, SSSC_DEVICE_IDENTIFIER_NULL, SSSS_DEVICE_IDENTIFIER_LENGTH);
    }

    // Initialize simple stream client
    SimpleStreamClient_init();

    // Initialize simple service discovery
    SimpleServiceDiscovery_init();

    // Initialize and open PIN driver to use LEDs
    LED_Params ledParams;
    LED_init();
    LED_Params_init(&ledParams);
    ledHandle[CONFIG_LED_0] = LED_open(CONFIG_LED_0, &ledParams);
    ledHandle[CONFIG_LED_1] = LED_open(CONFIG_LED_1, &ledParams);

    // Setup an initial read
    //UART2_read(uartHandle, uartReadBuffer, UART_MAX_READ_SIZE, NULL);

    // Make sure to leave at least MIN_HEAP_HEADROOM of heap
    // for the BLE stack application to operate.
    SimpleStreamClient_setHeadroomLimit(4000);

    // Initialize GATT attributes
    GGS_SetParameter(GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN,
                     (void* )attDeviceName);

    //Set default values for Data Length Extension
    //Extended Data Length Feature is already enabled by default
    //in build_config.opt in stack project.
    {
        //Change initial values of RX/TX PDU and Time, RX is set to max. by default(251 octets, 2120us)
#define APP_SUGGESTED_RX_PDU_SIZE 251     //default is 251 octets(RX)
#define APP_SUGGESTED_RX_TIME     17000   //default is 17000us(RX)
#define APP_SUGGESTED_TX_PDU_SIZE 27      //default is 27 octets(TX)
#define APP_SUGGESTED_TX_TIME     328     //default is 328us(TX)

        //This API is documented in hci.h
        //See the LE Data Length Extension section in the BLE5-Stack User's Guide for information on using this command:
        //http://software-dl.ti.com/lprf/ble5stack-latest/
        HCI_EXT_SetMaxDataLenCmd(APP_SUGGESTED_TX_PDU_SIZE,
                                 APP_SUGGESTED_TX_TIME,
                                 APP_SUGGESTED_RX_PDU_SIZE,
                                 APP_SUGGESTED_RX_TIME);
    }

    // Initialize GATT Client
    VOID GATT_InitClient("");

    // Register to receive incoming ATT Indications/Notifications
    GATT_RegisterForInd(selfEntity);

    // Initialize GATT attributes
    GGS_AddService(GATT_ALL_SERVICES);// GAP
    GATTServApp_AddService(GATT_ALL_SERVICES); // GATT attributes

    // Register with GAP for HCI/Host messages (for RSSI)
    GAP_RegisterForMsgs(selfEntity);

    // Register for GATT local events and ATT Responses pending for transmission
    GATT_RegisterForMsgs(selfEntity);

    // Accept all parameter update requests
    GAP_SetParamValue(GAP_PARAM_LINK_UPDATE_DECISION,
                      GAP_UPDATE_REQ_ACCEPT_ALL);

    // Initialize GAP layer for Central role and register to receive GAP events
    GAP_DeviceInit(GAP_PROFILE_CENTRAL, selfEntity, ADDRMODE_RP_WITH_PUBLIC_ID,
                   NULL);



    AudioPlayer_init();
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_taskFxn
 *
 * @brief   Application task entry point for the Simple Serial Socket Client.
 *
 * @param   none
 *
 * @return  events not processed
 */
static void SimpleSerialSocketClient_taskFxn(UArg a0, UArg a1)
{
    // Initialize application
    SimpleSerialSocketClient_init();

    // Application main loop
    for (;;)
    {
        uint32_t events;

        events = Event_pend(syncEvent, Event_Id_NONE, SSSC_ALL_EVENTS,
        ICALL_TIMEOUT_FOREVER);

        if (events)
        {
            ICall_EntityID dest;
            ICall_ServiceEnum src;
            ICall_HciExtEvt *pMsg = NULL;

            if (ICall_fetchServiceMsg(&src, &dest,
                                      (void**) &pMsg) == ICALL_ERRNO_SUCCESS)
            {
                if ((src == ICALL_SERVICE_CLASS_BLE) && (dest == selfEntity))
                {
                    // Process inter-task message
                    SimpleSerialSocketClient_processStackMsg((ICall_Hdr*) pMsg);
                }

                if (pMsg)
                {
                    ICall_freeMsg(pMsg);
                }
            }

            // If RTOS queue is not empty, process app message
            if (events & SSSC_QUEUE_EVT)
            {
                while (!Queue_empty(appMsgQueue))
                {
                    ssscEvt_t *pMsg = (ssscEvt_t*) Util_dequeueMsg(appMsgQueue);
                    if (pMsg)
                    {
                        // Process message
                        SimpleSerialSocketClient_processAppMsg(pMsg);

                        // Free the space from the message
                        ICall_free(pMsg);
                    }
                }
            }

            if (events & SSSC_START_DISCOVERY_EVT)
            {
                SimpleSerialSocketClient_startDiscovery();
            }

            // Enable stream notifications
            if (events & SSSC_NOTI_ENABLE_EVT)
            {
                int connIndex;
                for(connIndex = 0; connIndex < MAX_NUM_BLE_CONNS; connIndex++)
                {
                    if(connList[connIndex].state == BLE_STATE_CONNECTED_NOTIFICATIONS_TO_BE_ENABLED)
                    {
                        // Enable notification for incoming data
                        bStatus_t retVal = SimpleStreamClient_enableNotifications(connList[connIndex].connHandle);

                        if (retVal == SUCCESS)
                        {
                            connList[connIndex].state = BLE_STATE_CONNECTED;

                            // Consider notifications active
                            // (This might not be the case, we would have to read them back to make sure)
                            // Green LED indicates notifications is enabled
                            LED_setOn(ledHandle[CONFIG_LED_1], 100);
                        }
                    }
                }
            }
        }
    }
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_processStackMsg
 *
 * @brief   Process an incoming task message.
 *
 * @param   pMsg - message to process
 *
 * @return  none
 */
static void SimpleSerialSocketClient_processStackMsg(ICall_Hdr *pMsg)
{
    hciEvt_HardwareError_t *hardCode;

    switch (pMsg->event)
    {
    case GAP_MSG_EVENT:
        SimpleSerialSocketClient_processGapMsg((gapEventHdr_t*) pMsg);
        break;

    case GATT_MSG_EVENT:
        SimpleSerialSocketClient_processGATTMsg((gattMsgEvent_t*) pMsg);
        break;

    case HCI_GAP_EVENT_EVENT:
    {

        // Process HCI message
        switch (pMsg->status)
        {
        case HCI_BLE_HARDWARE_ERROR_EVENT_CODE:
            hardCode = (hciEvt_HardwareError_t*) pMsg;
            AssertHandler(HAL_ASSERT_CAUSE_HARDWARE_ERROR,
                          hardCode->hardwareCode);
            break;

        default:
            break;
        }
    }
        break;

    default:
        break;
    }
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_processAppMsg
 *
 * @brief   Central application event processing function.
 *
 * @param   pMsg - pointer to event structure
 *
 * @return  none
 */
static void SimpleSerialSocketClient_processAppMsg(ssscEvt_t *pMsg)
{
    bool safeToDealloc = TRUE;

    switch (pMsg->hdr.event)
    {
    case SSSC_EVT_ADV_REPORT:
    {

        GapScan_Evt_AdvRpt_t *pAdvRpt = (GapScan_Evt_AdvRpt_t*) (pMsg->pData);
        if (SimpleSerialSocketClient_findSerialStreamServer(
                SimpleStreamServerUUID, serverManufData,
                sizeof(serverManufData), pAdvRpt->pData, pAdvRpt->dataLen))
        {
            // We found our one of devices, establish a connection to the device
            GapInit_connect(pAdvRpt->addrType & MASK_ADDRTYPE_ID, pAdvRpt->addr,
                            INIT_PHY_1M, 0);
        }

        // Free report payload data
        if (pAdvRpt->pData != NULL)
        {
            ICall_free(pAdvRpt->pData);
        }

        break;
    }

    case SSSC_EVT_SCAN_DISABLED:
    {
        // If the device was not found, continue to scan for it
        if (scanningStarted == TRUE)
        {
            // Re-enable scan
            GapScan_enable(0, DEFAULT_SCAN_DURATION, DEFAULT_MAX_SCAN_RES);
        }
        break;
    }

    case SSSC_EVT_OUTGOING_DATA:
    {
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // The code below performs data interpretation before sending out
        // Bluetooth messages.
        // If you want to disable this and directly send data to all the
        // connected devices, leverage the code snippet available below.
        //
        // Note: Additional Task_sleep may be required if a large number
        //       (>8) of devices is connected.
        //
        // int connIndex;
        // for(connIndex = 0; connIndex < MAX_NUM_BLE_CONNS; connIndex++)
        // {
        //     if(connList[connIndex].state == BLE_STATE_CONNECTED)
        //     {
        //         bStatus_t status = SimpleStreamClient_sendData(connList[connIndex].connHandle,
        //                                                        &localUartBuffer,
        //                                                        pMsg->arg0);
        //
        //         // If status is not successful, register for connection events for further processing
        //         if (status != SUCCESS)
        //         {
        //             SimpleSerialSocketClient_RegisterToAllConnectionEvent(FOR_STREAM);
        //         }
        //     }
        // }

        // Start another read
        //UART2_read(uartHandle, uartReadBuffer, UART_MAX_READ_SIZE, NULL);


        if((pMsg->arg0 > SSSS_DEVICE_IDENTIFIER_LENGTH) &&
           (localUartBuffer[SSSS_DEVICE_IDENTIFIER_LENGTH] == ':'))
        {
            // UART data is for one one the device connected
            int connIndex;
            bool deviceFound = FALSE;
            for(connIndex = 0; connIndex < MAX_NUM_BLE_CONNS; connIndex++)
            {
                if((connList[connIndex].state == BLE_STATE_CONNECTED) &&
                  !(memcmp(localUartBuffer, connList[connIndex].deviceIdentifier, SSSS_DEVICE_IDENTIFIER_LENGTH)))
                {

                    deviceFound = TRUE;
                    bStatus_t status = SimpleStreamClient_sendData(connList[connIndex].connHandle,
                                                                   &localUartBuffer[SSSS_DEVICE_IDENTIFIER_LENGTH+1],
                                                                   pMsg->arg0 - SSSS_DEVICE_IDENTIFIER_LENGTH -1);

                    // If status is not successful, register for connection events for further processing
                    if (status != SUCCESS)
                    {
                        SimpleSerialSocketClient_RegisterToAllConnectionEvent(FOR_STREAM);
                    }

                    // Display
                    if(deviceFound)
                    {
                        uint8_t preprint[] = "(xx)<<";
                        memcpy(&preprint[1], connList[connIndex].deviceIdentifier, SSSS_DEVICE_IDENTIFIER_LENGTH);
                        //UART2_write(uartHandle, preprint, sizeof(preprint), NULL);
                        //UART2_write(uartHandle, &localUartBuffer[SSSS_DEVICE_IDENTIFIER_LENGTH+1],
                                    //pMsg->arg0 - SSSS_DEVICE_IDENTIFIER_LENGTH - 1,
                                    //NULL);
                        //UART2_write(uartHandle, "\n\r", 2, NULL);

                    }

                    break;
                }
            }

            // Display
            if(!deviceFound)
            {
                uint8_t notFounddMessage[] = "Info: Device not found - Data not sent\n\r";
                //UART2_write(uartHandle, notFounddMessage, sizeof(notFounddMessage), NULL);
            }
        }
        else if(pMsg->arg0 > SSSC_DEVICE_IDENTIFIER_LENGTH &&
                localUartBuffer[SSSC_DEVICE_IDENTIFIER_LENGTH] == ':')
        {
            // UART data is for all the devices connected
            uint8_t preprint[] = "ALL <<";
            //UART2_write(uartHandle, preprint, sizeof(preprint), NULL);
            //UART2_write(uartHandle, &localUartBuffer[SSSC_DEVICE_IDENTIFIER_LENGTH+1],
                        //pMsg->arg0 - SSSC_DEVICE_IDENTIFIER_LENGTH - 1,
                        //NULL);
            //UART2_write(uartHandle, "\n\r", 2, NULL);

            Task_sleep(1200);

            int connIndex;
            for(connIndex = 0; connIndex < MAX_NUM_BLE_CONNS; connIndex++)
            {
                if(connList[connIndex].state == BLE_STATE_CONNECTED)
                {
                    Task_sleep(1200);

                    bStatus_t status = SimpleStreamClient_sendData(connList[connIndex].connHandle,
                                                                   &localUartBuffer[SSSC_DEVICE_IDENTIFIER_LENGTH+1],
                                                                   pMsg->arg0 - SSSC_DEVICE_IDENTIFIER_LENGTH - 1);

                    Task_sleep(1200);

                    if(status == MSG_BUFFER_NOT_AVAIL)
                    {
                        uint8_t err_print[] = "(xx)XX Err: MSG_BUFFER_NOT_AVAIL\n\r";
                        memcpy(&err_print[1], connList[connIndex].deviceIdentifier, SSSS_DEVICE_IDENTIFIER_LENGTH);
                        //UART2_write(uartHandle, err_print, sizeof(err_print), NULL);
                    }
                    else if(status == bleMemAllocError)
                    {
                        uint8_t err_print[] = "(xx)XX Err: bleMemAllocError\n\r";
                        memcpy(&err_print[1], connList[connIndex].deviceIdentifier, SSSS_DEVICE_IDENTIFIER_LENGTH);
                        //UART2_write(uartHandle, err_print, sizeof(err_print), NULL);
                    }
                    else if(status == blePending)
                    {
                        uint8_t err_print[] = "(xx)XX Err: blePending\n\r";
                        memcpy(&err_print[1], connList[connIndex].deviceIdentifier, SSSS_DEVICE_IDENTIFIER_LENGTH);
                        //UART2_write(uartHandle, err_print, sizeof(err_print), NULL);
                    }

                    // If status is not successful, register for connection events for further processing
                    if (status != SUCCESS)
                    {
                        SimpleSerialSocketClient_RegisterToAllConnectionEvent(FOR_STREAM);
                    }
                }
            }
        }
        else if(pMsg->arg0 == (sizeof("PRINT_CONNECTION_TABLE") - 1) && !memcmp(localUartBuffer, "PRINT_CONNECTION_TABLE", (sizeof("PRINT_CONNECTION_TABLE") - 1)))
        {

            uint8_t table_print_l0[] = "\n\r -PRINT_CONNECTION_TABLE-\n\r";
            uint8_t table_print_l1[] = "____________________________\n\r";
            uint8_t table_print_l2[] = "| NAME | BLUETOOTH ADDRESS |\n\r";
            uint8_t table_print_l3[] = "|======|===================|\n\r";
            //UART2_write(uartHandle, table_print_l0, sizeof(table_print_l0), NULL);
            //UART2_write(uartHandle, table_print_l1, sizeof(table_print_l1), NULL);
            //UART2_write(uartHandle, table_print_l2, sizeof(table_print_l2), NULL);
            //UART2_write(uartHandle, table_print_l3, sizeof(table_print_l2), NULL);

            Task_sleep(500);

            int connIndex;
            for(connIndex = 0; connIndex < MAX_NUM_BLE_CONNS; connIndex++)
            {
                if(connList[connIndex].state == BLE_STATE_CONNECTED)
                {
                    const uint8_t BLE_ADDR_STR_SIZE = 14;// Size of string-converted device address ("0xXXXXXXXXXXXX")
                    uint8_t table_print_lx[] = "|  xx  |  0x0123456789AB   |\n\r";
                    memcpy(&table_print_lx[3], connList[connIndex].deviceIdentifier, SSSS_DEVICE_IDENTIFIER_LENGTH);
                    memcpy(&table_print_lx[10], Util_convertBdAddr2Str(connList[connIndex].addr), BLE_ADDR_STR_SIZE);
                    //UART2_write(uartHandle, table_print_lx, sizeof(table_print_lx), NULL);
                }
                Task_sleep(100);

            }

            uint8_t table_print_last[] = "----------------------------\n\r";
            //UART2_write(uartHandle, table_print_last, sizeof(table_print_last), NULL);
        }
        else
        {
            // Invalid data received from UART
            uint8_t invalidMessage[] = "Info: Invalid format. Should be \"xx:message\".\n\r";
            //UART2_write(uartHandle, invalidMessage, sizeof(invalidMessage), NULL);
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // Start another read
        //UART2_read(uartHandle, uartReadBuffer, UART_MAX_READ_SIZE, NULL);

        break;
    }

        // Connection event
    case SSSC_EVT_CONN:
    {
        SimpleSerialSocketClient_processConnEvt(
                (Gap_ConnEventRpt_t*) (pMsg->pData));

        ICall_free(pMsg->pData);
        break;
    }

    default:
        // Do nothing.
        break;
    }

    if ((safeToDealloc == TRUE) && (pMsg->pData != NULL))
    {
        ICall_free(pMsg->pData);
    }
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_processRoleEvent
 *
 * @brief   Central role event processing function.
 *
 * @param   pEvent - pointer to event structure
 *
 * @return  none
 */
static void SimpleSerialSocketClient_processGapMsg(gapEventHdr_t *pMsg)
{
    switch (pMsg->opcode)
    {
    case GAP_DEVICE_INIT_DONE_EVENT:
    {
        uint8_t temp8;
        uint16_t temp16;
        gapDeviceInitDoneEvent_t *pPkt = (gapDeviceInitDoneEvent_t*) pMsg;

        // Setup scanning
        // For more information, see the GAP section in the User's Guide:
        // http://software-dl.ti.com/lprf/ble5stack-latest/

        // Register callback to process Scanner events
        GapScan_registerCb(SimpleSerialSocketClient_scanCb, NULL);

        // Set Scanner Event Mask
        GapScan_setEventMask(
                GAP_EVT_SCAN_ENABLED | GAP_EVT_SCAN_DISABLED | GAP_EVT_ADV_REPORT);

        // Set Scan PHY parameters
        GapScan_setPhyParams(SCAN_PRIM_PHY_1M, SCAN_TYPE_ACTIVE,
                             DEFAULT_SCAN_INTERVAL, DEFAULT_SCAN_INTERVAL);

        // Set Advertising report fields to keep
        temp16 = (SCAN_ADVRPT_FLD_ADDRTYPE | SCAN_ADVRPT_FLD_ADDRESS);
        GapScan_setParam(SCAN_PARAM_RPT_FIELDS, &temp16);
        // Set Scanning Primary PHY
        temp8 = ADDRMODE_RP_WITH_PUBLIC_ID;
        GapScan_setParam(SCAN_PARAM_PRIM_PHYS, &temp8);
        // Set LL Duplicate Filter
        temp8 = SCAN_FLT_DUP_ENABLE;
        GapScan_setParam(SCAN_PARAM_FLT_DUP, &temp8);

        // Set PDU type filter -
        // Only 'Connectable' and 'Complete' packets are desired.
        // It doesn't matter if received packets are
        // whether Scannable or Non-Scannable, whether Directed or Undirected,
        // whether Scan_Rsp's or Advertisements, and whether Legacy or Extended.
        temp16 = SCAN_FLT_PDU_CONNECTABLE_ONLY | SCAN_FLT_PDU_COMPLETE_ONLY;
        GapScan_setParam(SCAN_PARAM_FLT_PDU_TYPE, &temp16);

        // Set initiating PHY parameters
        GapInit_setPhyParam(DEFAULT_INIT_PHY, INIT_PHYPARAM_CONN_INT_MIN,
                            INIT_PHYPARAM_MIN_CONN_INT);
        GapInit_setPhyParam(DEFAULT_INIT_PHY, INIT_PHYPARAM_CONN_INT_MAX,
                            INIT_PHYPARAM_MAX_CONN_INT);

        ssscMaxPduSize = pPkt->dataPktLen;

        // Start the scan
        scanningStarted = TRUE;
        GapScan_enable(0, DEFAULT_SCAN_DURATION, DEFAULT_MAX_SCAN_RES);
        //UART2_write(uartHandle, "Info: Done Initializing\n\r", 25, NULL);
    }
        break;

    case GAP_LINK_ESTABLISHED_EVENT:
    {
        if (pMsg->hdr.status == SUCCESS)
        {
            // Retrieve connection details
            uint16_t connHandle = ((gapEstLinkReqEvent_t*) pMsg)->connectionHandle;
            uint8_t* pAddr = ((gapEstLinkReqEvent_t*) pMsg)->devAddr;

            // Add this connection info to the list
            uint8_t connIndex = SimpleSerialSocketClient_addConnInfo(connHandle, pAddr);

            // Store the interesting data
            connList[connIndex].charHandle = 0;
            connList[connIndex].state = BLE_STATE_CONNECTED;
            connList[connIndex].startDiscClock = (Clock_Struct*) ICall_malloc(sizeof(Clock_Struct));
            connList[connIndex].discoveryDone = FALSE;
            memcpy(connList[connIndex].deviceIdentifier, &(Util_convertBdAddr2Str(connList[connIndex].addr)[12]), SSSS_DEVICE_IDENTIFIER_LENGTH);

            if(connList[connIndex].startDiscClock)
            {
                // Red LED indicates connection
                LED_setOn(ledHandle[CONFIG_LED_0], 100);

                // If service discovery not performed initiate service discovery
                if (streamServiceHandle[connIndex].startHandle == GATT_INVALID_HANDLE)
                {
                    Util_constructClock(connList[connIndex].startDiscClock,
                                        SimpleSerialSocketClient_genericClockHandler,
                                        DEFAULT_SVC_DISCOVERY_DELAY, 0, true,
                                        (connIndex << 8) | SSSC_START_DISCOVERY_EVT); // This is valid as SSSC_START_DISCOVERY_EVT uses the 8 LSBs
                }

                HCI_LE_ReadRemoteUsedFeaturesCmd(connHandle);

                // Display info
                const uint8_t BLE_ADDR_STR_SIZE = 14;// Size of string-converted device address ("0xXXXXXXXXXXXX")
                uint8_t infoMessage[] = "Info: Client 0x0123456789AB (xx) connected. Start typing.\n\r";
                memcpy(&infoMessage[13], Util_convertBdAddr2Str(connList[connIndex].addr), BLE_ADDR_STR_SIZE);
                memcpy(&infoMessage[29], connList[connIndex].deviceIdentifier, SSSS_DEVICE_IDENTIFIER_LENGTH);
                //UART2_write(uartHandle, infoMessage, sizeof(infoMessage), NULL);

                if(numConn == MAX_NUM_BLE_CONNS)
                {
                    // Disable scanning
                    scanningStarted = FALSE;
                    GapScan_disable("");
                }

            }
            else
            {
                GAP_TerminateLinkReq(connList[connIndex].connHandle, HCI_DISCONNECT_REMOTE_USER_TERM);
            }



        }
    }
        break;

    case GAP_LINK_TERMINATED_EVENT:
    {
        uint16_t connHandle = ((gapTerminateLinkEvent_t*) pMsg)->connectionHandle;

        uint8_t connIndex = SimpleSerialSocketClient_removeConnInfo(connHandle);

        connList[connIndex].state = BLE_STATE_IDLE;
        connList[connIndex].connHandle = LINKDB_CONNHANDLE_INVALID;
        connList[connIndex].discoveryDone = FALSE;

        if(connList[connIndex].startDiscClock)
        {
            Util_stopClock(connList[connIndex].startDiscClock);
            Clock_destruct(connList[connIndex].startDiscClock);
            ICall_free(connList[connIndex].startDiscClock);
            connList[connIndex].startDiscClock = NULL;
        }

        if(connList[connIndex].startNotiEnableClock)
        {
            Util_stopClock(connList[connIndex].startNotiEnableClock);
            Clock_destruct(connList[connIndex].startNotiEnableClock);
            ICall_free(connList[connIndex].startNotiEnableClock);
            connList[connIndex].startNotiEnableClock = NULL;
        }

        // Reset LEDs
        LED_setOff(ledHandle[CONFIG_LED_0]);
        LED_setOff(ledHandle[CONFIG_LED_1]);
        // Clean up service handles
        streamServiceHandle[connIndex].startHandle = GATT_INVALID_HANDLE;
        streamServiceHandle[connIndex].endHandle = GATT_INVALID_HANDLE;

        // Start the scan
        scanningStarted = TRUE;
        GapScan_enable(0, DEFAULT_SCAN_DURATION, DEFAULT_MAX_SCAN_RES);
    }
        break;

    default:
        break;
    }
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_processGATTMsg
 *
 * @brief   Process GATT messages and events.
 *
 * @return  none
 */
int notificationsSeen = 0;
int lengthOfLastNotification = 0;
uint8_t *lastNotificationPayload;
uint8_t testData [] =
{
 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,
 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31,
 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63,
 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d,
 0x6e, 0x6f
};
int comparisonTest = 0;
int comparisonsPassed = 0;
int comparisonsFailed = 0;
static void SimpleSerialSocketClient_processGATTMsg(gattMsgEvent_t *pMsg)
{
    uint8_t connIndex = SimpleSerialSocketClient_getConnIndex(pMsg->connHandle);

    if (connList[connIndex].state == BLE_STATE_CONNECTED)
    {
        // A notification was received, process data received from the stream socket
        if (pMsg->method == ATT_HANDLE_VALUE_NOTI)
        {
            notificationsSeen++;
            lengthOfLastNotification =  pMsg->msg.handleValueNoti.len;

            lastNotificationPayload = pMsg->msg.handleValueNoti.pValue;
            comparisonTest = memcmp(testData, pMsg->msg.handleValueNoti.pValue, 112);
            if(comparisonTest == 0)
            {
                comparisonsPassed++;
            }
            else
            {
                comparisonsFailed++;
            }


            AudioPlayer_addData(pMsg->msg.handleValueNoti.pValue, pMsg->msg.handleValueNoti.len);


        }
        else if (connList[connIndex].discoveryDone == FALSE)
        {
            SimpleSerialSocketClient_processGATTDiscEvent(pMsg);
        }
    } // else - in case a GATT message came after a connection has dropped, ignore it.

    // Needed only for ATT Protocol messages
    GATT_bm_free(&pMsg->msg, pMsg->method);
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_processConnEvt
 *
 * @brief   Process connection event.
 *
 * @param pReport pointer to connection event report
 */
static void SimpleSerialSocketClient_processConnEvt(Gap_ConnEventRpt_t *pReport)
{
    if (CONNECTION_EVENT_REGISTRATION_CAUSE(FOR_STREAM))
    {
        // The stream is active, process the stream at every connection event.
        uint8_t status = SimpleStreamClient_processStream(pReport->handle);

        if (status == SUCCESS)
        {
            // No more data is awaiting processing, un-register to the connection events
            SimpleSerialSocketClient_UnRegisterToAllConnectionEvent(FOR_STREAM);
        }
    }
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_processGATTDiscEvent
 *
 * @brief   Process GATT discovery event
 *
 * @return  none
 */

static void SimpleSerialSocketClient_processGATTDiscEvent(gattMsgEvent_t *pMsg)
{
    uint8_t retVal = SUCCESS;
    uint8_t connIndex = SimpleSerialSocketClient_getConnIndex(pMsg->connHandle);

    retVal = SimpleServiceDiscovery_discoverService(connList[connIndex].connHandle, selfEntity,
                                                    &streamServiceHandle[connIndex], pMsg);

    // If all handles is populated or the discovery is complete
    if (retVal == SIMPLE_DISCOVERY_SUCCESSFUL)
    {
        // Create a clock object and start
        connList[connIndex].startNotiEnableClock
              = (Clock_Struct*) ICall_malloc(sizeof(Clock_Struct));

        if (connList[connIndex].startNotiEnableClock)
        {
            Util_constructClock(connList[connIndex].startNotiEnableClock,
                                SimpleSerialSocketClient_genericClockHandler, 200, 0, true,
                                (connIndex << 8) | SSSC_NOTI_ENABLE_EVT);

            connList[connIndex].discoveryDone = TRUE;
        }
        else
        {
            GAP_TerminateLinkReq(connList[connIndex].connHandle, HCI_DISCONNECT_REMOTE_USER_TERM);
        }
    }
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_startDiscovery
 *
 * @brief   Start service discovery by issuing a MTU exchange.
 *
 * @return  none
 */
static void SimpleSerialSocketClient_startDiscovery(void)
{
    attExchangeMTUReq_t req;

    int connIndex;
    for(connIndex = 0; connIndex < MAX_NUM_BLE_CONNS; connIndex++)
    {
        if(connList[connIndex].state == BLE_STATE_CONNECTED_DISCOVERY_TO_BE_TRIGERED)
        {
            // Discover GATT Server's Rx MTU size
            req.clientRxMTU = ssscMaxPduSize - L2CAP_HDR_SIZE;

            // ATT MTU size should be set to the minimum of the Client Rx MTU
            // and Server Rx MTU values
            VOID GATT_ExchangeMTU(connList[connIndex].connHandle, &req, selfEntity);

            connList[connIndex].state = BLE_STATE_CONNECTED;
        }
    }
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_findSerialStreamServer
 *
 * @brief   Find a serial stream server based on given UUID in an advertiser's
 *          service UUID list and manufacturer specific data
 *
 * @return  TRUE if service UUID and matching manufacturer data found
 */
static bool SimpleSerialSocketClient_findSerialStreamServer(
        const uint8_t *uuid, const uint8_t *pManufData, uint8_t manDataLen,
        uint8_t *pData, uint8_t dataLen)
{
    uint8_t matchingIdAndUuid = 0;
    uint8_t adLen;
    uint8_t adType;
    uint8_t *pEnd;

    pEnd = pData + dataLen - 1;

    // While end of data not reached
    while ((dataLen > 0) && (pData < pEnd))
    {
        // Get length of next AD item
        adLen = *pData++;
        if (adLen > 0)
        {
            adType = *pData;

            // If AD type is for 128-bit service UUID
            if ((adType == GAP_ADTYPE_128BIT_MORE)
                    || (adType == GAP_ADTYPE_128BIT_COMPLETE))
            {
                pData++;
                adLen--;

                // For each UUID in list
                while (adLen >= 2 && pData < pEnd)
                {
                    // Check for match
                    if (!memcmp(pData, uuid, ATT_UUID_SIZE))
                    {
                        matchingIdAndUuid++;
                    }

                    // Go to next
                    pData += 2;
                    adLen -= 2;
                }

                // Handle possible erroneous extra byte in UUID list
                if (adLen == 1)
                {
                    pData++;
                }
            }

            // REMOVED FOR TESTING
            if (adType == GAP_ADTYPE_MANUFACTURER_SPECIFIC)
            {
                // Floor to the lowest length
                uint8_t len = adLen;
                if (manDataLen < len)
                {
                    len = manDataLen;
                }

                if (memcmp(pManufData, pData, len))
                {
                    matchingIdAndUuid++;
                }
            }

            if (matchingIdAndUuid < 2)
            {
                // Go to next item
                pData += adLen;
            }
            else
            {
                return TRUE;
            }

        }
    }

    // Match not found
    return FALSE;
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_genericClockHandler
 *
 * @brief   Generic clock handler for events
 *
 * @param   a0 - passed on
 *
 * @return  none
 */
void SimpleSerialSocketClient_genericClockHandler(UArg a0)
{

    uint8_t evtId = (uint8_t) (a0 & 0xFF);
    uint8_t connIndex = (uint8_t) (a0 >> 8) ;

    switch (evtId)
    {
      case SSSC_START_DISCOVERY_EVT:
          if(connList[connIndex].state == BLE_STATE_CONNECTED)
          {
              connList[connIndex].state = BLE_STATE_CONNECTED_DISCOVERY_TO_BE_TRIGERED;
              Event_post(syncEvent, SSSC_START_DISCOVERY_EVT);
          }
        break;

      case SSSC_NOTI_ENABLE_EVT:
          if(connList[connIndex].state == BLE_STATE_CONNECTED)
          {
              connList[connIndex].state = BLE_STATE_CONNECTED_NOTIFICATIONS_TO_BE_ENABLED;
              Event_post(syncEvent, SSSC_NOTI_ENABLE_EVT);
          }
        break;

      default:
        break;
    }


}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_connEvtCB
 *
 * @brief   Connection event callback.
 *
 * @param pReport pointer to connection event report
 */
static void SimpleSerialSocketClient_connEvtCB(Gap_ConnEventRpt_t *pReport)
{
    // Enqueue the event for processing in the app context.
    if (SimpleSerialSocketClient_enqueueMsg(SSSC_EVT_CONN, 0,
                                            (uint8_t*) pReport, 0) != SUCCESS)
    {
        ICall_free(pReport);
    }
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_enqueueMsg
 *
 * @brief   Creates a message and puts the message in RTOS queue.
 *
 * @param   event - message event.
 * @param   state - message state.
 * @param   pData - message data pointer.
 *
 * @return  TRUE or FALSE
 */
static uint8_t SimpleSerialSocketClient_enqueueMsg(uint8_t event, uint8_t state,
                                                   uint8_t *pData,
                                                   uint16_t arg0)
{
    uint8_t success;
    ssscEvt_t *pMsg = ICall_malloc(sizeof(ssscEvt_t));

    // Create dynamic pointer to message.
    if (pMsg)
    {
        pMsg->hdr.event = event;
        pMsg->hdr.state = state;
        pMsg->pData = pData;
        pMsg->arg0 = arg0;

        // Enqueue the message.
        success = Util_enqueueMsg(appMsgQueue, syncEvent, (uint8_t*) pMsg);
        return (success) ? SUCCESS : FAILURE;
    }

    return FALSE;
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_scanCb
 *
 * @brief   Callback called by GapScan module
 *
 * @param   evt - event
 * @param   msg - message coming with the event
 * @param   arg - user argument
 *
 * @return  none
 */
void SimpleSerialSocketClient_scanCb(uint32_t evt, void *pMsg, uintptr_t arg)
{
    uint8_t event;

    if (evt & GAP_EVT_ADV_REPORT)
    {
        event = SSSC_EVT_ADV_REPORT;
    }
    else if (evt & GAP_EVT_SCAN_ENABLED)
    {
        event = SSSC_EVT_SCAN_ENABLED;
    }
    else if (evt & GAP_EVT_SCAN_DISABLED)
    {
        event = SSSC_EVT_SCAN_DISABLED;
    }
    else if (evt & GAP_EVT_INSUFFICIENT_MEMORY)
    {
        event = SSSC_EVT_INSUFFICIENT_MEM;
    }
    else
    {
        return;
    }

    if (SimpleSerialSocketClient_enqueueMsg(event, SUCCESS, pMsg,
                                            0) != SUCCESS)
    {
        ICall_free(pMsg);
    }
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_addConnInfo
 *
 * @brief   Add a device to the connected device list
 *
 * @return  index of the connected device list entry where the new connection
 *          info is put in.
 *          if there is no room, MAX_NUM_BLE_CONNS will be returned.
 */
static uint8_t SimpleSerialSocketClient_addConnInfo(uint16_t connHandle, uint8_t *pAddr)
{
  uint8_t i;

  for (i = 0; i < MAX_NUM_BLE_CONNS; i++)
  {
    if (connList[i].connHandle == LINKDB_CONNHANDLE_INVALID)
    {
      // Found available entry to put a new connection info in
      connList[i].connHandle = connHandle;
      memcpy(connList[i].addr, pAddr, B_ADDR_LEN);
      numConn++;

      break;
    }
  }

  return i;
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_removeConnInfo
 *
 * @brief   Remove a device from the connected device list
 *
 * @return  index of the connected device list entry where the new connection
 *          info is removed from.
 *          if connHandle is not found, MAX_NUM_BLE_CONNS will be returned.
 */
static uint8_t SimpleSerialSocketClient_removeConnInfo(uint16_t connHandle)
{
  uint8_t i;

  for (i = 0; i < MAX_NUM_BLE_CONNS; i++)
  {
    if (connList[i].connHandle == connHandle)
    {
      // Found the entry to mark as deleted
      connList[i].connHandle = LINKDB_CONNHANDLE_INVALID;
      numConn--;

      break;
    }
  }

  return i;
}

/*********************************************************************
 * @fn      SimpleSerialSocketClient_getConnIndex
 *
 * @brief   Find index in the connected device list by connHandle
 *
 * @return  the index of the entry that has the given connection handle.
 *          if there is no match, MAX_NUM_BLE_CONNS will be returned.
 */
uint8_t SimpleSerialSocketClient_getConnIndex(uint16_t connHandle)
{
  uint8_t i;

  for (i = 0; i < MAX_NUM_BLE_CONNS; i++)
  {
    if (connList[i].connHandle == connHandle)
    {
      break;
    }
  }

  return i;
}

/*********************************************************************
 *********************************************************************/
