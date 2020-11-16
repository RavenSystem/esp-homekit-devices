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
#ifndef HAA_LIGHTBULD_WSS2812_DRIVER_H
#define HAA_LIGHTBULD_WSS2812_DRIVER_H
#include "haa_lightbulb_driver.h"

typedef struct 
{
    uint8_t model;
    uint8_t pin;
    uint8_t numPixels;
    uint8_t numChannelsPerPixel;
    float converationFactor;
} ws2812_driver_info_t;

void haa_ws2812_init(driver_interface_t** interface, const cJSON * const object, lightbulb_channels_t* channels);
void haa_ws2812_send(const void* driver_info, const uint16_t* multipwm_duty);

#endif