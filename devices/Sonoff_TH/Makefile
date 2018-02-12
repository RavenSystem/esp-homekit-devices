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

MORE_WOLFSSL_CFLAGS = \
    -DNO_SESSION_CACHE \
    -DRSA_LOW_MEM \
    -DGCM_SMALL \
    -DCURVE25519_SMALL \
    -DED25519_SMALL \
    -DUSE_SLOW_SHA512

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS $(MORE_WOLFSSL_CFLAGS) -DMDNS_RESPONDER_REPLY_SIZE=1500

include $(abspath ../../sdk/esp-open-rtos/common.mk)

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
