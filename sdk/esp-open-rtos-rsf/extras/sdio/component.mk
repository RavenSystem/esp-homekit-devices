# Component makefile for extras/sdio
INC_DIRS += $(sdio_ROOT)..

# args for passing into compile rule generation
sdio_SRC_DIR = $(sdio_ROOT)

# Workaround unsupported CMD25 for very old SD cards
SDIO_CMD25_WORKAROUND ?= 0

sdio_CFLAGS = $(CFLAGS) -DSDIO_CMD25_WORKAROUND=$(SDIO_CMD25_WORKAROUND)

$(eval $(call component_compile_rules,sdio))