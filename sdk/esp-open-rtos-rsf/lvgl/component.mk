# Component makefile for lvgl/lvgl

# expected anyone using this driver includes it as 'lvgl/lvgl.h'
INC_DIRS += $(lvgl_ROOT)

# args for passing into compile rule generation
lvgl_SRC_DIR = $(lvgl_ROOT) \
        $(lvgl_ROOT)/lvgl \
	$(lvgl_ROOT)/lvgl/lv_core \
	$(lvgl_ROOT)/lvgl/lv_draw \
	$(lvgl_ROOT)/lvgl/lv_hal \
	$(lvgl_ROOT)/lvgl/lv_misc \
	$(lvgl_ROOT)/lvgl/lv_fonts \
	$(lvgl_ROOT)/lvgl/lv_objx \
	$(lvgl_ROOT)/lvgl/lv_themes \
	$(lvgl_ROOT)/lv_drivers/display \
	$(lvgl_ROOT)/lv_drivers/indev

LVGL_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

#EXTRA_CFLAGS += -DLV_CONF_INCLUDE_SIMPLE=1

#include $(LVGL_DIR)defaults.mk
#
#fonts_CFLAGS = $(CFLAGS) \
	-DLV_HOR_RES=$(LV_HOR_RES) \
	-DLV_VER_RES=$(LV_VER_RES) \
	-DLV_DPI=$(LV_DPI) \
	-DLV_VDB_SIZE=$(LV_VDB_SIZE) \
	-DLV_VDB_ADR=$(LV_VDB_ADR) \
	-DLV_VDB_DOUBLE=$(LV_VDB_DOUBLE) \
	-DLV_VDB2_ADR=$(LV_VDB2_ADR) \
	-DLV_ANTIALIAS=$(LV_ANTIALIAS) \
	-DLV_FONT_ANTIALIAS=$(LV_FONT_ANTIALIAS) \
	-DLV_REFR_PERIOD=$(LV_REFR_PERIOD)

$(eval $(call component_compile_rules,lvgl))
