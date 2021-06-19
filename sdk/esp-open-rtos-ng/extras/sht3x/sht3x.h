/*
 * Driver for Sensirion SHT3x digital temperature and humidity sensor
 * connected to I2C
 *
 * This driver is for the usage with the ESP8266 and FreeRTOS (esp-open-rtos)
 * [https://github.com/SuperHouse/esp-open-rtos]. It is also working with ESP32
 * and ESP-IDF [https://github.com/espressif/esp-idf.git] as well as Linux
 * based systems using a wrapper library for ESP8266 functions.
 *
 * ----------------------------------------------------------------
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
 
#ifndef __SHT3x_H__
#define __SHT3x_H__

// Uncomment to enable debug output
// #define SHT3x_DEBUG_LEVEL_1     // only error messages
// #define SHT3x_DEBUG_LEVEL_2     // error and debug messages

#include "stdint.h"
#include "stdbool.h"

#include "sht3x_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

// definition of possible I2C slave addresses
#define SHT3x_ADDR_1 0x44        // ADDR pin connected to GND/VSS (default)
#define SHT3x_ADDR_2 0x45        // ADDR pin connected to VDD

// definition of error codes
#define SHT3x_OK                     0
#define SHT3x_NOK                    -1

#define SHT3x_I2C_ERROR_MASK         0x000f
#define SHT3x_DRV_ERROR_MASK         0xfff0

// error codes for I2C interface ORed with SHT3x error codes
#define SHT3x_I2C_READ_FAILED        1
#define SHT3x_I2C_SEND_CMD_FAILED    2
#define SHT3x_I2C_BUSY               3

// SHT3x driver error codes OR ed with error codes for I2C interface
#define SHT3x_MEAS_NOT_STARTED       (1  << 8)
#define SHT3x_MEAS_ALREADY_RUNNING   (2  << 8)
#define SHT3x_MEAS_STILL_RUNNING     (3  << 8)
#define SHT3x_READ_RAW_DATA_FAILED   (4  << 8)

#define SHT3x_SEND_MEAS_CMD_FAILED   (5  << 8)
#define SHT3x_SEND_RESET_CMD_FAILED  (6  << 8)
#define SHT3x_SEND_STATUS_CMD_FAILED (7  << 8)
#define SHT3x_SEND_FETCH_CMD_FAILED  (8  << 8)

#define SHT3x_WRONG_CRC_TEMPERATURE  (9  << 8)
#define SHT3x_WRONG_CRC_HUMIDITY     (10 << 8)

#define SHT3x_RAW_DATA_SIZE 6

/**
 * @brief	raw data type
 */
typedef uint8_t sht3x_raw_data_t [SHT3x_RAW_DATA_SIZE];


/**
 * @brief   possible measurement modes
 */
typedef enum {
    sht3x_single_shot = 0,  // one single measurement
    sht3x_periodic_05mps,   // periodic with 0.5 measurements per second (mps)
    sht3x_periodic_1mps,    // periodic with   1 measurements per second (mps)
    sht3x_periodic_2mps,    // periodic with   2 measurements per second (mps)
    sht3x_periodic_4mps,    // periodic with   4 measurements per second (mps)
    sht3x_periodic_10mps    // periodic with  10 measurements per second (mps)
} sht3x_mode_t;
    
    
/**
 * @brief   possible repeatability modes
 */
typedef enum {
    sht3x_high = 0,
    sht3x_medium,
    sht3x_low
} sht3x_repeat_t;

/**
 * @brief 	SHT3x sensor device data structure type
 */
typedef struct {

    uint32_t        error_code;      // combined error codes
    
    uint8_t         bus;             // I2C bus at which sensor is connected
    uint8_t         addr;            // I2C slave address of the sensor
    
    sht3x_mode_t    mode;            // used measurement mode
    sht3x_repeat_t  repeatability;   // used repeatability
 
    bool            meas_started;    // indicates whether measurement started
    uint32_t        meas_start_time; // measurement start time in us
    bool            meas_first;      // first measurement in periodic mode
    
} sht3x_sensor_t;    


/**
 * @brief	Initialize a SHT3x sensor
 * 
 * The function creates a data structure describing the sensor and
 * initializes the sensor device.
 *  
 * @param   bus       I2C bus at which the sensor is connected
 * @param   addr      I2C slave address of the sensor
 * @return            pointer to sensor data structure, or NULL on error
 */
sht3x_sensor_t* sht3x_init_sensor (uint8_t bus, uint8_t addr);


/**
 * @brief   High level measurement function
 *
 * For convenience this function comprises all three steps to perform
 * one measurement in only one function:
 *
 * 1. Starts a measurement in single shot mode with high reliability
 * 2. Waits using *vTaskDelay* until measurement results are available 
 * 3. Returns the results in kind of floating point sensor values 
 *
 * This function is the easiest way to use the sensor. It is most suitable
 * for users that don't want to have the control on sensor details.
 *
 * Please note: The function delays the calling task up to 30 ms to wait for
 * the  the measurement results. This might lead to problems when the function
 * is called from a software timer callback function.
 *
 * @param   dev         pointer to sensor device data structure
 * @param   temperature returns temperature in degree Celsius   
 * @param   humidity    returns humidity in percent
 * @return              true on success, false on error
 */
bool sht3x_measure (sht3x_sensor_t* dev, float* temperature, float* humidity);


/**
 * @brief	Start the measurement in single shot or periodic mode
 *
 * The function starts the measurement either in *single shot mode* 
 * (exactly one measurement) or *periodic mode* (periodic measurements)
 * with given repeatabilty.
 *
 * In the *single shot mode*, this function has to be called for each
 * measurement. The measurement duration has to be waited every time
 * before the results can be fetched. 
 *
 * In the *periodic mode*, this function has to be called only once. Also 
 * the measurement duration has to be waited only once until the first
 * results are available. After this first measurement, the sensor then
 * automatically performs all subsequent measurements. The rate of periodic
 * measurements can be 10, 4, 2, 1 or 0.5 measurements per second (mps).
 * 
 * Please note: Due to inaccuracies in timing of the sensor, the user task
 * should fetch the results at a lower rate. The rate of the periodic
 * measurements is defined by the parameter *mode*.
 *
 * @param   dev         pointer to sensor device data structure
 * @param   mode        measurement mode, see type *sht3x_mode_t*
 * @param   repeat      repeatability, see type *sht3x_repeat_t*
 * @return              true on success, false on error
 */
bool sht3x_start_measurement (sht3x_sensor_t* dev, sht3x_mode_t mode,
                              sht3x_repeat_t repeat);

/**
 * @brief   Get the duration of a measurement in RTOS ticks.
 *
 * The function returns the duration in RTOS ticks required by the sensor to
 * perform a measurement for the given repeatability. Once a measurement is
 * started with function *sht3x_start_measurement* the user task can use this
 * duration in RTOS ticks directly to wait with function *vTaskDelay* until
 * the measurement results can be fetched.
 *
 * Please note: The duration only depends on repeatability level. Therefore,
 * it can be considered as constant for a repeatibility.
 *
 * @param   repeat      repeatability, see type *sht3x_repeat_t*
 * @return              measurement duration given in RTOS ticks
 */
uint8_t sht3x_get_measurement_duration (sht3x_repeat_t repeat);


/**
 * @brief	Read measurement results from sensor as raw data
 *
 * The function read measurement results from the sensor, checks the CRC
 * checksum and stores them in the byte array as following.
 *
 *      data[0] = Temperature MSB
 *      data[1] = Temperature LSB
 *      data[2] = Temperature CRC
 *      data[3] = Pressure MSB
 *      data[4] = Pressure LSB 
 *      data[2] = Pressure CRC
 *
 * In case that there are no new data that can be read, the function fails.
 * 
 * @param   dev         pointer to sensor device data structure
 * @param   raw_data    byte array in which raw data are stored 
 * @return              true on success, false on error
 */
bool sht3x_get_raw_data(sht3x_sensor_t* dev, sht3x_raw_data_t raw_data);


/**
 * @brief	Computes sensor values from raw data
 *
 * @param   raw_data    byte array that contains raw data  
 * @param   temperature returns temperature in degree Celsius   
 * @param   humidity    returns humidity in percent
 * @return              true on success, false on error
 */
bool sht3x_compute_values (sht3x_raw_data_t raw_data, 
                           float* temperature, float* humidity);


/**
 * @brief	Get measurement results in form of sensor values
 *
 * The function combines function *sht3x_read_raw_data* and function 
 * *sht3x_compute_values* to get the measurement results.
 * 
 * In case that there are no results that can be read, the function fails.
 *
 * @param   dev         pointer to sensor device data structure
 * @param   temperature returns temperature in degree Celsius   
 * @param   humidity    returns humidity in percent
 * @return              true on success, false on error
 */
bool sht3x_get_results (sht3x_sensor_t* dev, 
                        float* temperature, float* humidity);


#ifdef __cplusplus
}
#endif

#endif /* __SHT3x_H__ */
