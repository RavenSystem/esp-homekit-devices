# Component makefile for adv_hlw

INC_DIRS += $(adv_hlw_ROOT)

adv_hlw_INC_DIR = $(adv_hlw_ROOT)
adv_hlw_SRC_DIR = $(adv_hlw_ROOT)

$(eval $(call component_compile_rules,adv_hlw))

