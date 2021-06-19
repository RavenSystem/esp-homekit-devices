/*
 * Softuart example
 *
 * Copyright (C) 2017 Ruslan V. Uss <unclerus@gmail.com>
 * Copyright (C) 2016 Bernhard Guillon <Bernhard.Guillon@web.de>
 * Copyright (c) 2015 plieningerweb
 *
 * MIT Licensed as described in the file LICENSE
 */
#include <esp/gpio.h>
#include <esp/uart.h>
#include <espressif/esp_common.h>
#include <stdio.h>
//#include <FreeRTOS.h>
//#include <task.h>

#include <softuart/softuart.h>

#define RX_PIN 5
#define TX_PIN 4

void user_init(void)
{
    // setup real UART for now
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n\n", sdk_system_get_sdk_version());

    // setup software uart to 9600 8n1
    softuart_open(0, 9600, RX_PIN, TX_PIN);

    while (true)
    {
        if (!softuart_available(0))
            continue;

        char c = softuart_read(0);
        printf("input: %c, 0x%02x\n", c, c);
        softuart_puts(0, "start\r\n");
    }
}
