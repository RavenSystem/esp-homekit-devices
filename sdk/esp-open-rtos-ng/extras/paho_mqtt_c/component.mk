# Component makefile for extras/paho_mqtt_c

# expected anyone using bmp driver includes it as 'paho_mqtt_c/MQTT*.h'
INC_DIRS += $(paho_mqtt_c_ROOT)..

# args for passing into compile rule generation
paho_mqtt_c_SRC_DIR =  $(paho_mqtt_c_ROOT)

$(eval $(call component_compile_rules,paho_mqtt_c))
