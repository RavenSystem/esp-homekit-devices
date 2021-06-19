CSRCS += lv_test_imgbtn.c

DEPPATH += --dep-path lv_examples/lv_tests/lv_test_objx/lv_test_imgbtn
VPATH += :lv_examples/lv_tests/lv_test_objx/lv_test_imgbtn

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_tests/lv_test_objx/lv_test_imgbtn"
