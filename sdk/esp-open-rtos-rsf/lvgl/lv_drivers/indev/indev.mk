CSRCS += FT5406EE8.c
CSRCS += keyboard.c
CSRCS += mouse.c
CSRCS += XPT2046.c

DEPPATH += --dep-path lv_drivers/indev
VPATH += :lv_drivers/indev

CFLAGS += "-I$(LVGL_DIR)/lv_drivers/indev"
