# Component makefile for extras/timekeeping

INC_DIRS += $(timekeeping_ROOT)

# args for passing into compile rule generation
timekeeping_SRC_DIR =  $(timekeeping_ROOT)

$(eval $(call component_compile_rules,timekeeping))
