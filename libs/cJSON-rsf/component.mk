# Component makefile for cJSON-rsf

INC_DIRS += $(cJSON-rsf_ROOT)

cJSON-rsf_INC_DIR = $(cJSON-rsf_ROOT)
cJSON-rsf_SRC_DIR = $(cJSON-rsf_ROOT)

$(eval $(call component_compile_rules,cJSON-rsf))
