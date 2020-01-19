# Component makefile for ping

INC_DIRS += $(ping_ROOT)

ping_INC_DIR = $(ping_ROOT)
ping_SRC_DIR = $(ping_ROOT)

$(eval $(call component_compile_rules,ping))
