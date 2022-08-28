/*
 * Example timer based software UART drive.
 *
 * Copyright (C) 2019 OurAirQuality.org
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

#include <string.h>
#include <ctype.h>

#include <espressif/esp_common.h>
#include <espressif/user_interface.h>
#include <esp/uart.h>
#include <FreeRTOS.h>
#include <task.h>

#include "lwip/sockets.h"

#include "wificfg/wificfg.h"
#include "tsoftuart/tsoftuart.h"

static void tsoftuart_task(void *pvParameters)
{
    /* Initialize the UART Tx. */
    uint32_t tx_pin = *(uint32_t *)pvParameters;
    struct tsoftuart *uart = tsoftuart_init(tx_pin, 9600);
    
    for (;;) {
        /* Reset the timing error records. */
        uart->output_queue_error_low = 0;
        uart->output_queue_error_high = 0;

        char str[] = "Hello 0123456789 abcdefghijklmnopqrstuvwxyz\r\n";
        for (size_t i = 0; i < strlen(str); i++) {
            tsoftuart_putc(uart, str[i]);
        }

        /* Check the timing error. */
        if (uart->output_queue_error_high > 2 || uart->output_queue_error_low < -2) {
            tsoftuart_write(uart, "X\r\n", 3);
        }

        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    wificfg_init(80, NULL);

    /* Start two tasks writing to different pins. */

    static uint32_t tx_pin1 = 1;
    xTaskCreate(&tsoftuart_task, "tsoftuart1", 256, &tx_pin1, 1, NULL);

    static uint32_t tx_pin2 = 2;
    xTaskCreate(&tsoftuart_task, "tsoftuart2", 256, &tx_pin2, 1, NULL);
}
