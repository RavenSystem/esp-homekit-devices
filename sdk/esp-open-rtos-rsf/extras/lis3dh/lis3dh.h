/**
 * Driver for LIS3DH 3-axes digital accelerometer connected to I2C or SPI.
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

#ifndef __LIS3DH_H__
#define __LIS3DH_H__

// Uncomment one of the following defines to enable debug output
// #define LIS3DH_DEBUG_LEVEL_1    // only error messages
// #define LIS3DH_DEBUG_LEVEL_2    // debug and error messages

// LIS3DH addresses (also used for LIS2DH, LIS2DH12 and LIS2DE12)
#define LIS3DH_I2C_ADDRESS_1           0x18  // SDO pin is low
#define LIS3DH_I2C_ADDRESS_2           0x19  // SDO pin is high

// LIS3DE addresse (also used for LIS2DE)
#define LIS3DE_I2C_ADDRESS_1           0x28  // SDO pin is low
#define LIS3DE_I2C_ADDRESS_2           0x29  // SDO pin is high

// LIS3DH chip id
#define LIS3DH_CHIP_ID                 0x33  // LIS3DH_REG_WHO_AM_I<7:0>

// Definition of error codes
#define LIS3DH_OK                      0
#define LIS3DH_NOK                     -1

#define LIS3DH_INT_ERROR_MASK          0x000f
#define LIS3DH_DRV_ERROR_MASK          0xfff0

// Error codes for I2C and SPI interfaces ORed with LIS3DH driver error codes
#define LIS3DH_I2C_READ_FAILED         1
#define LIS3DH_I2C_WRITE_FAILED        2
#define LIS3DH_I2C_BUSY                3
#define LIS3DH_SPI_WRITE_FAILED        4
#define LIS3DH_SPI_READ_FAILED         5
#define LIS3DH_SPI_BUFFER_OVERFLOW     6

// LIS3DH driver error codes ORed with error codes for I2C and SPI interfaces
#define LIS3DH_WRONG_CHIP_ID              ( 1 << 8)
#define LIS3DH_WRONG_BANDWIDTH            ( 2 << 8)
#define LIS3DH_GET_RAW_DATA_FAILED        ( 3 << 8)
#define LIS3DH_GET_RAW_DATA_FIFO_FAILED   ( 4 << 8)
#define LIS3DH_WRONG_INT_TYPE             ( 5 << 8)
#define LIS3DH_CONFIG_INT_SIGNALS_FAILED  ( 6 << 8)
#define LIS3DH_CONFIG_INT_FAILED          ( 7 << 8)
#define LIS3DH_INT_SOURCE_FAILED          ( 8 << 8)
#define LIS3DH_CONFIG_HPF_FAILED          ( 9 << 8)
#define LIS3DH_ENABLE_HPF_FAILED          (10 << 8)
#define LIS3DH_CONFIG_CLICK_FAILED        (11 << 8)
#define LIS3DH_CLICK_SOURCE_FAILED        (12 << 8)
#define LIS3DH_GET_ADC_DATA_FAILED        (13 << 8)
#define LIS3DH_SENSOR_IN_BYPASS_MODE      (14 << 8)
#define LIS3DH_SENSOR_IN_FIFO_MODE        (15 << 8)
#define LIS3DH_ODR_TOO_HIGH               (16 << 8)

#include "lis3dh_platform.h"
#include "lis3dh_types.h"

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
 * @param   bus     I2C or SPI bus at which LIS3DH sensor is connected
 * @param   addr    I2C addr of the LIS3DH sensor, 0 for using SPI
 * @param   cs      SPI CS GPIO, ignored for I2C
 * @return          pointer to sensor data structure, or NULL on error
 */
lis3dh_sensor_t* lis3dh_init_sensor (uint8_t bus, uint8_t addr, uint8_t cs);


/**
 * @brief   Set sensor mode
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   odr     sensor output data rate (ODR)
 * @param   res     sensor resolution
 * @param   x       true enable x-axis, false disable x-axis
 * @param   y       true enable y-axis, false disable y-axis
 * @param   z       true enable z-axis, false disable z-axis
 * @return          true on success, false on error
 */
bool lis3dh_set_mode (lis3dh_sensor_t* dev, 
                      lis3dh_odr_mode_t odr, lis3dh_resolution_t res,
                      bool x, bool y, bool z);
                       

/**
 * @brief   Set scale (full scale range)
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   scale   full range scale
 * @return          true on success, false on error
 */
bool lis3dh_set_scale (lis3dh_sensor_t* dev, lis3dh_scale_t scale);
                              
                              
/**
 * @brief   Set FIFO mode
 *
 * FIFO watermark can be used to generate an interrupt when FIFO content
 * exceeds the value. It is ignored in bypass mode. 
 * 
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   mode    FIFO mode
 * @param   thresh  FIFO watermark (ignored in bypass mode)
 * @param   trigger interrupt signal used as trigger (only in Stream-to-FIFO)
 * @return          true on success, false on error
 */
bool lis3dh_set_fifo_mode (lis3dh_sensor_t* dev, lis3dh_fifo_mode_t mode, 
                           uint8_t thresh, lis3dh_int_signal_t trigger);
                            

/**
 * @brief   Test whether new data samples are available
 *
 * @param   dev     pointer to the sensor device data structure
 * @return          true on new data, otherwise false
 */
bool lis3dh_new_data (lis3dh_sensor_t* dev);


/**
 * @brief   Get one sample of sensor data as floating point values (unit g)
 *
 * Function works only in bypass mode and fails in FIFO modes. In FIFO modes,
 * function *lis3dh_get_float_data_fifo* has to be used instead to get data.
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   data    pointer to float data structure filled with g values
 * @return          true on success, false on error
 */
bool lis3dh_get_float_data (lis3dh_sensor_t* dev,
                            lis3dh_float_data_t* data);


/**
 * @brief   Get all samples of sensor data stored in the FIFO (unit g)
 *
 * In bypass mode, it returns only one sensor data sample.
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   data    array of 32 float data structures filled with g values
 * @return          number of data sets read from fifo on success or 0 on error
 */
uint8_t lis3dh_get_float_data_fifo (lis3dh_sensor_t* dev,
                                    lis3dh_float_data_fifo_t data);


/**
 * @brief   Get one sample of raw sensor data as 16 bit two's complements
 *
 * Function works only in bypass mode and fails in FIFO modes. In FIFO modes,
 * function *lis3dh_get_raw_data_fifo* has to be used instead to get data.
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   raw     pointer to raw data structure filled with values
 * @return          true on success, false on error
 */
bool lis3dh_get_raw_data (lis3dh_sensor_t* dev, lis3dh_raw_data_t* raw);


/**
 * @brief   Get all samples of raw sensor data stored in the FIFO
 *
 * In bypass mode, it returns only one raw data sample.
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   raw     array of 32 raw data structures
 * @return          number of data sets read from fifo on success or 0 on error
 */
uint8_t lis3dh_get_raw_data_fifo (lis3dh_sensor_t* dev,
                                  lis3dh_raw_data_fifo_t raw);
                                   

/**
 * @brief   Enable / disable an interrupt on signal INT1 or INT2
 *
 * @param   dev     pointer to the sensor device data structure
 * @param   type    interrupt to be enabled or disabled
 * @param   signal  interrupt signal that is activated for the interrupt
 * @param   value   true to enable or false to disable the interrupt
 * @return          true on success, false on error
 */
bool lis3dh_enable_int (lis3dh_sensor_t* dev, 
                        lis3dh_int_type_t type, 
                        lis3dh_int_signal_t signal, bool value);
                                   

/**
 * @brief   Get the source of data ready and FIFO interrupts on INT1
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   source   pointer to the interrupt source
 * @return           true on success, false on error
 */
bool lis3dh_get_int_data_source (lis3dh_sensor_t* dev, 
                                 lis3dh_int_data_source_t* source);


/**
 * @brief   Set the configuration of an inertial event interrupt generator
 *
 * Inertial interrupt generators produce interrupts when certain inertial event
 * occures (event interrupts), that is, the acceleration of defined axes is
 * higher or lower than a defined threshold and one of the following event is
 * recognized: axis movement / wake up, free fall, 6D/4D orientation detection.
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   config   pointer to the interrupt generator configuration
 * @param   gen      interrupt generator to which the function is applied
 * @return           true on success, false on error
 */
bool lis3dh_set_int_event_config (lis3dh_sensor_t* dev,
                                  lis3dh_int_event_config_t* config,
                                  lis3dh_int_event_gen_t gen);


/**
 * @brief   Get the configuration of an inertial event interrupt generator
 *
 * Inertial interrupt generators produce interrupts when certain inertial event
 * occures (event interrupts), that is, the acceleration of defined axes is
 * higher or lower than a defined threshold and one of the following event is
 * recognized: axis movement / wake up, free fall, 6D/4D orientation detection.
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   config   pointer to the interrupt generator configuration
 * @param   gen      interrupt generator to which the function is applied
 * @return           true on success, false on error
 */
bool lis3dh_get_int_event_config (lis3dh_sensor_t* dev,
                                  lis3dh_int_event_config_t* config,
                                  lis3dh_int_event_gen_t gen);


/**
 * @brief   Get the source of an inertial event interrupt INT1/INT2
 *
 * Returns a byte with flags that indicate the event which triggered
 * the interrupt signal (see INTx_SRC register in datasheet for details)
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   source   pointer to the interrupt source data structure
 * @param   gen      interrupt generator to which the function is applied
 * @return           true on success, false on error
 */
bool lis3dh_get_int_event_source (lis3dh_sensor_t* dev,
                                  lis3dh_int_event_source_t* source,
                                  lis3dh_int_event_gen_t gen);


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
bool lis3dh_set_int_click_config (lis3dh_sensor_t* dev,
                                  lis3dh_int_click_config_t* config);

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
bool lis3dh_get_int_click_config (lis3dh_sensor_t* dev,
                                  lis3dh_int_click_config_t* config);


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
bool lis3dh_get_int_click_source (lis3dh_sensor_t* dev, 
                                  lis3dh_int_click_source_t* source);
                                     

/**
 * @brief   Set signal configuration for INT1 and INT2 signals
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   level    define interrupt signal as low or high active
 * @return           true on success, false on error
 */
bool lis3dh_config_int_signals (lis3dh_sensor_t* dev,
                                lis3dh_int_signal_level_t level);

                              
/**
 * @brief   Config HPF (high pass filter)
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   mode     filter mode
 * @param   cutoff   filter cutoff frequency (depends on ODR) [0 ... 3]
 * @param   data     if true, use filtered data as sensor output
 * @param   click    if true, use filtered data for CLICK function
 * @param   int1     if true, use filtered data for interrupt INT1 generation
 * @param   int2     if true, use filtered data for interrupt INT2 generation
 * @return           true on success, false on error
 */
bool lis3dh_config_hpf (lis3dh_sensor_t* dev, 
                        lis3dh_hpf_mode_t mode,  uint8_t cutoff,
                        bool data, bool click, bool int1, bool int2);

                              
/**
 * @brief   Set HPF (high pass filter) reference
 *
 * Used to set the reference of HPF in reference mode *lis3dh_hpf_reference*.
 * Used to reset the HPF in autoreset mode *lis3dh_hpf_autoreset*.
 * Reference is given as two's complement.
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   ref      reference *lis3dh_hpf_reference* mode, otherwise ignored
 * @return           true on success, false on error
 */
bool lis3dh_set_hpf_ref (lis3dh_sensor_t* dev, int8_t ref);


/**
 * @brief   Get HPF (high pass filter) reference
 *
 * Used to reset the HPF in normal mode *lis3dh_hpf_normal*.
 *
 * @param   dev      pointer to the sensor device data structure
 * @return           HPF reference as two's complement
 */
int8_t lis3dh_get_hpf_ref (lis3dh_sensor_t* dev);

/**
 * @brief   Enable / disable ADC or temperature sensor
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   enable   if true, ADC inputs are enabled 
 * @param   temp     if true, ADC input 3 is the output of temperature sensor
 * @return           true on success, false on error
 */
int8_t lis3dh_enable_adc (lis3dh_sensor_t* dev, bool enable, bool temp);


/**
 * @brief   Get ADC input or temperature
 *
 * @param   dev      pointer to the sensor device data structure
 * @param   adc1     ADC input 1
 * @param   adc2     ADC input 2
 * @param   adc3     ADC input 3 or temperature in degree if enabled
 * @return           true on success, false on error
 */
bool lis3dh_get_adc (lis3dh_sensor_t* dev,
                     uint16_t* adc1, uint16_t* adc2, uint16_t* adc3);


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
bool lis3dh_reg_write (lis3dh_sensor_t* dev, 
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
bool lis3dh_reg_read (lis3dh_sensor_t* dev, 
                      uint8_t reg, uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* __LIS3DH_H__ */
