# Component makefile for extras/hmc5883l

# expected anyone using this driver includes it as 'hmc5883l/hmc5883l.h'
INC_DIRS += $(hmc5883l_ROOT)..

# args for passing into compile rule generation
hmc5883l_SRC_DIR = $(hmc5883l_ROOT)

$(eval $(call component_compile_rules,hmc5883l))
