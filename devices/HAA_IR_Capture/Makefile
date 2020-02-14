PROGRAM = haa_ir_capture

include $(abspath ../../sdk/esp-open-rtos/common.mk)

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
