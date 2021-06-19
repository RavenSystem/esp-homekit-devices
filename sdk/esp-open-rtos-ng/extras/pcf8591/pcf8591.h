/**
 * Driver for 8-bit analog-to-digital conversion and
 * an 8-bit digital-to-analog conversion PCF8591
 *
 * Part of esp-open-rtos
 * Copyright (C) 2017 Pham Ngoc Thanh <pnt239@gmail.com>
 *               2017 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#ifndef _EXTRAS_PCF8591_H_
#define _EXTRAS_PCF8591_H_

#include <stdint.h>
#include <i2c/i2c.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PCF8591_DEFAULT_ADDRESS 0x48

/**
 * Analog inputs configuration, see datasheet
 */
typedef enum {
    PCF8591_IC_4_SINGLES = 0,   //!< Four single-ended inputs
    PCF8591_IC_DIFF,            //!< Three differential inputs
    PCF8591_IC_2_SINGLES_DIFF,  //!< Two single-ended and differnetial mixed
    PCF8591_IC_2_DIFFS          //!< Two differential inputs
} pcf8591_input_conf_t;

/**
 * Read input value of an analog pin.
 * @param[in] dev Pointer to device
 * @param[in] conf Analog inputs configuration
 * @param[in] channel Analog channel
 * @return analog value
 */
uint8_t pcf8591_read(i2c_dev_t *dev, pcf8591_input_conf_t conf, uint8_t channel);

/**
 * Write value to analog output
 * @param[in] dev Pointer to device
 * @param[in] value DAC value
 */
void pcf8591_write(i2c_dev_t *dev, uint8_t value);


#ifdef __cplusplus
}
#endif

#endif /* _EXTRAS_PCF8591_H_ */
