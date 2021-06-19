# Component makefile for extras/wificfg

# Expected anyone using wificfg includes it as 'wificfg/wificfg.h'
INC_DIRS += $(wificfg_ROOT)..

# args for passing into compile rule generation
wificfg_INC_DIR =
wificfg_SRC_DIR = $(wificfg_ROOT)

$(eval $(call component_compile_rules,wificfg))
