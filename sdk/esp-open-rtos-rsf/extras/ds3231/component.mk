# Component makefile for extras/ds3231

# expected anyone using bmp driver includes it as 'ds3231/ds3231.h'
INC_DIRS += $(ds3231_ROOT)..

# args for passing into compile rule generation
ds3231_SRC_DIR =  $(ds3231_ROOT)

$(eval $(call component_compile_rules,ds3231))
