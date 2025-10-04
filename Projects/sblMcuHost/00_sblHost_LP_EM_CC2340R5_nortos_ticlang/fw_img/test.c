/*
 * test.c
 *
 *  Created on: Aug 4, 2025
 *      Author: a0232184
 */


#include "test.h"
#include "fw_img_iter.h"

extern const unsigned char fw_ver_1_mainFlash[];
extern const size_t fw_ver_1_mainFlash_sizeof;
extern const unsigned char fw_ver_1_ccfgFlash[];
extern const size_t fw_ver_1_ccfgFlash_sizeof;
extern const unsigned char fw_ver_2_mainFlash[];
extern const size_t fw_ver_2_mainFlash_sizeof;
extern const unsigned char fw_ver_2_ccfgFlash[];
extern const size_t fw_ver_2_ccfgFlash_sizeof;

FirmwareImageHandle_t testImgFwi_mainFlash_v1;

FirmwareImageHandle_t testImgFwi_ccfgFlash_v1;

FirmwareImageHandle_t testImgFwi_mainFlash_v2;

FirmwareImageHandle_t testImgFwi_ccfgFlash_v2;


FwAccessFxns_t testFwAccessFxns;

Status_e testOpen()
{
    Status_e s = FWI_STATUS_OK;

    // replace this in case your storage medium requires additional access logic,
    // such as external SPI flash

    return s;
}
size_t testRead(FirmwareImageHandle_t *fw, void *dst, size_t numBytes, void *readStartAddr)
{
    size_t s = numBytes;

    // replace this in case your storage medium requires additional access logic,
    // such as external SPI flash
    memcpy(dst, readStartAddr, numBytes);

    return s;
}
size_t testWrite(FirmwareImageHandle_t *fw, void *src, size_t numBytes, void *writeSartAddr)
{
    size_t s = numBytes;

    // replace this in case your storage medium requires additional access logic,
    // such as external SPI flash
    memcpy(writeSartAddr, src, numBytes);

    return s;
}
Status_e testClose()
{
    Status_e s = FWI_STATUS_OK;

    // replace this in case your storage medium requires additional access logic,
    // such as external SPI flash

    return s;
}

// #define BUF_SZ_BYTES   (0x800)
// static uint8_t buf[BUF_SZ_BYTES];

void test_init()
{
    testImgFwi_mainFlash_v1.start = fw_ver_1_mainFlash;
    testImgFwi_mainFlash_v1.sizeBytes = fw_ver_1_mainFlash_sizeof;
    testImgFwi_mainFlash_v1.iterOffset = 0;

    testImgFwi_ccfgFlash_v1.start = fw_ver_1_ccfgFlash;
    testImgFwi_ccfgFlash_v1.sizeBytes = fw_ver_1_ccfgFlash_sizeof;
    testImgFwi_ccfgFlash_v1.iterOffset = 0;

    testImgFwi_mainFlash_v2.start = fw_ver_2_mainFlash;
    testImgFwi_mainFlash_v2.sizeBytes = fw_ver_2_mainFlash_sizeof;
    testImgFwi_mainFlash_v2.iterOffset = 0;

    testImgFwi_ccfgFlash_v2.start = fw_ver_2_ccfgFlash;
    testImgFwi_ccfgFlash_v2.sizeBytes = fw_ver_2_ccfgFlash_sizeof;
    testImgFwi_ccfgFlash_v2.iterOffset = 0;

    testFwAccessFxns.open = testOpen;
    testFwAccessFxns.read = testRead;
    testFwAccessFxns.write = testWrite;
    testFwAccessFxns.close = testClose;
}

FirmwareIterator_t getFwi(FirmwareImageHandle_t *fw, FwAccessFxns_t *fxns)
{
    FirmwareIterator_t fwi;
//    if (!(fw_open(&fwi, &fw, &fxns))) {
//        while (1); // error
//    }

    return fwi;

//     size_t numItemsRead = 0;
//     while ((numItemsRead = fw_read(buf, 1, BUF_SZ_BYTES, &fwi)))
//     {
//         fw_write(buf, numItemsRead, &fwi);
//     }
}

