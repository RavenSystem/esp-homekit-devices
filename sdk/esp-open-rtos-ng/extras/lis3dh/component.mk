# Component makefile for extras/lis3dh

# expected anyone using SHT3x driver includes it as 'lis3dh/lis3dh.h'
INC_DIRS += $(lis3dh_ROOT)..
INC_DIRS += $(lis3dh_ROOT)

# args for passing into compile rule generation
lis3dh_SRC_DIR =  $(lis3dh_ROOT)

$(eval $(call component_compile_rules,lis3dh))
