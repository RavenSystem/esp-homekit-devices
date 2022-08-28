PROGRAM=sysparam_editor

# Setting this to 1..3 will add extra debugging output to stdout
EXTRA_CFLAGS = -DSYSPARAM_DEBUG=0

# Avoid writing the wifi state to flash when using wificfg.
EXTRA_CFLAGS += -DWIFI_PARAM_SAVE=0

include ../../common.mk

# `make dump-flash` can be used to view the current contents of the sysparam
# regions in flash.
dump-flash:
	esptool.py read_flash 0x1f7000 8192 r1.bin
	hexdump -C r1.bin
	esptool.py read_flash 0x1f9000 8192 r2.bin
	hexdump -C r2.bin
