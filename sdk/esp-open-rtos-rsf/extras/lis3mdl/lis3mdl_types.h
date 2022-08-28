/**
 * Driver for LIS3MDL 3-axes digital accelerometer connected to I2C or SPI.
 *
 * This driver is for the usage with the ESP8266 and FreeRTOS (esp-open-rtos)
 * [https://github.com/SuperHouse/esp-open-rtos]. It is also working with ESP32
 * and ESP-IDF [https://github.com/espressif/esp-idf.git] as well as Linux
 * based systems using a wrapper library for ESP8266 functions.
 *
 * ---------------------------------------------------------------------------
 *
 * The BSD License (3-clause license)
 *
 * Copyright (c) 2017 Gunar Schorcht (https://github.com/gschorcht)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO Activity SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __LIS3MDL_TYPES_H__
#define __LIS3MDL_TYPES_H__

#include "stdint.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C"
{
#endif


/**
 * @brief   Operation mode (OM) and output data rates (ODR)
 */
typedef enum {

    lis3mdl_lpm_0_625 = 0,  // low power mode at 0.625 Hz
    lis3mdl_lpm_1_25,       // low power mode at 1.25 Hz
    lis3mdl_lpm_2_5,        // low power mode at 2.5 Hz
    lis3mdl_lpm_5,          // low power mode at 5 Hz
    lis3mdl_lpm_10,         // low power mode at 10 Hz
    lis3mdl_lpm_20,         // low power mode at 20 Hz
    lis3mdl_lpm_40,         // low power mode at 40 Hz
    lis3mdl_lpm_80,         // low power mode at 80 Hz
    lis3mdl_lpm_1000,       // low power mode at 1000 Hz
    lis3mdl_mpm_560,        // medium performance mode at 560 Hz
    lis3mdl_hpm_300,        // high performance mode at 300 Hz
    lis3mdl_uhpm_155,       // ultra high performance mode at 155 Hz
    lis3mdl_low_power,      // low power mode at 0.625 Hz
    lis3mdl_power_down      // power down mode

} lis3mdl_mode_t;


/**
 * @brief   Full scale measurement range in Gauss
 */
typedef enum {

    lis3mdl_scale_4_Gs = 0,     // default
    lis3mdl_scale_8_Gs,
    lis3mdl_scale_12_Gs,
    lis3mdl_scale_16_Gs

} lis3mdl_scale_t;

/**
 * @brief   Magnetic threshold interrupt configuration for INT signal
 */
typedef struct {
    
    uint16_t threshold; // threshold used for interrupt generation

    bool     x_enabled; // true - x exceeds threshold on positive side
    bool     y_enabled; // true - y exceeds threshold on positive side
    bool     z_enabled; // true - z exceeds threshold on positive side

    bool     latch;     // true - latch the interrupt until the interrupt
                        //        source has been read
    enum 
    {
        lis3mdl_low_active  = 0,
        lis3mdl_high_active = 1

    } signal_level;     // level of interrupt signal
    
} lis3mdl_int_config_t;


/**
 * @brief   Magnetic threshold interrupt source of INT signal
 */
typedef struct {
    
    bool x_pos :1;     // true - x exceeds threshold on positive side
    bool y_pos :1;     // true - y exceeds threshold on positive side
    bool z_pos :1;     // true - z exceeds threshold on positive side

    bool x_neg :1;     // true - x exceeds threshold on negative side
    bool y_neg :1;     // true - y exceeds threshold on negative side
    bool z_neg :1;     // true - z exceeds threshold on negative side

    bool mroi  :1;     // true - internal measurement range overflow
    bool active:1;     // true - interrupt event occured
    
} lis3mdl_int_source_t;


/**
 * @brief   Raw data set as two's complements
 */
typedef struct {

    int16_t mx; // magnetic value on x axis
    int16_t my; // magnetic value on y axis
    int16_t mz; // magnetic value on z axis

} lis3mdl_raw_data_t;


/**
 * @brief   Floating point output value set in Gauss
 */
typedef struct {

    float mx;   // magnetic value on x axis
    float my;   // magnetic value on y axis
    float mz;   // magnetic value on z axis

} lis3mdl_float_data_t;


/**
 * @brief   LIS3MDL sensor device data structure type
 */
typedef struct {

    int       error_code;       // error code of last operation

    uint8_t   bus;              // I2C = x, SPI = 1
    uint8_t   addr;             // I2C = slave address, SPI = 0

    uint8_t   cs;               // ESP8266, ESP32: GPIO used as SPI CS
                                // __linux__: device index

    lis3mdl_scale_t  scale;     // full range scale (default 4 Gauss)
      
} lis3mdl_sensor_t;
                                 

#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* __LIS3MDL_TYPES_H__ */
