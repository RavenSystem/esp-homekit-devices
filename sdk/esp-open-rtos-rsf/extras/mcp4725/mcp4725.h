/*
 * Driver for 12-bit DAC MCP4725
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#ifndef _EXTRAS_MCP4725_H_
#define _EXTRAS_MCP4725_H_

#include <stdint.h>
#include <stdbool.h>
#include <i2c/i2c.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MCP4725A0_ADDR0 0x60
#define MCP4725A0_ADDR1 0x61
#define MCP4725A1_ADDR0 0x62
#define MCP4725A1_ADDR1 0x63
#define MCP4725A2_ADDR0 0x64
#define MCP4725A2_ADDR1 0x65

#define MCP4725_MAX_VALUE 0x0fff

/**
 * Power mode, see datasheet
 */
typedef enum
{
    MCP4725_PM_NORMAL = 0,   //!< Normal mode
    MCP4725_PM_PD_1K,        //!< Power down, 1kOhm resistor to ground
    MCP4725_PM_PD_100K,      //!< Power down, 100kOhm resistor to ground
    MCP4725_PM_PD_500K,      //!< Power down, 500kOhm resistor to ground
} mcp4725_power_mode_t;

/**
 * Get device EEPROM status
 * @param addr Device address
 * @return true when EEPROM is busy
 */
bool mcp4725_eeprom_busy(i2c_dev_t *dev);

/**
 * Get power mode
 * @param addr Device address
 * @param eeprom Read power mode from EEPROM if true
 * @return Power mode
 */
mcp4725_power_mode_t mcp4725_get_power_mode(i2c_dev_t *dev, bool eeprom);

/**
 * Set power mode
 * @param addr Device address
 * @param mode Power mode
 * @param eeprom Store mode to device EEPROM if true
 */
void mcp4725_set_power_mode(i2c_dev_t *dev, mcp4725_power_mode_t mode, bool eeprom);

/**
 * Get current DAC value
 * @param addr Device address
 * @param eeprom Read value from device EEPROM if true
 * @return Raw output value, 0..4095
 */
uint16_t mcp4725_get_raw_output(i2c_dev_t *dev, bool eeprom);

/**
 * Set DAC output value
 * @param addr Device address
 * @param value Raw output value, 0..4095
 * @param eeprom Store value to device EEPROM if true
 */
void mcp4725_set_raw_output(i2c_dev_t *dev, uint16_t value, bool eeprom);

/**
 * Get current DAC output voltage
 * @param addr Device address
 * @param vdd Device operating voltage, volts
 * @param eeprom Read voltage from device EEPROM if true
 * @return Current output voltage, volts
 */
inline float mcp4725_get_voltage(i2c_dev_t *dev, float vdd, bool eeprom)
{
    return vdd / MCP4725_MAX_VALUE * mcp4725_get_raw_output(dev, eeprom);
}

/**
 * Set DAC output voltage
 * @param addr Device address
 * @param vdd Device operating voltage, volts
 * @param value Output value, volts
 * @param eeprom Store value to device EEPROM if true
 */
inline void mcp4725_set_voltage(i2c_dev_t *dev, float vdd, float value, bool eeprom)
{
    mcp4725_set_raw_output(dev, MCP4725_MAX_VALUE / vdd * value, eeprom);
}

#ifdef __cplusplus
}
#endif

#endif /* _EXTRAS_MCP4725_H_ */
