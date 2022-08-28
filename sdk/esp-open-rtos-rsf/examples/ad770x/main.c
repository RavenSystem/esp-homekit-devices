/*
 * Example of using AD7705/AD7706 driver
 *
 * Part of esp-open-rtos
 * Copyright (C) 2017 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include <esp/uart.h>
#include <espressif/esp_common.h>
#include <stdio.h>
#include <ad770x/ad770x.h>
#include <FreeRTOS.h>
#include <task.h>

#define CS_PIN 2
#define AIN_CHANNEL 0 // AIN1+,AIN1- for AD7705

static const ad770x_params_t dev = {
    .cs_pin = CS_PIN,
    .master_clock = AD770X_MCLK_4_9152MHz, // 4.9152 MHz
    .bipolar = false,                      // Unipolar mode
    .gain = AD770X_GAIN_1,                 // No gain
    .update_rate = AD770X_RATE_50          // 50 Hz output update rate
};

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    while (ad770x_init(&dev, AIN_CHANNEL) != 0)
    {
        printf("Cannot initialize AD7705\n");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    while (true)
    {
        // wait for data
        while (!ad770x_data_ready(&dev, AIN_CHANNEL)) {}

        // Read result
        uint16_t raw = ad770x_raw_adc_value(&dev, AIN_CHANNEL);

        printf("Raw ADC value: %d\n", raw);

        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
