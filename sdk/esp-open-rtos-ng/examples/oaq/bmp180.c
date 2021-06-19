/*
 * Driver for the BMP180 pressure sensor.
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



static i2c_dev_t bmp180_dev = {
    .addr = BMP180_DEVICE_ADDRESS,
    .bus = I2C_BUS,
};

static bool bmp180_available = false;
static uint32_t bmp180_counter = 0;
static int32_t bmp180_temperature = 0;
static uint32_t bmp180_pressure = 0;

bool bmp180_temp_press(uint32_t *counter, float *temp, float *press)
{
    if (!bmp180_available)
        return false;

    xSemaphoreTake(i2c_sem, portMAX_DELAY);
    *counter = bmp180_counter;
    *temp = (float)bmp180_temperature/10.0;
    *press = (float)bmp180_pressure;
    xSemaphoreGive(i2c_sem);

    return true;
}

static void bmp180_read_task(void *pvParameters)
{
    /* Delta encoding state. */
    uint32_t last_segment = 0;
    uint32_t last_bmp180_temp = 0;
    uint32_t last_bmp180_pressure = 0;

    xSemaphoreTake(i2c_sem, portMAX_DELAY);
    bmp180_constants_t constants;
    bool available = bmp180_is_available(&bmp180_dev) &&
        bmp180_fillInternalConstants(&bmp180_dev, &constants);
    xSemaphoreGive(i2c_sem);

    if (!available)
        vTaskDelete(NULL);

    for (;;) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);

        xSemaphoreTake(i2c_sem, portMAX_DELAY);

        int32_t temperature;
        uint32_t pressure;
        if (!bmp180_measure(&bmp180_dev, &constants, &temperature, &pressure, 3)) {
            xSemaphoreGive(i2c_sem);
            blink_red();
            continue;
        }
            
        bmp180_available = true;
        bmp180_counter = RTC.COUNTER;
        bmp180_temperature = temperature;
        bmp180_pressure = pressure;

        xSemaphoreGive(i2c_sem);

        while (1) {
            uint8_t outbuf[12];
            /* Delta encoding */
            int32_t temp_delta = (int32_t)temperature - (int32_t)last_bmp180_temp;
            uint32_t len = emit_leb128_signed(outbuf, 0, temp_delta);
            int32_t pressure_delta = (int32_t)pressure - (int32_t)last_bmp180_pressure;
            len = emit_leb128_signed(outbuf, len, pressure_delta);
            int32_t code = DBUF_EVENT_BMP180_TEMP_PRESSURE;
            uint32_t new_segment = dbuf_append(last_segment, code, outbuf, len, 1);
            if (new_segment == last_segment) {
                /*
                 * Commit the values logged. Note this is the only task
                 * accessing this state so these updates are synchronized with
                 * the last event of this class append.
                 */
                last_bmp180_temp = temperature;
                last_bmp180_pressure = pressure;
                break;
            }

            /* Moved on to a new buffer. Reset the delta encoding state and
             * retry. */
            last_segment = new_segment;
            last_bmp180_temp = 0;
            last_bmp180_pressure = 0;
        };

        blink_green();
    }
}



void init_bmp180()
{
    xTaskCreate(&bmp180_read_task, "bmp180_read_task", 240, NULL, 11, NULL);
}
