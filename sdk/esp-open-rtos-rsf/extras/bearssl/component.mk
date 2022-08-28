# Component makefile for BearSSL

BEARSSL_DIR = $(bearssl_ROOT)BearSSL/
INC_DIRS += $(BEARSSL_DIR)inc

# args for passing into compile rule generation
bearssl_INC_DIR = $(BEARSSL_DIR)inc $(BEARSSL_DIR)src
bearssl_SRC_DIR = $(BEARSSL_DIR)src $(sort $(dir $(wildcard $(BEARSSL_DIR)src/*/)))

$(eval $(call component_compile_rules,bearssl))

# Helpful error if git submodule not initialised
$(BEARSSL_DIR):
	$(error "bearssl git submodule not installed. Please run 'git submodule update --init'")
