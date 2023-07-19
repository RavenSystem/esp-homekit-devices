#!/bin/sh

#sniffer-testsuite.test

echo -e "\nStaring snifftest on testsuite.pcap...\n"
./sslSniffer/sslSnifferTest/snifftest ./scripts/testsuite.pcap ./certs/server-key.pem 127.0.0.1 11111

RESULT=$?
[ $RESULT -ne 0 ] && echo -e "\nsnifftest failed\n" && exit 1

echo -e "\nSuccess!\n"

exit 0
