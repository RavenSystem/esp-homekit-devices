/*
 * Part of esp-open-rtos
 * Copyright (C) 2016 Brian Schwind (https://github.com/bschwind)
 * BSD Licensed as described in the file LICENSE
 */

#ifndef __TSL2561_H__
#define __TSL2561_H__

#include <stdint.h>
#include <stdbool.h>
#include <i2c/i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

// I2C Addresses
typedef enum
{
    TSL2561_I2C_ADDR_VCC = 0x49,
    TSL2561_I2C_ADDR_GND = 0x29,
    TSL2561_I2C_ADDR_FLOAT = 0x39 // Default
} tsl2561_i2c_addr_t;

// Integration time IDs
typedef enum
{
    TSL2561_INTEGRATION_13MS = 0x00,
    TSL2561_INTEGRATION_101MS = 0x01,
    TSL2561_INTEGRATION_402MS = 0x02 // Default
} tsl2561_integration_time_t;

// Gain IDs
typedef enum
{
    TSL2561_GAIN_1X = 0x00, // Default
    TSL2561_GAIN_16X = 0x10
} tsl2561_gain_t;

typedef struct {
    i2c_dev_t i2c_dev;
    uint8_t integration_time;
    uint8_t gain;
    uint8_t package_type;
} tsl2561_t;

void tsl2561_init(tsl2561_t *device);
void tsl2561_set_integration_time(tsl2561_t *device, tsl2561_integration_time_t integration_time_id);
void tsl2561_set_gain(tsl2561_t *device, tsl2561_gain_t gain);
bool tsl2561_read_lux(tsl2561_t *device, uint32_t *lux);

#ifdef __cplusplus
}
#endif

#endif  // __TSL2561_H__
