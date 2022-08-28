# Component makefile for extras/ws2812_i2s

# expected anyone using ws2812_i2s driver includes it as 'ws2812_i2s/ws2812_i2s.h'
INC_DIRS += $(ws2812_i2s_ROOT)..

# args for passing into compile rule generation
ws2812_i2s_SRC_DIR =  $(ws2812_i2s_ROOT)

$(eval $(call component_compile_rules,ws2812_i2s))
