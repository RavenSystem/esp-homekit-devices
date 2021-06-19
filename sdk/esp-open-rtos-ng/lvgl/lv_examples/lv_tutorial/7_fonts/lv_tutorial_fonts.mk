CSRCS += lv_tutorial_fonts.c
CSRCS += arial_ascii_20.c
CSRCS += arial_cyrillic_20.c
CSRCS += arial_math_20.c

DEPPATH += --dep-path lv_examples/lv_tutorial/7_fonts
VPATH += :lv_examples/lv_tutorial/7_fonts

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tutorial/7_fonts"
