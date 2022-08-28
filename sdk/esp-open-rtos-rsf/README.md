# esp-open-rtos

A community developed open source [FreeRTOS](http://www.freertos.org/)-based framework for ESP8266 WiFi-enabled microcontrollers. Intended for use in both commercial and open source projects.

Originally based on, but substantially different from, the [Espressif IOT RTOS SDK](https://github.com/espressif/ESP8266_RTOS_SDK).

## Resources

[![Build Status](https://travis-ci.org/SuperHouse/esp-open-rtos.svg?branch=master)](https://travis-ci.org/SuperHouse/esp-open-rtos)

Email discussion list: https://groups.google.com/d/forum/esp-open-rtos

IRC channel: #esp-open-rtos on Freenode ([Web Chat Link](http://webchat.freenode.net/?channels=%23esp-open-rtos&uio=d4)).

Github issues list/bugtracker: https://github.com/superhouse/esp-open-rtos/issues

Please note that this project is released with a [Contributor Code of Conduct](https://github.com/SuperHouse/esp-open-rtos/blob/master/code_of_conduct.md). By participating in this project you agree to abide by its terms.

## Quick Start

* Install [esp-open-sdk](https://github.com/pfalcon/esp-open-sdk/), build it with `make toolchain esptool libhal STANDALONE=n`, then edit your PATH and add the generated toolchain `bin` directory. The path will be something like `/path/to/esp-open-sdk/xtensa-lx106-elf/bin`. (Despite the similar name esp-open-sdk has different maintainers - but we think it's fantastic!)

    (Other toolchains may also work, as long as a gcc cross-compiler is available on the PATH and libhal (and libhal headers) are compiled and available to gcc. The proprietary Tensilica "xcc" compiler will probably not work.)

* Install [esptool.py](https://github.com/themadinventor/esptool) and make it available on your PATH. If you used esp-open-sdk then this is done already.

* The esp-open-rtos build process uses `GNU Make`, and the utilities `sed` and `grep`. If you built esp-open-sdk then you have these already.

* Use git to clone the esp-open-rtos project (note the `--recursive`):

```
git clone --recursive https://github.com/Superhouse/esp-open-rtos.git
cd esp-open-rtos
```

* To build any examples that use WiFi, create `include/private_ssid_config.h` defining the two macro defines:

```c
#define WIFI_SSID "mywifissid"
#define WIFI_PASS "my secret password"
```

* Build an example project (found in the 'examples' directory) and flash it to a serial port:

```
make flash -j4 -C examples/http_get ESPPORT=/dev/ttyUSB0
```

Run `make help -C examples/http_get` for a summary of other Make targets.

(Note: the `-C` option to make is the same as changing to that directory, then running make.)

The [Build Process wiki page](https://github.com/SuperHouse/esp-open-rtos/wiki/Build-Process) has in-depth details of the build process.

## Goals

* Provide professional-quality framework for WiFi-enabled RTOS projects on ESP8266.
* Open source code for all layers above the MAC layer, ideally lower layers if possible (this is a work in progress, see [Issues list](https://github.com/superhouse/esp-open-rtos/issues).
* Leave upstream source clean, for easy interaction with upstream projects.
* Flexible build and compilation settings.

Current status is alpha quality, actively developed. AP STATION mode (ie wifi client mode) and UDP/TCP client modes are tested. Other functionality should work. Contributors and testers are welcome!

## Code Structure

* `examples` contains a range of example projects (one per subdirectory). Check them out!
* `include` contains header files from Espressif RTOS SDK, relating to the binary libraries & Xtensa core.
* `core` contains source & headers for low-level ESP8266 functions & peripherals. `core/include/esp` contains useful headers for peripheral access, etc. Minimal to no FreeRTOS dependencies.
* `extras` is a directory that contains optional components that can be added to your project. Most 'extras' components will have a corresponding example in the `examples` directory. Extras include:
   - mbedtls - [mbedTLS](https://tls.mbed.org/) is a TLS/SSL library providing up to date secure connectivity and encryption support.
   - i2c - software i2c driver ([upstream project](https://github.com/kanflo/esp-open-rtos-driver-i2c))
   - rboot-ota - OTA support (over-the-air updates) including a TFTP server for receiving updates ([for rboot by @raburton](http://richard.burtons.org/2015/05/18/rboot-a-new-boot-loader-for-esp8266/))
   - bmp180 driver for digital pressure sensor ([upstream project](https://github.com/Angus71/esp-open-rtos-driver-bmp180))
* `FreeRTOS` contains FreeRTOS implementation, subdirectory structure is the standard FreeRTOS structure. `FreeRTOS/source/portable/esp8266/` contains the ESP8266 port.
* `lwip` contains the lwIP TCP/IP library. See [Third Party Libraries](https://github.com/SuperHouse/esp-open-rtos/wiki/Third-Party-Libraries) wiki page for details.
* `libc` contains the newlib libc. [Libc details here](https://github.com/SuperHouse/esp-open-rtos/wiki/libc-configuration).

## Open Source Components

* [FreeRTOS](http://www.freertos.org/) V10.2.0
* [lwIP](http://lwip.wikia.com/wiki/LwIP_Wiki) v2.0.3, with [some modifications](https://github.com/ourairquality/lwip/).
* [newlib](https://github.com/ourairquality/newlib) v3.0.0, with patches for xtensa support and locking stubs for thread-safe operation on FreeRTOS.

For details of how third party libraries are integrated, [see the wiki page](https://github.com/SuperHouse/esp-open-rtos/wiki/Third-Party-Libraries).

## Binary Components

Binary libraries (inside the `lib` dir) are all supplied by Espressif as part of their RTOS SDK. These parts were MIT Licensed.

As part of the esp-open-rtos build process, all binary SDK symbols are prefixed with `sdk_`. This makes it easier to differentiate binary & open source code, and also prevents namespace conflicts.

Espressif's RTOS SDK provided a "libssl" based on axTLS. This has been replaced with the more up to date mbedTLS library (see below).

Some binary libraries appear to contain unattributed open source code:

* libnet80211.a & libwpa.a appear to be based on FreeBSD net80211/wpa, or forks of them. ([See this issue](https://github.com/SuperHouse/esp-open-rtos/issues/4)).
* libudhcp has been removed from esp-open-rtos. It was released with the Espressif RTOS SDK but udhcp is GPL licensed.

## Licensing

* BSD license (as described in LICENSE) applies to original source files, [lwIP](http://lwip.wikia.com/wiki/LwIP_Wiki). lwIP is Copyright (C) Swedish Institute of Computer Science.

* FreeRTOS (since v10) is provided under the MIT license. License details in files under FreeRTOS dir. FreeRTOS is Copyright (C) Amazon.

* Source & binary components from the [Espressif IOT RTOS SDK](https://github.com/espressif/esp_iot_rtos_sdk) were released under the MIT license. Source code components are relicensed here under the BSD license. The original parts are Copyright (C) Espressif Systems.

* Newlib is covered by several copyrights and licenses, as per the files in the `libc` directory.

* [mbedTLS](https://tls.mbed.org/) is provided under the Apache 2.0 license as described in the file extras/mbedtls/mbedtls/apache-2.0.txt. mbedTLS is Copyright (C) ARM Limited.

Components under `extras/` may contain different licenses, please see those directories for details.

## Contributions

Contributions are very welcome!

* If you find a bug, [please raise an issue to report it](https://github.com/superhouse/esp-open-rtos/issues).

* If you have feature additions or bug fixes then please send a pull request.

* There is a list of outstanding 'enhancements' in the [issues list](https://github.com/superhouse/esp-open-rtos/issues). Contributions to these, as well as other improvements, are very welcome.

If you are contributing code, *please ensure that it can be licensed under the BSD open source license*. Specifically:

* Code from Espressif IoT SDK cannot be merged, as it is provided under either the "Espressif General Public License" or the "Espressif MIT License", which are not compatible with the BSD license.

* Recent releases of the Espressif IoT RTOS SDK cannot be merged, as they changed from MIT License to the "Espressif MIT License" which is not BSD compatible. The Espressif binaries used in esp-open-rtos were taken from [revision ec75c85, as this was the last MIT Licensed revision](https://github.com/espressif/ESP8266_RTOS_SDK/commit/43585fa74550054076bdf4bfe185e808ad0da83e).

For code submissions based on reverse engineered binary functionality, please either reverse engineer functionality from MIT Licensed Espressif releases or make sure that the reverse engineered code does not directly copy the code structure of the binaries - it cannot be a "derivative work" of an incompatible binary.

The best way to write suitable code is to first add documentation somewhere like the [esp8266 reverse engineering wiki](http://esp8266-re.foogod.com/) describing factual information gained from reverse engineering - such as register addresses, bit masks, orders of register writes, etc. Then write new functions referring to that documentation as reference material.

## Coding Style

For new contributions in C, please use BSD style and indent using 4 spaces.

For assembly, please use the following:
* Instructions indented using 8 spaces.
* Inline comments use `#` as a comment delimiter.
* Comments on their own line(s) use `/*`..`*/`.
* First operand of each instruction should be vertically aligned where possible.
* For xtensa special registers, prefer `wsr aX, SR` over `wsr.SR aX`

If you're an emacs user then there is a .dir-locals.el file in the root which configures cc-mode and asm-mode (you will need to approve some variable values as safe). See also
the additional comments in .dir-locals.el, if you're editing assembly code.

Upstream code is left with the indentation and style of the upstream project.

## Sponsors

Work on parts of esp-open-rtos has been sponsored by [SuperHouse Automation](http://superhouse.tv/).
