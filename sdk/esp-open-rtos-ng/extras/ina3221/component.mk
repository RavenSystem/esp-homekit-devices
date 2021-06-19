# Component makefile for extras/ina3221

# expected anyone using this driver includes it as 'ina3221/ina3221.h'
INC_DIRS += $(ina3221_ROOT)..

# args for passing into compile rule generation
ina3221_SRC_DIR = $(ina3221_ROOT)

$(eval $(call component_compile_rules,ina3221))
