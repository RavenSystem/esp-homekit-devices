# Component makefile for extras/mactimer

# Expected anyone using mactimer includes it as 'mactimer/mactimer.h'
INC_DIRS += $(mactimer_ROOT)..

# args for passing into compile rule generation
mactimer_INC_DIR =
mactimer_SRC_DIR = $(mactimer_ROOT)

$(eval $(call component_compile_rules,mactimer))
