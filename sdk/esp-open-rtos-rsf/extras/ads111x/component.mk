# Component makefile for extras/ads111x

# expected anyone using ADC driver includes it as 'ads111x/ads111x.h'
INC_DIRS += $(ads111x_ROOT)..

# args for passing into compile rule generation
ads111x_SRC_DIR = $(ads111x_ROOT)

$(eval $(call component_compile_rules,ads111x))
