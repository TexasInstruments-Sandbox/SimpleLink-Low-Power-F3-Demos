/*
 * Copyright (c) 2025, Texas Instruments Incorporated
 * All rights reserved.
 *
 * EEPROM Emulation using NVS Driver
 * Wrapper for DALI library EEPROM functions
 */

#include "eeprom_emulation.h"
#include <ti/drivers/NVS.h>
#include "ti_drivers_config.h"
#include <string.h>

/* Global EEPROM flags */
volatile bool gEEPROMTypeASearchFlag = false;
volatile bool gEEPROMTypeAEraseFlag = false;

/* NVS handle */
static NVS_Handle nvsHandle = NULL;

/* Local buffer for NVS operations */
static uint8_t nvsBuffer[EEPROM_EMULATION_DATA_SIZE];

/**
 * @brief Initialize EEPROM Type A emulation using NVS
 */
uint32_t EEPROM_TypeA_init(uint32_t *data)
{
    NVS_Params nvsParams;
    int_fast16_t status;

    /* Open NVS driver */
    NVS_Params_init(&nvsParams);
    nvsHandle = NVS_open(CONFIG_NVS_INTERNAL, &nvsParams);

    if (nvsHandle == NULL) {
        return EEPROM_EMULATION_INIT_ERROR;
    }

    /* Try to read existing data from NVS */
    status = NVS_read(nvsHandle, 0, nvsBuffer, EEPROM_EMULATION_DATA_SIZE);

    if (status == NVS_STATUS_SUCCESS) {
        /* Copy from NVS buffer to data array */
        memcpy(data, nvsBuffer, EEPROM_EMULATION_DATA_SIZE);
        gEEPROMTypeASearchFlag = true;
    } else {
        /* No valid data found, NVS is empty or uninitialized */
        gEEPROMTypeASearchFlag = false;
    }

    return EEPROM_EMULATION_INIT_OK;
}

/**
 * @brief Write data to EEPROM emulation
 */
void EEPROM_TypeA_writeData(uint32_t *data)
{
    int_fast16_t status;

    if (nvsHandle == NULL) {
        return;
    }

    /* Copy data to NVS buffer */
    memcpy(nvsBuffer, data, EEPROM_EMULATION_DATA_SIZE);

    /* Erase the region first */
    status = NVS_erase(nvsHandle, 0, EEPROM_EMULATION_DATA_SIZE);
    if (status != NVS_STATUS_SUCCESS) {
        /* Erase failed */
        return;
    }

    /* Write data to NVS */
    status = NVS_write(nvsHandle, 0, nvsBuffer, EEPROM_EMULATION_DATA_SIZE,
                       NVS_WRITE_POST_VERIFY);

    if (status == NVS_STATUS_SUCCESS) {
        gEEPROMTypeASearchFlag = true;
    }
}

/**
 * @brief Erase last sector of EEPROM emulation
 */
void EEPROM_TypeA_eraseLastSector(void)
{
    if (nvsHandle == NULL) {
        return;
    }

    /* Erase the entire NVS region */
    NVS_erase(nvsHandle, 0, EEPROM_EMULATION_DATA_SIZE);

    gEEPROMTypeASearchFlag = false;
}
