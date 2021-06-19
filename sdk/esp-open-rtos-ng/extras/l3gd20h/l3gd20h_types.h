/**
 * Driver for L3GD20H 3-axes digital output gyroscope connected to I2C or SPI.
 * It can also be used with L3GD20 and L3G4200D.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __L3GD20H_TYPES_H__
#define __L3GD20H_TYPES_H__

#include "stdint.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C"
{
#endif


/**
 * @brief 	Output data rates (ODR)
 */
typedef enum 
{
    l3gd20h_power_down = 0,  // power down mode
    l3gd20h_normal_odr_12_5, // normal mode with low output data rate 12.5 Hz
    l3gd20h_normal_odr_25,   // normal mode with low output data rate 25 Hz
    l3gd20h_normal_odr_50,   // normal mode with low output data rate 50 Hz
    l3gd20h_normal_odr_100,  // normal mode with high output data rate 100 Hz
    l3gd20h_normal_odr_200,  // normal mode with high output data rate 200 Hz
    l3gd20h_normal_odr_400,  // normal mode with high output data rate 400 Hz
    l3gd20h_normal_odr_800,  // normal mode with high output data rate 800 Hz

} l3gd20h_mode_t;

#define l3gd20_normal_odr_95   l3gd20h_normal_odr_100
#define l3gd20_normal_odr_190  l3gd20h_normal_odr_200
#define l3gd20_normal_odr_380  l3gd20h_normal_odr_400
#define l3gd20_normal_odr_760  l3gd20h_normal_odr_800

/**
 * @brief   Sensitivity level
 */
typedef enum 
{
    l3gd20h_scale_245_dps = 0,     // default
    l3gd20h_scale_500_dps,
    l3gd20h_scale_2000_dps

} l3gd20h_scale_t;

#define l3gd20_scale_250_dps    l3gd20h_scale_245_dps
#define l3gd20_scale_500_dps    l3gd20h_scale_500_dps
#define l3gd20_scale_2000_dps   l3gd20h_scale_2000_dps

/**
 * @brief   FIFO mode
 */
typedef enum 
{
    l3gd20h_bypass = 0,     // default
    l3gd20h_fifo = 1,
    l3gd20h_stream = 2,
    l3gd20h_stream_to_fifo = 3,
    l3gd20h_bypass_to_stream = 5,
    l3gd20h_dynamic_stream = 6,
    l3gd20h_bypass_to_fifo = 7

} l3gd20h_fifo_mode_t;


/**
 * @brief   High pass filter (HPF) and low pass filter 2 (LPF2) modes
 */
typedef enum 
{
    l3gd20h_no_filter = 0,    // HPF not used, LPF2 not used
    l3gd20h_hpf_only,         // HPF used, LPF2 not used
    l3gd20h_lpf2_only,        // HPF not used, LPF2 used
    l3gd20h_hpf_and_lpf2      // HPF used, LPF2 used

} l3gd20h_filter_t;


/**
 * @brief   Interrupt types
 */
typedef enum {

    l3gd20h_int_data_ready,      // data are ready to read (INT2)

    l3gd20h_int_fifo_threshold,  // FIFO filling exceds FTH level (INT2)
    l3gd20h_int_fifo_overrun,    // FIFO is completely filled (INT2)
    l3gd20h_int_fifo_empty,      // FIFO becomes empty (INT2)

    l3gd20h_int_event            // angular rate of one or more axes becomes
                                 // lower or higher than threshold (INT1)
} l3gd20h_int_types_t;


/**
 * @brief   Event interrupt generator configuration (axis movement and wake up)
 *
 * memset to 0 to disable all interrupt conditions (default)
 */
typedef struct 
{
    bool     x_low_enabled;   // x lower than threshold interrupt enabled
    bool     x_high_enabled;  // x higher than threshold interrupt enabled
    uint16_t x_threshold;     // x threshold value
    
    bool     y_low_enabled;   // y lower than threshold interrupt enabled
    bool     y_high_enabled;  // y higher than threshold interrupt enabled
    uint16_t y_threshold;     // y threshold value

    bool     z_low_enabled;   // z lower than threshold interrupt enabled
    bool     z_high_enabled;  // z higher than threshold interrupt enabled
    uint16_t z_threshold;     // z threshold value

    l3gd20h_filter_t filter;  // HPF and LPF2 mode used for threshold comparison

    bool     and_or;          // interrupt combination true - AND, false - OR
                              // AND - all enabled axes passed the treshold
                              // OR  - at least one axes passed the threshold
                              
    bool     latch;           // latch the interrupt when true until the
                              // interrupt source has been read
                              
    uint8_t  duration;        // duration in 1/ODR an interrupt condition has
                              // to be given before the interrupt is generated

    bool     wait;            // when true, duration is also used when interrupt
                              // condition in no longer given before interrupt
                              // signal is reset
                              
    bool     counter_mode;    // DCRM is not documented and not used therefore
    
} l3gd20h_int_event_config_t;


/**
 * @brief   Event interrupt source (axis movement and wake up)
 */
typedef struct {

    bool    x_low :1;     // true - x is lower event occured
    bool    x_high:1;     // true - x is higher event occured

    bool    y_low :1;     // true - z is lower event occured
    bool    y_high:1;     // true - z is higher event occured

    bool    z_low :1;     // true - z is lower event occured
    bool    z_high:1;     // true - z is higher event occured
    
    bool    active:1;     // true - one ore more have been generated
    
} l3gd20h_int_event_source_t;


/**
 * @brief   Data interrupt source type (data ready and FIFO status)
 */
typedef struct {

    bool data_ready;        // true when data are ready to read

    bool fifo_threshold;    // true when FIFO filling >= FTH level
    bool fifo_overrun;      // true when FIFO is completely filled
    bool fifo_empty;        // true when FIFO is empty
    
} l3gd20h_int_data_source_t;


/**
 * @brief   INT1, INT2 signal activity level
 */
typedef enum {

    l3gd20h_high_active = 0,
    l3gd20h_low_active

} l3gd20h_signal_level_t;
    
    
/**
 * @brief   INT1, INT2 signal type
 */
typedef enum {

    l3gd20h_push_pull = 0,
    l3gd20h_open_drain

} l3gd20h_signal_type_t;


/**
 * @brief   Raw data set as two complements
 */
typedef struct {

    int16_t x;
    int16_t y;
    int16_t z;

} l3gd20h_raw_data_t;


/**
 * @brief   Raw data FIFO type
 */
typedef l3gd20h_raw_data_t l3gd20h_raw_data_fifo_t[32];


/**
 * @brief   Floating point output value set in degree
 */
typedef struct {

    float x;
    float y;
    float z;

} l3gd20h_float_data_t;


/**
 * @brief   Floating point output value FIFO type
 */
typedef l3gd20h_float_data_t l3gd20h_float_data_fifo_t[32];


/**
 * @brief   HPF (high pass filter) modes
 */
typedef enum {

    l3gd20h_hpf_normal = 0,
    l3gd20h_hpf_reference,
    l3gd20h_hpf_normal_x,
    l3gd20h_hpf_autoreset

} l3gd20h_hpf_mode_t;


/**
 * @brief   L3GD20H sensor device data structure type
 */
typedef struct {

    int       error_code;      // contains the error code of last operation

    uint8_t   bus;             // I2C = x, SPI = 1
    uint8_t   addr;            // I2C = slave address, SPI = 0

    uint8_t   cs;              // ESP8266, ESP32: GPIO used as SPI CS
                               // __linux__: device index

    l3gd20h_scale_t     scale;     // fill range scale (default 245 dps)
    l3gd20h_fifo_mode_t fifo_mode; // FIFO operation mode (default bypass)

    enum {
        l3gd20h,
        l3gd20,
        l3g4200d
    } mode;
      
} l3gd20h_sensor_t;
                                 

#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* __L3GD20H_TYPES_H__ */
