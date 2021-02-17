# Component makefile for adv_logger_ntp

INC_DIRS += $(adv_logger_ntp_ROOT)

adv_logger_ntp_INC_DIR = $(adv_logger_ntp_ROOT)
adv_logger_ntp_SRC_DIR = $(adv_logger_ntp_ROOT)

$(eval $(call component_compile_rules,adv_logger_ntp))
