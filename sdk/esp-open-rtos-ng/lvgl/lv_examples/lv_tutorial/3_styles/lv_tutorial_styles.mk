CSRCS += lv_tutorial_styles.c

DEPPATH += --dep-path lv_examples/lv_tutorial/3_styles
VPATH += :lv_examples/lv_tutorial/3_styles

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tutorial/3_styles"
