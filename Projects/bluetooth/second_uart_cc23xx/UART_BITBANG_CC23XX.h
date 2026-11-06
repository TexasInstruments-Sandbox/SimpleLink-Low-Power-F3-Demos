/*
 * BB_UART_TIMER.h
 *
 *  Created on: Apr 10, 2025
 *      Author: a0232184
 */

#ifndef BITBANG_UART_UART_BITBANG_CC23XX_H_
#define BITBANG_UART_UART_BITBANG_CC23XX_H_

#include <stdint.h>

#define UART_BITBANG_NUM (1)

#define UART_BITBANG_CONFIG_DEFAULT (0)

extern sem_t    sem1;

extern uint8_t mainThreadBuffer[0xFF];

typedef struct
{
    uint32_t baudRate;
    uint32_t overClockFactor;
} UART_BitBang_Params;

typedef struct {
    uint8_t isOpen;
} UART_BitBang_Object;

typedef struct {
    uint32_t timerId;
    uint32_t timerIntId;
} TimerInfo;

typedef struct {
    UART_BitBang_Object *obj;
    uint32_t id;
    uint32_t rxDio;
    TimerInfo timer;
} UART_BitBang_Config;
typedef UART_BitBang_Config* UART_BitBang_Handle; // TODO: for now this will not really be used



extern UART_BitBang_Handle UART_BitBang_open(const uint32_t index, const UART_BitBang_Params *p);

extern int_fast16_t UART_BitBang_read(UART_BitBang_Handle h, void *buffer, size_t size, size_t *bytesRead);

extern void UART_BitBang_close(UART_BitBang_Handle h);

extern void swUARTRead(uint8_t size);

extern uint8_t mainThreadBuffer[0xFF];

extern void bitbang_read(uint8_t size);

extern void bitbang_open(void);

extern void toggle_every_8p75us_polling(uint32_t pulses /*0 = forever*/);


#endif /* BITBANG_UART_UART_BITBANG_CC23XX_H_ */
