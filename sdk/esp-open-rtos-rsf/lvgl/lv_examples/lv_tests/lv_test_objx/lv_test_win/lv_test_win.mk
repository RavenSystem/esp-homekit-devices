CSRCS += lv_test_win.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_objx/lv_test_win
VPATH += :lv_examples/lv_tests/lv_test_objx/lv_test_win

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_objx/lv_test_win"
