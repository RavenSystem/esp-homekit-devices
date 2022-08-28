/**
 * Driver for LIS3MDL 3-axes digital magnetometer connected to I2C or SPI.
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

#ifndef __LIS3MDL_H__
#define __LIS3MDL_H__

// Uncomment one of the following defines to enable debug output
// #define LIS3MDL_DEBUG_LEVEL_1    // only error messages
// #define LIS3MDL_DEBUG_LEVEL_2    // debug and error messages

// LIS3MDL addresses
#define LIS3MDL_I2C_ADDRESS_1           0x1c  // SDO pin is low
#define LIS3MDL_I2C_ADDRESS_2           0x1e  // SDO pin is high

// LIS3MDL chip id
#define LIS3MDL_CHIP_ID                 0x3d  // LIS3MDL_REG_WHO_AM_I<7:0>

// Definition of error codes
#define LIS3MDL_OK                      0
#define LIS3MDL_NOK                     -1

#define LIS3MDL_INT_ERROR_MASK          0x000f
#define LIS3MDL_DRV_ERROR_MASK          0xfff0

// Error codes for I2C and SPI interfaces ORed with LIS3MDL driver error codes
#define LIS3MDL_I2C_READ_FAILED         1
#define LIS3MDL_I2C_WRITE_FAILED        2
#define LIS3MDL_I2C_BUSY                3
#define LIS3MDL_SPI_WRITE_FAILED        4
#define LIS3MDL_SPI_READ_FAILED         5
#define LIS3MDL_SPI_BUFFER_OVERFLOW     6

// LIS3MDL driver error codes ORed with error codes for I2C and SPI interfaces
#define LIS3MDL_WRONG_CHIP_ID              ( 1 << 8)
#define LIS3MDL_GET_RAW_DATA_FAILED        ( 2 << 8)
#define LIS3MDL_CONFIG_INT_FAILED          ( 3 << 8)
#define LIS3MDL_INT_SOURCE_FAILED          ( 4 << 8)
#define LIS3MDL_GET_ADC_DATA_FAILED        ( 5 << 8)

#include "lis3mdl_platform.h"
#include "lis3mdl_types.h"

#ifdef __cplusplus
extern "C"
{
#endif


/**
 * @brief   Initialize the sensor
 *
 * Reset the sensor and switch to power down mode. All registers are reset to 
 * default values.
 *
 * @param   bus     I2C or SPI bus at which LIS3MDL sensor is connected
 * @param   addr    I2C addr of the LIS3MDL sensor, 0 for using SPI
 * @param   cs      SPI CS GPIO, ignored for I2C
 * @return          pointer to sensor data structure, or NULL on error
 */
lis3mdl_sensor_t* lis3mdl_init_sensor (uint8_t bus, uint8_t addr, uint8_t cs);


/**
 * @brief   Set sensor operation mode (OM) and output data rate (ODR)
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   mode    sensor operation mode (OM) at output data rate (ODR)
 * @return          true on success, false on error
 */
bool lis3mdl_set_mode (lis3mdl_sensor_t* dev, lis3mdl_mode_t mode);
                       

/**
 * @brief   Set scale (full scale range)
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   scale   full range scale
 * @return          true on success, false on error
 */
bool lis3mdl_set_scale (lis3mdl_sensor_t* dev, lis3mdl_scale_t scale);
                              
                              
/**
 * @brief   Test whether new data samples are available
 *
 * @param   dev     pointer to the sensor device data structure
 * @return          true on new data, otherwise false
 */
bool lis3mdl_new_data (lis3mdl_sensor_t* dev);


/**
 * @brief   Get one sample of sensor data as floating point values (unit Gauss)
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   data    pointer to float data structure filled with g values
 * @return          true on success, false on error
 */
bool lis3mdl_get_float_data (lis3mdl_sensor_t* dev,
                            lis3mdl_float_data_t* data);


/**
 * @brief   Get one sample of raw sensor data as 16 bit two's complements
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   raw     pointer to raw data structure filled with values
 * @return          true on success, false on error
 */
bool lis3mdl_get_raw_data (lis3mdl_sensor_t* dev, lis3mdl_raw_data_t* raw);


/**
 * @brief   Set configuration for threshold interrupt signal INT
 *
 * The function enables the interrupt signal if one of the possible sources
 * is enabled for interrupts.
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   config   configuration for the specified interrupt signal
 * @return           true on success, false on error
 */
bool lis3mdl_set_int_config (lis3mdl_sensor_t* dev,
                             lis3mdl_int_config_t* config);


/**
 * @brief   Get configuration for threshold interrupt signal INT
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   config   configuration for the specified interrupt signal
 * @return           true on success, false on error
 */
bool lis3mdl_get_int_config (lis3mdl_sensor_t* dev, 
                             lis3mdl_int_config_t* config);


/**
 * @brief   Get the source of the threshold interrupt signal INT
 *
 * Returns a byte with flags that indicate the value(s) that triggered
 * the interrupt signal (see INT_SRC register in datasheet for details)
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   source   pointer to the interrupt source
 * @return           true on success, false on error
 */
bool lis3mdl_get_int_source (lis3mdl_sensor_t* dev,
                             lis3mdl_int_source_t* source);

/**
 * @brief   Enable/Disable temperature sensor
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   enable   if true, temperature sensor is enabled 
 * @return           true on success, false on error
 */
bool lis3mdl_enable_temperature (lis3mdl_sensor_t* dev, bool enable);


/**
 * @brief   Get temperature
 *
 * @param   dev      pointer to the sensor device data structure
 * @return           temperature in degree
 */
float lis3mdl_get_temperature (lis3mdl_sensor_t* dev);

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
bool lis3mdl_reg_write (lis3mdl_sensor_t* dev, 
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
bool lis3mdl_reg_read (lis3mdl_sensor_t* dev, 
                       uint8_t reg, uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* __LIS3MDL_H__ */
