/******************************************************************************

 @file sblUart.c

 @brief CC23xx Bootloader Platform Specific UART functions

 Group: WCS LPC
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2025 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include "sblUart.h"
#include <ti/drivers/UART2.h>

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

/************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * LOCAL VARIABLES
 */
static UART2_Handle serialPortFd = 0;

/*********************************************************************
 * API FUNCTIONS
 */

/*
 * return:
 *      - 0: ok
 *      - -1: nok
 */
int SblUart_open(void *uartPath, const char *deviceType)
{
    (void *) deviceType; // not used for now

    if (serialPortFd != 0)
    {
        // already allocated
        return 0;
    }

    /* open the device */
    UART2_Params uartParams;
    UART2_Params_init(&uartParams);
    uartParams.baudRate = 115200;
//    uartParams.readMode = UART2_Mode_POLLING;

    serialPortFd = UART2_open((uint_least8_t)uartPath, &uartParams);
    if (!serialPortFd)
    {
        return -1;
    }

    return 0;
}

bool SblUart_close(void)
{
    bool ret = true;

    UART2_close(serialPortFd);

    return (ret);
}

void SblUart_write(const unsigned char* buf, size_t len)
{
    UART2_write(serialPortFd, buf, len, 0);
}

unsigned char SblUart_read(unsigned char* buf, size_t len)
{
    unsigned char ret = UART2_read(serialPortFd, buf, len, 0);

    return (ret);
}
