CSRCS += lv_test_group.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_group
VPATH += :lv_examples/lv_tests/lv_test_group

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_group"
