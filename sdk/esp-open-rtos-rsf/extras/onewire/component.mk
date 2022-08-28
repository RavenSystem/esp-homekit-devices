# Component makefile for extras/onewire

# expected anyone using onewire driver includes it as 'onewire/onewire.h'
INC_DIRS += $(onewire_ROOT)..

# args for passing into compile rule generation
onewire_INC_DIR =
onewire_SRC_DIR = $(onewire_ROOT)

$(eval $(call component_compile_rules,onewire))
