# Component makefile for rs_ping

INC_DIRS += $(rs_ping_ROOT)

rs_ping_INC_DIR = $(rs_ping_ROOT)
rs_ping_SRC_DIR = $(rs_ping_ROOT)

$(eval $(call component_compile_rules,rs_ping))
