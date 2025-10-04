/*
 * Copyright (c) 2015-2019, Texas Instruments Incorporated
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

/*
 *  ======== empty.c ========
 */

/* For usleep() */
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
// #include <ti/drivers/I2C.h>
// #include <ti/drivers/SPI.h>
// #include <ti/drivers/Watchdog.h>
#include <ti/drivers/UART2.h>

/* Driver configuration */
#include "ti_drivers_config.h"

#include <stdint.h>
#include "fw_img/test.h"
#include "cc23xxdnld.h"
#include "sblUart.h"

typedef enum {
    SblOpt_Erase   = (1 << 0),
    SblOpt_Program = (1 << 1),
    SblOpt_Verify  = (1 << 2),
} SblOpts;

typedef struct {
    uint8_t hostBackdoorPin;
    uint8_t devActiveLevel;
} SblConfig_t;

typedef void (*Bootloader_enter)();
typedef void (*Bootloader_exit)();

typedef struct {
    SblOpts optionFlags;
    void *selectedSerialPort;
    const char *deviceType;
    size_t startAddr_main;
    size_t startAddr_ccfg;
    FirmwareImageHandle_t *flashMain;
    FirmwareImageHandle_t *flashCcfg;
    Bootloader_enter blEnter;
    Bootloader_exit blExit;
} TestConfigSblHost_t;

static uint8_t uartReadByte(void)
{
    uint8_t readByte;
    SblUart_read(&readByte, 1);
    return readByte;
}

static void uartWriteByte(uint8_t byte)
{
    SblUart_write(&byte, 1);
}

static void uartWrite(uint8_t *data, unsigned int len)
{
    SblUart_write(data, len);
}

static void connectDelay()
{
    usleep(51); // tested to work in previous setting
}

static void targetReset_app()
{
    GPIO_write(CONFIG_GPIO_SBL_RST, 0); // active low per datasheet
    usleep(10000); // assume 10msec hold time is enough
    GPIO_write(CONFIG_GPIO_SBL_RST, 1);
}

static void targetReset_sbl()
{
    CcDnld_reset(DEVICE_CC23XX);
    usleep(10000); // assume 10msec hold time is enough
}
static void bootloaderEnter()
{
    GPIO_write(CONFIG_GPIO_SBL_BACKDOOR, 0); // assume active low
    targetReset_app(); // reset the target while it is running an application
    usleep(1000); // give a bit of time for the SBL to detect the backdoor pin
}

static void bootloaderExit()
{
    GPIO_write(CONFIG_GPIO_SBL_BACKDOOR, 1); // assume active low
    targetReset_sbl(); // reset the target while it is running ROM bootloader
}

static uint8_t portIsOpen = 0;

static CcDnld_UartFxns_t sblUartFxns =
{
     .sbl_UartReadByte = uartReadByte,
     .sbl_UartWriteByte = uartWriteByte,
     .sbl_UartWrite = uartWrite,
     .sbl_ConnectSeqDelay = connectDelay,
};

#define DATA_BUFFER_SIZE (2*CCNDLD_CC23XX_PAGE_SIZE)
uint8_t pData[DATA_BUFFER_SIZE];
static CcDnld_Status downloadFile(FirmwareIterator_t *fwi, uint32_t startAddr, const char * deviceType)
{
    CcDnld_Status ccDnldStatus = CcDnld_Status_Success;
    unsigned int dataIdx = 0;
    unsigned int bytesInTransfer;
    unsigned int bytesInBuffer = 0;
    unsigned int bytesLeft;

    ccDnldStatus = CcDnld_startDownload(startAddr, fw_size(fwi), deviceType);
    if (ccDnldStatus != CcDnld_Status_Success) while (1); // error

    /* set bytesLeft so we know the progress */
    bytesLeft = fw_size(fwi);

    /* make sure the file pointer is at the start of the file */
    fw_rewind(fwi);

    while (!fw_eof(fwi))
    {
        /* check if we need to read more data */
        if(bytesInBuffer == 0)
        {
            dataIdx = 0;
            bytesInBuffer = fw_read(pData, 1, DATA_BUFFER_SIZE, fwi);
        }

        while (bytesInBuffer > 0)
        {
            /* Limit transfer count */
            if (CCDNLD_MAX_BYTES_PER_TRANSFER < bytesInBuffer)
            {
                bytesInTransfer = CCDNLD_MAX_BYTES_PER_TRANSFER;
            }
            else
            {
                bytesInTransfer = bytesInBuffer;
            }

            /* Send Data command */
            ccDnldStatus = CcDnld_sendData(&pData[dataIdx], bytesInTransfer);
            if (ccDnldStatus != CcDnld_Status_Success)
            {
                unsigned int retry = 0;
                /* Allow up to 3 successive retries to send the data to the target */
                while(retry++ < 3 && (ccDnldStatus = CcDnld_sendData(&pData[dataIdx], bytesInTransfer)) != CcDnld_Status_Success);
                if(ccDnldStatus != CcDnld_Status_Success) while (1); // error
            }

            /* Update index and bytesLeft */
            bytesInBuffer -= bytesInTransfer;
            dataIdx += bytesInTransfer;
            bytesLeft -= bytesInTransfer;
        }
    }
    return ccDnldStatus;
}

static CcDnld_Status verifyFile(FirmwareIterator_t *fwi)
{
    CcDnld_Status s = CcDnld_Status_NotImplemented;

    // TODO: determine if this is needed, or if verification happens elsewhere already

    return s;
}

static bool verifyStartAddress(uint32_t pageSize, uint32_t startAddress)
{
    /* Verify that the startAddress is at the top of a page in Flash
    */

    return (startAddress % pageSize) == 0;
}

static void connectToBootloader(void)
{
    while(CcDnld_connect() != CcDnld_Status_Success);
}

volatile int gFwVerToLoad = 1;

/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
//    TestConfigSblHost_t t =
//    {
//         .optionFlags = SblOpt_Erase | SblOpt_Program,
//         .selectedSerialPort = (void *) CONFIG_UART2_SBL,
//         .deviceType = DEVICE_CC23XX,
//         .startAddr_main = 0x00000000,
//         .startAddr_ccfg = 0x4e020000 // size of 0x800, single page
//    };
    volatile TestConfigSblHost_t t =
    {
         .optionFlags = SblOpt_Program,
         .selectedSerialPort = (void *) CONFIG_UART2_SBL,
         .deviceType = DEVICE_CC23XX,
         .startAddr_main = 0x00000000,
         .startAddr_ccfg = CCDNLD_CC23XX_CCFG_ADDRESS, // size of 0x800, single page
         .blEnter = bootloaderEnter,
         .blExit = bootloaderExit,
    };

    GPIO_init();
    GPIO_setConfig(CONFIG_GPIO_SBL_BACKDOOR, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_HIGH);
    GPIO_setConfig(CONFIG_GPIO_SBL_RST, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_HIGH);

    test_init();

    if (CcDnld_getPageSize(t.deviceType) == 0) {
        while (1); // error
    }

    if (SblUart_open(t.selectedSerialPort, t.deviceType) == -1) {
        while (1); // error
    }

    portIsOpen = 1;

    CcDnld_init(&sblUartFxns);

    volatile uint8_t waitingForUser = 1;
    while (1)
    {
        while (waitingForUser)
        {
            /*
             * spin on this loop to allow the developer to:
             *      1. verify that the target is running the intended fw
             *      2. select which FW image to use (gFwVerToLoad)
             * once done, user may exit the while loop by clearing waitingForUser
             */
        }
        waitingForUser = 1;
        t.blEnter();

        /* connect to CC23xx bootloader */
        connectToBootloader();

        /* Now that we know the device type, we can verify the startAddress is valid */
        if (t.startAddr_main && verifyStartAddress( CcDnld_getPageSize(t.deviceType), t.startAddr_main ))
        {
            while (1); // error
        }

        if (t.optionFlags & SblOpt_Erase)
        {
            CcDnld_chipErase(t.deviceType);
        }

        if (t.optionFlags & SblOpt_Program)
        {
            // for CC23xx, chip-erase must happen per program
            CcDnld_chipErase(t.deviceType);

            // for CC23xx, main flash and CCFG flash are separate regions, both must be programmed

            FirmwareIterator_t sblFileHandle;
    //        FirmwareImageHandle_t *mainFlash = &testImgFwi_mainFlash_v1;
    //        FirmwareImageHandle_t *ccfgFlash = &testImgFwi_ccfgFlash_v1;

            if (gFwVerToLoad == 1)
            {
                t.flashMain = &testImgFwi_mainFlash_v1;
                t.flashCcfg = &testImgFwi_ccfgFlash_v1;
            }
            else if (gFwVerToLoad == 2)
            {
                t.flashMain = &testImgFwi_mainFlash_v2;
                t.flashCcfg = &testImgFwi_ccfgFlash_v2;
            }

            // program main flash
            if (!(FWI_STATUS_OK ==
                    fw_open(&sblFileHandle, t.flashMain, &testFwAccessFxns, PERMISSION_READ)))
                while (1); // error
            downloadFile(&sblFileHandle, t.startAddr_main, t.deviceType);
            fw_close(&sblFileHandle);

            // program ccfg flash
            if (!(FWI_STATUS_OK ==
                    fw_open(&sblFileHandle, t.flashCcfg, &testFwAccessFxns, PERMISSION_READ)))
                while (1); // error
            downloadFile(&sblFileHandle, t.startAddr_ccfg, t.deviceType);
            fw_close(&sblFileHandle);
        }

        if (t.optionFlags & SblOpt_Verify)
        {
            // TODO
        }

        t.blExit();
    }

    if(SblUart_close())
    {
        portIsOpen = false;
    }

    while (1)
        ;
}
