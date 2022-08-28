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
#ifndef __I2S_DMA_H__
#define __I2S_DMA_H__

#include <stdint.h>
#include <stdbool.h>
#include "esp/slc_regs.h"

#ifdef	__cplusplus
extern "C" {
#endif

typedef void (*i2s_dma_isr_t)(void *);

typedef struct dma_descriptor {
    uint32_t blocksize:12;
    uint32_t datalen:12;
    uint32_t unused:5;
    uint32_t sub_sof:1;
    uint32_t eof:1;
    uint32_t owner:1;

    void* buf_ptr;
    struct dma_descriptor *next_link_ptr;
} dma_descriptor_t;

typedef struct {
    uint8_t bclk_div;
    uint8_t clkm_div;
} i2s_clock_div_t;

typedef struct {
    bool data;
    bool clock;
    bool ws;
} i2s_pins_t;

/**
 * Initialize I2S and DMA subsystems.
 *
 * @param isr ISR handler. Can be NULL if interrupt handling is not needed.
 * @param arg ISR handler arg.
 * @param clock_div I2S clock configuration.
 * @param pins I2S pin configuration. Specifies which pins are enabled in I2S.
 */
void i2s_dma_init(i2s_dma_isr_t isr, void *arg, i2s_clock_div_t clock_div, i2s_pins_t pins);

/**
 * Calculate I2S dividers for the specified frequency.
 *
 * I2S_FREQ = 160000000 / (bclk_div * clkm_div)
 * Base frequency is independent from the CPU frequency.
 */
i2s_clock_div_t i2s_get_clock_div(int32_t freq);

/**
 * Start I2S transmittion.
 *
 * @param descr Pointer to the first descriptor in the linked list of descriptors.
 */
void i2s_dma_start(dma_descriptor_t *descr);

/**
 * Stop I2S transmittion.
 */
void i2s_dma_stop();

/**
 * Clear interrupt in the I2S ISR handler.
 *
 * It is intended to be called from ISR.
 */
inline void i2s_dma_clear_interrupt()
{
    SLC.INT_CLEAR = 0xFFFFFFFF;
}

/**
 * Check if it is EOF interrupt.
 *
 * It is intended to be called from ISR.
 */
inline bool i2s_dma_is_eof_interrupt()
{
    return (SLC.INT_STATUS & SLC_INT_STATUS_RX_EOF);
}

/**
 * Get pointer to a descriptor that caused EOF interrupt.
 * It is the last processed descriptor.
 *
 * It is intended to be called from ISR.
 */
inline dma_descriptor_t *i2s_dma_get_eof_descriptor()
{
    return (dma_descriptor_t*)SLC.RX_EOF_DESCRIPTOR_ADDR;
}

#ifdef	__cplusplus
}
#endif

#endif  // __I2S_DMA_H__
