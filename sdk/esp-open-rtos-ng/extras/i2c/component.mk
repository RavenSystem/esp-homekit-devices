# Component makefile for extras/i2c

# expected anyone using i2c driver includes it as 'i2c/i2c.h'
INC_DIRS += $(i2c_ROOT)..

# args for passing into compile rule generation
i2c_INC_DIR =
i2c_SRC_DIR = $(i2c_ROOT)

$(eval $(call component_compile_rules,i2c))
