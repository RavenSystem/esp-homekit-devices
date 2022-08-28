# Driver for SSD1306/SH1106 OLED LCD

This driver is written for usage with the ESP8266 and FreeRTOS ([esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos)).

## Supported display sizes

 - 128x64
 - 128x32
 - 128x16
 - 96x16

## Supported connection interfaces

I2C, SPI3 and SPI4.

## Usage

If Reset pin is accesible in your display module, connect it to the RESET pin of ESP8266.
If you don't, display RAM could be glitchy after the power cycle.

### I2C protocol

Before using the OLED module you need to call the function `i2c_init(BUS, SCL_PIN, SDA_PIN, I2C_FREQ_400K)`
to configure the I2C interface and then you should call `ssd1306_init()`.

#### Example 

```C
#define SCL_PIN 5
#define SDA_PIN 4
#define I2C_BUS 0
...

static const ssd1306_t device = {
	.protocol     = SSD1306_PROTO_I2C,
	.screen       = SSD1306_SCREEN, // or SH1106_SCREEN
	.i2c_dev.bus  = I2C_BUS,
	.i2c_dev.addr = SSD1306_I2C_ADDR_0,
	.width        = 128,
	.height       = 64
};

...

i2c_init(I2C_BUS, SCL_PIN, SDA_PIN, I2C_FREQ_400K);

if (ssd1306_init(&device)) {
// An error occured, while performing SSD1306 init (E.g device not found etc.)
}

// rest of the code
```

### SPI3 and SPI4 protocols

These protocols are MUCH faster than I2C, but use 2 additional GPIO pins
(besides the **HSPI CLK** and **HSPI MOSI**): **Chip Select** and **Data/Command** (in case of SPI4).

No additional function calls are required before `ssd1306_init()`.


#### SPI4 Example 

```C
#define CS_PIN 5
#define DC_PIN 4

...

static const ssd1306_t device = {
	.protocol = SSD1306_PROTO_SPI4,
	.screen   = SSD1306_SCREEN,
	.cs_pin   = CS_PIN,
	.dc_pin   = DC_PIN,
	.width    = 128,
	.height   = 64
};

...

if (ssd1306_init(&device)) {
// An error occured, while performing SSD1306 init
}

// rest of the code
```

#### SPI3 example
```C

#define CS_PIN 5

...

static const ssd1306_t device = {
	.protocol = SSD1306_PROTO_SPI3,
	.screen   = SSD1306_SCREEN,
	.cs_pin   = CS_PIN,
	.width    = 128,
	.height   = 64
};

...

if (ssd1306_init(&device)) {
// An error occured, while performing SSD1306 init
}

// rest of the code
```
