# Component makefile for extras/dsm

INC_DIRS += $(ROOT)extras/dsm

# args for passing into compile rule generation
extras/dsm_INC_DIR =  $(ROOT)extras/dsm
extras/dsm_SRC_DIR =  $(ROOT)extras/dsm

$(eval $(call component_compile_rules,extras/dsm))
