# Component makefile for extras/ssd1306

# expected anyone using ssd1306 driver includes it as 'ssd1306/ssd1306.h'
INC_DIRS += $(ssd1306_ROOT)..

# I2C support is on by default
SSD1306_I2C_SUPPORT ?= 1
# SPI4 support is on by default
SSD1306_SPI4_SUPPORT ?= 1
# SPI3 support is on by default
SSD1306_SPI3_SUPPORT ?= 1

# args for passing into compile rule generation
ssd1306_SRC_DIR = $(ssd1306_ROOT)

ssd1306_CFLAGS = -DSSD1306_I2C_SUPPORT=${SSD1306_I2C_SUPPORT} -DSSD1306_SPI4_SUPPORT=${SSD1306_SPI4_SUPPORT} -DSSD1306_SPI3_SUPPORT=${SSD1306_SPI3_SUPPORT} $(CFLAGS)


$(eval $(call component_compile_rules,ssd1306))
