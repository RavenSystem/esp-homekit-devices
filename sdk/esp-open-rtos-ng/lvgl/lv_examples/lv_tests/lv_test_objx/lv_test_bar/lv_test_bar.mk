CSRCS += lv_test_bar.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_objx/lv_test_bar
VPATH += :lv_examples/lv_tests/lv_test_objx/lv_test_bar

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_objx/lv_test_bar"
