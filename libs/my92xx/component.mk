# Component makefile for adv_pwm

INC_DIRS += $(my92xx_ROOT)

my92xx_INC_DIR = $(my92xx_ROOT)
my92xx_SRC_DIR = $(my92xx_ROOT)

$(eval $(call component_compile_rules,my92xx))

