/*
 * bmp180.h
 *
 *  Created on: 23.08.2015
 *      Author: fbargste
 */

#ifndef DRIVER_BMP180_H_
#define DRIVER_BMP180_H_

#include "stdint.h"
#include "stdbool.h"

#include "FreeRTOS.h"
#include "queue.h"

#include "i2c/i2c.h"

// Uncomment to enable debug output
//#define BMP180_DEBUG

#define BMP180_DEVICE_ADDRESS     0x77

#define BMP180_TEMPERATURE (1<<0)
#define BMP180_PRESSURE    (1<<1)

#ifdef __cplusplus
extern "C" {
#endif

//
// Create bmp180_types
//

// temperature in °C
typedef float bmp180_temp_t;
// pressure in mPa (To get hPa divide by 100)
typedef uint32_t bmp180_press_t;

// BMP180_Event_Result
typedef struct
{
    uint8_t cmd;
    bmp180_temp_t  temperature;
    bmp180_press_t pressure;
} bmp180_result_t;

// Init bmp180 driver ...
bool bmp180_init(i2c_dev_t *dev);

// Trigger a "complete" measurement (temperature and pressure will be valid when given to "bmp180_informUser)
void bmp180_trigger_measurement(i2c_dev_t *dev, const QueueHandle_t* resultQueue);

// Trigger a "temperature only" measurement (only temperature will be valid when given to "bmp180_informUser)
void bmp180_trigger_temperature_measurement(i2c_dev_t *dev, const QueueHandle_t* resultQueue);

// Trigger a "pressure only" measurement (only pressure will be valid when given to "bmp180_informUser)
void bmp180_trigger_pressure_measurement(i2c_dev_t *dev, const QueueHandle_t* resultQueue);

// Give the user the chance to create it's own handler
extern bool (*bmp180_informUser)(const QueueHandle_t* resultQueue, uint8_t cmd, bmp180_temp_t temperature, bmp180_press_t pressure);

// Calibration constants
typedef struct
{
    int16_t  AC1;
    int16_t  AC2;
    int16_t  AC3;
    uint16_t AC4;
    uint16_t AC5;
    uint16_t AC6;

    int16_t  B1;
    int16_t  B2;

    int16_t  MB;
    int16_t  MC;
    int16_t  MD;
} bmp180_constants_t;

// Returns true if the bmp180 is detected.
bool bmp180_is_available(i2c_dev_t *dev);
// Reads all the internal constants, returning true on success.
bool bmp180_fillInternalConstants(i2c_dev_t *dev, bmp180_constants_t *c);
// Reads an optional temperature and pressure. The over sampling
// setting, oss, may be 0 to 3. Returns true on success.
bool bmp180_measure(i2c_dev_t *dev, bmp180_constants_t *c, int32_t *temperature,
                    uint32_t *pressure, uint8_t oss);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_BMP180_H_ */
