
#include <app_main.h>
#include <ti/drivers/UART2.h>
#include <ti/bleapp/ble_app_util/inc/bleapputil_api.h>
#include "bcomdef.h"


// Externs
//extern int8 rssi;
extern UART2_Handle uart;
extern UART2_Params uartParams;
extern BLEAppUtil_AdvInit_t advSetInitParamsSet_1;
extern const BLEAppUtil_AdvStart_t advSetStartParamsSet_1;

extern uint8_t advState;

extern uint8 peripheralAdvHandle_1;
extern const BLEAppUtil_AdvStart_t advSetStartParamsSet_1;

// Function prototypes
void processUARTFrame(char *buffer);


void callbackFxn(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status);




