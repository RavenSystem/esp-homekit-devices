# Component makefile for adv_i2c

INC_DIRS += $(adv_i2c_ROOT)

adv_i2c_INC_DIR = $(adv_i2c_ROOT)
adv_i2c_SRC_DIR = $(adv_i2c_ROOT)

$(eval $(call component_compile_rules,adv_i2c))
