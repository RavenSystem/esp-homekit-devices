PROGRAM = Sonoff_RavenCore

EXTRA_COMPONENTS = \
    extras/dht \
	extras/http-parser \
	extras/dhcpserver \
    extras/rboot-ota \
    $(abspath ../../external_libs/wolfssl) \
    $(abspath ../../external_libs/cJSON) \
    $(abspath ../../external_libs/homekit) \
    $(abspath ../../libs/led_codes) \
    $(abspath ../../libs/legacy_config)

FLASH_SIZE = 8
FLASH_MODE = dout
FLASH_SPEED = 40
WIFI_CONFIG_CONNECT_TIMEOUT = 60000
HOMEKIT_SPI_FLASH_BASE_ADDR = 0x8c000
HOMEKIT_MAX_CLIENTS = 12
HOMEKIT_OVERCLOCK_PAIR_SETUP = 0

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS

## DEBUG
#EXTRA_CFLAGS += -DHOMEKIT_DEBUG=1
#EXTRA_CFLAGS += -DqDebugLog

include $(abspath ../../sdk/esp-open-rtos/common.mk)

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)

signature:
    $(openssl sha384 -binary -out firmware/Sonoff_RavenCore.bin.sig firmware/Sonoff_RavenCore.bin)
    $(printf "%08x" `cat firmware/Sonoff_RavenCore.bin | wc -c` | xxd -r -p >> firmware/Sonoff_RavenCore.bin.sig)
