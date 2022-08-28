CSRCS += lv_test_roller.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_objx/lv_test_roller
VPATH += :lv_examples/lv_tests/lv_test_objx/lv_test_roller

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_objx/lv_test_roller"
