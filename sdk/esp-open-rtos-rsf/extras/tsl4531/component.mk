# Component makefile for extras/tsl4531

# Include the TSL4531 driver as "tsl4531/tsl4531.h"
INC_DIRS += $(tsl4531_ROOT)..

# args for passing into compile rule generation
tsl4531_SRC_DIR = $(tsl4531_ROOT)

$(eval $(call component_compile_rules,tsl4531))
