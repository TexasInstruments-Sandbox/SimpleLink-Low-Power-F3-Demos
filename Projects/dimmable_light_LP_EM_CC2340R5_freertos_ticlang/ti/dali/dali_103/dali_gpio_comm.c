/*
* Copyright (c) 2024, Texas Instruments Incorporated
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

#include "dali_gpio_comm.h"

#define IS_IN_RANGE(data, min, max) ((data) >= (min) && (data) <= (max))

/* Internal timing state */
static uint32_t lastEdgeTick = 0;
static uint32_t settlingStartTick = 0;

/* Forward declarations for callbacks */
static void DALI_TX_ClockCallback(void);
static void DALI_Settling_ClockCallback(void);
static void DALI_RX_EdgeCallback(void);
static void DALI_RX_TimeoutCallback(void);

/* Function Declarations */
/**
 * @brief Enable Collision Detection Mechanism
 */
static void DALI_TX_enableCollisionDetection(void);

/**
 * @brief Reset DALI Tx 
 */
static void DALI_TX_ResetTx(void);

/**
 * @brief Check if Bit received in Rx is similar to Tx  
 */
static bool DALI_TX_checkForCollision(void);

/**
 * @brief Check if Timing of the bit Received is within tolerance range  
 */
static bool DALI_TX_checkTiming(void);

/**
 * @brief define procedure to handle Collision
 */
static void DALI_TX_handleCollision(void);

/**
 * @brief decode received bits from DALI Rx and returns true if it is a proper frame
 */
static bool DALI_RX_decodeFrame(void);

void DALI_TX_enableTimer_Output(void)
{
    /* Configure TX pin as output for bit-banging */
    DALI_HAL_GPIO_setTxOutput();
}

static void DALI_TX_enableCollisionDetection(void)
{
    dali_gpio.Tx.isCDetectionActive = true;
    /* Collision detection is checked in TX clock callback after delay */
}

static void DALI_TX_ResetTx(void)
{
    DALI_HAL_Clock_stop(DALI_CLOCK_TX);
    DALI_HAL_GPIO_writeTx(1); /* Set TX line high (idle state) */

    dali_gpio.Tx.tIndex = 0;
    dali_gpio.Tx.isCDetectionActive = false;
}

static bool DALI_TX_checkForCollision(void)
{
    if(dali_gpio.Rx.currBit != dali_gpio.Tx.currBit)
    {
        return true;
    }

    return false;
}

static bool DALI_TX_checkTiming(void)
{
    for(int i = 0 ; i < dali_gpio.Rx.captIndex; i++)
    {
        if((!IS_IN_RANGE(dali_gpio.Rx.captBitTimings[i],DALI_TX_CD_HALFBIT_MIN_CYC,DALI_TX_CD_HALFBIT_MAX_CYC))
                &&
           (!IS_IN_RANGE(dali_gpio.Rx.captBitTimings[i],DALI_TX_CD_BIT_MIN_CYC,DALI_TX_CD_BIT_MAX_CYC)))
        {
            /* Timing of the signal has been violated */
            return false;
        }
    }

    return true;
}

static void DALI_TX_handleCollision(void)
{
    if(DALI_TX_checkTiming())
    {
        /* Collision Avoidance */
        DALI_TX_ResetTx();
        dali_gpio.Tx.status = TX_COLLISION_AVOIDANCE;
    }

    else
    {
        /* Collision Recovery */
        dali_gpio.Tx.status = TX_COLLISION_BREAK;

        dali_gpio.Tx.isCDetectionActive = false;

        DALI_HAL_Clock_stop(DALI_CLOCK_TX);

        /* Set TX low for break time */
        DALI_HAL_GPIO_writeTx(0);

        /* Set settling clock for collision recovery break time */
        DALI_HAL_Clock_setPeriod(DALI_CLOCK_SETTLING, TRANSMITTER_CR_BREAK_TIME);
        DALI_HAL_Clock_start(DALI_CLOCK_SETTLING);
    }
}

static bool DALI_RX_decodeFrame(void)
{
    uint8_t halfBits[100] = {0}, halfBitIndex= 0;

    /* Check if start bit is proper */
    if(!(dali_gpio.Rx.captBits[0] == 1 && dali_gpio.Rx.captBits[1] == 0))
        return false;


    for(int i=0 ; i < dali_gpio.Rx.captIndex ; i++)
    {
        if(i > 0 && (dali_gpio.Rx.captBits[i] == dali_gpio.Rx.captBits[i-1]))
            return false;

        if(IS_IN_RANGE(dali_gpio.Rx.captBitTimings[i], DALI_RX_HALFBIT_MIN_CYC,DALI_RX_HALFBIT_MAX_CYC))
        {
            halfBits[halfBitIndex++] = dali_gpio.Rx.captBits[i];
        }

        else if(IS_IN_RANGE(dali_gpio.Rx.captBitTimings[i], DALI_RX_BIT_MIN_CYC,DALI_RX_BIT_MAX_CYC))
        {
            halfBits[halfBitIndex++] = dali_gpio.Rx.captBits[i];
            halfBits[halfBitIndex++] = dali_gpio.Rx.captBits[i];
        }

        else
        {
            return false;
        }
    }

    /* Last bit is not counted if it is high ,so incrementing the index if it is odd*/
    if(halfBitIndex % 2 == 1)
    {
        halfBitIndex++;
    }

    /* Check if it contains all the bits (start bit and 8 bit frames )*/
    if((halfBitIndex - 2) % 16  != 0)
    {
        return false;
    }

    /* Check for Send Twice commands */
    uint32_t prevRxData = 0;
    for(int i = 0; i < dali_gpio.Rx.dataSize; i++)
    {
        prevRxData = prevRxData << 8;
        prevRxData |= dali_gpio.Rx.dataArr[i];
    }


    dali_gpio.Rx.dataSize = (halfBitIndex - 2) / 16;

    /* Decode Rx Data */
    uint32_t RxData = 0;

    for(int i = 2 ; i < halfBitIndex; i += 2)
    {
       if(halfBits[i] == 0 && halfBits[i + 1] == 1)
       {
           RxData = (RxData << 1) | 0 ;
       }

       else
       {
           RxData = (RxData << 1) | 1 ;
       }
    }



    /* Check if command follows Send Twice Command conditions */
    dali_gpio.Rx.isSendTwiceCmd = false;

    /* Calculate elapsed time since settling started */
    uint32_t currentTick = DALI_HAL_getTicks();
    uint32_t settlingTimeUs = DALI_HAL_getTickDiff(settlingStartTick, currentTick);

    if( (prevRxData == RxData)
        &&
        IS_IN_RANGE( settlingTimeUs, SEND_TWICE_SETTLING_MIN , SEND_TWICE_SETTLING_MAX))
    {
        dali_gpio.Rx.isSendTwiceCmd = true;
    }

    for(int i = dali_gpio.Rx.dataSize - 1; i >= 0 ; i--)
    {
        dali_gpio.Rx.dataArr[i] = RxData & 0xFF;
        RxData = RxData >> 8;
    }


    return true;
}

void DALI_TX_transmitFrame(void)
{
    dali_gpio.Tx.tBitsCount = 0;

    dali_gpio.Tx.transmitBits[dali_gpio.Tx.tBitsCount++] = 1;
    dali_gpio.Tx.transmitBits[dali_gpio.Tx.tBitsCount++] = 0;

    for(int j = 0; j < dali_gpio.Tx.dataSize ; j++)
    {
        for(int i = 7; i >= 0 ;i--)
        {
            if(dali_gpio.Tx.dataArr[j] & (1 << i))
            {
                dali_gpio.Tx.transmitBits[dali_gpio.Tx.tBitsCount++] = 1;
                dali_gpio.Tx.transmitBits[dali_gpio.Tx.tBitsCount++] = 0;
            }

            else
            {
                dali_gpio.Tx.transmitBits[dali_gpio.Tx.tBitsCount++] = 0;
                dali_gpio.Tx.transmitBits[dali_gpio.Tx.tBitsCount++] = 1;  
            }
        }
    }

    /* Stop Condition */
    dali_gpio.Tx.transmitBits[dali_gpio.Tx.tBitsCount++] = 0;

    dali_gpio.Tx.status = TX_TRANSMIT_INPROGRESS;

    /* First rising edge is not detected in ISR (Rx.currBit is not updated), so setting init value as 1 to Pass Collision Detection */
    dali_gpio.Rx.currBit = 1;

    DALI_HAL_Clock_start(DALI_CLOCK_TX);

    while(dali_gpio.Tx.status == TX_TRANSMIT_INPROGRESS)
    {};

    if(dali_gpio.Tx.status == TX_COLLISION_DETECTED)
    {
        DALI_TX_handleCollision();

        while((dali_gpio.Tx.status != TX_COLLISION_AVOIDANCE)
              && (dali_gpio.Tx.status != TX_COLLISION_RECOVER)
              && (dali_gpio.Tx.status != TX_IDLE))
        {};

        if(dali_gpio.Tx.status == TX_COLLISION_RECOVER)
        {
            dali_gpio.Tx.settlingTime = TRANSMITTER_CR_RECOVERY_TIME; 
        } 
    }

    return ;
}


/* TX Clock Callback - Called every 416.67us for DALI half-bit timing */
static void DALI_TX_ClockCallback(void)
{
    if(dali_gpio.Tx.status == TX_TRANSMIT_INPROGRESS)
    {
        if(dali_gpio.Tx.tIndex == dali_gpio.Tx.tBitsCount)
        {
            dali_gpio.Tx.status = TX_TRANSMIT_SUCCESS;
            DALI_TX_ResetTx();
            return;
        }

        /* Check if Tx is driving the line high or low */
        if(dali_gpio.Tx.tIndex > 0)
        {
            /* Enable Collision Detection */
            DALI_TX_enableCollisionDetection();
        }

        if(dali_gpio.Tx.isCDetectionActive && DALI_TX_checkForCollision())
        {
            DALI_TX_ResetTx();
            dali_gpio.Tx.status = TX_COLLISION_DETECTED;
        }

        /* Bit-bang the TX pin - DALI uses inverted logic (1 = low voltage, 0 = high voltage) */
        if(dali_gpio.Tx.transmitBits[dali_gpio.Tx.tIndex++] & 0x1)
        {
            dali_gpio.Tx.currBit = 1;
            DALI_HAL_GPIO_writeTx(0); /* DALI logic 1 = low voltage */
        }
        else
        {
            dali_gpio.Tx.currBit = 0;
            DALI_HAL_GPIO_writeTx(1); /* DALI logic 0 = high voltage */
        }
    }

    /* Check collision detection after delay */
    else if((dali_gpio.Tx.status == TX_COLLISION_BREAK) && dali_gpio.Tx.isCDetectionActive)
    {
        /* Check if bus is still in active state, after the break time */
        if(DALI_TX_checkForCollision())
        {
            DALI_TX_ResetTx();
            dali_gpio.Tx.status = TX_COLLISION_AVOIDANCE;
        }
        else
        {
            DALI_TX_ResetTx();
            dali_gpio.Tx.status = TX_COLLISION_RECOVER;
        }
    }
}

/* Settling Timer Callback - Called when collision recovery break time expires */
static void DALI_Settling_ClockCallback(void)
{
    if(dali_gpio.Tx.status == TX_COLLISION_BREAK)
    {
        DALI_HAL_Clock_stop(DALI_CLOCK_SETTLING);

        dali_gpio.Tx.currBit = 0;

        DALI_HAL_GPIO_writeTx(1);

        /* Enable Collision Detection to check if bus is in idle state for reduced settling time */
        DALI_TX_enableCollisionDetection();

        /* Restart TX clock for collision detection */
        DALI_HAL_Clock_start(DALI_CLOCK_TX);
    }
}

/* RX GPIO Interrupt Callback - Called on every edge transition */
static void DALI_RX_EdgeCallback(void)
{
    uint32_t currentTick = DALI_HAL_getTicks();
    uint32_t edgeDuration = DALI_HAL_getTickDiff(lastEdgeTick, currentTick);

    /* Read current RX pin state */
    uint8_t rxState = DALI_HAL_GPIO_readRx();

    /* Reset RX timeout timer - 2ms timeout for frame end detection */
    DALI_HAL_Clock_stop(DALI_CLOCK_RX_TIMEOUT);
    DALI_HAL_Clock_start(DALI_CLOCK_RX_TIMEOUT);

    /* If this is the first falling edge transition, do nothing*/
    if(!dali_gpio.Rx.isActive) dali_gpio.Rx.isActive = true;

    /* Falling edge (1->0 transition) */
    else if(rxState == 0)
    {
        dali_gpio.Rx.currBit = 0;

        /* Additional check for collision detection */
        if(dali_gpio.Tx.isCDetectionActive && (dali_gpio.Rx.currBit != dali_gpio.Tx.currBit))
        {
            if(dali_gpio.Tx.status == TX_TRANSMIT_INPROGRESS)
            {
                DALI_TX_ResetTx();
                dali_gpio.Tx.status = TX_COLLISION_DETECTED;
            }
        }

        /* Store the falling edge (bit = 1 in DALI) */
        if(dali_gpio.Rx.captIndex < 100)
        {
            dali_gpio.Rx.captBits[dali_gpio.Rx.captIndex] = 1;
            dali_gpio.Rx.captBitTimings[dali_gpio.Rx.captIndex++] = edgeDuration;
        }
    }
    /* Rising edge (0->1 transition) */
    else
    {
        dali_gpio.Rx.currBit = 1;

        /* Additional check for collision detection */
        if(dali_gpio.Tx.isCDetectionActive && (dali_gpio.Rx.currBit != dali_gpio.Tx.currBit))
        {
            if(dali_gpio.Tx.status == TX_TRANSMIT_INPROGRESS)
            {
                DALI_TX_ResetTx();
                dali_gpio.Tx.status = TX_COLLISION_DETECTED;
            }
        }

        /* Store the rising edge (bit = 0 in DALI) */
        if(dali_gpio.Rx.captIndex < 100)
        {
            dali_gpio.Rx.captBits[dali_gpio.Rx.captIndex] = 0;
            dali_gpio.Rx.captBitTimings[dali_gpio.Rx.captIndex++] = edgeDuration;
        }
    }

    lastEdgeTick = currentTick;
}

/* RX Timeout Callback - Called when 2ms elapses without edge (frame end) */
static void DALI_RX_TimeoutCallback(void)
{
    DALI_HAL_Clock_stop(DALI_CLOCK_RX_TIMEOUT);

    /* Decode captured bits only if the signal is received from another device */
    if(dali_gpio.Tx.status == TX_IDLE)
    {
        /* Verify if it's a proper frame */
        if(DALI_RX_decodeFrame())
        {
            /* set a Flag indicating new command is received */
            gReceive = true;
        }

        /* Handle Incorrect Rx Msg */
        else
        {
            dali_gpio.Rx.dataArr[0] = 0xFF;
            dali_gpio.Rx.dataArr[1] = 0xFF;
            dali_gpio.Rx.dataArr[2] = 0xFF;
            dali_gpio.Rx.dataArr[3] = 0xFF;
        }
    }

    /* Start settling timer */
    settlingStartTick = DALI_HAL_getTicks();

    /* Set bus status as Idle */
    dali_gpio.Rx.isActive = false;
    dali_gpio.Tx.status = TX_IDLE;

    dali_gpio.Rx.captIndex = 0;
}

/**
 * @brief Get elapsed time since settling timer started
 *
 * @return Elapsed time in microseconds
 */
uint32_t DALI_getSettlingElapsedTimeUs(void)
{
    uint32_t currentTick = DALI_HAL_getTicks();
    return DALI_HAL_getTickDiff(settlingStartTick, currentTick);
}

/**
 * @brief Initialize DALI GPIO communication
 *
 * This function must be called once before using DALI TX/RX functions.
 * It registers all callback functions with the HAL.
 */
void DALI_GPIO_init(void)
{
    /* Register TX clock callback for bit-banging transmission */
    DALI_HAL_registerTxCallback(DALI_TX_ClockCallback);

    /* Register settling timer callback for collision recovery */
    DALI_HAL_registerSettlingCallback(DALI_Settling_ClockCallback);

    /* Register RX timeout callback for frame end detection */
    DALI_HAL_registerRxTimeoutCallback(DALI_RX_TimeoutCallback);

    /* Register RX edge interrupt callback for receiving data */
    DALI_HAL_GPIO_setRxInput(DALI_RX_EdgeCallback);

    /* Enable RX edge interrupt */
    DALI_HAL_GPIO_enableRxInterrupt();

    /* Initialize timing state */
    lastEdgeTick = DALI_HAL_getTicks();
    settlingStartTick = lastEdgeTick;
}
