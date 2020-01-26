PROGRAM = otaboot

### select EITHER the top block OR the bottom block

#PROGRAM = otamain
#LINKER_SCRIPTS = $(ROOT)ld/program1.ld

#==================================================
# for this to work, we need to copy $(ROOT)ld/program.ld
# to $(ROOT)ld/program1.ld and in the copy change this:
# irom0_0_seg :                       	org = 0x40202010, len = (1M - 0x2010)
# to this:
# irom0_0_seg :                       	org = 0x4028D010, len = (0xf5000 - 0x2010)
# 
# note that the previous len is forgetting about the system settings area which
# is 9 sectors for esp-open-rtos, and then it is ignored as well...
#
# also we need to change paramters.mk like this:
# LINKER_SCRIPTS += $(ROOT)ld/program.ld $(ROOT)ld/rom.ld
# becomes:
# LINKER_SCRIPTS ?= $(ROOT)ld/program.ld
# LINKER_SCRIPTS += $(ROOT)ld/rom.ld
#==
# if you know a more elegant way, I am all open for it ;-)
