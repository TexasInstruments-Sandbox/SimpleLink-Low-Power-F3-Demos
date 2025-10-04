/*
 * Copyright (c) 2015-2024, Texas Instruments Incorporated
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
 *  ======== lcd.c ========
 */
/* For usleep() */
#include <unistd.h>
#include <stddef.h>

#include <semaphore.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/BatteryMonitor.h>
#include <ti/drivers/Temperature.h>
#include DeviceFamily_constructPath(inc/hw_systim.h)

/* Driver configuration */
#include "ti_drivers_config.h"

#include <ti/drivers/dpl/ClockP.h>

// Definitions
#define LIGHT_STRENGTH  2               // Backlight intensity (0-4): 0=0%, 1=25%, 2=50%, 3=75%, 4=100%
#define LIGHT_FREQUENCY 1000            // Backlight PWM frequency in Hz
#define REFRESH_RATE    120             // LCD refresh rate in Hz
#define THRESHOLD_DELTA_MILLIVOLT 200   // Voltage threshold for battery monitoring
#define NUM_SEGMENTS 7                  // Number of segments to display
#define NUM_DIGITS 10                   // Number of digits to display = 10: 0, 1, 2, ... , 8, 9.
#define TEMP_UPDATE 10000000            // Number of microseconds before the temperature updates
#define USE_TEMP

uint8_t lookupTable[NUM_SEGMENTS][NUM_DIGITS] = {
//   0  1  2  3  4  5  6  7  8  9 <- digit to write
    {1, 0, 1, 1, 0, 1, 1, 1, 1, 1}, // A  0
    {1, 1, 1, 1, 1, 0, 0, 1, 1, 1}, // B  1
    {1, 1, 0, 1, 1, 1, 1, 1, 1, 1}, // C  2
    {1, 0, 1, 1, 0, 1, 1, 0, 1, 1}, // D  3
    {1, 0, 1, 0, 0, 0, 1, 0, 1, 0}, // E  4
    {1, 0, 0, 0, 1, 1, 1, 0, 1, 1}, // F  5
    {0, 0, 1, 1, 1, 1, 1, 0, 1, 1}, // G  6
};

ClockP_Handle lightClockHandle;
ClockP_Handle lcdClockHandle;

uint8_t lightCount = 0;                     // Counter for backlight PWM
uint8_t lcdState = 0;                       // Iterate through LCD action states
static sem_t sem;                           // Event semaphore
uint8_t contrast = 100;                     // Value of contrast based on system voltage
BatteryMonitor_NotifyObj rangeNotifyObject; // Battery monitor notify object
uint8_t tensValue = 0;                      // Value to display in tens place from user input
uint8_t onesValue = 0;                      // Value to display in ones place from user input

#ifdef USE_TEMP
void tempUpdateHandler (uintptr_t arg)
{
    uint16_t currentTemperature = Temperature_getTemperature();
    onesValue = currentTemperature % 10;
    tensValue = (currentTemperature / 10) % 10;
}
#endif

void processVoltage(uint16_t millivolts)
{
    if(millivolts <= 2400) contrast = 1;
    else if(millivolts <= 2800) contrast = 2;
    else if(millivolts <= 3200) contrast = 4;
    else if(millivolts <= 3600) contrast = 16;
}

void deltaNotificationFxn(uint16_t currentVoltage,
                          uint16_t thresholdVoltage,
                          uintptr_t clientArg,
                          BatteryMonitor_NotifyObj *notifyObject) 
{
    int_fast16_t voltageStatus;
    processVoltage(currentVoltage);
    voltageStatus = BatteryMonitor_registerNotifyRange(notifyObject,
                                        currentVoltage + THRESHOLD_DELTA_MILLIVOLT,
                                        currentVoltage - THRESHOLD_DELTA_MILLIVOLT,
                                        deltaNotificationFxn,
                                        (uintptr_t)NULL);
    if (voltageStatus != BatteryMonitor_STATUS_SUCCESS) {
        while(1);
    }
}

// Periodic timer callback to update backlight strength
void lightUpdateHandler (uintptr_t arg)
{
    if(lightCount == 4) 
    {
        lightCount = 0;
        GPIO_write(CONFIG_GPIO_LED_BACKLIGHT, 1);
    }
    else lightCount++;
    if(lightCount == LIGHT_STRENGTH) GPIO_write(CONFIG_GPIO_LED_BACKLIGHT, 0);

}

// Periodic timer for updating the LCD using this design as reference
/*
| Pin  | 1  | 2  | 3  | 4  | 5  | 6  | 7  | 8  | 9  | 10 |
|------|----|----|----|----|----|----|----|----|----|----|
| COM1 | 1F | 1A | 1B | 2F | 2A | 2B | S2 |COM1|  - |  - | 
| COM2 | 1E | 1G | 1C | 2E | 2G | 2C | S4 |  - |COM2|  - | 
| COM3 | S1 | 1D | S7 | S6 | 2D | S5 | S3 |  - |  - |COM3| 

 _ 1A _       _ 2A _       * S1
|      |     |      |      * S2
1F     1B   2F      2B     * S3
|_ 1G _|     |_ 2G _|      * S4
|      |     |      |      * S5
1E     1C   2E      2C     * S6
|_ 1D _|     |_ 2D _|      * S7
*/
void lcdUpdateHandler (uintptr_t arg)
{
    if(lcdState == 7) lcdState = 0;
    else lcdState++;
    switch(lcdState)
    {
        // Case 0/1 are for the contrast control
        case 0:
            GPIO_write(CONFIG_GPIO_PWM, 0);
            GPIO_write(CONFIG_GPIO_PWM_N, 1);

            GPIO_write(CONFIG_GPIO_COM1, 0);
            GPIO_write(CONFIG_GPIO_COM2, 0);
            GPIO_write(CONFIG_GPIO_COM3, 0);

            GPIO_write(CONFIG_GPIO_SEG1, 1);
            GPIO_write(CONFIG_GPIO_SEG2, 1);
            GPIO_write(CONFIG_GPIO_SEG3, 1);
            GPIO_write(CONFIG_GPIO_SEG4, 1);
            GPIO_write(CONFIG_GPIO_SEG5, 1);
            GPIO_write(CONFIG_GPIO_SEG6, 1);
            GPIO_write(CONFIG_GPIO_SEG7, 1);
            break;
        case 1:
            GPIO_write(CONFIG_GPIO_PWM, 1);
            GPIO_write(CONFIG_GPIO_PWM_N, 0);

            GPIO_write(CONFIG_GPIO_COM1, 1);
            GPIO_write(CONFIG_GPIO_COM2, 1);
            GPIO_write(CONFIG_GPIO_COM3, 1);

            GPIO_write(CONFIG_GPIO_SEG1, 0);
            GPIO_write(CONFIG_GPIO_SEG2, 0);
            GPIO_write(CONFIG_GPIO_SEG3, 0);
            GPIO_write(CONFIG_GPIO_SEG4, 0);
            GPIO_write(CONFIG_GPIO_SEG5, 0);
            GPIO_write(CONFIG_GPIO_SEG6, 0);
            GPIO_write(CONFIG_GPIO_SEG7, 0);
            break;
        // Case 2/3 are for COM1 control   
        case 2:
            GPIO_write(CONFIG_GPIO_PWM, 0);
            GPIO_write(CONFIG_GPIO_PWM_N, 1);

            GPIO_write(CONFIG_GPIO_COM1, 0);
            GPIO_write(CONFIG_GPIO_COM2, 1);
            GPIO_write(CONFIG_GPIO_COM3, 1);

            GPIO_write(CONFIG_GPIO_SEG1, lookupTable[5][tensValue]);    // 1F
            GPIO_write(CONFIG_GPIO_SEG2, lookupTable[0][tensValue]);    // 1A
            GPIO_write(CONFIG_GPIO_SEG3, lookupTable[1][tensValue]);    // 1B
            GPIO_write(CONFIG_GPIO_SEG4, lookupTable[5][onesValue]);    // 2F
            GPIO_write(CONFIG_GPIO_SEG5, lookupTable[0][onesValue]);    // 2A
            GPIO_write(CONFIG_GPIO_SEG6, lookupTable[1][onesValue]);    // 2B
            GPIO_write(CONFIG_GPIO_SEG7, 0);                            // S2
            break;
        case 3:
            GPIO_write(CONFIG_GPIO_PWM, 1);
            GPIO_write(CONFIG_GPIO_PWM_N, 0);

            GPIO_write(CONFIG_GPIO_COM1, 1);
            GPIO_write(CONFIG_GPIO_COM2, 0);
            GPIO_write(CONFIG_GPIO_COM3, 0);

            GPIO_write(CONFIG_GPIO_SEG1, 0);
            GPIO_write(CONFIG_GPIO_SEG2, 0);
            GPIO_write(CONFIG_GPIO_SEG3, 0);
            GPIO_write(CONFIG_GPIO_SEG4, 0);
            GPIO_write(CONFIG_GPIO_SEG5, 0);
            GPIO_write(CONFIG_GPIO_SEG6, 0);
            GPIO_write(CONFIG_GPIO_SEG7, 0);
            break;

        // Case 4/5 are for COM2 control
        case 4:
            GPIO_write(CONFIG_GPIO_PWM, 0);
            GPIO_write(CONFIG_GPIO_PWM_N, 1);

            GPIO_write(CONFIG_GPIO_COM1, 1);
            GPIO_write(CONFIG_GPIO_COM2, 0);
            GPIO_write(CONFIG_GPIO_COM3, 1);

            GPIO_write(CONFIG_GPIO_SEG1, lookupTable[4][tensValue]);    // 1E
            GPIO_write(CONFIG_GPIO_SEG2, lookupTable[6][tensValue]);    // 1G
            GPIO_write(CONFIG_GPIO_SEG3, lookupTable[2][tensValue]);    // 1C
            GPIO_write(CONFIG_GPIO_SEG4, lookupTable[4][onesValue]);    // 2E
            GPIO_write(CONFIG_GPIO_SEG5, lookupTable[6][onesValue]);    // 2G
            GPIO_write(CONFIG_GPIO_SEG6, lookupTable[2][onesValue]);    // 2C
            GPIO_write(CONFIG_GPIO_SEG7, 0);                            // S4
            break;
        case 5:
            GPIO_write(CONFIG_GPIO_PWM, 1);
            GPIO_write(CONFIG_GPIO_PWM_N, 0);

            GPIO_write(CONFIG_GPIO_COM1, 0);
            GPIO_write(CONFIG_GPIO_COM2, 1);
            GPIO_write(CONFIG_GPIO_COM3, 0);

            GPIO_write(CONFIG_GPIO_SEG1, 0);
            GPIO_write(CONFIG_GPIO_SEG2, 0);
            GPIO_write(CONFIG_GPIO_SEG3, 0);
            GPIO_write(CONFIG_GPIO_SEG4, 0);
            GPIO_write(CONFIG_GPIO_SEG5, 0);
            GPIO_write(CONFIG_GPIO_SEG6, 0);
            GPIO_write(CONFIG_GPIO_SEG7, 0);
            break;
        // Case 6/7 are for COM3 control  
        case 6:
            GPIO_write(CONFIG_GPIO_PWM, 0);
            GPIO_write(CONFIG_GPIO_PWM_N, 1);

            GPIO_write(CONFIG_GPIO_COM1, 1);
            GPIO_write(CONFIG_GPIO_COM2, 1);
            GPIO_write(CONFIG_GPIO_COM3, 0);

            GPIO_write(CONFIG_GPIO_SEG1, 0);                            // S1
            GPIO_write(CONFIG_GPIO_SEG2, lookupTable[3][tensValue]);    // 1D
            GPIO_write(CONFIG_GPIO_SEG3, 1);                            // S7
            GPIO_write(CONFIG_GPIO_SEG4, 0);                            // S6
            GPIO_write(CONFIG_GPIO_SEG5, lookupTable[3][onesValue]);    // 2D
            GPIO_write(CONFIG_GPIO_SEG6, 0);                            // S5
            GPIO_write(CONFIG_GPIO_SEG7, 0);                            // S3
            break;
        case 7:
            GPIO_write(CONFIG_GPIO_PWM, 1);
            GPIO_write(CONFIG_GPIO_PWM_N, 0);

            GPIO_write(CONFIG_GPIO_COM1, 0);
            GPIO_write(CONFIG_GPIO_COM2, 0);
            GPIO_write(CONFIG_GPIO_COM3, 1);

            GPIO_write(CONFIG_GPIO_SEG1, 0);
            GPIO_write(CONFIG_GPIO_SEG2, 0);
            GPIO_write(CONFIG_GPIO_SEG3, 0);
            GPIO_write(CONFIG_GPIO_SEG4, 0);
            GPIO_write(CONFIG_GPIO_SEG5, 0);
            GPIO_write(CONFIG_GPIO_SEG6, 0);
            GPIO_write(CONFIG_GPIO_SEG7, 0);
            break;
    }
    if(lcdState > 1) ClockP_setTimeout(lcdClockHandle, (1000000 / (REFRESH_RATE*6)) / ClockP_getSystemTickPeriod());
    else ClockP_setTimeout(lcdClockHandle, (1000000 / (REFRESH_RATE*6)) / contrast / ClockP_getSystemTickPeriod());
    ClockP_start(lcdClockHandle);
}

/*
 *  ======== gpioButtonFxn0 ========
 *  Callback function for the GPIO interrupt on CONFIG_GPIO_BUTTON_0.
 *
 *  Increments the tens digit
 *
 *  Note: GPIO interrupts are cleared prior to invoking callbacks.
 */
void gpioButtonFxn0(uint_least8_t index)
{
    if(tensValue == 9) {
        tensValue = 0;
    } else {
        tensValue++;
    }
    sem_post(&sem);
}

/*
 *  ======== gpioButtonFxn1 ========
 *  Callback function for the GPIO interrupt on CONFIG_GPIO_BUTTON_1.
 *  This may not be used for all boards.
 *
 *   Increments the ones digit
 *
 *  Note: GPIO interrupts are cleared prior to invoking callbacks.
 */
void gpioButtonFxn1(uint_least8_t index)
{
    if(onesValue == 9) {
        if (tensValue == 9) {
            tensValue = 0;
        } else {
            tensValue++;
        }
        onesValue = 0;
    } else {
        onesValue++;
    }
    sem_post(&sem);
}

/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    int32_t semStatus;
    semStatus = sem_init(&sem, 0, 0);
    if (semStatus != 0)
    {
        /* Error creating semaphore */
        while (1) {}
    }
    Temperature_init();

    /* Configure the GPIOs */
    GPIO_init();

    GPIO_setConfig(CONFIG_GPIO_BTN_LEFT, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);
    GPIO_setConfig(CONFIG_GPIO_BTN_RIGHT, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);
    /* Install Button callback */
    GPIO_setCallback(CONFIG_GPIO_BTN_LEFT, gpioButtonFxn0);
    GPIO_setCallback(CONFIG_GPIO_BTN_RIGHT, gpioButtonFxn1);
    /* Enable interrupts */
    GPIO_enableInt(CONFIG_GPIO_BTN_LEFT);
    GPIO_enableInt(CONFIG_GPIO_BTN_RIGHT);

    /* Configure the ClockP */
    ClockP_Params clockParams;
    ClockP_Params_init(&clockParams);
    clockParams.arg       = 0xAA;
    clockParams.startFlag = true;                   
    clockParams.period    = (1000000 / (LIGHT_FREQUENCY)) / ClockP_getSystemTickPeriod();
    lightClockHandle      = ClockP_create(lightUpdateHandler, 0, &clockParams);
#ifdef USE_TEMP
    clockParams.period    = TEMP_UPDATE / ClockP_getSystemTickPeriod();
    lightClockHandle      = ClockP_create(tempUpdateHandler, 0, &clockParams);   
#endif             
    clockParams.period    = 0;
    lcdClockHandle        = ClockP_create(lcdUpdateHandler, (1000000 / (REFRESH_RATE*6)) / ClockP_getSystemTickPeriod(), &clockParams);

    /* Configure the Battery Monitor */
    uint16_t batVoltage;                        // Initial battery voltage level 

    BatteryMonitor_init();
    batVoltage = BatteryMonitor_getVoltage();
    processVoltage(batVoltage);
    deltaNotificationFxn(batVoltage, 0,(uintptr_t)NULL, &rangeNotifyObject);

    // Reset all LCD GPIOs to a known state before starting
    GPIO_resetConfig(CONFIG_GPIO_LED_BACKLIGHT);
    GPIO_resetConfig(CONFIG_GPIO_PWM);
    GPIO_resetConfig(CONFIG_GPIO_PWM_N);
    GPIO_resetConfig(CONFIG_GPIO_COM1);
    GPIO_resetConfig(CONFIG_GPIO_COM2);
    GPIO_resetConfig(CONFIG_GPIO_COM3);
    GPIO_resetConfig(CONFIG_GPIO_SEG1);
    GPIO_resetConfig(CONFIG_GPIO_SEG2);
    GPIO_resetConfig(CONFIG_GPIO_SEG3);
    GPIO_resetConfig(CONFIG_GPIO_SEG4);
    GPIO_resetConfig(CONFIG_GPIO_SEG5);
    GPIO_resetConfig(CONFIG_GPIO_SEG6);
    GPIO_resetConfig(CONFIG_GPIO_SEG7);

    /* Loop forever */
    while (1)
    {
        sem_wait(&sem);
    }
}
