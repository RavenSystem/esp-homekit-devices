wolfssl_VERSION = 3.13.0-stable

ifdef component_compile_rules
    # ESP_OPEN_RTOS
    wolfssl_THIRDPARTY_ROOT = $(wolfssl_ROOT)wolfssl-$(wolfssl_VERSION)

    INC_DIRS += $(wolfssl_ROOT) $(wolfssl_THIRDPARTY_ROOT)

    wolfssl_INC_DIR = $(wolfssl_THIRDPARTY_ROOT)
    wolfssl_SRC_DIR = $(wolfssl_THIRDPARTY_ROOT)/src $(wolfssl_THIRDPARTY_ROOT)/wolfcrypt/src
    wolfssl_SRC_FILES = $(foreach sdir,$(wolfssl_SRC_DIR),$(wildcard $(sdir)/*.c))

    EXTRA_CFLAGS += -DESP_OPEN_RTOS -DWOLFSSL_USER_SETTINGS

    $(eval $(call component_compile_rules,wolfssl))
else
    # ESP_IDF
    ifeq ($(IDF_TARGET),esp32)
    EXTRA_CFLAGS += -DWOLFSSL_ESPWROOM32
    endif

    wolfssl_THIRDPARTY_ROOT = wolfssl-$(wolfssl_VERSION)

    COMPONENT_SRCDIRS = $(wolfssl_THIRDPARTY_ROOT)/src $(wolfssl_THIRDPARTY_ROOT)/wolfcrypt/src $(wolfssl_THIRDPARTY_ROOT)/wolfcrypt/src/port/Espressif
    COMPONENT_OBJS = $(foreach compsrcdir,$(COMPONENT_SRCDIRS),$(patsubst $(COMPONENT_PATH)/%,%,$(patsubst %.c,%.o,$(wildcard $(COMPONENT_PATH)/$(compsrcdir)/*.c))))
    COMPONENT_ADD_INCLUDEDIRS = . include $(wolfssl_THIRDPARTY_ROOT)
endif
