# CCS811 Driver Examples

These examples demonstrate the usage of the CCS811 driver with only one sensors.

## Hardware setup

Most examples use only one CCS811 sensor. Following figure shows the hardware configuration if no interrupt is used.

```
  +------------------------+    +--------+
  | ESP8266  Bus 0         |    | CCS811 |
  |          GPIO 5 (SCL)  -----> SCL    |
  |          GPIO 4 (SDA)  <----> SDA    |
  |          GND           -----> /WAKE  |
  +------------------------+    +--------+
```

If *nINT* interrupt is used to fetch new data, additionally the interrupt pin has to be connected to a GPIO pin.

```
  +------------------------+    +--------+
  | ESP8266  Bus 0         |    | CCS811 |
  |          GPIO 5 (SCL)  -----> SCL    |
  |          GPIO 4 (SDA)  <----> SDA    |
  |          GPIO 2        <----- /nINT  |
  |          GND           -----> /WAKE  |
  +------------------------+    +--------+
```

In examples where CCS811 sensor is used in conjunction with a SHT3x sensor, the hardware configuration looks like following:

```
  +------------------------+       +--------+
  | ESP8266  Bus 0         |       | CCS811 |
  |          GPIO 5 (SCL)  ---+----> SCL    |
  |          GPIO 4 (SDA)  <--|-+--> SDA    |
  |          GND           ---|-|--> /WAKE  |
  |                        |  | |  +--------+
  |                        |  | |  | SHT3x  |
  |                        |  +----> SCL    |
  |                        |    +--> SDA    |
  +------------------------+       +--------+
```

## Example description

__*ccs811_one_sensor*__

Simple example with one CCS811 sensor connected to I2C bus 0. It demonstrates the different approaches to fetch the data. Either the interrupt *nINT* is used when new data are available or exceed defined thresholds or the new data are fetched periodically. Which approach is used is defined by the constants ```INT_DATA_RDY_USED``` and ```INT_THRESHOLD_USED```.

__*ccs811_plus_sht3x*__

Simple example with one CCS811 sensor connected to I2C bus 0 and one SHT3x sensor to determine ambient temperature. New data are fetched peridically every 2 seconds.

__*ccs811_temperature*__

Simple example with one CCS811 sensor connected to I2C bus 0. It demonstrates how to use CCS811 with an external NTC resistor to determine ambient temperature.
