
wolfssl_VERSION = 3.13.0-stable
wolfssl_THIRDPARTY_ROOT = $(wolfssl_ROOT)wolfssl-$(wolfssl_VERSION)

INC_DIRS += $(wolfssl_ROOT) $(wolfssl_THIRDPARTY_ROOT)

wolfssl_INC_DIR = $(wolfssl_THIRDPARTY_ROOT)
wolfssl_SRC_DIR = $(wolfssl_THIRDPARTY_ROOT)/src $(wolfssl_THIRDPARTY_ROOT)/wolfcrypt/src

EXTRA_CFLAGS += -DWOLFSSL_USER_SETTINGS \
    -DNO_SESSION_CACHE \
    -DRSA_LOW_MEM \
    -DGCM_SMALL \
#    -DCURVE25519_SMALL \
#    -DED25519_SMALL \
    -DUSE_SLOW_SHA512

$(eval $(call component_compile_rules,wolfssl))

