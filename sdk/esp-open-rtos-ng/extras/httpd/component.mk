# Component makefile for extras/httpd

# expected anyone using httpd includes it as 'httpd/httpd.h'
INC_DIRS += $(httpd_ROOT)..

# args for passing into compile rule generation
httpd_SRC_DIR = $(httpd_ROOT)

$(eval $(call component_compile_rules,httpd))
