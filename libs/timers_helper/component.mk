# Component makefile for timers_helper

INC_DIRS += $(timers_helper_ROOT)

timers_helper_INC_DIR = $(timers_helper_ROOT)
timers_helper_SRC_DIR = $(timers_helper_ROOT)

$(eval $(call component_compile_rules,timers_helper))
