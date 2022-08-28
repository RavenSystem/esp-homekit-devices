# Component makefile for extras/http-parser

# Include it as 'http-parser/http_parser.h'
INC_DIRS += $(http-parser_ROOT)

# args for passing into compile rule generation
http-parser_INC_DIR =
http-parser_SRC_DIR = $(http-parser_ROOT)http-parser
http-parser_SRC_FILES = $(http-parser_SRC_DIR)/http_parser.c

$(eval $(call component_compile_rules,http-parser))
