/*
 * Copyright (c) 2015-2019, Texas Instruments Incorporated
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
 *  ======== empty.c ========
 */

/* POSIX Header files */
#include <pthread.h>

/* RTOS header files */
#include <FreeRTOS.h>
#include <task.h>

/* Stack size in bytes */
#define THREADSTACKSIZE 1024

/* For usleep() */
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <semaphore.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>

/* Driver configuration */
#include "ti_drivers_config.h"

#include "UART_BITBANG_CC23XX.h"
#include <app_main.h>


uint8_t mainThreadBuffer[0xFF];
sem_t   sem1;
sem_t   sem2;
uint8_t Rxsize;

SPI_Handle    spi;
SPI_Params    spiParams;
sem_t         sem;
uint8_t       *spiBuffer;
uint8_t       spiSize;

bool                transferOK;
SPI_Transaction     spiTransaction;
uint16_t            *transmitBuffer;

uint8_t readComplete = 0;

extern GPIO_Config GPIO_config;

// READ FUNCTION
void swUARTRead(uint8_t size){
    readComplete = 0;
    Rxsize = size;

    // UART_BitBang_Handle ubbh = UART_BitBang_open(UART_BITBANG_CONFIG_DEFAULT, 0);
    // if (!ubbh) while (1); // error
    // //uint32_t key1 = HwiP_disable();
    // UART_BitBang_read(ubbh, mainThreadBuffer, Rxsize, 0);
    // // HwiP_restore(key1);
    // UART_BitBang_close(ubbh);
    bitbang_read(size);
}

// INVERTER FOR UART OVER SPI
uint16_t inverse(uint16_t result){
  result = (result >> 1)  & 0x5555 | (result << 1)  & 0xAAAA;
  result = (result >> 2)  & 0x3333 | (result << 2)  & 0xCCCC;
  result = (result >> 4)  & 0x0F0F | (result << 4)  & 0xF0F0;
  result = (result >> 8)  | (result << 8);

  result = result >> 7;
  result |= 1;
  return result;
}

// WRITE FUNCTION
void swUARTWrite(uint8_t *buffer, uint8_t size){
    spiBuffer = buffer;
    spiSize   = size;
    transmitBuffer = (uint16_t *)malloc(2*(spiSize+1));
    
    for(int i = 0; i < spiSize; i++){
        transmitBuffer[i] = inverse(spiBuffer[i]);
    }
    transmitBuffer[spiSize] = 0;
    spi = SPI_open(CONFIG_SPI_0, &spiParams);
    if(spi == NULL){
        // Error opening SPI
        while(1);
    }

    spiTransaction.count = spiSize + 1;
    spiTransaction.txBuf = (void *)transmitBuffer;
    spiTransaction.rxBuf = NULL;

    transferOK = SPI_transfer(spi, &spiTransaction);
    if(!transferOK){
        while(1);
    }
    free(transmitBuffer);
    SPI_close(spi);
}

/* TEST ACCURACY */
uint32_t correctBytes = 0;
uint32_t incorrectBytes = 0;

void accuracyTest(uint8_t buffer[255])
{
    for(uint16_t i = 0; i<100; i++){
        if(buffer[i] == 0xAA){
            correctBytes++;
        }
        else{
            incorrectBytes++;
            // GPIO_toggle(CONFIG_GPIO_LED_RED);
        }
    }
}

/*
 *  ======== mainThread ========
 */
void *mainThread2(void *arg0)
{
    //Get_Stack_Info();
    
    /* Call driver init functions */
    GPIO_init();

    // GPIO_config.intPriority = ~0;
    /* Create semaphore */
    int32_t semStatus;

    semStatus = sem_init(&sem1, 0, 0);
    if (semStatus != 0)
    {
        /* Error creating semaphore */
        while (1) {}
    }

    semStatus = sem_init(&sem2, 0 , 0);
    if (semStatus != 0)
    {
        /* Error creating semaphore */
        while (1) {}
    }


    /* Create semaphore */
    // int32_t semStatus;
    // semStatus = sem_init(&sem, 0, 0);

    // if (semStatus != 0)
    // {
    //     /* Error creating semaphore */
    //     while (1) {}
    // }

    /* Call driver init functions */
    SPI_init();
    SPI_Params_init(&spiParams);

    spiParams.transferMode = SPI_MODE_BLOCKING;
    //spiParams.transferCallbackFxn = spiCallback;
    spiParams.mode = SPI_CONTROLLER;
    // spiParams.bitRate = 115200;
    spiParams.bitRate = 118000; // Use a slightly higher baud rate to avoid any bitbang misinterpretation, or use differrent frame format
    spiParams.dataSize = 10;
    spiParams.frameFormat = SPI_POL0_PHA1;

    // bitbang_open();

    while(1)
    {
        // TODO: API for Setting Baud Rate 
        swUARTRead(1); // Has to be read to mainThreadBuffer
        swUARTWrite(mainThreadBuffer, 1);
    }

}

void emptyMain2(void)
{
    pthread_t thread;
    pthread_attr_t attrs;
    struct sched_param priParam;
    int retc;

    /* Initialize the attributes structure with default values */
    pthread_attr_init(&attrs);

    /* Set priority, detach state, and stack size attributes */
    priParam.sched_priority = 2; // Lower the priority of this task
    retc = pthread_attr_setschedparam(&attrs, &priParam);
    retc |= pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
    retc |= pthread_attr_setstacksize(&attrs, THREADSTACKSIZE);
    if (retc != 0)
    {
        /* failed to set attributes */
        while (1) {}
    }

    retc = pthread_create(&thread, &attrs, mainThread2, NULL);
    if (retc != 0)
    {
        /* pthread_create() failed */
        while (1) {}
    }
}