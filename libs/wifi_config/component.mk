# Component makefile for wifi_config

INC_DIRS += $(wifi_config_ROOT)/include

wifi_config_INC_DIR = $(wifi_config_ROOT)/include $(wifi_config_ROOT)/src $(wifi_config_OBJ_DIR)/content
wifi_config_SRC_DIR = $(wifi_config_ROOT)/src

$(eval $(call component_compile_rules,wifi_config))

$(wifi_config_OBJ_DIR)/src/wifi_config.o: $(wifi_config_OBJ_DIR)/content/index.html.h

$(wifi_config_OBJ_DIR)%.html.h: $(wifi_config_ROOT)%.html
	$(vecho "Embed %<")
	$(Q) mkdir -p $(@D)
	$(Q) $(wifi_config_ROOT)/tools/embed.py $< > $@

