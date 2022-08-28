CSRCS += terminal.c

DEPPATH += --dep-path lv_examples/lv_apps/terminal
VPATH += :lv_examples/lv_apps/terminal

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_apps/terminal"
