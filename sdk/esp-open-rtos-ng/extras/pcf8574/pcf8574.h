/**
 * \file Driver for PCF8574 compartible remote 8-bit I/O expanders for I2C-bus
 * \author Ruslan V. Uss
 */
#ifndef PCF8574_PCF8574_H_
#define PCF8574_PCF8574_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <i2c/i2c.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \brief Read GPIO port value
 * \param addr I2C register address (0b0100<A2><A1><A0> for PCF8574)
 * \return 8-bit GPIO port value
 */
uint8_t pcf8574_port_read(i2c_dev_t *dev);

/**
 * \brief Continiously read GPIO port values to buffer
 * @param addr I2C register address (0b0100<A2><A1><A0> for PCF8574)
 * @param buf Target buffer
 * @param len Buffer length
 * @return Number of bytes read
 */
size_t pcf8574_port_read_buf(i2c_dev_t *dev, void *buf, size_t len);

/**
 * \brief Write value to GPIO port
 * \param addr I2C register address (0b0100<A2><A1><A0> for PCF8574)
 * \param value GPIO port value
 */
void pcf8574_port_write(const i2c_dev_t *dev, uint8_t value);

/**
 * \brief Continiously write GPIO values to GPIO port
 * \param addr I2C register address (0b0100<A2><A1><A0> for PCF8574)
 * @param buf Buffer with values
 * @param len Buffer length
 * @return Number of bytes written
 */
size_t pcf8574_port_write_buf(const i2c_dev_t *dev, void *buf, size_t len);

/**
 * \brief Read input value of a GPIO pin
 * \param addr I2C register address (0b0100<A2><A1><A0> for PCF8574)
 * \param num pin number (0..7)
 * \return GPIO pin value
 */
bool pcf8574_gpio_read(i2c_dev_t *dev, uint8_t num);

/**
 * \brief Set GPIO pin output
 * Note this is READ - MODIFY - WRITE operation! Please read PCF8574
 * datasheet first.
 * \param addr I2C register address (0b0100<A2><A1><A0> for PCF8574)
 * \param num pin number (0..7)
 * \param value true for high level
 */
void pcf8574_gpio_write(i2c_dev_t *dev, uint8_t num, bool value);

#ifdef __cplusplus
}
#endif

#endif /* PCF8574_PCF8574_H_ */
