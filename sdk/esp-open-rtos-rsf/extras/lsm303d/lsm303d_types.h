/**
 * Driver for LSM303D 3-axes digital accelerometer and magnetometer connected
 * either to I2C or SPI.
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
 * Copyright (c) 2018 Gunar Schorcht (https://github.com/gschorcht)
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

#ifndef __LSM303D_TYPES_H__
#define __LSM303D_TYPES_H__

#include "stdint.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C"
{
#endif


/**
 * @brief   Accelerator output data rates (A_ODR)
 */
typedef enum {

    lsm303d_a_power_down = 0,  // power down mode (default)
    lsm303d_a_odr_3_125,       // normal power mode 3.125 Hz
    lsm303d_a_odr_6_25,        // normal power mode 6.25 Hz
    lsm303d_a_odr_12_5,        // normal power mode 12.5 Hz
    lsm303d_a_odr_25,          // normal power mode 25 Hz
    lsm303d_a_odr_50,          // normal power mode 50 Hz
    lsm303d_a_odr_100,         // normal power mode 100 Hz
    lsm303d_a_odr_200,         // normal power mode 200 Hz
    lsm303d_a_odr_400,         // normal power mode 400 Hz
    lsm303d_a_odr_800,         // normal power mode 800 Hz
    lsm303d_a_odr_1600,        // normal power mode 1.6 kHz

} lsm303d_a_odr_t;

/**
 * @brief   Accelerator anti-alias filter (A_AAF) bandwidth (BW) in Hz
 */
typedef enum {

    lsm303d_a_aaf_bw_773 = 0,  // default
    lsm303d_a_aaf_bw_194,    
    lsm303d_a_aaf_bw_362,    
    lsm303d_a_aaf_bw_50

} lsm303d_a_aaf_bw_t; 

/**
 * @brief   Accelerator full scale ranges (A_SCALE) in g
 */
typedef enum {

    lsm303d_a_scale_2_g = 0,     // default
    lsm303d_a_scale_4_g,
    lsm303d_a_scale_6_g,
    lsm303d_a_scale_8_g,
    lsm303d_a_scale_16_g

} lsm303d_a_scale_t;

/**
 * @brief   Magnetometer output data rates (M_ODR)
 */
typedef enum {

    lsm303d_m_odr_3_125 = 0,   // normal power mode at 3.125 Hz
    lsm303d_m_odr_6_25,        // normal power mode at 6.25 Hz
    lsm303d_m_odr_12_5,        // normal power mode at 12.5 Hz
    lsm303d_m_odr_25,          // normal power mode at 25 Hz
    lsm303d_m_odr_50,          // normal power mode at 50 Hz
    lsm303d_m_odr_100,         // normal power mode at 100 Hz
    lsm303d_m_do_not_use,      // power down mode (default)
    lsm303d_m_low_power        // low power mode at 3.125 Hz

} lsm303d_m_odr_t;

/**
 * @brief   Magnetometer sensor mode (M_MODE)
 */
typedef enum {

    lsm303d_m_continuous = 0,  // continuous conversion mode
    lsm303d_m_single,          // single conversion mode (default)
    lsm303d_m_power_down       // power-down mode

} lsm303d_m_mode_t;


/**
 * @brief   Magnetometer resolution selection
 */
typedef enum {

    lsm303d_m_low_res,     // low resolution (default)
    lsm303d_m_high_res     // high resolution

} lsm303d_m_resolution_t;

/**
 * @brief   Magnetometer full scale ranges (M_SCALE) in Gauss (Gs)
 */
typedef enum {

    lsm303d_m_scale_2_Gs = 0,
    lsm303d_m_scale_4_Gs,     // default
    lsm303d_m_scale_8_Gs,
    lsm303d_m_scale_12_Gs

} lsm303d_m_scale_t;

/**
 * @brief   FIFO mode for accelerator data
 */
typedef enum {

    lsm303d_bypass = 0,     // default
    lsm303d_fifo,
    lsm303d_stream,
    lsm303d_stream_to_fifo,
    lsm303d_bypass_to_stream
    
} lsm303d_fifo_mode_t;

/**
 * @brief   Interrupt signals
 */
typedef enum {

    lsm303d_int1_signal = 0,
    lsm303d_int2_signal = 1    

} lsm303d_int_signal_t;


/**
 * @brief   INT1, INT2 signal type
 */
typedef enum {

    lsm303d_push_pull = 0,
    lsm303d_open_drain

} lsm303d_int_signal_type_t;


/**
 * @brief   Inertial event interrupt generators
 */
typedef enum {

    lsm303d_int_event1_gen = 0,
    lsm303d_int_event2_gen = 1    

} lsm303d_int_event_gen_t;


/**
 * @brief   Interrupt types for interrupt signals INT1/INT2
 */
typedef enum {

    lsm303d_int_a_data_ready, // acceleration data ready for read interrupt
    lsm303d_int_m_data_ready, // magnetic data ready for read interrupt

    lsm303d_int_fifo_empty,   // FIFO is empty (only INT1)
    lsm303d_int_fifo_thresh,  // FIFO exceeds the threshold (only INT2)
    lsm303d_int_fifo_overrun, // FIFO is completely filled (only INT2)
    
    lsm303d_int_event1,       // inertial event interrupt 1
    lsm303d_int_event2,       // inertial event interrupt 2

    lsm303d_int_click,        // click detection interrupt

    lsm303d_int_m_thresh      // magnetic threshold interrupt
   
} lsm303d_int_type_t;


/**
 * @brief   Data ready and FIFO interrupt source for INT1/INT2
 */
typedef struct {

    bool a_data_ready;      // true when acceleration data are ready to read
    bool m_data_ready;      // true when magnetic data are ready to read

    bool fifo_empty;        // true when FIFO is empty
    bool fifo_thresh;       // true when FIFO exceeds the FIFO threshold
    bool fifo_overrun;      // true when FIFO is completely filled
    
} lsm303d_int_data_source_t;


/**
 * @brief   Magnetic threshold interrupt configuration for INT1/INT2 signals
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
        lsm303d_low_active  = 0,
        lsm303d_high_active = 1

    } signal_level;     // level of interrupt signal

} lsm303d_int_m_thresh_config_t;


/**
 * @brief   Magnetic threshold interrupt source of INT1/INT2 signals
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
    
} lsm303d_int_m_thresh_source_t;


/**
 * @brief   Inertial interrupt generator configuration for INT1/INT2
 *
 * Inertial events are: axis movement and 6D/4D detection.
 */
typedef struct {

    enum                      // interrupt mode
    {                         // AOI (IG_CFGx), 6D (IG_CFGx), 4D (INT_CTRL_M)

        lsm303d_or,           // AOI = 0, 6D = 0, 4D = X
        lsm303d_and,          // AOI = 1, 6D = 0, 4D = X

        lsm303d_6d_movement,  // AOI = 0, 6D = 1, 4D = 0
        lsm303d_6d_position,  // AOI = 1, 6D = 1, 4D = 0

        lsm303d_4d_movement,  // AOI = 0, 6D = 1, 4D = 1
        lsm303d_4d_position,  // AOI = 1, 6D = 1, 4D = 1
    
    } mode;            

    uint8_t  threshold;       // threshold used for comparison for all axes

    bool     x_low_enabled;   // x lower than threshold interrupt enabled
    bool     x_high_enabled;  // x higher than threshold interrupt enabled
    
    bool     y_low_enabled;   // y lower than threshold interrupt enabled
    bool     y_high_enabled;  // y higher than threshold interrupt enabled

    bool     z_low_enabled;   // z lower than threshold interrupt enabled
    bool     z_high_enabled;  // z higher than threshold interrupt enabled

    bool     latch;           // latch the interrupt when true until the
                              // interrupt source has been read
                              
    uint8_t  duration;        // duration in 1/ODR an interrupt condition has
                              // to be given before the interrupt is generated
                              
} lsm303d_int_event_config_t;


/**
 * @brief   Inertial event source type for interrupt generator INT1/INT2 
 */
typedef struct {

    bool    active:1;     // true - one ore more events occured
    
    bool    x_low :1;     // true - x is lower than threshold event
    bool    x_high:1;     // true - x is higher than threshold event

    bool    y_low :1;     // true - z is lower than threshold event
    bool    y_high:1;     // true - z is higher than threshold event

    bool    z_low :1;     // true - z is lower than threshold event
    bool    z_high:1;     // true - z is higher than threshold event
    
} lsm303d_int_event_source_t;


/**
 * @brief   Click interrupt configuration for interrupt signals INT1/INT2 
 */
typedef struct {

    bool     x_single;      // x-axis single tap interrupt enabled
    bool     x_double;      // x-axis double tap interrupt enabled
    
    bool     y_single;      // y-axis single tap interrupt enabled
    bool     y_double;      // y-axis double tap interrupt enabled

    bool     z_single;      // z-axis single tap interrupt enabled
    bool     z_double;      // z-axis double tap interrupt enabled

    uint8_t  threshold;     // threshold used for comparison for all axes

    bool     latch;         // latch the interrupt when true until the
                            // interrupt source has been read
                          
    uint8_t  time_limit;    // maximum time interval between the start and the
                            // end of a cick (accel increases and falls back)
    uint8_t  time_latency;  // click detection is disabled for that time after 
                            // a was click detected (in 1/ODR)
    uint8_t  time_window;   // time interval in which the second click has to
                            // to be detected in double clicks (in 1/ODR)

} lsm303d_int_click_config_t;


/**
 * @brief   Click interrupt source for interrupt signals INT1/INT2 
 */
typedef struct {

    bool    x_click:1;    // click detected in x direction
    bool    y_click:1;    // click detected in y direction
    bool    z_click:1;    // click detected in z direction

    bool    sign   :1;    // click sign (0 - posisitive, 1 - negative)

    bool    s_click:1;    // single click detected
    bool    d_click:1;    // double click detected

    bool    active :1;    // true - one ore more event occured

} lsm303d_int_click_source_t;


/**
 * @brief   HPF (high pass filter) modes for acceleration data
 */
typedef enum {

    lsm303d_hpf_normal = 0, // normal mode (reset by reading references)
    lsm303d_hpf_reference,  // reference signal used for filtering
    lsm303d_hpf_normal_x,   // normal mode
    lsm303d_hpf_autoreset   // autoreset on interrupt event

} lsm303d_hpf_mode_t;
    
    
/**
 * @brief   Raw accelerations data set of as two complements
 */
typedef struct {

    int16_t ax; // acceleration on x axis
    int16_t ay; // acceleration on y axis
    int16_t az; // acceleration on z axis

} lsm303d_raw_a_data_t;


/**
 * @brief   Raw acceleration data FIFO type
 */
typedef lsm303d_raw_a_data_t lsm303d_raw_a_data_fifo_t[32];


/**
 * @brief   Floating point accelerations output value set in g
 */
typedef struct {

    float ax;   // acceleration on x axis
    float ay;   // acceleration on y axis
    float az;   // acceleration on z axis

} lsm303d_float_a_data_t;


/**
 * @brief   Floating point accelerations output value FIFO type
 */
typedef lsm303d_float_a_data_t lsm303d_float_a_data_fifo_t[32];


/**
 * @brief   Raw magnetic data set as two's complements
 */
typedef struct {

    int16_t mx; // magnetic value on x axis
    int16_t my; // magnetic value on y axis
    int16_t mz; // magnetic value on z axis

} lsm303d_raw_m_data_t;


/**
 * @brief   Floating point magnetic output value set in Gauss
 */
typedef struct {

    float mx;   // magnetic value on x axis
    float my;   // magnetic value on y axis
    float mz;   // magnetic value on z axis

} lsm303d_float_m_data_t;


/**
 * @brief   LSM303D sensor device data structure type
 */
typedef struct {

    int       error_code;              // error code of last operation

    uint8_t   bus;                     // I2C = x, SPI = 1
    uint8_t   addr;                    // I2C = slave address, SPI = 0

    uint8_t   cs;                      // ESP8266, ESP32: GPIO used as SPI CS
                                       // __linux__: device index

    lsm303d_a_scale_t      a_scale;    // acceleration full scale (default 2 g)
    lsm303d_m_scale_t      m_scale;    // magnetic full scale (default 4 Gauss)
    lsm303d_m_resolution_t m_res;      // magnetic resolution (default low)

    lsm303d_fifo_mode_t    fifo_mode;  // FIFO operation mode (default bypass)
    bool                   fifo_first; // first FIFO access
      
} lsm303d_sensor_t;
                                 

#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* __LSM303D_TYPES_H__ */
