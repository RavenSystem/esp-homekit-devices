CSRCS += lv_test_mbox.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_objx/lv_test_mbox
VPATH += :lv_examples/lv_tests/lv_test_objx/lv_test_mbox

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_objx/lv_test_mbox"
