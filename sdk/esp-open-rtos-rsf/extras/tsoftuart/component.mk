# Component makefile for extras/tsoftuart

# Expected anyone using tsoftuart includes it as 'tsoftuart/tsoftuart.h'
INC_DIRS += $(tsoftuart_ROOT)..

# args for passing into compile rule generation
tsoftuart_INC_DIR =
tsoftuart_SRC_DIR = $(tsoftuart_ROOT)

$(eval $(call component_compile_rules,tsoftuart))
