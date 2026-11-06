#include "uartFrameProtocol.h"
#include <ti/drivers/UART2.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/GPIO.h>
#include <unistd.h>
#include "ti_ble_config.h"
#include <string.h>
#include "bcomdef.h"


uint8_t advState = 1;

void processUARTFrame(char *buffer)
{
    // detect cmd
    size_t size = buffer[0];    // Extract size
    char *cmd = buffer + 1; // Provide uart frame

    // Toggle red LED
    if(size == 3 && memcmp("red",cmd, size) == 0)
    {
        GPIO_toggle(CONFIG_GPIO_LED_RED);
        UART2_write(uart,"Toggle red LED!\n\r", 17, NULL);

    }
    // Toggle green LED
    else if(size == 5 && memcmp("green", cmd, size) == 0)
    {
        GPIO_toggle(CONFIG_GPIO_LED_GREEN);

        UART2_write(uart,"Toggle green LED!\n\r", 19, NULL);

    }
    // Toggle Advertising
    else if(size == 3 && memcmp("adv",cmd, size) == 0)
    {
        if(!advState)
        {
            UART2_write(uart,"Turning on adv!\n\r", 17, NULL);

            BLEAppUtil_advStart(peripheralAdvHandle_1, &advSetStartParamsSet_1);
        }
        else
        {
            UART2_write(uart,"Turning off adv!\n\r", 18, NULL);

            BLEAppUtil_advStop(peripheralAdvHandle_1);

        }
    }

    BLEAppUtil_free(buffer);
}
