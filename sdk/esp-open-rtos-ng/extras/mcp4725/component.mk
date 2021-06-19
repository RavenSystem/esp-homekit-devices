# Component makefile for extras/mcp4725

# expected anyone using this driver includes it as 'mcp4725/mcp4725.h'
INC_DIRS += $(mcp4725_ROOT)..

# args for passing into compile rule generation
mcp4725_SRC_DIR = $(mcp4725_ROOT)

$(eval $(call component_compile_rules,mcp4725))
