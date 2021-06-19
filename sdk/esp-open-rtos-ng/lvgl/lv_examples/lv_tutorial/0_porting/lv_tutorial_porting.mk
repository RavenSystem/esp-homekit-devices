CSRCS += lv_tutorial_porting.c

DEPPATH += --dep-path lv_examples/lv_tutorial/0_porting
VPATH += :lv_examples/lv_tutorial/0_porting

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tutorial/0_porting"
