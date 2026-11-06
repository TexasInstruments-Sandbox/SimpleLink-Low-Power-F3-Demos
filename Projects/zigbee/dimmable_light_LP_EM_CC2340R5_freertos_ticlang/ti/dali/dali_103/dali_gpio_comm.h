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

#ifndef DALI_GPIO_COMM_H_
#define DALI_GPIO_COMM_H_

#include <stdbool.h>
#include <stdint.h>
#include "dali_timings.h"
#include "dali_hal_simple.h"

/*! Timer DALI Rx Clock Frequency - provided by HAL */
#define TIMER_DALI_RX_CLK_MHZ DALI_HAL_TIMER_RX_CLK_MHZ

/*! Timer DALI Tx Clock Frequency - provided by HAL */
#define TIMER_DALI_TX_CLK_MHZ DALI_HAL_TIMER_TX_CLK_MHZ

/*! CC Index for DALI Tx - Channel 2 */
#define DALI_TX_CC_INDEX (2)

/*! CC Index for Collision Detection - Channel 1 */
#define DALI_TX_CD_CC_INDEX (1)

/*! Minimum cycles for DALI Rx Half Bit */
#define DALI_RX_HALFBIT_MIN_CYC (RECEIVER_HALF_BIT_MIN)
/*! Maximum cycles for DALI Rx Half Bit */
#define DALI_RX_HALFBIT_MAX_CYC (RECEIVER_HALF_BIT_MAX)
/*! Minimum cycles for DALI Rx Double Half Bit */
#define DALI_RX_BIT_MIN_CYC (RECEIVER_BIT_MIN)
/*! Maximum cycles for DALI Rx Double Half Bit */
#define DALI_RX_BIT_MAX_CYC (RECEIVER_BIT_MAX)

/*! Minimum cycles for Valid Half Bit */
#define DALI_TX_CD_HALFBIT_MIN_CYC \
    (TRANSMITTER_CD_HALFBIT_MIN)
/*! Maximum cycles for Valid Half Bit */
#define DALI_TX_CD_HALFBIT_MAX_CYC \
    (TRANSMITTER_CD_HALFBIT_MAX)
/*! Minimum cycles for Valid Bit */
#define DALI_TX_CD_BIT_MIN_CYC (TRANSMITTER_CD_BIT_MIN)
/*! Maximum cycles for Valid Bit */
#define DALI_TX_CD_BIT_MAX_CYC (TRANSMITTER_CD_BIT_MAX)

/*! DALI Tx Delay for checking Collision Detection */
#define DALI_TX_DELAY_COLLISION_DETECTION (3000)

/*! @enum DALI_TX_stateType */
typedef enum {
    /*! DALI Tx Idle state */
    TX_IDLE,
    /*! DALI Tx Transmit InProgress state*/
    TX_TRANSMIT_INPROGRESS,
    /*! DALI Tx Transmit Success state */
    TX_TRANSMIT_SUCCESS,
    /*! DALI Tx Collision Detected state */
    TX_COLLISION_DETECTED,
    /*! DALI Tx Collision Avoidance state */
    TX_COLLISION_AVOIDANCE,
    /*! DALI Tx Collision Break state */
    TX_COLLISION_BREAK,
    /*! DALI Tx Collision Recover state */
    TX_COLLISION_RECOVER
} DALI_TX_stateType;

/*! @enum DALI_frameType */
typedef enum {
    /*! 16 bit Forward Frame */
    FORWARD_FRAME_16,
    /*! 24 bit Forward Frame */
    FORWARD_FRAME_24,
    /*! Backward Frame*/
    BACKWARD_FRAME,
} DALI_frameType;

/*!
* @brief Variable to store DALI Rx information
*/
typedef struct {
    /*! Array to store receiving Rx Bits */
    uint8_t captBits[100];
    /*! Array to store timer duration of Rx Bits */
    uint32_t captBitTimings[100];
    /*! Stores number of bits being received */
    uint8_t captIndex;
    /*! Stores value of current bit received*/
    uint8_t currBit;
    /*! Stores size of the frame received */
    uint8_t dataSize;
    /*! Stores the frame received */
    uint8_t dataArr[4];
    /*! Flag to indicate whether received command is following SendTwice condn */
    bool isSendTwiceCmd;
    /*! Flag to indicate if the bus is not in Idle state */
    bool isActive;
} DALI_RX;

/*!
* @brief Variable to store DALI Tx information
*/
typedef struct {
    /*! Array to store Tx Bits */
    uint8_t transmitBits[100];
    /*! Stores Transmit Bits Count */
    uint8_t tBitsCount;
    /*! Stores Transmit Index */
    uint8_t tIndex;
    /*! Stores value of current bit transmitted */
    uint8_t currBit;
    /*! Stores Frame Type to be transmitted */
    DALI_frameType frame;
    /*! Stores Data Size to be transmitted*/
    uint8_t dataSize;
    /*! Stores Data to be transmitted */
    uint8_t dataArr[4];
    /*! Stores Settling time of the frame to be transmitted */
    float settlingTime;
    /*! Flag to indicate if Collision Detection mechanism is active */
    bool isCDetectionActive;
    /*! Flag to indicate if a frame is to be transmitted*/
    bool toSend;
    /*! Stores status of DALI Tx */
    DALI_TX_stateType status;
} DALI_TX;

/*!
 * @brief Variable to store DALI GPIO Comm information
*/
typedef struct {
    /*! DALI Rx info */
    DALI_RX Rx;
    /*! DALI Tx info */
    DALI_TX Tx;
} DALI_GPIO;

/*! Definition of global DALI GPIO variable */
volatile extern DALI_GPIO dali_gpio;

/*! Flag to Indicate if a frame has been received */
volatile extern bool gReceive;

/**
 * @brief   Get elapsed time since settling timer started
 *
 * @return Elapsed time in microseconds
 */
uint32_t DALI_getSettlingElapsedTimeUs(void);

/**
 * @brief   Initialize DALI GPIO Communication
 *
 * This function initializes DALI GPIO communication by registering all
 * callback functions with the HAL. Must be called before using DALI TX/RX.
 *
 */
void DALI_GPIO_init(void);

/**
 * @brief   Transmit Frame
 *
 * This function transmits the data with the information stored in DALI_TX
 *
 */
void DALI_TX_transmitFrame(void);

/**
 * @brief   Enable Tx Timer
 *
 * This function configures DALI_TX Timer and enables output generator for transmitting frame
 *
 */
void DALI_TX_enableTimer_Output(void);

#endif /* DALI_GPIO_COMM_H_ */
