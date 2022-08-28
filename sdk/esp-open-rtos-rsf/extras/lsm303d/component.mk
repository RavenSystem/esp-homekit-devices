# Component makefile for extras/lsm303d

# expected anyone using LIS3MDL driver includes it as 'lis3mld/lis3mld.h'
INC_DIRS += $(lsm303d_ROOT)..
INC_DIRS += $(lsm303d_ROOT)

# args for passing into compile rule generation
lsm303d_SRC_DIR =  $(lsm303d_ROOT)

$(eval $(call component_compile_rules,lsm303d))
