#!/bin/bash
openssl sha384 -binary -out firmware/main.bin.sig firmware/main.bin
printf "%08x" `cat firmware/main.bin | wc -c` | xxd -r -p >> firmware/main.bin.sig
