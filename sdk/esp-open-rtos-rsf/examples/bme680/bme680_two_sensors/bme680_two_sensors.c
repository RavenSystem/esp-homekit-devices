/**
 * Simple example with two sensors, one sensor connected to I2C bus 0 and
 * one sensor connected to SPI. It defines two different user tasks, one for
 * each sensor. It demonstrates the possible approaches to wait for measurement
 * results, active busy waiting using ```bme680_is_measuring``` and passive
 * waiting using *vTaskDelay*.
 *
 * Harware configuration:
 *
 *   +-------------------------+     +----------+
 *   | ESP8266  I2C Bus 1      |     | BME680_1 |
 *   |          GPIO 5 (SCL)   ------> SCL      |
 *   |          GPIO 4 (SDA)   <-----> SDA      |
 *   |                         |     +----------+
 *   |          SPI Bus 1      |     | BME680_2 |
 *   |          GPIO 14 (SCK)  >-----> SCK      |
 *   |          GPIO 13 (MOSI) >-----> SDI      |
 *   |          GPIO 12 (MISO) <------ SDO      |
 *   |          GPIO 2  (CS)   >-----> CS       |
 *   +-------------------------+     +----------+
 */

#include "espressif/esp_common.h"
#include "esp/uart.h"

#include "FreeRTOS.h"
#include "task.h"

// include communication interface driver
#include "esp/spi.h"
#include "i2c/i2c.h"

// include BME680 driver
#include "bme680/bme680.h"

// define I2C interface for BME680 sensor 1
#define SPI_BUS         1
#define SPI_CS_GPIO     2   // GPIO 15, the default CS of SPI bus 1, can't be used
// define SPI interface for BME680 sensor 2
#define I2C_BUS         1
#define I2C_SCL_PIN     5
#define I2C_SDA_PIN     4

static bme680_sensor_t* sensor1;
static bme680_sensor_t* sensor2;

/*
 * User task that triggers measurements of sensor1 every 5 seconds and
 * uses *vTaskDelay* to wait for measurement results.
 */
void user_task_sensor1(void *pvParameters)
{
    bme680_values_float_t values;

    TickType_t last_wakeup = xTaskGetTickCount();

    uint32_t duration = bme680_get_measurement_duration (sensor1);

    while (1)
    {
        // trigger the sensor to start one TPHG measurement cycle
        if (bme680_force_measurement (sensor1))
        {

            // passive waiting until measurement results are available
            vTaskDelay (duration);

            // get the results and so something with them
            if (bme680_get_results_float (sensor1, &values))
                printf("%.3f BME680 Sensor1: %.2f °C, %.2f %%, %.2f hPa, %.2f Ohm\n",
                       (double)sdk_system_get_time()*1e-3,
                       values.temperature, values.humidity,
                       values.pressure, values.gas_resistance);
        }

        // passive waiting until 5 seconds are over
        vTaskDelayUntil(&last_wakeup, 5000 / portTICK_PERIOD_MS);
    }
}

/*
 * User task that triggers measurements of sensor1 every 2 seconds and
 * uses *bme680_is_measuring* to wait for measurement results.
 */
void user_task_sensor2(void *pvParameters)
{
    bme680_values_float_t values;

    TickType_t last_wakeup = xTaskGetTickCount();

    while (1)
    {
        // trigger the sensor to start one TPHG measurement cycle
        if (bme680_force_measurement (sensor2))
        {
            // busy waiting until measurement results are available
            while (bme680_is_measuring (sensor2)) ;

            // get the results and so something with them
            if (bme680_get_results_float (sensor2, &values))
                printf("%.3f BME680 Sensor2: %.2f °C, %.2f %%, %.2f hPa, %.2f Ohm\n",
                       (double)sdk_system_get_time()*1e-3,
                       values.temperature, values.humidity,
                       values.pressure, values.gas_resistance);
        }

        // passive waiting until 2 seconds are over
        vTaskDelayUntil(&last_wakeup, 2000 / portTICK_PERIOD_MS);
    }
}


void user_init(void)
{
    // Set UART Parameter
    uart_set_baud(0, 115200);
    // Give the UART some time to settle
    sdk_os_delay_us(500);

    /** -- MANDATORY PART -- */

    // Init all I2C bus interfaces at which BME680 sensors are connected
    i2c_init(I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ_100K);

    // Init the sensors
    sensor1 = bme680_init_sensor (I2C_BUS, BME680_I2C_ADDRESS_2, 0);
    sensor2 = bme680_init_sensor (SPI_BUS, 0, SPI_CS_GPIO);

    if (sensor1 && sensor2)
    {
        /** -- SENSOR CONFIGURATION PART (optional) --- */

        // Changes the oversampling rates for both sensor to different values
        bme680_set_oversampling_rates(sensor1, osr_4x, osr_2x, osr_1x);
        bme680_set_oversampling_rates(sensor2, osr_8x, osr_8x, osr_8x);

        // Change the IIR filter size for temperature and and pressure to 7.
        bme680_set_filter_size(sensor1, iir_size_7);
        bme680_set_filter_size(sensor2, iir_size_7);

        // Change the heater profile 0 to 200 degree Celcius for 150 ms.
        bme680_set_heater_profile (sensor1, 0, 200, 150);
        bme680_set_heater_profile (sensor2, 0, 200, 150);

        // Activate the heater profile 0
        bme680_use_heater_profile (sensor1, 0);
        bme680_use_heater_profile (sensor2, 0);

        /** -- TASK CREATION PART --- */

        // must be done last to avoid concurrency situations with the sensor 
        // configuration part

        // Create the tasks that use the sensors
        xTaskCreate(user_task_sensor1, "user_task_sensor1", 256, NULL, 2, 0);
        xTaskCreate(user_task_sensor2, "user_task_sensor2", 256, NULL, 2, 0);
    }
}
