# Component makefile for extras/pca9685

# expected anyone using this driver includes it as 'pca9685/pca9685.h'
INC_DIRS += $(pca9685_ROOT)..

# args for passing into compile rule generation
pca9685_SRC_DIR = $(pca9685_ROOT)

$(eval $(call component_compile_rules,pca9685))
