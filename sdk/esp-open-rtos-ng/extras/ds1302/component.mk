# Component makefile for extras/ds1302

# expected anyone using RTC driver includes it as 'ds1302/ds1302.h'
INC_DIRS += $(ds1302_ROOT)..

# args for passing into compile rule generation
ds1302_SRC_DIR = $(ds1302_ROOT)

$(eval $(call component_compile_rules,ds1302))
