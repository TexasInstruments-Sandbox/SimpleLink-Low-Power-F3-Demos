/*
 * Copyright (c) 2024-2025, Texas Instruments Incorporated
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
 *  ======== rfPacket_ieee_test.c ========
 */

/* XDC module Headers */
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* POSIX Header files */
#include <pthread.h>

/* BIOS module Headers */
#include <FreeRTOS.h>
#include <task.h>

#include <ti/drivers/GPIO.h>
#include <ti/drivers/rcl/RCL.h>
#include <ti/drivers/rcl/RCL_Scheduler.h>
#include <ti/drivers/rcl/commands/ieee.h>

#include "ti_radio_config.h"

/* Driver configuration */
#include "ti_drivers_config.h"

#define PKT_LEN 38
#define HDR_LEN 1
#define NUM_PAD 0
#define NUM_RX_BUF 2
#define MULTI_BUF_SZ 256
#define FREQUENCY 2435000000
#define CRC_LEN 2
#define PAN_ID 0xF00D
#define SHORT_ADDRESS_RX 0xABCD
#define SHORT_ADDRESS_TX 0x1234 

#define EXAMPLE_MODE_RX 0
#define EXAMPLE_MODE_TX 1

/* Set to 0 to send the same packet an unlimited number of times */
/* Another value gives different packets - make sure all can fit in the receive buffer*/
volatile uint8_t numPkt = 0;

/* Define EXAMPLE_MODE to build for RX or TX, or use debugger to write to exampleMode variable to set the mode */
#ifndef EXAMPLE_MODE
volatile uint32_t exampleMode = EXAMPLE_MODE_TX;
#else
volatile uint32_t exampleMode = EXAMPLE_MODE;
#endif

volatile uint16_t numPktOk = 0;
volatile ti_drivers_utils_List__include
RCL_CommandStatus lastStatus;
uint8_t ieeeAddr[8] = {0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d};

static RCL_Client  rclClient;

/* Generate example packets */
void generatePacketIeee(RCL_Buffer_TxBuffer *txBuffer, uint32_t pktLen)
{
    uint8_t *txData;
    static uint32_t seqNumber = 0;

    /* Initialize Tx Buffer */
    txData = RCL_TxBuffer_init(txBuffer, NUM_PAD, HDR_LEN, pktLen);

    /* Insert length field in the header */
    txData[0] = pktLen + CRC_LEN;
    /* Write MAC header */
    /* FCF */
    txData[1] = 0x41; /* Data frame, PAN id compression */
    txData[2] = 0x98; /* Short src address, frame version 1, short destination address*/
    txData[3] = seqNumber;
    txData[4] = PAN_ID & 0xFF;
    txData[5] = (PAN_ID & 0xFF00) >> 8;
    txData[6] = SHORT_ADDRESS_RX & 0xFF;
    txData[7] = (SHORT_ADDRESS_RX & 0xFF00) >> 8;
    txData[8] = SHORT_ADDRESS_TX & 0xFF;
    txData[9] = (SHORT_ADDRESS_TX & 0xFF00) >> 8;

    /* Sequence number and address*/

    for (uint32_t i = 10; i < pktLen + HDR_LEN; i++)
    {
        txData[i] = i & 0xFF;
    }
    seqNumber++;
}


/* Callback function for use in RX in order to process packets and send ACK */
void ieeeCallback(RCL_Command *cmd, LRF_Events lrfEvents, RCL_Events rclEvents)
{
    RCL_CmdIeeeRxTx *rxTxCmd = (RCL_CmdIeeeRxTx *)cmd;

    if (lrfEvents.rxCtrlAck)
    {
        uint32_t rxBuffer[RCL_Buffer_bytesToWords(RCL_Buffer_entryLen(NUM_PAD, HDR_LEN, PKT_LEN))];
        RCL_Buffer_DataEntry *rxEntry = (RCL_Buffer_DataEntry *)rxBuffer;
        /* Ack is under reception. Look at the packet received so far */
        RCL_IEEE_readPartialFrame(rxTxCmd, rxEntry, sizeof(rxBuffer));
        if (rxEntry->length >= 4)
        {

        }
    }

    if (rclEvents.rxEntryAvail)
    {
        /* Packet received */
        List_List finishedBuffers;
        RCL_MultiBuffer *multiBuffer;
        /* Prepare list of RX buffers that are done */
        List_clearList(&finishedBuffers);
        RCL_Buffer_DataEntry *rxPkt = RCL_MultiBuffer_RxEntry_get(&rxTxCmd->rxAction->rxBuffers, &finishedBuffers);
        /* Make finished buffers available to RCL command */
        while ((multiBuffer = RCL_MultiBuffer_get(&finishedBuffers)) != NULL)
        {
            RCL_MultiBuffer_clear(multiBuffer);
            RCL_MultiBuffer_put(&rxTxCmd->rxAction->rxBuffers, multiBuffer);
        }

        bool pktOk = false;
        if (rxPkt != NULL)
        {
            /* Check contents of packet */
            if (rxPkt->length > 8)
            {
                pktOk = true;
                GPIO_toggle(CONFIG_GPIO_GLED);
            }
        }
    }
}

/* Function for running TX and trying to receive ACK */
void runIeeeTx(void)
{
    RCL_init();

    RCL_Handle h = RCL_open(&rclClient, &LRF_config);

    /* Prepare command */
    RCL_CmdIeeeRxTx cmd = RCL_CmdIeeeRxTx_DefaultRuntime();
    RCL_CmdIeee_RxAction rxAction = RCL_CmdIeee_RxAction_DefaultRuntime();
    RCL_CmdIeee_TxAction txAction = RCL_CmdIeee_TxAction_DefaultRuntime();
    uint32_t pktBuffer[RCL_TxBuffer_len_u32(NUM_PAD, HDR_LEN, PKT_LEN)];
    RCL_Buffer_TxBuffer *txBuffer;

    cmd.rfFrequency = FREQUENCY;
    /* Start command immediately */
    cmd.common.scheduling = RCL_Schedule_Now;
    cmd.common.allowDelay = true;
    cmd.txPower = (RCL_Command_TxPower) {.dBm = 0, .fraction = 0}; /* Send with 0 dBm */
    cmd.rxAction = NULL;
    cmd.txAction = &txAction;

    /* Set up TX Action */
    /* Start TX immediately */
    txAction.ccaScheduling = RCL_Schedule_Now;
    txAction.allowDelay = true;
    /* No CCA */
    txAction.ccaMode = RCL_CmdIeee_NoCca;
    /* No ACK */
    txAction.expectImmAck = false;
    txAction.expectEnhAck = false;

    txBuffer = (RCL_Buffer_TxBuffer *)pktBuffer;

    while(1)
    {
        generatePacketIeee(txBuffer, PKT_LEN);        
        txAction.txEntry = (RCL_Buffer_DataEntry *) (&txBuffer->length);
        /* Enter command */
        RCL_Command_submit(h, &cmd);
        /* Wait for command to finish. Alternatively, a callback on lastCmdDone may be used */
        RCL_Command_pend(&cmd);
        /* Record command status, may be checked to look for errors */
        lastStatus = cmd.common.status;
        GPIO_toggle(CONFIG_GPIO_GLED);
        sleep(1);
    }
}


/* Function for running RX and sending ACK */
void runIeeeRx(void)
{
    RCL_init();

    RCL_Handle h = RCL_open(&rclClient, &LRF_config);

    /* Prepare command */
    RCL_CmdIeeeRxTx cmd = RCL_CmdIeeeRxTx_DefaultRuntime();
    RCL_CmdIeee_RxAction rxAction = RCL_CmdIeee_RxAction_DefaultRuntime();
    uint32_t rxMultiBuffer[NUM_RX_BUF][MULTI_BUF_SZ / 4];

    cmd.rfFrequency = FREQUENCY;
    /* Start command immediately */
    cmd.common.scheduling = RCL_Schedule_Now;
    cmd.common.allowDelay = true;
    cmd.txPower = (RCL_Command_TxPower) {.dBm = 0, .fraction = 0}; /* Send with 0 dBm */
    /* Set the command to run for up to 20 seconds */
    /* If relGracefulStopTime is set to 0, the RX command will run until stopped or until it gets an error */
    cmd.common.timing.relGracefulStopTime = 0; //RCL_SCHEDULER_SYSTIM_MS(20000);

    cmd.rxAction = &rxAction;
    cmd.common.runtime.callback = ieeeCallback;
    /* Set up callback on packet under reception; trigger for ACK generation */
    cmd.common.runtime.lrfCallbackMask.value = LRF_EventRxCtrlAck.value | LRF_EventRxCtrl.value | LRF_EventRxOk.value | LRF_EventRxNok.value;
    /* Set up callbacks on packet fully received and command done */
    cmd.common.runtime.rclCallbackMask.value =
        RCL_EventLastCmdDone.value | RCL_EventRxEntryAvail.value;

    /* Set up RX action */
    rxAction = RCL_CmdIeee_RxAction_DefaultRuntime();
    rxAction.numPan = 1;
    rxAction.panConfig[0] = RCL_CmdIeee_PanConfig_DefaultRuntime();
    /* Enable ACK of received packets */
    //rxAction.panConfig[0].autoAckMode = RCL_CmdIeee_AutoAck_ImmAckProvidedFrame;
    rxAction.panConfig[0].panCoord        = 1;
    rxAction.panConfig[0].localPanId      = PAN_ID;
    memcpy(&rxAction.panConfig[0].localExtAddr, ieeeAddr, sizeof(rxAction.panConfig[0].localExtAddr));
    rxAction.panConfig[0].localShortAddr  = SHORT_ADDRESS_RX;
    rxAction.panConfig[0].autoAckMode     = RCL_CmdIeee_AutoAck_ImmAckAutoPendAll;
    rxAction.panConfig[0].defaultPend     = 1;
    rxAction.panConfig[0].maxFrameVersion = 1;
    rxAction.panConfig[0].sourceMatchingTableShort             = NULL;

    /* Set up Rx (multi)buffer */
    for (int i = 0; i < NUM_RX_BUF; i++)
    {
        RCL_MultiBuffer *multiBuffer = (RCL_MultiBuffer *) rxMultiBuffer[i];
        RCL_MultiBuffer_init(multiBuffer, MULTI_BUF_SZ);
        RCL_MultiBuffer_put(&rxAction.rxBuffers, multiBuffer);
    }

    /* Enter command */
    RCL_Command_submit(h, &cmd);

    /* Wait for command to finish. Alternatively, the callback on lastCmdDone may be used */
    RCL_Command_pend(&cmd);

    /* Record command status, may be checked to look for errors */
    lastStatus = cmd.common.status;
}

/*
 *  ======== mainThread ========
 */

void *mainThread(void *arg0)
{
    GPIO_init();

    /* Configure the LED pin */
    GPIO_setConfig(CONFIG_GPIO_GLED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    /* Turn off user LED */
    GPIO_write(CONFIG_GPIO_GLED, CONFIG_GPIO_LED_OFF);

    if (exampleMode == EXAMPLE_MODE_TX)
    {
        runIeeeTx();
    }
    else if (exampleMode == EXAMPLE_MODE_RX)
    {
        runIeeeRx();
    }

    pthread_exit(NULL);
    return NULL;
}