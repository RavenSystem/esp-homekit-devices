# Component makefile for led_codes

INC_DIRS += $(led_codes_ROOT)

led_codes_INC_DIR = $(led_codes_ROOT)
led_codes_SRC_DIR = $(led_codes_ROOT)

$(eval $(call component_compile_rules,led_codes))

