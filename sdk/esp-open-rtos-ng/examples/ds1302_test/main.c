/*
 * Example of using DS1302 RTC driver
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 *                    Pavel Merzlyakov <merzlyakovpavel@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include <esp/uart.h>
#include <espressif/esp_common.h>
#include <ds1302/ds1302.h>
#include <stdio.h>

#define CE_PIN   5
#define IO_PIN   4
#define SCLK_PIN 0

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    struct tm time = {
        .tm_year = 2016,
        .tm_mon  = 9,
        .tm_mday = 31,
        .tm_hour = 21,
        .tm_min  = 54,
        .tm_sec  = 10
    };

    ds1302_init(CE_PIN, IO_PIN, SCLK_PIN);
    ds1302_set_write_protect(false);

    ds1302_set_time(&time);
    ds1302_start(true);

    while (true)
    {
        ds1302_get_time(&time);

        printf("%04d-%02d-%02d %02d:%02d:%02d\n", time.tm_year, time.tm_mon + 1,
            time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);

        for (uint32_t i = 0; i < 1000; i++)
            sdk_os_delay_us(500);
    }
}
