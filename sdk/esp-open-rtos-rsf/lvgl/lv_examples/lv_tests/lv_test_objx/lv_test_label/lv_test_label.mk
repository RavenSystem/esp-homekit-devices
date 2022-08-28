CSRCS += lv_test_label.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_objx/lv_test_label
VPATH += :lv_examples/lv_tests/lv_test_objx/lv_test_label

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_objx/lv_test_label"
