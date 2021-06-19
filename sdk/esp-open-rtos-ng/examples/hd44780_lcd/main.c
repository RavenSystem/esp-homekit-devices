/*
 * Example of using HD44780 driver with LCD
 * connected directly to GPIO pins
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include <esp/uart.h>
#include <espressif/esp_common.h>
#include <stdio.h>

#include <hd44780/hd44780.h>

static const uint8_t char_data[] = {
    0x04, 0x0e, 0x0e, 0x0e, 0x1f, 0x00, 0x04, 0x00,
    0x1f, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x1f, 0x00
};

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    hd44780_t lcd = {
        .font = HD44780_FONT_5X8,
        .lines = 2,
        .pins = {
            .rs = 5,
            .e  = 4,
            .d4 = 0,
            .d5 = 2,
            .d6 = 14,
            .d7 = 12,
            .bl = HD44780_NOT_USED
        }
    };

    hd44780_init(&lcd);
    hd44780_upload_character(&lcd, 0, char_data);
    hd44780_upload_character(&lcd, 1, char_data + 8);

    hd44780_gotoxy(&lcd, 0, 0);
    hd44780_puts(&lcd, "\x08 Hello world!");
    hd44780_gotoxy(&lcd, 0, 1);
    hd44780_puts(&lcd, "\x09 ");

    char time[16];

    while (true)
    {
        hd44780_gotoxy(&lcd, 2, 1);

        snprintf(time, 7, "%u     ", sdk_system_get_time() / 1000000);
        time[sizeof(time) - 1] = 0;

        hd44780_puts(&lcd, time);

        for (uint32_t i = 0; i < 1000; i++)
            sdk_os_delay_us(1000);
    }
}
