/******************************************************************************

@file  app_att_char.c

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
#include "app_attutil_char.h"

//*****************************************************************************
//! Defines
//*****************************************************************************

#define INITIAL_CHARACTERISTIC_LIST_SIZE 10

//*****************************************************************************
//! Prototypes
//*****************************************************************************

static bStatus_t ATTUtil_addCharToCharsList(uint8_t* pDataList, uint16_t attLength);
static bStatus_t ATTUtil_expandCharListSize();

extern void* memcpy (void* destination, const void* source, size_t num);

//*****************************************************************************
//! Globals
//*****************************************************************************

static uint8_t isCharNew = 0;
static GATTUtil_char* GATTCharList;
static uint16_t GATTCharListLength;
static uint16_t GATTCharListMaxLength;

//*****************************************************************************
//! Functions
//*****************************************************************************

//! ATT functions

/*********************************************************************
 * @fn      ATTUtil_handleReadByTypeRsp
 *
 * @brief   This function constructs a list of GATTUtil_char objects from
 * 			the raw data of the ATT ReadByType responses. Since there 
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
bStatus_t ATTUtil_handleReadByTypeRsp(attReadByTypeRsp_t typeResponse, uint8_t rspStatus)
{
	uint16_t charIndex;

	// If this is a new characteristic request
	if(isCharNew)
	{
		GATTCharList = NULL;
		GATTCharListLength = 0;
		isCharNew = false;
	}


	// For each characteristic found, we add them to our list of characteristics
	uint8_t* pDataList = typeResponse.pDataList;
	for(charIndex = 0; charIndex < typeResponse.numPairs; charIndex++)
	{
		ATTUtil_addCharToCharsList(pDataList, typeResponse.len);

		// Increase the pointer to go to the next characteristic entry
		pDataList += typeResponse.len;
	}


	// If we receive the status bleProcedureComplete or
	// bleTimeout, we know that the next characteristic
	// will be for a new service
	if(rspStatus == bleProcedureComplete || rspStatus == bleTimeout)
	{
		ATTUtil_completeHandleReadByTypeRsp();
	}

	return SUCCESS;
}

/*********************************************************************
 * @fn      ATTUtil_addCharToCharsList
 *
 * @brief   This function adds an ATT packet to the list of GATTUtil_char 
 * 			objects. It extracts the data from a uint8_t array in the 
 * 			following order : handle, permissions, value handle and UUID.
 * 
 * @param   typeResponse - the ATT response object
 * @param   rspStatus - the status of the request
 *
 * @return  SUCCESS, FAILURE
 */
static bStatus_t ATTUtil_addCharToCharsList(uint8_t* pDataList, uint16_t attLength)
{
	uint16_t handle;
	uint16_t valueHandle;
	uint8_t permissions;
	uint16_t UUIDLength;

	// Move a 16 bits value from a 8 bits array, in little endian
	handle = (uint16_t)*pDataList++;
	handle |= *pDataList++ << 8;

	permissions = *pDataList++;

	valueHandle = (uint16_t)*pDataList++;
	valueHandle |= *pDataList++ << 8;

	// We subtract the 5 bytes used for the two handles and the permissions
	UUIDLength = attLength - 5;
	GATTUtil_UUID charUUID =
	{
		.len = UUIDLength
	};

	// Fill the value property of the UUID
	for(uint8_t UUIDIndex = 0; UUIDIndex < UUIDLength; UUIDIndex++)
	{
		charUUID.value[UUIDIndex] = pDataList[UUIDIndex];
	}

	// Create and fill the characteristic struct
	GATTUtil_char characteristic =
	{
		.handle       = handle,
		.valueHandle  = valueHandle,
		.permissions  = permissions,
		.UUID         = charUUID
	};

	GATTCharListLength += 1;

	// If space was never allocated
	if(GATTCharList == NULL)
	{
		// Make space for the struct in the characteristic list
		GATTCharList = ICall_malloc(sizeof(GATTUtil_char) * INITIAL_CHARACTERISTIC_LIST_SIZE);
		GATTCharListMaxLength = INITIAL_CHARACTERISTIC_LIST_SIZE;
	}

	// Check if need to realloc more space
	if(GATTCharListLength == GATTCharListMaxLength)
	{
		ATTUtil_expandCharListSize();
	}

	GATTCharList[GATTCharListLength - 1] = characteristic;

	return SUCCESS;
}

/*********************************************************************
 * @fn      ATTUtil_completeHandleReadByTypeRsp
 *
 * @brief   This function is executed when the ATT request is done. It
 * 			calls the request's callback with the finished array, and
 * 			the next ATT response packets will be associated to a new
 * 			ATT request.
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t ATTUtil_completeHandleReadByTypeRsp()
{
	GATTUtil_Queue_Elem queueElem;

	// If no characteristics are being processed currently, no need to do anything
	if(isCharNew)
	{
		return SUCCESS;
	}

	isCharNew = true;

	// Get the request associated to the result
	GATTUtil_popElementFromQueue(&queueElem);

	// Once the final object is built, call the original callback
	(queueElem.callback.charsCallback)(
		GATTCharList, 
		GATTCharListLength,
		queueElem.callbackContext);

	return SUCCESS;
}

/*********************************************************************
 * @fn      ATTUtil_expandCharListSize
 *
 * @brief   This function doubles the characteristics list size by 
 * 			allocating new memory and copying the old list into the new one.
 *
 * @return  SUCCESS, FAILURE
 */
static bStatus_t ATTUtil_expandCharListSize()
{
    // Double the size of the list every time it needs to be expanded
    GATTUtil_char* temporary_GATTCharList = ICall_malloc(sizeof(GATTUtil_char) * GATTCharListMaxLength * 2);

    // Copy old list into new one
    memcpy(temporary_GATTCharList, GATTCharList, sizeof(GATTUtil_char) * GATTCharListMaxLength);

    // Free old list
    ICall_free(GATTCharList);

    // Update values
    GATTCharList = temporary_GATTCharList;
    GATTCharListMaxLength = GATTCharListMaxLength * 2;

    return SUCCESS;
}

//! Permissions functions

/*********************************************************************
 * @fn      GATTUtil_isCharReadable
 *
 * @brief   This function checks if the characteristic is readable 
 * 			or not.
 *
 * @return  TRUE, FALSE
 */
uint8_t GATTUtil_isCharReadable(GATTUtil_char characteristic)
{
	return characteristic.permissions & 0x2;
}

/*********************************************************************
 * @fn      GATTUtil_isCharWritable
 *
 * @brief   This function checks if the characteristic is writable 
 * 			or not.
 *
 * @return  TRUE, FALSE
 */
uint8_t GATTUtil_isCharWritable(GATTUtil_char characteristic)
{
	return characteristic.permissions & 0x8;
}

/*********************************************************************
 * @fn      GATTUtil_isCharNotif
 *
 * @brief   This function checks if the characteristic is a notification 
 * 			or not.
 *
 * @return  TRUE, FALSE
 */
uint8_t GATTUtil_isCharNotif(GATTUtil_char characteristic)
{
	return characteristic.permissions & 0x10;
}

/*********************************************************************
 * @fn      GATTUtil_freeChars
 *
 * @brief   This function frees the memory associated with the characteristics.
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t GATTUtil_freeChars()
{
	GATTCharListLength = 0;
	isCharNew = true;

	// If characteristics list is already free
	if(!GATTCharList) {
		return FAILURE;
	}

	// Free the characteristics list
    ICall_free(GATTCharList);

	return SUCCESS;
}