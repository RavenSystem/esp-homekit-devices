/*
 * Example of using PCA9685 PWM driver
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * Public domain
 */
#include <esp/uart.h>
#include <espressif/esp_common.h>
#include <i2c/i2c.h>
#include <pca9685/pca9685.h>
#include <stdio.h>

#define ADDR PCA9685_ADDR_BASE

#define I2C_BUS 0
#define SCL_PIN 5
#define SDA_PIN 4

#define PWM_FREQ 500

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    i2c_init(I2C_BUS, SCL_PIN, SDA_PIN, I2C_FREQ_100K);
    i2c_dev_t dev = {
        .addr = ADDR,
        .bus = I2C_BUS,
    };

    pca9685_init(&dev);

    pca9685_set_pwm_frequency(&dev, 1000);
    printf("Freq 1000Hz, real %d\n", pca9685_get_pwm_frequency(&dev));

    uint16_t val = 0;
    while (true)
    {
        printf("Set ch0 to %d, ch4 to %d\n", val, 4096 - val);
        pca9685_set_pwm_value(&dev, 0, val);
        pca9685_set_pwm_value(&dev, 4, 4096 - val);

        if (val++ == 4096)
            val = 0;
    }
}
