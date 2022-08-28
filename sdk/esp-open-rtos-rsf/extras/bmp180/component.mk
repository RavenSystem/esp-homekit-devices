# Component makefile for extras/bmp180

# expected anyone using bmp driver includes it as 'bmp180/bmp180.h'
INC_DIRS += $(bmp180_ROOT)..

# args for passing into compile rule generation
bmp180_SRC_DIR =  $(bmp180_ROOT)

$(eval $(call component_compile_rules,bmp180))
