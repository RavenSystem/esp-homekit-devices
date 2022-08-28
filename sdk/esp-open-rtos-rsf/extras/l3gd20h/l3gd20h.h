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

#ifndef __L3GD20H_H__
#define __L3GD20H_H__

// Uncomment one of the following defines to enable debug output
// #define L3GD20H_DEBUG_LEVEL_1    // only error messages
// #define L3GD20H_DEBUG_LEVEL_2    // debug and error messages

// L3GD20H addresses
#define L3GD20H_I2C_ADDRESS_1           0x6a  // SDO pin is low
#define L3GD20H_I2C_ADDRESS_2           0x6b  // SDO pin is high

// L3GD20 addresses
#define L3GD20_I2C_ADDRESS_1            0x6a  // SDO pin is low
#define L3GD20_I2C_ADDRESS_2            0x6b  // SDO pin is high

// L3G4200D addresses
#define L3G4200D_I2C_ADDRESS_1          0x68  // SDO pin is low
#define L3G4200D_I2C_ADDRESS_2          0x69  // SDO pin is high

// L3GD20H chip id
#define L3GD20H_CHIP_ID                 0xd7  // L3GD20H_REG_WHO_AM_I<7:0>

// L3GD20 chip id
#define L3GD20_CHIP_ID                  0xd4  // L3GD20H_REG_WHO_AM_I<7:0>

// L3G4200D chip id
#define L3G4200D_CHIP_ID                0xd3  // L3GD20H_REG_WHO_AM_I<7:0>

// Definition of error codes
#define L3GD20H_OK                      0
#define L3GD20H_NOK                     -1

#define L3GD20H_INT_ERROR_MASK          0x000f
#define L3GD20H_DRV_ERROR_MASK          0xfff0

// Error codes for I2C and SPI interfaces ORed with L3GD20H driver error codes
#define L3GD20H_I2C_READ_FAILED         1
#define L3GD20H_I2C_WRITE_FAILED        2
#define L3GD20H_I2C_BUSY                3
#define L3GD20H_SPI_WRITE_FAILED        4
#define L3GD20H_SPI_READ_FAILED         5
#define L3GD20H_SPI_BUFFER_OVERFLOW     6
#define L3GD20H_SPI_SET_PAGE_FAILED     7

// L3GD20H driver error codes ORed with error codes for I2C and SPI interfaces
#define L3GD20H_WRONG_CHIP_ID              ( 1 << 8)
#define L3GD20H_WRONG_BANDWIDTH            ( 2 << 8)
#define L3GD20H_GET_RAW_DATA_FAILED        ( 3 << 8)
#define L3GD20H_GET_RAW_DATA_FIFO_FAILED   ( 4 << 8)
#define L3GD20H_WRONG_INT_TYPE             ( 5 << 8)
#define L3GD20H_CONFIG_INT_SIGNALS_FAILED  ( 6 << 8)
#define L3GD20H_CONFIG_INT1_FAILED         ( 7 << 8)
#define L3GD20H_CONFIG_INT2_FAILED         ( 8 << 8)
#define L3GD20H_INT1_SOURCE_FAILED         ( 9 << 8)
#define L3GD20H_INT2_SOURCE_FAILED         (10 << 8)
#define L3GD20H_SEL_OUT_FILTER_FAILED      (11 << 8)
#define L3GD20H_CONFIG_HPF_FAILED          (12 << 8)
#define L3GD20H_ENABLE_HPF_FAILED          (13 << 8)
#define L3GD20H_SENSOR_IN_BYPASS_MODE      (14 << 8)
#define L3GD20H_SENSOR_IN_FIFO_MODE        (15 << 8)
#define L3GD20H_ODR_TOO_HIGH               (16 << 8)
#define L3GD20H_ODR_NOT_AVAILABLE          (17 << 8)
#define L3GD20H_FIFO_MODE_NOT_AVAILABLE    (18 << 8)

#include "l3gd20h_platform.h"
#include "l3gd20h_types.h"

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
 * @param   bus     I2C or SPI bus at which L3GD20H sensor is connected
 * @param   addr    I2C addr of the L3GD20H sensor, 0 for using SPI
 * @param   cs      SPI CS GPIO, ignored for I2C
 * @return          pointer to sensor data structure, or NULL on error
 */
l3gd20h_sensor_t* l3gd20h_init_sensor (uint8_t bus, uint8_t addr, uint8_t cs);


/**
 * @brief   Set sensor mode
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   mode    sensor mode with certain output data rate
 * @param   bw      bandwidth
 * @param   x       true enable x-axis, false disable x-axis
 * @param   y       true enable y-axis, false disable y-axis
 * @param   z       true enable z-axis, false disable z-axis
 * @return          true on success, false on error
 */
bool l3gd20h_set_mode (l3gd20h_sensor_t* dev, l3gd20h_mode_t mode, uint8_t bw,
                       bool x, bool y, bool z);
                       

/**
 * @brief   Set scale (full range range)
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   scale   range setting
 * @return          true on success, false on error
 */
bool l3gd20h_set_scale (l3gd20h_sensor_t* dev, l3gd20h_scale_t sens);
                              
                              
/**
 * @brief   Set FIFO mode
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   mode    FIFO mode
 * @param   thresh  FIFO watermark (ignored in bypass mode)
 * @return          true on success, false on error
 */
bool l3gd20h_set_fifo_mode (l3gd20h_sensor_t* dev, 
                            l3gd20h_fifo_mode_t mode, uint8_t thresh);
                            

/**
 * @brief   Filter selection for raw data output values
 *
 * High pass filter (HPF) is configured with function *l3gd20h_config_hpf*. If
 * HPF is selected, it is enabled implicitly.
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   filter   selected filters for output values
 * @return           true on success, false on error
 */
bool l3gd20h_select_output_filter (l3gd20h_sensor_t* dev,
                                   l3gd20h_filter_t filter);


/**
 * @brief   Test whether new sets of data are available
 *
 * @param   dev     pointer to the sensor device data structure
 * @return          true on new data, otherwise false
 */
bool l3gd20h_new_data (l3gd20h_sensor_t* dev);


/**
 * @brief   Get one sample of floating point sensor data (unit degree)
 *
 * Function works only in bypass mode and fails in FIFO modes. In FIFO modes,
 * function *l3gd20h_get_float_data_fifo* has to be used instead to get data.
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   data    pointer to float data structure filled with values
 * @return          true on success, false on error
 */
bool l3gd20h_get_float_data (l3gd20h_sensor_t* dev,
                             l3gd20h_float_data_t* data);


/**
 * @brief   Get all samples of sensor data stored in the FIFO (unit dps)
 *
 * In bypass mode, it returns only one sensor data sample.
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   data    array of 32 float data structures
 * @return          number of data sets read from fifo on success or 0 on error
 */
uint8_t l3gd20h_get_float_data_fifo (l3gd20h_sensor_t* dev,
                                     l3gd20h_float_data_fifo_t data);


/**
 * @brief   Get one sample of raw sensor data as 16 bit two's complements
 *
 * Function works only in bypass mode and fails in FIFO modes. In FIFO modes,
 * function *l3gd20h_get_raw_data_fifo* has to be used instead to get data.
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   raw     pointer to raw data structure filled with values
 * @return          true on success, false on error
 */
bool l3gd20h_get_raw_data (l3gd20h_sensor_t* dev,
                           l3gd20h_raw_data_t* raw);


/**
 * @brief   Get all samples of raw sensor data stored in the FIFO
 *
 * In bypass mode, it returns only one raw data sample.
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   raw     array of 32 raw data structures
 * @return          number of data sets read from fifo on success or 0 on error
 */
uint8_t l3gd20h_get_raw_data_fifo (l3gd20h_sensor_t* dev,
                                   l3gd20h_raw_data_fifo_t raw);
                                   

/**
 * @brief   Enable / disable data or event interrupts on signal INT1/INT2
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   type     type of the interrupt to be enabled/disabled
 * @param   value    true to enable or false to disable the interrupt
 * @return           true on success, false on error
 */
bool l3gd20h_enable_int (l3gd20h_sensor_t* dev, 
                         l3gd20h_int_types_t type, bool value);
                                   


/**
 * @brief   Set the configuration of the event interrupt generator
 *
 * The event interrupt generator produces interrupts (axis movement and wake up)
 * on signal INT1 whenever the angular rate of one or more axes becomes higher
 * or lower than defined thresholds.
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   config   pointer to the interrupt generator configuration
 * @return           true on success, false on error
 */
bool l3gd20h_set_int_event_config (l3gd20h_sensor_t* dev, 
                                   l3gd20h_int_event_config_t* config);


/**
 * @brief   Get the configuration of the event interrupt generator
 *
 * The event interrupt generator produces interrupts (axis movement and wake up)
 * on signal INT1 whenever the angular rate of one or more axes becomes higher
 * or lower than defined thresholds.
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   config   pointer to the interrupt generator configuration
 * @return           true on success, false on error
 */
bool l3gd20h_get_int_event_config (l3gd20h_sensor_t* dev, 
                                   l3gd20h_int_event_config_t* config);


/**
 * @brief   Get the source of an event interrupt (axis movement and wake up)
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   type     pointer to the interrupt source
 * @return           true on success, false on error
 */
bool l3gd20h_get_int_event_source (l3gd20h_sensor_t* dev, 
                                   l3gd20h_int_event_source_t* source);


/**
 * @brief   Get the source of a data interrupt (data ready or FIFO status)
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   source   pointer to the interrupt source
 * @return           true on success, false on error
 */
bool l3gd20h_get_int_data_source (l3gd20h_sensor_t* dev, 
                                  l3gd20h_int_data_source_t* source);


/**
 * @brief   Set signal configuration for INT1 and INT2 signals
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   type     define interrupt signal as pushed/pulled or open drain
 * @return           true on success, false on error
 */
bool l3gd20h_config_int_signals (l3gd20h_sensor_t* dev,
                                 l3gd20h_signal_type_t type,
                                 l3gd20h_signal_level_t level);

                              
/**
 * @brief   Config HPF (high pass filter)
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   mode     high pass filter mode
 * @param   cutoff   cutoff frequency (depends on output data rate) [0 ... 15]
 * @return           true on success, false on error
 */
bool l3gd20h_config_hpf (l3gd20h_sensor_t* dev, 
                         l3gd20h_hpf_mode_t mode, uint8_t cutoff);


/**
 * @brief   Set HPF (high pass filter) reference
 *
 * Used to set the reference of HPF in reference mode *l3gd20h_hpf_reference*.
 * Used to reset the HPF in autoreset mode *l3gd20h_hpf_autoreset*. 
 * Reference is given as two's complement.
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   ref      reference *l3gd20h_hpf_reference* mode, otherwise ignored
 * @return           true on success, false on error
 */
bool l3gd20h_set_hpf_ref (l3gd20h_sensor_t* dev, int8_t ref);


/**
 * @brief   Get HPF (high pass filter) reference
 *
 * Used to reset the HPF in normal mode *l3gd20h_hpf_normal*.
 *
 * @param   dev      pointer to the sensor device data structure
 * @return           HPF reference as two's complement
 */
int8_t l3gd20h_get_hpf_ref (l3gd20h_sensor_t* dev);


/**
 * @brief   Get temperature
 *
 * @param   dev      pointer to the sensor device data structure
 * @return           temperature in degree as two's complement
 */
int8_t l3gd20h_get_temperature (l3gd20h_sensor_t* dev);


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
bool l3gd20h_reg_write (l3gd20h_sensor_t* dev, 
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
bool l3gd20h_reg_read (l3gd20h_sensor_t* dev, 
                       uint8_t reg, uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* __L3GD20H_H__ */
