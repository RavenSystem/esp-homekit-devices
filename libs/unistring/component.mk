# Component makefile for unistring

INC_DIRS += $(unistring_ROOT)

unistring_INC_DIR = $(unistring_ROOT)
unistring_SRC_DIR = $(unistring_ROOT)

$(eval $(call component_compile_rules,unistring))

