# include it as 'max7219/max7219.h'
INC_DIRS += $(max7219_ROOT)..

# args for passing into compile rule generation
max7219_SRC_DIR = $(max7219_ROOT)

$(eval $(call component_compile_rules,max7219))
