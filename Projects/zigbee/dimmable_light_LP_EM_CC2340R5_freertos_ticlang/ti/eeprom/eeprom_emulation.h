/*
 * Copyright (c) 2025, Texas Instruments Incorporated
 * All rights reserved.
 *
 * EEPROM Emulation using NVS Driver
 * Wrapper for DALI library EEPROM functions
 */

#ifndef EEPROM_EMULATION_H_
#define EEPROM_EMULATION_H_

#include <stdint.h>
#include <stdbool.h>

/* EEPROM Emulation Data Size (52 bytes as per DALI requirements) */
#define EEPROM_EMULATION_DATA_SIZE (52)

/* EEPROM Initialization States */
#define EEPROM_EMULATION_INIT_OK (0)
#define EEPROM_EMULATION_INIT_ERROR (1)

/* Global EEPROM flags */
extern volatile bool gEEPROMTypeASearchFlag;
extern volatile bool gEEPROMTypeAEraseFlag;

/**
 * @brief Initialize EEPROM Type A emulation using NVS
 * @param data Pointer to data buffer
 * @return EEPROM_EMULATION_INIT_OK on success
 */
uint32_t EEPROM_TypeA_init(uint32_t *data);

/**
 * @brief Write data to EEPROM emulation
 * @param data Pointer to data to write
 */
void EEPROM_TypeA_writeData(uint32_t *data);

/**
 * @brief Erase last sector of EEPROM emulation
 */
void EEPROM_TypeA_eraseLastSector(void);

#endif /* EEPROM_EMULATION_H_ */
