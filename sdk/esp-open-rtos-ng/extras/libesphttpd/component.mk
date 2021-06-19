# Component makefile for extras/libesphttpd

INC_DIRS += $(libesphttpd_ROOT)/libesphttpd/include $(libesphttpd_ROOT)/libesphttpd/espfs $(libesphttpd_ROOT)/libesphttpd/lib/heatshrink

LIBESPHTTPD_MAX_CONNECTIONS ?= 4
LIBESPHTTPD_STACKSIZE ?= 2048
LIBESPHTTPD_OTA_TAGNAME ?= generic
LIBESPHTTPD_HTML_DIR ?= html
LIBESPHTTPD_HTML_FILES ?= $(call rwildcard,$(LIBESPHTTPD_HTML_DIR)/,*)

RBOOT_OTA ?= 1
ESP_IP ?= 192.168.4.1

# args for passing into compile rule generation
libesphttpd_SRC_DIR = $(libesphttpd_ROOT)/libesphttpd/core $(libesphttpd_ROOT)/libesphttpd/espfs $(libesphttpd_ROOT)/libesphttpd/util
libesphttpd_CFLAGS += -DFREERTOS -DUSE_OPEN_SDK -DHTTPD_MAX_CONNECTIONS=$(LIBESPHTTPD_MAX_CONNECTIONS) -DHTTPD_STACKSIZE=$(LIBESPHTTPD_STACKSIZE) -DESPFS_HEATSHRINK -D__ets__ -DRBOOT_OTA=1 -std=gnu99

$(eval $(call component_compile_rules,libesphttpd))

rwildcard = $(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

LIBESPHTTPD_MKESPFSIMAGE_DIR = $(BUILD_DIR)mkespfsimage

LIBESPHTTPD_MKESPFS = $(LIBESPHTTPD_MKESPFSIMAGE_DIR)/mkespfsimage
LIBESPHTTPD_HTML_TINY_DIR = $(BUILD_DIR)html
LIBESPHTTPD_HTML_ESPFS = $(BUILD_DIR)web.espfs.bin
LIBESPHTTPD_HTML_ESPFS_PATH = $(PROGRAM_REAL_ROOT)/$(LIBESPHTTPD_HTML_ESPFS)
LIBESPHTTPD_HTML_ESPFS_OBJ = $(BUILD_DIR)web.espfs.o

CROSS_BINARY_ARCH ?= xtensa
CROSS_OUTPUT_TARGET ?= elf32-xtensa-le

LIBESPHTTPD_HTML_TINY ?= no

CURL = curl
LIBESPHTTPD_CURL_OPTS = --connect-timeout 3 --max-time 60 -s

$(LIBESPHTTPD_MKESPFSIMAGE_DIR):
	$(Q)mkdir -p $(LIBESPHTTPD_MKESPFSIMAGE_DIR)

$(PROGRAM_REAL_ROOT)/$(LIBESPHTTPD_MKESPFS): $(LIBESPHTTPD_MKESPFSIMAGE_DIR)
	make -C $(libesphttpd_ROOT)/libesphttpd/espfs/mkespfsimage CC=gcc GZIP_COMPRESSION=yes USE_HEATSHRINK=yes BUILD_DIR=$(PROGRAM_REAL_ROOT)/$(BUILD_DIR)

$(LIBESPHTTPD_HTML_ESPFS): $(PROGRAM_REAL_ROOT)/$(LIBESPHTTPD_MKESPFS) $(LIBESPHTTPD_HTML_FILES)
	cd $(LIBESPHTTPD_HTML_DIR) && find $(patsubst $(LIBESPHTTPD_HTML_DIR)/%,%,$(LIBESPHTTPD_HTML_FILES)) | $< > $(LIBESPHTTPD_HTML_ESPFS_PATH) || rm -f $(LIBESPHTTPD_HTML_ESPFS_PATH)

$(LIBESPHTTPD_HTML_ESPFS_OBJ): $(LIBESPHTTPD_HTML_ESPFS)
	$(Q)$(OBJCOPY) -I binary -O $(CROSS_OUTPUT_TARGET) -B $(CROSS_BINARY_ARCH) --rename-section .data=.irom.espfs $^ $@

$(BUILD_DIR)libesphttpd.a: $(LIBESPHTTPD_HTML_ESPFS_OBJ)

htmlfs: $(LIBESPHTTPD_HTML_ESPFS_OBJ)

$(libesphttpd_ROOT)/libesphttpd/mkupgimg/mkupgimg:
	make -C $(libesphttpd_ROOT)/libesphttpd/mkupgimg CC=gcc RBOOT_OTA=$(RBOOT_OTA)

$(FIRMWARE_DIR)ota_$(PROGRAM).bin: $(FW_FILE) $(libesphttpd_ROOT)/libesphttpd/mkupgimg/mkupgimg
	$(libesphttpd_ROOT)/libesphttpd/mkupgimg/mkupgimg $(FW_FILE) "$(LIBESPHTTPD_OTA_TAGNAME)" $(FIRMWARE_DIR)ota_$(PROGRAM).bin

ota: $(FIRMWARE_DIR)ota_$(PROGRAM).bin $(libesphttpd_ROOT)/libesphttpd/mkupgimg/mkupgimg

webflash: $(FIRMWARE_DIR)ota_$(PROGRAM).bin
	$(CURL) $(LIBESPHTTPD_CURL_OPTS) --data-binary "@$(FIRMWARE_DIR)ota_$(PROGRAM).bin" http://$(ESP_IP)/flash/upload && $(CURL) $(LIBESPHTTPD_CURL_OPTS) http://$(ESP_IP)/flash/reboot

