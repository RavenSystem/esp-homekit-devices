# Component makefile for adv_pwm

INC_DIRS += $(adv_pwm_ROOT)

adv_pwm_INC_DIR = $(adv_pwm_ROOT)
adv_pwm_SRC_DIR = $(adv_pwm_ROOT)

$(eval $(call component_compile_rules,adv_pwm))

