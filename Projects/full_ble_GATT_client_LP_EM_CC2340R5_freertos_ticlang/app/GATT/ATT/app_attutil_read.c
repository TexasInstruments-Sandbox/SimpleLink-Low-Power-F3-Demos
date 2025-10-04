/******************************************************************************

@file  app_att_read.c

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

#include <app_main.h>

#include "../app_attutil.h"
#include "../app_gattutil.h"
#include "app_attutil_read.h"

//*****************************************************************************
//! Prototypes
//*****************************************************************************

extern void* memcpy (void* destination, const void* source, size_t num);

//*****************************************************************************
//! Functions
//*****************************************************************************

/*********************************************************************
 * @fn      ATTUtil_handleReadRsp
 *
 * @brief   This function handles ATT Read responses. Since there 
 * 			might be multiple ATT response packets for one ATT request, 
 * 			this function may be called multiple times for one request.
 *          A request if complete once we recieve the status 
 *          SUCCESS or bleTimeout, or that we recieve a
 * 		    ATT response of type Error with SUCCESS status.
 * 
 * @param   typeResponse - the ATT response object
 * @param   rspStatus - the status of the request
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t ATTUtil_handleReadRsp(attReadRsp_t readResponse, uint8_t rspStatus)
{
	/* 
		TODO : Unlike the function ATTUtil_handleReadByGrpTypeRsp in 
		app_read_service.c, this function does not use a local table to store 
		the response and wait until we recieve a ATT_READ_RSP with with SUCCESS
		or bleTimeout status or ATT_ERROR_RSP with SUCCESS status.

		This has the consequence that if the Characteristic Value is greater 
		than (ATT_MTU - 1) octets in length, there will be data loss and not
		of the value will be read.
		
		In case this happens (unlikely), GATT_ReadLongCharValue should be used
		instead of GATT_ReadCharValue the function GATTUtil_GetValueOfAttribute 
		in app_gattutil.c. 
		
		This requires to change more in this function, as 
		the type returned by GATT_ReadLongCharValue is attReadBlobRsp_t instead
		of attReadRsp_t and the type to look for in ATTUtil_EventHandler will 
		be ATT_READ_BLOB_RSP instead of ATT_READ_RSP.
	*/

	GATTUtil_Queue_Elem queueElem;


	// Get the request associated to the result
	GATTUtil_popElementFromQueue(&queueElem);

	//Execute callback from request
	if(queueElem.reqType == CHAR_DESCRIPTOR)
	{
		// Malloc so that we own the data instead of the BLE stack
		uint8_t* pDesc = ICall_malloc(readResponse.len);
		memcpy(pDesc, readResponse.pValue, readResponse.len);

		GATTUtil_desc gatt_desc = 
		{
			.len = readResponse.len,
			.pDesc = pDesc
		};

		(queueElem.callback.descCallback)(gatt_desc, queueElem.callbackContext);
	}
	if(queueElem.reqType == CHAR_VALUE)
	{
		// Malloc so that we own the data instead of the BLE stack
		uint8_t* pValue = ICall_malloc(readResponse.len);
		memcpy(pValue, readResponse.pValue, readResponse.len);

		GATTUtil_value gatt_value = 
		{
			.len = readResponse.len,
			.pValue = pValue
		};

		(queueElem.callback.valueCallback)(gatt_value, queueElem.callbackContext);
	}

	return SUCCESS;
}

//! Memory functions

/*********************************************************************
 * @fn      GATTUtil_freeValue
 *
 * @brief   This function frees the memory associated with a GATTUtil_value
 *          object.
 * 
 * @param   value - the object to free
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t GATTUtil_freeValue(GATTUtil_value value)
{
	ICall_free(value.pValue);
	value.pValue = 0;

	return SUCCESS;
}

/*********************************************************************
 * @fn      GATTUtil_freeDesc
 *
 * @brief   This function frees the memory associated with a GATTUtil_desc
 *          object.
 * 
 * @param   description - the object to free
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t GATTUtil_freeDesc(GATTUtil_desc description)
{
	ICall_free(description.pDesc);
	description.pDesc = 0;

	return SUCCESS;
}

