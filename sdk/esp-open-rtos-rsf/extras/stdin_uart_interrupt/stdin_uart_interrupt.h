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

#ifndef __STDIN_UART_INTERRUPT_H__
#define __STDIN_UART_INTERRUPT_H__

#include "FreeRTOS.h"
#include <stdint.h>

/* Return number of characters waiting in UART0. */
uint32_t uart0_num_char(void);

/* Set UART0 input to nonblocking or blocking, returning the old state. */
int uart0_set_nonblock(int);

/* Set the UART0 input wait time in ticks, or zero to wait indefinitely,
 * returning the old wait time. The wait time is only used when the input is
 * blocking.
 */
TickType_t uart0_set_vtime(TickType_t ticks);

#endif
