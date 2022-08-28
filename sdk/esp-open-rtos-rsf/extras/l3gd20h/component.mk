# Component makefile for extras/l3gd20h

# expected anyone using L3GD20H driver includes it as 'l3gd20h/l3gd20h.h'
INC_DIRS += $(l3gd20h_ROOT)..
INC_DIRS += $(l3gd20h_ROOT)

# args for passing into compile rule generation
l3gd20h_SRC_DIR =  $(l3gd20h_ROOT)

$(eval $(call component_compile_rules,l3gd20h))
