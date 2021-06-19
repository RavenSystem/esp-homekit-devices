# Component makefile for extras/ultrasonic

# expected anyone using this driver includes it as 'ultrasonic/ultrasonic.h'
INC_DIRS += $(ultrasonic_ROOT)..

# args for passing into compile rule generation
ultrasonic_SRC_DIR = $(ultrasonic_ROOT)

$(eval $(call component_compile_rules,ultrasonic))
