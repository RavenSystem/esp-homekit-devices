# Component makefile for adv_logger

INC_DIRS += $(adv_logger_ROOT)

adv_logger_INC_DIR = $(adv_logger_ROOT)
adv_logger_SRC_DIR = $(adv_logger_ROOT)

$(eval $(call component_compile_rules,adv_logger))
