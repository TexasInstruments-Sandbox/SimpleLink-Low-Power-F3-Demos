/*
 * Copyright (c) 2016-2020, Texas Instruments Incorporated
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
 *  ======== adcdrv5055.c ========
 */
#include <stdint.h>
#include <stddef.h>

/* POSIX Header files */
#include <pthread.h>
#include <unistd.h>

/* Driver Header files */
#include <ti/drivers/ADC.h>
//#include <ti/display/Display.h>

/* Module Header */
#include <sensors/drv5055/drv5055.h>

/* Driver configuration */
#include "ti_drivers_config.h"
#include <ti/ble/app_util/framework/bleapputil_api.h>

#define THREADSTACKSIZE   (768)
/* change the sensitivity option according to the DRV5055 part number used*/
#define SENSITIVITY DRV5055A4_3_3V
/* Change the offset value according to the working environment. For environment without 
 *magnetic field the value of offset should be 1.65V for a 3.3V power supply
*/
#define OFFSET 1650.0f

float magneticFlux;
uint16_t bleMagneticFlux;

//static Display_Handle display;

extern void setdrvNotification();

/*
 *  ======== drv5055ReadFxn ========
 *  Open a ADC handle and get a array of sampling results after
 *  calling several conversions.
 */
void *drv5055ReadFxn(void *arg0)
{
    ADC_Handle   adc;
    ADC_Params   params;

    ADC_Params_init(&params);
    adc = ADC_open(CONFIG_ADC_DRV5055, &params);

    if (adc == NULL) {
//        Display_printf(display, 0, 0, "Error initializing ADC channel 1\n");
        while (1);
    }

    while (1) {
        magneticFlux = DRV5055_getMagneticFlux(adc, SENSITIVITY, OFFSET, DRV5055_3_3V);
//        Display_printf(display, 0, 0, "Magnetic Flux : %f mT\n", magneticFlux);

        bleMagneticFlux = (uint16_t)magneticFlux;
        BLEAppUtil_invokeFunctionNoData(setdrvNotification);

        sleep(2U);
    }

}

/*
 *  ======== mainThread ========
 */
void *maindrvThread(void *arg0)
{
    pthread_t           thread1;
    pthread_attr_t      attrs;
    struct sched_param  priParam;
    int                 retc;
    int                 detachState;

    /* Call driver init functions */
    ADC_init();
//    Display_init();

    /* Open the display for output */
    // display = Display_open(Display_Type_UART, NULL);
    // if (display == NULL) {
    //     /* Failed to open display driver */
    //     while (1);
    // }

    // Display_printf(display, 0, 0, "Starting the DRV5055 example\n");

    /* Create application threads */
    pthread_attr_init(&attrs);

    detachState = PTHREAD_CREATE_DETACHED;
    /* Set priority and stack size attributes */
    retc = pthread_attr_setdetachstate(&attrs, detachState);
    if (retc != 0) {
        /* pthread_attr_setdetachstate() failed */
        while (1);
    }

    retc |= pthread_attr_setstacksize(&attrs, THREADSTACKSIZE);
    if (retc != 0) {
        /* pthread_attr_setstacksize() failed */
        while (1);
    }

    /* Create drv5055ReadFxn0 thread */
    priParam.sched_priority = 1;
    pthread_attr_setschedparam(&attrs, &priParam);

    /* Create drv5055ReadFxn thread */
    retc = pthread_create(&thread1, &attrs, drv5055ReadFxn, (void* )0);
    if (retc != 0) {
        /* pthread_create() failed */
        while (1);
    }

    return (NULL);
}
