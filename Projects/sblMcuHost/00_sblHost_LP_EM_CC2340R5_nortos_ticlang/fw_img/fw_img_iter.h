/*
 * fw_img_iter.h
 *
 *  Created on: Aug 4, 2025
 *      Author: a0232184
 */

#ifndef FW_IMG_FW_IMG_ITER_H_
#define FW_IMG_FW_IMG_ITER_H_


#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum {
    FWI_STATUS_OK = 0,
    FWI_STATUS_NOK,
    FWI_STATUS_BAD_PARAM,
    FWI_STATUS_BUSY,
    FWI_STATUS_EOF,
} Status_e;

typedef enum {
    PERMISSION_READ = (1 << 0),
    PERMISSION_WRITE = (1 << 1)
} Permissions_e;

typedef struct {
    uint8_t *start;
    size_t sizeBytes;
    size_t iterOffset;
    Permissions_e permissions;
} FirmwareImageHandle_t;

typedef struct {
    Status_e (*open)();
    size_t   (*read)(FirmwareImageHandle_t *fw, void *dst, size_t numBytes, void *startAddr);
    size_t   (*write)(FirmwareImageHandle_t *fw, void *src, size_t numBytes, void *startAddr);
    Status_e (*close)();
} FwAccessFxns_t;

typedef struct {
    FirmwareImageHandle_t *image;
    FwAccessFxns_t *fxns;
} FirmwareIterator_t;

extern Status_e fw_open(FirmwareIterator_t *callerFwi, FirmwareImageHandle_t *fwh, FwAccessFxns_t *fxns, Permissions_e permits);

// rewind, so that the next "get" will return the start of the fw
extern Status_e fw_rewind(FirmwareIterator_t *fwi);

// check if end of fw reached
extern bool fw_eof(FirmwareIterator_t *fwi);

// get next piece of fw.
// assumes buf is a caller allocated buffer large enough to accommodate
// return: number of items read
extern size_t fw_read(void *dst, size_t itemSizeBytes, size_t numItems, FirmwareIterator_t *fwi);

// closes interface to fw.
// return:
//      0: ok
//      -1: nok
extern Status_e fw_close(FirmwareIterator_t *fwi);

// check size
// return: size of fw in bytes
extern size_t fw_size(FirmwareIterator_t *fwi);

#endif /* FW_IMG_FW_IMG_ITER_H_ */
