CSRCS += lv_test_tabview.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_objx/lv_test_tabview
VPATH += :lv_examples/lv_tests/lv_test_objx/lv_test_tabview

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_objx/lv_test_tabview"
