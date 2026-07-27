/******************************************************************************

@file  calendar.c

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

#include <stdio.h>
#include <stdbool.h>
#include "ti_drivers_config.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "calendar.h"
#include "ti/ble/stack_util/bcomdef.h"

uint64_t RTC_cnt_524M;
uint64_t RTC_cnt_8U;

/*********************************************************************
 * GLOBAL VARIABLES
 *
 * RTC Hardware Counter Values (read from hardware registers):
 * - RTC_cnt_8U: 8µs resolution counter (TIME8U register)
 * - RTC_cnt_524M: 524ms resolution counter (TIME524M register)
 *
 * Timestamp Tracking:
 * - BaseTimeStamp: Reference time when RTC was started (Jan 1, 2021)
 * - CurrentTimeStamp: Current time calculated from RTC + BaseTimeStamp
 * - LastEventTimeStamp: Time of last recorded event
 *********************************************************************/
Timestamp BaseTimeStamp = {2021, 1, 1, 0, 0, 0};
Timestamp CurrentTimeStamp= {0, 0, 0, 0, 0, 0};
Timestamp LastEventTimeStamp= {0, 0, 0, 0, 0, 0};

/*********************************************************************
 * @fn      is_leap_year
 *
 * @brief   Check if a given year is a leap year
 *
 *          Leap year rules (Gregorian calendar):
 *          1. Year divisible by 4 is leap year (e.g., 2024)
 *          2. EXCEPT if divisible by 100 (e.g., 1900 not leap year)
 *          3. EXCEPT if divisible by 400 (e.g., 2000 IS leap year)
 *
 * @param   year - Year to check (e.g., 2024)
 *
 * @return  1 if leap year, 0 if not
 *********************************************************************/
int is_leap_year(int year)
{
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

/*********************************************************************
 * @fn      days_in_month
 *
 * @brief   Return the number of days in a given month
 *
 *          Accounts for leap years (February has 29 days in leap years)
 *
 * @param   year - Year (needed to determine if February has 28 or 29 days)
 * @param   month - Month (1-12, where 1=January, 12=December)
 *
 * @return  Number of days in the month (28-31)
 *********************************************************************/
int days_in_month(int year, int month) {
    if (month == 2) {
        return is_leap_year(year) ? 29 : 28;
    }
    const int days_in_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    return days_in_month[month - 1];
}

/*********************************************************************
 * @fn      apply_offset
 *
 * @brief   Add offset seconds to a timestamp (in-place modification)
 *
 *          This function correctly handles:
 *          - Second/minute/hour overflow (carries propagate upward)
 *          - Day overflow (accounts for varying month lengths)
 *          - Month overflow (wraps to next year)
 *          - Leap years (via days_in_month function)
 *
 *          Algorithm:
 *          1. Add all offset to seconds field
 *          2. Propagate carries: seconds→minutes→hours→days
 *          3. Handle day overflow with month-aware logic
 *          4. Handle month overflow (12→1, increment year)
 *
 * @param   ts - Pointer to timestamp to modify (modified in-place)
 * @param   offset_seconds - Number of seconds to add (can be large value)
 *
 * @return  None (modifies ts in-place)
 *
 * @note    This function modifies the timestamp in-place.
 *          No return value - the input ts is updated directly.
 *********************************************************************/
void apply_offset(Timestamp *ts,long int offset_seconds )
{
       // Step 1: Add all offset to seconds and propagate carries upward
       ts->second += offset_seconds;
       ts->minute += ts->second / 60;
       ts->second = ts->second % 60;

       ts->hour += ts->minute / 60;
       ts->minute = ts->minute % 60;

       ts->day += ts->hour / 24;
       ts->hour = ts->hour % 24;

       // Step 2: Handle day overflow (month boundaries are variable)
       while (ts->day > days_in_month(ts->year, ts->month)) {
           ts->day -= days_in_month(ts->year, ts->month);
           ts->month++;
           if (ts->month > 12) {
               ts->month = 1;
               ts->year++;
           }
       }

}

/*********************************************************************
 * @fn      modify_timestamp
 *
 * @brief   Update CurrentTimeStamp by reading RTC hardware counters
 *
 *          The CC23xx RTC has two hardware counters with different resolutions:
 *          - TIME8U: 8 microsecond resolution (fast counter)
 *          - TIME524M: 524 millisecond resolution (slow counter)
 *
 *          These counters must be read atomically to prevent glitches
 *          when the slow counter increments during a read.
 *
 *          Synchronization Algorithm:
 *          1. Read both counters
 *          2. Check if upper 16 bits of TIME8U match lower 16 bits of TIME524M
 *          3. If mismatch: re-read (counter incremented during read)
 *          4. If match: counters are consistent, calculate elapsed time
 *
 *          Time Calculation:
 *          - offset_ms = (TIME524M × 524ms) + (TIME8U × 8µs)
 *          - offset_seconds = offset_ms / 1000
 *          - CurrentTimeStamp = BaseTimeStamp + offset_seconds
 *
 * @param   None
 *
 * @return  None (updates global CurrentTimeStamp)
 *
 * @note    The while loop ensures atomic reading of the two RTC counters
 *          by detecting and retrying if a counter increment occurs mid-read.
 *********************************************************************/
void modify_timestamp()
{

    long int offset_seconds;
    // Step 1: Read both RTC counters
    RTC_cnt_8U = HWREG(RTC_BASE + RTC_O_TIME8U) & 0xFFFFFFFF;
    RTC_cnt_524M = HWREG(RTC_BASE + RTC_O_TIME524M) & 0xFFFFFFFF;

    // Step 2: Verify atomic read (upper 16 bits of TIME8U must match lower 16 bits of TIME524M)
    // If mismatch, a counter increment occurred mid-read, so retry
    while ((RTC_cnt_524M & 0xFFFF) != ((RTC_cnt_8U >> 16) & 0xFFFF))
    {
       RTC_cnt_8U = HWREG(RTC_BASE + RTC_O_TIME8U);
       RTC_cnt_524M = HWREG(RTC_BASE + RTC_O_TIME524M);
    }

    // Step 3: Calculate elapsed time since BaseTimeStamp
    // Formula: (524ms × counter524M) + (8µs × lower 16 bits of counter8U)
    offset_seconds = (RTC_cnt_524M * 524) + (RTC_cnt_8U & 0xFFFF)*8/1000;
    offset_seconds = (uint32_t)offset_seconds/1000;

    // Step 4: Apply offset to base time to get current time
    Timestamp ts;
    ts = BaseTimeStamp;
    apply_offset(&ts,offset_seconds);

    CurrentTimeStamp=ts;

    // TODO: if needed after certain time change the base time stamp to current time and reset RTC

}

/*********************************************************************
 * @fn      Calendar_setTime
 *
 * @brief   Set current time from host time sync (used by backlighting)
 *          Preserves current date, only updates time.
 *          Resets RTC counter to restart from new base time.
 *
 * @param   hours - hour (0-23)
 * @param   minutes - minute (0-59)
 *
 * @return  None
 *
 * @note    This function is called when the host sends SET_TIME_CMD
 *          via the backlighting service for scheduled mode operation.
 *          The RTC counter is reset to maintain accurate time tracking.
 */
void Calendar_setTime(uint8_t hours, uint8_t minutes)
{
    // Step 1: Get current date from RTC
    // This ensures we preserve the current date (year/month/day)
    modify_timestamp();

    // Step 2: Update base timestamp with new time
    // Keep current date components
    BaseTimeStamp.year = CurrentTimeStamp.year;
    BaseTimeStamp.month = CurrentTimeStamp.month;
    BaseTimeStamp.day = CurrentTimeStamp.day;

    // Update time components with host-provided values
    BaseTimeStamp.hour = hours;
    BaseTimeStamp.minute = minutes;
    BaseTimeStamp.second = 0;  // Reset seconds for clean sync

    // Step 3: Reset RTC counter to zero
    // This allows the RTC to start counting from the new base time
    HWREG(RTC_BASE + RTC_O_CTL) |= 0x00000001U;  // Set reset bit

    // Step 4: Update current timestamp to reflect the sync
    CurrentTimeStamp = BaseTimeStamp;
}

/*********************************************************************
 * @fn      Calendar_getHour
 *
 * @brief   Get current hour from RTC for time-based logic
 *          (e.g., backlighting scheduled mode)
 *
 * @return  Current hour (0-23)
 *
 * @note    This function updates the current timestamp from RTC before
 *          returning the hour value to ensure accuracy.
 */
uint8_t Calendar_getHour(void)
{
    // Update current timestamp from RTC hardware counter
    modify_timestamp();

    // Return hour component (cast to uint8_t for consistency)
    return (uint8_t)CurrentTimeStamp.hour;
}

/*********************************************************************
 * @fn      Calendar_isNighttime
 *
 * @brief   Check if current time is within nighttime hours
 *          Used by backlighting scheduled mode to determine if
 *          backlight should be active.
 *
 * @return  true if nighttime (18:00-08:00), false if daytime
 *
 * @note    Nighttime span:
 *          - Starts: 18:00 (6:00 PM)
 *          - Ends:   08:00 (8:00 AM)
 *          This range wraps around midnight, so the logic checks:
 *          hour >= 18 OR hour < 8
 *
 *          Per Google ATV spec: Scheduled backlight mode only
 *          activates during these hours to save battery.
 */
bool Calendar_isNighttime(void)
{
    // Get current hour (0-23)
    uint8_t hour = Calendar_getHour();

    // Nighttime logic: 18:00-08:00 (wraps around midnight)
    // True if: [18:00-23:59] OR [00:00-07:59]
    return (hour >= 18 || hour < 8);
}