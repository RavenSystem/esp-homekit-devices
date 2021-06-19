CSRCS += lv_test_line.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_objx/lv_test_line
VPATH += :lv_examples/lv_tests/lv_test_objx/lv_test_line

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_objx/lv_test_line"
