/*
 * Driver for BH1750 light sensor
 *
 * Part of esp-open-rtos
 * Copyright (C) 2017 Andrej Krutak <dev@andree.sk>
 * BSD Licensed as described in the file LICENSE
 *
 * ROHM Semiconductor bh1750fvi-e.pdf
 */
#ifndef EXTRAS_BH1750_H_
#define EXTRAS_BH1750_H_

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <i2c/i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Possible chip addresses */
#define BH1750_ADDR_LO 0x23 // ADDR pin floating/low
#define BH1750_ADDR_HI 0x5c


/* Configuration options */

// No active state
#define BH1750_POWER_DOWN 0x00

// Wating for measurement command
#define BH1750_POWER_ON 0x01

// Reset data register value - not accepted in POWER_DOWN mode
#define BH1750_RESET 0x07


/* Measurement modes */

#define BH1750_CONTINUOUS_MODE  0x10
#define BH1750_ONE_TIME_MODE  0x20

// Start measurement at 1 lx resolution (measurement time typically 120ms)
#define BH1750_HIGH_RES_MODE 0x00
// Start measurement at 0.5 lx resolution  (measurement time typically 120ms)
#define BH1750_HIGH_RES_MODE2 0x01
// Start measurement at 4 lx resolution  (measurement time typically 16ms)
#define BH1750_LOW_RES_MODE 0x03

/* Adjust measurement time to account for optical window size (see datasheet).
 * Procedure from datasheet suggests order Hi, Low and finally measurement mode
 */
#define BH1750_MEASURE_TIME_HI(mt) (0x40 | (((mt) >> 5) & 0x7))
#define BH1750_MEASURE_TIME_LO(mt) (0x60 | ((mt) & 0x1f))
#define BH1750_DEFAULT_MEASURE_TIME 0x45


/**
 * Configure the device.
 * @param addr Device address
 * @param mode Combination of BH1750_* flags
 *
 * May be called multiple times e.g. to configure the measurement time and
 * the readout mode afterwards - or if one time mode is used consecutively.
 *
 * Example: BH1750_ADDR_LO, BH1750_CONTINUOUS_MODE | BH1750_HIGH_RES_MODE
 */
void bh1750_configure(i2c_dev_t *dev, uint8_t mode);

/**
 * Read LUX value from the device.
 *
 * @param addr Device address
 * @returns read value in lux units
 */
uint16_t bh1750_read(i2c_dev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* EXTRAS_BH1750_H_ */
