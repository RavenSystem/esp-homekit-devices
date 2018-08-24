PROGRAM = Sonoff_Basic_wPOW

EXTRA_COMPONENTS = \
	extras/http-parser \
	extras/dhcpserver \
    $(abspath ../../external_libs/wolfssl) \
    $(abspath ../../external_libs/cJSON) \
    $(abspath ../../external_libs/homekit) \
    $(abspath ../../external_libs/wifi_config) \
    $(abspath ../../libs/led_codes)

FLASH_SIZE = 8
FLASH_MODE = dout
FLASH_SPEED = 40
HOMEKIT_SPI_FLASH_BASE_ADDR = 0x7A000
HOMEKIT_MAX_CLIENTS = 12
HOMEKIT_OVERCLOCK_PAIR_SETUP = 0

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS

## DEBUG
#EXTRA_CFLAGS += -DHOMEKIT_DEBUG=1
#EXTRA_CFLAGS += -DqDebugLog

include $(abspath ../../sdk/esp-open-rtos/common.mk)

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
