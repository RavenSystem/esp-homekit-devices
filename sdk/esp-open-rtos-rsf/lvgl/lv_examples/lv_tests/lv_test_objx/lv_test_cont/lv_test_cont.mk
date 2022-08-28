CSRCS += lv_test_cont.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_objx/lv_test_cont
VPATH += :lv_examples/lv_tests/lv_test_objx/lv_test_cont

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_objx/lv_test_cont"
