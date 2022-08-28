/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 sheinz (https://github.com/sheinz)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "ws2812_i2s.h"
#include "i2s_dma/i2s_dma.h"

#include <string.h>
#include <malloc.h>

// #define WS2812_I2S_DEBUG

#ifdef WS2812_I2S_DEBUG
#include <stdio.h>
#define debug(fmt, ...) printf("%s" fmt "\n", "ws2812_i2s: ", ## __VA_ARGS__);
#else
#define debug(fmt, ...)
#endif

#define MAX_DMA_BLOCK_SIZE      4095
// #define DMA_PIXEL_SIZE          16    // each colour takes 4 bytes

/**
 * Amount of zero data to produce WS2812 reset condition.
 * DMA data must be multiple of 4
 * 16 bytes of 0 gives ~50 microseconds of low pulse
 */
#define WS2812_ZEROES_LENGTH    16

static uint8_t i2s_dma_zero_buf[WS2812_ZEROES_LENGTH] = {0};

static dma_descriptor_t *dma_block_list;
static uint32_t dma_block_list_size;

static void *dma_buffer;
static uint32_t dma_buffer_size;

#ifdef WS2812_I2S_DEBUG
volatile uint32_t dma_isr_counter = 0;
#endif

static volatile bool i2s_dma_processing = false;

static void dma_isr_handler(void *arg)
{
    if (i2s_dma_is_eof_interrupt()) {
#ifdef WS2812_I2S_DEBUG
        dma_isr_counter++;
#endif
        i2s_dma_processing = false;
    }
    i2s_dma_clear_interrupt();
}

/**
 * Form a linked list of descriptors (dma blocks).
 * The last two blocks are zero block and stop block.
 * The last block is a stop terminal block. It has no data and no next block.
 */
static inline void init_descriptors_list(uint8_t *buf, uint32_t total_dma_data_size)
{
    for (int i = 0; i < dma_block_list_size; i++) {
        dma_block_list[i].owner = 1;
        dma_block_list[i].eof = 0;
        dma_block_list[i].sub_sof = 0;
        dma_block_list[i].unused = 0;
        dma_block_list[i].buf_ptr = buf;

        if (total_dma_data_size >= MAX_DMA_BLOCK_SIZE) {
            dma_block_list[i].datalen = MAX_DMA_BLOCK_SIZE;
            dma_block_list[i].blocksize = MAX_DMA_BLOCK_SIZE;
            total_dma_data_size -= MAX_DMA_BLOCK_SIZE;
            buf += MAX_DMA_BLOCK_SIZE;
        } else {
            dma_block_list[i].datalen = total_dma_data_size;
            dma_block_list[i].blocksize = total_dma_data_size;
            total_dma_data_size = 0;
        }

        if (i == (dma_block_list_size - 2)) {  // zero block
            dma_block_list[i].buf_ptr = i2s_dma_zero_buf;
            dma_block_list[i].datalen = WS2812_ZEROES_LENGTH;
            dma_block_list[i].blocksize = WS2812_ZEROES_LENGTH;
        }

        if (i == (dma_block_list_size - 1)) {  // stop block
            // it needs a valid buffer even if no data to output
            dma_block_list[i].buf_ptr = i2s_dma_zero_buf;
            dma_block_list[i].datalen = 0;
            dma_block_list[i].blocksize = WS2812_ZEROES_LENGTH;
            dma_block_list[i].next_link_ptr = 0;

            // the last stop block should trigger interrupt
            dma_block_list[i].eof = 1;
        } else {
            dma_block_list[i].next_link_ptr = &dma_block_list[i + 1];
        }
    }
}

int ws2812_i2s_init(uint32_t pixels_number, pixeltype_t type)
{
    dma_buffer_size = pixels_number * type;
    dma_block_list_size = dma_buffer_size / MAX_DMA_BLOCK_SIZE;

    if (dma_buffer_size % MAX_DMA_BLOCK_SIZE) {
        dma_block_list_size += 1;
    }

    dma_block_list_size += 2;  // zero block and stop block

    debug("allocating %d dma blocks\n", dma_block_list_size);

    if(!dma_block_list)
    {
        dma_block_list = (dma_descriptor_t*)malloc(
                dma_block_list_size * sizeof(dma_descriptor_t));
        if(!dma_block_list)
        {
            return -1;
        }
    }
    debug("allocating %d bytes for DMA buffer\n", dma_buffer_size);
    if(!dma_buffer)
    {
        dma_buffer = malloc(dma_buffer_size);
        if(!dma_buffer)
        {
          return -1;
        }
    }
    memset(dma_buffer, 0xFA, dma_buffer_size);

    init_descriptors_list(dma_buffer, dma_buffer_size);

    i2s_clock_div_t clock_div = i2s_get_clock_div(3333333);
    i2s_pins_t i2s_pins = {.data = true, .clock = false, .ws = false};

    debug("i2s clock dividers, bclk=%d, clkm=%d\n",
            clock_div.bclk_div, clock_div.clkm_div);

    i2s_dma_init(dma_isr_handler, NULL, clock_div, i2s_pins);
    return 0;
}

const IRAM_DATA int16_t bitpatterns[16] =
{
    0b1000100010001000, 0b1000100010001110, 0b1000100011101000, 0b1000100011101110,
    0b1000111010001000, 0b1000111010001110, 0b1000111011101000, 0b1000111011101110,
    0b1110100010001000, 0b1110100010001110, 0b1110100011101000, 0b1110100011101110,
    0b1110111010001000, 0b1110111010001110, 0b1110111011101000, 0b1110111011101110,
};

void ws2812_i2s_update(ws2812_pixel_t *pixels, pixeltype_t type)
{
    while (i2s_dma_processing) {};

    uint16_t *p_dma_buf = dma_buffer;

    for (uint32_t i = 0; i < (dma_buffer_size / type); i++) {
        // green
        *p_dma_buf++ =  bitpatterns[pixels[i].green & 0x0F];
        *p_dma_buf++ =  bitpatterns[pixels[i].green >> 4];

        // red
        *p_dma_buf++ =  bitpatterns[pixels[i].red & 0x0F];
        *p_dma_buf++ =  bitpatterns[pixels[i].red >> 4];

        // blue
        *p_dma_buf++ =  bitpatterns[pixels[i].blue & 0x0F];
        *p_dma_buf++ =  bitpatterns[pixels[i].blue >> 4];
        
        if(type == PIXEL_RGBW) {
          // white
          *p_dma_buf++ =  bitpatterns[pixels[i].white & 0x0F];
          *p_dma_buf++ =  bitpatterns[pixels[i].white >> 4];
        }
    }

    i2s_dma_processing = true;
    i2s_dma_start(dma_block_list);
}
