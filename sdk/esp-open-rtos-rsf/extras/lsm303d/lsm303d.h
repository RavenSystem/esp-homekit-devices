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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __LSM303D_H__
#define __LSM303D_H__

// Uncomment one of the following defines to enable debug output
// #define LSM303D_DEBUG_LEVEL_1    // only error messages
// #define LSM303D_DEBUG_LEVEL_2    // debug and error messages

// LSM303D addresses
#define LSM303D_I2C_ADDRESS_1           0x1e  // SDO pin is low
#define LSM303D_I2C_ADDRESS_2           0x1d  // SDO pin is high

// LSM303D chip id
#define LSM303D_CHIP_ID                 0x49  // LSM303D_REG_WHO_AM_I<7:0>

// Definition of error codes
#define LSM303D_OK                      0
#define LSM303D_NOK                     -1

#define LSM303D_INT_ERROR_MASK          0x000f
#define LSM303D_DRV_ERROR_MASK          0xfff0

// Error codes for I2C and SPI interfaces ORed with LSM303D driver error codes
#define LSM303D_I2C_READ_FAILED         1
#define LSM303D_I2C_WRITE_FAILED        2
#define LSM303D_I2C_BUSY                3
#define LSM303D_SPI_WRITE_FAILED        4
#define LSM303D_SPI_READ_FAILED         5
#define LSM303D_SPI_BUFFER_OVERFLOW     6

// LSM303D driver error codes ORed with error codes for I2C and SPI interfaces
#define LSM303D_WRONG_CHIP_ID              ( 1 << 8)
#define LSM303D_WRONG_BANDWIDTH            ( 2 << 8)
#define LSM303D_GET_RAW_A_DATA_FAILED      ( 3 << 8)
#define LSM303D_GET_RAW_A_DATA_FIFO_FAILED ( 4 << 8)
#define LSM303D_GET_RAW_M_DATA_FAILED      ( 5 << 8)
#define LSM303D_GET_RAW_T_DATA_FAILED      ( 6 << 8)
#define LSM303D_INT_TYPE_WRONG             ( 8 << 8)
#define LSM303D_INT_ENABLE_FAILED          ( 9 << 8)
#define LSM303D_CONFIG_INT_SIGNALS_FAILED  (10 << 8)
#define LSM303D_GET_INT_DATA_SOURCE_FAILED (11 << 8)
#define LSM303D_SET_M_THRESH_CONFIG_FAILED (12 << 8)
#define LSM303D_GET_M_THRESH_CONFIG_FAILED (13 << 8)
#define LSM303D_GET_M_THRESH_SOURCE_FAILED (14 << 8)
#define LSM303D_SET_EVENT_CONFIG_FAILED    (15 << 8)
#define LSM303D_GET_EVENT_CONFIG_FAILED    (16 << 8)
#define LSM303D_GET_EVENT_SOURCE_FAILED    (17 << 8)
#define LSM303D_SET_CLICK_CONFIG_FAILED    (18 << 8)
#define LSM303D_GET_CLICK_CONFIG_FAILED    (19 << 8)
#define LSM303D_GET_CLICK_SOURCE_FAILED    (20 << 8)
#define LSM303D_CONFIG_HPF_FAILED          (21 << 8)
#define LSM303D_SET_HPF_REF_FAILED         (22 << 8)
#define LSM303D_GET_HPF_REF_FAILED         (23 << 8)
#define LSM303D_SET_M_OFFSET_FAILED        (24 << 8)
#define LSM303D_GET_M_OFFSET_FAILED        (25 << 8)
#define LSM303D_GET_ADC_DATA_FAILED        (26 << 8)
#define LSM303D_SENSOR_IN_BYPASS_MODE      (27 << 8)
#define LSM303D_SENSOR_IN_FIFO_MODE        (28 << 8)
#define LSM303D_ODR_TOO_HIGH               (29 << 8)
#define LSM303D_FIFO_THRESHOLD_INVALID     (30 << 8)
#define LSM303D_FIFO_GET_SRC_FAILED        (31 << 8)

#include "lsm303d_platform.h"
#include "lsm303d_types.h"

#ifdef __cplusplus
extern "C"
{
#endif


/**
 * @brief   Initialize the sensor
 *
 * Reset the sensor and switch to power down mode. All registers are reset to 
 * default values. FIFO is cleared.
 *
 * @param   bus     I2C or SPI bus at which LSM303D sensor is connected
 * @param   addr    I2C addr of the LSM303D sensor, 0 for using SPI
 * @param   cs      SPI CS GPIO, ignored for I2C
 * @return          pointer to sensor data structure, or NULL on error
 */
lsm303d_sensor_t* lsm303d_init_sensor (uint8_t bus, uint8_t addr, uint8_t cs);


/**
 * @brief   Set accelerator sensor mode
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   odr     accelerator output data rate (ODR)
 * @param   bw      accelerator anti-alias filter bandwidth
 * @param   x       true enable x-axis, false disable x-axis
 * @param   y       true enable y-axis, false disable y-axis
 * @param   z       true enable z-axis, false disable z-axis
 * @return          true on success, false on error
 */
bool lsm303d_set_a_mode (lsm303d_sensor_t* dev, 
                         lsm303d_a_odr_t odr, lsm303d_a_aaf_bw_t bw,
                         bool x, bool y, bool z);
                       
/**
 * @brief   Set magnetometer sensor mode
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   odr     magnetometer output data rate (ODR)
 * @param   res     magnetometer resolution
 * @param   mode    magnetometer mode (ODR)
 * @return          true on success, false on error
 */
bool lsm303d_set_m_mode (lsm303d_sensor_t* dev, 
                         lsm303d_m_odr_t odr, 
                         lsm303d_m_resolution_t res,
                         lsm303d_m_mode_t mode);

/**
 * @brief   Set accelerator scale (full scale)
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   scale   full scale (default 2 g) 
 * @return          true on success, false on error
 */
bool lsm303d_set_a_scale (lsm303d_sensor_t* dev, lsm303d_a_scale_t scale);
                              
                              
/**
 * @brief   Set magnetometer scale (full scale)
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   scale   full scale (default 4 Gauss)
 * @return          true on success, false on error
 */
bool lsm303d_set_m_scale (lsm303d_sensor_t* dev, lsm303d_m_scale_t scale);


/**
 * @brief   Test whether new acceleration data samples are available
 *
 * When the FIFO is used, it returns true if at least one acceleration
 * data sample is stored in the FIFO. Otherwise it returns true when new
 * acceleration data are available in the output registers.
 *
 * @param   dev     pointer to the sensor device data structure
 * @return          true on new data, otherwise false
 */
bool lsm303d_new_a_data (lsm303d_sensor_t* dev);


/**
 * @brief   Test whether new magnetometer data samples are available
 *
 * @param   dev     pointer to the sensor device data structure
 * @return          true on new data, otherwise false
 */
bool lsm303d_new_m_data (lsm303d_sensor_t* dev);


/**
 * @brief   Get one acceleration data sample as floating point values (unit g)
 *
 * Function works only in bypass mode and fails in FIFO modes. In FIFO modes,
 * function *lsm303d_get_a_float_data_fifo* has to be used instead to get data.
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   data    pointer to float data structure filled with g values
 * @return          true on success, false on error
 */
bool lsm303d_get_float_a_data (lsm303d_sensor_t* dev,
                               lsm303d_float_a_data_t* data);


/**
 * @brief   Get all samples of acceleration data stored in the FIFO (unit g)
 *
 * In bypass mode, it returns only one sensor data sample.
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   data    array of 32 float data structures filled with g values
 * @return          number of data sets read from fifo on success or 0 on error
 */
uint8_t lsm303d_get_float_a_data_fifo (lsm303d_sensor_t* dev,
                                       lsm303d_float_a_data_fifo_t data);


/**
 * @brief   Get one magnetic data sample as floating point values (unit Gauss)
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   data    pointer to float data structure filled with magnetic values
 * @return          true on success, false on error
 */
bool lsm303d_get_float_m_data (lsm303d_sensor_t* dev,
                               lsm303d_float_m_data_t* data);


/**
 * @brief   Get one sample of raw acceleration data as 16 bit two's complements
 *
 * Function works only in bypass mode and fails in FIFO modes. In FIFO modes,
 * function *lsm303d_get_a_raw_data_fifo* has to be used instead to get data.
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   raw     pointer to raw data structure filled with values
 * @return          true on success, false on error
 */
bool lsm303d_get_raw_a_data (lsm303d_sensor_t* dev, lsm303d_raw_a_data_t* raw);


/**
 * @brief   Get all samples of raw sensor data stored in the FIFO
 *
 * In bypass mode, it returns only one raw data sample.
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   raw     array of 32 raw data structures
 * @return          number of data sets read from fifo on success or 0 on error
 */
uint8_t lsm303d_get_raw_a_data_fifo (lsm303d_sensor_t* dev,
                                     lsm303d_raw_a_data_fifo_t raw);
                                   

/**
 * @brief   Get one sample of raw magnetic data as 16 bit two's complements
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   raw     pointer to raw data structure filled with values
 * @return          true on success, false on error
 */
bool lsm303d_get_raw_m_data (lsm303d_sensor_t* dev, lsm303d_raw_m_data_t* raw);


/**
 * @brief   Set FIFO mode (for acceleration data only)
 *
 * FIFO threshold can be used to generate an interrupt when FIFO content
 * exceeds the value. It is ignored in bypass mode. 
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   mode    FIFO mode
 * @param   thresh  FIFO threshold (ignored in bypass mode)
 * @return          true on success, false on error
 */
bool lsm303d_set_fifo_mode (lsm303d_sensor_t* dev, lsm303d_fifo_mode_t mode, 
                            uint8_t thresh);
                            

/**
 * @brief   Enable / disable an interrupt on signal INT1 or INT2
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   type    interrupt to be enabled or disabled
 * @param   signal  interrupt signal that is activated for the interrupt
 * @param   value   true to enable or false to disable the interrupt
 * @return          true on success, false on error
 */
bool lsm303d_enable_int (lsm303d_sensor_t* dev,
                         lsm303d_int_type_t type,
                         lsm303d_int_signal_t signal, bool value);


/**
 * @brief   Get the source of data ready and FIFO interrupts on INT1 or INT2
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   source   pointer to the interrupt source
 * @return           true on success, false on error
 */
bool lsm303d_get_int_data_source (lsm303d_sensor_t* dev,
                                  lsm303d_int_data_source_t* source);
                                  
/**
 * @brief   Set the configuration of the magnetic threshold interrupt generator
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   config   pointer to the interrupt generator configuration
 * @return           true on success, false on error
 */
bool lsm303d_set_int_m_thresh_config (lsm303d_sensor_t* dev,
                                      lsm303d_int_m_thresh_config_t* config);


/**
 * @brief   Get the configuration of the magnetic threshold interrupt generator
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   config   pointer to the interrupt generator configuration
 * @return           true on success, false on error
 */
bool lsm303d_get_int_m_thresh_config (lsm303d_sensor_t* dev, 
                                      lsm303d_int_m_thresh_config_t* config);


/**
 * @brief   Get the source of the magnetic threshold interrupt on INT/INT2
 *
 * Returns a byte with flags that indicate the value(s) that triggered
 * the interrupt signal (see INT_SRC_M register in datasheet for details)
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   source   pointer to the interrupt source
 * @return           true on success, false on error
 */
bool lsm303d_get_int_m_thresh_source (lsm303d_sensor_t* dev,
                                      lsm303d_int_m_thresh_source_t* source);
                             

/**
 * @brief   Set the configuration of an inertial event interrupt generator
 *
 * Inertial interrupt generators produce interrupts when certain inertial event
 * occures (event interrupts), that is, the acceleration of defined axes is
 * higher or lower than a defined threshold and one of the following event is
 * recognized: axis movement or 6D/4D orientation detection.
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   config   pointer to the interrupt generator configuration
 * @param   gen      interrupt generator to which the function is applied
 * @return           true on success, false on error
 */
bool lsm303d_set_int_event_config (lsm303d_sensor_t* dev,
                                   lsm303d_int_event_config_t* config,
                                   lsm303d_int_event_gen_t gen);


/**
 * @brief   Get the configuration of an inertial interrupt generator
 *
 * Inertial interrupt generators produce interrupts when certain inertial event
 * occures (event interrupts), that is, the acceleration of defined axes is
 * higher or lower than a defined threshold and one of the following event is
 * recognized: axis movement or 6D/4D orientation detection.
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   config   pointer to the interrupt generator configuration
 * @param   gen      interrupt generator to which the function is applied
 * @return           true on success, false on error
 */
bool lsm303d_get_int_event_config (lsm303d_sensor_t* dev,
                                   lsm303d_int_event_config_t* config,
                                   lsm303d_int_event_gen_t gen);


/**
 * @brief   Get the source of an inertial event interrupt on signal INT1/INT2
 *
 * Returns a byte with flags that indicate the event that triggered
 * the interrupt signal (see IG_SRCx register in datasheet for details)
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   source   pointer to the interrupt source data structure
 * @param   gen      interrupt generator to which the function is applied
 * @return           true on success, false on error
 */
bool lsm303d_get_int_event_source (lsm303d_sensor_t* dev,
                                   lsm303d_int_event_source_t* source,
                                   lsm303d_int_event_gen_t gen);


/**
 * @brief   Set the configuration of the click detection interrupt generator
 *
 * Set the configuration for interrupts that are generated when single or
 * double clicks are detected.
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   config   pointer to the interrupt generator configuration
 * @return           true on success, false on error
 */
bool lsm303d_set_int_click_config (lsm303d_sensor_t* dev,
                                   lsm303d_int_click_config_t* config);

/**
 * @brief   Get the configuration of the click detection interrupt generator
 *
 * Set the configuration for interrupts that are generated when single or
 * double clicks are detected.
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   config   pointer to the interrupt generator configuration
 * @return           true on success, false on error
 */
bool lsm303d_get_int_click_config (lsm303d_sensor_t* dev,
                                   lsm303d_int_click_config_t* config);


/**
 * @brief   Get the source of the click detection interrupt on signal INT1/INT2
 *
 * Returns a byte with flags that indicate the activity which triggered
 * the interrupt signal (see CLICK_SRC register in datasheet for details)
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   source   pointer to the interrupt source
 * @return           true on success, false on error
 */
bool lsm303d_get_int_click_source (lsm303d_sensor_t* dev, 
                                   lsm303d_int_click_source_t* source);
                                     

/**
 * @brief   Set signal configuration for INT1 and INT2 signals
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   type     define interrupt signal as pushed/pulled or open drain
 * @return           true on success, false on error
 */
bool lsm303d_config_int_signals (lsm303d_sensor_t* dev,
                                 lsm303d_int_signal_type_t type);

                              
/**
 * @brief   Configure HPF (high pass filter) for acceleration data
 *
 * The function resets implicitly reset the reference by a dummy read.
 * 
 * @param   dev      pointer to the sensor device data structure
 * @param   mode     filter mode
 * @param   data     if true, use filtered data as sensor output
 * @param   click    if true, use filtered data for CLICK function
 * @param   int1     if true, use filtered data for interrupt generator INT1
 * @param   int2     if true, use filtered data for interrupt generator INT2
 * @return           true on success, false on error
 */
bool lsm303d_config_a_hpf (lsm303d_sensor_t* dev, 
                           lsm303d_hpf_mode_t mode,
                           bool data, bool click, bool int1, bool int2);

        
/**
 * @brief   Set HPF (high pass filter) reference for acceleration data
 *
 * Used to set the reference of HPF in reference mode *lsm303d_hpf_reference*.
 * Used to reset the HPF in autoreset mode *lsm303d_hpf_autoreset*.
 * Reference is given as two's complement.
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   x_ref    x reference *lsm303d_hpf_reference* mode, otherwise ignored
 * @param   y_ref    y reference *lsm303d_hpf_reference* mode, otherwise ignored
 * @param   z_ref    z reference *lsm303d_hpf_reference* mode, otherwise ignored
 * @return           true on success, false on error
 */
bool lsm303d_set_a_hpf_ref (lsm303d_sensor_t* dev, 
                            int8_t x_ref, int8_t y_ref, int8_t z_ref);


/**
 * @brief   Get HPF (high pass filter) reference
 *
 * Used to reset the HPF in normal mode *lsm303d_hpf_normal*.
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   x_ref    pointer to variable filled with x reference
 * @param   y_ref    pointer to variable filled with y reference
 * @param   z_ref    pointer to variable filled with z reference
 * @return           true on success, false on error
 */
bool lsm303d_get_a_hpf_ref (lsm303d_sensor_t* dev, 
                            int8_t* x_ref, int8_t* y_ref, int8_t* z_ref);


/**
 * @brief   Set magnetic offset
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   x        magnetic offset for x axis
 * @param   y        magnetic offset for y axis
 * @param   z        magnetic offset for z axis
 * @return           true on success, false on error
 */
bool lsm303d_set_m_offset (lsm303d_sensor_t* dev, 
                           int16_t x, int16_t y, int16_t z);


/**
 * @brief   Get magnetic offset
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   x        magnetic offset for x axis
 * @param   y        magnetic offset for y axis
 * @param   z        magnetic offset for z axis
 * @return           true on success, false on error
 */
bool lsm303d_get_m_offset (lsm303d_sensor_t* dev, 
                           int16_t* x, int16_t* y, int16_t* z);


/**
 * @brief   Enable/Disable temperature sensor
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   enable   if true, temperature sensor is enabled 
 * @return           true on success, false on error
 */
bool lsm303d_enable_temperature (lsm303d_sensor_t* dev, bool enable);


/**
 * @brief   Get temperature
 *
 * @param   dev      pointer to the sensor device data structure
 * @return           temperature in degree
 */
float lsm303d_get_temperature (lsm303d_sensor_t* dev);


// ---- Low level interface functions -----------------------------

/**
 * @brief   Direct write to register
 *
 * PLEASE NOTE: This function should only be used to do something special that
 * is not covered by the high level interface AND if you exactly know what you
 * do and what effects it might have. Please be aware that it might affect the
 * high level interface.
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   reg      address of the first register to be changed
 * @param   data     pointer to the data to be written to the register
 * @param   len      number of bytes to be written to the register
 * @return           true on success, false on error
 */
bool lsm303d_reg_write (lsm303d_sensor_t* dev, 
                        uint8_t reg, uint8_t *data, uint16_t len);

/**
 * @brief   Direct read from register
 *
 * PLEASE NOTE: This function should only be used to do something special that
 * is not covered by the high level interface AND if you exactly know what you
 * do and what effects it might have. Please be aware that it might affect the
 * high level interface.
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   reg      address of the first register to be read
 * @param   data     pointer to the data to be read from the register
 * @param   len      number of bytes to be read from the register
 * @return           true on success, false on error
 */
bool lsm303d_reg_read (lsm303d_sensor_t* dev, 
                       uint8_t reg, uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* __LSM303D_H__ */
