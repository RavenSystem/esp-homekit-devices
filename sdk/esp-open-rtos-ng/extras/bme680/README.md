# Driver for **BME680** digital **environmental sensor**

The driver supports multiple BME680 sensors which are either connected to the SPI or to the same or different I2C interfaces with different addresses.

It is for the usage with the ESP8266 and [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos).
The driver is also working with ESP32 and [ESP-IDF](https://github.com/espressif/esp-idf.git) using a wrapper component for ESP8266 functions, see folder ```components/esp8266_wrapper```, as well as Linux based systems using a wrapper library.

## About the sensor

BME680 is an ultra-low-power environmental sensor that integrates temperature, pressure, humidity and gas sensors in only one unit.

## Communication interfaces

The BME680 sensor can be connected using I2C or SPI.

The I2C interface supports data rates up to 3.4 Mbps. It is possible to connect multiple BME680 sensors with different I2C slave addresses to the same I2C bus or to different I2C buses. Possible I2C slave addresses are 0x76 and 0x77.

The SPI interface allows clock rates up to 10 MHz and the SPI modes '00' (CPOL=CPHA=0) and '11' (CPOL=CPHA=1).

Interface selection is done automatically by the sensor using the SPI CS signal. As long as the CS signal keeps high after power-on reset, the I2C interface is used. Once the CS signal has been pulled down, SPI interface is used until next power-on reset.

## Measurement process

Once the BME680 has been initialized, it can be used for measurements. The BME680 operates in two different modes, the **sleep mode** and the **forced mode**. 

The sensor starts after power-up automatically in the *sleep mode* where it does not perform any measurement and consumes only 0.15 μA. Measurements are only done in *forced mode*. 

**Please note:** There are two further undocumented modes, the *parallel* and the *sequential* mode. They can't be supported by the driver, since it is not clear what they do and how to use them.

#### Measurement cylce

To perform measurements, the BME680 sensor has to be triggered to switch to the **forced mode**. In this mode, it performs exactly one measurement of temperature, pressure, humidity, and gas in that order, the so-called **TPHG measurement cycle**. After the execution of this TPHG measurement cycle, **raw sensor data** become available and the sensor returns automatically back to sleep mode.

Each of the individual measurements can be configured or skipped separately via the sensor settings, see section **Measurement settings**. Dependent on the configuration, the **duration of a TPHG measurement cycle** can vary from some milliseconds up to about 4.5 seconds, especially if gas measurement is enabled.

To avoid the blocking of the user task during measurements, the measurement process is therefore separated into the following steps:

1. Trigger the sensor with function ```bme680_force_measurement``` to switch to *forced mode* in which it performs exactly one THPG measurement cycle.

2. Wait the measurement duration using function ```vTaskDelay``` and the value returned from function ```bme680_get_measurement_duration``` or wait as long as function ```bme680_is_measuring``` returns true.

3. Fetch the results as fixed point values with function ```bme680_get_results_fixed``` or as floating point values with function ```bme680_get_results_float```.

```
...
// as long as sensor configuration isn't changed, the duration can be considered as constant
uint32_t duration = bme680_get_measurement_duration(sensor);
...
if (bme680_force_measurement (sensor)) // STEP 1
{
    // STEP 2: passive waiting until measurement results are available
    vTaskDelay (duration);

    // STEP 3: get the results and do something with them
    if (bme680_get_results_float (sensor, &values))
        ...
}
...
```
Alternatively, busy waiting can be realized using function ```bme680_is_measuring```.
```
...
if (bme680_force_measurement (sensor)) // STEP 1
{
    // STEP 2: busy waiting until measurement results are available
    while (bme680_is_measuring (sensor)) ;

    // STEP 3: get the results and do something with them
    if (bme680_get_results_float (sensor, &values))
        ...
}
...
```

For convenience, it is also possible to use the high-level functions ```bme680_measure_float``` or ```bme680_measure_fixed```. These functions combine all 3 steps above within a single function and are therefore very easy to use. **Please note** that these functions must not be used when they are called from a software timer callback function since the calling task is delayed using function *vTaskDelay*.

```
...
// ONE STEP: measure, wait, get the results and do something with them
if (bme680_measure_float (sensor, &values))
    ...
...
```

#### Measurement results

Once the sensor has finished the measurement raw data are available at the sensor. Either function ```bme680_get_results_fixed``` or function ```bme680_get_results_float``` can be used to fetch the results. Both functions read raw data from the sensor and converts them into utilizable fixed point or floating point sensor values. 

**Please note:** Conversion of raw sensor data into the final sensor values is based on very complex calculations that use a large number of calibration parameters. Therefore, the driver does not provide functions that only return the raw sensor data.

Dependent on sensor value representation, measurement results contain different dimensions:

| Value | Fixed Point | Floating Point | Conversion
| ----------- | ------------- | -------- |-----------
| temperature | 1/100 °C | °C | float = fixed / 100
| pressure | Pascal | hPascal | float = fixed / 100
| humidity | 1/1000 % | % | float = fixed / 1000
| gas_resistance | Ohm | Ohm | float = fixed

The gas resistance value in Ohm represents the resistance of sensor's gas sensitive layer.

If the TPHG measurement cycle or fetching the results fails, invalid sensor values are returned:

| Invalid Value | Fixed Point | Floating Point |
| ----------- | ------------- | -------- |
| temperature | INT16_MIN | -327.68 |
| pressure | 0 | 0.0 |
| humidity | 0 | 0.0 |
| gas_resistance | 0 | 0.0 |


## Measurement settings

The sensor allows to change a lot of measurement parameters.

#### Oversampling rates

To increase the resolution of raw sensor data, the sensor supports oversampling for temperature,  pressure, and humidity measurements. Using function ```bme680_set_oversampling_rates```, individual **oversampling rates** can be defined for these measurements. With an oversampling rate *osr*, the resolution of the according raw sensor data can be increased from 16 bit to 16+ld(*osr*) bit. 

Possible oversampling rates are 1x (default by the driver) 2x, 4x, 8x and 16x. It is also possible to define an oversampling rate of 0. This **deactivates** the corresponding measurement and the output values become invalid.

```
...
// Changes the oversampling rate for temperature to 4x and for pressure to 2x. Humidity measurement is skipped.
bme680_set_oversampling_rates(sensor, osr_4x, osr_2x, osr_none);
...
```

#### IIR Filter

The sensor also integrates an internal IIR filter (low pass filter) to reduce short-term changes in sensor output values caused by external disturbances. It effectively reduces the bandwidth of the sensor output values.

The filter can optionally be used for pressure and temperature data that are subject to many short-term changes. With the IIR filter the resolution of pressure and temperature data increases to 20 bit. Humidity and gas inside the sensor does not fluctuate rapidly and does not require such a low pass filtering.

Using function ```bme680_set_filter_size```, the user task can change the **size of the filter**. The default size is 3. If the size of the filter becomes 0, the filter is **not used**.

```
...
// Change the IIR filter size for temperature and pressure to 7.
bme680_set_filter_size(sensor, iir_size_7);
...
// Don't use IIR filter
bme680_set_filter_size(sensor, iir_size_0);
...
```

#### Heater profile

For the gas measurement, the sensor integrates a heater. Parameters for this heater are defined by **heater profiles**. The sensor supports up to 10 such heater profiles, which are numbered from 0 to 9. Each profile consists of a temperature set-point (the target temperature) and a heating duration. By default, only the heater profile 0 with 320 degree Celsius as target temperature and 150 ms heating duration is defined.

**Please note:** According to the data sheet, target temperatures between 200 and 400 degrees Celsius are typical and about 20 to 30 ms are necessary for the heater to reach the desired target temperature.

Function ```bme680_set_heater_profile``` can be used to set the parameters for one of the heater profiles 0 ... 9. Once the parameters of a heater profile are defined, the gas measurement can be activated with that heater profile using function ```bme680_use_heater_profile```. If -1 or ```BME680_HEATER_NOT_USED``` is used as heater profile, gas measurement is deactivated completely.

```
...
// Change the heater profile 1 to 300 degree Celsius for 100 ms and activate it
bme680_set_heater_profile (sensor, 1, 300, 100);
bme680_use_heater_profile (sensor, 1);
...
// Deactivate gas measurement completely
bme680_use_heater_profile (sensor, BME680_HEATER_NOT_USED);
...

```

If several heater profiles have been defined with function ```bme680_set_heater_profile```, a sequence of gas measurements with different heater parameters can be realized by a sequence of activations of different heater profiles for successive TPHG measurements using function ```bme680_use_heater_profile```.

For example, if there were 5 heater profiles defined with following code during the setup
```
bme680_set_heater_profile (sensor, 0, 200, 100);
bme680_set_heater_profile (sensor, 1, 250, 120);
bme680_set_heater_profile (sensor, 2, 300, 140);
bme680_set_heater_profile (sensor, 3, 350, 160);
bme680_set_heater_profile (sensor, 4, 400, 180);
```

the user task could use them as a sequence like following:

```
...
while (1)
{
    switch (count++ % 5)
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
        vTaskDelay (duration);

        // get the results and do something with them
        if (bme680_get_results_float (sensor, &values))
            ...
    }
    ...
}
...
```

#### Ambient temperature

The heater resistance calculation algorithm takes into account the ambient temperature of the sensor. Using function ```bme680_set_ambient_temperature```, the ambient temperature either determined from the sensor itself or from another temperature sensor can be set.

```
...
bme680_set_ambient_temperature (sensor, ambient);
...
```

## Error Handling

Most driver functions return a simple boolean value to indicate whether its execution was successful or an error happened. In the latter case, the member ```error_code``` of the sensor device data structure is set which indicates what error happened. 

There are two different error levels that are ORed into one single *error_code*, errors in the I2C or SPI communication and errors of the BME680 sensor itself.  To test for a certain error, first you can AND the *error_code* with one of the error masks, ```BME680_INT_ERROR_MASK``` for I2C or SPI errors and ```BME680_DRV_ERROR_MASK``` for other errors. Then you can test the result for a certain error code.

For example, error handling for ```bme680_get_results_float``` could look like:

```
if (bme680_get_results_float (sensor, &values))
{
    // no error happened
    ...
}
else
{
    // error happened

    switch (sensor->error_code & BME680_INT_ERROR_MASK)
    {
        case BME680_I2C_BUSY:        ...
        case BME680_I2C_READ_FAILED: ...
        ...
    }
    switch (sensor->error_code & BME680_DRV_ERROR_MASK)
    {
        case BME680_MEAS_STILL_RUNNING: ...
        case BME680_NO_NEW_DATA:        ...
        ...
    }
}
```

## Usage

First, the hardware configuration has to be established. This can differ dependent on the communication interface and the number of sensors used.

### Hardware configurations

The driver supports multiple BME680 sensors at the same time that are connected either to I2C or SPI. Following figures show some possible hardware configurations.

First figure shows the configuration with only one sensor at I2C bus 0.

```
 +------------------+   +----------+
 | ESP8266 / ESP32  |   | BME680   |
 |                  |   |          |
 |   GPIO 14 (SCL)  ----> SCL      |
 |   GPIO 13 (SDA)  <---> SDA      |
 +------------------+   +----------+
```

Next figure shows the configuration with only one sensor at SPI bus.

```
 +------------------+   +----------+        +-----------------+   +----------+
 | ESP8266 / ESP32  |   | BME680   |        | ESP32           |   | BME680   |
 |                  |   |          |        |                 |   |          |
 |   GPIO 14 (SCK)  ----> SCK      |        |   GPIO 16 (SCK) ----> SCK      |
 |   GPIO 13 (MOSI) ----> SDI      |        |   GPIO 17 (MOSI)----> SDI      |
 |   GPIO 12 (MISO) <---- SDO      |        |   GPIO 18 (MISO)<---- SDO      |
 |   GPIO 2  (CS)   ----> CS       |        |   GPIO 19 (CS)  ----> CS       |
 +------------------+    +---------+        +-----------------+   +----------+
```

**Please note:**

1. Since the system flash memory is connected to SPI bus 0, the sensor has to be connected to SPI bus 1.

2. GPIO15 which is used as CS signal of SPI bus 1 on ESP8266 does not work correctly together with the BME680. Therefore, the user has to specify another GPIO pin as CS signal, e.g., GPIO2.
Next figure shows a possible configuration with two I2C buses. In that case, the sensors can have same or different I2C slave addresses.

```
 +------------------+   +----------+
 | ESP8266 / ESP32  |   | BME680_1 |
 |                  |   |          |
 |   GPIO 14 (SCL)  ----> SCL      |
 |   GPIO 13 (SDA)  <---> SDA      |
 |                  |   +----------+
 |                  |   | BME680_2 |
 |                  |   |          |
 |   GPIO 5  (SCL)  ----> SCL      |
 |   GPIO 4  (SDA)  <---> SDA      |
 +------------------+   +----------+
```

Last figure shows a possible configuration using I2C bus 0 and SPI bus 1 at the same time.
```
 +------------------+   +----------+        +------------------+   +----------+
 | ESP8266          |   | BME680_1 |        | ESP8266          |   | BME680_1 |
 |                  |   |          |        |                  |   |          |
 |   GPIO 5  (SCL)  ----> SCL      |        |   GPIO 5  (SCL)  ----> SCL      |
 |   GPIO 4  (SDA)  <---> SDA      |        |   GPIO 4  (SDA)  <---> SDA      |
 |                  |   +----------+        |                  |   +----------+
 |                  |   | BME680_2 |        |                  |   | BME680_2 |
 |   GPIO 14 (SCK)  ----> SCK      |        |   GPIO 16 (SCK)  ----> SCK      |
 |   GPIO 13 (MOSI) ----> SDI      |        |   GPIO 17 (MOSI) ----> SDI      |
 |   GPIO 12 (MISO) <---- SDO      |        |   GPIO 18 (MISO) <---- SDO      |
 |   GPIO 2  (CS)   ----> CS       |        |   GPIO 19 (CS)   ----> CS       |
 +------------------+    +---------+        +------------------+   +----------+
```

Further configurations are possible, e.g., two sensors that are connected at the same I2C bus with different slave addresses.

### Communication interface settings

Dependent on the hardware configuration, the communication interface settings have to be defined.

```
// define SPI interface for BME680 sensors
#define SPI_BUS         1
#define SPI_CS_GPIO     2   // GPIO 15, the default CS of SPI bus 1, can't be used
#else

// define I2C interface for BME680 sensors
#define I2C_BUS         0
#define I2C_SCL_PIN     14
#define I2C_SDA_PIN     13
#endif

```

### Main programm

If I2C interfaces are used, they have to be initialized first.

```
i2c_init(I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ_100K))
```

SPI interface has not to be initialized explicitly.

Once the I2C interfaces are initialized, function ```bme680_init_sensor``` has to be called for each BME680 sensor in order to initialize the sensor and to check its availability as well as its error state. This function returns a pointer to a sensor device data structure or NULL in case of error.

The parameter *bus* specifies the ID of the I2C or SPI bus to which the sensor is connected.

```
static bme680_sensor_t* sensor;
```

For sensors connected to an I2C interface, a valid I2C slave address has to be defined as parameter *addr*. In that case parameter *cs* is ignored.

```
sensor = bme680_init_sensor (I2C_BUS, BME680_I2C_ADDRESS_2, 0);

```

If parameter *addr* is 0, the sensor is connected to a SPI bus. In that case, parameter *cs* defines the GPIO used as CS signal.

```
sensor = bme680_init_sensor (SPI_BUS, 0, SPI_CS_GPIO);

```

The remaining part of the program is independent on the communication interface.

Optionally, you could wish to set some measurement parameters. For details see the section **Measurement settings** above, the header file of the driver ```bme680.h```, and of course the data sheet of the sensor.

```
if (sensor)
{
    /** -- SENSOR CONFIGURATION PART (optional) --- */

    // Changes the oversampling rates to 4x oversampling for temperature
    // and 2x oversampling for humidity. Pressure measurement is skipped.
    bme680_set_oversampling_rates(sensor, osr_4x, osr_none, osr_2x);

    // Change the IIR filter size for temperature and pressure to 7.
    bme680_set_filter_size(sensor, iir_size_7);

    // Change the heater profile 0 to 200 degree Celsius for 100 ms.
    bme680_set_heater_profile (sensor, 0, 200, 100);
    bme680_use_heater_profile (sensor, 0);

    /** -- TASK CREATION PART --- */

    // must be done last to avoid concurrency situations with the sensor 
    // configuration part

    // Create a task that uses the sensor
    xTaskCreate(user_task, "user_task", TASK_STACK_DEPTH, NULL, 2, NULL);

    ...
}
```

Finally, a user task that uses the sensor has to be created.

**Please note:** To avoid concurrency situations when driver functions are used to access the sensor, for example to read data, the user task must not be created until the sensor configuration is completed.


### User task

BME680 supports only the *forced mode* that performs exactly one measurement. Therefore, the measurement has to be triggered in each cycle. The waiting for measurement results is also required in each cycle, before the results can be fetched.

Thus the user task could look like the following:


```
void user_task(void *pvParameters)
{
    bme680_values_float_t values;

    TickType_t last_wakeup = xTaskGetTickCount();

    // as long as sensor configuration isn't changed, duration is constant
    uint32_t duration = bme680_get_measurement_duration(sensor);

    while (1)
    {
        // trigger the sensor to start one TPHG measurement cycle
        bme680_force_measurement (sensor);

        // passive waiting until measurement results are available
        vTaskDelay (duration);

        // alternatively: busy waiting until measurement results are available
        // while (bme680_is_measuring (sensor)) ;

        // get the results and do something with them
        if (bme680_get_results_float (sensor, &values))
            printf("%.3f BME680 Sensor: %.2f °C, %.2f %%, %.2f hPa, %.2f Ohm\n",
                   (double)sdk_system_get_time()*1e-3,
                   values.temperature, values.humidity,
                   values.pressure, values.gas_resistance);

        // passive waiting until 1 second is over
        vTaskDelayUntil(&last_wakeup, 1000 / portTICK_PERIOD_MS);
    }
}
```

Function ```bme680_force_measurement``` is called inside the task loop to perform exactly one measurement in each cycle.

The task is then delayed using function ```vTaskDelay``` and the value returned from function ```bme680_get_measurement_duration``` or as long as function ```bme680_is_measuring``` returns true.

Since the measurement duration only depends on the current sensor configuration, it changes only when sensor configuration is changed. Therefore, it can be considered as constant as long as the sensor configuration isn't changed and can be determined with function ```bme680_get_measurement_duration``` outside the task loop. If the sensor configuration changes, the function has to be executed again.

Once the measurement results are available, they can be fetched as fixed point oder floating point sensor values using function ```bme680_get_results_float``` and ```bme680_get_results_fixed```, respectively.

## Full Example

```
/* -- use following constants to define the example mode ----------- */

// #define SPI_USED

/* -- includes ----------------------------------------------------- */

#include "bme680.h"

/* -- platform dependent definitions ------------------------------- */

#ifdef ESP_PLATFORM  // ESP32 (ESP-IDF)

// user task stack depth for ESP32
#define TASK_STACK_DEPTH 2048

// SPI interface definitions for ESP32
#define SPI_BUS       HSPI_HOST
#define SPI_SCK_GPIO  16
#define SPI_MOSI_GPIO 17
#define SPI_MISO_GPIO 18
#define SPI_CS_GPIO   19

#else  // ESP8266 (esp-open-rtos)

// user task stack depth for ESP8266
#define TASK_STACK_DEPTH 256

// SPI interface definitions for ESP8266
#define SPI_BUS       1
#define SPI_SCK_GPIO  14
#define SPI_MOSI_GPIO 13
#define SPI_MISO_GPIO 12
#define SPI_CS_GPIO   2   // GPIO 15, the default CS of SPI bus 1, can't be used

#endif  // ESP_PLATFORM

// I2C interface defintions for ESP32 and ESP8266
#define I2C_BUS       0
#define I2C_SCL_PIN   14
#define I2C_SDA_PIN   13
#define I2C_FREQ      I2C_FREQ_100K

/* -- user tasks --------------------------------------------------- */

static bme680_sensor_t* sensor = 0;

/*
 * User task that triggers measurements of sensor every seconds. It uses
 * function *vTaskDelay* to wait for measurement results. Busy wating
 * alternative is shown in comments
 */
void user_task(void *pvParameters)
{
    bme680_values_float_t values;

    TickType_t last_wakeup = xTaskGetTickCount();

    // as long as sensor configuration isn't changed, duration is constant
    uint32_t duration = bme680_get_measurement_duration(sensor);

    while (1)
    {
        // trigger the sensor to start one TPHG measurement cycle
        if (bme680_force_measurement (sensor))
        {
            // passive waiting until measurement results are available
            vTaskDelay (duration);

            // alternatively: busy waiting until measurement results are available
            // while (bme680_is_measuring (sensor)) ;

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

/* -- main program ------------------------------------------------- */

void user_init(void)
{
    // Set UART Parameter.
    uart_set_baud(0, 115200);
    // Give the UART some time to settle
    vTaskDelay(1);
    
    /** -- MANDATORY PART -- */

    #ifdef SPI_USED

    spi_bus_init (SPI_BUS, SPI_SCK_GPIO, SPI_MISO_GPIO, SPI_MOSI_GPIO);

    // init the sensor connected to SPI_BUS with SPI_CS_GPIO as chip select.
    sensor = bme680_init_sensor (SPI_BUS, 0, SPI_CS_GPIO);
    
    #else  // I2C

    // Init all I2C bus interfaces at which BME680 sensors are connected
    i2c_init(I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ);

    // init the sensor with slave address BME680_I2C_ADDRESS_2 connected to I2C_BUS.
    sensor = bme680_init_sensor (I2C_BUS, BME680_I2C_ADDRESS_2, 0);

    #endif  // SPI_USED

    if (sensor)
    {
        /** -- SENSOR CONFIGURATION PART (optional) --- */

        // Changes the oversampling rates to 4x oversampling for temperature
        // and 2x oversampling for humidity. Pressure measurement is skipped.
        bme680_set_oversampling_rates(sensor, osr_4x, osr_none, osr_2x);

        // Change the IIR filter size for temperature and pressure to 7.
        bme680_set_filter_size(sensor, iir_size_7);

        // Change the heater profile 0 to 200 degree Celcius for 100 ms.
        bme680_set_heater_profile (sensor, 0, 200, 100);
        bme680_use_heater_profile (sensor, 0);

        // Set ambient temperature to 10 degree Celsius
        bme680_set_ambient_temperature (sensor, 10);
            
        /** -- TASK CREATION PART --- */

        // must be done last to avoid concurrency situations with the sensor 
        // configuration part

        // Create a task that uses the sensor
        xTaskCreate(user_task, "user_task", TASK_STACK_DEPTH, NULL, 2, NULL);
    }
    else
        printf("Could not initialize BME680 sensor\n");
}
```

