/******************************************************************************

@file  app_att_service.c

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

#include "../app_gattutil.h"
#include "app_attutil_service.h"

//*****************************************************************************
//! Defines
//*****************************************************************************

#define INITIAL_SERVICE_LIST_SIZE 10

//*****************************************************************************
//! Prototypes
//*****************************************************************************

static bStatus_t ATTUtil_addServiceToServicesList(uint8_t* pDataList, uint16_t attLength);
static bStatus_t ATTUtil_expandServiceListSize();

extern void* memcpy (void* destination, const void* source, size_t num);

//*****************************************************************************
//! Globals
//*****************************************************************************

static uint8_t isServiceNew = 0;
static GATTUtil_service* GATTServiceList;
static uint16_t GATTServiceListLength;
static uint16_t GATTServiceListMaxLength;

//*****************************************************************************
//! Functions
//*****************************************************************************

/*********************************************************************
 * @fn      ATTUtil_handleReadByGrpTypeRsp
 *
 * @brief   This function constructs a list of GATTUtil_service objects from
 * 			the raw data of the ATT ReadByGroupType responses. Since there 
 * 			might be multiple ATT response packets for one ATT request, 
 * 			this function may be called multiple times for one request.
 *          A request if complete once we recieve the status 
 *          bleProcedureComplete or bleTimeout, or that we recieve a
 * 		    ATT response of type Error with SUCCESS status.
 * 
 * @param   typeResponse - the ATT response object
 * @param   rspStatus - the status of the request
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t ATTUtil_handleReadByGrpTypeRsp(attReadByGrpTypeRsp_t groupTypeResponse, uint8_t rspStatus)
{
	uint16_t serviceIndex;
	GATTUtil_Queue_Elem queueElem;


	// If this is a new service request
	if(isServiceNew)
	{
		GATTServiceList = NULL;
		GATTServiceListLength = 0;
		isServiceNew = false;
	}


	// For each service found, we add them to our local GATT table
	uint8_t* pDataList = groupTypeResponse.pDataList;
	for(serviceIndex = 0; serviceIndex < groupTypeResponse.numGrps; serviceIndex++)
	{
		

		// Add the service to the local table
		ATTUtil_addServiceToServicesList(
			pDataList, groupTypeResponse.len);

		// Increase the pointer to go to the next service entry
		pDataList += groupTypeResponse.len;
	}


	// If we receive the status bleProcedureComplete or
	// bleTimeout, we inform the GATT service that the
	// next service will be for a new table
	if(rspStatus == bleProcedureComplete || rspStatus == bleTimeout)
	{
		isServiceNew = true;

		// Get the request associated to the result
		GATTUtil_popElementFromQueue(&queueElem);

		// Once the final object is built, call the original callback
		(queueElem.callback.servicesCallback)(
			GATTServiceList, 
			GATTServiceListLength,
			queueElem.callbackContext);
	}

	return SUCCESS;
}

/*********************************************************************
 * @fn      ATTUtil_addServiceToServicesList
 *
 * @brief   This function adds an ATT packet to the list of GATTUtil_service 
 * 			objects. It extracts the data from a uint8_t array in the 
 * 			following order : start handle, end handle and UUID.
 * 
 * @param   typeResponse - the ATT response object
 * @param   rspStatus - the status of the request
 *
 * @return  SUCCESS, FAILURE
 */
static bStatus_t ATTUtil_addServiceToServicesList(uint8_t* pDataList, uint16_t attLength)
{
	uint16_t startHandle;
	uint16_t endHandle;
	uint16_t UUIDLength;

	// Move a 16 bits value from a 8 bits array, in little endian
	startHandle = (uint16_t)*pDataList++;
	startHandle |= *pDataList++ << 8;

	endHandle = (uint16_t)*pDataList++;
	endHandle |= *pDataList++ << 8;

	// We subtract the 4 bytes used for the two handles
	UUIDLength = attLength - 4;

	GATTUtil_UUID serviceUUID =
	{
		.len = UUIDLength
	};

	// Fill the value property of the UUID
	for(uint8_t UUIDIndex = 0; UUIDIndex < UUIDLength; UUIDIndex++)
	{
		serviceUUID.value[UUIDIndex] = pDataList[UUIDIndex];
	}

	// Create and fill the service struct
	GATTUtil_service service =
	{
		.startHandle =  startHandle,
		.endHandle =    endHandle,
		.UUID =         serviceUUID
	};

	GATTServiceListLength += 1;
	
	// If space was never allocated
	if(GATTServiceList == NULL)
	{
		// Make space for the struct in the service list
		GATTServiceList = ICall_malloc(sizeof(GATTUtil_service) * INITIAL_SERVICE_LIST_SIZE);
		GATTServiceListMaxLength = INITIAL_SERVICE_LIST_SIZE;
	}

	// Check if need to realloc more space
	if(GATTServiceListLength == GATTServiceListMaxLength)
	{
		ATTUtil_expandServiceListSize();
	}

	GATTServiceList[GATTServiceListLength - 1] = service;

	return SUCCESS;
}

/*********************************************************************
 * @fn      ATTUtil_expandServiceListSize
 *
 * @brief   This function doubles the service list size by allocating new 
 *          memory and copying the old list into the new one.
 *
 * @return  SUCCESS, FAILURE
 */
static bStatus_t ATTUtil_expandServiceListSize()
{
    // Double the size of the list every time it needs to be expanded
    GATTUtil_service* temporary_GATTServiceList = ICall_malloc(sizeof(GATTUtil_service) * GATTServiceListMaxLength * 2);

    // Copy old list into new one
    memcpy(temporary_GATTServiceList, GATTServiceList, sizeof(GATTUtil_service) * GATTServiceListMaxLength);

    // Free old list
    ICall_free(GATTServiceList);

    // Update values
    GATTServiceList = temporary_GATTServiceList;
    GATTServiceListMaxLength = GATTServiceListMaxLength * 2;

    return SUCCESS;
}

/*********************************************************************
 * @fn      GATTUtil_freeServices
 *
 * @brief   This function frees the memory associated with the services.
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t GATTUtil_freeServices()
{
	GATTServiceListLength = 0;
	isServiceNew = true;

	// If services list is already free
	if(!GATTServiceList) {
		return FAILURE;
	}

	// Free the services list
    ICall_free(GATTServiceList);

	return SUCCESS;
}