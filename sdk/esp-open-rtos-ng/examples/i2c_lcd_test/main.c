/*
 * Example of using driver for text LCD
 * connected to I2C by PCF8574
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include <esp/uart.h>
#include <espressif/esp_common.h>
#include <i2c/i2c.h>
#include <stdio.h>

#include <hd44780/hd44780.h>

#define I2C_BUS 0
#define SCL_PIN 5
#define SDA_PIN 4
#define ADDR 0x27

static const uint8_t char_data[] = {
    0x04, 0x0e, 0x0e, 0x0e, 0x1f, 0x00, 0x04, 0x00,
    0x1f, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x1f, 0x00
};

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    i2c_init(I2C_BUS, SCL_PIN, SDA_PIN, I2C_FREQ_100K);

    hd44780_t lcd = {
        .i2c_dev.bus = I2C_BUS,
        .i2c_dev.addr = ADDR,
        .font = HD44780_FONT_5X8,
        .lines = 2,
        .pins = {
            .rs = 0,
            .e  = 2,
            .d4 = 4,
            .d5 = 5,
            .d6 = 6,
            .d7 = 7,
            .bl = 3
        },
        .backlight = true
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
