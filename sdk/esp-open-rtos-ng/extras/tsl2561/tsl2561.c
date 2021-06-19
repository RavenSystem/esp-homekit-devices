/*
 * Part of esp-open-rtos
 * Copyright (C) 2016 Brian Schwind (https://github.com/bschwind)
 * BSD Licensed as described in the file LICENSE
 */

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "tsl2561.h"

// Registers
#define TSL2561_REG_COMMAND 0x80
#define TSL2561_REG_CONTROL 0x00
#define TSL2561_REG_TIMING 0x01
#define TSL2561_REG_THRESHOLD_LOW_0 0x02
#define TSL2561_REG_THRESHOLD_LOW_1 0x03
#define TSL2561_REG_THRESHOLD_HIGH_0 0x04
#define TSL2561_REG_THRESHOLD_HIGH_1 0x05
#define TSL2561_REG_INTERRUPT 0x06
#define TSL2561_REG_PART_ID 0x0A
#define TSL2561_REG_CHANNEL_0_LOW 0x0C
#define TSL2561_REG_CHANNEL_0_HIGH 0x0D
#define TSL2561_REG_CHANNEL_1_LOW 0x0E
#define TSL2561_REG_CHANNEL_1_HIGH 0x0F

// TSL2561 Misc Values
#define TSL2561_ON 0x03
#define TSL2561_OFF 0x00
#define TSL2561_READ_WORD 0x20

// Integration times in milliseconds
#define TSL2561_INTEGRATION_TIME_13MS 20
#define TSL2561_INTEGRATION_TIME_101MS 110
#define TSL2561_INTEGRATION_TIME_402MS 410 // Default

// Calculation constants
#define LUX_SCALE 14
#define RATIO_SCALE 9
#define CH_SCALE 10
#define CHSCALE_TINT0 0x7517
#define CHSCALE_TINT1 0x0fe7

// Package constants
#define TSL2561_PACKAGE_CS 0x00
#define TSL2561_PACKAGE_T_FN_CL 0x01

// Constants from the TSL2561 data sheet
#define K1T 0x0040 // 0.125 * 2^RATIO_SCALE
#define B1T 0x01f2 // 0.0304 * 2^LUX_SCALE
#define M1T 0x01be // 0.0272 * 2^LUX_SCALE
#define K2T 0x0080 // 0.250 * 2^RATIO_SCALE
#define B2T 0x0214 // 0.0325 * 2^LUX_SCALE
#define M2T 0x02d1 // 0.0440 * 2^LUX_SCALE
#define K3T 0x00c0 // 0.375 * 2^RATIO_SCALE
#define B3T 0x023f // 0.0351 * 2^LUX_SCALE
#define M3T 0x037b // 0.0544 * 2^LUX_SCALE
#define K4T 0x0100 // 0.50 * 2^RATIO_SCALE
#define B4T 0x0270 // 0.0381 * 2^LUX_SCALE
#define M4T 0x03fe // 0.0624 * 2^LUX_SCALE
#define K5T 0x0138 // 0.61 * 2^RATIO_SCALE
#define B5T 0x016f // 0.0224 * 2^LUX_SCALE
#define M5T 0x01fc // 0.0310 * 2^LUX_SCALE
#define K6T 0x019a // 0.80 * 2^RATIO_SCALE
#define B6T 0x00d2 // 0.0128 * 2^LUX_SCALE
#define M6T 0x00fb // 0.0153 * 2^LUX_SCALE
#define K7T 0x029a // 1.3 * 2^RATIO_SCALE
#define B7T 0x0018 // 0.00146 * 2^LUX_SCALE
#define M7T 0x0012 // 0.00112 * 2^LUX_SCALE
#define K8T 0x029a // 1.3 * 2^RATIO_SCALE
#define B8T 0x0000 // 0.000 * 2^LUX_SCALE
#define M8T 0x0000 // 0.000 * 2^LUX_SCALE
#define K1C 0x0043 // 0.130 * 2^RATIO_SCALE
#define B1C 0x0204 // 0.0315 * 2^LUX_SCALE
#define M1C 0x01ad // 0.0262 * 2^LUX_SCALE
#define K2C 0x0085 // 0.260 * 2^RATIO_SCALE
#define B2C 0x0228 // 0.0337 * 2^LUX_SCALE
#define M2C 0x02c1 // 0.0430 * 2^LUX_SCALE
#define K3C 0x00c8 // 0.390 * 2^RATIO_SCALE
#define B3C 0x0253 // 0.0363 * 2^LUX_SCALE
#define M3C 0x0363 // 0.0529 * 2^LUX_SCALE
#define K4C 0x010a // 0.520 * 2^RATIO_SCALE
#define B4C 0x0282 // 0.0392 * 2^LUX_SCALE
#define M4C 0x03df // 0.0605 * 2^LUX_SCALE
#define K5C 0x014d // 0.65 * 2^RATIO_SCALE
#define B5C 0x0177 // 0.0229 * 2^LUX_SCALE
#define M5C 0x01dd // 0.0291 * 2^LUX_SCALE
#define K6C 0x019a // 0.80 * 2^RATIO_SCALE
#define B6C 0x0101 // 0.0157 * 2^LUX_SCALE
#define M6C 0x0127 // 0.0180 * 2^LUX_SCALE
#define K7C 0x029a // 1.3 * 2^RATIO_SCALE
#define B7C 0x0037 // 0.00338 * 2^LUX_SCALE
#define M7C 0x002b // 0.00260 * 2^LUX_SCALE
#define K8C 0x029a // 1.3 * 2^RATIO_SCALE
#define B8C 0x0000 // 0.000 * 2^LUX_SCALE
#define M8C 0x0000 // 0.000 * 2^LUX_SCALE

#ifdef TSL2561_DEBUG
#include <stdio.h>
#define debug(fmt, ...) printf("%s: " fmt "\n", "TSL2561", ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

static int write_register(i2c_dev_t *i2c_dev, uint8_t reg, uint8_t value)
{
    reg = TSL2561_REG_COMMAND | reg;
    return i2c_slave_write(i2c_dev->bus, i2c_dev->addr, &reg, &value, 1);
}

static uint8_t read_register(i2c_dev_t *i2c_dev, uint8_t reg)
{
    uint8_t data[1];
    reg = TSL2561_REG_COMMAND | reg;

    if (i2c_slave_read(i2c_dev->bus, i2c_dev->addr, &reg, data, 1))
    {
        debug("Error in tsl2561 read_register\n");
    }

    return data[0];
}

static uint16_t read_register_16(i2c_dev_t *i2c_dev, uint8_t low_register_addr)
{
    uint16_t value = 0;
    uint8_t data[2];
    low_register_addr = TSL2561_REG_COMMAND | TSL2561_READ_WORD | low_register_addr;

    if (i2c_slave_read(i2c_dev->bus, i2c_dev->addr, &low_register_addr, data, 2))
    {
        debug("Error with i2c_slave_read in read_register_16\n");
    }

    value = ((uint16_t)data[1] << 8) | (data[0]);

    return value;
}

static int enable(i2c_dev_t *i2c_dev)
{
    return write_register(i2c_dev, TSL2561_REG_CONTROL, TSL2561_ON);
}

static int disable(i2c_dev_t *i2c_dev)
{
    return write_register(i2c_dev, TSL2561_REG_CONTROL, TSL2561_OFF);
}

void tsl2561_init(tsl2561_t *device)
{
    if (enable(&device->i2c_dev))
    {
        debug("Error initializing tsl2561\n");
    }

    uint8_t control_reg = (read_register(&device->i2c_dev, TSL2561_REG_CONTROL) & TSL2561_ON);

    if (control_reg != TSL2561_ON)
    {
        debug("Error initializing tsl2561, control register wasn't set to ON\n");
    }

    // Fetch the package type
    uint8_t part_reg = read_register(&device->i2c_dev, TSL2561_REG_PART_ID);
    uint8_t package = part_reg >> 6;
    device->package_type = package;

    // Fetch the gain and integration time
    uint8_t timing_register = read_register(&device->i2c_dev, TSL2561_REG_TIMING);
    device->gain = timing_register & 0x10;
    device->integration_time = timing_register & 0x03;

    disable(&device->i2c_dev);
}

void tsl2561_set_integration_time(tsl2561_t *device, tsl2561_integration_time_t integration_time_id)
{
    enable(&device->i2c_dev);
    write_register(&device->i2c_dev, TSL2561_REG_TIMING, integration_time_id | device->gain);
    disable(&device->i2c_dev);

    device->integration_time = integration_time_id;
}

void tsl2561_set_gain(tsl2561_t *device, tsl2561_gain_t gain)
{
    enable(&device->i2c_dev);
    write_register(&device->i2c_dev, TSL2561_REG_TIMING, gain | device->integration_time);
    disable(&device->i2c_dev);

    device->gain = gain;
}

static void get_channel_data(tsl2561_t *device, uint16_t *channel0, uint16_t *channel1)
{
    enable(&device->i2c_dev);

    // Since we just enabled the chip, we need to sleep
    // for the chip's integration time so it can gather a reading
    switch (device->integration_time)
    {
        case TSL2561_INTEGRATION_13MS:
            vTaskDelay(TSL2561_INTEGRATION_TIME_13MS / portTICK_PERIOD_MS);
            break;
        case TSL2561_INTEGRATION_101MS:
            vTaskDelay(TSL2561_INTEGRATION_TIME_101MS / portTICK_PERIOD_MS);
            break;
        default:
            vTaskDelay(TSL2561_INTEGRATION_TIME_402MS / portTICK_PERIOD_MS);
            break;
    }

    *channel0 = read_register_16(&device->i2c_dev, TSL2561_REG_CHANNEL_0_LOW);
    *channel1 = read_register_16(&device->i2c_dev, TSL2561_REG_CHANNEL_1_LOW);

    disable(&device->i2c_dev);
}

bool tsl2561_read_lux(tsl2561_t *device, uint32_t *lux)
{
    bool success = true;
    uint32_t chScale;
    uint32_t channel1;
    uint32_t channel0;

    switch (device->integration_time)
    {
        case TSL2561_INTEGRATION_13MS:
            chScale = CHSCALE_TINT0;
            break;
        case TSL2561_INTEGRATION_101MS:
            chScale = CHSCALE_TINT1;
            break;
        default:
            chScale = (1 << CH_SCALE);
            break;
    }

    // Scale if gain is 1x
    if (device->gain == TSL2561_GAIN_1X)
    {
        // 16x is nominal, so if the gain is set to 1x then
        // we need to scale by 16
        chScale = chScale << 4;
    }

    uint16_t ch0;
    uint16_t ch1;
    get_channel_data(device, &ch0, &ch1);

    // Scale the channel values
    channel0 = (ch0 * chScale) >> CH_SCALE;
    channel1 = (ch1 * chScale) >> CH_SCALE;

    // Find the ratio of the channel values (channel1 / channel0)
    // Protect against divide by zero
    uint32_t ratio1 = 0;
    if (channel0 != 0)
    {
        ratio1 = (channel1 << (RATIO_SCALE+1)) / channel0;
    }

    // Round the ratio value
    uint32_t ratio = (ratio1 + 1) >> 1;

    uint32_t b;
    uint32_t m;
    switch (device->package_type)
    {
        case TSL2561_PACKAGE_CS:
            if ((ratio >= 0) && (ratio <= K1C))
            {
                b = B1C;
                m = M1C;
            }
            else if (ratio <= K2C)
            {
                b = B2C;
                m = M2C;
            }
            else if (ratio <= K3C)
            {
                b = B3C;
                m = M3C;
            }
            else if (ratio <= K4C)
            {
                b = B4C;
                m = M4C;
            }
            else if (ratio <= K5C)
            {
                b = B5C;
                m = M5C;
            }
            else if (ratio <= K6C)
            {
                b = B6C;
                m = M6C;
            }
            else if (ratio <= K7C)
            {
                b = B7C;
                m = M7C;
            }
            else if (ratio > K8C)
            {
                b = B8C;
                m = M8C;
            }

            break;
        case TSL2561_PACKAGE_T_FN_CL:
            if ((ratio >= 0) && (ratio <= K1T))
            {
                b = B1T;
                m = M1T;
            }
            else if (ratio <= K2T)
            {
                b = B2T;
                m = M2T;
            }
            else if (ratio <= K3T)
            {
                b = B3T;
                m = M3T;
            }
            else if (ratio <= K4T)
            {
                b = B4T;
                m = M4T;
            }
            else if (ratio <= K5T)
            {
                b = B5T;
                m = M5T;
            }
            else if (ratio <= K6T)
            {
                b = B6T;
                m = M6T;
            }
            else if (ratio <= K7T)
            {
                b = B7T;
                m = M7T;
            }
            else if (ratio > K8T)
            {
                b = B8T;
                m = M8T;
            }

            break;
        default:
            debug("Invalid package type in CalculateLux\n");
            b = 0;
            m = 0;
            success = false;
            break;
    }

    int32_t temp;
    temp = ((channel0 * b) - (channel1 * m));

    // Do not allow negative lux value
    if (temp < 0)
    {
        temp = 0;
    }

    // Round lsb (2^(LUX_SCALE−1))
    temp += (1 << (LUX_SCALE - 1));

    // Strip off fractional portion
    *lux = temp >> LUX_SCALE;

    return success;
}
