# Parameters for the esp-open-rtos make process
#
# You can edit this file to change parameters, but a better option is
# to create a local.mk file and add overrides there. The local.mk file
# can be in the root directory, or the program directory alongside the
# Makefile, or both.
#
-include $(ROOT)local.mk
-include local.mk

# Flash size in megabits
# Valid values are same as for esptool.py - 2,4,8,16,32
FLASH_SIZE ?= 16

# Flash mode, valid values are same as for esptool.py - qio,qout,dio.dout
FLASH_MODE ?= qio

# Flash speed in MHz, valid values are same as for esptool.py - 80, 40, 26, 20
FLASH_SPEED ?= 40

# Output directories to store intermediate compiled files
# relative to the program directory
BUILD_DIR ?= $(PROGRAM_DIR)build/
FIRMWARE_DIR ?= $(PROGRAM_DIR)firmware/

# esptool.py from https://github.com/themadinventor/esptool
ESPTOOL ?= esptool.py
# serial port settings for esptool.py
ESPPORT ?= /dev/ttyUSB0
ESPBAUD ?= 115200

# firmware tool arguments
ESPTOOL_ARGS=-fs $(FLASH_SIZE)m -fm $(FLASH_MODE) -ff $(FLASH_SPEED)m


# set this to 0 if you don't need floating point support in printf/scanf
# this will save approx 14.5KB flash space and 448 bytes of statically allocated
# data RAM
#
# NB: Setting the value to 0 requires a recent esptool.py (Feb 2016 / commit ebf02c9)
PRINTF_SCANF_FLOAT_SUPPORT ?= 1

FLAVOR ?= release # or debug

# Compiler names, etc. assume gdb
CROSS ?= xtensa-lx106-elf-

# Path to the filteroutput.py tool
FILTEROUTPUT ?= $(ROOT)/utils/filteroutput.py

AR = $(CROSS)ar
CC = $(CROSS)gcc
CPP = $(CROSS)cpp
LD = $(CROSS)gcc
NM = $(CROSS)nm
C++ = $(CROSS)g++
SIZE = $(CROSS)size
OBJCOPY = $(CROSS)objcopy
OBJDUMP = $(CROSS)objdump

# Source components to compile and link. Each of these are subdirectories
# of the root, with a 'component.mk' file.
COMPONENTS     ?= $(EXTRA_COMPONENTS) FreeRTOS lwip core open_esplibs

# binary esp-iot-rtos SDK libraries to link. These are pre-processed prior to linking.
SDK_LIBS		?= main net80211 phy pp wpa

# open source libraries linked in
LIBS ?= hal

# set to 0 if you want to use the toolchain libc instead of esp-open-rtos newlib
OWN_LIBC ?= 1

# Note: you will need a recent esp
ENTRY_SYMBOL ?= call_user_start

# Set this to zero if you don't want individual function & data sections
# (some code may be slightly slower, linking will be slighty slower,
# but compiled code size will come down a small amount.)
SPLIT_SECTIONS ?= 1

# Set this to 1 to have all compiler warnings treated as errors (and stop the
# build).  This is recommended whenever you are working on code which will be
# submitted back to the main project, as all submitted code will be expected to
# compile without warnings to be accepted.
WARNINGS_AS_ERRORS ?= 0

# Common flags for both C & C++_
C_CXX_FLAGS ?= -Wtype-limits -Wwrite-strings -Wall -Wl,-EL -nostdlib $(EXTRA_C_CXX_FLAGS)
# Flags for C only
CFLAGS		?= $(C_CXX_FLAGS) -std=gnu99 $(EXTRA_CFLAGS)
# Flags for C++ only
CXXFLAGS	?= $(C_CXX_FLAGS) -std=c++0x -fno-exceptions -fno-rtti $(EXTRA_CXXFLAGS)

# these aren't all technically preprocesor args, but used by all 3 of C, C++, assembler
CPPFLAGS	+= -mlongcalls -mtext-section-literals

LDFLAGS		= -nostdlib -Wl,--no-check-sections -L$(BUILD_DIR)sdklib -L$(ROOT)lib -u $(ENTRY_SYMBOL) -Wl,-static -Wl,-Map=$(BUILD_DIR)$(PROGRAM).map $(EXTRA_LDFLAGS)

ifeq ($(WARNINGS_AS_ERRORS),1)
    C_CXX_FLAGS += -Werror
endif

ifeq ($(SPLIT_SECTIONS),1)
  C_CXX_FLAGS += -ffunction-sections -fdata-sections
  LDFLAGS += -Wl,-gc-sections
endif

ifeq ($(FLAVOR),debug)
    C_CXX_FLAGS += -g -O0
    LDFLAGS += -g -O0
else ifeq ($(FLAVOR),sdklike)
    # These are flags intended to produce object code as similar as possible to
    # the output of the compiler used to build the SDK libs (for comparison of
    # disassemblies when coding replacement routines).  It is not normally
    # intended to be used otherwise.
    CFLAGS += -O2 -Os -fno-inline -fno-ipa-cp -fno-toplevel-reorder -fno-caller-saves -fconserve-stack
    LDFLAGS += -O2
else
    C_CXX_FLAGS += -g -O2
    LDFLAGS += -g -O2
endif

GITSHORTREV=\"$(shell cd $(ROOT); git rev-parse --short -q HEAD 2> /dev/null)\"
ifeq ($(GITSHORTREV),\"\")
  GITSHORTREV="\"(nogit)\"" # (same length as a short git hash)
endif
CPPFLAGS += -DGITSHORTREV=$(GITSHORTREV)

LINKER_SCRIPTS += $(ROOT)ld/program.ld $(ROOT)ld/rom.ld

# rboot firmware binary paths for flashing
RBOOT_ARGS ?= 0x0 $(RBOOT_BIN) 0x1000 $(RBOOT_CONF)
RBOOT_BIN = $(ROOT)bootloader/firmware/rboot.bin
RBOOT_PREBUILT_BIN = $(ROOT)bootloader/firmware_prebuilt/rboot.bin
RBOOT_CONF = $(ROOT)bootloader/firmware_prebuilt/blank_config.bin

# if a custom bootloader hasn't been compiled, use the
# prebuilt binary from the source tree
ifeq (,$(wildcard $(RBOOT_BIN)))
RBOOT_BIN=$(RBOOT_PREBUILT_BIN)
endif
