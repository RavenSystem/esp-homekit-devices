/*
 * Tests of functionality within extras/timekeeping
 */

/*-
 * Copyright (c) 2018, Jeff Kletsky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <esp8266.h>
#include <esp/uart.h>

#include <FreeRTOS.h>
#include <task.h>

/* espressif/esp_system.h uses "bool" but fails to #include */
#include <stdbool.h>
#include <espressif/esp_common.h>
#include <espressif/esp_system.h>
#include <espressif/esp_wifi.h>

/* Not defined in espressif/esp_wifi.h */
extern bool
sdk_wifi_set_opmode_current(uint8_t opmode);

/*  For sdk_system_restart_in_nmi() */
#include <esplibs/libmain.h>

#include <stdio.h>
#include <stdlib.h>


#define BLUE_LED 2
#define LED_ON 0
#define LED_OFF 1

#define WAIT_FOR_UART_SECS 5  /* 2 might be OK */

#define TV2LD(TV) ((long double)TV.tv_sec + (long double)TV.tv_usec * 1.e-6)

void
dump_tzglobals(void) {

    /*
     * Defined in     newlib-xtensa/newlib/libc/include/time.h
     * implemented in newlib-xtensa/newlib/libc/time/tzvars.c
     */

    printf("_timezone  = %li\n", _timezone);
    printf("_daylight  = %i\n",  _daylight);
    printf("_tzname[0] = %s\n",  _tzname[0]);
    printf("_tzname[1] = %s\n",  _tzname[1]);
}

void
dump_tzinfo(void) {

    __tzinfo_type *tzinfo;
    __tzrule_type tzrule;

    tzinfo = __gettzinfo();
    printf("{%i, %i,\n", tzinfo->__tznorth, tzinfo->__tzyear);
    tzrule = tzinfo->__tzrule[0];
    printf("{%c, %i, %i, %i, %i, %li, %li},\n",
           tzrule.ch, tzrule.m, tzrule.n, tzrule.d, tzrule.s, (long)tzrule.change, (long)tzrule.offset);
    tzrule = tzinfo->__tzrule[1];
    printf("{%c, %i, %i, %i, %i, %li, %li}}\n",
           tzrule.ch, tzrule.m, tzrule.n, tzrule.d, tzrule.s, (long)tzrule.change, (long)tzrule.offset);
}

void
dump_both(void) {
    dump_tzglobals();
    dump_tzinfo();
}


void
testTask(void *pvParameters)
{
    char *env_value;
    struct timeval tv;
    time_t time_secs;
    long num_runs;
    long idx;
    TickType_t base_ticks;
    TickType_t increment_ticks;
    int seconds_per_report;
    int retval;
    uint32_t t0;
    uint32_t t1;

    /* These variables for calculation-speed tests */
    int64_t internal_clock;
    int64_t system_plus_offset;
    int64_t slew_start_time;
    int64_t seemingly_useful = 0;
#define ADJTIME_SLEW_PERIOD 2000
#define ADJTIME_SLEW_RATE 500.e-6


    gpio_enable(BLUE_LED, GPIO_OUTPUT);
    gpio_write(BLUE_LED, LED_OFF);

    vTaskDelay(WAIT_FOR_UART_SECS * 1000 / portTICK_PERIOD_MS);

    gpio_write(BLUE_LED, LED_ON);

    printf("\n");
    printf("===> setenv/getenv test; expect TEST ='test value'\n");

    setenv("TEST", "test value", 1);
    env_value = getenv("TEST");
    printf("TEST = '%s'\n", env_value);

    printf("\n");
    printf("===> Dump tzinfo before change:\n");
    dump_both();


    /* "Full" POSIX TZ spec in 2018 is TZ=PST8PDT7,M3.1.0/02:00:00,M11.1.0/02:00:00 */

    printf("\n");
    printf("===> Change TZ to PST8PDT7,M3.1.0,M11.1.0\n");

    setenv("TZ", "PST8PDT7,M3.1.0,M11.1.0", 1);
    dump_both();

    printf("\n");
    printf("===> Call tzset() just in case that's the reason it's not working\n");
    tzset();
    dump_both();


    printf("\n");
    printf("===> unsetenv TZ and confirm\n");
    unsetenv("TZ");
    env_value = getenv("TZ");
    if (env_value) {
        printf("TZ = %s\n", env_value);
    } else {
        printf("TZ not found (expected)\n");
    }
    tzset();
    dump_both();
    printf("##### NOTE: tzset() only changes the globals if TZ is unset\n");

    printf("\n");
    printf("===> Change TZ to PST8PDT7,M3.1.0,M11.1.0\n");

    setenv("TZ", "PST8PDT7,M3.1.0,M11.1.0", 1);
    tzset();
    dump_both();

    printf("\n");
    printf("===> Change TZ to UTC (not terribly useful without zoneinfo\n");

    setenv("TZ", "UTC", 1);
    tzset();
    dump_both();
    printf("##### NOTE: just changes name, not any values; 'UTC0UTC0' better\n");


    printf("\n");
    printf("===> Change TZ to UTC0\n");

    setenv("TZ", "UTC0", 1);
    tzset();
    dump_both();

    printf("\n");
    printf("===> Change TZ to UTC0UTC0\n");

    setenv("TZ", "UTC0UTC0", 1);
    tzset();
    dump_both();

    printf("\n");
    printf("===> Change TZ to UTC0UTC0,0,0\n");

    setenv("TZ", "UTC0UTC0,0,0", 1);
    tzset();
    dump_both();

    printf("\n");
    printf("===> Change TZ to UTC0UTC0,J0/0,J0/0\n");

    setenv("TZ", "UTC0UTC0,J0/0,J0/0", 1);
    tzset();
    dump_both();


    printf("\n");
    printf("===> Change TZ to PST8PDT7,M3.1.0,M11.1.0\n");
    printf("===> PST: 1518754387  Thursday, February 15, 2018 8:13:07 PM GMT-08:00\n");
    setenv("TZ", "PST8PDT7,M3.1.0,M11.1.0", 1);
    tzset();
    time_secs = 1518754387;
    printf("%s", ctime(&time_secs));
    printf("===> PDT: 1526469187  Wednesday, May 16, 2018 4:13:07 AM GMT-07:00\n");
    time_secs = 1526469187;
    printf("%s", ctime(&time_secs));

    printf("\n");
    printf("===> back-to-back printf of sdk_system_get_time() then look at gettimeofday()\n");
    printf("System clock: %0.6Lf seconds\n", ((long double)sdk_system_get_time())*1e-6);
    printf("System clock: %0.6Lf seconds\n", ((long double)sdk_system_get_time())*1e-6);
    gettimeofday(&tv, NULL);
    printf("now:          %0.6Lf seconds\n", TV2LD(tv));

    setenv("TZ", "UTC0UTC0,0,0", 1);
    tzset();
    printf("UTC: %s", ctime(&tv.tv_sec));  /* ctime() includes \n */

    setenv("TZ", "PST8PDT7,M3.1.0,M11.1.0", 1);
    tzset();
    printf("PxT: %s", ctime(&tv.tv_sec));

    printf("\n");
    printf("===> settimeofday to 1518768000, Friday, February 16, 2018 12:00:00 AM GMT-08:00\n");
    tv.tv_sec = 1518768000;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    gettimeofday(&tv, NULL);
    printf("PST: %s", ctime(&tv.tv_sec));


    num_runs = 1e3;

    printf("\n===== Calculation speed tests, %ld runs each, results in us =====\n", num_runs);

    system_plus_offset = 1518768000;
    slew_start_time = system_plus_offset;

    t0 = sdk_system_get_time();
    for (idx = 0; idx < num_runs; idx++) {
        internal_clock =
                (int64_t)(system_plus_offset
                           + (system_plus_offset
                              - slew_start_time)
                             / ADJTIME_SLEW_PERIOD);
        seemingly_useful = seemingly_useful + internal_clock;
        system_plus_offset++;  /* It's hard to keep this loop from being optimized out! */
    }
    t1 = sdk_system_get_time();
    if (seemingly_useful != 0)
        printf("internal clock with / ADJTIME_SLEW_PERIOD: %ld for %ld runs\n", (long)t1 - (long)t0, idx);

    t0 = sdk_system_get_time();
    for (idx = 0; idx < num_runs; idx++) {
        internal_clock =
                (int64_t)(system_plus_offset
                           + (double)(system_plus_offset
                              - slew_start_time)
                             * ADJTIME_SLEW_RATE);
        seemingly_useful = seemingly_useful + internal_clock;
        system_plus_offset++;
    }
    t1 = sdk_system_get_time();
    if (seemingly_useful != 0)
        printf("internal clock with (double) * ADJTIME_SLEW_RATE: %ld for %ld runs\n", (long)t1 - (long)t0, idx);


    t0 = sdk_system_get_time();
    for (idx = 0; idx < num_runs; idx++) {
        internal_clock =
                (int64_t)(system_plus_offset
                           + (int64_t)((double)(system_plus_offset
                                      - slew_start_time)
                             * ADJTIME_SLEW_RATE));
        seemingly_useful = seemingly_useful + internal_clock;
        system_plus_offset++;
    }
    t1 = sdk_system_get_time();
    if (seemingly_useful != 0)
        printf("internal clock with (unint64_t) and (double) * ADJTIME_SLEW_RATE: %ld for %ld runs\n", (long)t1 - (long)t0, idx);

    printf("\n===== time-display run =====\n");

    num_runs = 10;

    seconds_per_report = 1;

    increment_ticks = (seconds_per_report * 1000) / portTICK_PERIOD_MS;

    base_ticks = xTaskGetTickCount();

    for (idx = 0; idx <= num_runs; idx++) {
        gettimeofday(&tv, NULL);
        printf("%20.6Lf at %ld ticks\n", TV2LD(tv), (long)(xTaskGetTickCount() - base_ticks));
        vTaskDelay((base_ticks - xTaskGetTickCount()) + (increment_ticks * (idx + 1)));
    }

    printf("\n===== other tests =====\n");
    printf("===> confirm time() function works\n");
    printf("%li         from time(NULL)\n", (long)time(NULL));
    printf("%li and %li from time(&time_secs)\n", (long)time(&time_secs), (long)time_secs);

    printf("\n===> Confirm that too-large slew is rejected\n");

    tv.tv_sec = 10000;
    tv.tv_usec = 0;
    retval = adjtime(&tv, NULL);
    if (retval == 0) {
        printf("adjtime with +10000 s FAILED TO FAIL\n");
    } else {
        printf("adjtime with +10000 s properly rejected the change, errno: %d\n", errno);
    }
    tv.tv_sec = -10000;
    tv.tv_usec = 0;
    retval = adjtime(&tv, NULL);
    if (retval == 0) {
        printf("adjtime with -10000 s FAILED TO FAIL\n");
    } else {
        printf("adjtime with -10000 s properly rejected the change, errno: %d\n", errno);
    }


    printf("\n");
    printf("All done; reboot imminent\n");

    gpio_write(BLUE_LED, LED_OFF);

    sdk_system_restart_in_nmi();
}



void
user_init(void)
{
    uart_set_baud(0, 115200);
    sdk_wifi_set_opmode_current(NULL_MODE);  /* Temporarily disable */
    xTaskCreate(testTask, "testTask", 512, NULL, 2, NULL);
}
