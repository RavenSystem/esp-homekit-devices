/**
MIT License

Copyright (c) 2020 Alexander Lougovski

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef HAA_LIGHTBULD_DRIVER_H
#define HAA_LIGHTBULD_DRIVER_H

#include <stdint.h>
typedef enum _driver_family {
        DRIVER_PWM = 0X00,
        DRIVER_MY92XX = 0X01,
        DRIVER_WS2812 = 0X02,
} driver_family_t;

typedef void (*haa_driver_updater_callback_t)(const void* driver_info, const uint16_t* multipwm_duty);

typedef struct 
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
    uint8_t cw;
    uint8_t ww;
} lightbulb_channels_t;

typedef struct _driver_interface
{
    driver_family_t family;
    uint8_t numChips;
    uint8_t numChannelsPerChip;
    void* driver_info;
    haa_driver_updater_callback_t updater;
} driver_interface_t;

#endif
