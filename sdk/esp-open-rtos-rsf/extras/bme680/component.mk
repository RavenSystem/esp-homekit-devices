# Component makefile for extras/bme60

# expected anyone using bme680 driver includes it as 'bme680/bme680.h'
INC_DIRS += $(bme680_ROOT).. 
INC_DIRS += $(bme680_ROOT) 

# args for passing into compile rule generation
bme680_SRC_DIR =  $(bme680_ROOT)

$(eval $(call component_compile_rules,bme680))
