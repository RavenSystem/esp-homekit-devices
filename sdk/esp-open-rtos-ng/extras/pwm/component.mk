# Component makefile for extras/pwm

INC_DIRS += $(ROOT)extras/pwm

# args for passing into compile rule generation
extras/pwm_INC_DIR =  $(ROOT)extras/pwm
extras/pwm_SRC_DIR =  $(ROOT)extras/pwm

$(eval $(call component_compile_rules,extras/pwm))
