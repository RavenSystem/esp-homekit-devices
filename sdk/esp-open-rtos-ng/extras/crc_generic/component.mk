# Component makefile for extras/crc_generic

# expected anyone using bmp driver includes it as 'crc_generic/crc_generic.h'
INC_DIRS += $(crc_generic_ROOT)crc_lib/

# args for passing into compile rule generation
crc_generic_SRC_DIR =  $(crc_generic_ROOT)crc_lib/

$(eval $(call component_compile_rules,crc_generic))
