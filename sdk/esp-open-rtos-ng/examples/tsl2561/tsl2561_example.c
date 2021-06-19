/*
 * This sample code is in the public domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "i2c/i2c.h"
#include "task.h"
#include "tsl2561/tsl2561.h"

/* An example using the TSL2561 light sensor
 * to read and print lux values from a sensor
 * attached to GPIO pin 2 (SCL) and GPIO pin 0 (SDA)
 * Connect 3.3v from the ESP to Vin and GND to GND
 */

#define I2C_BUS (0)
#define SCL_PIN (2)
#define SDA_PIN (0)

void tsl2561MeasurementTask(void *pvParameters)
{
    tsl2561_t lightSensor;

    // Options:
    // TSL2561_I2C_ADDR_VCC   (0x49)
    // TSL2561_I2C_ADDR_GND   (0x29)
    // TSL2561_I2C_ADDR_FLOAT (0x39) Default
    lightSensor.i2c_dev.bus = I2C_BUS;
    lightSensor.i2c_dev.addr = TSL2561_I2C_ADDR_FLOAT;

    tsl2561_init(&lightSensor);

    // Options:
    // TSL2561_INTEGRATION_13MS  (0x00)
    // TSL2561_INTEGRATION_101MS (0x01)
    // TSL2561_INTEGRATION_402MS (0x02) Default
    tsl2561_set_integration_time(&lightSensor, TSL2561_INTEGRATION_402MS);

    // Options:
    // TSL2561_GAIN_1X  (0x00) Default
    // TSL2561_GAIN_16X (0x10)
    tsl2561_set_gain(&lightSensor, TSL2561_GAIN_1X);

    uint32_t lux = 0;

    while (1)
    {
        if (tsl2561_read_lux(&lightSensor, &lux))
        {
            printf("Lux: %u\n", lux);
        }
        else
        {
            printf("Could not read data from TSL2561\n");
        }

        // 0.1 second delay
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    i2c_init(I2C_BUS, SCL_PIN, SDA_PIN, I2C_FREQ_100K);

    xTaskCreate(tsl2561MeasurementTask, "tsl2561MeasurementTask", 256, NULL, 2, NULL);
}
