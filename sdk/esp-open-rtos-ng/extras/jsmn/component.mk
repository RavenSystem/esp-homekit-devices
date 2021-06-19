# Component makefile for extras/jsmn

# expected anyone using jsmn json component includes it as 'jsmn/jsmn.h'
INC_DIRS += $(jsmn_ROOT)jsmn

# args for passing into compile rule generation
jsmn_INC_DIR =
jsmn_SRC_DIR = $(jsmn_ROOT)jsmn

$(eval $(call component_compile_rules,jsmn))
