# Driver for **SHT3x** digital **temperature and humidity sensor**

The driver is for the usage with the ESP8266 and [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos). 

It is also working with ESP32 and [ESP-IDF](https://github.com/espressif/esp-idf.git) using a wrapper component for ESP8266 functions, see folder ```components/esp8266_wrapper```, as well as Linux based systems using a wrapper library.

## About the sensor

SHT3x is a digital temperature and humidity sensor that uses an I2C interface with up to 1 MHz communication speed. It can operate with **three levels of repeatability** (low, medium and high) and in two different modes, the **single shot data acquisition mode** (or short **single shot mode**) and the **periodic data acquisition mode** (or short **periodic mode**).

## Measurement process

Once the SHT3x sensor is initialized, it can be used for measurements. 

### Single shot mode

In **single shot mode**, a measurement command triggers the acquisition of **exactly one data pair**. Each data pair consists of temperature and humidity as 16-bit decimal values. 

Due to the measurement duration of up to 15 ms, the measurement process is separated into steps to avoid blocking the user task during measurements:

1. Trigger the sensor with function ```sht3x_start_measurement``` to perform exactly one single measurement.

2. Wait the measurement duration using function ```vTaskDelay``` until the results are available . Use either a constant duration of at least 30 ms or the duration in RTOS ticks returned from function ```sht3x_get_measurement_duration``` to wait.

3. Fetch the results as floating point sensor values with function ```sht3x_get_results``` or as raw data with function ```sht3x_get_raw_data```.

In the *single shot mode*, the user task has to perform all steps every time new sensor values ​​are needed. 

For convenience a high level function ```sht3x_measure``` that comprises all three steps above in only one function to perform a measurement. This function is the easiest way to use the sensor. It is most suitable for users that don't want to have the control on sensor details.

The advantage of this mode is that the sensor can switch between successive measurements into the sleep mode, which is more energy-efficient. This is particularly useful when the measurement rate is less than 1 measurement per second.

### Periodic mode

In this mode, one issued measurement command yields a stream of data pairs. Each data pair consists again of temperature and humidity as 16-bit decimal values. As soon as the measurement command has been sent to the sensor, it automatically performs measurements **periodically at a rate of 0.5, 1, 2, 4 or 10 measurements per second (mps)**. The data pairs can be fetched with the same rate or a lower rate.

As in *single shot mode*, the measurement process is separated into the following steps:

1. Trigger the sensor with function ```sht3x_start_measurement``` with a given rate to start periodic measurements.

2. Wait the measurement duration using function ```vTaskDelay``` until the first results are available . Use either a constant duration of at least 30 ms or the duration in RTOS ticks returned from function ```sht3x_get_measurement_duration``` to wait.

3. Fetch the results as floating point sensor values with function ```sht3x_get_results``` or as raw data with function ```sht3x_get_raw_data```.

However, in contrast to the *single shot mode*, steps 1 and 2 have to be executed only once. Once the measurement is started, the user task hast can simply fetch data periodically.

**Please note:** The rate of fetching the measurement results must not be greater than the rate of periodic measurements of the sensor. Even more, it *should be less* to avoid conflicts caused by the timing tolerance of the sensor.

## Measurement results

Once new measurement results are available, either function ```sht3x_get_raw_data``` or function ```sht3x_get_results``` can be used to fetch the results. 

Function ```_sht3x_get_raw_data``` fetches only raw sensor data in 16-decimal format, checks the CRC checksums and stores them in an byte array of type ```sht3x_raw_data_t```. The user task then can use them directly or to call function ```sht3x_compute_values``` to compute floating point sensor values from them. 

Function ```sht3x_get_results``` combines function ```sht3x_read_raw_data``` and function 
 ```sht3x_compute_values``` to get the sensor values. This is the preferred approach to get sensor values by the user task.

## Error Handling

Most driver functions return a simple boolean value to indicate whether its execution was successful or an error happened. In the latter case, the member ```error_code``` of the sensor device data structure is set which indicates what error happened. 

There are two different error levels that are ORed into one single ```error_code```, errors in the I2C communication and errors with the SHT3x sensor itself.  To test for a certain error you can AND the *error_code* with one of the error masks, ```SHT3x_I2C_ERROR_MASK``` for I2C errors and ```SHT3x_DRV_ERROR_MASK``` for other errors and then test for a certain error code.


## Repeatability

The SHT3x sensor supports **three levels of repeatability** (low, medium and high). Repeatability is the variation in measurement results taken by the sensor under the same conditions, and in a short period of time. It is a measure for the noise on the physical sensor output. The higher the repeatability the smaller are changes in the output of subsequent measurements.

The repeatability settings influences the measurement duration as well as the power consumption of the sensor. The measurement takes 3 ms with low repeatability, 5 ms with medium repeatability and 13.5 ms with high repeatability. That is, the measurement produces a noticeable delay in execution.

While the sensor measures at the lowest repeatability, the average current consumption is 800 μA. That is, the higher the repeatability level, the longer the measurement takes and the higher the power consumption. The sensor consumes only 0.2 μA in standby mode.

The repeatability used for a measurement is specified as parameter of function ```sht3x_start_measurement```.


## Usage

Before using the SHT3x driver, function ```i2c_init``` needs to be called for each I2C interface to setup them. 

```
#include "sht3x/sht3x.h"
...
#define I2C_BUS       0
#define I2C_SCL_PIN   14
#define I2C_SDA_PIN   13

...
i2c_init(I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ_100K)
...
```

Once I2C interfaces to be used are initialized, function ```sht3x_init_sensor``` has to be called for each SHT3x sensor to initialize the sensor and to check its availability as well as its error state. The parameters specify the I2C bus to which it is connected and its I2C slave address.

```
static sht3x_sensor_t* sensor;    // pointer to sensor device data structure
...
if ((sensor = sht3x_init_sensor (I2C_BUS, SHT3x_ADDR_2)))
{
    ...
}
```

Function ```sht3x_init_sensor``` returns a pointer to the sensor device data structure or NULL in case of error.

Finally, a user task that uses the sensor has to be created.

```
xTaskCreate(user_task, "user_task", 256, NULL, 2, 0);
```

**Please note:** To avoid concurrency situations when driver functions are used to access the sensor, for example to read data, the user task must not be created until the sensor configuration is completed.

In **periodic mode**, the user task has to start the periodic measurement only once at the beginning of the task. After that, it has only to wait for the results of the first measurement. In the task loop itself, it simply fetches the next measurement results in each cycle.

Thus, in this mode the user task could look like the following:

```
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

```

At the beginning of the task, the periodic measurement is started with function ```sht3x_start_measurement``` at high repeatability level and a rate of 1 measurement per second. The task is then delayed with function ```vTaskDelay``` to wait for first measurement results. The duration can be either a constant time of at least 30 ms or the duration returned by ```sht3x_get_measurement_duration```, as in the example. Inside the task loop, simply the measurement results are fetched periodically using function ```sht3x_get_results``` every 2 seconds.

**Please note:** The rate of fetching the measurement results must be not greater than the rate of periodic measurements of the sensor, however, it *should be less* to avoid conflicts caused by the timing tolerance of the sensor.

In **single shot mode**, the measurement has to be triggered 
in each cycle. Also the waiting for measurement results is required in each cylce, before the results can be fetched.

Thus the user task could look like the following:

```
void user_task (void *pvParameters)
{
    float temperature;
    float humidity;

    TickType_t last_wakeup = xTaskGetTickCount();

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
```

In contrast to the *periodic mode*, the function ```sht3x_start_measurement``` is called inside the task loop to start exactly one measurement in each cycle. The task is then also delayed every time using function ```vTaskDelay``` before the results are fetched with function ```sht3x_get_results```.

Alternatively, user task can use the high level function ```sht3x_measure``` that comprises these steps in only one function. This would simplify the user task that would then look like the following:

```
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
```


The code could be extended by an error handling. In the event of an error, most driver functions set the ```error_code``` element of the sensor device data structure. This indicates which error has occurred. Error codes are a combination of I2C communication error codes and SHT3x sensor error codes. To test for a particular error, the *error code* has to be ANDed with one of the error masks ```SHT3x_I2C_ERROR_MASK``` or ```SHT3x_DRV_ERROR_MASK``` and then tested for a certain value.

For example, error handling for ```sht3x_get_results``` could look like:
```

if (!sht3x_get_results (sensor, &values))
{
    // error happened
    
    switch (sensor->error_code & SHT3x_I2C_ERROR_MASK)
    {
        case SHT3x_I2C_BUSY:        ...
        case SHT3x_I2C_READ_FAILED: ...
        ...
    }
    switch (sensor->error_code & SHT3x_DRV_ERROR_MASK)
    {
        case SHT3x_MEAS_NOT_RUNNING:      ...
        case SHT3x_READ_RAW_DATA_FAILED:  ...
        case SHT3x_WRONG_CRC_TEMPERATURE: ...
        ...
    }
}
```

## Full Example 

```
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

    // That's it.
}
```

