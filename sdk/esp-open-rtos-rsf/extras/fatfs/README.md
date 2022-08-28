# FatFs - Generic FAT File System Module

Current version: R0.13a

## How to use

Connect your SD card to ESP module

 SD pin |  ESP8266 
--------|------------
1. DAT2 |  -
2. /CS  | Any accessible GPIO (15, 5, 4, 0, 2, 16)
3. DI   | HMOSI (GPIO13)
4. VDD  | +3V3
5. CLK  | HCLK (GPIO14)
6. VSS  | GND
7. DO   | HMISO (GPIO12)
8. RSV  | - 

Add `extras/sdio` and `extras/fatfs` to `EXTRA_COMPONENTS` parameter of your
makefile, e.g.

```Makefile
EXTRA_COMPONENTS = extras/sdio extras/fatfs
```

Use `const char *f_gpio_to_volume(uint8_t gpio)` to get the FatFs volume ID
based on GPIO which is used for CS pin.

## FatFs configuration

Almost all of the FatFs parameters are configurable in the Makefile of your
project. See default values and their meaning in `defaults.mk`.

## Original documentation

http://elm-chan.org/fsw/ff/00index_e.html

## License

Copyright (C) 20xx, ChaN, all right reserved.

FatFs module is an open source software. Redistribution and use of FatFs in
source and binary forms, with or without modification, are permitted provided
that the following condition is met:

1. Redistributions of source code must retain the above copyright notice,
   this condition and the following disclaimer.

This software is provided by the copyright holder and contributors "AS IS"
and any warranties related to this software are DISCLAIMED.
The copyright owner or contributors be NOT LIABLE for any damages caused
by use of this software.


