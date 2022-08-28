#!/bin/bash
set -euv

# Called by Travis to install a toolchain using esp-open-sdk (parts we
# ne for esp-open-rtos are the GNU toolchain, libhal, and esptool.py.)

if test -d ${CROSS_BINDIR}; then
	echo "Using cached toolchain in ${CROSS_BINDIR}"
	exit 0
fi

# Travis sets this due to "language: c", but it confuses autotools configure when cross-building
unset CC

git clone --recursive https://github.com/pfalcon/esp-open-sdk.git
cd esp-open-sdk
git reset --hard ${OPENSDK_COMMIT}
git submodule update --init

# this is a nasty hack as Ubuntu Precise only has autoconf 2.68 not 2.69...
sed -i "s/2.69/2.68/" lx106-hal/configure.ac

# build the toolchain relative to the CROSS_ROOT directory
sed -r -i 's%TOOLCHAIN ?=.*%TOOLCHAIN=${CROSS_ROOT}%' Makefile

# will dump log on failure
echo "Building toolchain without live progress, as progress spinner fills up log..."

if !( make toolchain esptool libhal STANDALONE=n 2>&1 > make.log ); then
	cat make.log
	echo "Exiting due to failed toolchain build"
	exit 3
fi

echo "Toolchain build completed in ${CROSS_ROOT}."
