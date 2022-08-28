CSRCS += lv_test_obj.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_obj
VPATH += :lv_examples/lv_tests/lv_test_obj

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_obj"
