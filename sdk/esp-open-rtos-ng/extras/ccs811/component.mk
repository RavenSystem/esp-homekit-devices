# Component makefile for extras/ccs811

# expected anyone using ccs811 driver includes it as 'ccs811/ccs811.h'
INC_DIRS += $(ccs811_ROOT).. 
INC_DIRS += $(ccs811_ROOT)

# args for passing into compile rule generation
ccs811_SRC_DIR =  $(ccs811_ROOT)

$(eval $(call component_compile_rules,ccs811))
