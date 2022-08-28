# Component makefile for extras/bmp280

# expected anyone using bmp driver includes it as 'bmp280/bmp280.h'
INC_DIRS += $(bmp280_ROOT)..

# args for passing into compile rule generation
bmp280_SRC_DIR =  $(bmp280_ROOT)

$(eval $(call component_compile_rules,bmp280))
