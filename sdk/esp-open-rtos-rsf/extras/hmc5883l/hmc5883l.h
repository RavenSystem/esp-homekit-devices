/*
 * Driver for 3-axis digital compass HMC5883L
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#ifndef EXTRAS_HMC5883L_H_
#define EXTRAS_HMC5883L_H_

#include <stdint.h>
#include <stdbool.h>
#include <i2c/i2c.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define HMC5883L_ADDR 0x1e

#define HMC5883L_ID 0x00333448  // "H43"

/**
 * Device operating mode
 */
typedef enum
{
    HMC5883L_MODE_CONTINUOUS = 0, //!< Continuous mode
    HMC5883L_MODE_SINGLE          //!< Single measurement mode, default
} hmc5883l_operating_mode_t;

/**
 * Number of samples averaged per measurement
 */
typedef enum
{
    HMC5883L_SAMPLES_1 = 0, //!< 1 sample, default
    HMC5883L_SAMPLES_2,     //!< 2 samples
    HMC5883L_SAMPLES_4,     //!< 4 samples
    HMC5883L_SAMPLES_8      //!< 8 samples
} hmc5883l_samples_averaged_t;

/**
 * Data output rate in continuous measurement mode
 */
typedef enum
{
    HMC5883L_DATA_RATE_00_75 = 0, //!< 0.75 Hz
    HMC5883L_DATA_RATE_01_50,     //!< 1.5 Hz
    HMC5883L_DATA_RATE_03_00,     //!< 3 Hz
    HMC5883L_DATA_RATE_07_50,     //!< 7.5 Hz
    HMC5883L_DATA_RATE_15_00,     //!< 15 Hz, default
    HMC5883L_DATA_RATE_30_00,     //!< 30 Hz
    HMC5883L_DATA_RATE_75_00      //!< 75 Hz
} hmc5883l_data_rate_t;

/**
 * Measurement mode of the device (bias)
 */
typedef enum
{
    HMC5883L_BIAS_NORMAL = 0, //!< Default flow, no bias
    HMC5883L_BIAS_POSITIVE,   //!< Positive bias configuration all axes, used for self test (see datasheet)
    HMC5883L_BIAS_NEGATIVE    //!< Negative bias configuration all axes, used for self test (see datasheet)
} hmc5883l_bias_t;

/**
 * Device gain
 */
typedef enum
{
    HMC5883L_GAIN_1370 = 0, //!< 0.73 mG/LSb, range -0.88..+0.88 G
    HMC5883L_GAIN_1090,     //!< 0.92 mG/LSb, range -1.3..+1.3 G, default
    HMC5883L_GAIN_820,      //!< 1.22 mG/LSb, range -1.9..+1.9 G
    HMC5883L_GAIN_660,      //!< 1.52 mG/LSb, range -2.5..+2.5 G
    HMC5883L_GAIN_440,      //!< 2.27 mG/LSb, range -4.0..+4.0 G
    HMC5883L_GAIN_390,      //!< 2.56 mG/LSb, range -4.7..+4.7 G
    HMC5883L_GAIN_330,      //!< 3.03 mG/LSb, range -5.6..+5.6 G
    HMC5883L_GAIN_230       //!< 4.35 mG/LSb, range -8.1..+8.1 G
} hmc5883l_gain_t;

/**
 * Raw measurement result
 */
typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} hmc5883l_raw_data_t;

/**
 * Measurement result, milligauss
 */
typedef struct
{
    float x;
    float y;
    float z;
} hmc5883l_data_t;

/**
 * \brief Init device
 * \return false if error occured
 */
bool hmc5883l_init(i2c_dev_t *dev);

/**
 * \brief Get device ID
 * Always returns 0x00333448 if IC functioning properly.
 * \return Device ID
 */
uint32_t hmc5883l_get_id(i2c_dev_t *dev);

/**
 * \brief Get operating mode
 * \return Measurement mode
 */
hmc5883l_operating_mode_t hmc5883l_get_operating_mode(i2c_dev_t *dev);

/**
 * \brief Set operating mode
 * \param mode Measurement mode
 */
void hmc5883l_set_operating_mode(i2c_dev_t *dev, hmc5883l_operating_mode_t mode);

/**
 * \brief Get number of samples averaged per measurement output
 * \return Number of samples
 */
hmc5883l_samples_averaged_t hmc5883l_get_samples_averaged(i2c_dev_t *dev);

/**
 * \brief Set number of samples averaged per measurement output
 * \param samples Number of samples
 */
void hmc5883l_set_samples_averaged(i2c_dev_t *dev, hmc5883l_samples_averaged_t samples);

/**
 * \brief Get data output rate in continuous measurement mode
 * \return Data output rate
 */
hmc5883l_data_rate_t hmc5883l_get_data_rate(i2c_dev_t *dev);

/**
 * \brief Set data output rate in continuous measurement mode
 * \param rate Data output rate
 */
void hmc5883l_set_data_rate(i2c_dev_t *dev, hmc5883l_data_rate_t rate);

/**
 * \brief Get measurement mode (bias of the axes)
 * See datasheet for self test description
 * \return Bias
 */
hmc5883l_bias_t hmc5883l_get_bias(i2c_dev_t *dev);

/**
 * \brief Set measurement mode (bias of the axes)
 * See datasheet for self test description
 * \param bias Bias
 */
void hmc5883l_set_bias(i2c_dev_t *dev, hmc5883l_bias_t bias);

/**
 * \brief Get device gain
 * \return Current gain
 */
hmc5883l_gain_t hmc5883l_get_gain(i2c_dev_t *dev);

/**
 * \brief Set device gain
 * \param gain Gain
 */
void hmc5883l_set_gain(i2c_dev_t *dev, hmc5883l_gain_t gain);

/**
 * \brief Get data state
 * \return true when data is written to all six data registers
 */
bool hmc5883l_data_is_ready(i2c_dev_t *dev);

/**
 * \brief Get lock state.
 * If data is locked, any new data will not be placed in data registers until
 * one of these conditions are met:
 * 1. data have been read,
 * 2. operating mode is changed,
 * 3. the measurement configuration (bias) is changed,
 * 4. power is reset.
 * \return true when data registers is locked
 */
bool hmc5883l_data_is_locked(i2c_dev_t *dev);

/**
 * \brief Get raw magnetic data
 * \param data Pointer to the struct to write raw data
 * \return false if error occured in single measurement mode, always true in continuous mode
 */
bool hmc5883l_get_raw_data(i2c_dev_t *dev, hmc5883l_raw_data_t *data);

/**
 * \brief Convert raw magnetic data to milligausses
 * \param raw Pointer to source raw data struct
 * \param mg Pointer to target struct to write converted data
 */
void hmc5883l_raw_to_mg(const hmc5883l_raw_data_t *raw, hmc5883l_data_t *mg);

/**
 * \brief Get magnetic data in milligausses
 * \param data Pointer to the struct to write data
 * \return false if error occured in single measurement mode, always true in continuous mode
 */
bool hmc5883l_get_data(i2c_dev_t *dev, hmc5883l_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* EXTRAS_HMC5883L_H_ */
