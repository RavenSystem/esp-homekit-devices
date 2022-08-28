/*
 * This sample code is in the public domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "i2c/i2c.h"
#include "task.h"
#include "tsl4531/tsl4531.h"

/* An example using the TSL4531 light sensor
 * to read and print lux values from a sensor
 * attached to GPIO pin 2 (SCL) and GPIO pin 0 (SDA)
 * Connect 3.3v from the ESP to Vin and GND to GND
 */

#define I2C_BUS (0)
#define SCL_PIN (2)
#define SDA_PIN (0)

void tsl4531MeasurementTask(void *pvParameters)
{
    tsl4531_t lightSensor;
    lightSensor.i2c_dev.bus= I2C_BUS;
    lightSensor.i2c_dev.addr= TSL4531_I2C_ADDR;
    tsl4531_init(&lightSensor);

    tsl4531_set_integration_time(&lightSensor, TSL4531_INTEGRATION_400MS);
    tsl4531_set_power_save_skip(&lightSensor, true);

    uint16_t lux = 0;

    while (1)
    {
        if (tsl4531_read_lux(&lightSensor, &lux))
        {
            printf("Lux: %u\n", lux);
        }
        else
        {
            printf("Could not read data from TSL4531\n");
        }

        // 0.05 second delay
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    i2c_init(I2C_BUS, SCL_PIN, SDA_PIN, I2C_FREQ_100K);

    xTaskCreate(tsl4531MeasurementTask, "tsl4531MeasurementTask", 256, NULL, 2, NULL);
}
