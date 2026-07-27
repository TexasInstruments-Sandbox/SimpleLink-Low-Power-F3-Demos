/******************************************************************************

@file  calendar.h

@brief This file contains the application main functionality

Group: WCS, BTS
Target Device: cc23xx

******************************************************************************

Copyright (c) 2022-2026, Texas Instruments Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

*  Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

*  Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

*  Neither the name of Texas Instruments Incorporated nor the names of
its contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

******************************************************************************

Copyright (c) 2022-2026, Texas Instruments Incorporated
All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

******************************************************************************
*****************************************************************************/

#include <stdbool.h>

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} Timestamp;


#define CLOCK_MS 1000 //MS UNIT
#define CALENDAR_UPDATE_MS  1000
#define UART_TIMEOUT_MS  10000
#define ADV_INTERVAL_MS 1000
#define EventTimeThreshold 10 // Event Time Threshold in seconds to validate Event


#define RTC_O_CTL            0x00000004U
#define RTC_O_TIME8U         0x00000018U
#define RTC_O_TIME524M       0x0000001CU

extern Timestamp BaseTimeStamp;
extern Timestamp CurrentTimeStamp;
extern Timestamp LastEventTimeStamp;

// Time synchronization functions for backlighting
void Calendar_setTime(uint8_t hours, uint8_t minutes);
uint8_t Calendar_getHour(void);
bool Calendar_isNighttime(void);