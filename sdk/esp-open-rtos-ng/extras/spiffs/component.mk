# Component makefile for extras/spiffs

# If spiffs is configured as SINGLETON it must be configured in compile time.
SPIFFS_SINGLETON ?= 1

SPIFFS_BASE_ADDR ?= 0x300000
SPIFFS_SIZE ?= 0x100000
SPIFFS_LOG_PAGE_SIZE ?= 256
SPIFFS_LOG_BLOCK_SIZE ?= 8192


spiffs_CFLAGS += -DSPIFFS_SINGLETON=$(SPIFFS_SINGLETON)
ifeq ($(SPIFFS_SINGLETON),1)
# Singleton configuration
spiffs_CFLAGS += -DSPIFFS_BASE_ADDR=$(SPIFFS_BASE_ADDR)
spiffs_CFLAGS += -DSPIFFS_SIZE=$(SPIFFS_SIZE)
endif

spiffs_CFLAGS += -DSPIFFS_LOG_PAGE_SIZE=$(SPIFFS_LOG_PAGE_SIZE)
spiffs_CFLAGS += -DSPIFFS_LOG_BLOCK_SIZE=$(SPIFFS_LOG_BLOCK_SIZE)

# Main program needs SPIFFS definitions because it includes spiffs_config.h
PROGRAM_CFLAGS += $(spiffs_CFLAGS)

spiffs_CFLAGS := $(CFLAGS) $(spiffs_CFLAGS)

INC_DIRS += $(spiffs_ROOT)
INC_DIRS += $(spiffs_ROOT)spiffs/src

# args for passing into compile rule generation
spiffs_SRC_DIR = $(spiffs_ROOT)spiffs/src
spiffs_SRC_DIR += $(spiffs_ROOT)

# Create an SPIFFS image of specified directory and flash it with
# the rest of the firmware.
#
# Argumens:
#   $(1) - directory with files which go into spiffs image
#
# Example:
#  $(eval $(call make_spiffs_image,files))
define make_spiffs_image
SPIFFS_IMAGE = $(addprefix $(FIRMWARE_DIR),spiffs.bin)
MKSPIFFS_DIR = $(ROOT)/extras/spiffs/mkspiffs
MKSPIFFS = $$(MKSPIFFS_DIR)/mkspiffs
SPIFFS_FILE_LIST = $(shell find $(1))

all: $$(SPIFFS_IMAGE)

clean: clean_spiffs_img clean_mkspiffs

$$(SPIFFS_IMAGE): $$(MKSPIFFS) $$(SPIFFS_FILE_LIST) Makefile
	$$< -D $(1) -f $$@ -s $(SPIFFS_SIZE) -p $(SPIFFS_LOG_PAGE_SIZE) \
		-b $(SPIFFS_LOG_BLOCK_SIZE)

# Rebuild SPIFFS if Makefile is changed, where SPIFF_SIZE is defined
$$(spiffs_ROOT)spiffs_config.h: Makefile
	$$(Q) touch $$@

$$(MKSPIFFS):
	$$(MAKE) -C $$(MKSPIFFS_DIR)

clean_spiffs_img:
	$$(Q) rm -f $$(SPIFFS_IMAGE)

clean_mkspiffs:
	$$(Q) $$(MAKE) -C $$(MKSPIFFS_DIR) clean

SPIFFS_ESPTOOL_ARGS = $(SPIFFS_BASE_ADDR) $$(SPIFFS_IMAGE)
endef

$(eval $(call component_compile_rules,spiffs))
