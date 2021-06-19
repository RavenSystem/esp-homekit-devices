# Component makefile for extras/softuart

# expected anyone using this driver includes it as 'softuart/softuart.h'
INC_DIRS += $(softuart_ROOT)..

# args for passing into compile rule generation
softuart_SRC_DIR = $(softuart_ROOT)

$(eval $(call component_compile_rules,softuart))

