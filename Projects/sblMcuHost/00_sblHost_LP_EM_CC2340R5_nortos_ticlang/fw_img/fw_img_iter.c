/*
 * fw_img_iter.c
 *
 *  Created on: Aug 4, 2025
 *      Author: a0232184
 */

#include "fw_img_iter.h"

static int gInUse = 0;

Status_e fw_open(FirmwareIterator_t *callerFwi, FirmwareImageHandle_t *fwh, FwAccessFxns_t *fxns, Permissions_e permits)
{
    Status_e s = FWI_STATUS_OK;

    // check params...
    if (!callerFwi || !fwh || !fxns) s = FWI_STATUS_BAD_PARAM;

    // check if caller is able to open the requested storage medium containing the firmware
    if (!(FWI_STATUS_OK == fxns->open())) s = FWI_STATUS_BUSY;

    // we got access, go ahead and initialize
    memset(callerFwi, 0x00, sizeof(callerFwi));
    callerFwi->image = fwh;
    callerFwi->fxns = fxns;
    callerFwi->image->permissions |= permits;

    return s;
}

// rewind, so that the next "get" will return the start of the fw
Status_e fw_rewind(FirmwareIterator_t *fwi)
{
    Status_e s = FWI_STATUS_OK;

    if (!fwi) s = FWI_STATUS_BAD_PARAM;
    fwi->image->iterOffset = 0;

    return s;
}

// check if end of fw reached
// return: true iff fwi is valid and end of file
bool fw_eof(FirmwareIterator_t *fwi)
{
    return fwi && (fwi->image->iterOffset == fwi->image->sizeBytes);
}

// get next piece of fw.
// assumes buf is a caller allocated buffer large enough to accommodate
// return: number of items read
size_t fw_read(void *dst, size_t itemSizeBytes, size_t numItems, FirmwareIterator_t *fwi)
{
    if (!fwi || !(PERMISSION_READ & fwi->image->permissions)) return 0;

    size_t totalBytesRequested = itemSizeBytes * numItems;
    size_t bytesRemaining = fwi->image->sizeBytes - fwi->image->iterOffset;
    size_t totalBytesToRead = (bytesRemaining > totalBytesRequested) ? totalBytesRequested : bytesRemaining;

    size_t bytesRead = fwi->fxns->read(fwi->image, dst, totalBytesToRead, fwi->image->start + fwi->image->iterOffset);
    fwi->image->iterOffset += bytesRead;

    return bytesRead;
}

// closes interface to fw.
// return:
//      0: ok
//      -1: nok
Status_e fw_close(FirmwareIterator_t *fwi)
{
    Status_e s = FWI_STATUS_OK;

    if (!fwi) s = FWI_STATUS_BAD_PARAM;
    if (!(FWI_STATUS_OK == fwi->fxns->close())) s = FWI_STATUS_NOK;

    return s;
}

// check size
// return:
//      - if fwi is valid: size of fw img in bytes
//      - else: 0
size_t fw_size(FirmwareIterator_t *fwi)
{
    size_t s = 0;

    if (fwi) s = fwi->image->sizeBytes;

    return s;
}

