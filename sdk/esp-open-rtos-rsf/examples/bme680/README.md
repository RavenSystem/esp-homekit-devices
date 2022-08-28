# BME680 Driver Examples

These examples demonstrate the usage of the BME680 driver with only one and multiple BME680 sensors.

## Hardware setup

There are examples that are using either I2C or SPI with one or two sensors.

For examples using BME680 sensor as I2C slave, just use GPIO5 (SCL) and GPIO4 (SDA) to connect to the BME680 sensor's I2C interface.

```
 +-------------------------+     +--------+
 | ESP8266  Bus 0          |     | BME680 |
 |          GPIO 14 (SCL)  ------> SCL    |
 |          GPIO 13 (SDA)  <-----> SDA    |
 |                         |     +--------+
 +-------------------------+
```

For examples that are using SPI, BME680 sensor has to be connected to SPI bus 1.  Since GPIO15 used as default CS signal of SPI bus 1 does not work correctly together with BME680, you have to connect CS to another GPIO pin, e.g., GPIO2.

```
 +-------------------------+     +----------+
 | ESP8266  Bus 1          |     | BME680   |
 |          GPIO 14 (SCK)  ------> SCK      |
 |          GPIO 13 (MOSI) ------> SDI      |
 |          GPIO 12 (MISO) <------ SDO      |
 |          GPIO 2  (CS)   ------> CS       |
 +-------------------------+     +----------+
```

The example with two sensors use the combination of I2C and SPI.

## Example description

__*bme680_one_sensor*__

This simple example uses only **one sensor** connected either to **I2C** or to **SPI**. Which of these interfaces is used is defined by constant **SPI_USED**. The user task triggers a measurement every second and uses function ```vTaskDelay``` to wait for the measurement results.

__*bme680_two_sensors*__

This example uses **two sensors**. One sensor is connected to **I2C** bus 0 and one sensor is connected to **SPI**. It defines two different user tasks, one for each sensor. It demonstrate the possible approaches to wait for measurement results, active busy waiting using ```bme680_is_measuring``` and passive waiting using *vTaskDelay*.

__*bme680_heating_profiles*__

This simple example uses one **only sensor** connected to **I2C** bus 0 and a sequence of heating profiles. The heating profile is changed with each cycle.
