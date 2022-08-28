# Driver for BMP280 and BME280 absolute barometric pressure sensors

The driver works only with BMP280 and BME280 sensors. For BMP080/BMP180 there's
a separate driver. Even though BMP280 is a successor of BMP180 they are not
compatible.  They have different registers and different operation modes.
BMP280 supports two ways of communication: spi and i2c.  This driver provides
only i2c communication.
The driver is written for [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos)
framework and requires [i2c driver](https://github.com/SuperHouse/esp-open-rtos/tree/master/extras/i2c)
from it.

## Features

 * I2C communication.
 * Forced mode (Similar to BMP180 operation).
 * Normal mode. Continuous measurement.
 * Soft reset.

## Usage

Connect BMP280 or BME280 module to you ESP8266 module and initialize the I2C SCL and SDA pins:

```
const uint8_t bus = 0;
const uint8_t scl_pin = 0;
const uint8_t sda_pin = 2;
i2c_init(bus, scl_pin, sda_pin, I2C_FREQ_100K);

```

Pull up SDO pin of BMP280 in order to have address 0x77 `BMP280_I2C_ADDRESS_1`.
Or pull down SDO pin for address 0x76 `BMP280_I2C_ADDRESS_0`. Otherwise your
sensor will not work.

The BMP280 or BME280 are auto-detected at initialization based on the chip ID
and this ID is stored in the device descriptor.

BMP280 supports two operation modes.

### Forced mode

In forced mode, a single measurement is performed according to selected
configuration. When the measurement is finished, the sensor returns to
sleep mode and the measurement results can be read.

### Normal mode

Normal mode continuously cycles between measurement period and standby period,
whose time is defined by standby_time.

## Example

### Forced mode

```
bmp280_params_t  params;
float pressure, temperature, humidity;

bmp280_init_default_params(&params);
params.mode = BMP280_MODE_FORCED;

bmp280_t bmp280_dev;
bmp280_dev.i2c_addr = BMP280_I2C_ADDRESS_0;
bmp280_init(&bmp280_dev, &params);
bool bme280p = bmp280_dev.id == BME280_CHIP_ID;

while(1) {
  bmp280_force_measurement(&bmp280_dev));
  // wait for measurement to complete
  while (bmp280_is_measuring(&bmp280_dev)) {};

  bmp280_read_float(&bmp280_dev, &temperature, &pressure, &humidity);
  printf("Pressure: %.2f Pa, Temperature: %.2f C", pressure, temperature);
  if (bme280p)
    printf(", Humidity: %.2f\n", humidity);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
```

### Normal mode

```
bmp280_params_t  params;
float pressure, temperature, humidity;

bmp280_init_default_params(&params);

bmp280_t bmp280_dev;
bmp280_dev.i2c_addr = BMP280_I2C_ADDRESS_0;
bmp280_init(&bmp280_dev, &params);
bool bme280p = bmp280_dev.id == BME280_CHIP_ID;

while(1) {
  bmp280_read_float(&bmp280_dev, &temperature, &pressure, &humidity);
  printf("Pressure: %.2f Pa, Temperature: %.2f C", pressure, temperature);
  if (bme280p)
    printf(", Humidity: %.2f\n", humidity);
  else
    printf("\n");
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
```

## License

The driver is released under MIT license.

Copyright (c) 2016 sheinz (https://github.com/sheinz)
