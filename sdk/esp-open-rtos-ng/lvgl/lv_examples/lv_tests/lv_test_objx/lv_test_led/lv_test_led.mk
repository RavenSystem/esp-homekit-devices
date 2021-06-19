CSRCS += lv_test_led.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_objx/lv_test_led
VPATH += :lv_examples/lv_tests/lv_test_objx/lv_test_led

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_objx/lv_test_led"
