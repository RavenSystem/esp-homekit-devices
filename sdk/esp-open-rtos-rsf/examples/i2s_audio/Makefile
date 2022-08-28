PROGRAM=i2s_audio_example
EXTRA_COMPONENTS = extras/spiffs extras/i2s_dma
FLASH_SIZE = 32

# spiffs configuration
SPIFFS_BASE_ADDR = 0x200000
SPIFFS_SIZE = 0x100000

include ../../common.mk

$(eval $(call make_spiffs_image,files))
