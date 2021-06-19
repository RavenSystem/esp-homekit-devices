/**
 * Simple example with one CCS811 sensor connected to I2C bus 0 and one SHT3x
 * sensor to determine ambient temperature. New data are fetched peridically.
 *
 * Harware configuration:
 *
 * +------------------------+       +--------+
 * | ESP8266  Bus 0         |       | CCS811 |
 * |          GPIO 14 (SCL) ---+----> SCL    |
 * |          GPIO 13 (SDA) <--|-+--> SDA    |
 * |          GND           ---|-|--> /WAKE  |
 * |                        |  | |  +--------+
 * |                        |  | |  | SHT3x  |
 * |                        |  +----> SCL    |
 * |                        |    +--> SDA    |
 * +------------------------+       +--------+
 */

#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "i2c/i2c.h"

#include "FreeRTOS.h"
#include <task.h>

// include CCS811 driver
#include "ccs811/ccs811.h"

// include SHT3x driver
#include "sht3x/sht3x.h"

// define I2C interfaces at which CCS811 and SHT3x sensors are connected
#define I2C_BUS       0
#define I2C_SCL_PIN   14
#define I2C_SDA_PIN   13

static ccs811_sensor_t* ccs811; // CCS811 device data structure
static sht3x_sensor_t*  sht3x;  // SHT3x device data structure

/*
 * User task that fetches the sensor values every 2 seconds.
 */
void user_task(void *pvParameters)
{
    uint16_t tvoc;
    uint16_t eco2;

    float    temperature;
    float    humidity;

    // start periodic measurement with 1 measurement per second
    ccs811_set_mode (ccs811, ccs811_mode_1s);

    // start periodic measurements with 1 measurement per second
    sht3x_start_measurement (sht3x, sht3x_periodic_1mps, sht3x_high);

    // passive waiting until measurement results are available
    vTaskDelay (sht3x_get_measurement_duration (sht3x_high));

    TickType_t last_wakeup = xTaskGetTickCount();

    while (1)
    {
        // get the results from CCS811 and do something with them
        if (ccs811_get_results (ccs811, &tvoc, &eco2, 0, 0))
            printf("%.3f CCS811 Sensor periodic: TVOC %d ppb, eCO2 %d ppm\n",
                   (double)sdk_system_get_time()*1e-3, tvoc, eco2);

        // get the values from SHT3x and do something with them
        if (sht3x_get_results (sht3x, &temperature, &humidity))
        {
            printf("%.3f SHT3x Sensor: %.2f °C, %.2f %%\n",
                   (double)sdk_system_get_time()*1e-3, temperature, humidity);

            // set CCS811 environmental data with values fetched from SHT3x
            ccs811_set_environmental_data (ccs811, temperature, humidity);
        }
        // passive waiting until 2 seconds is over
        vTaskDelayUntil(&last_wakeup, 2000 / portTICK_PERIOD_MS);
    }
}


void user_init(void)
{
    // set UART Parameter
    uart_set_baud(0, 115200);
    // give the UART some time to settle
    sdk_os_delay_us(500);

    /** -- MANDATORY PART -- */

    // init all I2C bus interfaces at which CCS811 sensors are connected
    i2c_init(I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ_100K);

    // longer clock stretching is required for CCS811
    i2c_set_clock_stretch (I2C_BUS, CCS811_I2C_CLOCK_STRETCH);

    // init the sensors
    ccs811 = ccs811_init_sensor (I2C_BUS, CCS811_I2C_ADDRESS_1);
    sht3x  = sht3x_init_sensor  (I2C_BUS, SHT3x_ADDR_2);

    if (ccs811 && sht3x)
        // create a task that uses the sensor
        xTaskCreate(user_task, "user_task", 256, NULL, 2, NULL);
}
