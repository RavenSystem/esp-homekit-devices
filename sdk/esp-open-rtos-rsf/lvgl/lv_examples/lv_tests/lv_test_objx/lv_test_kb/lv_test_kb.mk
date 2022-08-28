CSRCS += lv_test_kb.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_objx/lv_test_kb
VPATH += :lv_examples/lv_tests/lv_test_objx/lv_test_kb

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_objx/lv_test_kb"
