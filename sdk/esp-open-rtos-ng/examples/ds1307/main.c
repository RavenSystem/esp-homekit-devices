/*
 * Example of using DS1307 RTC driver
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include <esp/uart.h>
#include <espressif/esp_common.h>
#include <i2c/i2c.h>
#include <ds1307/ds1307.h>
#include <stdio.h>

#define I2C_BUS 0
#define SCL_PIN 5
#define SDA_PIN 4

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    i2c_init(I2C_BUS, SCL_PIN, SDA_PIN, I2C_FREQ_400K);
    i2c_dev_t dev = {
        .addr = DS1307_ADDR,
        .bus = I2C_BUS,
    };
    ds1307_start(&dev, true);

    // setup datetime: 2016-10-09 13:50:10
    struct tm time = {
        .tm_year = 2016,
        .tm_mon  = 9,  // 0-based
        .tm_mday = 9,
        .tm_hour = 13,
        .tm_min  = 50,
        .tm_sec  = 10
    };
    ds1307_set_time(&dev, &time);

    while (true)
    {
        ds1307_get_time(&dev, &time);

        printf("%04d-%02d-%02d %02d:%02d:%02d\n", time.tm_year, time.tm_mon + 1,
            time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);

        for (uint32_t i = 0; i < 1000; i++)
            sdk_os_delay_us(500);
    }
}
