/**
 * Driver for AD7705/AD7706 SPI ADC
 *
 * Part of esp-open-rtos
 * Copyright (C) 2017 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#ifndef _EXTRAS_AD770X_H_
#define _EXTRAS_AD770X_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Input gain
 */
typedef enum
{
    AD770X_GAIN_1 = 0,
    AD770X_GAIN_2,
    AD770X_GAIN_4,
    AD770X_GAIN_8,
    AD770X_GAIN_16,
    AD770X_GAIN_32,
    AD770X_GAIN_64,
    AD770X_GAIN_128
} ad770x_gain_t;

/**
 * Master clock frequency
 */
typedef enum
{
    AD770X_MCLK_1MHz = 0,  //!< 1 MHz
    AD770X_MCLK_2MHz,      //!< 2 MHz
    AD770X_MCLK_2_4576MHz, //!< 2.4576 MHz
    AD770X_MCLK_4_9152MHz, //!< 4.9152 MHz
} ad770x_master_clock_t;

/**
 * Output update rate
 */
typedef enum
{
    AD770X_RATE_20 = 0,  //!< Output update rate is 20 Hz,  -3 dB filter cutoff = 5.24 Hz, works with 1 or 2 MHz master clock
    AD770X_RATE_25,      //!< Output update rate is 25 Hz,  -3 dB filter cutoff = 6.55 Hz, works with 1 or 2 MHz master clock
    AD770X_RATE_100,     //!< Output update rate is 100 Hz, -3 dB filter cutoff = 26.2 Hz, works with 1 or 2 MHz master clock
    AD770X_RATE_200,     //!< Output update rate is 200 Hz, -3 dB filter cutoff = 52.4 Hz, works with 1 or 2 MHz master clock
    AD770X_RATE_50,      //!< Output update rate is 50 Hz,  -3 dB filter cutoff = 13.1 Hz, works with 2.4576 or 4.9152 MHz master clock
    AD770X_RATE_60,      //!< Output update rate is 60 Hz,  -3 dB filter cutoff = 15.7 Hz, works with 2.4576 or 4.9152 MHz master clock
    AD770X_RATE_250,     //!< Output update rate is 250 Hz, -3 dB filter cutoff = 65.5 Hz, works with 2.4576 or 4.9152 MHz master clock
    AD770X_RATE_500,     //!< Output update rate is 500 Hz, -3 dB filter cutoff = 131 Hz,  works with 2.4576 or 4.9152 MHz master clock
} ad770x_update_rate_t;

/**
 * Device mode
 */
typedef enum
{
    AD770X_MODE_NORMAL = 0,
    AD770X_MODE_CALIBRATION,
    AD770X_MODE_ZERO_CALIBRATION,
    AD770X_MODE_FULL_CALIBRATION
} ad770x_mode_t;

/**
 * Device descriptor
 */
typedef struct
{
    uint8_t cs_pin;                     //!< GPIO pin for chip select
    ad770x_master_clock_t master_clock; //!< Master clock frequency
    bool bipolar;                       //!< Bipolar/Unipolar mode
    ad770x_gain_t gain;                 //!< Input gain
    ad770x_update_rate_t update_rate;   //!< Output update rate
} ad770x_params_t;

/**
 * Init device and setup channel params
 * @param params Device descriptor pointer
 * @param channel Input channel
 * @return Non-zero when error occured
 */
int ad770x_init(const ad770x_params_t *params, uint8_t channel);

/**
 * Set device mode (see datasheet)
 * @param params Device descriptor pointer
 * @param channel Input channel
 * @param mode Device mode
 */
void ad770x_set_mode(const ad770x_params_t *params, uint8_t channel, ad770x_mode_t mode);

/**
 * Get conversion status
 * @param params Device descriptor pointer
 * @param channel Input channel
 * @return true when data is ready
 */
bool ad770x_data_ready(const ad770x_params_t *params, uint8_t channel);

/**
 * Get converted ADC value
 * @param params Device descriptor pointer
 * @param channel Input channel
 * @return Raw ADC value
 */
uint16_t ad770x_raw_adc_value(const ad770x_params_t *params, uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* _EXTRAS_AD770X_H_ */
