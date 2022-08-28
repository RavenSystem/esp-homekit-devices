/**
 * Driver for 16-channel, 12-bit PWM PCA9685
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#ifndef _EXTRAS_PCA9685_H_
#define _EXTRAS_PCA9685_H_

#include <stdint.h>
#include <stdbool.h>
#include <i2c/i2c.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PCA9685_ADDR_BASE 0x40

/**
 * Init device
 * @param addr Device address
 */
void pca9685_init(i2c_dev_t *dev);

/**
 * Setup device subaddress (see section 7.3.6 if the datasheet)
 * @param addr Device address
 * @param num Subaddress number, 0..2
 * @param subaddr Subaddress, 7 bit
 * @param enable True to enable subaddress, false to disable
 * @return False if error occured
 */
bool pca9685_set_subaddr(i2c_dev_t *dev, uint8_t num, uint8_t subaddr, bool enable);

/**
 * Restart device (see section 7.3.1.1 of the datasheet)
 * @param addr Device address
 */
void pca9685_restart(i2c_dev_t *dev);

/**
 * Check if device is in sleep mode
 * @param addr Device address
 * @return True if device is sleeping
 */
bool pca9685_is_sleeping(i2c_dev_t *dev);

/**
 * Switch device to low-power mode or wake it up.
 * @param addr Device address
 * @param sleep True for sleep mode, false for wake up
 */
void pca9685_sleep(i2c_dev_t *dev, bool sleep);

/**
 * Get logic inversion of the outputs
 * @param addr Device address
 * @return True if outputs are inverted, false otherwise
 */
bool pca9685_is_output_inverted(i2c_dev_t *dev);

/**
 * Logically invert outputs (see section 7.7 of the datasheet)
 * @param addr Device address
 * @param inverted True for inverted outputs
 */
void pca9685_set_output_inverted(i2c_dev_t *dev, bool inverted);

/**
 * Get outputs mode
 * @param addr Device address
 * @return True if outputs are in open drain mode
 */
bool pca9685_get_output_open_drain(i2c_dev_t *dev);

/**
 * Set outputs mode
 * @param addr Device address
 * @param open_drain True to set open drain mode, false to normal mode
 */
void pca9685_set_output_open_drain(i2c_dev_t *dev, bool open_drain);

/**
 * Get current PWM frequency prescaler.
 * @param addr Device address
 * @return Frequency prescaler
 */
uint8_t pca9685_get_prescaler(i2c_dev_t *dev);

/**
 * Set PWM frequency prescaler.
 * @param addr Device address
 * @param prescaler Prescaler value
 * @return False if error occured
 */
bool pca9685_set_prescaler(i2c_dev_t *dev, uint8_t prescaler);

/**
 * Get current PWM frequency
 * @param addr Device address
 * @return PWM frequency, Hz
 */
uint16_t pca9685_get_pwm_frequency(i2c_dev_t *dev);

/**
 * Set PWM frequency
 * @param addr Device address
 * @param freq PWM frequency, Hz
 * @return False if error occured
 */
bool pca9685_set_pwm_frequency(i2c_dev_t *dev, uint16_t freq);

/**
 * Set PWM value on output channel
 * @param addr Device address
 * @param channel Channel number, 0..15 or >15 for all channels
 * @param val PWM value, 0..4096
 */
void pca9685_set_pwm_value(i2c_dev_t *dev, uint8_t channel, uint16_t val);

/**
 * Set PWM values on output channels
 * @param addr Device address
 * @param first_ch First channel, 0..15
 * @param channels Number of updating channels
 * @param values Array of the channel values, each 0..4096
 * @return False if error occured
 */
bool pca9685_set_pwm_values(i2c_dev_t *dev, uint8_t first_ch, uint8_t channels, const uint16_t *values);

#ifdef __cplusplus
}
#endif

#endif /* _EXTRAS_PCA9685_H_ */
