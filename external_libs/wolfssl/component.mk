
wolfssl_VERSION = 3.13.0-stable
wolfssl_THIRDPARTY_ROOT = $(wolfssl_ROOT)wolfssl-$(wolfssl_VERSION)

INC_DIRS += $(wolfssl_ROOT) $(wolfssl_THIRDPARTY_ROOT)

wolfssl_INC_DIR = $(wolfssl_THIRDPARTY_ROOT)
wolfssl_SRC_DIR = $(wolfssl_THIRDPARTY_ROOT)/src $(wolfssl_THIRDPARTY_ROOT)/wolfcrypt/src
wolfssl_SRC_FILES = $(foreach sdir,$(wolfssl_SRC_DIR),$(wildcard $(sdir)/*.c))

EXTRA_CFLAGS += -DESP_OPEN_RTOS -DWOLFSSL_USER_SETTINGS

$(eval $(call component_compile_rules,wolfssl))

