# esp-open-rtos common Makefile
#
# ******************************************************************
# Run 'make help' in any example subdirectory to see a usage summary
# (or skip to the bottom of this file!)
#
# For example, from the top level run:
# make help -C examples/http_get
# ******************************************************************
#
# In-depth documentation is at https://github.com/SuperHouse/esp-open-rtos/wiki/Build-Process
#
# Most sections Copyright 2015 Superhouse Automation Pty Ltd
# BSD Licensed as described in the file LICENSE at top level.
#
# This makefile is adapted from the esp-mqtt makefile by @tuanpmt
# https://github.com/tuanpmt/esp_mqtt, but it has changed very significantly
# since then.

# assume the 'root' directory (ie top of the tree) is the directory common.mk is in
ROOT := $(dir $(lastword $(MAKEFILE_LIST)))

include $(ROOT)parameters.mk

ifndef PROGRAM
$(error "Set the PROGRAM environment variable in your Makefile before including common.mk")
endif

# hacky way to get a single space value
empty :=
space := $(empty) $(empty)

# GNU Make lowercase function, bit of a horrorshow but works (courtesy http://stackoverflow.com/a/665045)
lc = $(subst A,a,$(subst B,b,$(subst C,c,$(subst D,d,$(subst E,e,$(subst F,f,$(subst G,g,$(subst H,h,$(subst I,i,$(subst J,j,$(subst K,k,$(subst L,l,$(subst M,m,$(subst N,n,$(subst O,o,$(subst P,p,$(subst Q,q,$(subst R,r,$(subst S,s,$(subst T,t,$(subst U,u,$(subst V,v,$(subst W,w,$(subst X,x,$(subst Y,y,$(subst Z,z,$1))))))))))))))))))))))))))

# assume the program dir is the directory the top-level makefile was run in
PROGRAM_DIR := $(dir $(firstword $(MAKEFILE_LIST)))

# derive various parts of compiler/linker arguments
SDK_LIB_ARGS  = $(addprefix -l,$(SDK_LIBS))
LIB_ARGS      = $(addprefix -l,$(LIBS))
PROGRAM_OUT   = $(BUILD_DIR)$(PROGRAM).out
LDFLAGS      += $(addprefix -T,$(LINKER_SCRIPTS))

FW_FILE = $(addprefix $(FIRMWARE_DIR),$(PROGRAM).bin)

# Common include directories, shared across all "components"
# components will add their include directories to this argument
#
# Placing $(PROGRAM_DIR) and $(PROGRAM_DIR)include first allows
# programs to have their own copies of header config files for components
# , which is useful for overriding things.
INC_DIRS      = $(PROGRAM_DIR) $(PROGRAM_DIR)include $(ROOT)include

ifeq ($(OWN_LIBC),1)
    INC_DIRS += $(ROOT)libc/xtensa-lx106-elf/include
    LDFLAGS += -L$(ROOT)libc/xtensa-lx106-elf/lib
   ifeq ($(PRINTF_SCANF_FLOAT_SUPPORT),1)
     LDFLAGS += -u _printf_float -u _scanf_float
   endif
endif

ifeq ("$(V)","1")
Q :=
vecho := @true
else
Q := @
vecho := @echo
endif

.PHONY: all clean flash erase_flash test size rebuild

all: $(PROGRAM_OUT) $(FW_FILE_1) $(FW_FILE_2) $(FW_FILE)

# component_compile_rules: Produces compilation rules for a given
# component
#
# For user-facing documentation, see:
# https://github.com/SuperHouse/esp-open-rtos/wiki/Build-Process#adding-a-new-component
#
# Call arguments are:
# $(1) - component name
#
# Expects that the following component-specific variables are defined:
#
# $(1)_ROOT    = Top-level dir containing component. Can be in-tree or out-of-tree.
#                (if this variable isn't defined, directory containing component.mk is used)
# $(1)_SRC_DIR = List of source directories for the component. All must be under $(1)_ROOT
# $(1)_INC_DIR = List of include directories specific for the component
#
#
# Each call appends to COMPONENT_ARS or WHOLE_ARCHIVES which are lists of archive files for compiled components
COMPONENT_ARS =
WHOLE_ARCHIVES =
define component_compile_rules
$(1)_DEFAULT_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))
$(1)_ROOT ?= $$($(1)_DEFAULT_ROOT)
$(1)_OBJ_DIR   = $(call lc,$(BUILD_DIR)$(1)/)
### determine source files and object files ###
$(1)_SRC_FILES ?= $$(foreach sdir,$$($(1)_SRC_DIR), 				\
			$$(wildcard $$(sdir)/*.c) $$(wildcard $$(sdir)/*.S) 	\
			$$(wildcard $$(sdir)/*.cpp)) 				\
			$$($(1)_EXTRA_SRC_FILES)
$(1)_REAL_SRC_FILES = $$(foreach sfile,$$($(1)_SRC_FILES),$$(realpath $$(sfile)))
$(1)_REAL_ROOT = $$(realpath $$($(1)_ROOT))
# patsubst here substitutes real component root path for the relative OBJ_DIR path, making things short again
$(1)_OBJ_FILES_CXX = $$(patsubst $$($(1)_REAL_ROOT)%.cpp,$$($(1)_OBJ_DIR)%.o,$$($(1)_REAL_SRC_FILES))
$(1)_OBJ_FILES_C = $$(patsubst $$($(1)_REAL_ROOT)%.c,$$($(1)_OBJ_DIR)%.o,$$($(1)_OBJ_FILES_CXX))
$(1)_OBJ_FILES = $$(patsubst $$($(1)_REAL_ROOT)%.S,$$($(1)_OBJ_DIR)%.o,$$($(1)_OBJ_FILES_C))
# the last included makefile is our component's component.mk makefile (rebuild the component if it changes)
$(1)_MAKEFILE ?= $(lastword $(MAKEFILE_LIST))

### determine compiler arguments ###
$(1)_CPPFLAGS ?= $(CPPFLAGS)
$(1)_CFLAGS ?= $(CFLAGS)
$(1)_CXXFLAGS ?= $(CXXFLAGS)
$(1)_CC_BASE = $(Q) $(CC) $$(addprefix -I,$$(INC_DIRS)) $$(addprefix -I,$$($(1)_INC_DIR)) $$($(1)_CPPFLAGS)
$(1)_AR = $(call lc,$(BUILD_DIR)$(1).a)

$$($(1)_OBJ_DIR)%.o: $$($(1)_REAL_ROOT)%.c $$($(1)_MAKEFILE) $(wildcard $(ROOT)*.mk) | $$($(1)_SRC_DIR)
	$(vecho) "CC $$<"
	$(Q) mkdir -p $$(dir $$@)
	$$($(1)_CC_BASE) $$($(1)_CFLAGS) -c $$< -o $$@
	$$($(1)_CC_BASE) $$($(1)_CFLAGS) -MM -MT $$@ -MF $$(@:.o=.d) $$<

$$($(1)_OBJ_DIR)%.o: $$($(1)_REAL_ROOT)%.cpp $$($(1)_MAKEFILE) $(wildcard $(ROOT)*.mk) | $$($(1)_SRC_DIR)
	$(vecho) "C++ $$<"
	$(Q) mkdir -p $$(dir $$@)
	$$($(1)_CC_BASE) $$($(1)_CXXFLAGS) -c $$< -o $$@
	$$($(1)_CC_BASE) $$($(1)_CXXFLAGS) -MM -MT $$@ -MF $$(@:.o=.d) $$<

$$($(1)_OBJ_DIR)%.o: $$($(1)_REAL_ROOT)%.S $$($(1)_MAKEFILE) $(wildcard $(ROOT)*.mk) | $$($(1)_SRC_DIR)
	$(vecho) "AS $$<"
	$(Q) mkdir -p $$(dir $$@)
	$$($(1)_CC_BASE) -c $$< -o $$@
	$$($(1)_CC_BASE) -MM -MT $$@ -MF $$(@:.o=.d) $$<

$(1)_AR_IN_FILES = $$($(1)_OBJ_FILES)

# The component is shown to depend on both obj and source files so we get
# a meaningful error message for missing explicitly named source files.
# But do not include source files into a static library because when adding this
# library with '--whole-archive' linker gives error that archive contains
# unknown objects (source files)
ifndef $(1)_WHOLE_ARCHIVE
   $(1)_AR_IN_FILES += $$($(1)_SRC_FILES)
endif

$$($(1)_AR): $$($(1)_AR_IN_FILES)
	$(vecho) "AR $$@"
	$(Q) mkdir -p $$(dir $$@)
	$(Q) $(AR) cru $$@ $$^

ifdef $(1)_WHOLE_ARCHIVE
   WHOLE_ARCHIVES += $$($(1)_AR)
else
   COMPONENT_ARS += $$($(1)_AR)
endif

-include $$($(1)_OBJ_FILES:.o=.d)
endef

# Remove comment lines from libgcc.remove file
$(BUILD_DIR)libgcc.remove: $(ROOT)lib/libgcc.remove | $(BUILD_DIR)
	$(Q) grep -v "^#" $< | cat > $@

# Remove unwanted object files listed in libgcc.remove
$(BUILD_DIR)libgcc.a: $(ROOT)lib/libgcc.a $(BUILD_DIR)libgcc.remove | $(BUILD_DIR)
	@echo "Removing unwanted objects from $<"
	$(Q) cat $< > $@
	$(Q) $(AR) d $@ @$(word 2,$^)

# Remove comment lines from libc.remove file
$(BUILD_DIR)libc.remove: $(ROOT)libc/libc.remove | $(BUILD_DIR)
	$(Q) grep -v "^#" $< | cat > $@

# Remove unwanted object files listed in libgcc.remove
$(BUILD_DIR)libc.a: $(ROOT)libc/xtensa-lx106-elf/lib/libc.a $(BUILD_DIR)libc.remove | $(BUILD_DIR)
	@echo "Removing unwanted objects from $<"
	$(Q) cat $< > $@
	$(Q) $(AR) d $@ @$(word 2,$^)

## Linking rules for SDK libraries
## SDK libraries are preprocessed to:
# - remove object files named in <libname>.remove
# - prefix all defined symbols with 'sdk_'
# - weaken all global symbols so they can be overriden from the open SDK side
#
# SDK binary libraries are preprocessed into $(BUILD_DIR)/sdklib
SDK_PROCESSED_LIBS = $(addsuffix .a,$(addprefix $(BUILD_DIR)sdklib/lib,$(SDK_LIBS)))

# Make rules for preprocessing each SDK library

# hacky, but prevents confusing error messages if one of these files disappears
$(ROOT)lib/%.remove:
	touch $@

# Remove comment lines from <libname>.remove files
$(BUILD_DIR)sdklib/%.remove: $(ROOT)lib/%.remove | $(BUILD_DIR)sdklib
	$(Q) grep -v "^#" $< | cat > $@

# Stage 1: remove unwanted object files listed in <libname>.remove alongside each library
$(BUILD_DIR)sdklib/%_stage1.a: $(ROOT)lib/%.a $(BUILD_DIR)sdklib/%.remove | $(BUILD_DIR)sdklib
	@echo "SDK processing stage 1: Removing unwanted objects from $<"
	$(Q) cat $< > $@
	$(Q) $(AR) d $@ @$(word 2,$^)

# Stage 2: Redefine all SDK symbols as sdk_, weaken all symbols.
$(BUILD_DIR)sdklib/%.a: $(BUILD_DIR)sdklib/%_stage1.a $(ROOT)lib/allsymbols.rename
	@echo "SDK processing stage 2: Renaming symbols in SDK library $< -> $@"
	$(Q) $(OBJCOPY) --redefine-syms $(word 2,$^) --weaken $< $@

# include "dummy component" for the 'program' object files, defined in the Makefile
PROGRAM_SRC_DIR ?= $(PROGRAM_DIR)
PROGRAM_ROOT ?= $(PROGRAM_DIR)
PROGRAM_MAKEFILE = $(firstword $(MAKEFILE_LIST))
$(eval $(call component_compile_rules,PROGRAM))

## Include other components (this is where the actual compiler sections are generated)
##
## if component directory exists relative to $(ROOT), use that.
## otherwise try to resolve it as an absolute path
$(foreach component,$(COMPONENTS), 					\
	$(if $(wildcard $(ROOT)$(component)),				\
		$(eval include $(ROOT)$(component)/component.mk), 	\
		$(eval include $(component)/component.mk)		\
	)								\
)

# final linking step to produce .elf
$(PROGRAM_OUT): $(WHOLE_ARCHIVES) $(COMPONENT_ARS) $(BUILD_DIR)libgcc.a $(BUILD_DIR)libc.a $(SDK_PROCESSED_LIBS) $(LINKER_SCRIPTS)
	$(vecho) "LD $@"
	$(Q) $(LD) $(LDFLAGS) -Wl,--whole-archive $(WHOLE_ARCHIVES) -Wl,--no-whole-archive -Wl,--start-group $(COMPONENT_ARS) $(BUILD_DIR)libgcc.a $(BUILD_DIR)libc.a $(LIB_ARGS) $(SDK_LIB_ARGS) -Wl,--end-group -o $@

$(BUILD_DIR) $(FIRMWARE_DIR) $(BUILD_DIR)sdklib:
	$(Q) mkdir -p $@

$(FW_FILE_1) $(FW_FILE_2): $(PROGRAM_OUT) $(FIRMWARE_DIR)
	$(vecho) "FW $@"
	$(Q) $(ESPTOOL) elf2image $(ESPTOOL_ARGS) $< -o $(FIRMWARE_DIR)

$(FW_FILE): $(PROGRAM_OUT) $(FIRMWARE_DIR)
	$(vecho) "FW $@"
	$(Q) $(ESPTOOL) elf2image --version=2 $(ESPTOOL_ARGS) $< -o $(FW_FILE)

flash: all
	$(if will_flash, $(call will_flash, "flash"))
	$(ESPTOOL) -p $(ESPPORT) --baud $(ESPBAUD) write_flash $(ESPTOOL_ARGS) \
		$(RBOOT_ARGS) 0x2000 $(FW_FILE) $(SPIFFS_ESPTOOL_ARGS)
	$(if did_flash, $(call did_flash, "flash"))

erase_flash:
	$(if will_flash, $(call will_flash, "erase"))
	$(ESPTOOL) -p $(ESPPORT) --baud $(ESPBAUD) erase_flash
	$(if did_flash, $(call did_flash, "erase"))

size: $(PROGRAM_OUT)
	$(Q) $(CROSS)size --format=sysv $(PROGRAM_OUT)

test: flash
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)

# the rebuild target is written like this so it can be run in a parallel build
# environment without causing weird side effects
rebuild:
	$(MAKE) clean
	$(MAKE) all

clean:
	$(Q) rm -rf $(BUILD_DIR)
	$(Q) rm -rf $(FIRMWARE_DIR)

# prevent "intermediate" files from being deleted
.SECONDARY:

# print some useful help stuff
help:
	@echo "esp-open-rtos make"
	@echo ""
	@echo "Other targets:"
	@echo ""
	@echo "all"
	@echo "Default target. Will build firmware including any changed source files."
	@echo
	@echo "clean"
	@echo "Delete all build output."
	@echo ""
	@echo "rebuild"
	@echo "Build everything fresh from scratch."
	@echo ""
	@echo "flash"
	@echo "Build then upload firmware to MCU. Set ESPPORT & ESPBAUD to override port/baud rate."
	@echo ""
	@echo "test"
	@echo "'flash', then start a GNU Screen session on the same serial port to see serial output."
	@echo ""
	@echo "size"
	@echo "Build, then print a summary of built firmware size."
	@echo ""
	@echo "TIPS:"
	@echo "* You can use -jN for parallel builds. Much faster! Use 'make rebuild' instead of 'make clean all' for parallel builds."
	@echo "* You can create a local.mk file to create local overrides of variables like ESPPORT & ESPBAUD."
	@echo ""
	@echo "SAMPLE COMMAND LINE:"
	@echo "make -j2 test ESPPORT=/dev/ttyUSB0"
	@echo ""


