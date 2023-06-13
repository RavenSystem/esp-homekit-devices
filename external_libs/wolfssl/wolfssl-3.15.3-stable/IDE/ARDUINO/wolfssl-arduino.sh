#!/bin/sh

# this script will reformat the wolfSSL source code to be compatible with
# an Arduino project
# run as bash ./wolfssl-arduino.sh

DIR=${PWD##*/}

if [ "$DIR" = "ARDUINO" ]; then
	rm -rf wolfSSL
	mkdir wolfSSL

    cp ../../src/*.c ./wolfSSL
    cp ../../wolfcrypt/src/*.c ./wolfSSL

    mkdir wolfSSL/wolfssl
    cp ../../wolfssl/*.h ./wolfSSL/wolfssl
    mkdir wolfSSL/wolfssl/wolfcrypt
    cp ../../wolfssl/wolfcrypt/*.h ./wolfSSL/wolfssl/wolfcrypt

    # support misc.c as include in wolfcrypt/src
    mkdir ./wolfSSL/wolfcrypt
    mkdir ./wolfSSL/wolfcrypt/src
    cp ../../wolfcrypt/src/misc.c ./wolfSSL/wolfcrypt/src

    # put bio and evp as includes
    mv ./wolfSSL/bio.c ./wolfSSL/wolfssl
	mv ./wolfSSL/evp.c ./wolfSSL/wolfssl

    echo "/* Generated wolfSSL header file for Arduino */" >> ./wolfSSL/wolfssl.h
    echo "#include <wolfssl/wolfcrypt/settings.h>" >> ./wolfSSL/wolfssl.h
    echo "#include <wolfssl/ssl.h>" >> ./wolfSSL/wolfssl.h
else
    echo "ERROR: You must be in the IDE/ARDUINO directory to run this script"
fi
