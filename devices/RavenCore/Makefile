PROGRAM = main

EXTRA_COMPONENTS = \
    extras/onewire \
    extras/ds18b20 \
	extras/dht \
    extras/multipwm \
	extras/http-parser \
	extras/dhcpserver \
	extras/rboot-ota \
    $(abspath ../../external_libs/wolfssl) \
    $(abspath ../../external_libs/cJSON) \
    $(abspath ../../external_libs/homekit) \
    $(abspath ../../external_libs/wifi_config) \
    $(abspath ../../libs/adv_button)

FLASH_SIZE = 8
FLASH_MODE = dout
FLASH_SPEED = 40

HOMEKIT_SPI_FLASH_BASE_ADDR = 0x8c000
HOMEKIT_MAX_CLIENTS = 16
HOMEKIT_SMALL = 0

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS -DWIFI_CONFIG_CONNECT_TIMEOUT=300000
EXTRA_CFLAGS += -DHOMEKIT_OVERCLOCK_PAIR_VERIFY
EXTRA_CFLAGS += -DHOMEKIT_OVERCLOCK_PAIR_SETUP

## DEBUG
#EXTRA_CFLAGS += -DHOMEKIT_DEBUG=1

include $(abspath ../../sdk/esp-open-rtos/common.mk)

LIBS += m

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
