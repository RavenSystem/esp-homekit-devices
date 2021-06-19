#!/usr/bin/env python
#
# A thin Python wrapper around addr2line, can monitor esp-open-rtos
# output and uses gdb to convert any suitable looking hex numbers
# found in the output into function and line numbers.
#
# Works with a serial port if the --port option is supplied.
# Otherwise waits for input on stdin.
#
import serial
import argparse
import re
import os
import os.path
import subprocess
import termios
import sys
import time

# Try looking up anything in the executable address space
RE_EXECADDR = r"(0x)?40([0-9]|[a-z]){6}"

def find_elf_file():
    out_files = []
    for top,_,files in os.walk('.', followlinks=False):
        for f in files:
            if f.endswith(".out"):
                out_files.append(os.path.join(top,f))
    if len(out_files) == 1:
        return out_files[0]
    elif len(out_files) > 1:
        print("Found multiple .out files: %s. Please specify one with the --elf option." % out_files)
    else:
        print("No .out file found under current directory. Please specify one with the --elf option.")
    sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description='esp-open-rtos output filter tool', prog='filteroutput')
    parser.add_argument(
        '--elf', '-e',
        help="ELF file (*.out file) to load symbols from (if not supplied, will search for one)"),
    parser.add_argument(
        '--port', '-p',
        help='Serial port to monitor (will monitor stdin if None)',
        default=None)
    parser.add_argument(
        '--baud', '-b',
        help='Baud rate for serial port',
        type=int,
        default=74880)
    parser.add_argument(
        '--reset-on-connect', '-r',
        help='Reset ESP8266 (via DTR) on serial connect. (Linux resets even if not set, except when using NodeMCU-style auto-reset circuit.)',
        action='store_true')

    args = parser.parse_args()

    if args.elf is None:
        args.elf = find_elf_file()
    elif not os.path.exists(args.elf):
        print("ELF file '%s' not found" % args.elf)
        sys.exit(1)

    if args.port is not None:
        print("Opening %s at %dbps..." % (args.port, args.baud))
        port = serial.Serial(args.port, baudrate=args.baud)
        if args.reset_on_connect:
            print("Resetting...")
            port.setDTR(False)
            time.sleep(0.1)
            port.setDTR(True)

    else:
        print("Reading from stdin...")
        port = sys.stdin
        # disable echo
	try:
            old_attr = termios.tcgetattr(sys.stdin.fileno())
	    attr = termios.tcgetattr(sys.stdin.fileno())
	    attr[3] = attr[3] & ~termios.ECHO
	    termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, attr)
	except termios.error:
	     pass

    try:
        while True:
            line = port.readline()
            if line == '':
                break
            print(line.strip())
            for match in re.finditer(RE_EXECADDR, line, re.IGNORECASE):
                addr = match.group(0)
                if not addr.startswith("0x"):
                    addr = "0x"+addr
                    # keeping addr2line and feeding it addresses on stdin didn't seem to work smoothly
                addr2line = subprocess.check_output(["xtensa-lx106-elf-addr2line","-pfia","-e","%s" % args.elf, addr], cwd=".").strip()
                if not addr2line.endswith(": ?? ??:0"):
                    print("\n%s\n" % addr2line.strip())
    finally:
        if args.port is None:
            # restore echo
            termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, old_attr)

if __name__ == "__main__":
    main()
