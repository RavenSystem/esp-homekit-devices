CSRCS += fbdev.c
CSRCS += monitor.c
CSRCS += R61581.c
CSRCS += SSD1963.c
CSRCS += ST7565.c

DEPPATH += --dep-path lv_drivers/display
VPATH += :lv_drivers/display

CFLAGS += "-I$(LVGL_DIR)/lv_drivers/display"
