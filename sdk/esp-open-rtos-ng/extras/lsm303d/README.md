# Driver for the LSM303D e-Compass 3D accelerometer and 3D magnetometer module

The driver is for the usage with the ESP8266 and [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos). If you can't find it in folder [extras/lsm303d](https://github.com/SuperHouse/esp-open-rtos/tree/master/extras) of original repository, it is not yet merged. Please take a look to branch [lsm303d](https://github.com/gschorcht/esp-open-rtos/tree/lsm303d) of my fork in that case.

It is also working with ESP32 and [ESP-IDF](https://github.com/espressif/esp-idf.git) using a wrapper component for ESP8266 functions, see folder ```components/esp8266_wrapper```, as well as Linux based systems using a wrapper library.

## About the sensor

LSM303D is a high performance **3D digital linear acceleration and magnetic sensor** connected to **I2C** or **SPI** with a full scale of up to **±16 g** and **±12 Gauss**.

**Main features** of the sensor are:
 
- 3 magnetic field channels
- 3 acceleration channels
- ±2/±4/±8/±12 Gauss dynamically selectable magnetic full-scale
- ±2/±4/±6/±8/±16 g dynamically selectable linear acceleration full-scale
- different measuring rates for accelerator and magnetometer
- 16-bit data output
- embedded temperature sensor
- embedded 32 levels of 16 bit data output FIFO
- integrated high-pass filters
- programmable interrupt generators
- 6D/4D orientation detection
- free-fall detection
- motion detection
- single click and double click detection
- magnetic field detection

## Sensor operation

### Sensor modes

LSM303D provides different operating modes.

- **Power-down mode** is configured automatically after power up boot sequence. In this mode, almost all internal blocks of the device are switched off. Register content is preserved, but there are no measurements performed.

- **Normal mode**: is the operation mode in which measurements are performed at a defined output data rate (**ODR**).

The operation mode can be defined separately for the accelerator and the magnetometer.

### Output Data Rates

In normal mode, the accelerator and/or the magnetometer perform measurements with a defined output data rates (ODR). This output data rate can be defined separately for the accelerometer and the magnetometer.

**Acceleration sensor** supports the following output data rates

Mode | Output data rate (ODR) | Driver symbol
:------------- |---------:|:---------------
Power-down | - | ```lsm303d_a_power_down``` 
Normal mode | 3.125 Hz | ```lsm303d_a_odr_3_125```
Normal mode | 6.25 Hz  | ```lsm303d_a_odr_6_25```
Normal mode | 12.5 Hz  | ```lsm303d_a_odr_12_5```
Normal mode | 25 Hz    | ```lsm303d_a_odr_25```
Normal mode | 50 Hz    | ```lsm303d_a_odr_50```
Normal mode | 100 Hz   | ```lsm303d_a_odr_100```
Normal mode | 200 Hz   | ```lsm303d_a_odr_200```
Normal mode | 400 Hz   | ```lsm303d_a_odr_400```
Normal mode | 800 Hz   | ```lsm303d_a_odr_800```
Normal mode | 1600 Hz   | ```lsm303d_a_odr_1600```

**Magnetic sensor** supports the following output data rates

Mode | Output data rate (ODR) | Driver symbol
:------------- |---------:|:---------------
Power-down | - | ```lsm303d_m_do_not_use``` 
Normal mode | 3.125 Hz | ```lsm303d_m_odr_3_125```
Normal mode | 6.25 Hz  | ```lsm303d_m_odr_6_25```
Normal mode | 12.5 Hz  | ```lsm303d_m_odr_12_5```
Normal mode | 25 Hz    | ```lsm303d_m_odr_25```
Normal mode | 50 Hz    | ```lsm303d_m_odr_50```
Normal mode | 100 Hz   | ```lsm303d_m_odr_100```

**Please note**: An output data rate of 100 Hz is only available for the magnetometer if the output data rate of the accelerometer is greater than 50 Hz or the accelerometer is in power-down mode.

### Setting operation mode and output data rate

The **easiest way to use the sensor** is to initialize it with the ```lsm303d_init_sensor``` function and then switch it to any measurement mode to start measurement with a given output data rate (ODR). The acceleration sensor mode is set with the ```lsm303d_set_a_mode``` function and the magnetometer mode is set with lsm303d_set_m_mode function.
```
...
static lsm303d_sensor_t* sensor = 0;
...
if ((sensor = lsm303d_init_sensor (I2C_BUS, LSM303D_I2C_ADDRESS_2, 0)))
{
    ...
    lsm303d_set_a_mode (sensor, lsm303d_a_odr_12_5, lsm303d_a_aaf_bw_773, true, true, true);
    lsm303d_set_m_mode (sensor, lsm303d_m_odr_12_5, lsm303d_m_low_res, lsm303d_m_continuous);
    ...
}
...

```
In this example, the LSM303D sensor is connected to I2C. After its initialization, both the accelerometer and the magnetometer are switched to the normal measuring mode with an output data rate of 12.5 Hz each.

```lsm303d_set_a_mode``` function requires the bandwidth of the anti-alias filter (AAF) and the activated axes x, y and z as additional parameters. Possible bandwidths of the accelerator anti-alias filter are:

AAF Bandwidth | Driver symbol
-------------:|:-------------
773 Hz | ```lsm303d_a_aaf_bw_773```
362 Hz | ```lsm303d_a_aaf_bw_362```
194 Hz | ```lsm303d_a_aaf_bw_194```
50 Hz  | ```lsm303d_a_aaf_bw_50```

```lsm303d_set_m_mode``` function requires the resolution and the measurement mode as additional parameters. It is possible to define 

- low resolution (```lsm303d_m_low_res```) and 
- high resolution (```lsm303d_m_low_res```). 

Unfortunately, it is not documented what it exactly means. As measurement mode, the user can select 

- continuous conversion mode (```lsm303d_m_continuous```),
- single conversion mode (```lsm303d_m_single```), or
- power-down mode (```lsm303d_m_power_down```).

During single-conversion mode, the device performs a single measurement and switches then back to the power-down mode. During continuous-conversion mode, the device continuously performs measurements with defined output data rate. In power-down mode the magnetometer is switched off.

**Please note:** 
- ```lsm303d_init_sensor``` function resets the sensor completely. That is, all sensor registers are reset to their default values and the sensor is switched to the power-down mode. The function returns a pointer to an sensor device data structure on success which is allocated from memory.
- All sensor configurations should be done before calling ```lsm303d_a_set_mode``` or ```lsm303d_a_set_mode``` function. In particular, the interrupt configuration should be performed before to avoid loosing the first interrupt and locking the system.

## Measurement results

### Output data format

The sensor determines periodically the accelerations and/or magnetic values for all axes that are enabled for measurement and produces output data with the selected output data rate (ODR).

Raw **output data** (**raw data**) are given as 16-bit signed integer values in 2’s complement representation and are always left-aligned. The range and the resolution of raw data depend on the sensitivity of the sensor which is selected by the **full scale** parameter. LSM303D allows to select the following full scales:

Accelerometer full scale  | Resolution | Driver symbol
---------------------:|-----------:|:-----------
 ±2 g |  0.061 mg | ```lsm303d_a_scale_2_g```
 ±4 g |  0.122 mg | ```lsm303d_a_scale_4_g```
 ±6 g |  0.183 mg | ```lsm303d_a_scale_6_g``` 
 ±8 g |  0.244 mg | ```lsm303d_a_scale_8_g```
±16 g |  0.732 mg | ```lsm303d_a_scale_16_g```

Magnetormeter full scale | Resolution   | Driver symbol
---------------------:|:-----------|:------
 ±2 Gauss  |  0.080 mGauss | ```lsm303d_m_scale_2_Gs```
 ±4 Gauss  |  0.160 mGauss | ```lsm303d_m_scale_4_Gs```
 ±8 Gauss  |  0.320 mGauss | ```lsm303d_m_scale_8_Gs```
±12 Gauss  |  0.479 mGauss | ```lsm303d_m_scale_12_Gs```

By default, a full scale of ±2 g is used for the acceleration and ±2 Gauss for the magnetic measurement. ```lsm303d_set_a_scale``` and ```lsm303d_set_m_scale``` functions can be used to change these values.

```
lsm303d_set_a_scale(sensor, lsm303d_a_scale_4_g);
lsm303d_set_m_scale(sensor, lsm303d_m_scale_8_Gs);
```

### Fetching output data

To get the information whether new data are available, the user task can either use

- the ```lsm303d_new_a_data``` and ```lsm303d_new_m_data``` functions to check periodically whether new output data are available, or
- the data ready interrupt (DRDY) which are thrown as soon as new output data are available (see below).

Once new new data are available, they can be retrieved either

- as raw data with the ```lsm303d_get_raw_a_data``` and ```lsm303d_get_raw_m_data``` functions, or
- as floating-point values with the ```lsm303d_get_float_a_data``` and ```lsm303d_get_float_m_data``` functions in g and Gauss, respectively.

It is recommended to use ```lsm303d_get_a_float_data``` and ```lsm303d_get_float_m_data``` functions since they already converts measurement results to real values according to the selected full scales.

```
void user_task_periodic(void *pvParameters)
{
    lsm303d_float_a_data_t a_data;

    while (1)
    {
        // execute task every 10 ms
        vTaskDelay (10/portTICK_PERIOD_MS);
        ...
        // test for new accelerator data and fetch them
        if (lsm303d_new_a_data (sensor) &&
            lsm303d_get_float_a_data (sensor, &a_data))
        {
            // do something with data
            ...
        }
        
        lsm303d_float_m_data_t  m_data;

        // test for new magnetometer data and fetch them
        if (lsm303d_new_m_data (sensor) &&
            lsm303d_get_float_m_data (sensor, &m_data))
        
        {
            // do something with data
            ...
        }
        ...
    }
}
```

**Please note:** 
```lsm303d_get_float_a_data```, ```lsm303d_get_raw_a_data```, ```lsm303d_get_float_m_data```, and ```lsm303d_get_raw_m_data``` functions always return the last available results. If these functions are called more often than measurements are taken, some measurement results are retrieved multiple times. If these functions are called too rarely, some measurement results will be lost.

### High pass filtering

LSM303D provides embedded high-pass filtering capabilities for acceleration data to improve measurement results. Please refer the [datasheet](http://www.st.com/resource/en/datasheet/lsm303d.pdf).

The high pass filter (HPF) can independently apply to 

- the raw accelerator output data,
- the accelerator data used for click detection, and
- the accelerator data used for inertial interrupt generation like wake-up, free fall or 6D/4D orientation detection.

The mode of the high pass filter can be configured using ```lsm303d_config_a_hpf``` function. Following HPF modes are available:

HPF mode | Driver symbol
:--------------|:---------
Normal mode | ```lsm303d_hpf_normal```
Reference mode | ```lsm303d_hpf_reference```
Auto-reset on interrupt | ```lsm303d_hpf_autoreset```

The cutoff frequencies of the HPF are fixed and not documented.

```
...
// configure HPF and implicitly reset the reference by a dummy read
lsm303d_config_a_hpf (sensor, lsm303d_hpf_normal, true, true, true, true);
...
```

If HPF normal mode (```lsm303d_hpf_normal```) is used, ```lsm303d_config_a_hpf``` function implicitly reads the reference registers to reset the reference.

### FIFO

In order to limit the rate at which the host processor has to fetch the data, the LSM303D embeds a first-in first-out buffer (FIFO) for accelerator output data. This is in particular helpful at high output data rates. The FIFO buffer can work in different modes and is able to store up to 32 accelerometer samples. Please refer the [datasheet](http://www.st.com/resource/en/datasheet/lsm303d.pdf).

FIFO mode | Driver symbol
--------------|-------------------------
Bypass mode (FIFO is not used) | ```lsm303d_bypass```
FIFO mode                      | ```lsm303d_fifo```
Stream mode                    | ```lsm303d_stream```
Stream-to-FIFO mode            | ```lsm303d_stream_to_fifo```
Bypass-to-Stream mode          | ```lsm303d_bypass_to_stream```

The FIFO mode can be set using function ```lsm303d_set_fifo_mode```. This function takes as parameters

- the FIFO mode, and
- a threshold value.

The threshold value is used by the sensor to set a flag and to generate optionally an interrupt when the FIFO content exceeds this value. It can be used to gather a minimum number of axes acceleration samples by the sensor before the data are fetched from the sensor as a single read operation.

```
...
// clear FIFO
lsm303d_set_fifo_mode (sensor, lsm303d_bypass,  0);

//  activate FIFO mode
lsm303d_set_fifo_mode (sensor, lsm303d_stream, 10);
...
```

In this example an the threshold flag would be set, when 11 acceleration samples are stored in the FIFO.

**Please note**: 
- To clear the FIFO at any time, set the FIFO mode to ```lsm303d_bypass``` and back to the desired FIFO mode.
- When FIFO is used, ```lsm303d_new_a_data``` function can also be used to check whether there are new data stored in the FIFO. It returns true if at least one acceleration data sample is stored in the FIFO.

To read data from the FIFO, simply use either ```lsm303d_get_raw_a_data_fifo``` or ```lsm303d_get_float_a_data_fifo``` function to all get accelerator output data stored in the FIFO. Both functions clear the FIFO implicitly and return the number of samples read from the FIFO. 

```
void user_task_periodic (void *pvParameters)
{
    lsm303d_float_a_data_fifo_t  fifo;

    while (1)
    {
        // execute task every 500 ms
        vTaskDelay (500/portTICK_PERIOD_MS);
        ...

        // test for new accelerator data data
        if (lsm303d_new_a_data (sensor))
        {
            // fetch the accelerator data stored in FIFO
            uint8_t num = lsm303d_get_float_a_data_fifo (sensor, fifo);

            for (int i = 0; i < num; i++)
            {
               // do something with data[i] ...
            }
        }
        ...
}
```

## Interrupts

The LSM303D supports two dedicated interrupt signals **```INT1```** and **```INT2```** and four different types of interrupts:

- **data** interrupts (data ready, FIFO status),
- **inertial event** interrupts (axis movement, free fall, 6D/4D orientation detection),
- **click detection** interrupts (single click, double click) 
- **magnetic threshold** detection interrupts.

### Data interrupts (data ready and FIFO status)

Following sources can generate data interrupts:

Interrupt source | Interrupt Signals | Driver symbol
:-----------------|:-------------|:----------------
Accelerator data become ready to read | ```INT1```, ```INT2``` | ```lsm303d_int_a_data_ready```
Magnetometer data become ready to read | ```INT1```, ```INT2``` | ```lsm303d_int_m_data_ready```
FIFO becomes empty | ```INT1``` | ```lsm303d_int_fifo_thresh```
FIFO content exceeds the threshold value | ```INT2``` | ```lsm303d_int_fifo_thresh```
FIFO is completely filled | ```INT2``` | ```lsm303d_int_fifo_overrun```

Each of these interrupt sources can be enabled or disabled separately using the ```lsm303d_enable_int``` function. By default, all interrupt sources are disabled.

```
lsm303d_enable_int (sensor, lsm303d_int_a_data_ready, lsm303d_int2_signal, true);
```

Whenever a data interrupt is generated at the specified interrupt signal, the ```lsm303d_get_int_data_source``` function can be used to determine the source of the data interrupt. This function returns a data structure of type ```lsm303d_int_data_source_t``` that contain a boolean member for each source that can be tested for true.

```
void int2_handler ()
{
    lsm303d_int_data_source_t data_src;

    // get the source of the interrupt on *INT2* signal
    lsm303d_get_int_data_source  (sensor, &data_src);

    // in case of data ready interrupt, get the results and do something with them
    if (data_src.a_data_ready)
        ... // read accelerator data
   
    // in case of FIFO interrupts read the whole FIFO
    else  if (data_src.fifo_thresh || data_src.fifo_overrun)
        ... // read FIFO data
    ...
}
```

**Please note:** While FIFO interrupts are reset as soon as the interrupt source is read, the data-ready interrupts are not reset until the data has been read.

### Inertial event interrupts

Inertial interrupt generators allow to generate interrupts when certain inertial events occur (event interrupts), that is, the acceleration of defined axes is higher or lower than a defined threshold. If activated, the acceleration of each axis is compared with a defined threshold to check whether it is below or above the threshold. The results of all activated comparisons are then combined OR or AND to generate the interrupt signal.

The configuration of the threshold, the activated comparisons and the selected AND/OR combination allows to recognize special situations:

- **Axis movement** refers the special condition that the acceleration measured along any axis is above the defined threshold (```lsm303d_or```).
- **Free fall detection** refers the special condition that the acceleration measured along all the axes goes to zero (```lsm303d_and```).
- **6D/4D orientation detection** refers to the special condition that the measured acceleration along certain axes is above and along the other axes is below the threshold which indicates a particular orientation (```lsm303d_6d_movement```, ```lsm303d_6d_position```, ```lsm303d_4d_movement```, ```lsm303d_4d_position```).

Inertial event interrupts can be configured with the ```lsm303d_set_int_event_config``` function. This function requires as parameters the configuration of type ```lsm303d_int_event_config_t``` and the interrupt generator to be used for inertial event interrupts. 

Inertial event interrupts have to be enabled or disabled using ```lsm303d_enable_int``` function. The interrupt signal on which the interrupts are generated is given as parameter.

For example, axis movement detection interrupt generated by inertial interrupt generator 2 on signal ```INT1``` could be configured as following:

```
lsm303d_int_event_config_t event_config;
    
event_config.mode = lsm303d_or;
event_config.threshold = 50;
event_config.x_low_enabled  = false;
event_config.x_high_enabled = true;
event_config.y_low_enabled  = false;
event_config.y_high_enabled = true;
event_config.z_low_enabled  = false;
event_config.z_high_enabled = true;

event_config.duration = 0;
event_config.latch = true;
        
lsm303d_set_int_event_config (sensor, &event_config, lsm303d_int_event2_gen);
lsm303d_enable_int (sensor, lsm303d_int_event1, lsm303d_int1_signal, true);
```

The parameter of type ```lsm303d_int_event_config_t``` also configures

- whether the interrupt should be latched until the interrupt source is read, and 
- which time given in 1/ODR an interrupt condition has to be given before the interrupt is generated.

As with data ready and FIFO status interrupts, ```lsm303d_get_int_event_source``` function can be used to determine the source of an inertial event interrupt whenever it is generated. This function returns a data structure of type ```lsm303d_int_event_source_t``` which contains a boolean member for each source that can be tested for true.

```
void int1_handler ()
{
    lsm303d_int_event_source_t event_src;

    // get the source of the interrupt from interrupt generator 2 on *INT1* signal
    lsm303d_get_int_event_source (sensor, &event_src, lsm303d_int_event2_gen);

    // in case of inertial event interrupt from interrupt generator 2
    if (event_src.active)
         ... // do something
    ...
}
```

**Please note:** If the interrupt is configured to be latched, the interrupt signal is active until the interrupt source is read. Otherwise the interrupt signal is only active as long as the interrupt condition is satisfied.

**Please note** Activating all threshold comparisons and the OR combination (```lsm303d_or```) is the most flexible way to deal with inertial event interrupts. Functions such as free fall detection and so on can then be realized by suitably combining the various interrupt sources by the user task. Following example realizes the free fall detection in user task.

```
lsm303d_int_event_config_t event_config;
    
event_config.mode = lsm303d_or;
event_config.threshold = 10;
event_config.x_low_enabled  = true;
event_config.x_high_enabled = true;
event_config.y_low_enabled  = true;
event_config.y_high_enabled = true;
event_config.z_low_enabled  = true;
event_config.z_high_enabled = true;

event_config.duration = 0;
event_config.latch = true;
        
lsm303d_set_int_event_config (sensor, &event_config, lsm303d_int_event2_gen);
lsm303d_enable_int (sensor, lsm303d_int_event1, lsm303d_int1_signal, true);
```

```
void int1_handler ()
{
    lsm303d_int_event_source_t event_src;

    // get the source of the interrupt from interrupt generator 2 on *INT1* signal
    lsm303d_get_int_event_source (sensor, &event_src, lsm303d_int_event2_gen);

    // test for free fall condition (all accelerations are below the threshold)
    if (event_src.x_low && event_src.y_low && event_src.z_low)
        ... // do something
    ...
}

```

### Click detection interrupts

A sequence of acceleration values over time measured along certain axes can be used to detect single and double clicks. Please refer the [datasheet](http://www.st.com/resource/en/datasheet/lsm303d.pdf).

Click detection interrupts are configured using the ``` lsm303d_set_int_click_config``` function. This function requires the configuration of type ```lsm303d_int_click_config_t``` as parameter. The interrupt has to be activated or deactivated using the ```lsm303d_enable_int``` function with the interrupt signal on which the interrupts are generated as parameter.

In following example, the single click detection for z-axis is enabled with a time limit of 1/ODR, a time latency of 1/ODR and a time window of 3/ODR.

```
lsm303d_int_click_config_t click_config;
       
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
        
lsm303d_set_int_click_config (sensor, &click_config);
lsm303d_enable_int (sensor, lsm303d_int_click, lsm303d_int1_signal, true);
```

**Please note:** Because there is no application note for the LSM303D sensor, please refer to the [application note](http://www.st.com/resource/en/application_note/cd00290365.pdf) for the LIS3DH sensor for more information about the configuration parameters that are used in the same way.

As with other interrupts, the function ```lsm303d_get_int_click_source``` can be used to determine the source of the interrupt signal whenever it is generated. This function returns a data structure of type ```lsm303d_int_click_source_t``` that contains a boolean member for each source that can be tested for true.

```
void int1_handler ()
{
    lsm303d_int_click_source_t click_src;

    // get the source of the interrupt on *INT1* signal
    lsm303d_get_int_click_source (sensor, &click_src);

    // detect single click along z-axis
    if (click_src.z_click && click_src.s_click)
        ... // do something
    ...
}

```
**Please note:** If the interrupt is configured to be latched, the interrupt signal is active until the interrupt source is read. Otherwise the interrupt signal is only active as long as the interrupt condition is satisfied.

### Magnetic threshold interrupts

Magnetic threshold detection of LSM303D allows to generate interrupts whenever measured magnetic data exceed a defined threshold value at positive or negative side. It can be enabled for each axis separately. The defined threshold is valid for all enabled axes.

Magnetic threshold interrupts can be configured with ```lsm303d_set_int_m_thresh_config``` function . This function requires configuration of type ```lsm303d_int_m_thresh_config_t``` as parameter. The interrupt has to be activated or deactivated using the ```lsm303d_enable_int``` function with the interrupt signal on which the interrupts are generated as parameter.


```
lsm303d_int_m_thresh_config_t m_thresh_config;
    
m_thresh_config.threshold    = 2000;
m_thresh_config.x_enabled    = true;
m_thresh_config.y_enabled    = true;
m_thresh_config.z_enabled    = true;
m_thresh_config.latch        = true;
m_thresh_config.signal_level = lsm303d_high_active;
        
lsm303d_set_int_m_thresh_config (sensor, &m_thresh_config);
lsm303d_enable_int (sensor, lsm303d_int_m_thresh, lsm303d_int1_signal, true);

```

In this example, magnetic threshold detection is enabled for all axes and a threshold of 2000 is defined. 

The parameter of type ```lsm303d_int_m_thresh_config_t``` also configures

- whether the interrupt signal should latched until the interrupt source is read, and 
- whether the interrupt signal is high (default) or low active.

```lsm303d_get_int_m_thresh_source``` function can be used to determine the source of an magnetic threshold interrupt whenever it is generated. This function returns a data structure of type ```lsm303d_int_m_thresh_config_t``` which contains a boolean member for each source that can be tested for true.

```
void int1_handler ()
{
    lsm303d_int_m_thresh_source_t  thresh_src;

    // get the source of the interrupt on signal *INT1*
    lsm303d_get_int_m_thresh_source(sensor, &thresh_src);
    
    // test the source of the interrupt
    if (thresh_src.active)
    {
        if (thresh_srcx_pos || thresh_srcx_neg) ... ; // do something
        if (thresh_srcy_pos || thresh_srcy_neg) ... ; // do something
        if (thresh_srcz_pos || thresh_srcz_neg) ... ; // do something
    }
    ...
}
```

**Please note:** If the interrupt is configured to be latched, the interrupt signal is active until the interrupt source is read. Otherwise the interrupt signal is only active as long as the interrupt condition is satisfied.

### Interrupt signal properties

By default, interrupt signals are high active. For magnetic threshold interrupts this can be changed using the configuration. The type of the interrupt outputs can be changed using the ```lsm303d_config_int_signals``` function.

Type | Driver symbol 
:-------------|:-------
Interrupt output is pushed/pulled | ```lsm303d_push_pull```
Interrupt output is open-drain    | ```lsm303d_open_drain```


```
lsm303d_config_int_signals (sensor, lsm303d_push_pull);
```

## Temperature sensor

The LIS3MDL sensor contains an internal temperature sensor. It can be activated and deactivated with the ```lsm303d_enable_temperature``` function. Using ```lsm303d_get_temperature``` function, the temperature can be determined as a floating point value in degrees. The temperature is measured by the sensor at the same rate as the magnetic data.

## Low level functions

The LSM303D is a very complex and flexible sensor with a lot of features. It can be used for a big number of different use cases. Since it is quite impossible to implement a high level interface which is generic enough to cover all the functionality of the sensor for all different use cases, there are two low level interface functions that allow direct read and write access to the registers of the sensor.

```
bool lsm303d_reg_read  (lsm303d_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
bool lsm303d_reg_write (lsm303d_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
```

**Please note**
These functions should only be used to do something special that is not covered by driver's  high level interface AND if you exactly know what you do and what it might affect. Please be aware that it might always affect the high level interface.


## Usage

First, the hardware configuration has to be established.

### Hardware configurations

Following figure shows a possible hardware configuration for ESP8266 and ESP32 if I2C interface is used to connect the sensor.

```
  +-----------------+     +----------+
  | ESP8266 / ESP32 |     | LSM303D  |
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
  | ESP8266         |     | LSM303D  |              | ESP32           |     | LSM303D  |
  |                 |     |          |              |                 |     |          |
  |   GPIO 14 (SCK) ------> SCK      |              |   GPIO 16 (SCK) ------> SCK      |
  |   GPIO 13 (MOSI)------> SDI      |              |   GPIO 17 (MOSI)------> SDI      |
  |   GPIO 12 (MISO)<------ SDO      |              |   GPIO 18 (MISO)<------ SDO      |
  |   GPIO 2  (CS)  ------> CS       |              |   GPIO 19 (CS)  ------> CS       |
  |   GPIO 5        <------ INT1     |              |   GPIO 5        <------ INT1     |
  |   GPIO 4        <------ INT2     |              |   GPIO 4        <------ INT2     |
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

Once the interfaces are initialized, function ```lsm303d_init_sensor``` has to be called for each LSM303D sensor in order to initialize the sensor and to check its availability as well as its error state. This function returns a pointer to a sensor device data structure or NULL in case of error.

The parameter *bus* specifies the ID of the I2C or SPI bus to which the sensor is connected.

```
static lsm303d_sensor_t* sensor;
```

For sensors connected to an I2C interface, a valid I2C slave address has to be defined as parameter *addr*. In that case parameter *cs* is ignored.

```
sensor = lsm303d_init_sensor (I2C_BUS, LSM303D_I2C_ADDRESS_2, 0);

```

If parameter *addr* is 0, the sensor is connected to a SPI bus. In that case, parameter *cs* defines the GPIO used as CS signal.

```
sensor = lsm303d_init_sensor (SPI_BUS, 0, SPI_CS_GPIO);

```

The remaining of the program is independent on the communication interface.

#### Configuring the sensor

Optionally, you could wish to set some measurement parameters. For details see the sections above, the header file of the driver ```lsm303d.h```, and of course the data sheet of the sensor.

#### Starting measurements

As last step, the sensor mode has be set to start periodic measurement. The sensor mode can be changed anytime later.

```
...
// start periodic measurement with output data rate of 12.5 Hz
lsm303d_set_a_mode (sensor, lsm303d_a_odr_12_5, lsm303d_a_aaf_bw_773, true, true, true);
lsm303d_set_m_mode (sensor, lsm303d_m_odr_12_5, lsm303d_m_low_res, lsm303d_m_continuous);
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
    lsm303d_float_a_data_t  a_data;
    lsm303d_float_m_data_t  m_data;

    while (1)
    {
        // execute task every 10 ms
        vTaskDelay (10/portTICK_PERIOD_MS);
        ...
        // test for new accelerator data and fetch them
        if (lsm303d_new_a_data (sensor) &&
            lsm303d_get_float_a_data (sensor, &a_data))
        {
            // do something with accelerator data
            ...
        }
        
        // test for new magnetometer data and fetch them
        if (lsm303d_new_m_data (sensor) &&
            lsm303d_get_float_m_data (sensor, &m_data))
        {
            // do something with magnetometer data
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

A different approach is to use one of the **interrupt** signals INT1 or INT2. In this case, the user has to implement an interrupt handler that either fetches the data directly or triggers a task, that is waiting to fetch the data.

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
            // test for new accelerator data and fetch them
            if (lsm303d_new_a_data (sensor) &&
                lsm303d_get_float_a_data (sensor, &a_data))
            {
                // do something with accelerator data
                ...
            }
        
            // test for new magnetometer data and fetch them
            if (lsm303d_new_m_data (sensor) &&
                lsm303d_get_float_m_data (sensor, &m_data))
            {
                // do something with magnetometer data
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

Furthermore, the interrupts have to be enabled and configured in the LSM303D sensor, see section **Interrupts** above.

## Full Example

```
/* -- use following constants to define the example mode ----------- */

// #define SPI_USED   // SPI interface is used, otherwise I2C
// #define FIFO_MODE  // multiple sample read mode
// #define TEMP_USED  // temperature sensor used
// #define INT_DATA   // data interrupts used (data ready and FIFO status)
// #define INT_EVENT  // inertial event interrupts used (axis movement or 6D/4D orientation)
// #define INT_CLICK  // click detection interrupts used
// #define INT_THRESH // magnetic value exceeds threshold interrupt used

#if defined(INT_DATA) || defined(INT_EVENT) || defined(INT_CLICK) || defined(INT_THRESH)
#define INT_USED
#endif

/* -- includes ----------------------------------------------------- */

#include "lsm303d.h"

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

static lsm303d_sensor_t* sensor;

/**
 * Common function used to get sensor data.
 */
void read_data ()
{
    #ifdef FIFO_MODE

    lsm303d_float_a_data_fifo_t fifo;

    // test for new accelerator data data
    if (lsm303d_new_a_data (sensor))
    {
        // fetch the accelerator data stored in FIFO
        uint8_t num = lsm303d_get_float_a_data_fifo (sensor, fifo);

        printf("%.3f LSM303D num=%d\n", (double)sdk_system_get_time()*1e-3, num);

        for (int i=0; i < num; i++)
            // max. full scale is +-16 g and best resolution is 1 mg, i.e. 5 digits
            printf("%.3f LSM303D (xyz)[g]  ax=%+7.3f ay=%+7.3f az=%+7.3f\n",
                   (double)sdk_system_get_time()*1e-3, 
                   fifo[i].ax, fifo[i].ay, fifo[i].az);
    }

    #else

    lsm303d_float_a_data_t  a_data;

    // test for new accelerator data and fetch them
    if (lsm303d_new_a_data (sensor) &&
        lsm303d_get_float_a_data (sensor, &a_data))
        // max. full scale is +-16 g and best resolution is 1 mg, i.e. 5 digits
        printf("%.3f LSM303D (xyz)[g]  ax=%+7.3f ay=%+7.3f az=%+7.3f\n",
               (double)sdk_system_get_time()*1e-3, 
                a_data.ax, a_data.ay, a_data.az);
        
    #endif // FIFO_MODE

    lsm303d_float_m_data_t  m_data;

    // test for new magnetometer data and fetch them
    if (lsm303d_new_m_data (sensor) &&
        lsm303d_get_float_m_data (sensor, &m_data))
        // max. full scale is +-12 Gs and best resolution is 1 mGs, i.e. 5 digits
        printf("%.3f LSM303D (xyz)[Gs] mx=%+7.3f my=%+7.3f mz=%+7.3f\n",
               (double)sdk_system_get_time()*1e-3, 
                m_data.mx, m_data.my, m_data.mz);

    #ifdef TEMP_USED
    float temp = lsm303d_get_temperature (sensor);
    
    printf("%.3f LSM303D (tmp)[°C] %+7.3f\n", (double)sdk_system_get_time()*1e-3, temp);
    #endif
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
    uint8_t gpio;

    while (1)
    {
        if (xQueueReceive(gpio_evt_queue, &gpio, portMAX_DELAY))
        {
            lsm303d_int_data_source_t      data_src   = {};
            lsm303d_int_event_source_t     event_src  = {};
            lsm303d_int_click_source_t     click_src  = {};
            lsm303d_int_m_thresh_source_t  thresh_src = {};

            // get the source of the interrupt that reset *INTx* signals
            #ifdef INT_DATA
            lsm303d_get_int_data_source    (sensor, &data_src);
            #endif
            #ifdef INT_THRESH
            lsm303d_get_int_m_thresh_source(sensor, &thresh_src);
            #endif
            #ifdef INT_EVENT
            lsm303d_get_int_event_source   (sensor, &event_src, lsm303d_int_event1_gen);
            #endif
            #ifdef INT_CLICK
            lsm303d_get_int_click_source   (sensor, &click_src);
            #endif

            // in case of DRDY interrupt
            if (data_src.a_data_ready || data_src.m_data_ready)
                read_data ();
                
            // in case of FIFO interrupts read the whole FIFO
            else if (data_src.fifo_thresh || data_src.fifo_overrun)
                read_data ();
    
            // in case of magnetic threshold interrupt
            else if (thresh_src.active)
            {
                printf("%.3f LSM303D ", (double)sdk_system_get_time()*1e-3);
                if (thresh_src.x_pos) printf("x exceeds threshold on positive side\n");
                if (thresh_src.y_pos) printf("y exceeds threshold on positive side\n");
                if (thresh_src.z_pos) printf("z exceeds threshold on positive side\n");
                if (thresh_src.x_neg) printf("x exceeds threshold on negative side\n");
                if (thresh_src.y_neg) printf("y exceeds threshold on negative side\n");
                if (thresh_src.z_neg) printf("z exceeds threshold on negative side\n");
            }
            
            // in case of event interrupt
            else if (event_src.active)
            {
                printf("%.3f LSM303D ", (double)sdk_system_get_time()*1e-3);
                if (event_src.x_low)  printf("x is lower than threshold\n");
                if (event_src.y_low)  printf("y is lower than threshold\n");
                if (event_src.z_low)  printf("z is lower than threshold\n");
                if (event_src.x_high) printf("x is higher than threshold\n");
                if (event_src.y_high) printf("y is higher than threshold\n");
                if (event_src.z_high) printf("z is higher than threshold\n");
            }

            // in case of click detection interrupt   
            else if (click_src.active)
               printf("%.3f LSM303D %s\n", (double)sdk_system_get_time()*1e-3, 
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
        vTaskDelay(200/portTICK_PERIOD_MS);
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

    // init the SPI interface at which LMS303D sensors are connected
    spi_bus_init (SPI_BUS, SPI_SCK_GPIO, SPI_MISO_GPIO, SPI_MOSI_GPIO);

    // init the sensor connected to SPI_BUS with SPI_CS_GPIO as chip select.
    sensor = lsm303d_init_sensor (SPI_BUS, 0, SPI_CS_GPIO);
    
    #else

    // init all I2C busses at which LSM303D sensors are connected
    i2c_init (I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ);
    
    // init the sensor with slave address LSM303D_I2C_ADDRESS_2 connected to I2C_BUS.
    sensor = lsm303d_init_sensor (I2C_BUS, LSM303D_I2C_ADDRESS_2, 0);

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
        gpio_enable(INT2_PIN, GPIO_INPUT);
        gpio_set_interrupt(INT1_PIN, GPIO_INTTYPE_EDGE_POS, int_signal_handler);
        gpio_set_interrupt(INT2_PIN, GPIO_INTTYPE_EDGE_POS, int_signal_handler);

        #endif  // INT_USED
        
        /** -- SENSOR CONFIGURATION PART --- */

        // set the type of INTx signals if necessary
        // lsm303d_config_int_signals (sensor, lsm303d_push_pull);

        #ifdef INT_DATA
        // enable data interrupts on *INT2* (data ready or FIFO overrun and FIFO threshold)
        // data ready and FIFO status interrupts must not be enabled at the same time
        #ifdef FIFO_MODE
        lsm303d_enable_int (sensor, lsm303d_int_fifo_overrun, lsm303d_int2_signal, true);
        lsm303d_enable_int (sensor, lsm303d_int_fifo_thresh , lsm303d_int2_signal, true);
        #else
        lsm303d_enable_int (sensor, lsm303d_int_a_data_ready, lsm303d_int2_signal, true);
        lsm303d_enable_int (sensor, lsm303d_int_m_data_ready, lsm303d_int2_signal, true);
        #endif // FIFO_MODE
        #endif // INT_DATA
        
        #ifdef INT_THRESH
        // enable magnetic threshold interrupts on signal *INT1* 
        lsm303d_int_m_thresh_config_t m_thresh_config;
    
        m_thresh_config.threshold    = 2000;
        m_thresh_config.x_enabled    = true;
        m_thresh_config.y_enabled    = true;
        m_thresh_config.z_enabled    = true;
        m_thresh_config.latch        = true;
        m_thresh_config.signal_level = lsm303d_high_active;
        
        lsm303d_set_int_m_thresh_config (sensor, &m_thresh_config);
        lsm303d_enable_int (sensor, lsm303d_int_m_thresh, lsm303d_int1_signal, true);
        #endif // INT_THRESH

        #ifdef INT_EVENT
        // enable inertial event interrupts on *INT1*
        lsm303d_int_event_config_t event_config;
    
        event_config.mode = lsm303d_or;       // axes movement wake-up
        // event_config.mode = lsm303d_and;   // free fall
        // event_config.mode = lsm303d_6d_movement;
        // event_config.mode = lsm303d_6d_position;
        // event_config.mode = lsm303d_4d_movement;
        // event_config.mode = lsm303d_4d_position;
        event_config.threshold = 50;
        event_config.x_low_enabled  = false;
        event_config.x_high_enabled = true;
        event_config.y_low_enabled  = false;
        event_config.y_high_enabled = true;
        event_config.z_low_enabled  = false;
        event_config.z_high_enabled = true;
        event_config.duration = 0;
        event_config.latch = true;
        
        lsm303d_set_int_event_config (sensor, &event_config, lsm303d_int_event1_gen);
        lsm303d_enable_int (sensor, lsm303d_int_event1, lsm303d_int1_signal, true);
        #endif // INT_EVENT

        #ifdef INT_CLICK
        // enable single click interrupt for z-axis on signal *INT1*
        lsm303d_int_click_config_t click_config;
        
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
        
        lsm303d_set_int_click_config (sensor, &click_config);
        lsm303d_enable_int (sensor, lsm303d_int_click, lsm303d_int1_signal, true);
        #endif // INT_CLICK

        #ifdef FIFO_MODE
        // clear the FIFO
        lsm303d_set_fifo_mode (sensor, lsm303d_bypass, 0);

        // activate the FIFO with a threshold of 10 samples (max. 31); if 
        // interrupt *lsm303d_fifo_thresh* is enabled, an interrupt is
        // generated when the FIFO content exceeds this threshold, i.e., 
        // when 11 samples are stored in FIFO
        lsm303d_set_fifo_mode (sensor, lsm303d_stream, 10);
        #endif

        // configure HPF and implicitly reset the reference by a dummy read
        lsm303d_config_a_hpf (sensor, lsm303d_hpf_normal, true, true, true, true);
        
        #ifdef TEMP_USED
        // enable the temperature sensor
        lsm303d_enable_temperature (sensor, true);
        #endif
                
        // LAST STEP: Finally set scale and mode to start measurements
        lsm303d_set_a_scale(sensor, lsm303d_a_scale_2_g);
        lsm303d_set_m_scale(sensor, lsm303d_m_scale_4_Gs);
        lsm303d_set_a_mode (sensor, lsm303d_a_odr_12_5, lsm303d_a_aaf_bw_773, true, true, true);
        lsm303d_set_m_mode (sensor, lsm303d_m_odr_12_5, lsm303d_m_low_res, lsm303d_m_continuous);

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
        printf("Could not initialize LSM303D sensor\n");
}

```
