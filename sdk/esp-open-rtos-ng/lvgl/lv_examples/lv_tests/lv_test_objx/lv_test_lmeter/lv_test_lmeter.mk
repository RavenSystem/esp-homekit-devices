CSRCS += lv_test_lmeter.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_objx/lv_test_lmeter
VPATH += :lv_examples/lv_tests/lv_test_objx/lv_test_lmeter

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_objx/lv_test_lmeter"
