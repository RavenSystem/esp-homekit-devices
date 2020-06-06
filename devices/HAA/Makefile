PROGRAM = haamain


EXTRA_COMPONENTS = \
	$(abspath setup_mode) \
    extras/onewire \
    extras/ds18b20 \
    extras/multipwm \
	extras/http-parser \
	extras/dhcpserver \
	extras/rboot-ota \
    $(abspath ../../external_libs/wolfssl) \
    $(abspath ../../external_libs/cJSON) \
    $(abspath ../../external_libs/homekit) \
    $(abspath ../../libs/adv_button) \
	$(abspath ../../libs/new_dht) \
	$(abspath ../../libs/ping) \
	$(abspath ../../libs/adv_logger)
	

FLASH_SIZE = 8
FLASH_MODE = dout
FLASH_SPEED = 40

# Max firmware size is about 565000 bytes
HOMEKIT_SPI_FLASH_BASE_ADDR = 0x8c000
HOMEKIT_MAX_CLIENTS = 20
HOMEKIT_SMALL = 0

EXTRA_CFLAGS += -Os
EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS

EXTRA_CFLAGS += -DHOMEKIT_OVERCLOCK_PAIR_VERIFY
EXTRA_CFLAGS += -DHOMEKIT_OVERCLOCK_PAIR_SETUP
EXTRA_CFLAGS += -DHOMEKIT_OVERCLOCK_GET_ACC
EXTRA_CFLAGS += -DHOMEKIT_OVERCLOCK_GET_CH
EXTRA_CFLAGS += -DHOMEKIT_OVERCLOCK_UPDATE_CH

EXTRA_CFLAGS += -DLWIP_NETIF_HOSTNAME=1
EXTRA_CFLAGS += -DLWIP_RAW=1
EXTRA_CFLAGS += -DDEFAULT_RAW_RECVMBOX_SIZE=5
EXTRA_CFLAGS += -DWIFI_PARAM_SAVE=0

## Create HAA_LCM compatible binary
#EXTRA_CFLAGS += -DHAALCM
#PROGRAM = haa_lcm

## HAA_DEBUG
#EXTRA_CFLAGS += -DHAA_DEBUG

## HOMEKIT DEBUG
#EXTRA_CFLAGS += -DHOMEKIT_DEBUG=1

include $(abspath ../../sdk/esp-open-rtos/common.mk)

LIBS += m

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
