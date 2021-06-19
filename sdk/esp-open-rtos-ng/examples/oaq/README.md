# Our Air Quality logger for the ESP8266 running the [ESP Open RTOS](https://github.com/SuperHouse/esp-open-rtos)

A community developed open source air quality data logger.

Currently supporting the [Plantower](http://plantower.com/) PMS1003, PMS3003, PMS5003, and PMS7003 particle counters.

Supports a few temperature, relative humidity, and pressure sensors on the same I2C. These are auto-detected and used if available. This data might be important for qualifying if the air conditions support reliable PM measurement.

* BME280 temperature, relative humidity and air pressure sensor.

* SHT2x temperature and relative humidity sensors. 

* BMP180 temperature and air pressure sensor.

The DS3231 real-time-clock is also supported on the I2C bus and used if available. The time is logged periodically and synchronsized to the time that the http server sends in its responses. This can be very helpful when logging data away from Wifi and Internet access so that measurments are correctly time-stamped.


## Building

Follow the build instructions for esp-open-rtos, and when the examples are running copy this code into `examples/oaq/`.

This code supports a 4MiB SPI flash, so the ESP12, and the Nodemcu and Witty boards have been tested. For this flash it was necessary to edit `parameters.mk` to change `FLASH_SIZE ?= 32` and `FLASH_MODE ?= dio`.

The following will build the code and flash a device on Linux.

`make flash -j4 -C examples/oaq ESPPORT=/dev/ttyUSB0`


## Features

* Logs the data to a memory buffer and then to the SPI flash storage, using 3MiB of the 4MiB for data storage. This allows the data logger to be used out of reach of Wifi and the Internet for a period.

* All the data from the Plantower sensor is logged, each sample at 0.8 second intervals, and all the data in the samples which includes the PM1.0, PM2.5, and PM10 values plus the particle counts, and even the checksum for each sample. This might be useful for local sources of pollution that can cause quick changes in air quality, and might be useful for post-analysis such as noise reduction.

* The data is compressed to fit more data into the flash and this also reduces wear on the flash and perhaps power usage. The particle count distributions are converted to differential values reducing their magnitude, and after delta encoding they are encoding using a custom variable bit length code. There are special events for the case of no change in the values in which case only a time delta is encoded. This typically compresses the data to 33% to 15% of the original size.

* The compressed data is stored in flash sectors, and each sector stands on its own and can be uncompressed on its own. An attempt is made to handle bad sectors, in which case the data is written to the next good sector. Each valid sector is assigned a monotonically increasing 32-bit index. The sectors are organized as a ring-buffer, so when full the oldest is overwritten. The sectors are buffered in memory before writing to reduce the number of writes and the current data is periodically flushed to the flash storage to avoid too much data loss if power is lost. ESP flash tools can read these sectors for downloading the data without Wifi.

* The compressed sectors are HTTP-POSTed to a server. The current head sector is periodically posted to the server too to keep it updated and only the new data is posted. The server response can request re-sending of sectors still stored on the device to handle data loss at the server. The server can not affected the data stored on the device or the logging of the data to flash as a safety measure.

* The ESP8266 Real-Time-Clock (RTC) counter is logged with every event. The server response includes the real time and response events are logged allowing estimation of the real time of events in post-analysis. This can be be supported by the optional DS3231 real-time-clock. Support for logging a button press will be added to allow people to synchronize logging and events times manually.

* The data posted to the server is signed using the MAC-SHA3 algorithm ensuring integrity of the data and preventing forgery of data posted to the server.


## Connection

Two ESP8266 boards have been tested.

### Nodemcu

UART pins for the PMSx003:

| PCB name | Function | Cable color |
| -------- | -------- | ----------- |
| VIN | +5V | Purple |
| GND | 0V | Orange |
| D7 | RX | Green |
| D8 | TX | Blue |

Note: the Lolin V3 Nodemcu board VIN pin is not usable as a +5V output, but it has +5V output on it's VU pin which is a reserved pin on other Nodemcu boards.

Note: the TX line is to be not connect at present as the device sends verbose debug output on the serial line that might affect the sensor.

I2C pins:

| PCB name | Function |
| -------- | -------- |
| D1 | SCL |
| D2 | SDA |
| 3V3 | +V |
| G | Ground |

### Witty

UART pins for the PMSx003:

| PCB name | Function | Cable color |
| -------- | -------- | ----------- |
| VIN | +5V | Purple |
| GND | 0V | Orange |
| RX | RX | Green |
| TX | TX | Blue |

I2C pins:

| PCB name | Function |
| -------- | -------- |
| GPIO5 | SCL |
| GPIO4 | SDA |
| VCC | +V |
| G | Ground |


## Configuration

The configuration is stored in the esp-free-rtos sysparam database. See the esp-free-rtos sysparam example for an editor which can be used to set these for now. The database uses four more sectors at the end of the flash.

* `board` - single binary byte that can be 0 for Nodemcu, and 1 for Witty. It is only used to blink some LEDs at present.

* `pms_uart` - single binary byte that gives the PMS*003 serial port: 0 - None, disabled (default); 1 - UART0 on GPIO3 aka RX (Nodemcu pin D9); 2 - UART0 swapped pins mode, GPIO13 (Nodemcu pin D7); 3 - TODO Flipping between the above for two sensors?

* `i2c_scl`, `i2c_sda` - single binary bytes giving the I2C bus pin definitions, GPIO numbers. SCL defaults to GPIO 0 (Nodemcu pin D3) and SDA to GPIO 2 (Nodemcu pin D4) if not supplied.

The follow are network parameters. If not sufficiently initialized to communicate with a server then Wifi is disabled and the post-data task is not created, but the data will still be logged to the internal Flash storage and can be downloaded to a PC.

* `web_server` - a string, e.g. 'ourairquality.org', '192.168.1.1'

* `web_port` - a string, e.g. '80'

* `web_path` - a string, e.g. '/cgi-bin/recv'

* `sensor_id` - a binary 32 bit number.

* `key_size` - a binary 32 bit number.

* `sha3_key` - a binary blob, with `key_size` bytes.

* `wifi_ssid` - a string, the Wifi station ID.

* `wifi_pass` - a string, the respective Wifi password.


## TODO

This is at the proof of concept stage.

The URL to upload the data to and the key is hard coded, see `post.c`, and this needs to be configurable.

TODO a web client app to create the sysparam sectors.

The server side code for logging the data to files has been prototyped, and is CGI code written in a few pages of C code and tested on Apache and expected to work on economical cPanel shared hosting. The client front end is stil TODO and is just some hack scripts for now.
