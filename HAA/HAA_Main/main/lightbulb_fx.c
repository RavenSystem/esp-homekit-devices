/*
 * Lightbulb FX for Home Accessory Architect
 *
 * Copyright 2025-2026 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

/*
  Based on https://github.com/kitesurfer1404/WS2812FX
  
  LICENSE

  The MIT License (MIT)

  Copyright (c) 2016  Harm Aldick

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM

#include "freertos/FreeRTOS.h"
#include "esp_attr.h"
#include "esp_random.h"
#define hwrand()    esp_random()

#else

#include <esplibs/libmain.h>

#endif

#include "lightbulb_fx.h"

lightbulb_fx_data_t* current_lightbulb_fx_data = NULL;

// some common colors
#define RED        (uint32_t)0xFF0000
#define GREEN      (uint32_t)0x00FF00
#define BLUE       (uint32_t)0x0000FF
#define WHITE      (uint32_t)0xFFFFFF
#define BLACK      (uint32_t)0x000000
#define YELLOW     (uint32_t)0xFFFF00
#define CYAN       (uint32_t)0x00FFFF
#define MAGENTA    (uint32_t)0xFF00FF
#define PURPLE     (uint32_t)0x400080
#define ORANGE     (uint32_t)0xFF3000
#define PINK       (uint32_t)0xFF1493
#define GRAY       (uint32_t)0x101010
#define ULTRAWHITE (uint32_t)0xFFFFFFFF
#define DIM(c)     (uint32_t)((c >> 2) & 0x3f3f3f3f) // color at 25% intensity
#define DARK(c)    (uint32_t)((c >> 4) & 0x0f0f0f0f) // color at  6% intensity

// segment options
// bit    7: reverse animation
// bits 4-6: fade rate (0-7)
// bit    3: gamma correction
// bits 1-2: size
// bits   0: TBD
#define NO_OPTIONS      ((uint8_t) 0b00000000)
#define REVERSE         ((uint8_t) 0b10000000)
#define IS_REVERSE      ((current_lightbulb_fx_data->options & REVERSE) == REVERSE)
#define FADE_XFAST      ((uint8_t) 0b00010000)
#define FADE_FAST       ((uint8_t) 0b00100000)
#define FADE_MEDIUM     ((uint8_t) 0b00110000)
#define FADE_SLOW       ((uint8_t) 0b01000000)
#define FADE_XSLOW      ((uint8_t) 0b01010000)
#define FADE_XXSLOW     ((uint8_t) 0b01100000)
#define FADE_GLACIAL    ((uint8_t) 0b01110000)
#define FADE_RATE       ((current_lightbulb_fx_data->options >> 4) & 7)
#define GAMMA           ((uint8_t) 0b00001000)
#define IS_GAMMA        ((current_lightbulb_fx_data->options & GAMMA) == GAMMA)
#define SIZE_SMALL      ((uint8_t) 0b00000000)
#define SIZE_MEDIUM     ((uint8_t) 0b00000010)
#define SIZE_LARGE      ((uint8_t) 0b00000100)
#define SIZE_XLARGE     ((uint8_t) 0b00000110)
#define SIZE_OPTION     ((current_lightbulb_fx_data->options >> 1) & 3)


lightbulb_fx_data_t* new_lightbulb_fx_data(uint16_t size, uint16_t channels) {
    lightbulb_fx_data_t* lightbulb_fx_data = calloc(1, sizeof(lightbulb_fx_data_t));
    
    lightbulb_fx_data->leds_array_size = size;
    lightbulb_fx_data->channels = channels;
    
    lightbulb_fx_data->speed = 1370;
    
    lightbulb_fx_data->leds_array = calloc(size * channels, sizeof(uint8_t));;
    
    return lightbulb_fx_data;
}

void set_fx_reverse(lightbulb_fx_data_t* lightbulb_fx_data, const bool is_reverse) {
    if (is_reverse) {
        lightbulb_fx_data->options |= REVERSE;
    } else {
        lightbulb_fx_data->options &= ~REVERSE;
    }
}

void set_fx_size(lightbulb_fx_data_t* lightbulb_fx_data, const uint8_t new_size) {
    lightbulb_fx_data->options &= ~SIZE_XLARGE;
    lightbulb_fx_data->options |= new_size << 1;
}

uint32_t get_lightbulb_fx_effect_now_ms() {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static void set_next_time(const uint32_t delay_ms) {
    current_lightbulb_fx_data->next_time = get_lightbulb_fx_effect_now_ms() + delay_ms;
}

#ifdef ESP_PLATFORM
static unsigned int IRAM_ATTR private_abs(int number) {
#else
static unsigned int IRAM private_abs(int number) {
#endif
    return (number < 0 ? -number : number);
}

#ifdef ESP_PLATFORM
static int IRAM_ATTR private_min(int a, int b) {
#else
    static int IRAM private_min(int a, int b) {
#endif
    if (a < b) {
        return a;
    }
    
    return b;
}

static void WS2812FX_setPixelColor_5(uint16_t address, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    if (address >= current_lightbulb_fx_data->leds_array_size) {
        return;
    }
    
    const unsigned int led_address_base = address * current_lightbulb_fx_data->channels;
    
    uint8_t color_components[4] = { r, g, b, w };
    
    for (unsigned int i = 0; i < current_lightbulb_fx_data->channels; i++) {
        current_lightbulb_fx_data->leds_array[led_address_base + i] = color_components[i];
    }
}

static uint32_t WS2812FX_random(uint32_t min, uint32_t max) {
    if (min < max) {
        uint32_t randomValue = hwrand() % (max - min + 1);
        return randomValue + min;
        
    } else if (min == max) {
        return min;
    }
    
    return 0;
}

static uint8_t WS2812FX_random8() {
    return (uint8_t) WS2812FX_random(0, 255);
}

static uint8_t WS2812FX_random8_lim(uint8_t lim) {
    return (uint8_t) WS2812FX_random(0, lim);
}

/*
static uint16_t WS2812FX_random16() {
    return (uint16_t) WS2812FX_random(0, 65535);
}
*/

static uint16_t WS2812FX_random16_lim(uint16_t lim) {
    return (uint16_t) WS2812FX_random(0, lim);
}

/*
static void WS2812FX_setPixelColor_4(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
    WS2812FX_setPixelColor_5(n, r, g, b, 0);
}
*/

static void WS2812FX_setPixelColor_2(uint16_t n, uint32_t c) {
    uint8_t w = (c >> 24) & 0xFF;
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >>  8) & 0xFF;
    uint8_t b =  c        & 0xFF;
    
    WS2812FX_setPixelColor_5(n, r, g, b, w);
}

static void WS2812FX_fill(uint32_t c, uint16_t first, uint16_t count) {
    // If first LED is past end of strip or outside segment boundaries, nothing to do
    if (first >= current_lightbulb_fx_data->leds_array_size) {
        return;
    }

    // Calculate the index ONE AFTER the last pixel to fill
    unsigned int end;
    
    if (count == 0) {
        end = current_lightbulb_fx_data->leds_array_size; // Fill to end of segment
        
    } else {
        end = first + count;
        if (end > current_lightbulb_fx_data->leds_array_size) {
            end = current_lightbulb_fx_data->leds_array_size;
        }
    }

    for (unsigned int i = first; i < end; i++) {
        WS2812FX_setPixelColor_2(i, c);
    }
}

static uint32_t WS2812FX_color_wheel(uint8_t pos) {
    pos = 255 - pos;
    
    if (pos < 85) {
        return ((uint32_t)(255 - pos * 3) << 16) | ((uint32_t)(0) << 8) | (pos * 3);
        
    } else if (pos < 170) {
        pos -= 85;
        return ((uint32_t)(0) << 16) | ((uint32_t)(pos * 3) << 8) | (255 - pos * 3);
        
    } else {
        pos -= 170;
        return ((uint32_t)(pos * 3) << 16) | ((uint32_t)(255 - pos * 3) << 8) | (0);
    }
}

static uint8_t WS2812FX_get_random_wheel_index(uint8_t pos) {
    uint8_t r = 0;
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t d = 0;
    
    while (d < 42) {
        r = WS2812FX_random8();
        x = private_abs((int) pos - (int) r);
        y = 255 - x;
        d = private_min(x, y);
    }
    
    return r;
}

static void WS2812FX_copyPixels(unsigned int dest, unsigned int src, unsigned int count) {
    if (dest + count <= current_lightbulb_fx_data->leds_array_size &&
        src + count <= current_lightbulb_fx_data->leds_array_size) {
        memmove(current_lightbulb_fx_data->leds_array + (dest * current_lightbulb_fx_data->channels),
                current_lightbulb_fx_data->leds_array + (src * current_lightbulb_fx_data->channels),
                count * current_lightbulb_fx_data->channels);
    }
}

static uint16_t WS2812FX_blink(uint32_t color1, uint32_t color2, bool strobe) {
    if (current_lightbulb_fx_data->counter_mode_call & 1) {
        uint32_t color = (IS_REVERSE) ? color1 : color2; // off
        WS2812FX_fill(color, 0, current_lightbulb_fx_data->leds_array_size);
        return strobe ? current_lightbulb_fx_data->speed - 20 : (current_lightbulb_fx_data->speed / 2);
        
    } else {
        uint32_t color = (IS_REVERSE) ? color2 : color1; // on
        WS2812FX_fill(color, 0, current_lightbulb_fx_data->leds_array_size);
        return strobe ? 20 : (current_lightbulb_fx_data->speed / 2);
    }
}

static uint16_t WS2812FX_color_wipe(uint32_t color1, uint32_t color2, bool rev) {
    if (current_lightbulb_fx_data->counter_mode_step < current_lightbulb_fx_data->leds_array_size) {
        uint32_t led_offset = current_lightbulb_fx_data->counter_mode_step;
        
        if (IS_REVERSE) {
            WS2812FX_setPixelColor_2(current_lightbulb_fx_data->leds_array_size - 1 - led_offset, color1);
        } else {
            WS2812FX_setPixelColor_2(led_offset, color1);
        }
        
    } else {
        uint32_t led_offset = current_lightbulb_fx_data->counter_mode_step - current_lightbulb_fx_data->leds_array_size;
        
        if ((IS_REVERSE && !rev) || (!IS_REVERSE && rev)) {
            WS2812FX_setPixelColor_2(current_lightbulb_fx_data->leds_array_size - 1 - led_offset, color2);
        } else {
            WS2812FX_setPixelColor_2(led_offset, color2);
        }
    }

    current_lightbulb_fx_data->counter_mode_step = (current_lightbulb_fx_data->counter_mode_step + 1) % (current_lightbulb_fx_data->leds_array_size * 2);

    return (current_lightbulb_fx_data->speed / (current_lightbulb_fx_data->leds_array_size * 2));
}
    


static uint8_t* WS2812FX_blend(uint8_t *dest, uint8_t *src1, uint8_t *src2, uint16_t cnt, uint8_t blendAmt) {
    if (blendAmt == 0) {
        memmove(dest, src1, cnt);
        
    } else if (blendAmt == 255) {
        memmove(dest, src2, cnt);
        
    } else {
        for (unsigned int i = 0; i < cnt; i++) {
            // dest[i] = map(blendAmt, 0, 255, src1[i], src2[i]);
            dest[i] =  blendAmt * ((int)src2[i] - (int)src1[i]) / 256 + src1[i]; // map() function
        }
    }
    
    return dest;
}
    
static uint32_t WS2812FX_color_blend(uint32_t color1, uint32_t color2, uint8_t blendAmt) {
    uint32_t blendedColor;
    WS2812FX_blend((uint8_t*) &blendedColor, (uint8_t*) &color1, (uint8_t*) &color2, sizeof(uint32_t), blendAmt);
    return blendedColor;
}

static uint32_t WS2812FX_getPixelColor(uint16_t address) {
    const unsigned int led_address_base = address * current_lightbulb_fx_data->channels;
    
    uint32_t color = 0;
    
    color += current_lightbulb_fx_data->leds_array[led_address_base] << 16;
    color += current_lightbulb_fx_data->leds_array[led_address_base + 1] << 8;
    color += current_lightbulb_fx_data->leds_array[led_address_base + 2];
    
    if (current_lightbulb_fx_data->channels > 3) {
        color += current_lightbulb_fx_data->leds_array[led_address_base + 3] << 24;
    }
    
    return color;
}

static void WS2812FX_fade_out_color(uint32_t targetColor) {
    /*
    const uint8_t rateMapH[] = { 0, 1, 1, 1, 2, 3, 4, 6 };
    const uint8_t rateMapL[] = { 0, 2, 3, 8, 8, 8, 8, 8 };
    
    uint8_t rate  = FADE_RATE;
    uint8_t rateH = rateMapH[rate];
    uint8_t rateL = rateMapL[rate];
    */
    
    const uint8_t rateH = 1;
    const uint8_t rateL = 3;
    
    uint32_t color = targetColor;
    int w2 = (color >> 24) & 0xFF;
    int r2 = (color >> 16) & 0xFF;
    int g2 = (color >>  8) & 0xFF;
    int b2 =  color        & 0xFF;
    
    for (unsigned int i = 0; i < current_lightbulb_fx_data->leds_array_size; i++) {
        color = WS2812FX_getPixelColor(i); // current color
        
        /*
        if (rate == 0) { // old fade-to-black algorithm
            WS2812FX_setPixelColor_2(i, (color >> 1) & 0x7F7F7F7F);
            
        } else { // new fade-to-color algorithm
        */
            int w1 = (color >> 24) & 0xFF;
            int r1 = (color >> 16) & 0xFF;
            int g1 = (color >>  8) & 0xFF;
            int b1 =  color        & 0xFF;
        
            // calculate the color differences between the current and target colors
            int wdelta = w2 - w1;
            int rdelta = r2 - r1;
            int gdelta = g2 - g1;
            int bdelta = b2 - b1;
        
            // if the current and target colors are almost the same, jump right to the target
            // color, otherwise calculate an intermediate color. (fixes rounding issues)
            wdelta = private_abs(wdelta) < 3 ? wdelta : (wdelta >> rateH) + (wdelta >> rateL);
            rdelta = private_abs(rdelta) < 3 ? rdelta : (rdelta >> rateH) + (rdelta >> rateL);
            gdelta = private_abs(gdelta) < 3 ? gdelta : (gdelta >> rateH) + (gdelta >> rateL);
            bdelta = private_abs(bdelta) < 3 ? bdelta : (bdelta >> rateH) + (bdelta >> rateL);

            WS2812FX_setPixelColor_5(i, r1 + rdelta, g1 + gdelta, b1 + bdelta, w1 + wdelta);
        //}
    }
}

static void WS2812FX_fade_out() {
    WS2812FX_fade_out_color(current_lightbulb_fx_data->colors[1]);
}

static void WS2812FX_scan(uint32_t color1, uint32_t color2, bool dual, bool with_fade) {
    int8_t dir = current_lightbulb_fx_data->aux_param ? -1 : 1;
    uint8_t size = 1 << SIZE_OPTION;
    
    if (size >= current_lightbulb_fx_data->leds_array_size) {
        return;
    }
    
    if (with_fade) {
        WS2812FX_fade_out();
    } else {
        WS2812FX_fill(color2, 0, current_lightbulb_fx_data->leds_array_size);
    }

    for (unsigned int i = 0; i < size; i++) {
        if (IS_REVERSE || dual) {
            WS2812FX_setPixelColor_2(current_lightbulb_fx_data->leds_array_size - 1 - current_lightbulb_fx_data->counter_mode_step - i, color1);
        }
        
        if (!IS_REVERSE || dual) {
            WS2812FX_setPixelColor_2(current_lightbulb_fx_data->counter_mode_step + i, color1);
        }
    }

    current_lightbulb_fx_data->counter_mode_step += dir;
    
    if (current_lightbulb_fx_data->counter_mode_step == 0) {
        current_lightbulb_fx_data->aux_param = 0;
    }
    
    if (current_lightbulb_fx_data->counter_mode_step >= (uint16_t)(current_lightbulb_fx_data->leds_array_size - size)) {
        current_lightbulb_fx_data->aux_param = 1;
    }

    set_next_time(current_lightbulb_fx_data->speed / (current_lightbulb_fx_data->leds_array_size * 2));
}

static void WS2812FX_tricolor_chase(uint32_t color1, uint32_t color2, uint32_t color3) {
    uint8_t sizeCnt = 1 << SIZE_OPTION;
    uint8_t sizeCnt2 = sizeCnt + sizeCnt;
    uint8_t sizeCnt3 = sizeCnt2 + sizeCnt;
    unsigned int index = current_lightbulb_fx_data->counter_mode_step % sizeCnt3;
    
    for (unsigned int i = 0; i < current_lightbulb_fx_data->leds_array_size; i++, index++) {
        index = index % sizeCnt3;

        uint32_t color = color3;
        if (index < sizeCnt) {
            color = color1;
        } else if (index < sizeCnt2) {
            color = color2;
        }

        if (IS_REVERSE) {
            WS2812FX_setPixelColor_2(i, color);
        } else {
            WS2812FX_setPixelColor_2(current_lightbulb_fx_data->leds_array_size - 1 - i, color);
        }
    }
    
    current_lightbulb_fx_data->counter_mode_step++;

    set_next_time(current_lightbulb_fx_data->speed / 16);
}

// https://github.com/adafruit/Adafruit_NeoPixel/blob/master/Adafruit_NeoPixel.h#L302
static uint8_t WS2812FX_ada_sine8(const uint8_t x) {
    const uint8_t _NeoPixelSineTable[256] = {
        128, 131, 134, 137, 140, 143, 146, 149, 152, 155, 158, 162, 165, 167, 170,
        173, 176, 179, 182, 185, 188, 190, 193, 196, 198, 201, 203, 206, 208, 211,
        213, 215, 218, 220, 222, 224, 226, 228, 230, 232, 234, 235, 237, 238, 240,
        241, 243, 244, 245, 246, 248, 249, 250, 250, 251, 252, 253, 253, 254, 254,
        254, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 251,
        250, 250, 249, 248, 246, 245, 244, 243, 241, 240, 238, 237, 235, 234, 232,
        230, 228, 226, 224, 222, 220, 218, 215, 213, 211, 208, 206, 203, 201, 198,
        196, 193, 190, 188, 185, 182, 179, 176, 173, 170, 167, 165, 162, 158, 155,
        152, 149, 146, 143, 140, 137, 134, 131, 128, 124, 121, 118, 115, 112, 109,
        106, 103, 100, 97,  93,  90,  88,  85,  82,  79,  76,  73,  70,  67,  65,
        62,  59,  57,  54,  52,  49,  47,  44,  42,  40,  37,  35,  33,  31,  29,
        27,  25,  23,  21,  20,  18,  17,  15,  14,  12,  11,  10,  9,   7,   6,
        5,   5,   4,   3,   2,   2,   1,   1,   1,   0,   0,   0,   0,   0,   0,
        0,   1,   1,   1,   2,   2,   3,   4,   5,   5,   6,   7,   9,   10,  11,
        12,  14,  15,  17,  18,  20,  21,  23,  25,  27,  29,  31,  33,  35,  37,
        40,  42,  44,  47,  49,  52,  54,  57,  59,  62,  65,  67,  70,  73,  76,
        79,  82,  85,  88,  90,  93,  97,  100, 103, 106, 109, 112, 115, 118, 121,
        124
    };
    
    return _NeoPixelSineTable[x];
}

static void WS2812FX_twinkle(uint32_t color1, uint32_t color2) {
    if (current_lightbulb_fx_data->counter_mode_step == 0) {
        WS2812FX_fill(color2, 0, current_lightbulb_fx_data->leds_array_size);
        uint16_t min_leds = (current_lightbulb_fx_data->leds_array_size / 4) + 1; // make sure, at least one LED is on
        current_lightbulb_fx_data->counter_mode_step = WS2812FX_random(min_leds, min_leds * 2);
    }
    
    WS2812FX_setPixelColor_2(WS2812FX_random16_lim(current_lightbulb_fx_data->leds_array_size - 1), color1);
    
    current_lightbulb_fx_data->counter_mode_step--;
    
    set_next_time(current_lightbulb_fx_data->speed / current_lightbulb_fx_data->leds_array_size);
}

static void WS2812FX_twinkle_fade(uint32_t color) {
    WS2812FX_fade_out();

    if (WS2812FX_random8_lim(3) == 0) {
        uint8_t size = 1 << SIZE_OPTION;
        uint16_t index = WS2812FX_random16_lim(current_lightbulb_fx_data->leds_array_size - size);
        WS2812FX_fill(color, index, size);
    }
    
    set_next_time(current_lightbulb_fx_data->speed / 16);
}

static void WS2812FX_sparkle(uint32_t color1, uint32_t color2) {
    if (current_lightbulb_fx_data->counter_mode_step == 0) {
        WS2812FX_fill(color1, 0, current_lightbulb_fx_data->leds_array_size);
    }

    uint8_t size = 1 << SIZE_OPTION;
    
    WS2812FX_fill(color1, current_lightbulb_fx_data->aux_param3, size);

    current_lightbulb_fx_data->aux_param3 = WS2812FX_random16_lim(current_lightbulb_fx_data->leds_array_size - size); // aux_param3 stores the random led index
    WS2812FX_fill(color2, current_lightbulb_fx_data->aux_param3, size);
    
    set_next_time(current_lightbulb_fx_data->speed / 32);
}

static void WS2812FX_chase(uint32_t color1, uint32_t color2, uint32_t color3) {
    uint8_t size = 1 << SIZE_OPTION;
    
    for (unsigned int i = 0; i < size; i++) {
        uint16_t a = (current_lightbulb_fx_data->counter_mode_step + i) % current_lightbulb_fx_data->leds_array_size;
        uint16_t b = (a + size) % current_lightbulb_fx_data->leds_array_size;
        uint16_t c = (b + size) % current_lightbulb_fx_data->leds_array_size;
        
        if (IS_REVERSE) {
            WS2812FX_setPixelColor_2(current_lightbulb_fx_data->leds_array_size - 1 - a, color1);
            WS2812FX_setPixelColor_2(current_lightbulb_fx_data->leds_array_size - 1 - b, color2);
            WS2812FX_setPixelColor_2(current_lightbulb_fx_data->leds_array_size - 1 - c, color3);
            
        } else {
            WS2812FX_setPixelColor_2(a, color1);
            WS2812FX_setPixelColor_2(b, color2);
            WS2812FX_setPixelColor_2(c, color3);
        }
    }

    current_lightbulb_fx_data->counter_mode_step = (current_lightbulb_fx_data->counter_mode_step + 1) % current_lightbulb_fx_data->leds_array_size;
    
    set_next_time(current_lightbulb_fx_data->speed / current_lightbulb_fx_data->leds_array_size);
}

static void WS2812FX_chase_flash(uint32_t color1, uint32_t color2) {
    const uint8_t flash_count = 1 << SIZE_OPTION;   // It was = 4
    uint8_t flash_step = current_lightbulb_fx_data->counter_mode_call % ((flash_count * 2) + 1);
    
    if (flash_step < (flash_count * 2)) {
        uint32_t color = (flash_step % 2 == 0) ? color2 : color1;
        uint16_t n = current_lightbulb_fx_data->counter_mode_step;
        uint16_t m = (current_lightbulb_fx_data->counter_mode_step + 1) % current_lightbulb_fx_data->leds_array_size;
        
        if (IS_REVERSE) {
            WS2812FX_setPixelColor_2(current_lightbulb_fx_data->leds_array_size - 1 - n, color);
            WS2812FX_setPixelColor_2(current_lightbulb_fx_data->leds_array_size - 1 - m, color);
            
        } else {
            WS2812FX_setPixelColor_2(n, color);
            WS2812FX_setPixelColor_2(m, color);
        }
        
        set_next_time(30);
        
    } else {
        current_lightbulb_fx_data->counter_mode_step = (current_lightbulb_fx_data->counter_mode_step + 1) % current_lightbulb_fx_data->leds_array_size;
        if (current_lightbulb_fx_data->counter_mode_step == 0) {
            // update aux_param so mode_chase_flash_random() will select the next color
            current_lightbulb_fx_data->aux_param = WS2812FX_get_random_wheel_index(current_lightbulb_fx_data->aux_param);
        }
        
        set_next_time(current_lightbulb_fx_data->speed / current_lightbulb_fx_data->leds_array_size);
    }
}

static void WS2812FX_running(uint32_t color1, uint32_t color2) {
    uint8_t size = 2 << SIZE_OPTION;
    
    uint32_t color = (current_lightbulb_fx_data->counter_mode_step & size) ? color1 : color2;

    if (IS_REVERSE) {
        WS2812FX_copyPixels(0, 1, current_lightbulb_fx_data->leds_array_size - 1);
        WS2812FX_setPixelColor_2(current_lightbulb_fx_data->leds_array_size - 1, color);
    } else {
        WS2812FX_copyPixels(1, 0, current_lightbulb_fx_data->leds_array_size - 1);
        WS2812FX_setPixelColor_2(0, color);
    }

    current_lightbulb_fx_data->counter_mode_step++;
    
    set_next_time(current_lightbulb_fx_data->speed / 16);
}

static void WS2812FX_fireworks(uint32_t color) {
    WS2812FX_fade_out();
    
    // for better performance, manipulate the Adafruit_NeoPixels pixels[] array directly
    uint8_t *pixels = current_lightbulb_fx_data->leds_array;
    uint8_t bytesPerPixel = current_lightbulb_fx_data->channels;
    uint16_t stopPixel = (current_lightbulb_fx_data->leds_array_size - 1) * bytesPerPixel;
    
    if (current_lightbulb_fx_data->leds_array_size >= 3) {
        for (unsigned int i = bytesPerPixel; i < stopPixel; i++) {
            uint16_t tmpPixel = (pixels[i - bytesPerPixel] >> 2) + pixels[i] + (pixels[i + bytesPerPixel] >> 2);
            
            pixels[i] = tmpPixel > 255 ? 255 : tmpPixel;
        }
    }
    
    uint8_t size = 2 << SIZE_OPTION;
    
    uint16_t numBursts = current_lightbulb_fx_data->leds_array_size / 10 > 1 ? current_lightbulb_fx_data->leds_array_size / 10 : 1;
    for (unsigned int i = 0; i < numBursts; i++) {
        uint16_t index = WS2812FX_random16_lim(current_lightbulb_fx_data->leds_array_size - size + 1);
        WS2812FX_fill(color, index, size);
    }
    
    set_next_time(current_lightbulb_fx_data->speed / 16);
}

static void WS2812FX_fire_flicker(int rev_intensity) {
    uint8_t w = (current_lightbulb_fx_data->colors[0] >> 24) & 0xFF;
    uint8_t r = (current_lightbulb_fx_data->colors[0] >> 16) & 0xFF;
    uint8_t g = (current_lightbulb_fx_data->colors[0] >>  8) & 0xFF;
    uint8_t b = (current_lightbulb_fx_data->colors[0]        & 0xFF);
    
    uint8_t maxLum = g > b ? g : b;
    
    maxLum = maxLum > r ? maxLum : r;
    maxLum = maxLum > w ? maxLum : w;
    
    uint8_t lum = maxLum / rev_intensity;
    
    for (unsigned int i = 0; i < current_lightbulb_fx_data->leds_array_size; i++) {
        uint8_t flicker = WS2812FX_random8_lim(lum);
        uint8_t r2 = (r - flicker) > 0 ? (r - flicker) : 0;
        uint8_t g2 = (g - flicker) > 0 ? (g - flicker) : 0;
        uint8_t b2 = (b - flicker) > 0 ? (b - flicker) : 0;
        uint8_t w2 = (w - flicker) > 0 ? (w - flicker) : 0;
        WS2812FX_setPixelColor_5(i, r2, g2, b2, w2);
    }
    
    set_next_time(current_lightbulb_fx_data->speed / current_lightbulb_fx_data->leds_array_size);
}

// Effects
// -------

/*
 * Normal blinking. 50% on/off time.
 */
static void WS2812FX_mode_blink() {
    set_next_time(WS2812FX_blink(current_lightbulb_fx_data->colors[0], current_lightbulb_fx_data->colors[1], false));
}

/*
 * Classic Blink effect. Cycling through the rainbow.
 */
static void WS2812FX_mode_blink_rainbow() {
    set_next_time(WS2812FX_blink(WS2812FX_color_wheel((current_lightbulb_fx_data->counter_mode_call << 2) & 0xFF), current_lightbulb_fx_data->colors[1], false));
}

/*
 * Classic Strobe effect.
 */
static void WS2812FX_mode_strobe() {
    set_next_time(WS2812FX_blink(current_lightbulb_fx_data->colors[0], current_lightbulb_fx_data->colors[1], true));
}

/*
 * Classic Strobe effect. Cycling through the rainbow.
 */
static void WS2812FX_mode_strobe_rainbow() {
    set_next_time(WS2812FX_blink(WS2812FX_color_wheel((current_lightbulb_fx_data->counter_mode_call << 2) & 0xFF), current_lightbulb_fx_data->colors[1], true));
}

/*
 * Lights all LEDs one after another.
 */
static void WS2812FX_mode_color_wipe(void) {
    set_next_time(WS2812FX_color_wipe(current_lightbulb_fx_data->colors[0], current_lightbulb_fx_data->colors[1], false));
}

static void WS2812FX_mode_color_wipe_inv(void) {
    set_next_time(WS2812FX_color_wipe(current_lightbulb_fx_data->colors[1], current_lightbulb_fx_data->colors[0], false));
}

static void WS2812FX_mode_color_wipe_rev(void) {
    set_next_time(WS2812FX_color_wipe(current_lightbulb_fx_data->colors[0], current_lightbulb_fx_data->colors[1], true));
}

static void WS2812FX_mode_color_wipe_rev_inv(void) {
    set_next_time(WS2812FX_color_wipe(current_lightbulb_fx_data->colors[1], current_lightbulb_fx_data->colors[0], true));
}

/*
 * Turns all LEDs after each other to a random color.
 * Then starts over with another color.
 */
static void WS2812FX_mode_color_wipe_random() {
    if (current_lightbulb_fx_data->counter_mode_step % current_lightbulb_fx_data->leds_array_size == 0) { // aux_param will store our random color wheel index
        current_lightbulb_fx_data->aux_param = WS2812FX_get_random_wheel_index(current_lightbulb_fx_data->aux_param);
    }
    
    uint32_t color = WS2812FX_color_wheel(current_lightbulb_fx_data->aux_param);
    
    set_next_time(WS2812FX_color_wipe(color, color, false) * 2);
}

/*
 * Random color introduced alternating from start and end of strip.
 */
static void WS2812FX_mode_color_sweep_random() {
    if (current_lightbulb_fx_data->counter_mode_step % current_lightbulb_fx_data->leds_array_size == 0) { // aux_param will store our random color wheel index
        current_lightbulb_fx_data->aux_param = WS2812FX_get_random_wheel_index(current_lightbulb_fx_data->aux_param);
    }
    
    uint32_t color = WS2812FX_color_wheel(current_lightbulb_fx_data->aux_param);
    
    set_next_time(WS2812FX_color_wipe(color, color, true) * 2);
}

/*
 * Lights all LEDs in one random color up. Then switches them
 * to the next random color.
 */
static void WS2812FX_mode_random_color() {
    current_lightbulb_fx_data->aux_param = WS2812FX_get_random_wheel_index(current_lightbulb_fx_data->aux_param); // aux_param will store our random color wheel index
    uint32_t color = WS2812FX_color_wheel(current_lightbulb_fx_data->aux_param);
    WS2812FX_fill(color, 0, current_lightbulb_fx_data->leds_array_size);
    
    set_next_time(current_lightbulb_fx_data->speed);
}

/*
 * Lights every LED in a random color. Changes one random LED after the other
 * to another random color.
 */
static void WS2812FX_mode_single_dynamic() {
    uint8_t size = 1 << SIZE_OPTION;
    
    if (current_lightbulb_fx_data->counter_mode_call == 0) { // initialize segment with random colors
        for (unsigned int i = 0; i < current_lightbulb_fx_data->leds_array_size; i += size) {
            WS2812FX_fill(WS2812FX_color_wheel(WS2812FX_random8()), i, size);
        }
    }
    
    uint16_t first = (WS2812FX_random16_lim(current_lightbulb_fx_data->leds_array_size / size) * size);
    WS2812FX_fill(WS2812FX_color_wheel(WS2812FX_random8()), first, size);
    
    set_next_time(current_lightbulb_fx_data->speed / 16);
}

/*
 * Lights every LED in a random color. Changes all LED at the same time
 * to new random colors.
 */
static void WS2812FX_mode_multi_dynamic() {
    if (SIZE_OPTION) {
        uint8_t size = 1 << SIZE_OPTION;
        
        for (unsigned int i = 0; i < current_lightbulb_fx_data->leds_array_size; i += size) {
            WS2812FX_fill(WS2812FX_color_wheel(WS2812FX_random8()), i, size);
        }
        
    } else {
        for (unsigned int i = 0; i < current_lightbulb_fx_data->leds_array_size; i++) {
            WS2812FX_setPixelColor_2(i, WS2812FX_color_wheel(WS2812FX_random8()));
        }
    }
    
    set_next_time(current_lightbulb_fx_data->speed / 4);
}

/*
 * Does the "standby-breathing" of well known i-Devices.
 */
static void WS2812FX_mode_breath() {
    int lum = current_lightbulb_fx_data->counter_mode_step;
    if (lum > 255) {
        lum = 511 - lum;    // lum = 15 -> 255 -> 15
    }
    
    unsigned int delay;
    if (lum == 15) delay = 970;         // 970ms pause before each breath
    else if (lum <=  25) delay = 38;    // 19
    else if (lum <=  50) delay = 36;    // 18
    else if (lum <=  75) delay = 28;    // 14
    else if (lum <= 100) delay = 20;    // 10
    else if (lum <= 125) delay = 14;    // 7
    else if (lum <= 150) delay = 11;    // 5
    else delay = 10;                    // 4
    
    uint32_t color = WS2812FX_color_blend(current_lightbulb_fx_data->colors[1], current_lightbulb_fx_data->colors[0], lum);
    WS2812FX_fill(color, 0, current_lightbulb_fx_data->leds_array_size);
    
    current_lightbulb_fx_data->counter_mode_step += 2;
    if (current_lightbulb_fx_data->counter_mode_step > (512 - 15)) {
        current_lightbulb_fx_data->counter_mode_step = 15;
    }
    
    set_next_time(delay * current_lightbulb_fx_data->speed / 1000);
}

/*
 * Fades the LEDs between two colors
 */
static void WS2812FX_mode_fade() {
    int lum = current_lightbulb_fx_data->counter_mode_step;
    if (lum > 255) {
        lum = 511 - lum;    // lum = 0 -> 255 -> 0
    }
    
    uint32_t color = WS2812FX_color_blend(current_lightbulb_fx_data->colors[1], current_lightbulb_fx_data->colors[0], lum);
    WS2812FX_fill(color, 0, current_lightbulb_fx_data->leds_array_size);

    current_lightbulb_fx_data->counter_mode_step += 4;
    if (current_lightbulb_fx_data->counter_mode_step > 511) {
        current_lightbulb_fx_data->counter_mode_step = 0;
    }
    
    set_next_time(current_lightbulb_fx_data->speed / 128);
}

/*
 * Runs a block of pixels back and forth.
 */
static void WS2812FX_mode_scan() {
    WS2812FX_scan(current_lightbulb_fx_data->colors[0], current_lightbulb_fx_data->colors[1], false, false);
}

/*
 * Runs two blocks of pixels back and forth in opposite directions.
 */
static void WS2812FX_mode_dual_scan() {
    WS2812FX_scan(current_lightbulb_fx_data->colors[0], current_lightbulb_fx_data->colors[1], true, false);
}
    
/*
 * Cycles all LEDs at once through a rainbow.
 */
static void WS2812FX_mode_rainbow() {
    uint32_t color = WS2812FX_color_wheel(current_lightbulb_fx_data->counter_mode_step);
    WS2812FX_fill(color, 0, current_lightbulb_fx_data->leds_array_size);

    current_lightbulb_fx_data->counter_mode_step = (current_lightbulb_fx_data->counter_mode_step + 1) & 0xFF;

    set_next_time(current_lightbulb_fx_data->speed / 256);
}

/*
 * Cycles a rainbow over the entire string of LEDs.
 */
static void WS2812FX_mode_rainbow_cycle() {
    uint32_t color = WS2812FX_color_wheel(current_lightbulb_fx_data->counter_mode_step);
    
    if (IS_REVERSE) {
        WS2812FX_copyPixels(0, 1, current_lightbulb_fx_data->leds_array_size - 1);
        WS2812FX_setPixelColor_2(current_lightbulb_fx_data->leds_array_size - 1, color);
    } else {
        WS2812FX_copyPixels(1, 0, current_lightbulb_fx_data->leds_array_size - 1);
        WS2812FX_setPixelColor_2(0, color);
    }
    
    uint8_t colorIndexIncr = 256 / current_lightbulb_fx_data->leds_array_size;
    if (colorIndexIncr == 0) {
        colorIndexIncr = 1;
    }
    
    current_lightbulb_fx_data->counter_mode_step += colorIndexIncr;
    
    if (current_lightbulb_fx_data->counter_mode_step > 255) {
        current_lightbulb_fx_data->counter_mode_step &= 0xFF;
    }

    set_next_time(current_lightbulb_fx_data->speed / 64);
}

/*
 * Tricolor chase mode
 */
static void WS2812FX_mode_tricolor_chase() {
    WS2812FX_tricolor_chase(current_lightbulb_fx_data->colors[0], current_lightbulb_fx_data->colors[1], current_lightbulb_fx_data->colors[2]);
}

/*
 * Alternating white/red/black pixels running.
 */
static void WS2812FX_mode_circus_combustus() {
    WS2812FX_tricolor_chase(RED, WHITE, BLACK);
}

/*
 * Theatre-style crawling lights.
 * Inspired by the Adafruit examples.
 */
static void WS2812FX_mode_theater_chase() {
    WS2812FX_tricolor_chase(current_lightbulb_fx_data->colors[0], current_lightbulb_fx_data->colors[1], current_lightbulb_fx_data->colors[1]);
}

/*
 * Theatre-style crawling lights with rainbow effect.
 * Inspired by the Adafruit examples.
 */
static void WS2812FX_mode_theater_chase_rainbow() {
    current_lightbulb_fx_data->aux_param = (current_lightbulb_fx_data->aux_param + 1) & 0xFF;
    uint32_t color = WS2812FX_color_wheel(current_lightbulb_fx_data->aux_param);
    WS2812FX_tricolor_chase(color, current_lightbulb_fx_data->colors[1], current_lightbulb_fx_data->colors[1]);
}

/*
 * Running lights effect with smooth sine transition.
 */
static void WS2812FX_mode_running_lights() {
    uint8_t size = 1 << SIZE_OPTION;
    uint8_t sineIncr = (256 / current_lightbulb_fx_data->leds_array_size) * size;
    sineIncr = sineIncr > 1 ? sineIncr : 1;
    
    for (unsigned int i = 0; i < current_lightbulb_fx_data->leds_array_size; i++) {
        const uint8_t lum = WS2812FX_ada_sine8(((i + current_lightbulb_fx_data->counter_mode_step) * sineIncr));
        uint32_t color = WS2812FX_color_blend(current_lightbulb_fx_data->colors[0], current_lightbulb_fx_data->colors[1], lum);
        
        if (IS_REVERSE) {
            WS2812FX_setPixelColor_2(i, color);
        } else {
            WS2812FX_setPixelColor_2(current_lightbulb_fx_data->leds_array_size - 1 - i,  color);
        }
    }
    
    current_lightbulb_fx_data->counter_mode_step = (current_lightbulb_fx_data->counter_mode_step + 1) % 256;
    
    set_next_time(current_lightbulb_fx_data->speed / current_lightbulb_fx_data->leds_array_size);
}

/*
 * Blink several LEDs on, reset, repeat.
 * Inspired by www.tweaking4all.com/hardware/arduino/arduino-led-strip-effects/
 */
static void WS2812FX_mode_twinkle() {
    WS2812FX_twinkle(current_lightbulb_fx_data->colors[0], current_lightbulb_fx_data->colors[1]);
}

/*
 * Blink several LEDs in random colors on, reset, repeat.
 * Inspired by www.tweaking4all.com/hardware/arduino/arduino-led-strip-effects/
 */
static void WS2812FX_mode_twinkle_random() {
    WS2812FX_twinkle(WS2812FX_color_wheel(WS2812FX_random8()), current_lightbulb_fx_data->colors[1]);
}

/*
 * Blink several LEDs on, fading out.
 */
static void WS2812FX_mode_twinkle_fade() {
    WS2812FX_twinkle_fade(current_lightbulb_fx_data->colors[0]);
}

/*
 * Blink several LEDs in random colors on, fading out.
 */
static void WS2812FX_mode_twinkle_fade_random() {
    WS2812FX_twinkle_fade(WS2812FX_color_wheel(WS2812FX_random8()));
}

/*
 * Blinks one LED at a time.
 * Inspired by www.tweaking4all.com/hardware/arduino/arduino-led-strip-effects/
 */
static void WS2812FX_mode_sparkle() {
    WS2812FX_sparkle(current_lightbulb_fx_data->colors[1], current_lightbulb_fx_data->colors[0]);
}

/*
 * Lights all LEDs in the color. Flashes white pixels randomly.
 * Inspired by www.tweaking4all.com/hardware/arduino/arduino-led-strip-effects/
 */
static void WS2812FX_mode_flash_sparkle() {
    WS2812FX_sparkle(current_lightbulb_fx_data->colors[0], WHITE);
}

/*
 * Like flash sparkle. With more flash.
 * Inspired by www.tweaking4all.com/hardware/arduino/arduino-led-strip-effects/
 */
static void WS2812FX_mode_hyper_sparkle() {
    WS2812FX_fill(current_lightbulb_fx_data->colors[0], 0, current_lightbulb_fx_data->leds_array_size);

    uint8_t size = 1 << SIZE_OPTION;
    for (unsigned int i = 0; i < 8; i++) {
        WS2812FX_fill(WHITE, WS2812FX_random16_lim(current_lightbulb_fx_data->leds_array_size - size), size);
    }
    
    set_next_time(current_lightbulb_fx_data->speed / 32);
}

/*
 * Strobe effect with different strobe count and pause, controlled by speed.
 */
static void WS2812FX_mode_multi_strobe() {
    WS2812FX_fill(current_lightbulb_fx_data->colors[1], 0, current_lightbulb_fx_data->leds_array_size);

    uint16_t delay = 200 + ((9 - (current_lightbulb_fx_data->speed % 10)) * 100);
    uint16_t count = 2 * ((current_lightbulb_fx_data->speed / 100) + 1);
    if (current_lightbulb_fx_data->counter_mode_step < count) {
        if ((current_lightbulb_fx_data->counter_mode_step & 1) == 0) {
            WS2812FX_fill(current_lightbulb_fx_data->colors[0], 0, current_lightbulb_fx_data->leds_array_size);
            delay = 20;
        } else {
            delay = 50;
        }
    }

    current_lightbulb_fx_data->counter_mode_step = (current_lightbulb_fx_data->counter_mode_step + 1) % (count + 1);
    
    set_next_time(delay);
}

/*
 * Bicolor chase mode
 */
static void WS2812FX_mode_bicolor_chase() {
    WS2812FX_chase(current_lightbulb_fx_data->colors[0], current_lightbulb_fx_data->colors[1], current_lightbulb_fx_data->colors[2]);
}

/*
 * White running on _color.
 */
static void WS2812FX_mode_chase_color() {
    WS2812FX_chase(current_lightbulb_fx_data->colors[0], WHITE, WHITE);
}

/*
 * Black running on _color.
 */
static void WS2812FX_mode_chase_blackout() {
    WS2812FX_chase(current_lightbulb_fx_data->colors[0], BLACK, BLACK);
}

/*
 * _color running on white.
 */
static void WS2812FX_mode_chase_white() {
    WS2812FX_chase(WHITE, current_lightbulb_fx_data->colors[0], current_lightbulb_fx_data->colors[0]);
}

/*
 * White running followed by random color.
 */
static void WS2812FX_mode_chase_random() {
    if (current_lightbulb_fx_data->counter_mode_step == 0) {
        current_lightbulb_fx_data->aux_param = WS2812FX_get_random_wheel_index(current_lightbulb_fx_data->aux_param);
    }
    
    WS2812FX_chase(WS2812FX_color_wheel(current_lightbulb_fx_data->aux_param), WHITE, WHITE);
}

/*
 * Rainbow running on white.
 */
static void WS2812FX_mode_chase_rainbow_white() {
    uint16_t n = current_lightbulb_fx_data->counter_mode_step;
    uint16_t m = (current_lightbulb_fx_data->counter_mode_step + 1) % current_lightbulb_fx_data->leds_array_size;
    uint32_t color2 = WS2812FX_color_wheel(((n * 256 / current_lightbulb_fx_data->leds_array_size) + (current_lightbulb_fx_data->counter_mode_call & 0xFF)) & 0xFF);
    uint32_t color3 = WS2812FX_color_wheel(((m * 256 / current_lightbulb_fx_data->leds_array_size) + (current_lightbulb_fx_data->counter_mode_call & 0xFF)) & 0xFF);

    WS2812FX_chase(WHITE, color2, color3);
}

/*
 * White running on rainbow.
 */
static void WS2812FX_mode_chase_rainbow() {
    uint8_t color_sep = 256 / current_lightbulb_fx_data->leds_array_size;
    uint8_t color_index = current_lightbulb_fx_data->counter_mode_call & 0xFF;
    uint32_t color = WS2812FX_color_wheel(((current_lightbulb_fx_data->counter_mode_step * color_sep) + color_index) & 0xFF);

    WS2812FX_chase(color, WHITE, WHITE);
}

/*
 * Black running on rainbow.
 */
static void WS2812FX_mode_chase_blackout_rainbow() {
    uint8_t color_sep = 256 / current_lightbulb_fx_data->leds_array_size;
    uint8_t color_index = current_lightbulb_fx_data->counter_mode_call & 0xFF;
    uint32_t color = WS2812FX_color_wheel(((current_lightbulb_fx_data->counter_mode_step * color_sep) + color_index) & 0xFF);

    WS2812FX_chase(color, BLACK, BLACK);
}

/*
 * White flashes running on _color.
 */
static void WS2812FX_mode_chase_flash() {
    WS2812FX_chase_flash(current_lightbulb_fx_data->colors[0], WHITE);
}

/*
 * White flashes running, followed by random color.
 */
static void WS2812FX_mode_chase_flash_random() {
    WS2812FX_chase_flash(WS2812FX_color_wheel(current_lightbulb_fx_data->aux_param), WHITE);
}

/*
 * Alternating color/white pixels running.
 */
static void WS2812FX_mode_running_color() {
    WS2812FX_running(current_lightbulb_fx_data->colors[0], current_lightbulb_fx_data->colors[1]);
}

/*
 * Alternating red/blue pixels running.
 */
static void WS2812FX_mode_running_red_blue() {
    WS2812FX_running(RED, BLUE);
}

/*
 * Alternating red/green pixels running.
 */
static void WS2812FX_mode_merry_christmas() {
    WS2812FX_running(RED, GREEN);
}

/*
 * Alternating orange/purple pixels running.
 */
static void WS2812FX_mode_halloween() {
    WS2812FX_running(PURPLE, ORANGE);
}

/*
 * Random colored pixels running.
 */
static void WS2812FX_mode_running_random() {
    uint8_t size = 2 << SIZE_OPTION;
    
    if ((current_lightbulb_fx_data->counter_mode_step) % size == 0) {
        current_lightbulb_fx_data->aux_param = WS2812FX_get_random_wheel_index(current_lightbulb_fx_data->aux_param);
    }
    
    uint32_t color = WS2812FX_color_wheel(current_lightbulb_fx_data->aux_param);

    WS2812FX_running(color, color);
}

/*
 * K.I.T.T.
 */
static void WS2812FX_mode_larson_scanner() {
    WS2812FX_scan(current_lightbulb_fx_data->colors[0], current_lightbulb_fx_data->colors[1], false, true);
}

/*
 * Firing comets from one end.
 */
static void WS2812FX_mode_comet() {
    WS2812FX_fade_out();

    if (IS_REVERSE) {
        WS2812FX_setPixelColor_2(current_lightbulb_fx_data->leds_array_size - 1 - current_lightbulb_fx_data->counter_mode_step, current_lightbulb_fx_data->colors[0]);
    } else {
        WS2812FX_setPixelColor_2(current_lightbulb_fx_data->counter_mode_step, current_lightbulb_fx_data->colors[0]);
    }

    current_lightbulb_fx_data->counter_mode_step = (current_lightbulb_fx_data->counter_mode_step + 1) % current_lightbulb_fx_data->leds_array_size;

    set_next_time(current_lightbulb_fx_data->speed / current_lightbulb_fx_data->leds_array_size);
}

/*
 * Firework sparks.
 */
static void WS2812FX_mode_fireworks() {
    uint32_t color = BLACK;
    
    do {    // randomly choose a non-BLACK color from the colors array
        color = current_lightbulb_fx_data->colors[WS2812FX_random8_lim(2)];
    } while (color == BLACK);
    
    WS2812FX_fireworks(color);
}

/*
 * Random colored firework sparks.
 */
static void WS2812FX_mode_fireworks_random() {
    WS2812FX_fireworks(WS2812FX_color_wheel(WS2812FX_random8()));
}

/*
 * Random flickering.
 */
static void WS2812FX_mode_fire_flicker() {
    WS2812FX_fire_flicker(3);
}

/*
* Random flickering, less intensity.
*/
static void WS2812FX_mode_fire_flicker_soft() {
    WS2812FX_fire_flicker(6);
}

/*
* Random flickering, more intensity.
*/
static void WS2812FX_mode_fire_flicker_intense() {
    WS2812FX_fire_flicker(1);
}

// An adaptation of Mark Kriegsman's FastLED twinkleFOX effect
// https://gist.github.com/kriegsman/756ea6dcae8e30845b5a
static void WS2812FX_mode_twinkleFOX() {
    uint8_t size = 1 << SIZE_OPTION;
    uint16_t mySeed = 0;    // reset the random number generator seed
    
    // Get the segment's colors array values
    uint32_t color0 = current_lightbulb_fx_data->colors[0];
    uint32_t color1 = current_lightbulb_fx_data->colors[1];
    uint32_t color2 = current_lightbulb_fx_data->colors[2];
    uint32_t blendedColor;
    
    for (unsigned int i = 0; i < current_lightbulb_fx_data->leds_array_size; i += size) {
        // Use Mark Kriegsman's clever idea of using pseudo-random numbers to determine
        // each LED's initial and increment blend values
        mySeed = (mySeed * 2053) + 13849;   // a random, but deterministic, number
        uint16_t initValue = (mySeed + (mySeed >> 8)) & 0xFF;   // the LED's initial blend index (0-255)
        mySeed = (mySeed * 2053) + 13849;   // another random, but deterministic, number
        uint16_t incrValue = (((mySeed + (mySeed >> 8)) & 0x07) + 1) * 2;   // blend index increment (2,4,6,8,10,12,14,16)
        
        // We're going to use a sine function to blend colors, instead of Mark's triangle
        // function, simply because a sine lookup table is already built into the
        // Adafruit_NeoPixel lib. Yes, I'm lazy.
        // Use the counter_mode_call var as a clock "tick" counter and calc the blend index
        uint8_t blendIndex = (initValue + (current_lightbulb_fx_data->counter_mode_call * incrValue)) & 0xFF;   // 0-255
        // Index into the built-in Adafruit_NeoPixel sine table to lookup the blend amount
        uint8_t blendAmt = WS2812FX_ada_sine8(blendIndex); // 0-255
        
        // If colors[0] is BLACK, blend random colors
        if (color0 == BLACK) {
            blendedColor = WS2812FX_color_blend(WS2812FX_color_wheel(initValue), color1, blendAmt);
            // If colors[2] isn't BLACK, choose to blend colors[0]/colors[1] or colors[1]/colors[2]
            // (which color pair to blend is picked randomly)
            
        } else if ((color2 != BLACK) && (initValue < 128) == 0) {
            blendedColor = WS2812FX_color_blend(color2, color1, blendAmt);
            // Otherwise always blend colors[0]/colors[1]
            
        } else {
            blendedColor = WS2812FX_color_blend(color0, color1, blendAmt);
        }
        
        // Assign the new color to the number of LEDs specified by the SIZE option
        for (unsigned int j = 0; j < size; j++) {
            const uint16_t address = i + j;
            if (address < current_lightbulb_fx_data->leds_array_size) {
                WS2812FX_setPixelColor_2(address, blendedColor);
            }
        }
    }
    
    set_next_time(current_lightbulb_fx_data->speed / 32);
}

// A combination of the Fireworks effect and the running effect
// to create an effect that looks like rain.
static void WS2812FX_mode_rain() {
    uint32_t rainColor = 0;
    
    // randomly choose colors[0], colors[1] or colors[2]
    while (rainColor == 0) {
        rainColor = current_lightbulb_fx_data->colors[WS2812FX_random8_lim(2)];
    }
    
    // run the fireworks effect to create a "raindrop"
    WS2812FX_fireworks(rainColor);
    
    // shift everything two pixels
    if (IS_REVERSE) {
        WS2812FX_copyPixels(0, 2, current_lightbulb_fx_data->leds_array_size - 2);
    } else {
        WS2812FX_copyPixels(2, 0, current_lightbulb_fx_data->leds_array_size - 2);
    }
}

static void WS2812FX_mode_pause() {
    set_next_time(current_lightbulb_fx_data->speed);
}

// -------------

void set_fx_speed(lightbulb_fx_data_t* lightbulb_fx_data, const uint8_t new_speed) {
    const unsigned int new_speed_inv = 100 - new_speed;
    const unsigned int new_fx_speed = ((new_speed_inv * 16) * (new_speed_inv / 10)) + 20;
    
    lightbulb_fx_data->speed = new_fx_speed;
    lightbulb_fx_data->next_time = get_lightbulb_fx_effect_now_ms() + 100;
}

bool set_lightbulb_fx_effect(lightbulb_fx_data_t* lightbulb_fx_data) {
    if (!lightbulb_fx_data || lightbulb_fx_data->effect == 0) {
        return false;
    }
    
    current_lightbulb_fx_data = lightbulb_fx_data;
    
    unsigned int is_frame = false;
    
    uint32_t now = get_lightbulb_fx_effect_now_ms();
    if (now >= current_lightbulb_fx_data->next_time || current_lightbulb_fx_data->next_time - now > 10000000 ) {
        switch (current_lightbulb_fx_data->effect) {
            case 1:
                WS2812FX_mode_blink();
                break;
                
            case 2:
                WS2812FX_mode_breath();
                break;
                
            case 3:
                WS2812FX_mode_color_wipe();
                break;
                
            case 4:
                WS2812FX_mode_color_wipe_inv();
                break;
                
            case 5:
                WS2812FX_mode_color_wipe_rev();
                break;
                
            case 6:
                WS2812FX_mode_color_wipe_rev_inv();
                break;
                
            case 7:
                WS2812FX_mode_color_wipe_random();
                break;
                
            case 8:
                WS2812FX_mode_random_color();
                break;
                
            case 9:
                WS2812FX_mode_single_dynamic();
                break;
                
            case 10:
                WS2812FX_mode_multi_dynamic();
                break;
                
            case 11:
                WS2812FX_mode_rainbow();
                break;
                
            case 12:
                WS2812FX_mode_rainbow_cycle();
                break;
                
            case 13:
                WS2812FX_mode_scan();
                break;
                
            case 14:
                WS2812FX_mode_dual_scan();
                break;
                
            case 15:
                WS2812FX_mode_fade();
                break;
                
            case 16:
                WS2812FX_mode_theater_chase();
                break;
                
            case 17:
                WS2812FX_mode_theater_chase_rainbow();
                break;
                
            case 18:
                WS2812FX_mode_running_lights();
                break;
                
            case 19:
                WS2812FX_mode_twinkle();
                break;
                
            case 20:
                WS2812FX_mode_twinkle_random();
                break;
                
            case 21:
                WS2812FX_mode_twinkle_fade();
                break;
                
            case 22:
                WS2812FX_mode_twinkle_fade_random();
                break;
                
            case 23:
                WS2812FX_mode_sparkle();
                break;
                
            case 24:
                WS2812FX_mode_flash_sparkle();
                break;
                
            case 25:
                WS2812FX_mode_hyper_sparkle();
                break;
                
            case 26:
                WS2812FX_mode_strobe();
                break;
                
            case 27:
                WS2812FX_mode_strobe_rainbow();
                break;
                
            case 28:
                WS2812FX_mode_multi_strobe();
                break;
                
            case 29:
                WS2812FX_mode_blink_rainbow();
                break;
                
            case 30:
                WS2812FX_mode_chase_white();
                break;
                
            case 31:
                WS2812FX_mode_chase_color();
                break;
                
            case 32:
                WS2812FX_mode_chase_random();
                break;
                
            case 33:
                WS2812FX_mode_chase_rainbow();
                break;
                
            case 34:
                WS2812FX_mode_chase_flash();
                break;
                
            case 35:
                WS2812FX_mode_chase_flash_random();
                break;
                
            case 36:
                WS2812FX_mode_chase_rainbow_white();
                break;
                
            case 37:
                WS2812FX_mode_chase_blackout();
                break;
                
            case 38:
                WS2812FX_mode_chase_blackout_rainbow();
                break;
                
            case 39:
                WS2812FX_mode_color_sweep_random();
                break;
                
            case 40:
                WS2812FX_mode_running_color();
                break;
                
            case 41:
                WS2812FX_mode_running_red_blue();
                break;
                
            case 42:
                WS2812FX_mode_running_random();
                break;
                
            case 43:
                WS2812FX_mode_larson_scanner();
                break;
                
            case 44:
                WS2812FX_mode_comet();
                break;
                
            case 45:
                WS2812FX_mode_fireworks();
                break;
                
            case 46:
                WS2812FX_mode_fireworks_random();
                break;
                
            case 47:
                WS2812FX_mode_merry_christmas();
                break;
                
            case 48:
                WS2812FX_mode_fire_flicker();
                break;
                
            case 49:
                WS2812FX_mode_fire_flicker_soft();
                break;
                
            case 50:
                WS2812FX_mode_fire_flicker_intense();
                break;
                
            case 51:
                WS2812FX_mode_circus_combustus();
                break;
                
            case 52:
                WS2812FX_mode_halloween();
                break;
                
            case 53:
                WS2812FX_mode_bicolor_chase();
                break;
                
            case 54:
                WS2812FX_mode_tricolor_chase();
                break;
                
            case 55:
                WS2812FX_mode_twinkleFOX();
                break;
                
            case 56:
                WS2812FX_mode_rain();
                break;
                
            case 100:
                WS2812FX_mode_pause();
                current_lightbulb_fx_data->counter_mode_call--;
                break;
                
            default:
                current_lightbulb_fx_data->effect = 0;
                break;
        }
        
        current_lightbulb_fx_data->counter_mode_call++;
        
        is_frame = true;
    }
    
    current_lightbulb_fx_data = NULL;
    
    return is_frame;
}
