# esp-open-rtos tests

Testing is based on [Unity](https://github.com/ThrowTheSwitch/Unity)
C testing framework.

## Features

* Single device test case.
* Dual devices test cases. Run test case on two ESP8266 modules simultaneously.
* Run only specified test cases.
* List available test cases on a device.

## Usage

There's a test runner script `test_runner.py` written in Python3 that runs
test cases on ESP8266 connected to a host.

### Requirements and dependencies

* Python3 version > 3.4 `sudo apt-get install python3 python3-pip`
* pyserial `sudo pip3 install pyserial`
* ESP8266 board with reset to boot mode support
* Two ESP8266 for dual mode test cases

Test runner is heavily relying on device reset using DTR and RTS signals.
Popular ESP8266 boards such as **NodeMcu** and **Wemos D1** support device
reset into flash mode.

### Options

`--type` or `-t` - Type of test case to run. Can be 'solo' or 'dual'.
If not specified 'solo' test will be run.

`--aport` or `-a` - Serial port for device A.
If not specified device `/dev/ttyUSB0` is used.

`--bport` or `-b` - Serial port for device B.
If not specified device `/dev/ttyUSB1` is used.

`--no-flash` or `-n` - Do not flash the test firmware before running tests.

`--list` or `-l` - Display list of the available test cases on the device.

### Example

Build test firmware, flash it using serial device `/dev/tty.wchusbserial1410`
and run only *solo* test cases:

`./test_runner.py -a /dev/tty.wchusbserial1410`

Build test firmware. Flash both devices as `-t dual` is specified. And run both
*solo* and *dual* test cases:

`./test_runner.py -a /dev/tty.wchusbserial1410 -b /dev/tty.wchusbserial1420 -t dual`

Do not flash the firmware, only display available test cases on the device:

`./test_runner.py -a /dev/tty.wchusbserial1410 -n -l`

Do not flash the firmware and run only 2 and 4 test cases:

`./test_runner.py -a /dev/tty.wchusbserial1410 -n 2 4`

## References

[Unity](https://github.com/ThrowTheSwitch/Unity) - Simple Unit Testing for C
