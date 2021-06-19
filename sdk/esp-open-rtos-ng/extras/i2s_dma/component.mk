# Component makefile for extras/i2s_dma

# expected anyone using i2s_dma driver includes it as 'i2s_dma/i2s_dma.h'
INC_DIRS += $(i2s_dma_ROOT)..

# args for passing into compile rule generation
i2s_dma_SRC_DIR =  $(i2s_dma_ROOT)

$(eval $(call component_compile_rules,i2s_dma))
