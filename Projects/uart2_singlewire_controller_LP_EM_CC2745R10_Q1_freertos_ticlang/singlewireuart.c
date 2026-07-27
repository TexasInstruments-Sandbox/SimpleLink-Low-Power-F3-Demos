/*
 * Copyright (c) 2020, Texas Instruments Incorporated
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
 *  ======== uart2callback.c ========
 */
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

/* POSIX Header files */
#include <semaphore.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/display/Display.h>
#include <ti/drivers/UART2.h>
#include <ti/drivers/uart2/UART2LPF3.h>
#include <ti/drivers/apps/LED.h>
#include <ti/drivers/apps/Button.h>
#include <ti/drivers/dpl/ClockP.h>
#include DeviceFamily_constructPath(driverlib/cpu.h)

/* Driver configuration */
#include "ti_drivers_config.h"
#include "singlewireuart.h"

#define CA_DELAY    250 // us
#define BUFFER      1024 // integer
#define NUM_RETRIES 5   // integer
#define ID_LEN      4   // integer

static sem_t sem;
static uint8_t semEvent;
static int32_t semStatus;
static uint8_t controllerState;
static uint32_t status = UART2_STATUS_SUCCESS;
static volatile size_t numBytesRead;
static bool buttonID;
static bool uartExit;
uint16_t targetAck;
uint32_t passTimes;
static volatile size_t numBytesRead;
int cdRetries = 0;
UART2LPF3_HWAttrs const *hwAttrs;

/************* Single-wire Command Definition ******************/

typedef struct _CONTROLLER_INSTANCE
{
    uint16_t ID;//the current assigned ID
    uint32_t txCommand[COMMAND_SIZE+1];
    uint32_t rxCommand[COMMAND_SIZE+1];
    uint32_t IDRequestSequence;
    BROADCAST_DATA broadcastData;
    UNICASTING_DATA unicastingData;
    UNICASTING_ACK_DATA unicastingAckData;
    ACTIVE_SEND_DATA activeSendData;
    uint32_t tagTraceID[MAXIMUM_ID+1];//index start from 1
    uint8_t waitForBreakFlag;
} CONTROLLER_INSTANCE;

CONTROLLER_INSTANCE controllerInst = {
    .ID = 0,
    .broadcastData = {
        .begin = 's',
        .bdcCmd = 3, //1->set PA0, 2->clear PA0, 3->toggle PA0
        .checksum = 3,
        .end = 'e'
    },
    .unicastingData = {
        .begin = 's',
        .cmdType = 1,//1:led toggling, 2:ADC result fetching
        .ledColor = 8,
        .adcChannel = 0,
        .checksum = 9,
        .end = 'e'
    }
};

Button_Handle buttonHandle[2];
LED_Handle ledHandle[2];
Display_Handle display;
UART2_Params uartParams;
UART2_Handle uart;
ClockP_Handle timeoutClkHandle;  
ClockP_Handle caTimeoutClkHandle;    

/*
 *  ======== rxCallbackFxn ========
 */
void rxCallbackFxn(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status)
{
    if (status == UART2_STATUS_SUCCESS)
    {
        numBytesRead = count;
        semEvent = UART_RECEIVED;
        if(ClockP_isActive(timeoutClkHandle)) ClockP_stop(timeoutClkHandle);
        sem_post(&sem);
    }
}

void uartEventCallbackFxn(UART2_Handle handle, uint32_t event, uint32_t data, void *userArg)
{
    if(event == UART2_EVENT_TX_FINISHED)
    {
        semEvent = UART_TRANSMITTED;
        //sem_post(&sem);
    }
}

/*
 *  ======== handleButtonCallback ========
 */
void handleButtonCallback(Button_Handle handle, Button_EventMask events)
{
    if (Button_EV_CLICKED == (events & Button_EV_CLICKED))
    {
        buttonID = (buttonHandle[CONFIG_BUTTON_0] == handle) ? 0 : 1;
        if(controllerState > MODE_ASSIGN_ID) 
        {
            semEvent = LPBUTTON_PRESSED;
            sem_post(&sem);
        }
    }
}

/*
 * ClockP callback fxn for timeout timer
 */
void timeoutHandler (uintptr_t arg)
{
    UART2_readCancel(uart);
    semEvent = CLOCK_TIMEOUT;
    sem_post(&sem);
}

void caTimeoutHandler (uintptr_t arg)
{
    sem_post(&sem);
}

void CollisionAvoidanceCallback(uint_least8_t index)
{
    sem_post(&sem);
}

int collisionAvoidance(void)
{
    bool status = 0x00;
    GPIO_resetConfig(hwAttrs->rxPin);
    GPIO_setConfig(hwAttrs->rxPin, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);
    GPIO_setCallback(hwAttrs->rxPin, CollisionAvoidanceCallback);
    GPIO_clearInt(hwAttrs->rxPin);
    GPIO_enableInt(hwAttrs->rxPin);

    if(GPIO_read(hwAttrs->rxPin))
    {
        ClockP_start(caTimeoutClkHandle);
        sem_wait(&sem);
        if(ClockP_isActive(caTimeoutClkHandle) || !GPIO_read(hwAttrs->rxPin)) 
        {
            status = 0x01;
            ClockP_stop(caTimeoutClkHandle);
        }
        else status = 0x00;
    }
    else status = 0x01;

    GPIO_disableInt(hwAttrs->rxPin);
    GPIO_setConfigAndMux(hwAttrs->rxPin, GPIO_CFG_INPUT, hwAttrs->rxPinMux);
    return status;
}

void peripheralInterruptCallback(uint_least8_t index)
{
    semEvent = PERIPHERAL_ACTIVITY;
    sem_post(&sem);
}

void peripheralWakeup(void)
{
    GPIO_resetConfig(hwAttrs->txPin);
    GPIO_write(hwAttrs->txPin, 0);
    CPUDelay(1500);
    GPIO_write(hwAttrs->txPin, 1);
    GPIO_setConfigAndMux(hwAttrs->txPin, GPIO_CFG_NO_DIR, hwAttrs->txPinMux);
}

uint8_t waitForWakeup (void)
{
    semEvent = 0;
    UART2_close(uart);
    GPIO_setConfig(hwAttrs->rxPin, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);
    GPIO_setCallback(hwAttrs->rxPin, peripheralInterruptCallback);
    GPIO_clearInt(hwAttrs->rxPin);
    GPIO_enableInt(hwAttrs->rxPin);

    while((semEvent != PERIPHERAL_ACTIVITY) && (semEvent != LPBUTTON_PRESSED))
    {
        sem_wait(&sem);
    }

    GPIO_disableInt(hwAttrs->rxPin);
    uart = UART2_open(CONFIG_UART2_1, &uartParams);
    return semEvent;
}

/**
 * Calculates the CRC-16/AUG-CCITT checksum for a given data buffer.
 *
 * Parameters:
 *   data: Pointer to the input data array.
 *   len: Length of the input data in bytes.
 *
 * Returns:
 *   The calculated 16-bit CRC checksum.
 */
uint32_t crc16_aug_ccitt(const uint8_t *data, size_t len) {
    // Parameters for CRC-16/AUG-CCITT:
    // Polynomial: 0x1021
    // Initial value (seed): 0x0F1D (from user input 0xf1d)
    // Input data not reflected, output CRC not reflected, no final XOR.
    uint16_t crc = 0x1D0F; 
    uint16_t polynomial = 0x1021;
    for (size_t i = 0; i < len; i+=2) {
        crc ^= (uint16_t)(data[i+1] << 8); // XOR current byte with the high byte of CRC

        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
        crc ^= (uint16_t)(data[i] << 8); // XOR current byte with the high byte of CRC

        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc; // No final XOR operation
}

uint8_t getSumUint8(uint8_t *buffer, uint16_t size)
{
    uint8_t sum = 0;
    for (int i=0;i<size;i++){
        sum += buffer[i];
    }
    return sum;
}

bool parseRxDataPacket(uint8_t *rxDataBuffer, uint16_t *payLoadLength)
{
    *payLoadLength = (BUILD_UINT16(rxDataBuffer[PAYLOAD_LEN_IDX+1], rxDataBuffer[PAYLOAD_LEN_IDX]));

    if (rxDataBuffer[SOF_IDX] != 's' || rxDataBuffer[EOF_IDX(*payLoadLength)] != 'e'){
        return false;
    }

   uint16_t calCheckSum = crc16_aug_ccitt((uint8_t*)&rxDataBuffer[PAYLOAD_LEN_IDX], *payLoadLength+2);
   uint16_t targetCheckSum = BUILD_UINT16(rxDataBuffer[CS_IDX(*payLoadLength)], rxDataBuffer[CS_IDX(*payLoadLength)+1]);
    if (calCheckSum == targetCheckSum){
        return true;
    }else{
        return false;
    }
}

void composeTxDataPacket(uint8_t *txDataBuffer, uint8_t *payLoadBuffer, uint16_t payLoadLength)
{
    txDataBuffer[SOF_IDX] = 's';
    txDataBuffer[PAYLOAD_LEN_IDX+1] = payLoadLength & 0x00FF;
    txDataBuffer[PAYLOAD_LEN_IDX] = (payLoadLength >> 8) & 0x00FF;
    for (int i=0; i<payLoadLength; i++){
        txDataBuffer[i+PAYLOAD_IDX] = payLoadBuffer[i];
    }
    txDataBuffer[EOF_IDX(payLoadLength)] = 'e';
    uint16_t calCheckSum = crc16_aug_ccitt((uint8_t*)&txDataBuffer[PAYLOAD_LEN_IDX], payLoadLength+2);
    txDataBuffer[CS_IDX(payLoadLength)] = calCheckSum & 0x00FF;
    txDataBuffer[CS_IDX(payLoadLength)+1] = (calCheckSum>>8) & 0x00FF;
}

bool parseRxCommandPacket(uint8_t* rxCommand, COMMAND_PACKET *cmdPacket)
{

    cmdPacket->cmdCode = rxCommand[0];
    cmdPacket->reserved = (rxCommand[1] & 0xF0) >> 4;
    cmdPacket->cmdSeq = rxCommand[1] & 0x0F;
    cmdPacket->TagID = BUILD_UINT32(rxCommand[2], rxCommand[3], rxCommand[4], rxCommand[5]);
    cmdPacket->dataLength = BUILD_UINT16(rxCommand[7], rxCommand[6]);
    cmdPacket->checksum = rxCommand[8];

    return (getSumUint8(rxCommand, COMMAND_SIZE - 1) == cmdPacket->checksum);
}

void composeTxCommandPacket(uint8_t *txCommand, COMMAND_PACKET *cmdPacket)
{
    //command code
    txCommand[0] = (uint8_t) cmdPacket->cmdCode;
    //command sequence + command length
    txCommand[1] = (uint8_t) (cmdPacket->cmdSeq + (cmdPacket->reserved << 4));
    // Tag ID
    txCommand[2] = GET_BYTE_0(cmdPacket->TagID);
    txCommand[3] = GET_BYTE_1(cmdPacket->TagID);
    txCommand[4] = GET_BYTE_2(cmdPacket->TagID);
    txCommand[5] = GET_BYTE_3(cmdPacket->TagID);
    //data length
    txCommand[6] = GET_BYTE_1(cmdPacket->dataLength);
    txCommand[7] = GET_BYTE_0(cmdPacket->dataLength);
    //command checksum
    cmdPacket->checksum = getSumUint8(txCommand, COMMAND_SIZE - 1);
    txCommand[8] = cmdPacket->checksum;

}

void assign_id(void)
{
    uint16_t idIdx = 1;
    while (idIdx <= MAXIMUM_ID)
    {
        //waitForWakeup();

        COMMAND_PACKET rxCmdPacket;

        UART2_flushRx(uart);
        numBytesRead = 0;
        while(numBytesRead != COMMAND_SIZE)
        {
           status = UART2_read(uart, rxBuffer, COMMAND_SIZE, NULL);
           if (status != UART2_STATUS_SUCCESS) while (1) {}
           sem_wait(&sem); /* Do not write until read callback executes */
        }

        bool rxCmdFlag = parseRxCommandPacket((uint8_t*)&rxBuffer[0], &rxCmdPacket);

        if (rxCmdFlag && rxCmdPacket.cmdCode == CMD_PLUGIN_NOTIFY)
        {
            controllerInst.tagTraceID[idIdx] = rxCmdPacket.TagID;

            COMMAND_PACKET txCmdPacket;
            txCmdPacket.cmdCode = (uint8_t) CMD_ACK;
            txCmdPacket.TagID = controllerInst.tagTraceID[idIdx];
            txCmdPacket.reserved = 0;
            txCmdPacket.cmdSeq = 0;
            txCmdPacket.dataLength = 0;//the ACK message has no data segment
            composeTxCommandPacket((uint8_t*)controllerInst.txCommand, &txCmdPacket);

            cdRetries = 0;
            while (cdRetries < NUM_RETRIES)
            {
                UART2_flushRx(uart);
                status = UART2_read(uart, rxBuffer, COMMAND_SIZE, NULL);
                status = UART2_write(uart, &controllerInst.txCommand, COMMAND_SIZE, NULL);
                if (status != UART2_STATUS_SUCCESS) while (1) {}
                sem_wait(&sem); /* Do not read until write callback executes */

                if (numBytesRead != COMMAND_SIZE || memcmp(rxBuffer, &controllerInst.txCommand, COMMAND_SIZE))
                {
                    cdRetries++;
                }
                else break;
            }

            if (cdRetries >= NUM_RETRIES)
            {
                Display_print0(display, 0, 0, "CD failed!");
                return;
            }
            else 
            {
                Display_print2(display,
                        0,
                        0,
                        "The ID 0x%x has been assigned to Index %d!",
                        controllerInst.tagTraceID[idIdx],
                        idIdx);
                idIdx++;
            
            }
        }
        else 
        {
            Display_print1(display,
                    0,
                    0,
                    "The ID %d assigned process failed, please reset current peripheral board and assign again",
                    idIdx);
        
        }
    }
}

void broadcast_test(void)
{
    Display_print0(display,
        0,
        0,
        "Clear the LED1 in all peripheral board? Press S1->Yes, Press S2->No: ");

    sem_wait(&sem);

    if(!buttonID)
    {
        Display_print0(display, 0, 0, "Yes");

        if (collisionAvoidance())
        {
            Display_print0(display, 0, 0, "CA failed!");
            return;
        }

        broadcastSize = 4;
        txPayLoad[0] = 0;//LED1
        txPayLoad[1] = 0;//LED2-RED
        txPayLoad[2] = 0;//LED2-GREEN
        txPayLoad[3] = 0;//LED2-BLUE

        COMMAND_PACKET txCmdPacket;
        txCmdPacket.cmdCode = (uint8_t) CMD_BROADCAST;
        txCmdPacket.TagID = 0xFFFFFFFF;
        txCmdPacket.reserved = 0;
        txCmdPacket.cmdSeq = 0;
        txCmdPacket.dataLength = GET_DATA_FRAME_SIZE(broadcastSize);//the ACK message has no data segment
        composeTxCommandPacket((uint8_t*)controllerInst.txCommand, &txCmdPacket);

        composeTxDataPacket(txBuffer, txPayLoad, broadcastSize);

        peripheralWakeup();

        cdRetries = 0;
        while (cdRetries < NUM_RETRIES)
        {
            UART2_flushRx(uart);
            status = UART2_read(uart, rxBuffer, COMMAND_SIZE, NULL);
            status = UART2_write(uart, &controllerInst.txCommand, COMMAND_SIZE, NULL);
            if (status != UART2_STATUS_SUCCESS) while (1) {}
            sem_wait(&sem); /* Do not read until write callback executes */

            if (numBytesRead != COMMAND_SIZE || memcmp(rxBuffer, &controllerInst.txCommand, COMMAND_SIZE))
            {
                cdRetries++;
                continue;
            }

            status = UART2_read(uart, rxBuffer, txCmdPacket.dataLength, NULL);
            status = UART2_write(uart, txBuffer, txCmdPacket.dataLength, NULL);
            if (status != UART2_STATUS_SUCCESS) while (1) {}
            sem_wait(&sem); /* Do not read until write callback executes */
            if (numBytesRead != (txCmdPacket.dataLength) || memcmp(rxBuffer, txBuffer, txCmdPacket.dataLength))
            {
                cdRetries++;
            }
            else break;
        }
        if (cdRetries >= NUM_RETRIES)
        {
            Display_print0(display, 0, 0, "CD failed!");
        }
    }
    else Display_print0(display, 0, 0, "No");

    Display_print0(display,
        0,
        0,
        "Set the LED1 in all peripheral board? Press S1->Yes, Press S2->No: ");

    sem_wait(&sem);

    if(!buttonID)
    {
        Display_print0(display, 0, 0, "Yes");

        if (collisionAvoidance())
        {
            Display_print0(display, 0, 0, "CA failed!");
            return;
        }

        broadcastSize = 4;
        txPayLoad[0] = 1;//LED1
        txPayLoad[1] = 0;//LED2-RED
        txPayLoad[2] = 0;//LED2-GREEN
        txPayLoad[3] = 0;//LED2-BLUE

        COMMAND_PACKET txCmdPacket;
        txCmdPacket.cmdCode = (uint8_t) CMD_BROADCAST;
        txCmdPacket.TagID = 0xFFFFFFFF;
        txCmdPacket.reserved = 0;
        txCmdPacket.cmdSeq = 0;
        txCmdPacket.dataLength = GET_DATA_FRAME_SIZE(broadcastSize);//the ACK message has no data segment
        composeTxCommandPacket((uint8_t*)controllerInst.txCommand, &txCmdPacket);

        composeTxDataPacket(txBuffer, txPayLoad, broadcastSize);

        peripheralWakeup();
        
        cdRetries = 0;
        while (cdRetries < NUM_RETRIES)
        {
            UART2_flushRx(uart);
            status = UART2_read(uart, rxBuffer, COMMAND_SIZE, NULL);
            status = UART2_write(uart, &controllerInst.txCommand, COMMAND_SIZE, NULL);
            if (status != UART2_STATUS_SUCCESS) while (1) {}
            sem_wait(&sem); /* Do not read until write callback executes */

            if (numBytesRead != COMMAND_SIZE || memcmp(rxBuffer, &controllerInst.txCommand, COMMAND_SIZE))
            {
                cdRetries++;
                continue;
            }

            status = UART2_read(uart, rxBuffer, txCmdPacket.dataLength, NULL);
            status = UART2_write(uart, txBuffer, txCmdPacket.dataLength, NULL);
            if (status != UART2_STATUS_SUCCESS) while (1) {}
            sem_wait(&sem); /* Do not read until write callback executes */
            if (numBytesRead != (txCmdPacket.dataLength) || memcmp(rxBuffer, txBuffer, txCmdPacket.dataLength))
            {
                cdRetries++;
            }
            else break;
        }
        if (cdRetries >= NUM_RETRIES)
        {
            Display_print0(display, 0, 0, "CD failed!");
        }
    }
    else Display_print0(display, 0, 0, "No");
}

void repeat_unicasting_test(uint8_t testID, uint32_t idx)
{

    COMMAND_PACKET txCmdPacket;
    txCmdPacket.cmdCode = (uint8_t) CMD_UNICASTING;
    txCmdPacket.TagID = controllerInst.tagTraceID[testID];
    txCmdPacket.reserved = 0;
    txCmdPacket.cmdSeq = 0;
    txCmdPacket.dataLength = GET_DATA_FRAME_SIZE(TX_PAYLOAD_SIZE);//the ACK message has no data segment
    composeTxCommandPacket((uint8_t*)controllerInst.txCommand, &txCmdPacket);

    txPayLoad[0] = idx;
    composeTxDataPacket(txBuffer, txPayLoad, TX_PAYLOAD_SIZE);

    if (collisionAvoidance())
    {
        Display_print0(display, 0, 0, "CA failed!");
        return;
    }

    peripheralWakeup();

    cdRetries = 0;
    while (cdRetries < NUM_RETRIES)
    {
        UART2_flushRx(uart);
        status = UART2_read(uart, rxBuffer, COMMAND_SIZE, NULL);
        status = UART2_write(uart, &controllerInst.txCommand, COMMAND_SIZE, NULL);
        if (status != UART2_STATUS_SUCCESS) while (1) {}
        sem_wait(&sem); /* Do not read until write callback executes */

        if (numBytesRead != COMMAND_SIZE || memcmp(rxBuffer, &controllerInst.txCommand, COMMAND_SIZE))
        {
            cdRetries++;
            continue;
        }

        status = UART2_read(uart, rxBuffer, txCmdPacket.dataLength, NULL);
        status = UART2_write(uart, txBuffer, txCmdPacket.dataLength, NULL);
        if (status != UART2_STATUS_SUCCESS) while (1) {}
        sem_wait(&sem); /* Do not read until write callback executes */
        if (numBytesRead != txCmdPacket.dataLength || memcmp(rxBuffer, txBuffer, txCmdPacket.dataLength))
        {
            cdRetries++;
        }
        else break;
    }

    if (cdRetries >= NUM_RETRIES)
    {
        Display_print0(display, 0, 0, "CD failed!");
        return;
    }

    UART2_flushRx(uart);
    status = UART2_read(uart, rxBuffer, COMMAND_SIZE, NULL);
    if (status != UART2_STATUS_SUCCESS) while (1) {}
    ClockP_start(timeoutClkHandle);
    sem_wait(&sem); /* Do not write until read callback executes */

    if(semEvent == CLOCK_TIMEOUT)
    {
        Display_print0(display,
            0,
            0,
            "Receive ACK-command Time Out!!!");
    }
    else
    {
        COMMAND_PACKET rxCmdPacket;
        bool rxACKFlag = parseRxCommandPacket((uint8_t*)&rxBuffer[0], &rxCmdPacket);
        if (rxACKFlag && rxCmdPacket.cmdCode == CMD_ACK && rxCmdPacket.TagID == txCmdPacket.TagID)
        {
            Display_print0(display, 0, 0, "Command ACK successfully !");
            passTimes++;
        }
        else Display_print0(display, 0, 0, "Command ACK error (data checksum failed)!");
    }

}

void unicast_test(uint8_t testID)
{
    bool checkResult;

    targetAck = (testID << 8) | (uint8_t) CMD_ACK;

    Display_print0(display,
        0,
        0,
        "Set the LED2 color in the peripheral board, Press S1->RED, Press S2->GREEN: ");

    sem_wait(&sem);

    if(!buttonID)
    {
        Display_print0(display, 0, 0, "[RED]");
        unicastSize = 4;
        txPayLoad[0] = 0;//LED1
        txPayLoad[1] = 1;//LED2-RED
        txPayLoad[2] = 0;//LED2-GREEN
        txPayLoad[3] = 0;//LED2-BLUE
    }
    else
    {
        Display_print0(display, 0, 0, "[GREEN]");
        unicastSize = 4;
        txPayLoad[0] = 0;//LED1
        txPayLoad[1] = 0;//LED2-RED
        txPayLoad[2] = 1;//LED2-GREEN
        txPayLoad[3] = 0;//LED2-BLUE
    }

    COMMAND_PACKET txCmdPacket;
    txCmdPacket.cmdCode = (uint8_t) CMD_UNICASTING;
    txCmdPacket.TagID = controllerInst.tagTraceID[testID];
    txCmdPacket.reserved = 0;
    txCmdPacket.cmdSeq = 0;
    txCmdPacket.dataLength = GET_DATA_FRAME_SIZE(unicastSize);//the ACK message has no data segment
    composeTxCommandPacket((uint8_t*)controllerInst.txCommand, &txCmdPacket);

    composeTxDataPacket(txBuffer, txPayLoad, unicastSize);

    if (collisionAvoidance())
    {
        Display_print0(display, 0, 0, "CA failed!");
        return;
    }

    peripheralWakeup();

    cdRetries = 0;
    while (cdRetries < NUM_RETRIES)
    {
        UART2_flushRx(uart);
        status = UART2_read(uart, rxBuffer, COMMAND_SIZE, NULL);
        status = UART2_write(uart, &controllerInst.txCommand, COMMAND_SIZE, NULL);
        if (status != UART2_STATUS_SUCCESS) while (1) {}
        sem_wait(&sem); /* Do not read until write callback executes */

        if (numBytesRead != COMMAND_SIZE || memcmp(rxBuffer, &controllerInst.txCommand, COMMAND_SIZE))
        {
            cdRetries++;
            continue;
        }

        status = UART2_read(uart, rxBuffer, txCmdPacket.dataLength, NULL);
        status = UART2_write(uart, txBuffer, txCmdPacket.dataLength, NULL);
        if (status != UART2_STATUS_SUCCESS) while (1) {}
        sem_wait(&sem); /* Do not read until write callback executes */
        if (numBytesRead != txCmdPacket.dataLength || memcmp(rxBuffer, txBuffer, txCmdPacket.dataLength))
        {
            cdRetries++;
        }
        else break;
    }

    if (cdRetries >= NUM_RETRIES)
    {
        Display_print0(display, 0, 0, "CD failed!");
    }

    UART2_flushRx(uart);
    status = UART2_read(uart, rxBuffer, COMMAND_SIZE, NULL);
    if (status != UART2_STATUS_SUCCESS) while (1) {}
    ClockP_start(timeoutClkHandle);
    sem_wait(&sem); /* Do not write until read callback executes */

    if(semEvent == CLOCK_TIMEOUT)
    {
        Display_print0(display,
            0,
            0,
            "Receive ACK-command Time Out!!!");
    }
    else
    {
        COMMAND_PACKET rxCmdPacket;
        bool rxACKFlag = parseRxCommandPacket((uint8_t*)&rxBuffer[0], &rxCmdPacket);
        if (rxACKFlag && rxCmdPacket.cmdCode == CMD_ACK && rxCmdPacket.TagID == txCmdPacket.TagID)
            Display_print0(display, 0, 0, "Command ACK successfully !");
        else Display_print0(display, 0, 0, "Command ACK error (data checksum failed)!");
    }
}

void active_send_test(void)
{
    Display_print0(display,
        0,
        0,
        "Keep waiting active send from peripheral......");

    while(1)
    {
        //waitForWakeup();
        semEvent = 0;

        COMMAND_PACKET rxCmdPacket;

        UART2_flushRx(uart);
        numBytesRead = 0;
        while(numBytesRead != COMMAND_SIZE && semEvent != LPBUTTON_PRESSED)
        {
           status = UART2_read(uart, rxBuffer, COMMAND_SIZE, NULL);
           if (status != UART2_STATUS_SUCCESS) while (1) {}
           sem_wait(&sem); /* Do not write until read callback executes */
        }

        bool rxCmdFlag = parseRxCommandPacket((uint8_t*)&rxBuffer[0], &rxCmdPacket);

        if(semEvent == LPBUTTON_PRESSED)
        {   
            UART2_readCancel(uart);
            Display_print0(display,
                0,
                0,
                "Active send command test halt");
            break;
        }
        else 
        {

            if (rxCmdFlag && rxCmdPacket.cmdCode == CMD_ACTIVE_SEND) //active send command
            {
                status = UART2_read(uart, &rxBuffer, rxCmdPacket.dataLength, NULL);
                if (status != UART2_STATUS_SUCCESS) while (1) {}
                ClockP_start(timeoutClkHandle);
                sem_wait(&sem); /* Do not write until read callback executes */

                uint8_t* rxPayLoadBuffer = PAYLOAD_BUFFER(rxBuffer);
                uint16_t rxPayLoadLength;
                bool checkResult = parseRxDataPacket(rxBuffer, &rxPayLoadLength);

                if(semEvent == CLOCK_TIMEOUT)
                {
                    Display_print0(display,
                        0,
                        0,
                        "Active Send Command received but data!!!");
                }
                else if(checkResult)
                {
                    COMMAND_PACKET txCmdPacket;
                    txCmdPacket.cmdCode = (uint8_t) CMD_ACK;
                    txCmdPacket.TagID = rxCmdPacket.TagID;
                    txCmdPacket.reserved = 0;
                    txCmdPacket.cmdSeq = 0;
                    txCmdPacket.dataLength = 0;//the ACK message has no data segment
                    composeTxCommandPacket((uint8_t*)controllerInst.txCommand, &txCmdPacket);

                    cdRetries = 0;
                    while (cdRetries < NUM_RETRIES)
                    {
                        UART2_flushRx(uart);
                        status = UART2_read(uart, rxBuffer, COMMAND_SIZE, NULL);
                        status = UART2_write(uart, &controllerInst.txCommand, COMMAND_SIZE, NULL);
                        if (status != UART2_STATUS_SUCCESS) while (1) {}
                        sem_wait(&sem); /* Do not read until write callback executes */

                        if (numBytesRead != COMMAND_SIZE || memcmp(rxBuffer, &controllerInst.txCommand, COMMAND_SIZE))
                        {
                            cdRetries++;
                        }
                        else break;
                    }

                    if (cdRetries >= NUM_RETRIES)
                    {
                        Display_print0(display, 0, 0, "CD failed!");
                        return;
                    }

                    else
                    {
                        Display_print1(display, 0, 0, "The ADC result of peripheral ID-0x%x:", rxCmdPacket.TagID);
                        Display_print1(display, 0, 0, "Channel-0: %0.2fV", 
                            (float) (BUILD_UINT32(rxPayLoadBuffer[0], rxPayLoadBuffer[1], rxPayLoadBuffer[2], rxPayLoadBuffer[3])));
                    }
                }
                else Display_print1(display, 0, 0, "Data CRC verification failed ID-0x%x", rxCmdPacket.TagID);
            }
            else Display_print1(display, 0, 0, "Invalid Command: Not Peripheral Active Send Command 0x%x", rxCmdPacket.cmdCode);
        }
    }
}

/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    Button_Params buttonParams;
    LED_Params ledParams;

    /* Call driver init functions */
    GPIO_init();
    Display_init();
    Button_init();
    LED_init();

    /* Open the UART display for output */
    display = Display_open(Display_Type_UART, NULL);
    if (display == NULL)
    {
        while (1) {}
    }

    Display_print0(display,
        0,
        0,
        "\r\nProgram Start: \n\n");

    /* Open button 1 and button 2 */
    Button_Params_init(&buttonParams);
    buttonParams.buttonCallback   = handleButtonCallback;
    buttonParams.buttonEventMask  = Button_EV_CLICKED;
    buttonHandle[CONFIG_BUTTON_0] = Button_open(CONFIG_BUTTON_0, &buttonParams);
    buttonHandle[CONFIG_BUTTON_1] = Button_open(CONFIG_BUTTON_1, &buttonParams);

    /* Check if the button open is successful */
    if ((buttonHandle[CONFIG_BUTTON_1] == NULL) || (buttonHandle[CONFIG_BUTTON_0] == NULL))
    {
        Display_print0(display, 0, 0, "Button Open Failed!");
    }

    /* Open LED0 and LED1 with default params */
    LED_Params_init(&ledParams);
    ledHandle[CONFIG_LED_0] = LED_open(CONFIG_LED_0, &ledParams);
    ledHandle[CONFIG_LED_1] = LED_open(CONFIG_LED_1, &ledParams);
    if ((ledHandle[CONFIG_LED_0] == NULL) || (ledHandle[CONFIG_LED_1] == NULL))
    {
        Display_print0(display, 0, 0, "LED Open Failed!");
    }

    /* Create semaphore */
    semStatus = sem_init(&sem, 0, 0);
    if (semStatus != 0) while (1) {}

    /* Call ClockP init functions */
    ClockP_Params clockParams;
    ClockP_Params_init(&clockParams);
    clockParams.period = 0;
    clockParams.startFlag = false;
    timeoutClkHandle = ClockP_create(timeoutHandler, MAX_TIMEOUT / ClockP_getSystemTickPeriod(), &clockParams);
    caTimeoutClkHandle = ClockP_create(caTimeoutHandler, CA_DELAY / ClockP_getSystemTickPeriod(), &clockParams);

    /* Create a UART in CALLBACK read mode */
    UART2_Params_init(&uartParams);
    uartParams.readMode         = UART2_Mode_CALLBACK;
    uartParams.writeMode        = UART2_Mode_NONBLOCKING;
    uartParams.readCallback     = rxCallbackFxn;
    uartParams.eventCallback    = uartEventCallbackFxn;
    uartParams.eventMask        = UART2_EVENT_TX_FINISHED;
    uartParams.baudRate         = 921600;

    uart = UART2_open(CONFIG_UART2_1, &uartParams);
    hwAttrs = uart->hwAttrs;

    if (uart == NULL) while (1) {}

    //fill unicasting data
    for (int i=0;i<248;i++) controllerInst.unicastingData.payloadData[i] = i | i << 8 | i << 16 | i << 24;

    Display_print0(display,
        0,
        0,
        "============================Assign ID Stage============================");

    Display_print1(display,
        0,
        0,
        "Please assign ID for totally <%d> peripherals by press S1 on peripheral board one by one\n", 
        MAXIMUM_ID);

    controllerState = MODE_ASSIGN_ID;
    assign_id();

    Display_print1(display,
        0,
        0,
        "\nAll of %d peripheral boards ID has been assigned!!\n", 
        MAXIMUM_ID);

    controllerState = MODE_REPEAT_UNICAST;
    Display_print0(display,
        0,
        0,
        "\r\n======================= Repeat Test (Unicasting) ==========================");

    while (1)
    {
        Display_print0(display,
                0,
                0,
                "\n>> Take a repeat test based on unicasting command? Press S1->Yes, Press S2->No: ");
        sem_wait(&sem);

        if(!buttonID)
        {
            uint32_t repeatTimes = 100;
            Display_print0(display, 0, 0, "Yes");
            Display_print0(display,
                    0,
                    0,
                    "Input the repeat times. Press S1->Select, Press S2->Increment: \n");
            while(1)
            {
                Display_print1(display, 0, 0, "\b\b\b\b\b%05d", repeatTimes);
                sem_wait(&sem);
                if(!buttonID)
                {
                    Display_print1(display,
                        0,
                        0,
                        "%d times", 
                        repeatTimes);
                    break;
                }
                else 
                {
                    if (repeatTimes == 100) repeatTimes = 1000;
                    else if (repeatTimes == 1000) repeatTimes = 3000;
                    else if (repeatTimes == 3000) repeatTimes = 10000;
                    else if (repeatTimes == 10000) repeatTimes = 60000;
                    else repeatTimes = 100;
                }
            }
            Display_print0(display, 0, 0, "Repeat Test Ongoing...");
            uint8_t testID = 0;
            passTimes = 0;
            for (int i=0; i < repeatTimes; i++)
            {
                testID ++;
                if (testID > MAXIMUM_ID) testID = 1;
                Display_print2(display, 
                    0, 
                    0, 
                    "Test <%d> for ID-%d",
                    i, 
                    testID);
                repeat_unicasting_test(testID, i);
            }
            float passRate = passTimes * 100.0 / repeatTimes;
            Display_print2(display, 
                        0, 
                        0, 
                        "The pass rate among %d times test is: %0.2f%%",
                        repeatTimes, 
                        passRate);
        }
        else 
        {
            Display_print0(display, 0, 0, "No");
            break;
        }
    }

    Display_print0(display,
        0,
        0,
        "\r\n==========================Command Test Stage============================");

    // /* Loop forever echoing */
    while (1)
    {
        controllerState = MODE_BROADCAST;
        while (1)
        {
            Display_print0(display,
                0,
                0,
                "\n>> Take a Broadcast command test? Press S1->Yes, Press S2->No: ");
            sem_wait(&sem);
            if(!buttonID)
            {
                Display_print0(display, 0, 0, "Yes");
                broadcast_test();
            }
            else 
            {
                Display_print0(display, 0, 0, "No"); 
                break;
            }
        }
        controllerState = MODE_UNICAST;
        while (1)
        {
            Display_print0(display,
                0,
                0,
                "\n>> Take a Unicasting command test? Press S1->Yes, Press S2->No: ");
            sem_wait(&sem);
            if(!buttonID)
            {
                uint8_t testID = 1;
                Display_print0(display, 0, 0, "Yes");
                Display_print0(display,
                    0,
                    0,
                    "Input the peripheral ID you want to test. Press S1->Select, Press S2->Increment: \n");
                while(1)
                {
                    Display_print1(display, 0, 0, "\b\b%02d", testID);
                    sem_wait(&sem);
                    if(!buttonID)
                    {
                        Display_print1(display,
                            0,
                            0,
                            "Currently Peripheral ID <%d> is in testing", 
                            testID);
                        unicast_test(testID);
                        break;
                    }
                    else 
                    {
                        if (testID == MAXIMUM_ID) testID = 1;
                        else testID++;
                    }
                }
            }
            else 
            {
                Display_print0(display, 0, 0, "No"); 
                break;
            }
        }
        controllerState = MODE_ACTIVE_SEND;
        while (1)
        {
            Display_print0(display,
                0,
                0,
                "\n>> Take a Peripheral active send test? Press S1->Yes, Press S2->No: ");
            sem_wait(&sem);
            if(!buttonID)
            {
                Display_print0(display, 0, 0, "Yes");
                active_send_test();
            }
            else 
            {
                Display_print0(display, 0, 0, "No"); 
                break;
            }
        }
    }
}
