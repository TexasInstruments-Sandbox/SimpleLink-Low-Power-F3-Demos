/******************************************************************************

@file  app_super_adv.c

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

#include <app_super_adv.h>

//*****************************************************************************
//! Defines
//*****************************************************************************

//*****************************************************************************
//! Globals
//*****************************************************************************

//*****************************************************************************
//! Functions
//*****************************************************************************

/*********************************************************************
 * @fn      SuperAdv_getNextAddressInPool
 *
 * @brief   This function gets the next address in the given pool of
 *          device addresses.
 *          It goes through the addresses linearly, and comes back to
 *          the beginning of the array when no addresses are left.
 *
 * @param   deviceAddressPool - the device address pool to get the
 *          next address from.
 *
 * @return  the next device address in the pool.
 */
uint8_t* SuperAdv_getNextAddressInPool(SuperAdv_deviceAddressPool* deviceAddressPool)
{
    uint8_t addressIndex = deviceAddressPool->currentAddressIndex;

    // Increment and save the current address index
    uint8_t newAddressIndex = (addressIndex + 1) % deviceAddressPool->numAddresses;
    deviceAddressPool->currentAddressIndex = newAddressIndex;

    // Get the address at the new index of the pool
    uint8_t* virtual_device_address = (deviceAddressPool->deviceAddressPool)[newAddressIndex];

    return virtual_device_address;
}

/*********************************************************************
 * @fn      SuperAdv_generateDeviceAddressPool
 *
 * @brief   This function generates a new pool of device addresses.
 *
 * @param   numOfAddresses - the amount of addresses wanted in the pool.
 * @param   deviceAddressPool - a pointer to a SuperAdv_deviceAddressPool
 *          struct.
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t SuperAdv_generateDeviceAddressPool(uint8_t numOfAddresses, SuperAdv_deviceAddressPool* deviceAddressPool)
{
    RNG_Handle handle;
    int_fast16_t result;
    uint8_t lowerLimit[2] = {0x00, 0x00};
    uint8_t upperLimit[2] = {0x01, 0xFF};
    uint8_t randomAddress[6] = {0};
    uint8_t randomNumberBitLength = 8;

    // Refuse zero numOfAddresses (can't be negative because of unsigned)
    if(numOfAddresses <= 0)
    {
        return FAILURE;
    }

    // Initialize randomness drivers to create random uint8
    result = RNG_init();
    if(result != RNG_STATUS_SUCCESS)
    {
        // TODO: Call Error Handler
        return FAILURE;
    }

    /*
     * Open RNG driver
     * Config 0 is already used by the bluetooth stack, so
     * we must create a config in SysConfig and use Config 1
     */
    handle = RNG_open(1, NULL);

    if(!handle)
    {
        RNG_close(handle);

        // TODO: Call Error Handler
        return FAILURE;
    }

    // If address pool already allocated, need to free it to change size
    if(deviceAddressPool->deviceAddressPool)
    {
        free(deviceAddressPool->deviceAddressPool);
    }

    // Allocate memory for the device address pool
    uint8_t (*pool)[6] = malloc(sizeof(uint8_t[numOfAddresses][6]));

    // For each desired addres
    for(uint8_t deviceAddressIndex = 0; deviceAddressIndex < numOfAddresses; deviceAddressIndex++)
    {
        uint8_t* deviceAddress = pool[deviceAddressIndex];

        // Generate a random address
        result = RNG_getRandomBits(handle, randomAddress, 8*6);
        if (result != RNG_STATUS_SUCCESS)
        {
            RNG_close(handle);

            // TODO: Call Error Handler
            return FAILURE;
        }

        // Memcpy of randomAddress into deviceAddress
        for(uint8_t deviceAddressByteIndex = 0; deviceAddressByteIndex < 6; deviceAddressByteIndex++)
        {
            deviceAddress[deviceAddressByteIndex] = randomAddress[deviceAddressByteIndex];
        }
    }

    deviceAddressPool->deviceAddressPool = pool;
    deviceAddressPool->numAddresses = numOfAddresses;
    deviceAddressPool->currentAddressIndex = 0;

    RNG_close(handle);

    return SUCCESS;
}

/*********************************************************************
 * @fn      SuperAdv_incrementNumClient
 *
 * @brief   This function generates a new pool of device addresses,
 *          with exactly one more device address than the given device
 *          address pool.
 *          If the amount of devices is higher than 100, wraps around to 1.
 *
 * @param   deviceAddressPool - a pointer to a SuperAdv_deviceAddressPool
 *          struct.
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t SuperAdv_incrementNumClient(SuperAdv_deviceAddressPool* deviceAddressPool)
{
    uint8_t newNumOfDevices = deviceAddressPool->numAddresses + 1;

    if(newNumOfDevices > 100)
    {
        newNumOfDevices = 1;
    }

    return SuperAdv_generateDeviceAddressPool(newNumOfDevices, deviceAddressPool);
}


/*********************************************************************
 * @fn      SuperAdv_incrementNumClient
 *
 * @brief   This function generates a new pool of device addresses,
 *          with exactly one less device address than the given device
 *          address pool.
 *          If the amount of devices is lower than 1, wraps around to 100.
 *
 * @param   deviceAddressPool - a pointer to a SuperAdv_deviceAddressPool
 *          struct.
 *
 * @return  SUCCESS, FAILURE
 */
bStatus_t SuperAdv_decrementNumClient(SuperAdv_deviceAddressPool* deviceAddressPool)
{
    uint8_t newNumOfDevices = deviceAddressPool->numAddresses - 1;

    if(newNumOfDevices <= 0)
    {
        newNumOfDevices = 100;
    }

    return SuperAdv_generateDeviceAddressPool(newNumOfDevices, deviceAddressPool);
}
