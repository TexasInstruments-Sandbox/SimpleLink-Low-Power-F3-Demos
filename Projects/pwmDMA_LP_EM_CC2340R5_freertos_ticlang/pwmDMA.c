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
 *  ======== pwmDMA.c ========
 */
/* For usleep() */
#include <unistd.h>
#include <stddef.h>

/* Driver Header files */
#include <ti/drivers/PWM.h>
#include <ti/drivers/dma/UDMALPF3.h>
#include <ti/drivers/pwm/PWMTimerLPF3.h>
#include <ti/drivers/timer/LGPTimerLPF3.h>
#include <ti/drivers/ADCBuf.h>

/* Driver configuration */
#include "ti_drivers_config.h"

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_lgpt.h)
#include DeviceFamily_constructPath(inc/hw_evtsvt.h)
#include DeviceFamily_constructPath(driverlib/evtsvt.h)


/* DMA option
 * (transfer size | src increment size | dst increment size | arbitrate size | mode)
 */
#define GPTDMA_CONTROL_OPTS (UDMA_SIZE_32 | UDMA_SRC_INC_32 | UDMA_DST_INC_NONE | UDMA_ARB_1 | UDMA_NEXT_USEBURST | UDMA_MODE_PINGPONG)
#define TRANSFER_SIZE_IN_ONE_BURST  512     // Transfers per DMA burst sequence
#define ADC_SAMPLE_SIZE             64      // Sample size of ADC buffer
#define DMA_CHANNEL                 6       // DMA channel used for transfers
#define PWM_PERIOD                  50000    /* Period in nanoseconds times 20 */

PWM_Handle pwm1 = NULL;

uint16_t sampleBufferOne[ADC_SAMPLE_SIZE];
uint16_t sampleBufferTwo[ADC_SAMPLE_SIZE];
uint32_t microVoltBuffer[ADC_SAMPLE_SIZE];
uint32_t outputBuffer[ADC_SAMPLE_SIZE];
uint32_t buffersCompletedCounter = 0;

volatile uDMAControlTableEntry *dmaGptTableEntryPri;
volatile uDMAControlTableEntry *dmaGptTableEntryAlt;
uint32_t gptDmaChannelMask = UDMA_CHANNEL_6_M;

/* Array to store duty cycles, DMA will use it as transfer source */
uint32_t dutyCycles[TRANSFER_SIZE_IN_ONE_BURST] = {0};

/* DMA transfer size, DMA done interrupt will be triggered after size is reached */
uint16_t dmaTransferSize = TRANSFER_SIZE_IN_ONE_BURST;
volatile uint32_t dataTransfered;

// Allocate DMA control table entry
ALLOCATE_CONTROL_TABLE_ENTRY(dmaTableEntryTimerPri, DMA_CHANNEL + UDMA_PRI_SELECT);
ALLOCATE_CONTROL_TABLE_ENTRY(dmaTableEntryTimerAlt, DMA_CHANNEL + UDMA_ALT_SELECT);

void adcBufCallback(ADCBuf_Handle handle,
                    ADCBuf_Conversion *conversion,
                    void *completedADCBuffer,
                    uint32_t completedChannel,
                    int_fast16_t status)
{
    /* Adjust raw ADC values and convert them to microvolts */
    ADCBuf_adjustRawValues(handle, completedADCBuffer, ADC_SAMPLE_SIZE, completedChannel);
    ADCBuf_convertAdjustedToMicroVolts(handle, completedChannel, completedADCBuffer, microVoltBuffer, ADC_SAMPLE_SIZE);
    GPIO_toggle(CONFIG_GPIO_GLED);
}

void timer1Callback(LGPTimerLPF3_Handle lgptHandle, LGPTimerLPF3_IntMask interruptMask) {
    // interrupt callback code goes here. Minimize processing in interrupt.
    GPIO_toggle(CONFIG_GPIO_RLED);

    if (HWREG(DMA_BASE + DMA_O_SETCHNLPRIALT) & (1 << DMA_CHANNEL)) {
        dmaGptTableEntryPri = &dmaTableEntryTimerPri;
        dmaGptTableEntryPri->pSrcEndAddr = dutyCycles + dmaTransferSize - 1;
        dmaGptTableEntryPri->pDstEndAddr = (void *)(LGPT3_BASE + LGPT_O_DMARW);
        dmaGptTableEntryPri->control  = GPTDMA_CONTROL_OPTS;
        dmaGptTableEntryPri->control |= UDMALPF3_SET_TRANSFER_SIZE(dmaTransferSize);

    }
    else {
        /* Set DMA control table entry */
        dmaGptTableEntryAlt = &dmaTableEntryTimerAlt;
        dmaGptTableEntryAlt->pSrcEndAddr = dutyCycles + dmaTransferSize - 1;
        dmaGptTableEntryAlt->pDstEndAddr = (void *)(LGPT3_BASE + LGPT_O_DMARW);
        dmaGptTableEntryAlt->control  = GPTDMA_CONTROL_OPTS;
        dmaGptTableEntryAlt->control |= UDMALPF3_SET_TRANSFER_SIZE(dmaTransferSize);
    }

}

/*
 *  ======== timerDmaEnable ========
 *  Enable GPTimer DMA event trigger
 */
static inline void timerDmaEnable(uint32_t ui32Base, uint32_t ui32DMAReq)
{
    uintptr_t key;

    key = HwiP_disable();
    /* Set RWADDR in the GPTimer DMA control register. */
    HWREG(ui32Base + LGPT_O_DMA) |= (LGPT_O_PC2CC/4) << LGPT_DMA_RWADDR_S;
    /* Set RWCNTR in the GPTimer DMA control register. */
    HWREG(ui32Base + LGPT_O_DMA) |= 0 << LGPT_DMA_RWCNTR_S;
    /* Set the requested bits in the GPTimer DMA control register. */
    HWREG(ui32Base + LGPT_O_DMA) |= ui32DMAReq;

    HwiP_restore(key);
}

/*
 *  ======== timerDmaDisable ========
 *  Disable GPTimer DMA event trigger
 */
static inline void timerDmaDisable(uint32_t ui32Base, uint32_t ui32DMAReq)
{
    uintptr_t key;

    key = HwiP_disable();
    /* Clear the requested bits in the GPTimer DMA control register. */
    HWREG(ui32Base + LGPT_O_DMA) &= ~ui32DMAReq;

    HwiP_restore(key);
}

/*
 *  ======== gptCaptureDmaInit ========
 *  Initialize DMA for GPTimer capture mode
 *  @param  transferSize      Data size to transfer
 */
void gptCaptureDmaInit(uint16_t transferSize)
{
    /* Set DMA control table entry */
    dmaGptTableEntryPri = &dmaTableEntryTimerPri;
    dmaGptTableEntryAlt = &dmaTableEntryTimerAlt;

    /* Set DMA src addresses */
    dmaGptTableEntryPri->pSrcEndAddr = dutyCycles + transferSize - 1;
    dmaGptTableEntryAlt->pSrcEndAddr = dutyCycles + transferSize - 1;

    /* Mux DMA channel to LGPT DMA request event */
    EVTSVTConfigureDma(EVTSVT_DMA_CH6, EVTSVT_PUB_LGPT3_DMA);

    /* Setup DMA destination addr and control options */
    dmaGptTableEntryPri->pDstEndAddr = (void *)(LGPT3_BASE + LGPT_O_DMARW);
    dmaGptTableEntryAlt->pDstEndAddr = (void *)(LGPT3_BASE + LGPT_O_DMARW);

}

/*
 *  ======== gptCaptureDmaStart ========
 *  Start a DMA transfer for GPTimer capture mode
 *  @param  transferSize      Data size to transfer
 */
void gptCaptureDmaStart(uint16_t transferSize)
{
    /* Power up and enable clocks for uDMA. */
    Power_setDependency(PowerLPF3_PERIPH_DMA);

    /* Reconfigure control table before each transfer */
    dmaGptTableEntryPri->control  = GPTDMA_CONTROL_OPTS;
    dmaGptTableEntryAlt->control  = GPTDMA_CONTROL_OPTS;

    /* Set the size in the control options */
    dmaGptTableEntryPri->control |= UDMALPF3_SET_TRANSFER_SIZE(transferSize);
    dmaGptTableEntryAlt->control |= UDMALPF3_SET_TRANSFER_SIZE(transferSize);

    /* Enable burst mode */
    uDMAEnableChannelAttribute(gptDmaChannelMask, UDMA_ATTR_USEBURST);

    /* Enable DMA channel */
    UDMALPF3_channelEnable(gptDmaChannelMask);

    /* Enable zero interrupt */
    PWMTimerLPF3_Object *lgptObject = pwm1->object;
    LGPTimerLPF3_enableInterrupt(lgptObject->hTimer, LGPTimerLPF3_INT_ZERO);

    /* Configure the request bit */
    timerDmaEnable(LGPT3_BASE, LGPT_DMA_REQ_ZERO);
}

/*
 *  ======== gptCaptureDmaStop ========
 *  Stop a DMA transfer for GPTimer capture mode
 *  @param  transferSize      Data size to transfer
 */
void gptCaptureDmaStop(void)
{
    /* Disable clock on the DMA peripheral */
    Power_releaseDependency(PowerLPF3_PERIPH_DMA);

    /* Disable the RX DMA channel and stop the transfer. Disable and clear interrupts */
    UDMALPF3_channelDisable(gptDmaChannelMask);
    timerDmaDisable(LGPT3_BASE, LGPT_DMA_REQ_ZERO);
    UDMALPF3_clearInterrupt(gptDmaChannelMask);
}

/*
 *  ======== mainThread ========
 *  Task periodically increments the PWM duty for the on board LED.
 */
void *mainThread(void *arg0)
{
    /* Sleep time in microseconds */
    uint32_t time   = 50000;
    PWM_Params params;

    UDMALPF3_init();

    /* Call driver init functions. */
    PWM_init();

    PWM_Params_init(&params);
    params.dutyUnits   = PWM_DUTY_US;
    params.dutyValue   = 0;
    params.periodUnits = PWM_PERIOD_COUNTS;
    params.periodValue = PWM_PERIOD;
    pwm1               = PWM_open(CONFIG_PWM_0, &params);
    if (pwm1 == NULL)
    {
        /* CONFIG_PWM_0 did not open */
        while (1) {}
    }

    /* Prepare duty cycle buffer, duty cycle is the compare value for timer */
    uint32_t i;
    for(i = 0; i < TRANSFER_SIZE_IN_ONE_BURST/2; i++)
    {
        dutyCycles[i] = i * PWM_PERIOD / (TRANSFER_SIZE_IN_ONE_BURST/2);
    }
    for(i = TRANSFER_SIZE_IN_ONE_BURST/2; i < TRANSFER_SIZE_IN_ONE_BURST; i++)
    {
        dutyCycles[i] = (TRANSFER_SIZE_IN_ONE_BURST - i) * PWM_PERIOD / (TRANSFER_SIZE_IN_ONE_BURST/2);
    }

    //LGPT1 capture input configured as the LGPT3 DMA request event
    LGPTimerLPF3_Handle lgptHandle;
    LGPTimerLPF3_Params lgptParams;
    // Initialize parameters and assign callback function to be used
    LGPTimerLPF3_Params_init(&lgptParams);
    lgptParams.hwiCallbackFxn = timer1Callback;
    // Open driver
    lgptHandle = LGPTimerLPF3_open(CONFIG_LGPTIMER_1, &lgptParams);
    // Set counter target
    LGPTimerLPF3_setInitialCounterTarget(lgptHandle, TRANSFER_SIZE_IN_ONE_BURST, true);
    // Enable counter target interrupt
    LGPTimerLPF3_enableInterrupt(lgptHandle, LGPTimerLPF3_INT_TGT);
    HWREG(LGPT1_BASE + LGPT_O_PRECFG) = LGPT_PRECFG_TICKSRC_RISE_TICK;
    EVTSVTConfigureEvent(EVTSVT_SUB_LGPT1TEN, EVTSVT_PUB_LGPT3_DMA);
    // Start counter in count-up-periodic mode
    LGPTimerLPF3_start(lgptHandle, LGPTimerLPF3_CTL_MODE_UP_PER);

    /* Initialize DMA */
    gptCaptureDmaInit(dmaTransferSize);
    gptCaptureDmaStart(dmaTransferSize);

    EVTSVTConfigureEvent(EVTSVT_SUB_ADCTRG, EVTSVT_PUB_LGPT3_ADC);

    ADCBuf_Handle adcBuf;
    ADCBuf_Params adcBufParams;
    ADCBuf_Conversion continuousConversion;

    ADCBuf_init();
    /* Set up an ADCBuf peripheral in ADCBuf_RECURRENCE_MODE_CONTINUOUS */
    ADCBuf_Params_init(&adcBufParams);
    adcBufParams.callbackFxn       = adcBufCallback;
    adcBufParams.recurrenceMode    = ADCBuf_RECURRENCE_MODE_CONTINUOUS;
    adcBufParams.returnMode        = ADCBuf_RETURN_MODE_CALLBACK;
    adcBuf                         = ADCBuf_open(CONFIG_ADCBUF_0, &adcBufParams);

        /* Configure the conversion struct */
    continuousConversion.arg                   = NULL;
    continuousConversion.adcChannel            = CONFIG_ADC_CHANNEL_0;
    continuousConversion.sampleBuffer          = sampleBufferOne;
    continuousConversion.sampleBufferTwo       = sampleBufferTwo;
    continuousConversion.samplesRequestedCount = ADC_SAMPLE_SIZE;

    if (adcBuf == NULL)
    {
        /* ADCBuf failed to open. */
        while (1) {}
    }

    /* Start converting. */
    if (ADCBuf_convert(adcBuf, &continuousConversion, 1) != ADCBuf_STATUS_SUCCESS)
    {
        /* Did not start conversion process correctly. */
        while (1) {}
    }

    PWM_start(pwm1);

    /* Loop forever incrementing the PWM duty */
    while (1)
    {
        usleep(time);
    }
}
