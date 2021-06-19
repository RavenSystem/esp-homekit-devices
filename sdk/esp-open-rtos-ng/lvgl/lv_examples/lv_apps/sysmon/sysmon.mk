CSRCS += sysmon.c

DEPPATH += --dep-path lv_examples/lv_apps/sysmon
VPATH += :lv_examples/lv_apps/sysmon

CFLAGS += "-I$(LVGL_DIR)/lv_examples/lv_apps/sysmon"
