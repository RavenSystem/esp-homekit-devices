# Component makefile for raven_ntp

INC_DIRS += $(raven_ntp_ROOT)

raven_ntp_INC_DIR = $(raven_ntp_ROOT)
raven_ntp_SRC_DIR = $(raven_ntp_ROOT)

$(eval $(call component_compile_rules,raven_ntp))

