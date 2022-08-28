# Component makefile for extras/mdnsresponder

INC_DIRS += $(mdnsresponder_ROOT)

# args for passing into compile rule generation
mdnsresponder_INC_DIR = $(mdnsresponder_ROOT)
mdnsresponder_SRC_DIR = $(mdnsresponder_ROOT)

$(eval $(call component_compile_rules,mdnsresponder))
