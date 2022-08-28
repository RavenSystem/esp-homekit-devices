CSRCS += lv_tutorial_objects.c

DEPPATH += --dep-path lv_examples/lv_tutorial/2_objects
VPATH += :lv_examples/lv_tutorial/2_objects

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tutorial/2_objects"
