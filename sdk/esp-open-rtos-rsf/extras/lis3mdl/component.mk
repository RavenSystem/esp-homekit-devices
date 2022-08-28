# Component makefile for extras/lis3mdl

# expected anyone using LIS3MDL driver includes it as 'lis3mld/lis3mld.h'
INC_DIRS += $(lis3mdl_ROOT)..
INC_DIRS += $(lis3mdl_ROOT)

# args for passing into compile rule generation
lis3mdl_SRC_DIR =  $(lis3mdl_ROOT)

$(eval $(call component_compile_rules,lis3mdl))
