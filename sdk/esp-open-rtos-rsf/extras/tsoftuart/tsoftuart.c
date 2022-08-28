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

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <esp/uart.h>
#include <stdio.h>
#include <espressif/esp_system.h>
#include "FreeRTOS.h"
#include "task.h"
#include "tsoftuart/tsoftuart.h"


/*
 * The design builds a sequence of UART output transitions - the delay between
 * each toggling of the output. This sequence is then followed by a timer and
 * the timer handler toggles the state and resets the timer for the next
 * transition until done. This design avoids the code having to spin to
 * implement delays, and avoid having to disable interrupts for more reliable
 * timing.
 *
 * The MAC timer interrupt is used here as it has a high priority which helps
 * keep accurate output transition times. The MAC timer interrupt is a NMI and
 * the handler needs to take care not to interact widely. The handler only
 * accesses the timer queue, which has already been initialized.
 *
 * This software UART is not completely reliable, but might suit debug output
 * or communication that has an error detection layer, and it is more reliable
 * at lower baud rates. While it can run up to 115200 baud it is not very
 * reliable at these higher rates. It is not uncommon for the MAC timer
 * handler to be delayed 20us, so at UART baud rates above 19200 errors are
 * expected. This driver attempts to measure the timing errors and this can be
 * used to help detect when timing errors have occurred.
 */

static void IRAM output_handler(void *arg)
{
    tsoftuart_t *uart = arg;
    uint32_t tail = uart->output_queue_tail;

    do {
        uint32_t state = uart->output_queue_state ^ 1;
        uint64_t current = mactime_get_count();
        gpio_write(uart->tx_pin, state);
        uart->output_queue_state = state;

        if (tail == 0) {
            // First transition.
            uart->output_start_time = current;
            uart->output_expected_time = current;
        }

        /* The difference can be negative because the delay is skipped
         * if very short, see below. */
        int32_t err = current - uart->output_expected_time;
        if (err > uart->output_queue_error_high) {
            uart->output_queue_error_high = err;
        }
        if (err < uart->output_queue_error_low) {
            uart->output_queue_error_low = err;
        }
        
        if (tail >= uart->output_queue_head) {
            // Done.
            uart->output_queue_tail = tail;
            uart->output_done = 1;
            return;
        }

        /* Offset from the start. */
        uint32_t next = uart->output_queue[tail++];
        uint64_t target = uart->output_start_time + next;
        uart->output_expected_time = target;
        /* Target an earlier time, that would not give an error if
         * actually met, to give more room for the response delay. */
        target -= 4;
        int64_t diff = target - current;
        if (diff >= 0) {
            uart->output_queue_tail = tail;
            mactime_add_pending(&uart->output_mactimer, target);
            break;
        }
    } while(1);
}

void tsoftuart_putc(tsoftuart_t *uart, uint8_t ch)
{
    uart->output_queue_state = 1;
    gpio_write(uart->tx_pin, uart->output_queue_state);

    uart->output_queue_head = 0;
    uart->output_queue_tail = 0;

    uart->output_queue_error_high = 0;
    uart->output_queue_error_low = 0;

    uart->output_done = 0;
    
    uint32_t state = 0;
    uint32_t count = 1;
    size_t head = 0;
    uint32_t cumulative = 0;
    uint32_t td = uart->td;


    for (size_t i = 0; i < 8; i++) {
        if ((ch & 1) == state) {
            /* No change */
            count++;
        } else {
            cumulative += count * td;
            uart->output_queue[head++] = (cumulative + 128) >> 8;
            state ^= 1;
            count = 1;
        }
        ch >>= 1;
    }

    if (state == 0) {
        cumulative += count * td;
        uart->output_queue[head++] = (cumulative + 128) >> 8;
        state ^= 1;
        count = 1;
    }

    uart->output_queue_head = head;

    /* Trigger the first transition in the future. */
    mactimer_arm(&uart->output_mactimer, 20);

    /* Wait until the transmittions is expected to have completed. */
    uint32_t delay = (td * 11 + 128) >> 8;
    vTaskDelay(((delay / 1000) + portTICK_PERIOD_MS) / portTICK_PERIOD_MS);

    /* Double check that it is done. There is a possibility that the timer has
     * failed to trigger, and this needed to be detected and the timer removed
     * from the pending list before retrying. */
    size_t i;
    for (i = 0; uart->output_done == 0 && i < 10; i++) {
        vTaskDelay(1);
    }

    if (uart->output_done == 0) {
        /* Remove the timer. */
        mactimer_disarm(&uart->output_mactimer);
        /* Set the output high */
        gpio_write(uart->tx_pin, 1);
    }
}

ssize_t tsoftuart_write(tsoftuart_t *uart, const void *ptr, size_t len)
{
    for(int i = 0; i < len; i++) {
        tsoftuart_putc(uart, ((char *)ptr)[i]);
    }
    return len;
}

tsoftuart_t *tsoftuart_init(uint8_t tx_pin, uint32_t baud_rate)
{
    tsoftuart_t *uart = malloc(sizeof(tsoftuart_t));

    if (uart) {
        uart->tx_pin = tx_pin;
        uart->td = 256000000 / baud_rate;
        gpio_enable(tx_pin, GPIO_OUTPUT);
        gpio_set_pullup(tx_pin, true, false);
        gpio_write(tx_pin, 1);
        mactimer_init();
        mactimer_setfn(&uart->output_mactimer, output_handler, uart);
    }

    return uart;
}

