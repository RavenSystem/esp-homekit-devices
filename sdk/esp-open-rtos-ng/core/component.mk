INC_DIRS += $(core_ROOT)include

# args for passing into compile rule generation
core_SRC_DIR = $(core_ROOT)

$(eval $(call component_compile_rules,core))
