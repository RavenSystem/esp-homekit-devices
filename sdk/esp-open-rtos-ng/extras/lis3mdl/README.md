# Driver for the LIS3MDL 3-axes digital output magnetometer

The driver is for the usage with the ESP8266 and [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos). If you can't find it in folder [extras/lis3mdl](https://github.com/SuperHouse/esp-open-rtos/tree/master/extras) of original repository, it is not yet merged. Please take a look to branch [lis3mdl](https://github.com/gschorcht/esp-open-rtos/tree/lis3mdl) of my fork in that case. 

It is also working with ESP32 and [ESP-IDF](https://github.com/espressif/esp-idf.git) using a wrapper component for ESP8266 functions, see folder ```components/esp8266_wrapper```, as well as Linux based systems using a wrapper library.

## About the sensor

The LIS3MDL is an ultra-low-power high-performance three-axis magnetic sensor connected to **I2C** or **SPI** with a full scale of up to **±16 Gauss**. It supports different measuring rates.

**Main features** of the sensor are:
 
- 4 selectable full scales of ±4, ±8, ±12, and ±16 Gauss
- 12 different measuring rates from 0.625 Hz up to 1 kHz
- 16 bit magnetic data output
- interrupt generators for magnetic thresholds
- embedded temperature sensor
- I2C and SPI digital output interface

## Sensor operation

### Sensor operation modes

LIS3MDL provides different operating modes (OM):

- **Power Down mode** is configured automatically after power up boot sequence. In this mode, almost all internal blocks of the device are switched off. Register content is preserved, but there are no measurements performed.

- **Measurement modes** are a set of operation modes in which measurements are performed with a different output data rates (**ODR**) and different power consumtions.

### Output Data Rates

In measurement modes, measurements are performed at a defined output rate. Following output data rates (ODR) are supported in the different modes operation modes (OM):

 Power Mode | Output data rate (ODR) | Driver symbol
:------------- |---------:|:---------------
Power-down mode| -        | ```lis3mdl_power_down```
Low-power mode | 0.625 Hz | ```lis3mdl_lpm_0_625```, ```lis3mdl_low_power```
Low-power mode | 1.25 Hz  | ```lis3mdl_lpm_1_25```
Low-power mode | 2.5 Hz   | ```lis3mdl_lpm_2_5```
Low-power mode | 5 Hz     | ```lis3mdl_lpm_5```
Low-power mode | 10 Hz    | ```lis3mdl_lpm_10```
Low-power mode | 20 Hz    | ```lis3mdl_lpm_20```
Low-power mode | 40 Hz    | ```lis3mdl_lpm_40```
Low-power mode | 80 Hz    | ```lis3mdl_lpm_80```
Low-power mode | 1000 Hz  | ```lis3mdl_lpm_1000```
Medium-performance mode       | 560 Hz | ```lis3mdl_mpm_560```
High-performance mode         | 300 Hz | ```lis3mdl_hpm_300```
Ultra-high-performance mode   | 155 Hz | ```lis3mdl_uhpm_155```

The **easiest way to use the sensor** is to initialize it with the ```lis3mdl_init_sensor``` function and then switch it to any measurement mode with the ```lis3mdl_set_mode``` function to start measurements with the given output data rate (ODR).

```
...
static lis3mdl_sensor_t* sensor = 0;
...
if ((sensor = lis3mdl_init_sensor (I2C_BUS, LIS3MDL_I2C_ADDRESS_2, 0)))
{
    ...
    lis3mdl_set_mode (sensor, lis3mdl_lpm_10);
    ...
}
...

```
In this example, a LIS3MDL sensor is connected to I2C bus. It is initialized and set to low-power measurement mode with an output data rate (ODR) of 10 Hz to start the measurements.

**Please note:** 
- ```lis3mdl_init_sensor``` function resets the sensor completely. That is, all sensor registers are reset to their default values and the sensor is switched to the power-down mode. The function returns a pointer to an sensor device data structure on success which is allocated from memory.
- All sensor configurations should be done before calling ```lis3mdl_set_mode``` function. In particular, the interrupt configuration should be performed before to avoid loosing the first interrupt and locking the system.

## Measurement results

### Output data format

The sensor determines periodically the magnetic values for all axes and produces output data with the selected output data rate (ODR).

Raw **output data** (**raw data**) are given as 16-bit signed integer values in 2’s complement representation and are always left-aligned. The range and the resolution of raw data depend on the sensitivity of the sensor which is selected by the **full scale** parameter. The LIS3MDL allows to select the following full scales:

Full Scale | Resolution   | Driver symbol
---------------------:|:-----------|:------
 ±4 Gauss  |  1/6.842 mGauss | ```lis3mdl_scale_4_Gs```
 ±8 Gauss  |  1/3.421 mGauss | ```lis3mdl_scale_8_Gs```
±12 Gauss  |  1/2.281 mGauss | ```lis3mdl_scale_12_Gs```
±16 Gauss  |  1/1.711 mGauss | ```lis3mdl_scale_16_Gs```

By default, a full scale of ±4 Gauss is used. ```lis3mdl_set_scale``` function can be used to change it.

```
lis3mdl_set_scale(sensor, lis3mdl_scale_4_Gs);
```

### Fetching output data

To get the information whether new data are available, the user task can either use

- the ```lis3mdl_new_data``` function to **check periodically** whether new output data are available, or
- the **data ready interrupt** on ```DRDY``` signal which becomes active as soon as complete sample of new output data are available (see below).

Last measurement results can then be fetched either 

- as **raw data** using ```lis3mdl_get_raw_data``` function or 
- as **floating point values in Gauss (Gs)** using ```lis3mdl_get_float_data``` function.

It is recommended to use ```lis3mdl_get_float_data``` function since it already converts measurement results to real values according to the selected full scale.

```
void user_task_periodic(void *pvParameters)
{
    lis3mdl_float_data_t data;

    while (1)
    {
        // execute task every 10 ms
        vTaskDelay (10/portTICK_PERIOD_MS);
        ...
        // test for new data
        if (!lis3mdl_new_data (sensor))
            continue;
    
        // fetch new data
        if (lis3mdl_get_float_data (sensor, &data))
        {
            // do something with data
            ...
        }
    }
}
```

**Please note:** 
```lis3mdl_get_float_data``` and ```lis3mdl_get_raw_data``` functions always return the last available results. If these functions are called more often than measurements are taken, some measurement results are retrieved multiple times. If these functions are called too rarely, some measurement results will be lost.

## Interrupts

The LIS3MDL supports two dedicated interrupt signals for two different types of interrupts:

- **data ready** interrupts on the **```DRDY```** signal, and
- **magnetic threshold** interrupts on the **```INT```** signal.

While magnetic threshold interrupts can be configured as well as enabled or disabled, data-ready interrupts are always enabled and can not be explicitly configured.

### Data ready interrupts

Whenever an interrupt is generated at interrupt signal ```DRDY```, new data are available and can be read with ```lis3mdl_get_float_data``` function or ```lis3mdl_get_raw_data``` function.

```
void drdy_handler ()
{
    // fetch new data
    if (lis3mdl_get_float_data (sensor, &data))
    {
        // do something with data
        ...
    }
}
```

### Magnetic threshold interrupts

Magnetic threshold detection of LIS3MDL allows to generate interrupts on ```INT``` signal whenever measured magnetic data exceed a defined threshold value at positive or negative side. It can be enabled for each axis separatly. The defined threshhold is valid for all enabled axes.

Magnetic threshold interrupts can be configured with ```lis3mdl_get_int_config``` function . This function requires configuration of type ```lis3mdl_int_config_t``` as paramater.

```
lis3mdl_int_config_t int_config;

int_config.threshold   = 1000;
int_config.x_enabled   = true;
int_config.y_enabled   = true;
int_config.z_enabled   = true;
int_config.latch       = true;
int_config.signal_level= lis3mdl_high_active;
        
lis3mdl_set_int_config (sensor, &int_config);
```

In this example, magnetic threshold detection is enabled for all axes and a threshold of 1000 is defined. 

The parameter of type ```lis3mdl_int_config_t``` also configures

- whether the interrupt signal should latched until the interrupt source is read, and 
- whether the interrupt signal is high (default) or low active.

```lis3mdl_get_int_source``` function can be used to determine the source of an magnetic threshold interrupt whenever it is generated. This function returns a data structure of type ```lis3mdl_int_source_t``` which contains a boolean member for each source that can be tested for true.

```
void int_handler ()
{
    lis3mdl_int_source_t int_src;

    // get the source of the interrupt and reset the INT signal
    lis3mdl_get_int_source (sensor, &int_src);
    
    // test the source of the interrupt
    if (int_src.active)
    {
        if (int_src.x_pos || int_src.x_neg) ... ; // do something
        if (int_src.y_pos || int_src.y_neg) ... ; // do something
        if (int_src.z_pos || int_src.z_neg) ... ; // do something
    }
    ...
}
```
**Please note:** If the interrupt is configured to be latched, the interrupt signal is active until the interrupt source is read. Otherwise the interrupt signal is only active as long as the interrupt condition is satisfied.

## Temperature sensor

The LIS3MDL sensor contains an internal temperature sensor. It can be activated and deactivated with the ```lis3mdl_enable_temperature``` function. Using ```lis3mdl_get_temperature``` function, the temperature can be determined as a floating point value in degrees. The temperature is measured by the sensor at the same rate as the magnetic data.

## Low level functions

The LIS3MDL is a very complex and flexible sensor with a lot of features. It can be used for a big number of different use cases. Since it is quite impossible to implement a high level interface which is generic enough to cover all the functionality of the sensor for all different use cases, there are two low level interface functions that allow direct read and write access to the registers of the sensor.

```
bool lis3mdl_reg_read  (lis3mdl_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
bool lis3mdl_reg_write (lis3mdl_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
```

**Please note**
These functions should only be used to do something special that is not covered by drivers's high level interface AND if you exactly know what you do and what it might affect. Please be aware that it might always affect the high level interface.


## Usage

First, the hardware configuration has to be established.

### Hardware configurations

Following figure shows a possible hardware configuration for ESP8266 and ESP32 if I2C interface is used to connect the sensor.

```
  +-----------------+     +----------+
  | ESP8266 / ESP32 |     | LIS3MDL  |
  |                 |     |          |
  |   GPIO 14 (SCL) >-----> SCL      |
  |   GPIO 13 (SDA) <-----> SDA      |
  |   GPIO 5        <------ INT      |
  |   GPIO 4        <------ DRDY     |
  +-----------------+     +----------+
```

If SPI interface is used, configuration for ESP8266 and ESP32 could look like following.

```
  +-----------------+     +----------+              +-----------------+     +----------+
  | ESP8266         |     | LIS3MDL  |              | ESP32           |     | LIS3MDL  |
  |                 |     |          |              |                 |     |          |
  |   GPIO 14 (SCK) ------> SCK      |              |   GPIO 16 (SCK) ------> SCK      |
  |   GPIO 13 (MOSI)------> SDI      |              |   GPIO 17 (MOSI)------> SDI      |
  |   GPIO 12 (MISO)<------ SDO      |              |   GPIO 18 (MISO)<------ SDO      |
  |   GPIO 2  (CS)  ------> CS       |              |   GPIO 19 (CS)  ------> CS       |
  |   GPIO 5        <------ INT      |              |   GPIO 5        <------ INT      |
  |   GPIO 4        <------ DRDY     |              |   GPIO 4        <------ DRDY     |
  +-----------------+     +----------+              +-----------------+     +----------+
```

### Communication interface settings

Dependent on the hardware configuration, the communication interface and interrupt settings have to be defined. In case ESP32 is used, the configuration could look like 

```
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

// interrupt GPIOs defintions for ESP8266 and ESP32
#define PIN_INT       5
#define PIN_DRDY      4

```

### Main program

#### Initialization

If I2C interfaces are used, they have to be initialized first.

```
i2c_init (I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ);
```

SPI interface has only to be initialized explicitly on ESP32 platform to declare the GPIOs that are used for SPI interface.

```
spi_bus_init (SPI_BUS, SPI_SCK_GPIO, SPI_MISO_GPIO, SPI_MOSI_GPIO);
```

Once the interfaces are initialized, function ```lis3mdl_init_sensor``` has to be called for each LIS3MDL sensor in order to initialize the sensor and to check its availability as well as its error state. This function returns a pointer to a sensor device data structure or NULL in case of error.

The parameter *bus* specifies the ID of the I2C or SPI bus to which the sensor is connected.

```
static lis3mdl_sensor_t* sensor;
```

For sensors connected to an I2C interface, a valid I2C slave address has to be defined as parameter *addr*. In that case parameter *cs* is ignored.

```
sensor = lis3mdl_init_sensor (I2C_BUS, LIS3MDL_I2C_ADDRESS_2, 0);

```

If parameter *addr* is 0, the sensor is connected to a SPI bus. In that case, parameter *cs* defines the GPIO used as CS signal.

```
sensor = lis3mdl_init_sensor (SPI_BUS, 0, SPI_CS_GPIO);

```

The remaining of the program is independent on the communication interface.

#### Configuring the sensor

Optionally, you could wish to set some measurement parameters. For details see the sections above, the header file of the driver ```lis3mdl.h```, and of course the data sheet of the sensor.

#### Starting measurements

As last step, the sensor mode has be set to start periodic measurement. The sensor mode can be changed anytime later.

```
...
// start periodic measurement with output data rate of 10 Hz
lis3mdl_set_mode (sensor, lis3mdl_lpm_10);
...
```

#### Periodic user task

Finally, a user task that uses the sensor has to be created. 

**Please note:** To avoid concurrency situations when driver functions are used to access the sensor, for example to read data, the user task must not be created until the sensor configuration is completed.

The user task can use different approaches to fetch new data. Either new data are fetched periodically or interrupt signals are used when new data are available or a configured event happens.

If new data are fetched **periodically** the implementation of the user task is quite simple and could look like following.

```
void user_task_periodic(void *pvParameters)
{
    lis3mdl_float_data_t data;

    while (1)
    {
        // execute task every 10 ms
        vTaskDelay (10/portTICK_PERIOD_MS);
        ...
        // test for new data
        if (!lis3mdl_new_data (sensor))
            continue;
    
        // fetch new data
        if (lis3mdl_get_float_data (sensor, &data))
        {
            // do something with data
            ...
        }
    }
}
...
// create a user task that fetches data from sensor periodically
xTaskCreate(user_task_periodic, "user_task_periodic", TASK_STACK_DEPTH, NULL, 2, NULL);
```

The user task simply tests periodically with a rate higher than the output data rate (ODR) of the sensor whether new data are available. If new data are available, it fetches the data.

#### Interrupt user task

A different approach is to use one of the **interrupt signals** ```INT``` or ```DRDY```. In this case, the user has to implement an interrupt handler that either fetches the data directly or triggers a task, that is waiting to fetch the data.

```
static QueueHandle_t gpio_evt_queue = NULL;

// Interrupt handler which resumes sends an event to the waiting user_task_interrupt

void IRAM int_signal_handler (uint8_t gpio)
{
    // send an event with GPIO to the interrupt user task
    xQueueSendFromISR(gpio_evt_queue, &gpio, NULL);
}

// User task that fetches the sensor values

void user_task_interrupt (void *pvParameters)
{
    uint32_t gpio_num;

    while (1)
    {
        if (xQueueReceive(gpio_evt_queue, &gpio_num, portMAX_DELAY))
        {
            // test for new data
            if (!lis3mdl_new_data (sensor))
                continue;
    
            // fetch new data
            if (lis3mdl_get_float_data (sensor, &data))
            {
                // do something with data
                ...
            }
        }
    }
}
...

// create a task that is triggered only in case of interrupts to fetch the data

xTaskCreate(user_task_interrupt, "user_task_interrupt", TASK_STACK_DEPTH, NULL, 2, NULL);
...
```

In this example, there is 

- a task that is fetching data when it receives an event, and 
- an interrupt handler that generates the event on interrupt.

Finally, interrupt handlers have to be activated for the GPIOs which are connected to the interrupt signals.

```
// configure interrupt pins for *INT1* and *INT2* signals and set the interrupt handler
gpio_set_interrupt(PIN_INT , GPIO_INTTYPE_EDGE_POS, int_signal_handler);
gpio_set_interrupt(PIN_DRDY, GPIO_INTTYPE_EDGE_POS, int_signal_handler);
```

Furthermore, the interrupts have to be enabled and configured in the LIS3MDL sensor, see section **Interrupts** above.

## Full Example

```
/* -- use following constants to define the example mode ----------- */

// #define SPI_USED     // if defined SPI is used, otherwise I2C
// #define INT_DATA     // data ready interrupt used
// #define INT_THRESH   // threshold interrupt used

#if defined(INT_DATA) || defined(INT_THRESH)
#define INT_USED
#endif

/* -- includes ----------------------------------------------------- */

#include "lis3mdl.h"

/** -- platform dependent definitions ------------------------------ */

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

// interrupt GPIOs defintions for ESP8266 and ESP32
#define PIN_INT       5
#define PIN_DRDY      4

/* -- user tasks --------------------------------------------------- */

static lis3mdl_sensor_t* sensor;

/**
 * Common function used to get sensor data.
 */
void read_data ()
{
    lis3mdl_float_data_t  data;

    if (lis3mdl_new_data (sensor) &&
        lis3mdl_get_float_data (sensor, &data))
        // max. full scale is +-16 g and best resolution is 1 mg, i.e. 5 digits
        printf("%.3f LIS3MDL (xyz)[Gs] mx=%+7.3f my=%+7.3f mz=%+7.3f\n",
               (double)sdk_system_get_time()*1e-3, 
                data.mx, data.my, data.mz);
}


#ifdef INT_USED
/**
 * In this case, any of the possible interrupts on interrupt signal *INT1* is
 * used to fetch the data.
 *
 * When interrupts are used, the user has to define interrupt handlers that
 * either fetches the data directly or triggers a task which is waiting to
 * fetch the data. In this example, the interrupt handler sends an event to
 * a waiting task to trigger the data gathering.
 */

static QueueHandle_t gpio_evt_queue = NULL;

// User task that fetches the sensor values.

void user_task_interrupt (void *pvParameters)
{
    uint8_t gpio_num;

    while (1)
        if (xQueueReceive(gpio_evt_queue, &gpio_num, portMAX_DELAY))
        {
            if (gpio_num == PIN_DRDY)
            {
                read_data ();
            }
            else if (gpio_num == PIN_INT)
            {
                lis3mdl_int_source_t int_src;

                // get the source of the interrupt and reset INT signals
                lis3mdl_get_int_source (sensor, &int_src);
    
                // in case of DRDY interrupt or activity interrupt read one data sample
                if (int_src.active)
                    read_data ();
            }
        }
}

// Interrupt handler which resumes user_task_interrupt on interrupt

void IRAM int_signal_handler (uint8_t gpio)
{
    // send an event with GPIO to the interrupt user task
    xQueueSendFromISR(gpio_evt_queue, &gpio, NULL);
}

#else // !INT_USED

/*
 * In this example, user task fetches the sensor values every seconds.
 */

void user_task_periodic(void *pvParameters)
{
    vTaskDelay (100/portTICK_PERIOD_MS);
    
    while (1)
    {
        // read sensor data
        read_data ();
        
        // passive waiting until 1 second is over
        vTaskDelay(100/portTICK_PERIOD_MS);
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

    #ifdef SPI_USED

    // init the sensor connnected to SPI
    spi_bus_init (SPI_BUS, SPI_SCK_GPIO, SPI_MISO_GPIO, SPI_MOSI_GPIO);

    // init the sensor connected to SPI_BUS with SPI_CS_GPIO as chip select.
    sensor = lis3mdl_init_sensor (SPI_BUS, 0, SPI_CS_GPIO);
    
    #else

    // init all I2C bus interfaces at which LIS3MDL  sensors are connected
    i2c_init (I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ);
    
    // init the sensor with slave address LIS3MDL_I2C_ADDRESS_2 connected to I2C_BUS.
    sensor = lis3mdl_init_sensor (I2C_BUS, LIS3MDL_I2C_ADDRESS_2, 0);

    #endif
    
    if (sensor)
    {
        #ifdef INT_USED

        /** --- INTERRUPT CONFIGURATION PART ---- */
        
        // Interrupt configuration has to be done before the sensor is set
        // into measurement mode to avoid losing interrupts

        // create an event queue to send interrupt events from interrupt
        // handler to the interrupt task
        gpio_evt_queue = xQueueCreate(10, sizeof(uint8_t));

        // configure interupt pins for *INT* and *DRDY* signals and set the interrupt handler
        gpio_enable(PIN_INT , GPIO_INPUT);
        gpio_enable(PIN_DRDY, GPIO_INPUT);
        gpio_set_interrupt(PIN_INT , GPIO_INTTYPE_EDGE_POS, int_signal_handler);
        gpio_set_interrupt(PIN_DRDY, GPIO_INTTYPE_EDGE_POS, int_signal_handler);

        #endif  // !defined(INT_USED)
        
        // -- SENSOR CONFIGURATION PART ---

        // Interrupt configuration has to be done before the sensor is set
        // into measurement mode
        
        #ifdef INT_THRESH
        // enable threshold interrupts on INT1 
        lis3mdl_int_config_t int_config;
    
        int_config.threshold   = 1000;
        int_config.x_enabled   = true;
        int_config.y_enabled   = true;
        int_config.z_enabled   = true;
        int_config.latch       = true;
        int_config.signal_level= lis3mdl_high_active;
        
        lis3mdl_set_int_config (sensor, &int_config);
        #endif // INT_THRESH

        // LAST STEP: Finally set scale and mode to start measurements
        lis3mdl_set_scale(sensor, lis3mdl_scale_4_Gs);
        lis3mdl_set_mode (sensor, lis3mdl_lpm_10);

        /** -- TASK CREATION PART --- */

        // must be done last to avoid concurrency situations with the sensor
        // configuration part

        #ifdef INT_USED

        // create a task that is triggered only in case of interrupts to fetch the data
        xTaskCreate(user_task_interrupt, "user_task_interrupt", TASK_STACK_DEPTH, NULL, 2, NULL);
        
        #else // INT_USED

        // create a user task that fetches data from sensor periodically
        xTaskCreate(user_task_periodic, "user_task_periodic", TASK_STACK_DEPTH, NULL, 2, NULL);

        #endif
    }
    else
        printf("Could not initialize LIS3MDL sensor\n");
}

```
