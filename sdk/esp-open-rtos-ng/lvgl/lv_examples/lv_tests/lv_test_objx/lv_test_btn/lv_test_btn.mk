CSRCS += lv_test_btn.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_objx/lv_test_btn
VPATH += :lv_examples/lv_tests/lv_test_objx/lv_test_btn

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_objx/lv_test_btn"
