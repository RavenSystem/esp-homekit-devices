PROGRAM = adv_button

FLASH_SIZE = 8
FLASH_MODE = dout
FLASH_SPEED = 40

EXTRA_CFLAGS += -I../..

include $(abspath ../../sdk/esp-open-rtos/common.mk)

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
