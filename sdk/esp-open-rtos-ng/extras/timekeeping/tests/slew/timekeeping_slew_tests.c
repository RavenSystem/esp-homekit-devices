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
testTask(void *pvParameters)
{
    struct timeval tv;
    long num_runs;
    long idx;
    TickType_t base_ticks;
    TickType_t increment_ticks;
    int seconds_per_report;
    struct timeval delta;
    struct timeval olddelta;


    gpio_enable(BLUE_LED, GPIO_OUTPUT);
    gpio_write(BLUE_LED, LED_OFF);

    vTaskDelay(WAIT_FOR_UART_SECS * 1000 / portTICK_PERIOD_MS);

    gpio_write(BLUE_LED, LED_ON);

    printf("\n");
    printf("settimeofday to 1000000000\n");
    tv.tv_sec = 1000000000;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);


    printf("\n===== time-display runs =====\n");

    num_runs = 10;
    seconds_per_report = 1;

    increment_ticks = (seconds_per_report * 1000) / portTICK_PERIOD_MS;

    base_ticks = xTaskGetTickCount();

    for (idx = 0; idx <= num_runs; idx++) {
        gettimeofday(&tv, NULL);
        printf("%20.6Lf at %ld ticks\n", TV2LD(tv), (long)(xTaskGetTickCount() - base_ticks));
        vTaskDelay((base_ticks - xTaskGetTickCount()) + (increment_ticks * (idx + 1)));
    }


    num_runs = 15;
    seconds_per_report = 1;
    increment_ticks = (seconds_per_report * 1000) / portTICK_PERIOD_MS;

    delta.tv_sec = 0;
    delta.tv_usec = 5000;  /* 5 ms -- should take 10 seconds */

    printf("\n");
    printf("with +5 ms slew requested\n");
    adjtime(&delta, NULL);
    base_ticks = xTaskGetTickCount();

    for (idx = 0; idx <= num_runs; idx++) {
        gettimeofday(&tv, NULL);
        adjtime(NULL, &olddelta);
        printf("%20.6Lf at %ld ticks; %0.6Lf olddelta\n",
               TV2LD(tv), (long)(xTaskGetTickCount() - base_ticks), TV2LD(olddelta));
        vTaskDelay((base_ticks - xTaskGetTickCount()) + (increment_ticks * (idx + 1)));
    }

    delta.tv_sec = 0;
    delta.tv_usec = -5000;  /* -5 ms -- should take 10 seconds */

    printf("\n");
    printf("with -5 ms slew requested\n");
    adjtime(&delta, NULL);
    base_ticks = xTaskGetTickCount();

    for (idx = 0; idx <= num_runs; idx++) {
        gettimeofday(&tv, NULL);
        adjtime(NULL, &olddelta);
        printf("%20.6Lf at %ld ticks; %0.6Lf olddelta\n",
               TV2LD(tv), (long)(xTaskGetTickCount() - base_ticks), TV2LD(olddelta));
        vTaskDelay((base_ticks - xTaskGetTickCount()) + (increment_ticks * (idx + 1)));
    }

    num_runs = 15;
    seconds_per_report = 20;
    increment_ticks = (seconds_per_report * 1000) / portTICK_PERIOD_MS;

    delta.tv_sec = 0;
    delta.tv_usec = 130 * 1000;  /* 130 ms -- 128 ms needed for NTP */

    printf("\n");
    printf("with 130 ms slew requested (20-s reporting period)\n");
    adjtime(&delta, NULL);
    base_ticks = xTaskGetTickCount();

    for (idx = 0; idx <= num_runs; idx++) {
        gettimeofday(&tv, NULL);
        adjtime(NULL, &olddelta);
        printf("%20.6Lf at %ld ticks; %0.6Lf olddelta\n",
               TV2LD(tv), (long)(xTaskGetTickCount() - base_ticks), TV2LD(olddelta));
        vTaskDelay((base_ticks - xTaskGetTickCount()) + (increment_ticks * (idx + 1)));
    }

    delta.tv_sec = 0;
    delta.tv_usec = -130 * 1000;  /* -130 ms -- 128 ms needed for NTP */

    printf("\n");
    printf("with -130 ms slew requested\n");
    adjtime(&delta, NULL);
    base_ticks = xTaskGetTickCount();

    for (idx = 0; idx <= num_runs; idx++) {
        gettimeofday(&tv, NULL);
        adjtime(NULL, &olddelta);
        printf("%20.6Lf at %ld ticks; %0.6Lf olddelta\n",
               TV2LD(tv), (long)(xTaskGetTickCount() - base_ticks), TV2LD(olddelta));
        vTaskDelay((base_ticks - xTaskGetTickCount()) + (increment_ticks * (idx + 1)));
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
