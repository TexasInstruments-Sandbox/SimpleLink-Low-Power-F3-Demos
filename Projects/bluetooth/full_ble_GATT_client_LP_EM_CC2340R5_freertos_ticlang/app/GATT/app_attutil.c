/******************************************************************************

@file  app_att.c

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

#include <ti/ble/app_util/menu/menu_module.h>
#include <ti/ble/app_util/framework/bleapputil_api.h>
#include "app_main.h"

#include "app_attutil.h"
#include "app_gattutil.h"
#include "ATT/app_attutil_service.h"
#include "ATT/app_attutil_char.h"
#include "ATT/app_attutil_read.h"

//*****************************************************************************
//! Globals
//*****************************************************************************

// Events handlers struct, contains the handlers and event masks
// of the application data module
BLEAppUtil_EventHandler_t dataGATTHandler =
{
    .handlerType    = BLEAPPUTIL_GATT_TYPE,
    .pEventHandler  = ATTUtil_EventHandler,
    .eventMask      = BLEAPPUTIL_ATT_READ_BY_GRP_TYPE_RSP |
                      BLEAPPUTIL_ATT_READ_BY_TYPE_RSP |
                      BLEAPPUTIL_ATT_READ_RSP |
                      BLEAPPUTIL_ATT_ERROR_RSP
};

//*****************************************************************************
//! Functions
//*****************************************************************************

/*********************************************************************
 * @fn      Data_start
 *
 * @brief   This function is called after stack initialization,
 *          the purpose of this function is to initialize and
 *          register the specific events handlers of the data
 *          application module
 *
 * @return  SUCCESS, errorInfo
 */
bStatus_t Data_start( void )
{
  bStatus_t status = SUCCESS;

  // Register the handlers
  status = BLEAppUtil_registerEventHandler( &dataGATTHandler );

  // Return status value
  return( status );
}

/*********************************************************************
 * @fn      ATTUtil_EventHandler
 *
 * @brief   This function is called when the BLE stack recieved an ATT
 *          packet. the pMsgData parameter contains all the unformatted
 *          information of the packet.
 * 
 * @param   event - unused
 * @param   pMsgData - data of the event
 *          
 * @return  none
 */
void ATTUtil_EventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
    gattMsgEvent_t *gattMsg = ( gattMsgEvent_t * )pMsgData;
    
    switch ( gattMsg->method )
    {
        case ATT_READ_BY_GRP_TYPE_RSP:
        {
            attReadByGrpTypeRsp_t groupTypeResponse = gattMsg->msg.readByGrpTypeRsp;
            uint8_t rspStatus = gattMsg->hdr.status;
            
            ATTUtil_handleReadByGrpTypeRsp(groupTypeResponse, rspStatus);
            GATT_bm_free(&gattMsg->msg, ATT_READ_BY_GRP_TYPE_RSP );
        }
        break;

        case ATT_READ_BY_TYPE_RSP:
        {
            attReadByTypeRsp_t typeResponse = gattMsg->msg.readByTypeRsp;
            uint8_t rspStatus = gattMsg->hdr.status;
            
            ATTUtil_handleReadByTypeRsp(typeResponse, rspStatus);
            GATT_bm_free(&gattMsg->msg, ATT_READ_BY_TYPE_RSP );
        }
        break;

        case ATT_READ_RSP:
        {
            attReadRsp_t readResponse = gattMsg->msg.readRsp;
            uint8_t rspStatus = gattMsg->hdr.status;

            ATTUtil_handleReadRsp(readResponse, rspStatus);
            GATT_bm_free(&gattMsg->msg, ATT_READ_RSP );
        }
        break;

        case ATT_ERROR_RSP:
        {
            attErrorRsp_t errResponse = gattMsg->msg.errorRsp;
            uint8_t rspStatus = gattMsg->hdr.status;

            // Permission error
            if(errResponse.errCode == 5)
            {
                MenuModule_printf(APP_MENU_GENERAL_STATUS_LINE, 0,
                        MENU_MODULE_COLOR_RED "Error occured when processing ATT request : Permission Denied" MENU_MODULE_COLOR_RESET);
                return;
            }

            // If the error doesn't indicate a finished sub-procedure
            if(rspStatus != SUCCESS)
            {
                return;
            }

            ATTUtil_completeHandleReadByTypeRsp();
            GATT_bm_free(&gattMsg->msg, ATT_ERROR_RSP );
        }
        break;


        default:
            break;
    }
}