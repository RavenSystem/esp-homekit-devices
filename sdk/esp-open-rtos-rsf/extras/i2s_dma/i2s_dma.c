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

#include "i2s_dma.h"
#include "esp/iomux.h"
#include "esp/i2s_regs.h"
#include "esp/interrupts.h"
#include "esp/iomux.h"
#include "common_macros.h"

#include <stdlib.h>

// #define I2S_DMA_DEBUG

#ifdef I2S_DMA_DEBUG
#include <stdio.h>
#define debug(fmt, ...) printf("%s" fmt "\n", "i2s_dma: ", ## __VA_ARGS__);
#else
#define debug(fmt, ...)
#endif

// The following definitions is taken from ESP8266_MP3_DECODER demo
// https://github.com/espressif/ESP8266_MP3_DECODER/blob/master/mp3/driver/i2s_freertos.c
// It is requred to set clock to I2S subsystem
void sdk_rom_i2c_writeReg_Mask(uint32_t block, uint32_t host_id,
        uint32_t reg_add, uint32_t Msb, uint32_t Lsb, uint32_t indata);

#ifndef i2c_bbpll
#define i2c_bbpll                               0x67
#define i2c_bbpll_en_audio_clock_out            4
#define i2c_bbpll_en_audio_clock_out_msb        7
#define i2c_bbpll_en_audio_clock_out_lsb        7
#define i2c_bbpll_hostid                        4
#endif

#define i2c_writeReg_Mask(block, host_id, reg_add, Msb, Lsb, indata) \
    sdk_rom_i2c_writeReg_Mask(block, host_id, reg_add, Msb, Lsb, indata)

#define i2c_writeReg_Mask_def(block, reg_add, indata) \
    i2c_writeReg_Mask(block, block##_hostid,  reg_add,  reg_add##_msb, \
            reg_add##_lsb,  indata)


void i2s_dma_init(i2s_dma_isr_t isr, void *arg, i2s_clock_div_t clock_div, i2s_pins_t pins)
{
    // reset DMA
    SET_MASK_BITS(SLC.CONF0, SLC_CONF0_RX_LINK_RESET);
    CLEAR_MASK_BITS(SLC.CONF0, SLC_CONF0_RX_LINK_RESET);

    // clear DMA int flags
    SLC.INT_CLEAR = 0xFFFFFFFF;
    SLC.INT_CLEAR = 0;

    // Enable and configure DMA
    SLC.CONF0 = SET_FIELD(SLC.CONF0, SLC_CONF0_MODE, 0);   // does it really needed?
    SLC.CONF0 = SET_FIELD(SLC.CONF0, SLC_CONF0_MODE, 1);

    // Do we really need to set and clear?
    SET_MASK_BITS(SLC.RX_DESCRIPTOR_CONF, SLC_RX_DESCRIPTOR_CONF_INFOR_NO_REPLACE |
            SLC_RX_DESCRIPTOR_CONF_TOKEN_NO_REPLACE);
    CLEAR_MASK_BITS(SLC.RX_DESCRIPTOR_CONF, SLC_RX_DESCRIPTOR_CONF_RX_FILL_ENABLE |
            SLC_RX_DESCRIPTOR_CONF_RX_EOF_MODE | SLC_RX_DESCRIPTOR_CONF_RX_FILL_MODE);

    if (isr) {
        _xt_isr_attach(INUM_SLC, isr, arg);
        SET_MASK_BITS(SLC.INT_ENABLE, SLC_INT_ENABLE_RX_EOF);
        SLC.INT_CLEAR = 0xFFFFFFFF;
        _xt_isr_unmask(1<<INUM_SLC);
    }

    // start transmission
    SET_MASK_BITS(SLC.RX_LINK, SLC_RX_LINK_START);

    if (pins.data) {
        iomux_set_function(gpio_to_iomux(3), IOMUX_GPIO3_FUNC_I2SO_DATA);
    }
    if (pins.clock) {
        iomux_set_function(gpio_to_iomux(15), IOMUX_GPIO15_FUNC_I2SO_BCK);
    }
    if (pins.ws) {
        iomux_set_function(gpio_to_iomux(2), IOMUX_GPIO2_FUNC_I2SO_WS);
    }

    // enable clock to i2s subsystem
    i2c_writeReg_Mask_def(i2c_bbpll, i2c_bbpll_en_audio_clock_out, 1);

    // reset I2S subsystem
    CLEAR_MASK_BITS(I2S.CONF, I2S_CONF_RESET_MASK);
    SET_MASK_BITS(I2S.CONF, I2S_CONF_RESET_MASK);
    CLEAR_MASK_BITS(I2S.CONF, I2S_CONF_RESET_MASK);

    // select 16bits per channel (FIFO_MOD=0), no DMA access (FIFO only)
    CLEAR_MASK_BITS(I2S.FIFO_CONF, I2S_FIFO_CONF_DESCRIPTOR_ENABLE);
    I2S.FIFO_CONF = SET_FIELD(I2S.FIFO_CONF, I2S_FIFO_CONF_RX_FIFO_MOD, 0);
    I2S.FIFO_CONF = SET_FIELD(I2S.FIFO_CONF, I2S_FIFO_CONF_TX_FIFO_MOD, 0);

    //trans master&rece slave,MSB shift,right_first,msb right
    CLEAR_MASK_BITS(I2S.CONF, I2S_CONF_TX_SLAVE_MOD);
    I2S.CONF = SET_FIELD(I2S.CONF, I2S_CONF_BITS_MOD, 0);
    I2S.CONF = SET_FIELD(I2S.CONF, I2S_CONF_BCK_DIV, 0);
    I2S.CONF = SET_FIELD(I2S.CONF, I2S_CONF_CLKM_DIV, 0);

    SET_MASK_BITS(I2S.CONF, I2S_CONF_RIGHT_FIRST | I2S_CONF_MSB_RIGHT |
            I2S_CONF_RX_SLAVE_MOD | I2S_CONF_RX_MSB_SHIFT | I2S_CONF_TX_MSB_SHIFT);
    I2S.CONF = SET_FIELD(I2S.CONF, I2S_CONF_BCK_DIV, clock_div.bclk_div);
    I2S.CONF = SET_FIELD(I2S.CONF, I2S_CONF_CLKM_DIV, clock_div.clkm_div);
}

// Base frequency for I2S subsystem is independent from CPU clock.
#define BASE_FREQ (160000000L)

i2s_clock_div_t i2s_get_clock_div(int32_t freq)
{
    i2s_clock_div_t div = {0, 0};
    int32_t best_freq = 0;

    for (uint32_t bclk_div = 1; bclk_div < 64; bclk_div++) {
        for (uint32_t clkm_div = 1; clkm_div < 64; clkm_div++) {
            int32_t curr_freq = BASE_FREQ / (bclk_div * clkm_div);
            if (abs(freq - curr_freq) < abs(freq - best_freq)) {
                best_freq = curr_freq;
                div.clkm_div = clkm_div;
                div.bclk_div = bclk_div;
            }
        }
    }

    debug("Requested frequency: %d, set frequency: %d\n", freq, best_freq);
    debug("clkm_div: %d, bclk_div: %d\n", div.clkm_div, div.bclk_div);

    return div;
}

void i2s_dma_start(dma_descriptor_t *descr)
{
    // configure DMA descriptor
    SLC.RX_LINK = SET_FIELD(SLC.RX_LINK, SLC_RX_LINK_DESCRIPTOR_ADDR, 0);
    SLC.RX_LINK = SET_FIELD(SLC.RX_LINK, SLC_RX_LINK_DESCRIPTOR_ADDR, (uint32_t)descr);

    // enable DMA in i2s subsystem
    SET_MASK_BITS(I2S.FIFO_CONF, I2S_FIFO_CONF_DESCRIPTOR_ENABLE);

    //Start transmission
    SET_MASK_BITS(I2S.CONF, I2S_CONF_TX_START);
}

void i2s_dma_stop()
{
    SLC.RX_LINK = SET_FIELD(SLC.RX_LINK, SLC_RX_LINK_DESCRIPTOR_ADDR, 0);
    CLEAR_MASK_BITS(I2S.FIFO_CONF, I2S_FIFO_CONF_DESCRIPTOR_ENABLE);
}
