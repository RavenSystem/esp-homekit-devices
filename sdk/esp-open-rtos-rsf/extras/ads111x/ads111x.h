/*
 * Driver for ADS1113/ADS1114/ADS1115 I2C ADC
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#ifndef _EXTRAS_ADS111X_H_
#define _EXTRAS_ADS111X_H_

#include <stdint.h>
#include <stdbool.h>
#include <i2c/i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADS111X_ADDR_GND 0x48
#define ADS111X_ADDR_VCC 0x49
#define ADS111X_ADDR_SDA 0x4a
#define ADS111X_ADDR_SCL 0x4b

#define ADS111X_MAX_VALUE 0x7fff
#define ADS101X_MAX_VALUE 0x7ff

// ADS101X overrides
#define ADS101X_DATA_RATE_128  	ADS111X_DATA_RATE_8 
#define ADS101X_DATA_RATE_250  	ADS111X_DATA_RATE_16
#define ADS101X_DATA_RATE_490  	ADS111X_DATA_RATE_32
#define ADS101X_DATA_RATE_920  	ADS111X_DATA_RATE_64
#define ADS101X_DATA_RATE_1600	ADS111X_DATA_RATE_128
#define ADS101X_DATA_RATE_2400	ADS111X_DATA_RATE_250
#define ADS101X_DATA_RATE_3300	ADS111X_DATA_RATE_475

/**
 * Gain amplifier
 */
typedef enum
{
    ADS111X_GAIN_6V144 = 0, //!< +-6.144V
    ADS111X_GAIN_4V096,     //!< +-4.096V
    ADS111X_GAIN_2V048,     //!< +-2.048V (default)
    ADS111X_GAIN_1V024,     //!< +-1.024V
    ADS111X_GAIN_0V512,     //!< +-0.512V
    ADS111X_GAIN_0V256,     //!< +-0.256V
    ADS111X_GAIN_0V256_2,   //!< +-0.256V (same as ADS111X_GAIN_0V256)
    ADS111X_GAIN_0V256_3,   //!< +-0.256V (same as ADS111X_GAIN_0V256)
} ads111x_gain_t;

/**
 * Gain values
 */
extern const float ads111x_gain_values[];

/**
 * Input multiplexer configuration (ADS1115 only)
 */
typedef enum
{
    ADS111X_MUX_0_1 = 0, //!< positive = AIN0, negative = AIN1 (default)
    ADS111X_MUX_0_3,     //!< positive = AIN0, negative = AIN3
    ADS111X_MUX_1_3,     //!< positive = AIN1, negative = AIN3
    ADS111X_MUX_2_3,     //!< positive = AIN2, negative = AIN3
    ADS111X_MUX_0_GND,   //!< positive = AIN0, negative = GND
    ADS111X_MUX_1_GND,   //!< positive = AIN1, negative = GND
    ADS111X_MUX_2_GND,   //!< positive = AIN2, negative = GND
    ADS111X_MUX_3_GND,   //!< positive = AIN3, negative = GND
} ads111x_mux_t;

/**
 * Data rate
 */
typedef enum
{
    ADS111X_DATA_RATE_8 = 0, //!< 8 samples per second
    ADS111X_DATA_RATE_16,    //!< 16 samples per second
    ADS111X_DATA_RATE_32,    //!< 32 samples per second
    ADS111X_DATA_RATE_64,    //!< 64 samples per second
    ADS111X_DATA_RATE_128,   //!< 128 samples per second (default)
    ADS111X_DATA_RATE_250,   //!< 250 samples per second
    ADS111X_DATA_RATE_475,   //!< 475 samples per second
    ADS111X_DATA_RATE_860    //!< 860 samples per second
} ads111x_data_rate_t;

/**
 * Operational mode
 */
typedef enum
{
    ADS111X_MODE_CONTUNOUS = 0, //!< Continuous conversion mode
    ADS111X_MODE_SINGLE_SHOT    //!< Power-down single-shot mode (default)
} ads111x_mode_t;

/**
 * Comparator mode (ADS1114 and ADS1115 only)
 */
typedef enum
{
    ADS111X_COMP_MODE_NORMAL = 0, //!< Traditional comparator with hysteresis (default)
    ADS111X_COMP_MODE_WINDOW      //!< Window comparator
} ads111x_comp_mode_t;

/**
 * Comparator polarity (ADS1114 and ADS1115 only)
 */
typedef enum
{
    ADS111X_COMP_POLARITY_LOW = 0, //!< Active low (default)
    ADS111X_COMP_POLARITY_HIGH     //!< Active high
} ads111x_comp_polarity_t;

/**
 * Comparator latch (ADS1114 and ADS1115 only)
 */
typedef enum
{
    ADS111X_COMP_LATCH_DISABLED = 0, //!< Non-latching comparator (default)
    ADS111X_COMP_LATCH_ENABLED       //!< Latching comparator
} ads111x_comp_latch_t;

/**
 * Comparator queue
 */
typedef enum
{
    ADS111X_COMP_QUEUE_1 = 0,   //!< Assert ALERT/RDY pin after one conversion
    ADS111X_COMP_QUEUE_2,       //!< Assert ALERT/RDY pin after two conversions
    ADS111X_COMP_QUEUE_4,       //!< Assert ALERT/RDY pin after four conversions
    ADS111X_COMP_QUEUE_DISABLED //!< Disable comparator (default)
} ads111x_comp_queue_t;

/**
 * Get device operational status
 * @param addr Deivce address
 * @return true when device performing conversion
 */
bool ads111x_busy(i2c_dev_t *dev);

/**
 * Begin a single conversion (when in single-shot mode)
 * @param addr Deivce address
 */
void ads111x_start_conversion(i2c_dev_t *dev);

/**
 * Read last conversion result
 * @param addr
 * @return Last conversion result
 */
int16_t ads111x_get_value(i2c_dev_t *dev);

/**
 * Read last conversion result for 101x
 * @param addr
 * @return Last conversion result
 */
int16_t ads101x_get_value(i2c_dev_t *dev);

/**
 * Read the programmable gain amplifier configuration
 * (ADS1114 and ADS1115 only).
 * @param addr Deivce address
 * @return Gain value
 */
ads111x_gain_t ads111x_get_gain(i2c_dev_t *dev);

/**
 * Configure the programmable gain amplifier (ADS1114 and ADS1115 only)
 * @param addr Deivce address
 * @param gain Gain value
 */
void ads111x_set_gain(i2c_dev_t *dev, ads111x_gain_t gain);

/**
 * Read the input multiplexer configuration (ADS1115 only)
 * @param addr Deivce address
 * @return Input multiplexer configuration
 */
ads111x_mux_t ads111x_get_input_mux(i2c_dev_t *dev);

/**
 * Configure the input multiplexer configuration (ADS1115 only)
 * @param addr Deivce address
 * @param mux Input multiplexer configuration
 */
void ads111x_set_input_mux(i2c_dev_t *dev, ads111x_mux_t mux);

/**
 * Read the device operating mode
 * @param addr Deivce address
 * @return Device operating mode
 */
ads111x_mode_t ads111x_get_mode(i2c_dev_t *dev);

/**
 * Set the device operating mode
 * @param addr Deivce address
 * @param mode Device operating mode
 */
void ads111x_set_mode(i2c_dev_t *dev, ads111x_mode_t mode);

/**
 * Read the data rate
 * @param addr Deivce address
 * @return Data rate
 */
ads111x_data_rate_t ads111x_get_data_rate(i2c_dev_t *dev);

/**
 * Configure the data rate
 * @param addr Deivce address
 * @param rate Data rate
 */
void ads111x_set_data_rate(i2c_dev_t *dev, ads111x_data_rate_t rate);

/**
 * Get comparator mode (ADS1114 and ADS1115 only)
 * @param addr Deivce address
 * @return Comparator mode
 */
ads111x_comp_mode_t ads111x_get_comp_mode(i2c_dev_t *dev);

/**
 * Set comparator mode (ADS1114 and ADS1115 only)
 * @param addr Deivce address
 * @param mode Comparator mode
 */
void ads111x_set_comp_mode(i2c_dev_t *dev, ads111x_comp_mode_t mode);

/**
 * Get polarity of the comparator output pin ALERT/RDY
 * (ADS1114 and ADS1115 only)
 * @param addr Deivce address
 * @return Comparator output pin polarity
 */
ads111x_comp_polarity_t ads111x_get_comp_polarity(i2c_dev_t *dev);

/**
 * Set polarity of the comparator output pin ALERT/RDY
 * (ADS1114 and ADS1115 only)
 * @param addr Deivce address
 * @param polarity Comparator output pin polarity
 */
void ads111x_set_comp_polarity(i2c_dev_t *dev, ads111x_comp_polarity_t polarity);

/**
 * Get comparator output latch mode, see datasheet.
 * (ADS1114 and ADS1115 only)
 * @param addr Deivce address
 * @return Comparator output latch mode
 */
ads111x_comp_latch_t ads111x_get_comp_latch(i2c_dev_t *dev);

/**
 * Set comparator output latch mode (ADS1114 and ADS1115 only)
 * @param addr Deivce address
 * @param latch Comparator output latch mode
 */
void ads111x_set_comp_latch(i2c_dev_t *dev, ads111x_comp_latch_t latch);

/**
 * Set number of the comparator conversions before pin ALERT/RDY
 * assertion, or disable comparator (ADS1114 and ADS1115 only)
 * @param addr Deivce address
 * @return Number of the comparator conversions
 */
ads111x_comp_queue_t ads111x_get_comp_queue(i2c_dev_t *dev);

/**
 * Get number of the comparator conversions before pin ALERT/RDY
 * assertion (ADS1114 and ADS1115 only)
 * @param addr Deivce address
 * @param queue Number of the comparator conversions
 */
void ads111x_set_comp_queue(i2c_dev_t *dev, ads111x_comp_queue_t queue);

/**
 * Get the lower threshold value used by comparator
 * @param addr Deivce address
 * @return Lower threshold value
 */
int16_t ads111x_get_comp_low_thresh(i2c_dev_t *dev);

/**
 * Set the lower threshold value used by comparator
 * @param addr Deivce address
 * @param thresh Lower threshold value
 */
void ads111x_set_comp_low_thresh(i2c_dev_t *dev, int16_t thresh);

/**
 * Get the upper threshold value used by comparator
 * @param addr Deivce address
 * @return Upper threshold value
 */
int16_t ads111x_get_comp_high_thresh(i2c_dev_t *dev);

/**
 * Set the upper threshold value used by comparator
 * @param addr Deivce address
 * @param thresh Upper threshold value
 */
void ads111x_set_comp_high_thresh(i2c_dev_t *dev, int16_t thresh);

#ifdef __cplusplus
}
#endif

#endif /* _EXTRAS_ADS111X_H_ */
