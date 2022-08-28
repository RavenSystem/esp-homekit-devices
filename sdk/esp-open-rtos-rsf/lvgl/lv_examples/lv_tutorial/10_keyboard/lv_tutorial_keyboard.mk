#CSRCS += lv_tutorial_keyboard_simkpad.c
CSRCS += lv_tutorial_keyboard.c

DEPPATH += --dep-path lv_examples/lv_tutorial/10_keyboard
VPATH += :lv_examples/lv_tutorial/10_keyboard

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tutorial/10_keyboard"
