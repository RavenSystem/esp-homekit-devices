# Yet another I2C driver for the ESP8266

This time a driver for the excellent esp-open-rtos. This is a bit banging I2C driver based on the Wikipedia pesudo C code [1].

### Basic usage

```C
#include <i2c.h>

#define BUS     (0)
#define SCL_PIN (0)
#define SDA_PIN (2)

uint8_t slave_addr = 0x20;
uint8_t reg_addr = 0x1f;
uint8_t reg_data;

i2c_init(BUS, SCL_PIN, SDA_PIN, I2C_FREQ_400K);

// Write 1 byte to slave register
int err = i2c_slave_write(BUS, slave_addr, &reg_addr, &data, 1);
if (err != 0)
{
	// do something with error
}

// Issue write to slave, sending reg_addr, followed by reading 1 byte
err = i2c_slave_read(BUS, slave_addr, &reg_addr, &reg_data, 1);

```

For details please see `extras/i2c/i2c.h`.

The driver is released under the MIT license.

[1] https://en.wikipedia.org/wiki/I²C#Example_of_bit-banging_the_I.C2.B2C_Master_protocol