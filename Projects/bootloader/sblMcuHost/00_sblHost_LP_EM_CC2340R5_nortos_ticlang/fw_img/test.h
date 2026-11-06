/*
 * test.h
 *
 *  Created on: Aug 4, 2025
 *      Author: a0232184
 */

#ifndef FW_IMG_TEST_H_
#define FW_IMG_TEST_H_

#include <stdint.h>
#include <stdlib.h>
#include "fw_img_iter.h"

extern FirmwareImageHandle_t testImgFwi_mainFlash_v1;
extern FirmwareImageHandle_t testImgFwi_ccfgFlash_v1;
extern FirmwareImageHandle_t testImgFwi_mainFlash_v2;
extern FirmwareImageHandle_t testImgFwi_ccfgFlash_v2;

extern FwAccessFxns_t testFwAccessFxns;

extern void test_init();
extern FirmwareIterator_t getFwi(FirmwareImageHandle_t *fw, FwAccessFxns_t *fxns);

extern void loadFw2();

#endif /* FW_IMG_TEST_H_ */
