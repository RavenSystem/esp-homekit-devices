/*
 * Driver for barometic pressure sensor MS5611-01BA03
 *
 * Copyright (C) 2016 Bernhard Guillon <Bernhard.Guillon@begu.org>
 *
 * Loosely based on hmc5831 with:
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#ifndef EXTRAS_MS561101BA03_H_
#define EXTRAS_MS561101BA03_H_

#include <stdint.h>
#include <stdbool.h>
#include <i2c/i2c.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MS561101BA03_ADDR_CSB_HIGH    0x76
#define MS561101BA03_ADDR_CSB_LOW     0x77

/**
 * Oversampling ratio
 */
typedef enum
{
    MS561101BA03_OSR_256  = 0x00, //!< 256 samples per measurement
    MS561101BA03_OSR_512  = 0x02, //!< 512 samples per measurement
    MS561101BA03_OSR_1024 = 0x04, //!< 1024 samples per measurement
    MS561101BA03_OSR_2048 = 0x06, //!< 2048 samples per measurement
    MS561101BA03_OSR_4096 = 0x08  //!< 4096 samples per measurement
} ms561101ba03_osr_t;

/**
 * Configuration data
 */
typedef struct
{
    uint16_t sens;       //!< C1 Pressure sensitivity                             | SENS_t1
    uint16_t off;        //!< C2 Pressure offset                                  | OFF_t1
    uint16_t tcs;        //!< C3 Temperature coefficient of pressure sensitivity  | TCS
    uint16_t tco;        //!< C4 Temperature coefficient of pressuer offset       | TCO
    uint16_t t_ref;      //!< C5 Reference temperature                            | T_ref
    uint16_t tempsens;   //!< C6 Temperature coefficient of the temperature       | TEMPSENSE
} ms561101ba03_config_data_t;

/**
 * Result
 */
typedef struct
{
    int32_t pressure;       //!< Compensated pressure from 10 mbar to 1200 mbar with 0.01 mbar resolution
    int32_t temperature;    //!< Temperature from -40 C to 85 C with 0.01 C resulution
} ms561101ba03_result_t;

/**
 * Device descriptor
 */
typedef struct
{
    i2c_dev_t i2c_dev;                      //!< I2C device settings
    ms561101ba03_osr_t osr;                 //!< Oversampling setting
    ms561101ba03_config_data_t config_data; //!< Device configuration, filled upon initalize
    ms561101ba03_result_t result;           //!< Result, filled upon co
    int32_t dT;                             //!< delta temperature, filled uppon co and for internal use only
}  ms561101ba03_t;

/**
 * Initialize device and read its configuration
 * @param dev Pointer to device descriptor
 * @return true if no errors occured
 */
bool ms561101ba03_init(ms561101ba03_t *dev);

/**
 * Get sensor data with second order temperature compensation
 *
 * The result will be:
 * Compensated pressure from 10 mbar to 1200 mbar with 0.01 mbar resolution
 * dev->result.pressure
 *
 * Temperature from -40 C to 85 C with 0.01 C resulution
 * dev->result.temperature
 *
 * @param dev Pointer to device descriptor
 * @return true if no errors occured
 */
bool ms561101ba03_get_sensor_data(ms561101ba03_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* EXTRAS_MS561101BA03_H_ */
