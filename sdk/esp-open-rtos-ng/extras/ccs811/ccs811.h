/*
 * Driver for AMS CCS811 digital gas sensor connected to I2C.
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
 
#ifndef CCS811_DRV_H_
#define CCS811_DRV_H_

// Uncomment one of the following defines to enable debug output
// #define CCS811_DEBUG_LEVEL_1             // only error messages
// #define CCS811_DEBUG_LEVEL_2             // debug and error messages

#include "stdint.h"
#include "stdbool.h"

#include "ccs811_platform.h"

// CCS811 I2C addresses
#define CCS811_I2C_ADDRESS_1      0x5A      // default
#define CCS811_I2C_ADDRESS_2      0x5B

// CCS811 clock streching counter
#define CCS811_I2C_CLOCK_STRETCH  200

// Definition of error codes
#define CCS811_OK                 0
#define CCS811_NOK                -1

#define CCS811_INT_ERROR_MASK     0x000f
#define CCS811_DRV_ERROR_MASK     0xfff0

// Error codes for the I2C interface ORed with CCS811 driver error codes
#define CCS811_I2C_READ_FAILED    1
#define CCS811_I2C_WRITE_FAILED   2
#define CCS811_I2C_BUSY           3

// CCS811 driver error codes ORed with error codes for I2C the interface
#define CCS811_DRV_BOOT_MODE      (1  << 8) // firmware is in boot mode
#define CCS811_DRV_NO_APP         (2  << 8) // no application firmware loaded
#define CCS811_DRV_NO_NEW_DATA    (3  << 8) // no new data samples are ready
#define CCS811_DRV_NO_IAQ_DATA    (4  << 8) // no new data samples are ready
#define CCS811_DRV_HW_ID          (5  << 8) // wrong hardware ID
#define CCS811_DRV_INV_SENS       (6  << 8) // invalid sensor ID
#define CCS811_DRV_WR_REG_INV     (7  << 8) // invalid register addr on write
#define CCS811_DRV_RD_REG_INV     (8  << 8) // invalid register addr on read
#define CCS811_DRV_MM_INV         (9  << 8) // invalid measurement mode
#define CCS811_DRV_MAX_RESIST     (10 << 8) // max sensor resistance reached
#define CCS811_DRV_HEAT_FAULT     (11 << 8) // heater current not in range
#define CCS811_DRV_HEAT_SUPPLY    (12 << 8) // heater voltage not correct
#define CCS811_DRV_WRONG_MODE     (13 << 8) // wrong measurement mode
#define CCS811_DRV_RD_STAT_FAILED (14 << 8) // read status register failed
#define CCS811_DRV_RD_DATA_FAILED (15 << 8) // read sensor data failed
#define CCS811_DRV_APP_START_FAIL (16 << 8) // sensor app start failure
#define CCS811_DRV_WRONG_PARAMS   (17 << 8) // wrong parameters used

// ranges
#define CCS_ECO2_RANGE_MIN 400
#define CCS_ECO2_RANGE_MAX 8192
#define CCS_TVOC_RANGE_MIN 0
#define CCS_TVOC_RANGE_MAX 1187

#ifdef __cplusplus
extern "C"
{
#endif


/**
 * @brief 	CCS811 operation modes
 */
typedef enum {
    ccs811_mode_idle  = 0, // Idle, low current mode
    ccs811_mode_1s    = 1, // Constant Power mode, IAQ values every 1 s
    ccs811_mode_10s   = 2, // Pulse Heating mode, IAQ values every 10 s
    ccs811_mode_60s   = 3, // Low Power Pulse Heating, IAQ values every 60 s
    ccs811_mode_250ms = 4  // Constant Power mode, RAW data every 250 ms
} ccs811_mode_t;


/**
 * @brief 	CCS811 sensor device data structure
 */
typedef struct {

    int           error_code;  // contains the error code of last operation

    uint8_t       bus;         // I2C bus
    uint8_t       addr;        // I2C slave address

    ccs811_mode_t mode;        // operation mode

} ccs811_sensor_t;


/**
 * @brief	Initialize a CCS811 sensor
 *
 * The function initializes the CCS811 sensor and checks its availability.
 *
 * @param  bus       I2C bus at which CCS811 sensor is connected
 * @param  addr      I2C slave address of the CCS811 sensor
 *
 * @return           pointer to sensor data structure, or NULL on error
 */
ccs811_sensor_t* ccs811_init_sensor (uint8_t bus, uint8_t addr);


/**
 * @brief 	Set the operation mode of the sensor
 *
 * The function sets the operating mode of the sensor. If the parameter
 * *mode* is either *ccs811_mode_1s*, *ccs811_mode_10s*, *ccs811_mode_60s*
 * or *ccs811_mode_250ms*, the sensor starts a periodic measurement with
 * the specified period. Function *ccs811_get_results* can then be used at
 * the same rate to get the results.
 *
 * In *ccs811_mode_1s*, *ccs811_mode_10s* and *ccs811_mode_60s*, raw sensor
 * data as well as IAQ values calculated by the  sensor values are available.
 * In *ccs811_mode_250ms*, only raw data are available.
 *
 * In case, parameter mode is *ccs811_mode_idle*, the sensor does not perform
 * any measurements.
 *
 * Please note: Mode timings are subject to typical 2% tolerance due
 * to accuracy of internal sensor clock.
 *
 * Please note: After setting the sensor mode, the sensor needs up to
 * 20 minutes, before accurate readings are generated.
 *
 * Please note: When a sensor operating mode is changed to a new mode with
 * a lower sample rate, e.g., from *ccs811_mode_60s* to *ccs811_mode_1s, it
 * should be placed in *mode_idle* for at least 10 minutes before enabling
 * the new mode.
 *
 * @param  sensor    pointer to the sensor device data structure
 * @param  period    measurement period in ms (default 1000 ms)
 *
 * @return           true on success, false on error
 */
bool ccs811_set_mode (ccs811_sensor_t* sensor, ccs811_mode_t mode);


/**
 * @brief	Get latest IAQ sensor values and/or RAW sensor data
 *
 * The function reads the IAQ sensor values (TVOC and eCO2) and/or the raw
 * sensor data. If some of the results are not needed, the corresponding
 * pointer parameters can be set to NULL.
 *
 * Please note: If the function is called and no new data are available,
 * e.g., due to the sensor mode time tolerance of 2%, the function still
 * returns successfully. In this case, the results of the last measurement
 * are returned and the error code CCS811_DRV_NO_NEW_DATA is set.
 *
 * Please note: In *ccs811_mode_250ms*, only RAW data are available. In
 * that case, the function fails with error_code CCS811_DRV_NO_IAQ_DATA
 * if parameters *iaq_tvoc* and *iaq_eco2* are not NULL.
 * @param  sensor    pointer to the sensor device data structure
 * @param  iaq_tvoc  TVOC total volatile organic compound (0 - 1187 ppb)
 * @param  iaq_eco2  eCO2 equivalent CO2 (400 - 8192 ppm)
 * @param  raw_i     current through the sensor used for measuring (0 - 63 uA)
 * @param  raw_v     voltage across the sensor measured (0 - 1023 = 1.65 V)
 *
 * @return           true on success, false on error
 */
bool ccs811_get_results (ccs811_sensor_t* sensor,
                         uint16_t* iaq_tvoc,
                         uint16_t* iaq_eco2,
                         uint8_t*  raw_i,
                         uint16_t* raw_v);


/**
 * brief    Get the resistance of connected NTC thermistor
 *
 * CCS811 supports an external interface for connecting a negative thermal
 * coefficient thermistor (R_NTC) to provide a cost effective and power
 * efficient means of calculating the local ambient temperature. The sensor
 * measures the voltage V_NTC across the R_NTC as well as the voltage V_REF
 * across a connected reference resistor (R_REF).
 *
 * The function returns the current resistance of R_NTC using the equation
 *
 *          R_NTC = R_REF / V_REF * V_NTC
 *
 * Using the data sheet of the NTC, the ambient temperature can be calculated.
 *
 * @param  sensor     pointer to the sensor device data structure
 * @param  reference  resistance of R_REF in Ohm
 * @return            resistance of R_NTC in Ohm, or 0 on error
 */
uint32_t ccs811_get_ntc_resistance (ccs811_sensor_t* sensor, uint32_t r_ref);


/*
 * @brief   Set environmental data
 *
 * If information about the environment are available from another sensor,
 * they can be used by CCS811 to compensate gas readings due to
 * temperature and humidity changes.
 *
 * @param  sensor       pointer to the sensor device data structure
 * @param  temperature  measured temperature in degree Celsius
 * @param  humidity     measured relative humidity in percent
 *
 * @return               true on success, false on error
 */
bool ccs811_set_environmental_data (ccs811_sensor_t* sensor,
                                    float temperature, float humidity);


/**
 * @brief   Enable or disable data ready interrupt signal *nINT*
 *
 * At the end of each measurement cycle (250ms, 1s, 10s, 60s), CCS811 can
 * optionally trigger an interrupt. The signal *nINT* is driven low as soon
 * as new sensor values are ready to read. It will stop being driven low
 * when sensor data are read with function *ccs811_get_results*.
 *
 * The interrupt is disabled by default.
 *
 * @param  sensor   pointer to the sensor device data structure
 * @param  enabled  if true, the interrupt is enabled, or disabled otherwise
 *
 * @return          true on success, false on error
 */
bool ccs811_enable_interrupt (ccs811_sensor_t* sensor, bool enabled);


/*
 * @brief   Set eCO2 threshold mode for data ready interrupts
 *
 * The user task can choose that the data ready interrupt is not generated
 * every time when new sensor values become ready but only if the eCO2 value
 * moves from the current range (LOW, MEDIUM, or HIGH) into another range by
 * more than a hysteresis value. Hysteresis is used to prevent multiple
 * interrupts close to a threshold.
 *
 *   LOW     below parameter value *low*
 *   MEDIUM  between parameter values *low* and *high*
 *   HIGH    above parameter value *high* is range HIGH.
 *
 * If all parameters have valid values, the function sets the thresholds and
 * enables the data ready interrupt. Using 0 for all parameters disables the
 * interrupt.
 *
 * The interrupt is disabled by default.
 *
 * @param  sensor      pointer to the sensor device data structure
 * @param  low         threshold LOW to MEDIUM  (>  400, default 1500)
 * @param  high        threshold MEDIUM to HIGH (< 8192, default 2500)
 * @param  hysteresis  hysteresis value (default 50)
 *
 * @return             true on success, false on error
 */
bool ccs811_set_eco2_thresholds (ccs811_sensor_t* sensor,
                                 uint16_t low,
                                 uint16_t high,
                                 uint8_t  hysteresis);

/*
 * @brief   Get the current baseline value from sensor
 *
 * The sensor supports automatic baseline correction over a minimum time of
 * 24 hours. Using this function, the current baseline value can be saved
 * before the sensor is powered down. This baseline can then be restored after
 * sensor is powered up again to continue the automatic baseline process.
 *
 * @param  sensor      pointer to the sensor device data structure
 * @return             current baseline value on success, or 0 on error
 */
uint16_t ccs811_get_baseline (ccs811_sensor_t* sensor);


/*
 * @brief   Write a previously stored baseline value to the sensor
 *
 * The sensor supports automatic baseline correction over a minimum time of
 * 24 hours. Using this function, a previously saved baseline value be
 * restored after the sensor is powered up to continue the automatic baseline
 * process.
 *
 * Please note: The baseline must be written after the conditioning period
 * of 20 min after power up.
 *
 * @param  sensor      pointer to the sensor device data structure
 * @param  basline     baseline to be set
 * @return             true on success, false on error
 */
bool ccs811_set_baseline (ccs811_sensor_t* sensor, uint16_t baseline);


#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* CCS811_DRV_H_ */


