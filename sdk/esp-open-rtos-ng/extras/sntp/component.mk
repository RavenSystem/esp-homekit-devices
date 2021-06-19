# Component makefile for extras/sntp

INC_DIRS += $(sntp_ROOT)

# args for passing into compile rule generation
sntp_SRC_DIR =  $(sntp_ROOT)

# For SNTP logging, either supply own SNTP_LOGD
# or define SNTP_LOGD_WITH_PRINTF (see sntp_fun.c)

# sntp_CFLAGS = $(CFLAGS) -DSNTP_LOGD_WITH_PRINTF

$(eval $(call component_compile_rules,sntp))
