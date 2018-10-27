#!/bin/bash
openssl sha384 -binary -out firmware/Sonoff_RavenCore.bin.sig firmware/Sonoff_RavenCore.bin
printf "%08x" `cat firmware/Sonoff_RavenCore.bin | wc -c` | xxd -r -p >> firmware/Sonoff_RavenCore.bin.sig
mv -v firmware/Sonoff_RavenCore.bin firmware/sonoff_ravencore.bin
mv -v firmware/Sonoff_RavenCore.bin.sig firmware/sonoff_ravencore.bin.sig
