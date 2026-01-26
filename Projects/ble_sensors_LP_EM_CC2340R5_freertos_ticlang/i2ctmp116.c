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
 *    ======== i2ctmp116.c ========
 */
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

/* POSIX Header files */
#include <pthread.h>
#include <semaphore.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/I2C.h>
//#include <ti/display/Display.h>

/* Module Header */
#include <sensors/tmp116/tmp116.h>

/* Driver configuration */
#include "ti_drivers_config.h"
#include <ti/ble/app_util/framework/bleapputil_api.h>

//static Display_Handle display;

#define SAMPLE_TIME     1        /*In seconds*/
#define HIGH_LIMIT      30.0F
#define LOW_LIMIT       20.0F

#define TMP_TASK_STACK_SIZE   768

#define CONFIG_TMP116            0
#define CONFIG_TMP116_ON         1
#define CONFIG_TMP116_COUNT      1

TMP116_Handle  tmp116Handle = NULL;
// I2C_Handle     i2cHandle    = NULL;
extern I2C_Handle i2cHandle;

sem_t tmp116Sem;

/* Please check <ti/sail/tmp116/tmp116.h> file to know more about TMP116_HWAttrs and TMP116_Config structures */
TMP116_Object TMP116_object[CONFIG_TMP116_COUNT];
const TMP116_HWAttrs TMP116_hwAttrs[CONFIG_TMP116_COUNT] = {
        {
            .slaveAddress = TMP116_SA1,
            .gpioIndex = CONFIG_GPIO_TMP116_INT,
        },
};
const TMP116_Config TMP116_config[] = {
    {
        .hwAttrs = &TMP116_hwAttrs[0],
        .object =  &TMP116_object[0],
    },
    {NULL, NULL},
};

/* Global values which may be accessed from GUI Composer App */
float temp;
uint8_t bleTemp;

/* Global sample rate which may be accessed and set from GUI Composer App */
volatile uint16_t sampleTime;

extern void settmpNotification();

/*
 *  ======== tmp116Callback ========
 *  When an ALERTing condition is met on the TMP116 hardware, the ALERT pin
 *  is asserted generating an interrupt. This callback function serves as
 *  an ISR for a single TMP116 sensor.
 */
void tmp116Callback(uint_least8_t index)
{
    sem_post(&tmp116Sem);
}

/*
 *  ======== tmp116AlertTask ========
 *  This task is unblocked when the ALERT pin is asserted and generates an
 *  interrupt. When the TMP116 is in INTERRUPT mode, the status register must
 *  be read to clear the ALERT pin.
 */
void *tmp116AlertTask(void *arg0)
{
    uint16_t data;

    while(1) {

        /* Pend on semaphore, tmp116Sem */
        if (0 == sem_wait(&tmp116Sem)) {

            /* Reads status register, resetting the ALERT pin */
            TMP116_readStatus(tmp116Handle, &data);

            /* Check Object Temperature High Flag */
            if (data & TMP116_STAT_ALR_HI) {
//                Display_print0(display, 0, 0, "ALERT: Object Temperature High\n");
            }

            /* Check Object Temperature Low Flag */
            if (data & TMP116_STAT_ALR_LO) {
//                Display_print0(display, 0, 0, "ALERT: Object Temperature Low\n");
            }

        }
    }
}

/*
 *  ======== mainThread ========
 */
void *maintmpThread(void *arg0)
{
    // I2C_Handle      i2cHandle;
    // I2C_Params      i2cParams;
    int             retc;
    pthread_t alertTask;
    pthread_attr_t       pAttrs;
    float hiLim = HIGH_LIMIT;
    float loLim = LOW_LIMIT;
    sampleTime = SAMPLE_TIME;

    TMP116_Params  tmp116Params;

    /* Call driver init functions */
    GPIO_init();
//    I2C_init();
    TMP116_init();

    /* Open the HOST display for output */
    // display = Display_open(Display_Type_UART, NULL);
    // if (display == NULL) {
    //     while (1);
    // }

    /* Turn on user LED and TMP116 Sensor */
//    GPIO_write(CONFIG_LED_0_GPIO, CONFIG_GPIO_LED_ON);
    GPIO_write(CONFIG_GPIO_TMP116_PWR, CONFIG_TMP116_ON);

//    Display_print0(display, 0, 0, "Starting the i2ctmp116 example\n");

    /* Create I2C for usage */
//     I2C_Params_init(&i2cParams);

//     i2cParams.bitRate = I2C_400kHz;

//     i2cHandle = I2C_open(CONFIG_I2C_TMP, &i2cParams);
//     if (i2cHandle == NULL) {
// //        Display_print0(display, 0, 0, "Error Initializing I2C\n");
//         while (1);
//     }
//     else {
// //        Display_print0(display, 0, 0, "I2C Initialized!\n");
//     }

    if(0 != sem_init(&tmp116Sem,0,0))
    {
        /* sem_init() failed */
//        Display_print0(display, 0, 0, "tmp116Sem Semaphore creation failed");
        while (1);
    }

    pthread_attr_init(&pAttrs);
    pthread_attr_setstacksize(&pAttrs, TMP_TASK_STACK_SIZE);
    retc = pthread_create(&alertTask, &pAttrs, tmp116AlertTask, NULL);
    if (retc != 0) {
        /* pthread_create() failed */
//        Display_print0(display, 0, 0, "Alert Task creation failed");
        while (1);
    }

    /* Initialize tmp116Params structure to defaults */
    TMP116_Params_init(&tmp116Params);

    /* Callback for ALERT event */
    tmp116Params.callback = &tmp116Callback;

    /* Open TMP116 sensor with custom Params */
    tmp116Handle = TMP116_open(CONFIG_TMP116, i2cHandle, &tmp116Params);

    /* Check if the open is successful */
    if (tmp116Handle == NULL) {
//        Display_print0(display, 0, 0, "TMP116_ROOMTEMP0 Open Failed!");
        while(1);
    }

    /* Allow the sensor hardware to complete its first conversion */
    sleep(2);

    /* Set Object Temperature Alert Limits */
    if (!TMP116_setTempLimit(tmp116Handle, TMP116_CELSIUS, hiLim, loLim)) {
//        Display_print0(display, 0, 0, "Setting Object Temperature Limits Failed!");
    }

    // Display_print1(display, 0, 0, "Temperature High Limit set: %f (C)\n",
    //                 hiLim);
    // Display_print1(display, 0, 0, "Temperature Low Limit set: %f (C)\n",
    //                 loLim);

    // Display_print0(display, 0, 0, "Taking Preliminary Readings...\n\n");

    /* Get Die Temperature in Celsius */
    if (!TMP116_getTemp(tmp116Handle, TMP116_CELSIUS, &temp)) {
//        Display_print0(display, 0, 0, "TMP116 sensor read failed");
    }

//    Display_print1(display, 0, 0, "Temperature: %f (C)\n", temp);

    /* Begin infinite task loop */
    while (1) {

        /* Get Die and Object Temperatures */
        if (!TMP116_getTemp(tmp116Handle, TMP116_CELSIUS, &temp)) {
//            Display_print0(display, 0, 0, "TMP116 sensor read failed");

        }

        // Display_print1(display, 0, 0, "Temp: %f (C)\n\n", temp);
        // Display_clear(display);

        bleTemp = (uint8_t)temp;

        BLEAppUtil_invokeFunctionNoData(settmpNotification);

        sleep(sampleTime);
    }
}
