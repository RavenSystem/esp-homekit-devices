# Component makefile for extras/dhcpserver

INC_DIRS += $(dhcpserver_ROOT)include

# args for passing into compile rule generation
dhcpserver_INC_DIR =  $(dhcpserver_ROOT)
dhcpserver_SRC_DIR =  $(dhcpserver_ROOT)

$(eval $(call component_compile_rules,dhcpserver))
