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
 *    ======== i2chdc2010.c ========
 */
#include <stdint.h>
#include <stddef.h>

/* POSIX Header files */
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/I2C.h>
//#include <ti/display/Display.h>

/* Module Header */
#include <sensors/hdc2010/hdc2010.h>

/* Driver configuration */
#include "ti_drivers_config.h"
#include <ti/ble/app_util/framework/bleapputil_api.h>

#define SAMPLE_TIME     1        /*In seconds*/
#define HIGH_LIMIT      30.0F
#define LOW_LIMIT       20.0F

#define HDC_TASK_STACK_SIZE   768

#define CONFIG_HDC2010          0
#define CONFIG_HDC2010_ON       (0)
#define CONFIG_HDC2010_COUNT    1

/* Please check <ti/sail/hdc2010/hdc2010.h> file to know more about HDC2010_HWAttrs and HDC2010_Config structures */
HDC2010_Object HDC2010_object[CONFIG_HDC2010_COUNT];

const HDC2010_HWAttrs HDC2010_hwAttrs[CONFIG_HDC2010_COUNT] = {
        {
            .slaveAddress = HDC2010_SA1,
            .gpioIndex = CONFIG_GPIO_HDC2010_INT,
        },
};

const HDC2010_Config HDC2010_config[] = {
    {
        .hwAttrs = &HDC2010_hwAttrs[0],
        .object =  &HDC2010_object[0],
    },
    {NULL, NULL},
};

HDC2010_Handle  hdc2010Handle = NULL;
//I2C_Handle      i2cHandle    = NULL;
extern I2C_Handle i2cHandle;

sem_t hdc2010Sem;

float     g_actualTemp   = 0;
float     g_actualHumity = 0;

uint16_t bleActualTemp;
uint16_t bleActualHumity;

//static Display_Handle display;

/* Global values which may be accessed from GUI Composer App */
float temp;

/* Global sample rate which may be accessed and set from GUI Composer App */
volatile uint16_t sampleTime;

extern void sethdcNotification();

/*
 *  ======== hdc2010Callback ========
 *  When an ALERTing condition is met on the HDC2010 hardware, the ALERT pin
 *  is asserted generating an interrupt. This callback function serves as
 *  an ISR for a single HDC2010 sensor.
 */
void hdc2010Callback(uint_least8_t index)
{
    sem_post(&hdc2010Sem);
}

/*
 *  ======== hdc2010AlertTask ========
 *  This task is unblocked when the ALERT pin is asserted and generates an
 *  interrupt. When the HDC2010 is in INTERRUPT mode, the status register must
 *  be read to clear the ALERT pin.
 */
void *hdc2010AlertTask(void *arg0)
{

    while(1) {

        /* Pend on semaphore, hdc2010Sem */
        if (0 == sem_wait(&hdc2010Sem)) {

//            Display_print0(display, 0, 0, "ALERT: \n");

//            /* Reads status register, resetting the ALERT pin */
//            HDC2010_readStatus(hdc2010Handle, &data);
//
//            /* Check Object Temperature High Flag */
//            if (data & HDC2010_STAT_ALR_HI) {
//                Display_print0(display, 0, 0, "ALERT: Object Temperature High\n");
//            }
//
//            /* Check Object Temperature Low Flag */
//            if (data & HDC2010_STAT_ALR_LO) {
//                Display_print0(display, 0, 0, "ALERT: Object Temperature Low\n");
//            }

        }
    }
}


/*
 *  ======== mainThread ========
 */
void *mainhdcThread(void *arg0)
{
    // I2C_Handle      i2cHandle;
    // I2C_Params      i2cParams;
    int             retc;
    pthread_t       alertTask;
    pthread_attr_t  pAttrs;
    float hiLim = HIGH_LIMIT;
    float loLim = LOW_LIMIT;
    sampleTime = SAMPLE_TIME;

    HDC2010_Params  hdc2010Params;

    /* Call driver init functions */
    GPIO_init();
//    I2C_init();
    HDC2010_init();

    /* Open the HOST display for output */
    // display = Display_open(Display_Type_UART, NULL);
    // if (display == NULL) {
    //     while (1);
    // }

    /* Turn on user LED and HDC2010 Sensor */
//    GPIO_write(CONFIG_LED_0_GPIO, CONFIG_GPIO_LED_ON);
    GPIO_write(CONFIG_HDC2010, CONFIG_HDC2010_ON);

//    Display_print0(display, 0, 0, "Starting the i2chdc2010 sensor example...\n\n");

    /* Create I2C for usage */
//     I2C_Params_init(&i2cParams);

//     i2cParams.bitRate = I2C_100kHz;

//     i2cHandle = I2C_open(CONFIG_I2C_HDC, &i2cParams);
//     if (i2cHandle == NULL) {
// //        Display_print0(display, 0, 0, "Error Initializing I2C\n");
//     }
//     else {
// //        Display_print0(display, 0, 0, "I2C Initialized!\n");
//     }

    if(0 != sem_init(&hdc2010Sem,0,0))
    {
        /* sem_init() failed */
//        Display_print0(display, 0, 0, "hdc2010Sem Semaphore creation failed");
        while (1);
    }

    pthread_attr_init(&pAttrs);
    pthread_attr_setstacksize(&pAttrs, HDC_TASK_STACK_SIZE);
    retc = pthread_create(&alertTask, &pAttrs, hdc2010AlertTask, NULL);
    if (retc != 0) {
        /* pthread_create() failed */
//        Display_print0(display, 0, 0, "Alert Task creation failed");
        while (1);
    }

    /* Initialize hdc2010Params structure to defaults */
    HDC2010_Params_init(&hdc2010Params);

    hdc2010Params.humResolution = HDC2010_H14_BITS;
    hdc2010Params.tempResolution = HDC2010_T14_BITS;
    hdc2010Params.interruptPinPolarity = HDC2010_ACTIVE_LO;
    hdc2010Params.interruptEn = HDC2010_ENABLE_MODE;
    hdc2010Params.interruptMask = (HDC2010_InterruptMask) (HDC2010_TH_MASK | HDC2010_TL_MASK | HDC2010_HH_MASK | HDC2010_HL_MASK);
    hdc2010Params.interruptMode = HDC2010_COMP_MODE;
    hdc2010Params.measurementMode = HDC2010_HT_MODE;
    hdc2010Params.callback = &hdc2010Callback;

    /* Open HDC2010 sensor with custom Params */
    hdc2010Handle = HDC2010_open(CONFIG_HDC2010, i2cHandle, &hdc2010Params);

    /* Check if the open is successful */
    if (hdc2010Handle == NULL) {
//        Display_print0(display, 0, 0, "HDC2010 Open Failed!");
        while(1);
    }

    /* Allow the sensor hardware to complete its first conversion */
    sleep(2);

    /* Set Object Temperature Alert Limits */
    if (!HDC2010_setTempLimit(hdc2010Handle, HDC2010_CELSIUS, hiLim, loLim)) {
//        Display_print0(display, 0, 0, "Setting Object Temperature Limits Failed!");
    }

    // Display_print1(display, 0, 0, "Temperature High Limit set: %f (C)\n",hiLim);
    // Display_print1(display, 0, 0, "Temperature Low Limit set: %f (C)\n",loLim);

    if (!HDC2010_setHumLimit(hdc2010Handle, hiLim, loLim)) {
//        Display_print0(display, 0, 0, "Setting Object Temperature Limits Failed!");
    }

    // Display_print1(display, 0, 0, "Humidity High Limit set: %f (%)\n",hiLim);
    // Display_print1(display, 0, 0, "Humidity Low Limit set: %f (%)\n",loLim);

    HDC2010_triggerMeasurement(hdc2010Handle);

    while(1)
    {
        if (!HDC2010_getTemp(hdc2010Handle, HDC2010_CELSIUS, &g_actualTemp) & !HDC2010_getHum(hdc2010Handle, &g_actualHumity)) {
//            Display_print0(display, 0, 0, "HDC2010 sensor read failed");
        }
//        Display_print2(display, 0, 0, "%f DegC(Temp), %f %%RH(Humidity)\n", g_actualTemp, g_actualHumity);

        bleActualTemp = (uint8_t)g_actualTemp;
        bleActualHumity = (uint8_t)g_actualHumity;
        BLEAppUtil_invokeFunctionNoData(sethdcNotification);

        sleep(1);
    }
}
