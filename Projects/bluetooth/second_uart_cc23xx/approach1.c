#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <semaphore.h>

#include <ti/drivers/GPIO.h>
#include <ti/drivers/dpl/SwiP.h>
#include <ti/devices/cc23x0r5/inc/hw_ioc.h>
#include <ti/devices/cc23x0r5/inc/hw_gpio.h>
#include <ti/devices/cc23x0r5/inc/hw_lgpt.h>
#include <ti/devices/cc23x0r5/inc/hw_lgpt1.h>
#include <ti/devices/cc23x0r5/inc/hw_lgpt3.h>
#include <ti/devices/cc23x0r5/inc/hw_ints.h>
#include <ti/devices/cc23x0r5/inc/hw_clkctl.h>
#include <ti/devices/cc23x0r5/inc/hw_types.h>
#include <ti/devices/cc23x0r5/inc/hw_memmap.h>
#include <ti/devices/cc23x0r5/inc/hw_memmap_common.h>
#include <ti/devices/cc23x0r5/driverlib/interrupt.h>
#include <ti/drivers/power/PowerCC23X0.h>
#include "UART_BITBANG_CC23XX.h"
#include "ti_drivers_config.h"
#include "icall.h"
#include "icall_platform.h"
#include <icall_cc23x0_defs.h>

#include "FreeRTOS.h"
#include "task.h"

// LGPT3 for 24-bit timer
#define LOCAL_LGPT_TGT_VAL_S                    LGPT_TGT_VAL_S // same value works for all LGPT
#define LOCAL_LGPT_TGT_VAL_M                    LGPT3_TGT_VAL_M
#define LOCAL_CLKCTL_CLKENSET0_LGPTx_CLK_SET    CLKCTL_CLKENSET0_LGPT3_CLK_SET
#define LOCAL_CLKCTL_CLKCFG0_LGPTx_M            CLKCTL_CLKCFG0_LGPT3_M
#define LOCAL_CLKCTL_CLKCFG0_LGPTx_CLK_EN       CLKCTL_CLKCFG0_LGPT3_CLK_EN
#define LOCAL_INT_LGPTx_COMB                    INT_LGPT3_COMB
#define LOCAL_LGPTx_CNTR_VAL_M                  LGPT3_CNTR_VAL_M
#define LOCAL_PowerLPF3_PERIPH_LGPTx            PowerLPF3_PERIPH_LGPT3

// adding this hard code in case accessing timer address contributes too much to processing overhead
#define FAST_ACCESS_DEFAULT_TIMER_ID    (LGPT3_BASE)


#define FAST_ACCESS_DEFAULT_PIN_INDEX   (CONFIG_GPIO_BITBANG_UART_RX) // DIO1

#define HAS_ALL_BITS_RX(n) (n >= 8) // ORIGINAL
#define CHUNK_SIZE  8
#define ONE_BAUD_TICKS  334
#define INITIAL_OFFSET ONE_BAUD_TICKS + 100

#define ROUND_UP_DIV_UINT(n, d) (((n) + (d) - 1) / (d))
// #define ASSERT(s) { if (!s) while (1); }

#define SYS_CLK_FREQ_HZ (48000000)
#define DEFAULT_BAUD (115200) // Eventually to be turned into a variable for dynamic baud rate
#define DEFAULT_OVERCLOCKFACTOR ROUND_UP_DIV_UINT( SYS_CLK_FREQ_HZ, DEFAULT_BAUD ) // clock close to baudrate

#define TIMER_BASE          (LGPT3_BASE)
#define SYSCLK_HZ           (48000000u)
#define TICKS_PER_8p75US    (408u)   // 8.75us * 48MHz // 408

#define CNTR_BITS             24u
#define CNTR_MASK             ((1u << CNTR_BITS) - 1u)

#define PHASE_TICKS  (8u)  // ~167 ns extra delay on first tick

uint8_t rxBits = 0x0; // makeshift shift register
uint8_t rxBitCount = 0; // tracks how many bits received, including start and stop. reset to 0 upon receiving full 10-bit UART frame
uint8_t rxSize;
uint8_t byteReady = 0;
uint8_t chunkLeft;
uint8_t num;
uint8_t chunkFinal;

uint8_t firstByte = 0;

static void startBitDetectCallback(uint_least8_t index);

// Open UART BitBang implementation
void bitbang_open(void)
{
    GPIO_setConfig(FAST_ACCESS_DEFAULT_PIN_INDEX, GPIO_CFG_IN_NOPULL | GPIO_CFG_IN_INT_FALLING);
    GPIO_setCallback(FAST_ACCESS_DEFAULT_PIN_INDEX, startBitDetectCallback);

    // Enable LGPT3 clock & power
    HWREG(CLKCTL_BASE + CLKCTL_O_CLKENSET0) |= CLKCTL_CLKENSET0_LGPT3_CLK_SET;
    while ((HWREG(CLKCTL_BASE + CLKCTL_O_CLKCFG0) & CLKCTL_CLKCFG0_LGPT3_M) !=
            CLKCTL_CLKCFG0_LGPT3_CLK_EN) { /* wait */ }
    Power_setDependency(PowerLPF3_PERIPH_LGPT3);

    // Clean state
    HWREG(TIMER_BASE + LGPT_O_CTL) &= ~LGPT_CTL_MODE_M;     // stop
    HWREG(TIMER_BASE + LGPT_O_EMU)  = LGPT_EMU_HALT_DIS;

    // Tick source = CLK (48MHz), divider = 0 → 1 tick = 1/48e6 s
    HWREG(TIMER_BASE + LGPT_O_PRECFG) &= ~(LGPT_PRECFG_TICKSRC_M | LGPT_PRECFG_TICKDIV_M);
    HWREG(TIMER_BASE + LGPT_O_PRECFG) |=
        (LGPT_PRECFG_TICKSRC_CLK << LGPT_PRECFG_TICKSRC_S) |
        (0u << LGPT_PRECFG_TICKDIV_S);

    HWREG(TIMER_BASE + LGPT_O_TGT)  = ((TICKS_PER_8p75US + PHASE_TICKS) << LGPT_TGT_VAL_S);
    HWREG(TIMER_BASE + LGPT_O_PTGT) = (TICKS_PER_8p75US << LGPT_TGT_VAL_S);

    // Clear pending, reset counter, then start periodic-up (interrupts not used)
    HWREG(TIMER_BASE + LGPT_O_ICLR)  = LGPT_ICLR_TGT_CLR;
    HWREG(TIMER_BASE + LGPT_O_CNTR)  = 0;
    // firstByte = 0;

}


static void startBitDetectCallback(uint_least8_t index)
{
    // Disable GPIO Interrupt Until the next full packet
    GPIO_disableInt(FAST_ACCESS_DEFAULT_PIN_INDEX);

    // Disable HWI and SWI to ensure no interruptions
    uint32_t key1 = HwiP_disable();
    uint32_t key2 = SwiP_disable();

    // Debug Signal
    GPIO_write(CONFIG_GPIO_TEST, 0);

    // We will be sampling every 16 Bytes Together until All Bytes Have Been Sampled
    uint8_t sample_size = 0;
    uint8_t byteNum = CHUNK_SIZE;

    // // Number of bytes to sample
    if(chunkLeft == 0)
    {
        byteNum = chunkFinal;
    }

    // Re-align LGPT to the falling edge (kill stale hits, zero counter)
    HWREG(TIMER_BASE + LGPT_O_CTL)  &= ~LGPT_CTL_MODE_M;       // stop
    (void)HWREG(TIMER_BASE + LGPT_O_RIS);                      // sync read (optional)
    HWREG(TIMER_BASE + LGPT_O_ICLR)  = LGPT_ICLR_TGT_CLR;      // clear any pending TGT
    HWREG(TIMER_BASE + LGPT_O_CNTR)  = 0;                      // reset phase
    HWREG(TIMER_BASE + LGPT_O_CTL)   =
        LGPT_CTL_INTP_LATE | (LGPT_CTL_MODE_UP_PER << LGPT_CTL_MODE_S);  // start fresh
    HWREG(FAST_ACCESS_DEFAULT_TIMER_ID + LGPT_O_CTL) |= (LGPT_CTL_MODE_UP_PER << LGPT_CTL_MODE_S);


    for(int i = 0; i< (byteNum * 8); i++)
    {
        // Debug Signal
        GPIO_write(CONFIG_GPIO_TEST, 1);

        // Read the GPIO state
        uint_fast8_t pinLevel = HWREGB(GPIO_BASE + GPIO_O_DIN3_0 + FAST_ACCESS_DEFAULT_PIN_INDEX);
        
        rxBits = (rxBits >> 1) | (pinLevel << 7); //used to be 8
        rxBitCount++;

        if( HAS_ALL_BITS_RX(rxBitCount ) )
        {
            rxBitCount = 0;
            
            // Add the data into the array
            mainThreadBuffer[num] = (uint8_t) rxBits;
            num++;
            // Increment Sampled Bytes Size
            sample_size++;

            // Check if 16 Bytes have been sampled
            if(sample_size >=CHUNK_SIZE && chunkLeft != 0)
            {
                // Enable HWI Temporarily 
                SwiP_restore(key2);
                HwiP_restore(key1);

                // Enable interrupts to detect next start bit
                GPIO_enableInt(FAST_ACCESS_DEFAULT_PIN_INDEX);
    
                // Track How Many byte Chunks Need to Be Sampled
                chunkLeft--;
                GPIO_write(CONFIG_GPIO_TEST, 0);
                return;
            }
            GPIO_write(CONFIG_GPIO_TEST, 0);

            while ( !(HWREG(FAST_ACCESS_DEFAULT_TIMER_ID + LGPT_O_RIS) & LGPT_RIS_TGT_M) );
    
            // clear interrupt
            HWREG(FAST_ACCESS_DEFAULT_TIMER_ID + LGPT_O_ICLR) = LGPT_ICLR_TGT_CLR;

            while ( !(HWREG(FAST_ACCESS_DEFAULT_TIMER_ID + LGPT_O_RIS) & LGPT_RIS_TGT_M) );
    
            // clear interrupt
            HWREG(FAST_ACCESS_DEFAULT_TIMER_ID + LGPT_O_ICLR) = LGPT_ICLR_TGT_CLR;

        }

        // Debug Signal
        GPIO_write(CONFIG_GPIO_TEST, 0);

        while ( !(HWREG(FAST_ACCESS_DEFAULT_TIMER_ID + LGPT_O_RIS) & LGPT_RIS_TGT_M) );
    
        // clear interrupt
        HWREG(FAST_ACCESS_DEFAULT_TIMER_ID + LGPT_O_ICLR) = LGPT_ICLR_TGT_CLR;

    }
    
    // Enable HWI
    SwiP_restore(key2);
    HwiP_restore(key1);

    // Unblock bitbang_read
    sem_post(&sem1);

}

// Read UART packet
void bitbang_read(uint8_t size)
{
    bitbang_open();

    // enable interrupt to detect start bit falling edge
    GPIO_enableInt(FAST_ACCESS_DEFAULT_PIN_INDEX);

    // 2. Globalize the Size
    rxSize = size;
    chunkLeft = size/CHUNK_SIZE;
    chunkFinal = size % CHUNK_SIZE;
    num = 0;

    // 3. Wait Until Sampling is done
    sem_wait(&sem1);
}
