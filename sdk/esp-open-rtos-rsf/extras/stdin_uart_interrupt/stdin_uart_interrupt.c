/* 
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Johan Kanflo (github.com/kanflo)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <esp8266.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <stdio.h>

#if (configUSE_COUNTING_SEMAPHORES == 0)
 #error "You need to define configUSE_COUNTING_SEMAPHORES in a local FreeRTOSConfig.h, see examples/terminal/FreeRTOSConfig.h"
#endif

// IRQ driven UART RX driver for ESP8266 written for use with esp-open-rtos
// TODO: Handle UART1

#ifndef UART0
#define UART0 (0)
#endif

#define UART0_RX_SIZE  (128) // ESP8266 UART HW FIFO size

static QueueHandle_t uart0_queue;
static bool inited = false;
static bool uart0_rx_init(void);
static int uart0_nonblock;
static TickType_t uart0_vtime = portMAX_DELAY;

uint32_t uart0_parity_errors;
uint32_t uart0_framing_errors;
uint32_t uart0_breaks_detected;

IRAM void uart0_rx_handler(void *arg)
{
    // TODO: Handle UART1, see reg 0x3ff20020, bit2, bit0 represents uart1 and uart0 respectively

    // printf(" [%08x (%d)]\n", READ_PERI_REG(UART_INT_ST(UART0)), READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S));

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    do {
        // If new data arrives and the status changes after checking here, them
        // the interrupt will be re-triggered.
        uint32_t int_status = UART(UART0).INT_STATUS;

        if (int_status & UART_INT_STATUS_RXFIFO_FULL) {
            size_t count = UART(UART0).STATUS & (UART_STATUS_RXFIFO_COUNT_M << UART_STATUS_RXFIFO_COUNT_S);
            for (size_t i = 0; i < count; i++) {
                char ch = UART(UART0).FIFO & (UART_FIFO_DATA_M << UART_FIFO_DATA_S);
                xQueueSendToBackFromISR(uart0_queue, &ch, &xHigherPriorityTaskWoken);
            }
            UART(UART0).INT_CLEAR = UART_INT_CLEAR_RXFIFO_FULL;
            // If new data has arrived then the interrupt status will remain set.
        } else if (int_status & UART_INT_STATUS_PARITY_ERR) {
            uart0_parity_errors++;
            UART(UART0).INT_CLEAR = UART_INT_CLEAR_PARITY_ERR;
        } else if (int_status & UART_INT_STATUS_FRAMING_ERR) {
            uart0_framing_errors++;
            UART(UART0).INT_CLEAR = UART_INT_CLEAR_FRAMING_ERR;
        } else if (int_status & UART_INT_STATUS_BREAK_DETECTED) {
            uart0_breaks_detected++;
            UART(UART0).INT_CLEAR = UART_INT_CLEAR_BREAK_DETECTED;
        } else if (int_status & 0xff) {
            printf("Error: unexpected uart irq, INT_STATUS 0x%02x\n", int_status);
        } else {
            break;
        }
    } while (1);

    if(xHigherPriorityTaskWoken) {
        portYIELD();
    }
}

uint32_t uart0_num_char(void)
{
    uint32_t count;
    if (!inited) uart0_rx_init();
    count = uxQueueMessagesWaiting(uart0_queue);
    return count;
}

int uart0_set_nonblock(int nonblock)
{
    int current = uart0_nonblock;
    uart0_nonblock = nonblock;
    return current;
}

TickType_t uart0_set_vtime(TickType_t ticks)
{
    TickType_t current = uart0_vtime;
    uart0_vtime = ticks;
    return current;
}

// _read_stdin_r in core/newlib_syscalls.c will be skipped by the linker in favour
// of this function
long _read_stdin_r(struct _reent *r, int fd, char *ptr, int len)
{
    TickType_t vtime = uart0_vtime;
    int nonblock = uart0_nonblock;

    if (nonblock) {
        vtime = 0;
    }

    if (!inited) uart0_rx_init();

    for(size_t i = 0; i < len; i++, ptr++) {
        if (xQueueReceive(uart0_queue, (void*)ptr, vtime) == pdFALSE) {
            if (i > 0) {
                return i;
            }
            if (nonblock) {
                r->_errno = EAGAIN;
                return -1;
            }
            return 0;
        }
    }
    return len;
}

static bool uart0_rx_init(void)
{
    uart0_queue = xQueueCreate(64, sizeof(char));

    if (!uart0_queue) {
        return false;
    }

    int trig_lvl = 1;

    _xt_isr_attach(INUM_UART, uart0_rx_handler, NULL);
    _xt_isr_unmask(1 << INUM_UART);

    // reset the rx fifo
    uint32_t conf = UART(UART0).CONF0;
    UART(UART0).CONF0 = conf | UART_CONF0_RXFIFO_RESET;
    UART(UART0).CONF0 = conf & ~UART_CONF0_RXFIFO_RESET;

    // set rx fifo trigger
    UART(UART0).CONF1 |= (trig_lvl & UART_CONF1_RXFIFO_FULL_THRESHOLD_M) << UART_CONF1_RXFIFO_FULL_THRESHOLD_S;

    // clear all interrupts
    UART(UART0).INT_CLEAR = 0x1ff;

    // enable rx_interrupt
    UART(UART0).INT_ENABLE = UART_INT_ENABLE_RXFIFO_FULL | UART_INT_ENABLE_PARITY_ERR |
        UART_INT_ENABLE_FRAMING_ERR | UART_INT_ENABLE_BREAK_DETECTED;

    inited = true;

    return true;
}
