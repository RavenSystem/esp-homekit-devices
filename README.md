# esp-homekit-devices

Works to bring Apple HomeKit to several devices based on an ESP chip.

This project is based on the work of [Maxim Kulkin](https://github.com/maximkulkin) [esp-homekit-demo](https://github.com/maximkulkin/esp-homekit-demo)

## Usage

1. After cloning repo, initialize and sync all submodules (recursively):
```shell
git submodule update --init --recursive
```
2. Install [esp-open-sdk](https://github.com/pfalcon/esp-open-sdk), build it with `make toolchain esptool libhal STANDALONE=n`, then edit your PATH and add the generated toolchain bin directory. The path will be something like /path/to/esp-open-sdk/xtensa-lx106-elf/bin. (Despite the similar name esp-open-sdk has different maintainers - but we think it's fantastic!)

3. Install [esptool.py](https://github.com/themadinventor/esptool) and make it available on your PATH. If you used esp-open-sdk then this is done already.
4. Build example:
```shell
make -C devices/Sonoff_Basic all
```
5. Set ESPPORT environment variable pointing to USB device your ESP8266 is attached to (assuming your device is at /dev/ttyUSB0):
```shell
export ESPPORT=/dev/ttyUSB0
```
6. Upload firmware to ESP:
```shell
make -C devices/Sonoff_Basic test
```
or
```shell
make -C devices/Sonoff_Basic flash
make -C devices/Sonoff_Basic monitor
```
