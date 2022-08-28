# Component makefile for extras/pcf8574

INC_DIRS += $(pcf8574_ROOT)..
pcf8574_SRC_DIR =  $(pcf8574_ROOT)

$(eval $(call component_compile_rules,pcf8574))
