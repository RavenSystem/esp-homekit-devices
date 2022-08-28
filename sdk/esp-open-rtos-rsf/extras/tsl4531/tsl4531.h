/*
 * Part of esp-open-rtos
 * Copyright (C) 2017 Brian Schwind (https://github.com/bschwind)
 * BSD Licensed as described in the file LICENSE
 */

#ifndef __TSL4531_H__
#define __TSL4531_H__

#include <stdint.h>
#include <stdbool.h>
#include <i2c/i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

// I2C Addresses
typedef enum
{
    TSL4531_I2C_ADDR = 0x29
} tsl4531_i2c_addr_t;

// Integration time IDs
typedef enum
{
    TSL4531_INTEGRATION_100MS = 0x02,
    TSL4531_INTEGRATION_200MS = 0x01,
    TSL4531_INTEGRATION_400MS = 0x00 // Default
} tsl4531_integration_time_t;

// Part IDs
typedef enum
{
    TSL4531_PART_TSL45317 = 0x08,
    TSL4531_PART_TSL45313 = 0x09,
    TSL4531_PART_TSL45315 = 0x0A,
    TSL4531_PART_TSL45311 = 0x0B
} tsl4531_part_id_t;

typedef struct {
    i2c_dev_t i2c_dev;
    uint8_t integration_time_id;
    bool skip_power_save;
    tsl4531_part_id_t part_id;
} tsl4531_t;

void tsl4531_init(tsl4531_t *device);
void tsl4531_set_integration_time(tsl4531_t *device, tsl4531_integration_time_t integration_time_id);
void tsl4531_set_power_save_skip(tsl4531_t *device, bool skip_power_save);
bool tsl4531_read_lux(tsl4531_t *device, uint16_t *lux);

#ifdef __cplusplus
}
#endif

#endif  // __TSL4531_H__
