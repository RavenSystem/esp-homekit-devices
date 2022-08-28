# Component makefile for extras/ds1307

# expected anyone using RTC driver includes it as 'ds1307/ds1307.h'
INC_DIRS += $(ds1307_ROOT)..

# args for passing into compile rule generation
ds1307_SRC_DIR =  $(ds1307_ROOT)

$(eval $(call component_compile_rules,ds1307))
