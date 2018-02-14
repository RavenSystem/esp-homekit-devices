
INC_DIRS += $(cJSON_ROOT)/cJSON

cJSON_INC_DIR = $(cJSON_ROOT)/cJSON
cJSON_SRC_DIR = $(cJSON_ROOT)/cJSON

$(eval $(call component_compile_rules,cJSON))

