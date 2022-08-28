/*
 * Compatibility routines for the Espressif SDK and its API
 *
 * Part of esp-open-rtos
 * Copyright (C) 2015 Superhouse Automation Pty Ltd
 * BSD Licensed as described in the file LICENSE
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <common_macros.h>

#include <esp/uart.h>

void IRAM *zalloc(size_t nbytes)
{
    return calloc(1, nbytes);
}

/* UART RX function from Espressif SDK internals.
 *
 * Not part of published API.
 */
int sdk_uart_rx_one_char(char *buf) {
    int c = uart_getc_nowait(0);
    if(c < 0)
        return 1;
    *buf = (char)c;
    return 0;
}
