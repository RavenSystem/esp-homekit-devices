/*
 * Part of esp-open-rtos
 * Copyright (C) 2016 Jonathan Hartsuiker (https://github.com/jsuiker)
 * BSD Licensed as described in the file LICENSE
 *
 */

#ifndef __DHT_H__
#define __DHT_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sensor type
 */
typedef enum
{
    DHT_TYPE_DHT11 = 0, //!< DHT11
    DHT_TYPE_DHT22,     //!< DHT22
    DHT_TYPE_SI7021     //!< Itead SI7021
} dht_sensor_type_t;

/**
 * Read data from sensor on specified pin.
 *
 * Humidity and temperature is returned as integers.
 * For example: humidity=625 is 62.5 %
 *              temperature=24.4 is 24.4 degrees Celsius
 *
 */
bool dht_read_data(dht_sensor_type_t sensor_type, uint8_t pin, int16_t *humidity, int16_t *temperature);


/**
 * Float version of dht_read_data.
 *
 * Return values as floating point values.
 */
bool dht_read_float_data(dht_sensor_type_t sensor_type, uint8_t pin, float *humidity, float *temperature);

#ifdef __cplusplus
}
#endif

#endif  // __DHT_H__
