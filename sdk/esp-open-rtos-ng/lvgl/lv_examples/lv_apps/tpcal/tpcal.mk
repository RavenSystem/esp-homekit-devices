CSRCS += tpcal.c

DEPPATH += --dep-path lv_examples/lv_apps/tpcal
VPATH += :lv_examples/lv_apps/tpcal

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_apps/tpcal"
