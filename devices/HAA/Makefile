PROGRAM = haamain


EXTRA_COMPONENTS = \
	$(abspath setup_mode) \
    extras/onewire \
    extras/ds18b20 \
	extras/dhcpserver \
	extras/rboot-ota \
	$(abspath ../../external_libs/http-parser) \
    $(abspath ../../external_libs/wolfssl) \
    $(abspath ../../external_libs/cJSON) \
    $(abspath ../../external_libs/homekit) \
	$(abspath ../../libs/adv_i2c) \
    $(abspath ../../libs/adv_button) \
    $(abspath ../../libs/adv_hlw) \
	$(abspath ../../libs/adv_pwm) \
	$(abspath ../../libs/new_dht) \
	$(abspath ../../libs/raven_ntp) \
	$(abspath ../../libs/ping) \
	$(abspath ../../libs/adv_logger_ntp) \
	$(abspath ../../libs/timers_helper)

FLASH_SIZE = 8
FLASH_MODE = dout
FLASH_SPEED = 40

# Max HAA firmware size is 581616 bytes

HOMEKIT_SPI_FLASH_BASE_ADDR = 0xf2000
HOMEKIT_MAX_CLIENTS = 8
HOMEKIT_SMALL = 0

EXTRA_CFLAGS += -O2 \
	-fpredictive-commoning \
	-ftree-slp-vectorize \
	-fgcse-after-reload \
	-floop-interchange \
	-fvect-cost-model \
	-ftree-partial-pre \
	-ftree-loop-distribution \
	-fipa-cp-clone \
	-funswitch-loops \
	-fpeel-loops \

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS

EXTRA_CFLAGS += -DESP_OPEN_RTOS

EXTRA_CFLAGS += -DHOMEKIT_OVERCLOCK_PAIR_VERIFY
EXTRA_CFLAGS += -DHOMEKIT_OVERCLOCK_PAIR_SETUP
EXTRA_CFLAGS += -DHOMEKIT_OVERCLOCK_GET_ACC
EXTRA_CFLAGS += -DHOMEKIT_DISABLE_MAXLEN_CHECK

EXTRA_CFLAGS += -DLWIP_NETIF_HOSTNAME=1
EXTRA_CFLAGS += -DLWIP_RAW=1
EXTRA_CFLAGS += -DDEFAULT_RAW_RECVMBOX_SIZE=5
EXTRA_CFLAGS += -DWIFI_PARAM_SAVE=0

EXTRA_CFLAGS += -DconfigMAX_TASK_NAME_LEN=10
EXTRA_CFLAGS += -DconfigCHECK_FOR_STACK_OVERFLOW=0
EXTRA_CFLAGS += -DconfigMINIMAL_STACK_SIZE=256
#EXTRA_CFLAGS += -DconfigTIMER_TASK_PRIORITY=\(configMAX_PRIORITIES-1\)
EXTRA_CFLAGS += -DconfigTIMER_TASK_PRIORITY=\(tskIDLE_PRIORITY+3\)
EXTRA_CFLAGS += -DconfigTIMER_QUEUE_LENGTH=35
EXTRA_CFLAGS += -DconfigTIMER_TASK_STACK_DEPTH=660

## HAA DEBUG
#EXTRA_CFLAGS += -DHAA_DEBUG -DconfigUSE_TRACE_FACILITY

## HAA POWER MONITOR DEBUG
#EXTRA_CFLAGS += -DPOWER_MONITOR_DEBUG

## HAA LIGHTBULB DEBUG
#EXTRA_CFLAGS += -DLIGHT_DEBUG

## HOMEKIT DEBUG
#EXTRA_CFLAGS += -DHOMEKIT_DEBUG=1

## mDNS Responder DEBUG
#EXTRA_CFLAGS += -DqDebugLog -DqLogIncoming -DqLogAllTraffic

include $(abspath ../../sdk/esp-open-rtos/common.mk)

LIBS += m

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
