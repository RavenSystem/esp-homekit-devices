PROGRAM = haamain


EXTRA_COMPONENTS = \
	$(abspath setup_mode) \
    extras/onewire \
    extras/ds18b20 \
    extras/multipwm \
	extras/http-parser \
	extras/dhcpserver \
	extras/rboot-ota \
	extras/i2c \
    $(abspath ../../external_libs/wolfssl) \
    $(abspath ../../external_libs/cJSON) \
    $(abspath ../../external_libs/homekit) \
    $(abspath ../../libs/adv_button) \
    $(abspath ../../libs/adv_hlw) \
	$(abspath ../../libs/new_dht) \
	$(abspath ../../libs/ping) \
	$(abspath ../../libs/adv_logger) \
	$(abspath ../../libs/timers_helper)

FLASH_SIZE = 8
FLASH_MODE = dout
FLASH_SPEED = 40

# Max HAA firmware size is 581616 bytes

HOMEKIT_SPI_FLASH_BASE_ADDR = 0xf2000
HOMEKIT_MAX_CLIENTS = 16
HOMEKIT_SMALL = 0

#EXTRA_CFLAGS += -Os
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

EXTRA_CFLAGS += -DconfigMINIMAL_STACK_SIZE=256
#EXTRA_CFLAGS += -DconfigTIMER_TASK_PRIORITY=\(configMAX_PRIORITIES-1\)
EXTRA_CFLAGS += -DconfigTIMER_QUEUE_LENGTH=30
EXTRA_CFLAGS += -DconfigTIMER_TASK_STACK_DEPTH=640

## HAA DEBUG
#EXTRA_CFLAGS += -DHAA_DEBUG -DconfigUSE_TRACE_FACILITY

## HOMEKIT DEBUG
#EXTRA_CFLAGS += -DHOMEKIT_DEBUG=1

## mDNS Responder DEBUG
#EXTRA_CFLAGS += -DqDebugLog -DqLogAllTraffic

include $(abspath ../../sdk/esp-open-rtos/common.mk)

LIBS += m

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
