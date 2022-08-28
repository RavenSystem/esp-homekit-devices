/**
 * Simple example with one sensor connected to I2C bus 0 and a sequence of
 * heating profiles. The heating profile is changed with each cycle.
 *
 * Harware configuration:
 *
 *   I2C   +-------------------------+     +----------+
 *         | ESP8266  Bus 0          |     | BME680   |
 *         |          GPIO 14 (SCL)  ------> SCL      |
 *         |          GPIO 13 (SDA)  ------- SDA      |
 *         +-------------------------+     +----------+
 */

#include "espressif/esp_common.h"
#include "esp/uart.h"

#include "FreeRTOS.h"
#include "task.h"

// include communication interface driver
#include "i2c/i2c.h"

// include BME680 driver
#include "bme680/bme680.h"

// define I2C interface for BME680 sensors
#define I2C_BUS         0
#define I2C_SCL_PIN     14
#define I2C_SDA_PIN     13

static bme680_sensor_t* sensor;

/*
 * User task that triggers measurements of sensor every seconds. It uses
 * function *vTaskDelay* to wait for measurement results and changes the
 * heating profile in each cycle.
 */
void user_task(void *pvParameters)
{
    bme680_values_float_t values;

    TickType_t last_wakeup = xTaskGetTickCount();

    uint32_t count = 0;

    while (1)
    {
        if (count++ < 60)
            // disable gas measurement for cycle counter < 60
            bme680_use_heater_profile (sensor, BME680_HEATER_NOT_USED);
        else
            // change heating profile in each cycle
            switch (count % 5)
            {
                case 0: bme680_use_heater_profile (sensor, 0); break;
                case 1: bme680_use_heater_profile (sensor, 1); break;
                case 2: bme680_use_heater_profile (sensor, 2); break;
                case 3: bme680_use_heater_profile (sensor, 3); break;
                case 4: bme680_use_heater_profile (sensor, 4); break;
            }

        // measurement duration changes in each cycle
        uint32_t duration = bme680_get_measurement_duration(sensor);

        // trigger the sensor to start one TPHG measurement cycle
        if (bme680_force_measurement (sensor))
        {
            // passive waiting until measurement results are available
            vTaskDelay (duration);

            // get the results and do something with them
            if (bme680_get_results_float (sensor, &values))
                printf("%.3f BME680 Sensor: %.2f °C, %.2f %%, %.2f hPa, %.2f Ohm\n",
                       (double)sdk_system_get_time()*1e-3,
                       values.temperature, values.humidity,
                       values.pressure, values.gas_resistance);
        }
        // passive waiting until 1 second is over
        vTaskDelayUntil(&last_wakeup, 1000 / portTICK_PERIOD_MS);
    }
}


void user_init(void)
{
    // Set UART Parameter
    uart_set_baud(0, 115200);
    // Give the UART some time to settle
    sdk_os_delay_us(500);

    /** -- MANDATORY PART -- */

    #ifdef SPI_USED
    // Init the sensor connected to SPI.
    sensor = bme680_init_sensor (SPI_BUS, 0, SPI_CS_GPIO);
    #else
    // Init all I2C bus interfaces at which BME680 sensors are connected
    i2c_init(I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ_100K);

    // Init the sensor connected to I2C.
    sensor = bme680_init_sensor (I2C_BUS, BME680_I2C_ADDRESS_2, 0);
    #endif

    if (sensor)
    {
        /** -- SENSOR CONFIGURATION PART (optional) --- */

        // Changes the oversampling rates to 4x oversampling for temperature
        // and 2x oversampling for humidity. Pressure measurement is skipped.
        bme680_set_oversampling_rates(sensor, osr_4x, osr_none, osr_2x);

        // Change the IIR filter size for temperature and pressure to 7.
        bme680_set_filter_size(sensor, iir_size_7);

        // Define a number of different heating profiles
        bme680_set_heater_profile (sensor, 0, 200, 100);
        bme680_set_heater_profile (sensor, 1, 250, 120);
        bme680_set_heater_profile (sensor, 2, 300, 140);
        bme680_set_heater_profile (sensor, 3, 350, 160);
        bme680_set_heater_profile (sensor, 4, 400, 180);

        /** -- TASK CREATION PART --- */

        // must be done last to avoid concurrency situations with the sensor 
        // configuration part

        // Create a task that uses the sensor
        xTaskCreate(user_task, "user_task", 256, NULL, 2, NULL);
    }
    else
        printf("Could not initialize BME680 sensor\n");
}
