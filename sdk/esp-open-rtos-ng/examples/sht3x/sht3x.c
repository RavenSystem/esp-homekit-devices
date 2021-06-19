/**
 * Simple example with SHT3x sensor. 
 *
 * It shows different user task implementations in *single shot mode* and
 * *periodic mode*. In *single shot* mode either low level or high level
 * functions are used.
 * 
 * Constants SINGLE_SHOT_LOW_LEVEL and SINGLE_SHOT_HIGH_LEVEL controls which
 * task implementation is used.
 *
 * Harware configuration:
 *
 *    +-----------------+     +----------+
 *    | ESP8266 / ESP32 |     | SHT3x    |
 *    |                 |     |          |
 *    |   GPIO 14 (SCL) ------> SCL      |
 *    |   GPIO 13 (SDA) <-----> SDA      |
 *    +-----------------+     +----------+
 */

/* -- use following constants to define the example mode ----------- */

// #define SINGLE_SHOT_LOW_LEVEL
// #define SINGLE_SHOT_HIGH_LEVEL

/* -- includes ----------------------------------------------------- */

#include "sht3x.h"

/* -- platform dependent definitions ------------------------------- */

#ifdef ESP_PLATFORM  // ESP32 (ESP-IDF)

// user task stack depth for ESP32
#define TASK_STACK_DEPTH 2048

#else  // ESP8266 (esp-open-rtos)

// user task stack depth for ESP8266
#define TASK_STACK_DEPTH 256

#endif  // ESP_PLATFORM

// I2C interface defintions for ESP32 and ESP8266
#define I2C_BUS       0
#define I2C_SCL_PIN   14
#define I2C_SDA_PIN   13
#define I2C_FREQ      I2C_FREQ_100K

/* -- user tasks --------------------------------------------------- */

static sht3x_sensor_t* sensor;    // sensor device data structure

#if defined(SINGLE_SHOT_HIGH_LEVEL)
/*
 * User task that triggers a measurement every 5 seconds. Due to power
 * efficiency reasons it uses *single shot* mode. In this example it uses the
 * high level function *sht3x_measure* to perform one measurement in each cycle.
 */
void user_task (void *pvParameters)
{
    float temperature;
    float humidity;

    TickType_t last_wakeup = xTaskGetTickCount();

    while (1) 
    {
        // perform one measurement and do something with the results
        if (sht3x_measure (sensor, &temperature, &humidity))
            printf("%.3f SHT3x Sensor: %.2f °C, %.2f %%\n", 
                   (double)sdk_system_get_time()*1e-3, temperature, humidity);

        // wait until 5 seconds are over
        vTaskDelayUntil(&last_wakeup, 5000 / portTICK_PERIOD_MS);
    }
}

#elif defined(SINGLE_SHOT_LOW_LEVEL)
/*
 * User task that triggers a measurement every 5 seconds. Due to power
 * efficiency reasons it uses *single shot* mode. In this example it starts the
 * measurement, waits for the results and fetches the results using separate
 * functions
 */
void user_task (void *pvParameters)
{
    float temperature;
    float humidity;

    TickType_t last_wakeup = xTaskGetTickCount();

    // get the measurement duration for high repeatability;
    uint8_t duration = sht3x_get_measurement_duration(sht3x_high);
    
    while (1) 
    {
        // Trigger one measurement in single shot mode with high repeatability.
        sht3x_start_measurement (sensor, sht3x_single_shot, sht3x_high);
        
        // Wait until measurement is ready (constant time of at least 30 ms
        // or the duration returned from *sht3x_get_measurement_duration*).
        vTaskDelay (duration);
        
        // retrieve the values and do something with them
        if (sht3x_get_results (sensor, &temperature, &humidity))
            printf("%.3f SHT3x Sensor: %.2f °C, %.2f %%\n", 
                   (double)sdk_system_get_time()*1e-3, temperature, humidity);

        // wait until 5 seconds are over
        vTaskDelayUntil(&last_wakeup, 5000 / portTICK_PERIOD_MS);
    }
}

#else  // PERIODIC MODE
/*
 * User task that fetches latest measurement results of sensor every 2
 * seconds. It starts the SHT3x in periodic mode with 1 measurements per
 * second (*sht3x_periodic_1mps*).
 */
void user_task (void *pvParameters)
{
    float temperature;
    float humidity;

    // Start periodic measurements with 1 measurement per second.
    sht3x_start_measurement (sensor, sht3x_periodic_1mps, sht3x_high);

    // Wait until first measurement is ready (constant time of at least 30 ms
    // or the duration returned from *sht3x_get_measurement_duration*).
    vTaskDelay (sht3x_get_measurement_duration(sht3x_high));

    TickType_t last_wakeup = xTaskGetTickCount();
    
    while (1) 
    {
        // Get the values and do something with them.
        if (sht3x_get_results (sensor, &temperature, &humidity))
            printf("%.3f SHT3x Sensor: %.2f °C, %.2f %%\n", 
                   (double)sdk_system_get_time()*1e-3, temperature, humidity);
                   
        // Wait until 2 seconds (cycle time) are over.
        vTaskDelayUntil(&last_wakeup, 2000 / portTICK_PERIOD_MS);
    }
}
#endif

/* -- main program ------------------------------------------------- */

void user_init(void)
{
    // Set UART Parameter.
    uart_set_baud(0, 115200);
    // Give the UART some time to settle
    vTaskDelay(1);
    
    // Init I2C bus interfaces at which SHT3x sensors are connected
    // (different busses are possible).
    i2c_init(I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ);
    
    // Create the sensors, multiple sensors are possible.
    if ((sensor = sht3x_init_sensor (I2C_BUS, SHT3x_ADDR_2)))
    {
        // Create a user task that uses the sensors.
        xTaskCreate(user_task, "user_task", TASK_STACK_DEPTH, NULL, 2, 0);
    }
    else
        printf("Could not initialize SHT3x sensor\n");

    // That's it.
}
