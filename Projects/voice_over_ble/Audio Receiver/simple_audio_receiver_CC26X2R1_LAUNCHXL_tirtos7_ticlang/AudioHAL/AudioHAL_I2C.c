/*
 * Copyright (c) 2015-2020, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <AudioHAL/AudioHAL_I2C.h>

/* DeviceFamily Includes */
#include <ti/devices/DeviceFamily.h>

static bool i2cWrite(I2C_Handle handle , uint8_t codecI2CAddr,
                                uint8_t registerAddr, uint8_t registerValue)
{
    /* Setup local variables */
    uint8_t writeDataBuf[2];
    uint8_t readDataBuf[1];
    I2C_Transaction i2cTransaction;

    #if (DeviceFamily_PARENT != DeviceFamily_PARENT_CC13X0_CC26X0)
    /* I2C_transferTimeout() is not defined for CC2640R2 devices */
    int_fast16_t i2c_status;
    int16_t i2c_remainingTries;
    #endif

    /* Initialize write buffer, write transactions are |registerAddr|registerValue| */
    writeDataBuf[0] = registerAddr;
    writeDataBuf[1] = registerValue;

    #if (DeviceFamily_PARENT == DeviceFamily_PARENT_CC13X0_CC26X0)
    /* I2C_transferTimeout() is not defined for CC2640R2 devices */

    i2cTransaction.writeBuf = writeDataBuf;
    i2cTransaction.writeCount = 2;
    i2cTransaction.readBuf = readDataBuf;
    i2cTransaction.readCount = 0;
    i2cTransaction.slaveAddress = codecI2CAddr;

    /* Read back to the register */
    if (!I2C_transfer(handle, &i2cTransaction))
    {
        return (false);
    }

    i2cTransaction.writeBuf = writeDataBuf;
    i2cTransaction.writeCount = 1;
    i2cTransaction.readBuf = readDataBuf;
    i2cTransaction.readCount = 1;

    /* Write to the register and return status */
    return (I2C_transfer(handle, &i2cTransaction));

    #else /* For CC26x2, CC13x2 and CC32xx devices */
    i2c_remainingTries = AUDIOHAL_I2C_RETRIES_ON_TIMEOUT;
    do{

        /* Read back to the register
         *    Note: If a timeout occurs during this I2C transfer, then
         *    it can be retried right away.
         */
        i2cTransaction.writeBuf = writeDataBuf;
        i2cTransaction.writeCount = 2;
        i2cTransaction.readBuf = readDataBuf;
        i2cTransaction.readCount = 0;
        i2cTransaction.slaveAddress = codecI2CAddr;

        do{
            i2c_status = I2C_transferTimeout(handle, &i2cTransaction, (uint32_t)AUDIOHAL_I2C_TIMEOUT);

            if(I2C_STATUS_SUCCESS == i2c_status){
                /* Everything went right - Do nothing*/
            }
            else if(I2C_STATUS_TIMEOUT == i2c_status){
                /* I2C timeout */
                if(0 == i2c_remainingTries){
                    return (false);
                }
                else{
                    i2c_remainingTries --;
                }
            }
            else{
                /* Unexpected error */
                return (false);
            }

        }while(i2c_status != I2C_STATUS_SUCCESS);

        /* Write to the register
         *    Note: If a timeout occurs during this I2C transfer, then
         *    the first I2C transfer must be redone too.
         */
        i2cTransaction.writeBuf = writeDataBuf;
        i2cTransaction.writeCount = 1;
        i2cTransaction.readBuf = readDataBuf;
        i2cTransaction.readCount = 1;

        i2c_status = I2C_transferTimeout(handle, &i2cTransaction, AUDIOHAL_I2C_TIMEOUT);

        if(I2C_STATUS_SUCCESS == i2c_status){
            /* Everything went right - Do nothing*/
        }
        else if(I2C_STATUS_TIMEOUT == i2c_status){
            /* I2C timeout */
            if(0 == i2c_remainingTries){
                return (false);
            }
            else{
                i2c_remainingTries --;
            }
        }
        else{
            /* Unexpected error */
            return (false);
        }

    }while(i2c_status != I2C_STATUS_SUCCESS);

    return (true);
    #endif /* For CC26x2, CC13x2 and CC32xx devices */
}

I2C_Handle AudioHAL_I2C_open(uint_least8_t i2cIdx)
{
    I2C_Handle i2cHandle;
    I2C_Params i2cParams;

    /* Setup I2C params and open */
    I2C_Params_init(&i2cParams);

    /* Use fast bitrate, blocking mode */
    i2cParams.bitRate = I2C_400kHz;
    i2cParams.transferMode = I2C_MODE_BLOCKING;

    /* Open the driver and check that it succeeded */
    i2cHandle = I2C_open(i2cIdx, &i2cParams);
    if(i2cHandle == NULL)
    {
        return (NULL);
    }

    return (i2cHandle);
}

bool AudioHAL_I2C_writeReg(I2C_Handle i2cHandle ,
                                    uint8_t codecI2CAddr,
                                    uint8_t pageSelAddr,
                                    uint8_t page,
                                    uint8_t registerAddr,
                                    uint8_t registerValue)
{
    /* This memory will persist to track current page */
    static uint8_t currentPage = 0xFF;

    /* If needed, first, change the page */
    if(page != currentPage)
    {
        if(i2cWrite(i2cHandle, codecI2CAddr, pageSelAddr, page))
        {
            currentPage = page;
        }
        else
        {
            return (false);
        }
    }

    /* If page change succeeded, write the register */
    return (i2cWrite(i2cHandle, codecI2CAddr, registerAddr, registerValue));
}

bool AudioHAL_I2C_runScript( I2C_Handle i2cHandle , uint8_t i2cAddr,
                                    uint8_t pageSelAddr,
                                    const AudioHAL_I2CScript *script)
{
    uint32_t idx;
    bool ret = true;
    for(idx = (uint32_t)0U; idx < script->size; idx++)
    {
        ret = AudioHAL_I2C_writeReg(i2cHandle, i2cAddr, pageSelAddr,
                                script->data[idx].page, script->data[idx].addr,
                                script->data[idx].value);

        if(!ret)
        {
            /* If any of the script elements failed, report fail */
            return (false);
        }
    }

    return (true);
}

void AudioHAL_I2C_close(I2C_Handle i2cHandle)
{
    I2C_close(i2cHandle);
}
