# SHT3x Driver Examples

These examples demonstrate the usage of the SHT3x driver with only one and multiple SHT3x sensors.

## Hardware setup

There are examples for only one sensor and examples for two sensors. 

To run examples with **one sensor**, just use GPIO14 (SCL) and GPIO13 (SDA) to connect to the SHT3x sensor's I2C interface. 

```
 +-----------------+     +----------+
 | ESP8266 / ESP32 |     | SHT3x    |
 |                 |     |          |
 |   GPIO 14 (SCL) ------> SCL      |
 |   GPIO 13 (SDA) <-----> SDA      |
 +-----------------+     +----------+
```

If you want to run examples with **two sensors**, you could do this with only one bus and different I2C addresses or with two buses and the same or different I2C addresses. In later case, use GPIO5 (SCL) and GPIO4 (SDA) for the second bus to connect to the second SHT3x sensor's I2C interface.

```
 +-----------------+     +----------+
 | ESP8266 / ESP32 |     | SHT3x_1  |
 |                 |     |          |
 |   GPIO 14 (SCL) ------> SCL      |
 |   GPIO 13 (SDA) <-----> SDA      |
 |                 |     +----------+
 |                 |     | SHT3x_2  |
 |                 |     |          |
 |   GPIO 5  (SCL) ------> SCL      |
 |   GPIO 4  (SDA) <-----> SDA      |
 +-----------------+     +----------+
```

## Example description

It shows different user task implementations in *single shot mode* and *periodic mode*. In *single shot* mode either low level or high level functions are used. Constants SINGLE_SHOT_LOW_LEVEL and SINGLE_SHOT_HIGH_LEVEL controls which task implementation is used.
