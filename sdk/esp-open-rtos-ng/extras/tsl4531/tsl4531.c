/*
 * Part of esp-open-rtos
 * Copyright (C) 2017 Brian Schwind (https://github.com/bschwind)
 * BSD Licensed as described in the file LICENSE
 */

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "tsl4531.h"

// Registers
#define TSL4531_REG_COMMAND 0x80
#define TSL4531_REG_CONTROL 0x00
#define TSL4531_REG_CONFIG 0x01
#define TSL4531_REG_DATA_LOW 0x04
#define TSL4531_REG_DATA_HIGH 0x05
#define TSL4531_REG_DEVICE_ID 0x0A

// TSL4531 Misc Values
#define TSL4531_ON 0x03
#define TSL4531_OFF 0x00

// Integration times in milliseconds
#define TSL4531_INTEGRATION_TIME_100MS 120
#define TSL4531_INTEGRATION_TIME_200MS 240
#define TSL4531_INTEGRATION_TIME_400MS 480 // Default

static int write_register(i2c_dev_t *i2c_dev, uint8_t reg, uint8_t value)
{
    reg = TSL4531_REG_COMMAND | reg;
    return i2c_slave_write(i2c_dev->bus, i2c_dev->addr, &reg, &value, 1);
}

static uint8_t read_register(i2c_dev_t *i2c_dev, uint8_t reg)
{
    uint8_t data[1];
    reg = TSL4531_REG_COMMAND | reg;

    if (i2c_slave_read(i2c_dev->bus, i2c_dev->addr, &reg, data, 1))
    {
        printf("Error in tsl4531 read_register\n");
    }

    return data[0];
}

static uint16_t read_register_16(i2c_dev_t *i2c_dev, uint8_t low_register_addr)
{
    uint16_t value = 0;
    uint8_t data[2];
    low_register_addr = TSL4531_REG_COMMAND | low_register_addr;

    if (i2c_slave_read(i2c_dev->bus, i2c_dev->addr, &low_register_addr, data, 2))
    {
        printf("Error with i2c_slave_read in read_register_16\n");
    }

    value = ((uint16_t)data[1] << 8) | (data[0]);

    return value;
}

static int enable(tsl4531_t *device)
{
    return write_register(&device->i2c_dev, TSL4531_REG_CONTROL, TSL4531_ON);
}

static int disable(tsl4531_t *device)
{
    return write_register(&device->i2c_dev, TSL4531_REG_CONTROL, TSL4531_OFF);
}

void tsl4531_init(tsl4531_t *device)
{
    if (enable(device))
    {
        printf("Error initializing tsl4531, the enable write failed\n");
    }

    uint8_t control_reg = read_register(&device->i2c_dev, TSL4531_REG_CONTROL);

    if (control_reg != TSL4531_ON) {
        printf("Error initializing tsl4531, control register wasn't set to ON\n");
    }

    uint8_t idRegister = read_register(&device->i2c_dev, TSL4531_REG_DEVICE_ID);
    uint8_t id = (idRegister & 0xF0) >> 4;

    if (id == TSL4531_PART_TSL45317) {
        device->part_id = TSL4531_PART_TSL45317;
    } else if (id == TSL4531_PART_TSL45313) {
        device->part_id = TSL4531_PART_TSL45313;
    } else if (id == TSL4531_PART_TSL45315) {
        device->part_id = TSL4531_PART_TSL45315;
    } else if (id == TSL4531_PART_TSL45311) {
        device->part_id = TSL4531_PART_TSL45311;
    } else {
        printf("Unknown part id for TSL4531 sensor: %u\n", id);
    }

    disable(device);
}

void tsl4531_set_integration_time(tsl4531_t *device, tsl4531_integration_time_t integration_time_id)
{
    uint8_t power_save_bit = device->skip_power_save ? 0x08 : 0x00;
    uint8_t integration_time_bits = 0x03 & integration_time_id;
    uint8_t new_config_reg = power_save_bit | integration_time_bits;

    enable(device);
    write_register(&device->i2c_dev, TSL4531_REG_CONFIG, new_config_reg);
    disable(device);

    device->integration_time_id = integration_time_id;
}

void tsl4531_set_power_save_skip(tsl4531_t *device, bool skip_power_save)
{
    uint8_t power_save_bit = skip_power_save ? 0x08 : 0x00;
    uint8_t integration_time_bits = 0x03 & device->integration_time_id;
    uint8_t new_config_reg = power_save_bit | integration_time_bits;

    enable(device);
    write_register(&device->i2c_dev, TSL4531_REG_CONFIG, new_config_reg);
    disable(device);

    device->skip_power_save = skip_power_save;
}

bool tsl4531_read_lux(tsl4531_t *device, uint16_t *lux)
{
    bool success = true;
    uint16_t multiplier = 1;

    enable(device);

    switch (device->integration_time_id)
    {
        case TSL4531_INTEGRATION_100MS:
            multiplier = 4;
            vTaskDelay(TSL4531_INTEGRATION_TIME_100MS / portTICK_PERIOD_MS);
            break;
        case TSL4531_INTEGRATION_200MS:
            multiplier = 2;
            vTaskDelay(TSL4531_INTEGRATION_TIME_200MS / portTICK_PERIOD_MS);
            break;
        case TSL4531_INTEGRATION_400MS:
            multiplier = 1;
            vTaskDelay(TSL4531_INTEGRATION_TIME_400MS / portTICK_PERIOD_MS);
            break;
        default:
            multiplier = 1;
            vTaskDelay(TSL4531_INTEGRATION_TIME_400MS / portTICK_PERIOD_MS);
            break;
    }

    uint16_t lux_data = read_register_16(&device->i2c_dev, TSL4531_REG_DATA_LOW);

    disable(device);

    *lux = multiplier * lux_data;

    return success;
}
