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
#include <stdio.h>
#include <i2c/i2c.h>
#include <ads111x/ads111x.h>
#include <FreeRTOS.h>
#include <task.h>

// Connect ADDR pin to GND
#define ADDR ADS111X_ADDR_GND

#define I2C_BUS 0
#define SCL_PIN 5
#define SDA_PIN 4

// +-4.096V
#define GAIN ADS111X_GAIN_4V096

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    i2c_dev_t dev = {
        .addr = ADDR,
        .bus = I2C_BUS,
    };
    i2c_init(I2C_BUS, SCL_PIN, SDA_PIN, I2C_FREQ_100K);

    ads111x_set_mode(&dev, ADS111X_MODE_CONTUNOUS);
    ads111x_set_data_rate(&dev, ADS111X_DATA_RATE_32);

    ads111x_set_input_mux(&dev, ADS111X_MUX_0_GND);
    ads111x_set_gain(&dev, GAIN);

    float gain_val = ads111x_gain_values[GAIN];

    while (true)
    {
        // wait for conversion end
        while (ads111x_busy(&dev)) {}

        // Read result
        int16_t raw = ads111x_get_value(&dev);

        float voltage = gain_val / ADS111X_MAX_VALUE * raw;

        printf("Raw ADC value: %d, voltage: %.04f volts\n", raw, voltage);

        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
