# Component makefile for extras/rboot-ota

# global include directories need to find rboot.h for integration, even 
# when just including rboot-api.h :(
INC_DIRS += $(rboot-ota_ROOT) $(ROOT)bootloader $(ROOT)bootloader/rboot

rboot-ota_SRC_DIR =  $(rboot-ota_ROOT)

$(eval $(call component_compile_rules,rboot-ota))

