# Component makefile for extras/bh1750

# expected anyone using RTC driver includes it as 'bh1750/bh1750.h'
INC_DIRS += $(bh1750_ROOT)..

# args for passing into compile rule generation
bh1750_SRC_DIR =  $(bh1750_ROOT)

$(eval $(call component_compile_rules,bh1750))
