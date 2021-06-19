# Driver for the L3GD20H 3-axes digital output gyroscope

The driver is for the usage with the ESP8266 and [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos). If you can't find it in folder [extras/l3gd20h](https://github.com/SuperHouse/esp-open-rtos/tree/master/extras) of original repository, it is not yet merged. Please take a look to branch [l3gd20h](https://github.com/gschorcht/esp-open-rtos/tree/l3gd20h) of my fork in that case.

It is also working with ESP32 and [ESP-IDF](https://github.com/espressif/esp-idf.git) using a wrapper component for ESP8266 functions, see folder ```components/esp8266_wrapper```, as well as Linux based systems using a wrapper library.

The driver can also be used with L3GD20 and L3G4200D.

## About the sensor

L3GD20H is a low-power **3-axis angular rate sensor** connected to **I2C** or **SPI** with a full scale of up to **2000 dps**. It supports different measuring rates with a user selectable bandwidth.

**Main features** of the sensor are:

- 3 selectable full scales of ±245, ±500, and ±2000 dps
- 7 measuring rates from 12.5 Hz to 800 Hz with 4 bandwidths
- 16 bit angular rate value data output
- 8 bit temperature data output
- 2 dedicated interrupt signals for data and event interrupts
- integrated high-pass filters with 3 modes and 10 different cut off frequencies
- embedded temperature sensor
- embedded 32 levels of 16 bit data output FIFO
- I2C and SPI digital output interface
- embedded power-down and sleep mode with fast turn-on and wake-up

## Sensor operation

### Sensor modes

L3GD20H provides different operating modes.

- **Power Down mode** is configured automatically after power up boot sequence. In this mode, all gyros are switched off. Therefore, it takes up to 100 ms to switch to another mode.

- **Normal mode** is the normal measurement mode. All gyros are switched on and at least one axis is enabled for measurements. Measurements are performed at a defined output data rate (**ODR**).

- **Sleep mode** is the normal mode when no axis is enabled for measurement. In this modes, all gyros are kept switched on. Therefore, it only takes 1/ODR to switch to normal mode if low pass filtering is disabled or 6/ODR if low pass filtering is enabled.

### Output Data Rates

In normal mode, measurements are performed at a defined output rate with a user selectable bandwidth. Following output data rates (ODR) are supported.

Mode | Output Data Rate (ODR) | Driver symbol
:---- |:----------------------:|:------------------ 
Power down  | -        | ```l3gd20h_power_down``` 
Normal mode | 12.5 Hz  | ```l3gd20h_normal_12_5```
Normal mode | 25 Hz    | ```l3gd20h_normal_25```
Normal mode | 50 Hz    | ```l3gd20h_normal_50```
Normal mode | 100 Hz   | ```l3gd20h_normal_100```
Normal mode | 200 Hz   | ```l3gd20h_normal_200```
Normal mode | 400 Hz   | ```l3gd20h_normal_400```
Normal mode | 800 Hz   | ```l3gd20h_normal_800```

Output data rates (ODR) of less than 100 Hz are called Low Data Rates. 

For each ODR, one of the four bandwidths 0...3 can be selected that defines the cutoff frequency (please refer datasheet) of an embedded low pass filter for the measurement results.

The **easiest way to use the sensor** is simply to initialize it with function ```l3gd20h_init_sensor``` and then set it to the normal mode with function ```l3gd20h_set_mode``` to start measurements with the given output data rate (ODR). The bandwidth of the embedded low-pass filter and the axes x, y and z that are activated for measurements are also given as parameters.

```
...
static l3gd20h_sensor_t* sensor = 0;
...
if ((sensor = l3gd20h_init_sensor (I2C_BUS, L3GD20H_I2C_ADDRESS_2, 0)))
{
    ...
    l3gd20h_set_mode (sensor, l3gd20h_normal_odr_200, 3, true, true, true);
    ...
}
...

```
In this example, a L3GD20H sensor connected to I2C is initialized and set to normal mode to start measurements for all three axes with an output data rate (ODR) of 200 Hz and bandwidth 3 (please refer datasheet).

**Please note:** 
- Function ```l3gd20h_init_sensor``` resets the sensor completely, switches it to the power down mode, and returns a pointer to a sensor device data structure on success. All registers are reset to default values and the embedded FIFO is cleared.
- All sensor configurations should be done before calling function ```l3gd20h_set_mode```. In particular, the interrupt configuration should be performed before to avoid loosing the first interrupt and locking the system.

## Measurement results

### Output data format

In normal mode, sensor determines periodically the angular rate for all axes that are enabled for measurement and produces output data with the selected output data rate (ODR).

Function ```l3gd20h_new_data``` or the data ready interrupt (see below) can be used to determine when new data are available.

Raw **output data** (**raw data**) are given as 16-bit signed integer values in 2’s complement representation. The range and the resolution of these data depend on the sensitivity of the sensor which is selected by the **full scale** value. The L3GD20H allows to select the following full scales:

Full Scale  | Resolution | Driver symbol 
-----------:|-----------:|:---------------
±245 dps | 2 mdps  | ```l3gd20h_scale_245_dps```
±500 dps | 4 mdps  | ```l3gd20h_scale_500_dps```
±2000 dps | 16 mdps | ```l3gd20h_scale_2000_dps```

By default, a full scale of ±245 dps is used. Function ```l3gd20h_set_scale``` can be used to change it.

```
l3gd20h_set_scale(sensor, l3gd20h_scale_500_dps);
```

### Fetching output data

To get the information whether new data are available, the user task can either use

- the function ```l3gd20h_new_data```  to check periodically whether new output data are available, or
- the data ready interrupt (DRDY) which is thrown as soon as new output data are available (see below).

Last measurement results can then be fetched either 

- as raw data using function ```l3gd20h_get_raw_data``` or 
- as floating point values in dps (degrees per second) using function ```l3gd20h_get_float_data```.

It is recommended to use function ```l3gd20h_get_float_data``` since it already converts measurement results to real values according to the selected full scale.

```
void user_task_periodic(void *pvParameters)
{
    l3gd20h_float_data_t data;

    while (1)
    {
        // execute task every 10 ms
        vTaskDelay (10/portTICK_PERIOD_MS);
        ...
        // test for new data
        if (!l3gd20h_new_data (sensor))
            continue;
    
        // fetch new data
        if (l3gd20h_get_float_data (sensor, &data))
        {
            // do something with data
            ...
        }
    }
}
```

**Please note:** 
The functions ```l3gd20h_get_float_data``` and ```l3gd20h_get_raw_data``` always return the last available results. If these functions are called more often than measurements are performed, some measurement results are retrieved multiple times. If these functions are called too rarely, some measurement results will be lost.

### Filters

L3GD20H provides embedded low-pass as well as high-pass filtering capabilities to improve measurement results.
It is possible to independently apply the filters on the output data and/or on the data used for event interrupt generation (selective axis movement and wake up, see below) separately. Please refer the [datasheet](http://www.st.com/resource/en/datasheet/l3gd20.pdf) or [application note](http://www.st.com/resource/en/application_note/dm00119036.pdf) for more details.

The filters applied to the output data are selected with function ```l3gd20h_select_output_filter```.  Following selections are possible:

Driver symbol | Low pass filter (LPF2) used | High pass filter (HPF) used
:--------------|:-----------------------------:|:---------------------:
```l3gd20h_no_filter```    | - | -
```l3gd20h_hpf_only```     | x | -
```l3gd20h_lpf2_only```    | - | x
```l3gd20h_hpf_and_lpf2``` | x | x

These filters can also be applied to data used for event interrupt generation (selective axis movement and wake up). The filter mode is defined by member ```filter``` in the settings of interrupt generator configuration, see function ```l3gd20h_set_int_event_config```.

While the cutoff frequency of the low pass filter (LPF2) is fixed and depends only on the output data rate (ODR), the mode and the cutoff frequency of the high pass filter can be configured using function ```l3gd20h_config_hpf```. Following HPF modes are available:

Driver symbol | HPF mode
:--------------|:---------
```l3gd20h_hpf_normal```    | Normal mode
```l3gd20h_hpf_reference``` | Reference mode
```l3gd20h_hpf_autoreset``` | Auto-reset on interrupt

For each output data rate (ODR), 10 different HPF cutoff frequencies can be used.

```
...
// select LPF/HPF
l3gd20h_select_output_filter (sensor, l3gd20h_hpf_only);

// configure HPF in normal mode with cutoff frequency 0
l3gd20h_config_hpf (sensor, l3gd20h_hpf_normal, 2);

// reset the reference by a dummy read
l3gd20h_get_hpf_ref (sensor);
...
```

**Please note:** Since same filters are used for the output data as well as the data used for event interrupt generation (selective axes movement / wake up), the configuration of the filters always affects both data.


### FIFO

In order to limit the rate at which the host processor has to fetch the data, the L3GD20H embeds a first-in first-out buffer (FIFO). This is in particular helpful at high output data rates. The FIFO buffer can work in seven different modes and is able to store up to 32 angular rate samples. Please refer the [datasheet](http://www.st.com/resource/en/datasheet/l3gd20.pdf) or [application note](http://www.st.com/resource/en/application_note/dm00119036.pdf) for more details.

Driver symbol | FIFO mode
--------------|-------------------------
```l3gd20h_bypass```  | Bypass mode (FIFO is not used)
```l3gd20h_fifo```    | FIFO mode
```l3gd20h_stream```  | Stream mode
```l3gd20h_stream_to_fifo``` | Stream-to-FIFO mode
```l3gd20h_bypass_to_stream``` | Bypass-to-Stream mode
```l3gd20h_dynamic_stream``` | Dynamic Stream mode
```l3gd20h_bypass_to_fifo``` | Bypass to FIFO mode

The FIFO mode can be set using function ```l3gd20h_set_fifo_mode```. This function takes two parameters, the FIFO mode and a threshold value which defines a watermark level. When the FIFO content exceeds this level, a watermark flag is set and an interrupt can be generated. They can be used to gather a minimum number of axes angular rate samples with the sensor before the data are fetched as a single read operation from the sensor.

```
...
// clear FIFO
l3gd20h_set_fifo_mode (sensor, l3gd20h_bypass, 0);

//  activate FIFO mode
l3gd20h_set_fifo_mode (sensor, l3gd20h_stream, 10);
...
```

**Please note**: To clear the FIFO at any time, set the FIFO mode to ```l3gd20h_bypass``` and back to the desired FIFO mode.

To read data from the FIFO, simply use either 

- the function ```l3gd20h_get_raw_data_fifo``` to all get raw output data stored in FIFO or
- the function ```l3gd20h_get_float_data_fifo``` to get all data stored in FIFO and converted to real values in dps (degrees per second). 

Both functions clear the FIFO and return the number of samples read from the FIFO. 

```
void user_task_periodic (void *pvParameters)
{
    l3gd20h_float_data_fifo_t  data;

    while (1)
    {
        // execute task every 500 ms
        vTaskDelay (500/portTICK_PERIOD_MS);
        ...
        // test for new data
        if (!l3gd20h_new_data (sensor))
            continue;
    
        // fetch data from fifo
        uint8_t num = l3gd20h_get_float_data_fifo (sensor, data);
        
        for (int i = 0; i < num; i++)
        {
           // do something with data[i] ...
        }
}
```

## Interrupts

The L3GD20H allows to activate interrupts on two dedicated interrupt signals

- for data interrupts (data ready and FIFO status) on signal **```DRDY/INT2```**, and
- for event interrupts (axis movement and wake up) on signal **```INT1```**.

### Data interrupts (data ready and FIFO status) on  signal **```DRDY/INT2```**

Interrupts on signal ```DRDY/INT2``` can be generated by following sources:

Interrupt source | Driver symbol
:-----------------|:-------------
Output data become ready to read | ```l3gd20h_int_data_ready```
FIFO content exceeds the watermark level | ```l3gd20h_int_fifo_threshold```
FIFO is completely filled | ```l3gd20h_int_fifo_overrun```
FIFO becomes empty | ```l3gd20h_int_fifo_empty```

Each of these interrupt sources can be enabled or disabled separately with function ```l3gd20h_enable_int```. By default all interrupt sources are disabled.

```
l3gd20h_enable_int (sensor, l3gd20h_int_data_ready, true);
```

Whenever the interrupt signal ```DRDY/INT2``` is generated, function ```l3gd20h_get_int_data_source``` can be used to determine the source of the interrupt signal. This function returns a data structure of type ```l3gd20h_int_data_source_t``` that contains a boolean member for each source that can be tested for true.

```
void int2_handler ()
{
   l3gd20h_int_data_source_t source;

   // get the interrupt source of INT2
   l3gd20h_get_int_data_source (sensor, &source);

   // in case of data ready interrupt, get the results and do something with them
   if (source.data_ready)
   {
      l3gd20h_get_float_data (sensor, &data)

      // do something with data
      ...
   }
}
```

### Event interrupts (Axes movement and wake up) on signal **```INT1```**

This interrupt signal allows to recognize independent rotations of the x, y and z axes. For this purpose, a separate threshold can be defined for each axis. If activated, the angular rate of each axis is compared with its threshold to check whether it is below or above the threshold. The results of all activated comparisons are combined OR or AND to generate the interrupt signal.

The configuration of the thresholds, the activated comparisons and selected AND/OR combination allows to recognize special situations like selective axis movement (SA) or axes movement wakeup (WU).

- **Selective axis movement recognition (SA)** means that only one axis is rotating. This is the case if the angular rate of selected axis is above its threshold AND angular rates of all other axes are below their thresholds.

- **Axis movement wake up (WU)** means that the angular rate of any axis is above its threshold (OR).

To configure event interrupts, the function ```l3gd20h_set_int_event_config``` has to be used with a parameter of structured data type ```l3gd20h_int_event_config_t``` which contains the configuration. For example, selective axis movement recognition (SA) for the z-axis could be configured as following:

```
l3gd20h_int_event_config_t int_cfg;

// thresholds
int_cfg.x_threshold = 100;
int_cfg.y_threshold = 100;
int_cfg.z_threshold = 1000;

// x axis below threshold
int_cfg.x_low_enabled  = false;
int_cfg.x_high_enabled = true;

// y axis below threshold
int_cfg.y_low_enabled  = true;
int_cfg.y_high_enabled = false;

// z axis below threshold
int_cfg.z_low_enabled  = false;
int_cfg.z_high_enabled = true;

// AND combination of all conditions
int_cfg.and_or = true;

// further parameters
int_cfg.filter = l3gd20h_hpf_only;
int_cfg.latch = true;
int_cfg.duration = 0;
int_cfg.wait = false;

// set the configuration and enable the interrupt
l3gd20h_set_int_cfg (sensor, &int_cfg);
l3gd20h_enable_int (sensor, l3gd20h_int_event, true);
```

Furthermore, with this data structure it is also configured

- whether the interrupt signal should latched until the interrupt source is read,
- which filters are applied to data used for interrupt generation,
- which time in 1/ODR an interrupt condition has to be given before the interrupt is generated, and
- whether this time is also used when interrupt condition in no longer given before interrupt signal is reset.

Function ```l3gd20h_enable_int``` is used to enable or disable the event interrupt generation.

As with data ready and FIFO interrupts, function ```l3gd20h_get_int1_source``` can be used to determine the source of the interrupt signal whenever it is generated. This function returns a data structure of type ```l3gd20h_int1_source_t``` that contain a boolean member for each source that can be tested for true.

```
void int1_handler ()
{
   l3gd20h_int_event_source_t source;

   // get the source of INT1 reset INT1 signal
   l3gd20h_get_int_event_source (sensor, &source);

   // if all conditions where true interrupt
   if (source.active)
   {
      l3gd20h_get_float_data (sensor, &data)

      // do something with data
      ...
   }
}
```
**Please note:** If the interrupt is configured to be latched, the interrupt signal is active until the interrupt source is read. Otherwise the interrupt signal is only active as long as the interrupt condition is satisfied.

**Please note** Activating all threshold comparisons and the OR combination is the most flexible way, functions like selective axis movement can then be realized combining the different interrupt sources. Following example realizes also the selective axis movement recognition (SA) for the z-axis.

```
l3gd20h_int_event_config_t int_cfg;

// thresholds
int_cfg.x_threshold = 100;
int_cfg.y_threshold = 100;
int_cfg.z_threshold = 100;

// x axis
int_cfg.x_low_enabled  = true;
int_cfg.x_high_enabled = true;

// y axis
int_cfg.y_low_enabled  = true;
int_cfg.y_high_enabled = true;

// z axis
int_cfg.z_low_enabled  = true;
int_cfg.z_high_enabled = true;

// OR combination of all conditions
int_cfg.and_or = false;
...
// set the configuration and enable the interrupt
l3gd20h_set_int_cfg (sensor, &int_cfg);
l3gd20h_enable_int (sensor, l3gd20h_int_event, true);
```

```
void int1_handler ()
{
   l3gd20h_int1_source_t source;

   // get the interrupt source of INT1
   l3gd20h_get_int1_source (sensor, &source);

   // if all conditions where true interrupt
   if (source.y_low && source.y_low && source.z_high)
   {
      // selective axis movement of z-axis
      ...
   }
}
```

### Interrupt signal properties

By default, interrupt signals are high active. Using function ```l3gd20h_config_int_signals```, the level of the interrupt signal and the type of the interrupt outputs can be changed.

Driver symbol | Meaning
:-------------|:-------
```l3gd20h_high_active``` | Interrupt signal is high active (default)
```l3gd20h_low_active``` | Interrupt signal is low active

Driver symbol | Meaning
:-------------|:-------
```l3gd20h_push_pull```  | Interrupt output is pushed/pulled
```l3gd20h_open_drain``` | Interrupt output is open-drain

## Temperature sensor

The L3GD20H contains a temperature sensor. Function ```l3gd20h_get_temperature``` can be used to get the temperature. The temperature is given as 8-bit signed integer values in 2’s complement.

## Low level functions

The L3GD20H is a very complex and flexible sensor with a lot of features. It can be used for a big number of different use cases. Since it is quite impossible to implement a high level interface which is generic enough to cover all the functionality of the sensor for all different use cases, there are two low level interface functions that allow direct read and write access to the registers of the sensor.

```
bool l3gd20h_reg_read  (l3gd20h_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
bool l3gd20h_reg_write (l3gd20h_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
```
**Please note**
These functions should only be used to do something special that is not covered by the high level interface AND if you exactly know what you do and what it might affect. Please be aware that it might affect the high level interface.


## Usage

First, the hardware configuration has to be established.

### Hardware configurations

Following figure shows a possible hardware configuration for ESP8266 and ESP32 if I2C interface is used to connect the sensor.

```
  +-----------------+     +----------+
  | ESP8266 / ESP32 |     | L3GD20H  |
  |                 |     |          |
  |   GPIO 14 (SCL) >-----> SCL      |
  |   GPIO 13 (SDA) <-----> SDA      |
  |   GPIO 5        <------ INT1     |
  |   GPIO 4        <------ DRDY/INT2|
  +-----------------+     +----------+
```

If SPI interface is used, configuration for ESP8266 and ESP32 could look like following.

```
  +-----------------+     +----------+              +-----------------+     +----------+
  | ESP8266         |     | L3GD20H  |              | ESP32           |     | L3GD20H  |
  |                 |     |          |              |                 |     |          |
  |   GPIO 14 (SCK) ------> SCK      |              |   GPIO 16 (SCK) ------> SCK      |
  |   GPIO 13 (MOSI)------> SDI      |              |   GPIO 17 (MOSI)------> SDI      |
  |   GPIO 12 (MISO)<------ SDO      |              |   GPIO 18 (MISO)<------ SDO      |
  |   GPIO 2  (CS)  ------> CS       |              |   GPIO 19 (CS)  ------> CS       |
  |   GPIO 5        <------ INT1     |              |   GPIO 5        <------ INT1     |
  |   GPIO 4        <------ DRDY/INT2|              |   GPIO 4        <------ DRDY/INT2|
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
#define INT1_PIN      5
#define INT2_PIN      4
```

### Main program

#### Initialization

If I2C interfaces are used, they have to be initialized first.

```
i2c_init (I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ);
```

SPI interface has only to be initialized explicitly on ESP32 platform to declare the GPIOs that are used for SPI interface.

```
#ifdef ESP_PLATFORM
spi_bus_init (SPI_BUS, SPI_SCK_GPIO, SPI_MISO_GPIO, SPI_MOSI_GPIO);
#endif
```

Once the interfaces are initialized, function ```l3gd20h_init_sensor``` has to be called for each L3GD20H sensor in order to initialize the sensor and to check its availability as well as its error state. This function returns a pointer to a sensor device data structure or NULL in case of error.

The parameter *bus* specifies the ID of the I2C or SPI bus to which the sensor is connected.

```
static l3gd20h_sensor_t* sensor;
```

For sensors connected to an I2C interface, a valid I2C slave address has to be defined as parameter *addr*. In that case parameter *cs* is ignored.

```
sensor = l3gd20h_init_sensor (I2C_BUS, L3GD20H_I2C_ADDRESS_2, 0);

```

If parameter *addr* is 0, the sensor is connected to a SPI bus. In that case, parameter *cs* defines the GPIO used as CS signal.

```
sensor = l3gd20h_init_sensor (SPI_BUS, 0, SPI_CS_GPIO);

```

The remaining of the program is independent on the communication interface.

#### Configuring the sensor

Optionally, you could wish to set some measurement parameters. For details see the sections above, the header file of the driver ```l3gd20h.h```, and of course the data sheet of the sensor.

#### Starting measurements

As last step, the sensor mode has be set to start periodic measurement. The sensor mode can be changed anytime later.

```
...
// start periodic measurement with output data rate of 12.5 Hz
l3gd20h_set_mode (sensor, l3gd20h_normal_odr_12_5, 3, true, true, true);
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
    l3gd20h_float_data_t data;

    while (1)
    {
        // execute task every 10 ms
        vTaskDelay (10/portTICK_PERIOD_MS);
        ...
        // test for new data
        if (!l3gd20h_new_data (sensor))
            continue;
    
        // fetch new data
        if (l3gd20h_get_float_data (sensor, &data))
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

The user task simply tests periodically with a higher rate than the output data rate (ODR) of the sensor whether new data are available. If new data are available, it fetches the data.

#### Interrupt user task

A different approach is to use one of the **interrupts** INT1 or INT2.

- **```DRDY/INT2```** is triggered when new data become available or the FIFO queue status changes.
- **```INT1```** is triggered when configured axis movements are recognized.

In both cases, the user has to implement an interrupt handler that either fetches the data directly or triggers a task, that is waiting to fetch the data.

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
            if (!l3gd20h_new_data (sensor))
                continue;
    
            // fetch new data
            if (l3gd20h_get_float_data (sensor, &data))
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
gpio_set_interrupt(INT1_PIN, GPIO_INTTYPE_EDGE_POS, int_signal_handler);
gpio_set_interrupt(INT2_PIN, GPIO_INTTYPE_EDGE_POS, int_signal_handler);
```

Furthermore, the interrupts have to be enabled and configured in the L3GD20H sensor, see section **Interrupts** above.

## Full Example

```
/* -- use following constants to define the example mode ----------- */

// #define SPI_USED    // if defined SPI is used, otherwise I2C
// #define FIFO_MODE   // multiple sample read mode
// #define INT_DATA    // data interrupts used (data ready and FIFO status)
// #define INT_EVENT   // event interrupts used (axis movement and wake up)

#if defined(INT_EVENT) || defined(INT_DATA)
#define INT_USED
#endif

/* -- includes -------------------------------------------------- */

#include "l3gd20h.h"

/* -- platform dependent definitions ---------------------------- */

#ifdef ESP_PLATFORM  // ESP32 (ESP-IDF)

// user task stack depth
#define TASK_STACK_DEPTH 2048

// define SPI interface for L3GD20H sensors
#define SPI_BUS       HSPI_HOST
#define SPI_SCK_GPIO  16
#define SPI_MOSI_GPIO 17
#define SPI_MISO_GPIO 18
#define SPI_CS_GPIO   19

#else  // ESP8266 (esp-open-rtos)

// user task stack depth
#define TASK_STACK_DEPTH 256

// define SPI interface for L3GD20H sensors
#define SPI_BUS       1
#define SPI_SCK_GPIO  14
#define SPI_MOSI_GPIO 13
#define SPI_MISO_GPIO 12
#define SPI_CS_GPIO   2   // GPIO 15, the default CS of SPI bus 1, can't be used

#endif  // ESP_PLATFORM

// define I2C interfaces for L3GD20H sensors
#define I2C_BUS       0
#define I2C_SCL_PIN   14
#define I2C_SDA_PIN   13
#define I2C_FREQ      I2C_FREQ_100K

// define GPIOs for interrupt
#define INT1_PIN      5
#define INT2_PIN      4

/* -- user tasks ---------------------------------------------- */

static l3gd20h_sensor_t* sensor;

/**
 * Common function used to get sensor data.
 */
void read_data (void)
{
    #ifdef FIFO_MODE
    
    l3gd20h_float_data_fifo_t  data;

    if (l3gd20h_new_data (sensor))
    {
        uint8_t num = l3gd20h_get_float_data_fifo (sensor, data);
        printf("%.3f L3GD20H num=%d\n", (double)sdk_system_get_time()*1e-3, num);
        for (int i = 0; i < num; i++)
            // max. full scale is +-2000 dps and best sensitivity is 1 mdps, i.e. 7 digits
            printf("%.3f L3GD20H (xyz)[dps]: %+9.3f %+9.3f  %+9.3f\n",
                   (double)sdk_system_get_time()*1e-3, data[i].x, data[i].y, data[i].z);
    }
    
    #else // !FIFO_MODE
    
    l3gd20h_float_data_t  data;

    if (l3gd20h_new_data (sensor) &&
        l3gd20h_get_float_data (sensor, &data))
        // max. full scale is +-2000 dps and best sensitivity is 1 mdps, i.e. 7 digits
        printf("%.3f L3GD20H (xyz)[dps]: %+9.3f %+9.3f  %+9.3f\n",
               (double)sdk_system_get_time()*1e-3, data.x, data.y, data.z);
               
    #endif // FIFO_MODE
}


#ifdef INT_USED
/**
 * In this case, axes movement wake up interrupt *INT1* and/or data ready
 * interrupt *INT2* are used. While data ready interrupt *INT2* is generated
 * every time new data are available or the FIFO status changes, the axes
 * movement wake up interrupt *INT1* is triggered when output data across
 * defined thresholds.
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
    {
        if (xQueueReceive(gpio_evt_queue, &gpio_num, portMAX_DELAY))
        {
            if (gpio_num == INT1_PIN)
            {
                l3gd20h_int_event_source_t source;

                // get the source of INT1 reset INT1 signal
                l3gd20h_get_int_event_source (sensor, &source);

                // in case of data ready interrupt, get the results and do something with them
                if (source.active)
                    read_data ();
            }
            else if (gpio_num == INT2_PIN)
            {
                l3gd20h_int_data_source_t source;

                // get the source of INT2
                l3gd20h_get_int_data_source (sensor, &source);

                // if data ready interrupt, get the results and do something with them
                read_data();
            }
        }
    }
}

// Interrupt handler which resumes sends an event to the waiting user_task_interrupt

void IRAM int_signal_handler (uint8_t gpio)
{
    // send an event with GPIO to the interrupt user task
    xQueueSendFromISR(gpio_evt_queue, &gpio, NULL);
}

#else // !INT_USED

/*
 * In this case, no interrupts are used and the user task fetches the sensor
 * values periodically every seconds.
 */

void user_task_periodic(void *pvParameters)
{
    vTaskDelay (100/portTICK_PERIOD_MS);
    
    while (1)
    {
        // read sensor data
        read_data ();

        // passive waiting until 1 second is over
        vTaskDelay (100/portTICK_PERIOD_MS);
    }
}

#endif // INT_USED

/* -- main program ---------------------------------------------- */

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
    sensor = l3gd20h_init_sensor (SPI_BUS, 0, SPI_CS_GPIO);

    #else  // I2C

    // init all I2C bus interfaces at which L3GD20H sensors are connected
    i2c_init (I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ);
    
    // init the sensor with slave address L3GD20H_I2C_ADDRESS_2 connected to I2C_BUS.
    sensor = l3gd20h_init_sensor (I2C_BUS, L3GD20H_I2C_ADDRESS_2, 0);

    #endif  // SPI_USED
    
    if (sensor)
    {
        #ifdef INT_USED

        /** --- INTERRUPT CONFIGURATION PART ---- */
        
        // Interrupt configuration has to be done before the sensor is set
        // into measurement mode to avoid losing interrupts

        // create an event queue to send interrupt events from interrupt
        // handler to the interrupt task
        gpio_evt_queue = xQueueCreate(10, sizeof(uint8_t));

        // configure interupt pins for *INT1* and *INT2* signals and set the
        // interrupt handler
        gpio_enable(INT1_PIN, GPIO_INPUT);
        gpio_enable(INT2_PIN, GPIO_INPUT);
        gpio_set_interrupt(INT1_PIN, GPIO_INTTYPE_EDGE_POS, int_signal_handler);
        gpio_set_interrupt(INT2_PIN, GPIO_INTTYPE_EDGE_POS, int_signal_handler);

        #endif  // INT_USED
        
        /** -- SENSOR CONFIGURATION PART --- */

        // set type and polarity of INT signals if necessary
        // l3gd20h_config_int_signals (dev, l3gd20h_push_pull, l3gd20h_high_active);

        #ifdef INT_EVENT
        // enable event interrupts (axis movement and wake up)
        l3gd20h_int_event_config_t int_cfg;
    
        l3gd20h_get_int_event_config (sensor, &int_cfg);
    
        int_cfg.x_high_enabled = true;
        int_cfg.y_high_enabled = true;
        int_cfg.z_high_enabled = true;
        int_cfg.x_low_enabled  = false;
        int_cfg.y_low_enabled  = false;
        int_cfg.z_low_enabled  = false;
        int_cfg.x_threshold = 1000;
        int_cfg.y_threshold = 1000;
        int_cfg.z_threshold = 1000;
    
        int_cfg.filter = l3gd20h_hpf_only;
        int_cfg.and_or = false;
        int_cfg.duration = 0;
        int_cfg.latch = true;
    
        l3gd20h_set_int_event_config (sensor, &int_cfg);
        l3gd20h_enable_int (sensor, l3gd20h_int_event, true);
        
        #endif // INT_EVENT
        
        #ifdef INT_DATA
        // enable data ready (DRDY) and FIFO interrupt signal *INT2*
        // NOTE: DRDY and FIFO interrupts must not be enabled at the same time
        #ifdef FIFO_MODE
        l3gd20h_enable_int (sensor, l3gd20h_int_fifo_overrun, true);
        l3gd20h_enable_int (sensor, l3gd20h_int_fifo_threshold, true);
        #else
        l3gd20h_enable_int (sensor, l3gd20h_int_data_ready, true);
        #endif
        #endif // INT_DATA

        #ifdef FIFO_MODE
        // clear FIFO and activate FIFO mode if needed
        l3gd20h_set_fifo_mode (sensor, l3gd20h_bypass, 0);
        l3gd20h_set_fifo_mode (sensor, l3gd20h_stream, 10);
        #endif
        
        // select LPF/HPF, configure HPF and reset the reference by dummy read
        l3gd20h_select_output_filter (sensor, l3gd20h_hpf_only);
        l3gd20h_config_hpf (sensor, l3gd20h_hpf_normal, 0);
        l3gd20h_get_hpf_ref (sensor);

        // LAST STEP: Finally set scale and sensor mode to start measurements
        l3gd20h_set_scale(sensor, l3gd20h_scale_245_dps);
        l3gd20h_set_mode (sensor, l3gd20h_normal_odr_12_5, 3, true, true, true);

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
        printf("Could not initialize L3GD20H sensor\n");
}
```

