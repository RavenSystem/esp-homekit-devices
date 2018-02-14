#!/bin/sh

# this script will reformat the wolfSSL source code to be compatible with
# an Arduino project
# run as bash ./wolfssl-arduino.sh

DIR=${PWD##*/}

if [ "$DIR" = "ARDUINO" ]; then
    cp ../../src/*.c ../../
    cp ../../wolfcrypt/src/*.c ../../
    echo "/* stub header file for Arduino compatibility */" >> ../../wolfssl.h
else
    echo "ERROR: You must be in the IDE/ARDUINO directory to run this script"
fi

#UPDATED: 19 Apr 2017 to remove bio.c and evp.c from the root directory since
#         they are included inline and should not be compiled directly

ARDUINO_DIR=${PWD}
cd ../../
rm bio.c
rm evp.c
cd $ARDUINO_DIR
# end script in the origin directory for any future functionality that may be added.
#End UPDATE: 19 Apr 2017
