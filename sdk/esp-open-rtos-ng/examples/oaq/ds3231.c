/*
 * Driver for the DS3231 real time clock.
 *
 * Copyright (C) 2016, 2017 OurAirQuality.org
 *
 * Licensed under the Apache License, Version 2.0, January 2004 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *      http://www.apache.org/licenses/
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE CONTRIBUTORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS WITH THE SOFTWARE.
 *
 */

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <esp/uart.h>
#include <stdio.h>
#include <time.h>
#include <strings.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "i2c/i2c.h"
#include "bmp180/bmp180.h"
#include "ds3231/ds3231.h"

#include <espressif/esp_misc.h>
#include "espressif/esp8266/gpio_register.h"

#include "buffer.h"
#include "i2c.h"
#include "leds.h"



i2c_dev_t ds3231_dev = {
    .addr = DS3231_ADDR,
    .bus = I2C_BUS,
};


/*
 * Handle a time in a response from a server.
 */
void ds3231_note_time(time_t recv_time)
{
    struct tm tm;

    xSemaphoreTake(i2c_sem, portMAX_DELAY);

    bool ds3231_available = ds3231_getTime(&ds3231_dev, &tm);
    if (ds3231_available) {
        time_t clock_time = mktime(&tm);
        /*
         * If the clock time is less that the server response time
         * then the clock must be slow so in that case update the
         * clock time. If the clock time is behind then this brings it
         * forward to within the minimum network delay of the server
         * time.
         *
         * If the clock time is greater than the server response time
         * then this might just be due to the delay replying, and this
         * could vary. But if it is more than four seconds ahead then
         * update the clock time.
         */
        if (clock_time < recv_time || clock_time > recv_time + 4) {
            gmtime_r(&recv_time, &tm);
            if (ds3231_setTime(&ds3231_dev, &tm)) {
                /*
                 * Log all steps in the clock time.
                 */
                uint32_t last_segment = dbuf_head_index();
                while (1) {
                    uint8_t outbuf[8];
                    outbuf[0] = clock_time;
                    outbuf[1] = clock_time >> 8;
                    outbuf[2] = clock_time >> 16;
                    outbuf[3] = clock_time >> 24;
                    outbuf[4] = recv_time;
                    outbuf[5] = recv_time >> 8;
                    outbuf[6] = recv_time >> 16;
                    outbuf[7] = recv_time >> 24;
                    int32_t code = DBUF_EVENT_DS3231_TIME_STEP;
                    uint32_t new_segment = dbuf_append(last_segment, code, outbuf, sizeof(outbuf), 1);
                    if (new_segment == last_segment)
                        break;

                    /* Moved on to a new buffer, retry. */
                    last_segment = new_segment;
                };
            }
        }
    }

    xSemaphoreGive(i2c_sem);
}

bool ds3231_available = false;
int32_t ds3231_counter = 0;
struct tm ds3231_time;
int16_t ds3231_temperature = 0;

bool ds3231_time_temp(uint32_t *counter, struct tm *time, float *temp)
{
    if (!ds3231_available)
        return false;

    xSemaphoreTake(i2c_sem, portMAX_DELAY);
    *counter = ds3231_counter;
    *time = ds3231_time;
    *temp = (float)ds3231_temperature * 0.25;
    xSemaphoreGive(i2c_sem);

    return true;
}

static void ds3231_read_task(void *pvParameters)
{
    /* Delta encoding state. */
    uint32_t last_segment = 0;
    time_t last_clock_time = 0;
    int16_t last_temperature = 0;

    xSemaphoreTake(i2c_sem, portMAX_DELAY);

    bzero(&ds3231_time, sizeof(ds3231_time));

    struct tm time;
    bool available = ds3231_getTime(&ds3231_dev, &time);

    xSemaphoreGive(i2c_sem);
    
    if (!available)
        vTaskDelete(NULL);

    for (;;) {
        vTaskDelay(180000 / portTICK_PERIOD_MS);

        xSemaphoreTake(i2c_sem, portMAX_DELAY);

        if (!ds3231_getTime(&ds3231_dev, &time)) {
            xSemaphoreGive(i2c_sem);
            blink_red();
            continue;
        }

        time_t clock_time = mktime(&time);
        int16_t temperature;
        if (!ds3231_getRawTemp(&ds3231_dev, &temperature)) {
            xSemaphoreGive(i2c_sem);
            blink_red();
            continue;
        }

        ds3231_available = true;
        ds3231_counter = RTC.COUNTER;
        ds3231_time = time;
        ds3231_temperature = temperature;

        xSemaphoreGive(i2c_sem);

        while (1) {
            uint8_t outbuf[12];
            /* Delta encoding */
            uint32_t time_delta = clock_time - last_clock_time;
            uint32_t len = emit_leb128(outbuf, 0, time_delta);
            int32_t temp_delta = (int32_t)temperature - (int32_t)last_temperature;
            len = emit_leb128_signed(outbuf, len, temp_delta);
            int32_t code = DBUF_EVENT_DS3231_TIME_TEMP;
            uint32_t new_segment = dbuf_append(last_segment, code, outbuf, len, 1);
            if (new_segment == last_segment)
                break;

            /* Moved on to a new buffer. Reset the delta encoding
             * state and retry. */
            last_segment = new_segment;
            last_clock_time = 0;
            last_temperature = 0;
        };

        blink_green();

        /*
         * Commit the values logged. Note this is the only task
         * accessing this state so these updates are synchronized
         * with the last event of this class append.
         */
        last_clock_time = clock_time;
        last_temperature = temperature;
    }
}




void init_ds3231()
{
    xTaskCreate(&ds3231_read_task, "DS3231 reader", 224, NULL, 11, NULL);
}
