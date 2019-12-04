# Component makefile for new_dht

INC_DIRS += $(new_dht_ROOT)

new_dht_INC_DIR = $(new_dht_ROOT)
new_dht_SRC_DIR = $(new_dht_ROOT)

$(eval $(call component_compile_rules,new_dht))

