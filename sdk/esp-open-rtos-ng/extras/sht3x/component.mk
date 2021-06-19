# Component makefile for extras/sht3x

# expected anyone using SHT3x driver includes it as 'sht3x/sht3x.h'
INC_DIRS += $(sht3x_ROOT)..
INC_DIRS += $(sht3x_ROOT)

# args for passing into compile rule generation
sht3x_SRC_DIR =  $(sht3x_ROOT)

$(eval $(call component_compile_rules,sht3x))
