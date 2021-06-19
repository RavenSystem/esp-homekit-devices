INC_DIRS += $(hd44780_ROOT)..
hd44780_SRC_DIR = $(hd44780_ROOT)

HD44780_I2C ?= 1

hd44780_CFLAGS = -DHD44780_I2C=${HD44780_I2C} $(CFLAGS)

$(eval $(call component_compile_rules,hd44780))
