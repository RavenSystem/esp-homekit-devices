CSRCS += lv_test_page.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_objx/lv_test_page
VPATH += :lv_examples/lv_tests/lv_test_objx/lv_test_page

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_objx/lv_test_page"
