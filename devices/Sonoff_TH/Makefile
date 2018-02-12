PROGRAM = Sonoff_TH

EXTRA_COMPONENTS = \
    extras/dht \
	extras/http-parser \
	extras/dhcpserver \
	$(abspath ../../components/wifi_config) \
	$(abspath ../../components/wolfssl) \
	$(abspath ../../components/cJSON) \
	$(abspath ../../components/homekit) \
    $(abspath ../../components/led_codes)

FLASH_SIZE = 8
FLASH_MODE = dout
FLASH_SPEED = 40
HOMEKIT_SPI_FLASH_BASE_ADDR = 0x7A000

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS -DMDNS_RESPONDER_REPLY_SIZE=1024

include $(abspath ../../sdk/esp-open-rtos/common.mk)

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
