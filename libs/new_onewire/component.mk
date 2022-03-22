# Component makefile for new_onewire

# expected anyone using onewire driver includes it as 'onewire/onewire.h'
INC_DIRS += $(new_onewire_ROOT)..

# args for passing into compile rule generation
new_onewire_INC_DIR =
new_onewire_SRC_DIR = $(new_onewire_ROOT)

$(eval $(call component_compile_rules,new_onewire))
