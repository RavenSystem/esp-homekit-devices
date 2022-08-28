/*
 * Software timer based UART driver.
 *
 * Copyright (C) 2018 to 2019 OurAirQuality.org
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

#ifndef _TSOFTUART_H
#define _TSOFTUART_H

#include "mactimer/mactimer.h"

typedef struct tsoftuart {
    uint32_t tx_pin;
    /* Bit time period in usec * 256 */
    uint32_t td;
    mactimer_t output_mactimer;
    uint32_t output_queue[16];
    volatile uint64_t output_start_time;
    volatile size_t output_queue_head;
    volatile size_t output_queue_tail;
    size_t output_queue_state;
    uint64_t output_expected_time;
    int32_t output_queue_error_high;
    int32_t output_queue_error_low;
    uint32_t output_done;
} tsoftuart_t;

void tsoftuart_putc(tsoftuart_t *uart, uint8_t ch);
ssize_t tsoftuart_write(tsoftuart_t *uart, const void *ptr, size_t len);
tsoftuart_t *tsoftuart_init(uint8_t tx_pin, uint32_t baud_rate);

#endif /* _TSOFTUART_H */
