#!/bin/bash
#
# addrsource <addr> [<filename>]
#
# Given a memory address (in hex), print (if available) the C source code line
# and disassembly associated with that address.
#
# <filename>, if provided, should be the name of the ELF (*.out) file for the
# firmware image (typically located in the "build" subdirectory).  If
# <filename> is not provided, the script will look for a "build" subdirectory
# of the current directory, and look in there for a filename matching "*.out"
# and use that.

print_usage() {
    echo "Usage: $0 addr [file.out]" >&2
}

addr="$1"
exefile="$2"

# If user didn't provide any parameters, or attempted to use a switch (such as
# "--help"), just print the usage message and exit.  If they provided an 'addr'
# parameter, but it didn't start with "0x", then prepend "0x" to it before
# continuing..
case "$addr" in
    "") print_usage; exit 1 ;;
    -*) print_usage; exit 1 ;;
    0x*) ;;
    *) addr="0x$addr" ;;
esac

if [[ -z "$exefile" ]]; then
    # User didn't specify what ELF (.out) file to look things up in.
    # See if we can find an appropriate .out file in the build subdirectory
    # (there's usually only one)
    exefile=$(ls build/*.out)
    if [[ -z "$exefile" ]] || [[ "${exefile#* }" != "$exefile" ]]; then
        # Either we couldn't find anything, or we found more than one of them.
        echo "Unable to determine which .out file to use.  You will need to specify it explicitly on the command line." >&2
        echo "" >&2
        print_usage
        exit 1
    fi
fi

# All our parameters should be correct now.
# Invoke GDB to do the hard work for us...
xtensa-lx106-elf-gdb -batch "$exefile" -ex "list *$addr" -ex "info line *$addr" -ex "disassemble $addr"
