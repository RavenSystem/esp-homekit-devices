CSRCS += lv_tutorial_antialiasing.c
CSRCS += apple_icon_alpha.c
CSRCS += apple_icon_chroma.c

DEPPATH += --dep-path lv_examples/lv_tutorial/5_antialiasing
VPATH += :lv_examples/lv_tutorial/5_antialiasing

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tutorial/5_antialiasing"
