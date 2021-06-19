# Component makefile for extras/tsl2561

# Include the TSL2561 driver as "tsl2561/tsl2561.h"
INC_DIRS += $(tsl2561_ROOT)..

# args for passing into compile rule generation
tsl2561_SRC_DIR = $(tsl2561_ROOT)

$(eval $(call component_compile_rules,tsl2561))
