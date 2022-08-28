# Component makefile for LWIP

LWIP_DIR = $(lwip_ROOT)lwip/src/
INC_DIRS += $(LWIP_DIR)include $(ROOT)lwip/include $(lwip_ROOT)include $(LWIP_DIR)include/compat/posix $(LWIP_DIR)include/ipv4 $(LWIP_DIR)include/ipv4/lwip $(LWIP_DIR)include/lwip

# args for passing into compile rule generation
lwip_INC_DIR =  # all in INC_DIRS, needed for normal operation
lwip_SRC_DIR = $(lwip_ROOT) $(LWIP_DIR)api $(LWIP_DIR)core $(LWIP_DIR)core/ipv4 $(LWIP_DIR)core/ipv6 $(LWIP_DIR)netif
lwip_SRC_DIR += $(LWIP_DIR)apps/*

$(eval $(call component_compile_rules,lwip))

# Helpful error if git submodule not initialised
$(lwip_SRC_DIR):
	$(error "LWIP git submodule not installed. Please run 'git submodule init' then 'git submodule update'")

