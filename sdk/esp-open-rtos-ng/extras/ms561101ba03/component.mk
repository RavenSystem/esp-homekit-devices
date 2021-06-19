# Component makefile for extras/ms561101ba03

# expected anyone using this driver includes it as 'ms561101ba03/ms561101ba03.h'
INC_DIRS += $(ms561101ba03_ROOT)..

# args for passing into compile rule generation
ms561101ba03_SRC_DIR = $(ms561101ba03_ROOT)

$(eval $(call component_compile_rules,ms561101ba03))
