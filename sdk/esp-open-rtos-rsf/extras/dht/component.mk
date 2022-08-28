# Component makefile for extras/dht

# include it as 'dht/dht.h'
INC_DIRS += $(dht_ROOT)..

# args for passing into compile rule generation
dht_SRC_DIR = $(dht_ROOT)

$(eval $(call component_compile_rules,dht))

