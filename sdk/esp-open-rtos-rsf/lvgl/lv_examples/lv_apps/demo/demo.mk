CSRCS += demo.c
CSRCS += img_bubble_pattern.c

DEPPATH += --dep-path lv_examples/lv_apps/demo
VPATH += :lv_examples/lv_apps/demo

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_apps/demo"
