CSRCS += lv_test_stress.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_stress
VPATH += :lv_examples/lv_tests/lv_test_stress

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_stress"
