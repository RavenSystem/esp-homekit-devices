# Component makefile for extras/http_client_ota

# Expected anyone using http_client_ota includes it as 'http_client_ota/ota'
INC_DIRS += $(http_client_ota_ROOT)

# args for passing into compile rule generation
http_client_ota_INC_DIR = $(http_client_ota_ROOT)
http_client_ota_SRC_DIR = $(http_client_ota_ROOT)

$(eval $(call component_compile_rules,http_client_ota))
