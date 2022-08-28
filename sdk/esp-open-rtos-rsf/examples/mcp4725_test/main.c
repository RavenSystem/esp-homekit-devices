/*
 * Example of using MCP4725 DAC
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include <esp/uart.h>
#include <espressif/esp_common.h>
#include <i2c/i2c.h>
#include <mcp4725/mcp4725.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>

#define I2C_BUS 0
#define SCL_PIN 5
#define SDA_PIN 4
#define ADDR MCP4725A0_ADDR0
#define VDD 3.3

inline static void wait_for_eeprom(i2c_dev_t *dev)
{
    while (mcp4725_eeprom_busy(dev))
    {
        printf("...DAC is busy, waiting...\n");
        vTaskDelay(1);
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    i2c_init(I2C_BUS, SCL_PIN, SDA_PIN, I2C_FREQ_100K);
    i2c_dev_t dev = {
        .addr = ADDR,
        .bus = I2C_BUS,
    };

    // setup EEPROM values
    if (mcp4725_get_power_mode(&dev, true) != MCP4725_PM_NORMAL)
    {
        printf("DAC was sleeping... Wake up Neo!\n");
        mcp4725_set_power_mode(&dev, MCP4725_PM_NORMAL, true);
        wait_for_eeprom(&dev);
    }

    printf("Set default DAC ouptut value to MAX...\n");
    mcp4725_set_raw_output(&dev, MCP4725_MAX_VALUE, true);
    wait_for_eeprom(&dev);

    printf("And now default DAC output value is 0x%03x\n", mcp4725_get_raw_output(&dev, true));

    printf("Now let's generate the sawtooth wave in slow manner\n");

    float vout = 0;
    while (true)
    {
        vout += 0.1;
        if (vout > VDD) vout = 0;

        printf("Vout: %.02f\n", vout);
        mcp4725_set_voltage(&dev, VDD, vout, false);

        // It will be very low freq wave
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
