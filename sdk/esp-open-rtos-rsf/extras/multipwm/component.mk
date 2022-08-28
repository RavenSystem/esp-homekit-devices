# Component makefile for extras/multipwm

INC_DIRS += $(ROOT)extras/multipwm

# args for passing into compile rule generation
extras/multipwm_INC_DIR =  $(ROOT)extras/multipwm
extras/multipwm_SRC_DIR =  $(ROOT)extras/multipwm

$(eval $(call component_compile_rules,extras/multipwm))

