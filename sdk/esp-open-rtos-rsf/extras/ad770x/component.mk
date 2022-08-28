# Component makefile for extras/ad770x (AD7705/AD7706 driver)

# expected anyone using ADC driver includes it as 'ad770x/ad770x.h'
INC_DIRS += $(ad770x_ROOT)..

# args for passing into compile rule generation
ad770x_SRC_DIR = $(ad770x_ROOT)

$(eval $(call component_compile_rules,ad770x))
