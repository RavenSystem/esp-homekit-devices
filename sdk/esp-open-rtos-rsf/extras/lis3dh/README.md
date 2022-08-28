# Driver for the LIS3DH 3-axes digital output accelerometer 

The driver is for the usage with the ESP8266 and [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos). If you can't find it in folder [extras/lis3dh](https://github.com/SuperHouse/esp-open-rtos/tree/master/extras) of original repository, it is not yet merged. Please take a look to branch [lis3dh](https://github.com/gschorcht/esp-open-rtos/tree/lis3dh) of my fork in that case.

It is also working with ESP32 and [ESP-IDF](https://github.com/espressif/esp-idf.git) using a wrapper component for ESP8266 functions, see folder ```components/esp8266_wrapper```, as well as Linux based systems using a wrapper library.

The driver can also be used with LIS3DE, LIS2DH, LIS2DH12, LIS2DE, and LIS2DE12

## About the sensor

LIS3DH is a low-power high performance **3-axis accelerometer sensor** connected to **I2C** or **SPI** with a full scale of up to **±16 g**. It supports different measuring rates.

**Main features** of the sensor are:
 
- 4 selectable full scales of ±2 g, ±4 g, ±8 g, and ±16 g
- 9 measuring rates from 1 Hz to 5 kHz
- 16 bit accelerometer value data output
- 2 independent programmable interrupt generators for free-fall and motion detection
- integrated high-pass filters with 3 modes and 4 different cut off frequencies
- embedded temperature sensor
- embedded 32 levels of 16 bit data output FIFO
- 6D/4D orientation detection
- Free-fall detection
- Motion detection
- click/double click recognition
- I2C and SPI digital output interface

## Sensor operation

### Sensor modes

LIS3DH provides different operating modes.

- **Power Down mode** is configured automatically after power up boot sequence. In this mode, almost all internal blocks of the device are switched off. Register content is preserved, but there are no measurements performed.

- **Normal mode** is the standard measurement mode. In this mode measurements are performed with a resolution of **10 bit** at the defined output data rate (**ODR**).

- **Low-power mode** is the measurement mode with reduced power consumption. Measurements are performed with a resolution of only **8 bit** at the defined output data rate (**ODR**).

- **High-resolution mode** is the measurement mode where measurements are performed with a resolution of 12 bit at the defined output data rate (**ODR**). Only output data rates (ODR) up to 400 Hz are available.

Switching from any mode to any another mode with the exception of high-resolution mode takes only 1/ODR. Switching from any mode to the high-resolution mode takes 7/ODRs.

### Output Data Rates

In normal, low-power and high-resolution modes, measurements are performed at a defined output rate. Following output data rates (ODR) are supported in the different modes:

Driver symbol | Normal mode<br>```lis3dh_normal``` | Low-power mode<br>```lis3dh_low_power``` | High-resolution mode<br>```lis3dh_high_res```
:---------------------- |:------------:|:---------------:|:--------------------:
```lis3dh_power_down``` | Power down  | Power down     | Power down
```lis3dh_normal_1```   |    1 Hz |    1 Hz |    1 Hz
```lis3dh_normal_10```  |   10 Hz |   10 Hz |   10 Hz
```lis3dh_normal_25```  |   25 Hz |   25 Hz |   25 Hz
```lis3dh_normal_50```  |   50 Hz |   50 Hz |   50 Hz
```lis3dh_normal_100``` |  100 Hz |  100 Hz |  100 Hz
```lis3dh_normal_200``` |  200 Hz |  200 Hz |  200 Hz
```lis3dh_normal_400``` |  400 Hz |  400 Hz |  400 Hz
```lis3dh_normal_1600```|  -      | 1600 Hz |  -
```lis3dh_normal_5000```| 1250 Hz | 5000 Hz |  -

The **easiest way to use the sensor** is simply to initialize it with function ```lis3dh_init_sensor``` and then set it to any measurement mode with function ```lis3dh_set_mode``` to start measurements with the given output data rate (ODR).

```
...
static lis3dh_sensor_t* sensor = 0;
...
if ((sensor = lis3dh_init_sensor (I2C_BUS, LIS3DH_I2C_ADDRESS_2, 0)))
{
    ...
    lis3dh_set_mode (sensor, lis3dh_odr_10, lis3dh_high_res, true, true, true)
    ...
}
...

```
In this example, a LIS3DH sensor connected to I2C is initialized and set to high-resolution mode to start measurements for all three axes with an output data rate (ODR) of 10 Hz.

**Please note:** 
- ```lis3dh_init_sensor``` function resets the sensor completely, switches it to the power down mode, and returns a pointer to a sensor device data structure on success. All registers are reset to default values and the embedded FIFO is cleared.
- All sensor configurations should be done before calling function ```lis3dh_set_mode```. In particular, the interrupt configuration should be performed before to avoid loosing the first interrupt and locking the system.

## Measurement results

### Output data format

The sensor determines periodically the accelerations for all axes that are enabled for measurement and produces output data with the selected output data rate (ODR).

Raw **output data** (**raw data**) are given as 16-bit signed integer values in 2’s complement representation and are always left-aligned. The resolution depends on the selected operation mode and the selected full scale. For example, in low power mode with 8-bit resolution only the high byte is used. LIS3DH allows to select the following full scales:

Full Scale  | Driver symbol | Resolution 12 bit <br>```lis3dh_high_res``` | Resolution 10 bit<br>```lis3dh_normal``` | Resolution 8 bit <br>```lis3dh_low_power```
---------------------:|:-----------:|-----------:|---------------:|-----:
 ±2 g | ```lis3dh_scale_2_g```  |  1 mg  |  4 mg |  16 mg
 ±4 g | ```lis3dh_scale_4_g```  |  2 mg  |  8 mg |  32 mg
 ±8 g | ```lis3dh_scale_8_g```  |  4 mg  | 16 mg |  64 mg
±16 g | ```lis3dh_scale_16_g``` | 12 mg  | 48 mg | 192 mg

By default, a full scale of ±2 g is used. Function ```lis3dh_set_scale``` can be used to change it.

```
lis3dh_set_scale(sensor, lis3dh_scale_4_g);
```

### Fetching output data

To get the information whether new data are available, the user task can either use

- the function ```lis3dh_new_data```  to check periodically whether new output data are available, or
- the data ready interrupt (DRDY) which is thrown as soon as new output data are available (see below).

Last measurement results can then be fetched either 

- as raw data using function ```lis3dh_get_raw_data``` or 
- as floating point values in g using function ```lis3dh_get_float_data```.

It is recommended to use function ```lis3dh_get_float_data``` since it already converts measurement results to real values according to the selected full scale.

```
void user_task_periodic(void *pvParameters)
{
    lis3dh_float_data_t data;

    while (1)
    {
        // execute task every 10 ms
        vTaskDelay (10/portTICK_PERIOD_MS);
        ...
        // test for new data
        if (!lis3dh_new_data (sensor))
            continue;
    
        // fetch new data
        if (lis3dh_get_float_data (sensor, &data))
        {
            // do something with data
            ...
        }
    }
}
```

**Please note:** 
Functions ```lis3dh_get_float_data``` and ```lis3dh_get_raw_data``` always return the last available results. If these functions are called more often than measurements are taken, some measurement results are retrieved multiple times. If these functions are called too rarely, some measurement results will be lost.

### High pass filtering

LIS3DH provides embedded high-pass filtering capabilities to improve measurement results. Please refer the [datasheet](http://www.st.com/resource/en/datasheet/lis3dh.pdf) or [application note](http://www.st.com/resource/en/application_note/cd00290365.pdf) for more details.

The high pass filter can independently apply to 

- the raw output data,
- the data used for click detection, and
- the data used for inertial interrupt generation like wake-up, free fall or 6D/4D orientation detection.

The mode and the cutoff frequency of the high pass filter can be configured using function ```lis3dh_config_hpf```. Following HPF modes are available:

Driver symbol | HPF mode
:--------------|:---------
```lis3dh_hpf_normal```    | Normal mode
```lis3dh_hpf_reference``` | Reference mode
```lis3dh_hpf_autoreset``` | Auto-reset on interrupt

For each output data rate (ODR), 4 different HPF cutoff frequencies can be used. Furthermore, a number of boolean parameters indicate to which data the HPF is applied.

```
...
// configure HPF
lis3dh_config_hpf (sensor, lis3dh_hpf_normal, 0, true, true, true, true);

// reset the reference by dummy read
lis3dh_get_hpf_ref (sensor);
...
```

### FIFO

In order to limit the rate at which the host processor has to fetch the data, the LIS3DH embeds a first-in first-out buffer (FIFO). This is in particular helpful at high output data rates. The FIFO buffer can work in four different modes and is able to store up to 32 accelerometer samples. Please refer the [datasheet](http://www.st.com/resource/en/datasheet/lis3dh.pdf) or [application note](http://www.st.com/resource/en/application_note/cd00290365.pdf) for more details.

Driver symbol | FIFO mode
--------------|-------------------------
```lis3dh_bypass```  | Bypass mode (FIFO is not used)
```lis3dh_fifo```    | FIFO mode
```lis3dh_stream```  | Stream mode
```lis3dh_stream_to_fifo``` | Stream-to-FIFO mode

The FIFO mode can be set using function ```lis3dh_set_fifo_mode```. This function takes three parameters

- the FIFO mode,
- a threshold value which defines a watermark level, and
- an interrupt source that is used in Stream-to-FIFO mode.

The watermark level is used by the sensor to set a watermark flag and to generate optionally an interrupt when the FIFO content exceeds this level. They can be used to gather a minimum number of axes acceleration samples with the sensor before the data are fetched as a single read operation from the sensor.

```
...
// clear FIFO
lis3dh_set_fifo_mode (sensor, lis3dh_bypass,  0, lis3dh_int1_signal);

//  activate FIFO mode
lis3dh_set_fifo_mode (sensor, lis3dh_stream, 10, lis3dh_int1_signal);
...
```

**Please note**: To clear the FIFO at any time, set the FIFO mode to ```lis3dh_bypass``` and back to the desired FIFO mode.

To read data from the FIFO, simply use either 

- the function ```lis3dh_get_raw_data_fifo``` to all get raw output data stored in FIFO or
- the function ```lis3dh_get_float_data_fifo``` to get all data stored in FIFO and converted to real values in g. 

Both functions clear the FIFO and return the number of samples read from the FIFO. 

```
void user_task_periodic (void *pvParameters)
{
    lis3dh_float_data_fifo_t  data;

    while (1)
    {
        // execute task every 500 ms
        vTaskDelay (500/portTICK_PERIOD_MS);
        ...
        // test for new data
        if (!lis3dh_new_data (sensor))
            continue;
    
        // fetch data from fifo
        uint8_t num = lis3dh_get_float_data_fifo (sensor, data);
        
        for (int i = 0; i < num; i++)
        {
           // do something with data[i] ...
        }
}
```

## Interrupts

The LIS3DH supports two dedicated interrupt signals **```INT1```** and **```INT2```** and three different types of interrupts:

- **data** interrupts (data ready and FIFO status),
- **inertial event** interrupts (axis movement, wake-up, free fall, and 6D/4D orientation detection), and
- **click detection** interrupts.

While inertial event interrupts and click detection interrupts can be configured for both interrupt signals, data ready and FIFO status interrupts can be configured only for interrupt signal ```INT1```.

### Data interrupts (data ready and FIFO status)

Following sources can generate an interrupt on signal ```INT1```:

Interrupt source | Driver symbol
:-----------------|:-------------
Output data become ready to read | ```lis3dh_int_data_ready```
FIFO content exceeds the watermark level | ```lis3dh_int_fifo_watermark```
FIFO is completely filled | ```lis3dh_int_fifo_overrun```

Each of these interrupt sources can be enabled or disabled separately with function ```lis3dh_enable_int```. By default all interrupt sources are disabled.

```
lis3dh_enable_int (sensor, lis3dh_int_data_ready, lis3dh_int1_signal, true);
```

Whenever an interrupt is generated at interrupt signal ```INT1```, the function ```lis3dh_get_int_data_source``` can be used to determine the source of the interrupt. This function returns a data structure of type ```lis3dh_int_data_source_t``` that contain a boolean member for each source that can be tested for true.

```
void int1_handler ()
{
    lis3dh_int_data_source_t data_src;

    // get the source of the interrupt on *INT1* signal
    lis3dh_get_int_data_source  (sensor, &data_src);

    // in case of data ready interrupt, get the results and do something with them
    if (data_src.data_ready)
        ... // read data
   
    // in case of FIFO interrupts read the whole FIFO
    else  if (data_src.fifo_watermark || data_src.fifo_overrun)
        ... // read FIFO data
    ...
}
```

### Inertial event interrupts

Inertial interrupt generators allow to generate interrupts when certain inertial event occures (event interrupts), that is, the acceleration of defined axes is higher or lower than a defined threshold. If activated, the acceleration of each axis is compared with a defined threshold to check whether it is below or above the threshold. The results of all activated comparisons are then combined OR or AND to generate the interrupt signal.

The configuration of the threshold valid for all axes, the activated comparisons and the selected AND/OR combination allows to recognize special situations:

- **Wake-up detection** refers the special condition that the acceleration measured along any axis is above the defined threshold (```lis3dh_wake_up```).
- **Free fall detection** refers the special condition that the acceleration measured along all the axes goes to zero (```lis3dh_free_fall```).
- **6D/4D orientation detection** refers to the special condition that the measured acceleration along certain axes is above and along the other axes is below the threshold which indicates a particular orientation (```lis3dh_6d_movement```, ```lis3dh_6d_position```, ```lis3dh_4d_movement```, ```lis3dh_4d_position```).

Inertial event interrupts can be configured with the function ```lis3dh_get_int_event_config```. This function requires as parameters the configuration of type ```lis3dh_int_event_config_t``` and the interrupt generator to be used for inertial event interrupts. 

Inertial event interrupts have to be enabled or disabled using function ```lis3dh_enable_int```. The interrupt signal on which the interrupts are generated is given as parameter.

For example, axis movement detection interrupt generated by inertial interrupt generator 2 on signal ```INT1``` could be configured as following:

```
lis3dh_int_event_config_t event_config;
    
event_config.mode = lis3dh_wake_up;
event_config.threshold = 10;
event_config.x_low_enabled  = false;
event_config.x_high_enabled = true;
event_config.y_low_enabled  = false;
event_config.y_high_enabled = true;
event_config.z_low_enabled  = false;
event_config.z_high_enabled = true;

event_config.duration = 0;
event_config.latch = true;
        
lis3dh_set_int_event_config (sensor, &event_config, lis3dh_int_event2_gen);
lis3dh_enable_int (sensor, lis3dh_int_event1, lis3dh_int1_signal, true);
```

The parameter of type ```lis3dh_int_event_config_t``` also configures

- whether the interrupt should be latched until the interrupt source is read, and 
- which time given in 1/ODR an interrupt condition has to be given before the interrupt is generated.

As with data ready and FIFO status interrupts, function ```lis3dh_get_int_event_source``` can be used to determine the source of an inertial event interrupt whenever it is generated. This function returns a data structure of type ```lis3dh_int_event_source_t``` which contains a boolean member for each source that can be tested for true.

```
void int1_handler ()
{
    lis3dh_int_data_source_t  data_src;
    lis3dh_int_event_source_t event_src;

    // get the source of the interrupt on *INT1* signal
    lis3dh_get_int_data_source  (sensor, &data_src);
    lis3dh_get_int_event_source (sensor, &event_src, lis3dh_int_event2_gen);

    // in case of data ready interrupt, get the results and do something with them
    if (data_src.data_ready)
        ... // read data
   
    // in case of FIFO interrupts read the whole FIFO
    else  if (data_src.fifo_watermark || data_src.fifo_overrun)
        ... // read FIFO data

    // in case of inertial event interrupt
    else if (event_src.active)
        ... // read data
    ...
}
```

**Please note:** If the interrupt is configured to be latched, the interrupt signal is active until the interrupt source is read. Otherwise the interrupt signal is only active as long as the interrupt condition is satisfied.

**Please note** Activating all threshold comparisons and the OR combination (```lis3dh_wake_up```) is the most flexible way to deal with inertial event interrupts. Functions such as free fall detection and so on can then be realized by suitably combining the various interrupt sources by the user task. Following example realizes the free fall detection in user task.

```
lis3dh_int_event_config_t event_config;
    
event_config.mode = lis3dh_wake_up;
event_config.threshold = 10;
event_config.x_low_enabled  = true;
event_config.x_high_enabled = true;
event_config.y_low_enabled  = true;
event_config.y_high_enabled = true;
event_config.z_low_enabled  = true;
event_config.z_high_enabled = true;

event_config.duration = 0;
event_config.latch = true;
        
lis3dh_set_int_event_config (sensor, &event_config, lis3dh_int_event2_gen);
lis3dh_enable_int (sensor, lis3dh_int_event1, lis3dh_int1_signal, true);
```

```
void int1_handler ()
{
    lis3dh_int_event_source_t event_src;

    // get the source of the interrupt from interrupt generator 2 on *INT1* signal
    lis3dh_get_int_event_source (sensor, &event_src, lis3dh_int_event2_gen);

    // test for free fall condition (all accelerations are below the threshold)
    if (event_src.x_low && event_src.y_low && event_src.z_low)
        ...  // do something
    ...
}

```

### Click detection interrupts

A sequence of acceleration values over time measured along certain axes can be used to detect single and double clicks. Please refer the [datasheet](http://www.st.com/resource/en/datasheet/lis3dh.pdf) or [application note](http://www.st.com/resource/en/application_note/cd00290365.pdf) for more information.

Click detection interrupts are configured using the ``` lis3dh_set_int_click_config``` function. This function requires the configuration of type ```lis3dh_int_click_config_t``` as parameter. The interrupt has to be activated or deactivated using the ```lis3dh_enable_int``` function with the interrupt signal on which the interrupts are generated as parameter.

In following example, the single click detection for z-axis is enabled with a time limit of 1/ODR, a time latency of 1/ODR and a time window of 3/ODR.

```
lis3dh_int_click_config_t click_config;
       
click_config.threshold = 10;
click_config.x_single = false;
click_config.x_double = false;
click_config.y_single = false;
click_config.y_double = false;
click_config.z_single = true;
click_config.z_double = false;
click_config.latch = true;
click_config.time_limit   = 1;
click_config.time_latency = 1;
click_config.time_window  = 3;
        
lis3dh_set_int_click_config (sensor, &click_config);
lis3dh_enable_int (sensor, lis3dh_int_click, lis3dh_int1_signal, true);
```

Please refer the [application note](http://www.st.com/resource/en/application_note/cd00290365.pdf) for more information about the configuration parameters.

As with other interrupts, the function ```lis3dh_get_int_click_source``` can be used to determine the source of the interrupt signal whenever it is generated. This function returns a data structure of type ```lis3dh_int_click_source_t``` that contains a boolean member for each source that can be tested for true.

```
void int1_handler ()
{
    lis3dh_int_click_source_t click_src;

    // get the source of the interrupt on *INT1* signal
    lis3dh_get_int_click_source (sensor, &click_src);

    // detect single click along z-axis
    if (click_src.z_click && click_src.s_click)
        ...  // do something
    ...
}

```
**Please note:** If the interrupt is configured to be latched, the interrupt signal is active until the interrupt source is read. Otherwise the interrupt signal is only active as long as the interrupt condition is satisfied.

### Interrupt signal properties

By default, interrupt signals are high active. Using function ```lis3dh_config_int_signals```, the level of the interrupt signal can be changed.

Driver symbol | Meaning
:-------------|:-------
```lis3dh_high_active``` | Interrupt signal is high active (default)
```lis3dh_low_active``` | Interrupt signal is low active


## Analog inputs and temperature sensor

The LIS3DH sensor contains an auxiliary ADC with 3 separate dedicated inputs ADC1, ADC2, and ADC3. ADC3 can be connected to the internal temperatur sensor. The input range is 1200 ± 400 mV. The resolution of the A/D converter is 10 bit in normal and high-resolution mode, but only 8 bit in low-power mode. 

ADC inputs can be activated and deactivated (default) with function ```lis3dh_enable_adc```. If parameter ```temp``` is true, ADC3 is connected to the internal temperature sensor and provides the temperature in degrees.

ADC sampling rate is the same the output data rate (ODR). Results are given as left-aligned 16-bit signed integer values in 2’s complement. Function ```lis3dh_get_adc``` can be used to get the results.

## Low level functions

The LIS3DH is a very complex and flexible sensor with a lot of features. It can be used for a big number of different use cases. Since it is quite impossible to implement a high level interface which is generic enough to cover all the functionality of the sensor for all different use cases, there are two low level interface functions that allow direct read and write access to the registers of the sensor.

```
bool lis3dh_reg_read  (lis3dh_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
bool lis3dh_reg_write (lis3dh_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
```

**Please note**
These functions should only be used to do something special that is not covered by drivers's  high level interface AND if you exactly know what you do and what it might affect. Please be aware that it might always affect the high level interface.


## Usage

First, the hardware configuration has to be established.

### Hardware configurations

Following figure shows a possible hardware configuration for ESP8266 and ESP32 if I2C interface is used to connect the sensor.

```
  +-----------------+     +----------+
  | ESP8266 / ESP32 |     | LIS3DH   |
  |                 |     |          |
  |   GPIO 14 (SCL) >-----> SCL      |
  |   GPIO 13 (SDA) <-----> SDA      |
  |   GPIO 5        <------ INT1     |
  |   GPIO 4        <------ INT2     |
  +-----------------+     +----------+
```

If SPI interface is used, configuration for ESP8266 and ESP32 could look like following.

```
  +-----------------+     +----------+              +-----------------+     +----------+
  | ESP8266         |     | LIS3DH   |              | ESP32           |     | LIS3DH   |
  |                 |     |          |              |                 |     |          |
  |   GPIO 14 (SCK) ------> SCK      |              |   GPIO 16 (SCK) ------> SCK      |
  |   GPIO 13 (MOSI)------> SDI      |              |   GPIO 17 (MOSI)------> SDI      |
  |   GPIO 12 (MISO)<------ SDO      |              |   GPIO 18 (MISO)<------ SDO      |
  |   GPIO 2  (CS)  ------> CS       |              |   GPIO 19 (CS)  ------> CS       |
  |   GPIO 5        <------ INT1     |              |   GPIO 5        <------ INT1     |
  |   GPIO 4        <------ INT2     |              |   GPIO 5        <------ INT2     |
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
spi_bus_init (SPI_BUS, SPI_SCK_GPIO, SPI_MISO_GPIO, SPI_MOSI_GPIO);
```

Once the interfaces are initialized, function ```lis3dh_init_sensor``` has to be called for each LIS3DH sensor in order to initialize the sensor and to check its availability as well as its error state. This function returns a pointer to a sensor device data structure or NULL in case of error.

The parameter *bus* specifies the ID of the I2C or SPI bus to which the sensor is connected.

```
static lis3dh_sensor_t* sensor;
```

For sensors connected to an I2C interface, a valid I2C slave address has to be defined as parameter *addr*. In that case parameter *cs* is ignored.

```
sensor = lis3dh_init_sensor (I2C_BUS, LIS3DH_I2C_ADDRESS_1, 0);

```

If parameter *addr* is 0, the sensor is connected to a SPI bus. In that case, parameter *cs* defines the GPIO used as CS signal.

```
sensor = lis3dh_init_sensor (SPI_BUS, 0, SPI_CS_GPIO);

```

The remaining of the program is independent on the communication interface.

#### Configuring the sensor

Optionally, you could wish to set some measurement parameters. For details see the sections above, the header file of the driver ```lis3dh.h```, and of course the data sheet of the sensor.

#### Starting measurements

As last step, the sensor mode has be set to start periodic measurement. The sensor mode can be changed anytime later.

```
...
// start periodic measurement with output data rate of 10 Hz
lis3dh_set_mode (sensor, lis3dh_odr_10, lis3dh_high_res, true, true, true);
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
    lis3dh_float_data_t data;

    while (1)
    {
        // execute task every 10 ms
        vTaskDelay (10/portTICK_PERIOD_MS);
        ...
        // test for new data
        if (!lis3dh_new_data (sensor))
            continue;
    
        // fetch new data
        if (lis3dh_get_float_data (sensor, &data))
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

A different approach is to use one of the **interrupts** INT1 or INT2. In this case, the user has to implement an interrupt handler that either fetches the data directly or triggers a task, that is waiting to fetch the data.

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
            if (!lis3dh_new_data (sensor))
                continue;
    
            // fetch new data
            if (lis3dh_get_float_data (sensor, &data))
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

Furthermore, the interrupts have to be enabled and configured in the LIS3DH sensor, see section **Interrupts** above.

## Full Example

```
/* -- use following constants to define the example mode ----------- */

// #define SPI_USED     // SPI interface is used, otherwise I2C
// #define FIFO_MODE    // multiple sample read mode
// #define INT_DATA     // data interrupts used (data ready and FIFO status)
// #define INT_EVENT    // inertial event interrupts used (wake-up, free fall or 6D/4D orientation)
// #define INT_CLICK    // click detection interrupts used

#if defined(INT_DATA) || defined(INT_EVENT) || defined(INT_CLICK)
#define INT_USED
#endif

/* -- includes ----------------------------------------------------- */

#include "lis3dh.h"

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
#define INT1_PIN      5
#define INT2_PIN      4

/* -- user tasks --------------------------------------------------- */

static lis3dh_sensor_t* sensor;

/**
 * Common function used to get sensor data.
 */
void read_data ()
{
    #ifdef FIFO_MODE

    lis3dh_float_data_fifo_t fifo;

    if (lis3dh_new_data (sensor))
    {
        uint8_t num = lis3dh_get_float_data_fifo (sensor, fifo);

        printf("%.3f LIS3DH num=%d\n", (double)sdk_system_get_time()*1e-3, num);

        for (int i=0; i < num; i++)
            // max. full scale is +-16 g and best resolution is 1 mg, i.e. 5 digits
            printf("%.3f LIS3DH (xyz)[g] ax=%+7.3f ay=%+7.3f az=%+7.3f\n",
                   (double)sdk_system_get_time()*1e-3, 
                   fifo[i].ax, fifo[i].ay, fifo[i].az);
    }

    #else

    lis3dh_float_data_t  data;

    if (lis3dh_new_data (sensor) &&
        lis3dh_get_float_data (sensor, &data))
        // max. full scale is +-16 g and best resolution is 1 mg, i.e. 5 digits
        printf("%.3f LIS3DH (xyz)[g] ax=%+7.3f ay=%+7.3f az=%+7.3f\n",
               (double)sdk_system_get_time()*1e-3, 
                data.ax, data.ay, data.az);
        
    #endif // FIFO_MODE
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
    {
        if (xQueueReceive(gpio_evt_queue, &gpio_num, portMAX_DELAY))
        {
            lis3dh_int_data_source_t  data_src  = {};
            lis3dh_int_event_source_t event_src = {};
            lis3dh_int_click_source_t click_src = {};

            // get the source of the interrupt and reset *INTx* signals
            #ifdef INT_DATA
            lis3dh_get_int_data_source  (sensor, &data_src);
            #endif
            #ifdef INT_EVENT
            lis3dh_get_int_event_source (sensor, &event_src, lis3dh_int_event1_gen);
            #endif
            #ifdef INT_CLICK
            lis3dh_get_int_click_source (sensor, &click_src);
            #endif
    
            // in case of DRDY interrupt or inertial event interrupt read one data sample
            if (data_src.data_ready)
                read_data ();
   
            // in case of FIFO interrupts read the whole FIFO
            else  if (data_src.fifo_watermark || data_src.fifo_overrun)
                read_data ();
    
            // in case of event interrupt
            else if (event_src.active)
            {
                printf("%.3f LIS3DH ", (double)sdk_system_get_time()*1e-3);
                if (event_src.x_low)  printf("x is lower than threshold\n");
                if (event_src.y_low)  printf("y is lower than threshold\n");
                if (event_src.z_low)  printf("z is lower than threshold\n");
                if (event_src.x_high) printf("x is higher than threshold\n");
                if (event_src.y_high) printf("y is higher than threshold\n");
                if (event_src.z_high) printf("z is higher than threshold\n");
            }

            // in case of click detection interrupt   
            else if (click_src.active)
               printf("%.3f LIS3DH %s\n", (double)sdk_system_get_time()*1e-3, 
                      click_src.s_click ? "single click" : "double click");
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
    sensor = lis3dh_init_sensor (SPI_BUS, 0, SPI_CS_GPIO);
    
    #else

    // init all I2C bus interfaces at which LIS3DH  sensors are connected
    i2c_init (I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ);
    
    // init the sensor with slave address LIS3DH_I2C_ADDRESS_1 connected to I2C_BUS.
    sensor = lis3dh_init_sensor (I2C_BUS, LIS3DH_I2C_ADDRESS_1, 0);

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

        // configure interupt pins for *INT1* and *INT2* signals and set the interrupt handler
        gpio_enable(INT1_PIN, GPIO_INPUT);
        gpio_set_interrupt(INT1_PIN, GPIO_INTTYPE_EDGE_POS, int_signal_handler);

        #endif  // INT_USED
        
        /** -- SENSOR CONFIGURATION PART --- */

        // set polarity of INT signals if necessary
        // lis3dh_config_int_signals (sensor, lis3dh_high_active);

        #ifdef INT_DATA
        // enable data interrupts on INT1 (data ready or FIFO status interrupts)
        // data ready and FIFO status interrupts must not be enabled at the same time
        #ifdef FIFO_MODE
        lis3dh_enable_int (sensor, lis3dh_int_fifo_overrun  , lis3dh_int1_signal, true);
        lis3dh_enable_int (sensor, lis3dh_int_fifo_watermark, lis3dh_int1_signal, true);
        #else
        lis3dh_enable_int (sensor, lis3dh_int_data_ready, lis3dh_int1_signal, true);
        #endif // FIFO_MODE
        #endif // INT_DATA
        
        #ifdef INT_EVENT
        // enable data interrupts on INT1 
        lis3dh_int_event_config_t event_config;
    
        event_config.mode = lis3dh_wake_up;
        // event_config.mode = lis3dh_free_fall;
        // event_config.mode = lis3dh_6d_movement;
        // event_config.mode = lis3dh_6d_position;
        // event_config.mode = lis3dh_4d_movement;
        // event_config.mode = lis3dh_4d_position;
        event_config.threshold = 10;
        event_config.x_low_enabled  = false;
        event_config.x_high_enabled = true;
        event_config.y_low_enabled  = false;
        event_config.y_high_enabled = true;
        event_config.z_low_enabled  = false;
        event_config.z_high_enabled = true;
        event_config.duration = 0;
        event_config.latch = true;
        
        lis3dh_set_int_event_config (sensor, &event_config, lis3dh_int_event1_gen);
        lis3dh_enable_int (sensor, lis3dh_int_event1, lis3dh_int1_signal, true);
        #endif // INT_EVENT

        #ifdef INT_CLICK
        // enable click interrupt on INT1
        lis3dh_int_click_config_t click_config;
        
        click_config.threshold = 10;
        click_config.x_single = false;
        click_config.x_double = false;        
        click_config.y_single = false;
        click_config.y_double = false;        
        click_config.z_single = true;
        click_config.z_double = false;
        click_config.latch = true;
        click_config.time_limit   = 1;
        click_config.time_latency = 1;
        click_config.time_window  = 3;
        
        lis3dh_set_int_click_config (sensor, &click_config);
        lis3dh_enable_int (sensor, lis3dh_int_click, lis3dh_int1_signal, true);
        #endif // INT_CLICK

        #ifdef FIFO_MODE
        // clear FIFO and activate FIFO mode if needed
        lis3dh_set_fifo_mode (sensor, lis3dh_bypass,  0, lis3dh_int1_signal);
        lis3dh_set_fifo_mode (sensor, lis3dh_stream, 10, lis3dh_int1_signal);
        #endif

        // configure HPF and reset the reference by dummy read
        lis3dh_config_hpf (sensor, lis3dh_hpf_normal, 0, true, true, true, true);
        lis3dh_get_hpf_ref (sensor);
        
        // enable ADC inputs and temperature sensor for ADC input 3
        lis3dh_enable_adc (sensor, true, true);
        
        // LAST STEP: Finally set scale and mode to start measurements
        lis3dh_set_scale(sensor, lis3dh_scale_2_g);
        lis3dh_set_mode (sensor, lis3dh_odr_10, lis3dh_high_res, true, true, true);

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
        printf("Could not initialize LIS3DH sensor\n");
}

```
