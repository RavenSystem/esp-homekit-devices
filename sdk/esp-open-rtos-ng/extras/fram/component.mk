# Component makefile for extras/fram

# expected anyone using ADC driver includes it as 'fram/fram.h'
INC_DIRS += $(fram_ROOT)..

# args for passing into compile rule generation
fram_SRC_DIR = $(fram_ROOT)

$(eval $(call component_compile_rules,fram))
