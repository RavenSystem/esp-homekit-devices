# Component makefile for extras/pcf8591

INC_DIRS += $(pcf8591_ROOT)..
pcf8591_SRC_DIR =  $(pcf8591_ROOT)

$(eval $(call component_compile_rules,pcf8591))
