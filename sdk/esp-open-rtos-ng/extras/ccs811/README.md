# Driver for the ams CCS811 digital gas sensor for monitoring indoor air quality.

The driver is for the usage with the ESP8266 and [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos). 

It is also working with ESP32 and [ESP-IDF](https://github.com/espressif/esp-idf.git) using a wrapper component for ESP8266 functions, see folder ```components/esp8266_wrapper```, as well as Linux based systems using a wrapper library.

## About the sensor

The CCS811 is an ultra-low power digital sensor which detects **Volatile Organic Compounds (VOC)** for Indoor Air Quality (IAQ) monitoring that. The sensor allows to

- convert raw sensor data to Total Volatile Organic Compound (TVOC) and equivalent CO2 (eCO2),
- compensate gas readings due to temperature and humidity using an external sensor,
- trigger interrupts when new measurement results are available or eCO2 value exceeds thresholds,
- correct baseline automatically or manually
- connect a NTC thermistor to provide means of calculating the local ambient temperature.

The sensor uses an I2C interface and supports clock stretching. See the notes on clock stretching during I2C interface intialization.

## Measurement Process

### Sensor modes

The CCS811 can operate in 5 different modes:

Mode | Driver symbol | Period | RAW data | IAQ values
---- | ------------- | ------ |:--------:|:----------:
Idle, Low Current Mode | ``` ccs811_mode_idle  ``` | -  | - | -
Constant Power Mode | ``` ccs811_mode_1s    ``` | 1 s | X | X
Pulse Heating Mode | ``` ccs811_mode_10s   ``` | 10 s | X | X
Low Power Pulse Heating Mode | ``` ccs811_mode_60s   ``` | 60 s | X | X
Constant Power Mode | ``` ccs811_mode_250ms ``` | 250 ms | X | -

After power up, the sensor starts automatically in *Idle, Low Current Mode* (```mode_idle```). To start periodic measurements, the mode of the sensor has to be changed to any measurement mode. Measurement modes with with different rates of periodic measurements are available, see table above.

**Please note:** In *Constant Power Mode* with measurements every 250 ms (```mode_250ms```) only raw data are available. In all other measurement modes, the Indoor Air Quality (IAQ) values are available additionally. The *Constant Power Mode* with measurements every 250 ms (```mode_250ms```) is only intended for systems where an external host system wants to run an algorithm with raw data.

Once the sensor is initialized with function ```ccs811_init_sensor```, function ```ccs811_set_mode``` can be used to start periodic measurements with a given period.

```
static ccs811_sensor_t* sensor;
...
if ((sensor = ccs811_init_sensor (I2C_BUS, CCS811_I2C_ADDRESS_1)))
{
   ...
   // start periodic measurement with one measurement per second
   ccs811_set_mode (sensor, ccs811_mode_1s);
}
...
```

**Please note:**

1. After setting the mode, the sensor is in conditioning period that needs up to 20 minutes, before accurate readings are generated, see the data sheet for more details.

2. During the early-live (burn-in) period, the CCS811 sensor should run for 48 hours in the selected mode of operation to ensure sensor performance is stable, see the data sheet for more details.

3. When the sensor operating mode is changed to a new mode with a lower sample rate, e.g., from *Pulse Heating Mode* (```mode_10s```) to *Low Power Pulse Heating Mode* (```mode_60s```), it should be placed in *Idle, Low Current Mode* (```mode_idle```) for at least 10 minutes before enabling the new mode.

When a sensor operating mode is changed to a new mode with a higher sample rate, e.g., from *Low Power Pulse Heating Mode* (```mode_60s```) to *Pulse Heating Mode* (```mode_10s```), there is no requirement to wait before enabling the new mode.

## Measurement results

Once the measurement mode is set, the user task can use function ```ccs811_get_results``` with same rate as the measurement rate to fetch the results. The function returns **raw data** as well as **Indoor Air Quality (IAQ)** values.

While raw data represents simply the current through the sensor and the voltage across the sensor with the selected current, IAQ values are the results of the processing these raw data by the sensor. IAQ values consist of the **equivalent CO2 (eCO2)** with a range from 400 ppm to 8192 ppm and **Total Volatile Organic Compound (TVOC)** with a range from 0 ppb to 1187 ppb.

```
uint16_t iaq_tvoc;
uint16_t iaq_eco2;
uint8_t  raw_i;
uint16_t raw_v;
...
// get the results and do something with them
if (ccs811_get_results (sensor, &tvoc, &eco2, &raw_i, &raw_v))
{
    ...
}
...
```

If some of the results are not needed, the corresponding pointer parameters can be set to NULL.

If the function ```ccs811_get_results``` is called and no new data are available, e.g., due to the sensor mode time tolerance of 2%, the function still returns successfully. In this case, the results of the last measurement are returned and the error code CCS811_DRV_NO_NEW_DATA is set.

**Please note:**

1. In *Constant Power Mode* with measurements every 250 ms (```mode_250ms```) only raw data are available.

2. The rate of fetching data must not be greater than the rate of measurement. Due to the sensor mode timing tolerance of 2 %, the rate of fetching data should be lower than the measurement rate.

3. If the function is called and no new data are available, the results of the latest measurement are returned and error_code CCS811_DRV_NO_NEW_DATA is set.

### Compensation

If information about the environment like temperature and humidity are available from another sensor, they can be used by CCS811 to compensate gas readings due to temperature and humidity changes. Function ```ccs811_set_environmental_data``` can be used to set these environmental data.

```
float    temperature;
float    humidity;
...
if (sht3x_get_results (sht3x, &temperature, &humidity))
    // set CCS811 environmental data with values fetched from SHT3x
    ccs811_set_environmental_data (ccs811, temperature, humidity);
...
```

### NTC

CCS811 supports an external interface for connecting a negative thermal coefficient thermistor (R_NTC) to provide a cost effective and power efficient means of calculating the local ambient temperature. The sensor measures the voltage V_NTC across R_NTC as well as the voltage V_REF across a connected reference resistor (R_REF). Function ```ccs811_get_ntc_resistance``` can be used to fetch the current resistance of R_NTC. It uses the resistance of R_REF and measured voltages V_REF and V_NTV with the following equation:

          R_NTC = R_REF / V_REF * V_NTC

Using the data sheet of the NTC, the ambient temperature can be calculated. See application note ams AN000372 for more details. For example, with Adafruit CCS811 Air Quality Sensor Breakout the ambienttemperature can be determined as following:

```
...
#define CCS811_R_REF        100000      // resistance of the reference resistor
#define CCS811_R_NTC        10000       // resistance of NTC at a reference temperature
#define CCS811_R_NTC_TEMP   25          // reference temperature for NTC
#define CCS811_BCONSTANT    3380        // B constant

// get NTC resistance
uint32_t r_ntc = ccs811_get_ntc_resistance (sensor, CCS811_R_REF);

// calculation of temperature from application note ams AN000372
double ntc_temp;
ntc_temp  = log((double)r_ntc / CCS811_R_NTC);      // 1
ntc_temp /= CCS811_BCONSTANT;                       // 2
ntc_temp += 1.0 / (CCS811_R_NTC_TEMP + 273.15);     // 3
ntc_temp  = 1.0 / ntc_temp;                         // 4
ntc_temp -= 273.15;                                 // 5
....
```

### Interrupts

CCS811 supports two types of interrupts that can be used to fetch data:

- data ready interrupt (INT_DATA_RDY)
- threshold interrupt (INT_THRESHOLD)

#### Data ready interrupt

At the end of each measurement cycle (every 250 ms, 1 second, 10 seconds, or 60 seconds), CCS811 can optionally trigger an interrupt. The signal *nINT* is driven low as soon as new sensor values are ready to read. It will stop being driven low when sensor data are read with function ```ccs811_get_results```.

The interrupt is disabled by default. It can be enabled with function ```ccs811_enable_interrupt```.

```
...
// enable the data ready interrupt
ccs811_enable_interrupt (sensor, true);
...
```

#### Threshold interrupt

The user task can choose that the data ready interrupt is not generated every time when new sensor values become ready but only if the eCO2 value moves from the current range (LOW, MEDIUM, or HIGH) into another range by more than a hysteresis value. Hysteresis is used to prevent multiple interrupts close to a threshold.

The interrupt is disabled by default and can be enabled with function ```ccs811_set_eco2_thresholds```. The ranges are defined by parameters *low* and *high* as following

  LOW     below parameter value *low*
  MEDIUM  between parameter values *low* and *high*
  HIGH    above parameter value *high* is range HIGH.

If all parameters have valid values, the function sets the thresholds and enables the data ready interrupt. Using 0 for all parameters disables the interrupt.

```
...
// set threshold parameters and enable threshold interrupt mode
ccs811_set_eco2_thresholds (sensor, 600, 1100, 40);
...
```

### Baseline

CCS81 supports automatic baseline correction over a minimum time of 24 hours. Using function ```ccs811_get_baseline```, the current baseline value can be saved before the sensor is powered down. This baseline can then be restored with function ```ccs811_set_baseline``` after sensor is powered up again to continue the automatic baseline process.

## Error Handling

Most driver functions return a simple boolean value to indicate whether its execution was successful or an error happened. In the latter case, the member ```error_code``` of the sensor device data structure is set which indicates what error happened.

There are two different error levels that are ORed into one single *error_code*, errors in the I2C communication and errors of the CCS811 sensor itself.  To test for a certain error, first you can AND the *error_code* with one of the error masks, ```CCS811_I2C_ERROR_MASK``` for I2C errors and ```CCS811_DRV_ERROR_MASK``` for other errors. Then you can test the result for a certain error code.

For example, error handling for ```ccs811_get_results``` could look like:

```
if (ccs811_get_results (sensor, &tvoc, &eco2, &raw_i, &raw_v))
{
    // no error happened
    ...
}
else
{
    // error happened
    switch (sensor->error_code & CCS811_I2C_ERROR_MASK)
    {
        case CCS811_I2C_BUSY:        ...
        case CCS811_I2C_READ_FAILED: ...
        ...
    }
    switch (sensor->error_code & CCS811_DRV_ERROR_MASK)
    {
        case CCS811_WRONG_MODE:      ...
        case CCS811_DRV_NO_IAQ_DATA: ...
        ...
    }
}
```

## Usage

First, the hardware configuration has to be established.

### Hardware configurations

Following figure shows the hardware configuration for ESP8266 and ESP32 if no interrupt is used.

```
  +------------------+       +--------+
  | ESP8266 / ESP32  |       | CCS811 |
  |                  |       |        |
  |    GPIO 14 (SCL) >-------> SCL    |
  |    GPIO 13 (SDA) <-------> SDA    |
  |    GND           --------> /WAKE  |
  +------------------+       +--------+
```

If interrupt signal *nINT* is used to fetch new data, additionally the interrupt pin has to be connected to a GPIO pin.

```
  +------------------+       +--------+
  | ESP8266 / ESP32  |       | CCS811 |
  |                  |       |        |
  |    GPIO 14 (SCL) >-------> SCL    |
  |    GPIO 13 (SDA) <-------> SDA    |
  |    GPIO 5        <-------- /nINT  |
  |    GND           --------> /WAKE  |
  +------------------+       +--------+
```

If CCS811 sensor is used in conjunction with another sensor, e.g., a SHT3x sensor, the hardware configuration looks like following:

```
  +------------------+       +--------+
  | ESP8266 / ESP32  |       | CCS811 |
  |                  |       |        |
  |    GPIO 14 (SCL) >--+----> SCL    |
  |    GPIO 13 (SDA) <--|-+--> SDA    |
  |    GND           ---|-|--> /WAKE  |
  |                  |  | |  +--------+
  |                  |  | |  | SHT3x  |
  |                  |  | |  |        |
  |                  |  +----> SCL    |
  |                  |    +--> SDA    |
  +------------------+       +--------+
```

### Communication interface settings

Dependent on the hardware configuration, the communication interface settings have to be defined.

```
// define I2C interfaces at which CCS811 sensors can be connected
#define I2C_BUS       0
#define I2C_SCL_PIN   14
#define I2C_SDA_PIN   13

// define GPIO for interrupt
#define INT_GPIO      5
```

### Main program

Before using the CCS811 driver, function ```i2c_init``` needs to be called for each I2C interface to setup them.

**Please note:** CCS811 uses clock streching that can be longer than the default I2C clock stretching. Therefore the clock stretching parameter of I2C has to be set to at least ```CCS811_I2C_CLOCK_STRETCH```.

```
...
i2c_init(I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ_100K);
i2c_set_clock_stretch (I2C_BUS, CCS811_I2C_CLOCK_STRETCH);
...
```

Once I2C interfaces to be used are initialized, function ```ccs811_init_sensor``` has to be called for each CCS811 sensor to initialize the sensor and to check its availability as well as its error state. The parameters specify the I2C bus to which it is connected and its I2C slave address.

```
static ccs811_sensor_t* sensor;    // pointer to sensor device data structure
...
if ((sensor = ccs811_init_sensor (I2C_BUS, CCS811_I2C_ADDRESS_1)))
{
    ...
}
...
```

Function ```ccs811_init_sensor``` returns a pointer to the sensor device data structure or NULL in case of error.

If initialization of the sensor was successful, the sensor mode has be set to start periodic measurement. The sensor mode can be changed anytime later.

```
...
// start periodic measurement with one measurement per second
ccs811_set_mode (sensor, ccs811_mode_1s);
...
```

Finally, a user task that uses the sensor has to be created.

```
xTaskCreate(user_task, "user_task", 256, NULL, 2, 0);
```

**Please note:** To avoid concurrency situations when driver functions are used to access the sensor, for example to read data, the user task must not be created until the sensor configuration is completed.

The user task can use different approaches to fetch new data. Either new data are fetched periodically or the interrupt signal *nINT* is used when new data are available or eCO2 value exceeds defined thresholds.

If new data are fetched **periodically** the implementation of the user task is quite simply and could look like following.

```
void user_task(void *pvParameters)
{
    uint16_t tvoc;
    uint16_t eco2;

    TickType_t last_wakeup = xTaskGetTickCount();

    while (1)
    {
        // get the results and do something with them
        if (ccs811_get_results (sensor, &tvoc, &eco2, 0, 0))
            ...
        // passive waiting until 1 second is over
        vTaskDelayUntil(&last_wakeup, 1000 / portTICK_PERIOD_MS);
    }
}
...
```

The user task simply fetches new data with the same rate as the measurements are performed.

**Please note:** The rate of fetching the measurement results must be not greater than the rate of periodic measurements of the sensor, however, it *should be less* to avoid conflicts caused by the timing tolerance of the sensor.

A different approach is to use the **interrupt** *nINT*. This interrupt signal is either triggered every time when new data are available (INT_DATA_RDY) or only whenever eCO2 value exceeds defined thresholds (INT_THRESHOLD). In both cases, the user has to implement an interrupt handler that either fetches the data directly or triggers a task, that is waiting to fetch the data.

```
...
TaskHandle_t nINT_task;

// Interrupt handler which resumes user_task_interrupt on interrupt

void nINT_handler (uint8_t gpio)
{
    xTaskResumeFromISR (nINT_task);
}

// User task that fetches the sensor values.

void user_task_interrupt (void *pvParameters)
{
    uint16_t tvoc;
    uint16_t eco2;

    while (1)
    {
        // task suspends itself and waits to be resumed by interrupt handler
        vTaskSuspend (NULL);

        // after resume get the results and do something with them
        if (ccs811_get_results (sensor, &tvoc, &eco2, 0, 0))
            ...
    }
}
...

xTaskCreate(user_task_interrupt, "user_task_interrupt", 256, NULL, 2, &nINT_task);
...
```

In this example, a task is defined which suspends itself in each cycle to wait for fetching the data. The task is resumed by the interrupt handler.

Finally, the interrupt handler has to be activated for the GPIO which is connected to the interrupt signal. Furthermore, the interrupt has to be enabled in the CCS811 sensor.

Function ```ccs811_enable_interrupt``` enables the interrupt that is triggered whenever new data are available (INT_DATA_RDY).

```
...
// activate the interrupt for INT_GPIO and set the interrupt handler
gpio_set_interrupt(INT_GPIO, GPIO_INTTYPE_EDGE_NEG, nINT_handler);

// enable the data ready interrupt INT_DATA_RDY
ccs811_enable_interrupt (sensor, true);
...
````

Function ```ccs811_set_eco2_thresholds``` enables the interrupt that is triggered whenever eCO2 value exceeds the thresholds (INT_THRESHOLD) defined by parameters.

```
...
// activate the interrupt for INT_GPIO and set the interrupt handler
gpio_set_interrupt(INT_GPIO, GPIO_INTTYPE_EDGE_NEG, nINT_handler);

// set threshold parameters and enable threshold interrupt mode INT_THRESHOLD
ccs811_set_eco2_thresholds (sensor, 600, 1100, 40);
...
````

## Full Example

```
/* -- use following constants to define the example mode ----------- */

// #define INT_DATA_RDY_USED
// #define INT_THRESHOLD_USED

#if defined(INT_DATA_RDY_USED) || defined(INT_THRESHOLD_USED)
#define INT_USED
#endif

/* -- includes ----------------------------------------------------- */

#include "ccs811.h"

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

// interrupt GPIOs defintions for ESP8266 and ESP32
#define nINT_PIN      13

/* -- user tasks --------------------------------------------------- */

static ccs811_sensor_t* sensor;

#ifdef INT_USED
/**
 * In this example, the interrupt *nINT* is used. It is triggered every time
 * new data are available (INT_DATA_RDY_USED) or exceed defined thresholds
 * (INT_THRESHOLD_USED). In this case, the user has to define an interrupt
 * handler that fetches the data directly or triggers a task, that is waiting
 * to fetch the data. In this example, a task is defined which suspends itself
 * in each cycle to wait for fetching the data. The task is resumed by the
 * the interrupt handler.
 */

TaskHandle_t nINT_task;

// User task that fetches the sensor values.

void user_task_interrupt (void *pvParameters)
{
    uint16_t tvoc;
    uint16_t eco2;

    while (1)
    {
        // task suspends itself and waits to be resumed by interrupt handler
        vTaskSuspend (NULL);

        // after resume get the results and do something with them
        if (ccs811_get_results (sensor, &tvoc, &eco2, 0, 0))
            printf("%.3f CCS811 Sensor interrupt: TVOC %d ppb, eCO2 %d ppm\n",
                   (double)sdk_system_get_time()*1e-3, tvoc, eco2);
    }
}

// Interrupt handler which resumes user_task_interrupt on interrupt

static void IRAM nINT_handler(uint8_t gpio)
{
    xTaskResumeFromISR (nINT_task);
}

#else // !INT_USED

/*
 * In this example, user task fetches the sensor values every seconds.
 */

void user_task_periodic(void *pvParameters)
{
    uint16_t tvoc;
    uint16_t eco2;

    TickType_t last_wakeup = xTaskGetTickCount();

    while (1)
    {
        // get environmental data from another sensor and set them
        // ccs811_set_environmental_data (sensor, 25.3, 47.8);

        // get the results and do something with them
        if (ccs811_get_results (sensor, &tvoc, &eco2, 0, 0))
            printf("%.3f CCS811 Sensor periodic: TVOC %d ppb, eCO2 %d ppm\n",
                   (double)sdk_system_get_time()*1e-3, tvoc, eco2);

        // passive waiting until 1 second is over
        vTaskDelayUntil(&last_wakeup, 1000 / portTICK_PERIOD_MS);
    }
}

#endif // INT_USED

/* -- main program ------------------------------------------------- */

void user_init(void)
{
    // Set UART Parameter.
    uart_set_baud(0, 115200);
    // Give the UART some time to settle
    vTaskDelay(1);

    /** -- MANDATORY PART -- */

    // init all I2C bus interfaces at which CCS811 sensors are connected
    i2c_init (I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ);
    
    // longer clock stretching is required for CCS811
    i2c_set_clock_stretch (I2C_BUS, CCS811_I2C_CLOCK_STRETCH);

    // init the sensor with slave address CCS811_I2C_ADDRESS_1 connected I2C_BUS.
    sensor = ccs811_init_sensor (I2C_BUS, CCS811_I2C_ADDRESS_1);

    if (sensor)
    {
        #if !defined (INT_USED)

        // create a periodic task that uses the sensor
        xTaskCreate(user_task_periodic, "user_task_periodic", TASK_STACK_DEPTH, NULL, 2, NULL);

        #else // INT_USED

        // create a task that is resumed by interrupt handler to use the sensor
        xTaskCreate(user_task_interrupt, "user_task_interrupt", TASK_STACK_DEPTH, NULL, 2, &nINT_task);

        // activate the interrupt for nINT_PIN and set the interrupt handler
        gpio_enable(nINT_PIN, GPIO_INPUT);
        gpio_set_interrupt(nINT_PIN, GPIO_INTTYPE_EDGE_NEG, nINT_handler);

        #ifdef INT_DATA_RDY_USED
        // enable the data ready interrupt
        ccs811_enable_interrupt (sensor, true);
        #else // INT_THRESHOLD_USED
        // set threshold parameters and enable threshold interrupt mode
        ccs811_set_eco2_thresholds (sensor, 600, 1100, 40);
        #endif

        #endif  // !defined(INT_USED)

        // start periodic measurement with one measurement per second
        ccs811_set_mode (sensor, ccs811_mode_1s);
    }
    else
        printf("Could not initialize CCS811 sensor\n");
}

```

## Further Examples

See also the example in the examples directory [examples directory](../../examples/ccs811/README.md).
