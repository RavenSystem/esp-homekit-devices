/* Timer peripheral management functions for esp/timer.h.
 *
 *
 * Part of esp-open-rtos
 * Copyright (C) 2015 Superhouse Automation Pty Ltd
 * BSD Licensed as described in the file LICENSE
 */
#include <esp/timer.h>
#include <esp/dport_regs.h>
#include <stdio.h>
#include <stdlib.h>

/* Timer divisor number to maximum frequency */
#define _FREQ_DIV1  (80*1000*1000)
#define _FREQ_DIV16 (5*1000*1000)
#define _FREQ_DIV256 312500

const static uint32_t IROM _TIMER_FREQS[] = { _FREQ_DIV1, _FREQ_DIV16, _FREQ_DIV256 };

/* Timer divisor index to divisor value */
const static uint32_t IROM _TIMER_DIV_VAL[] = { 1, 16, 256 };

void timer_set_interrupts(const timer_frc_t frc, bool enable)
{
    const uint32_t dp_bit = (frc == FRC1) ? DPORT_INT_ENABLE_FRC1 : DPORT_INT_ENABLE_FRC2;
    const uint32_t int_mask = BIT((frc == FRC1) ? INUM_TIMER_FRC1 : INUM_TIMER_FRC2);
    if(enable) {
        DPORT.INT_ENABLE |= dp_bit;
        _xt_isr_unmask(int_mask);
    } else {
        DPORT.INT_ENABLE &= ~dp_bit;
        _xt_isr_mask(int_mask);
    }
}

uint32_t timer_freq_to_count(const timer_frc_t frc, const uint32_t freq, const timer_clkdiv_t div)
{
    if(div < TIMER_CLKDIV_1 || div > TIMER_CLKDIV_256)
        return 0; /* invalid divider */

    if(freq > _TIMER_FREQS[div])
        return 0; /* out of range for given divisor */

    uint64_t counts = _TIMER_FREQS[div]/freq;
    return counts;
}

uint32_t timer_time_to_count(const timer_frc_t frc, uint32_t us, const timer_clkdiv_t div)
{
    if(div < TIMER_CLKDIV_1 || div > TIMER_CLKDIV_256)
        return 0; /* invalid divider */

    const uint32_t TIMER_MAX = timer_max_load(frc);

    if(div != TIMER_CLKDIV_256) /* timer tick in MHz */
    {
        /* timer is either 80MHz or 5MHz, so either 80 or 5 MHz counts per us */
        const uint32_t counts_per_us = ((div == TIMER_CLKDIV_1) ? _FREQ_DIV1 : _FREQ_DIV16)/1000/1000;
        if(us > TIMER_MAX/counts_per_us)
            return 0; /* Multiplying us by mhz_per_count will overflow TIMER_MAX */
        return us*counts_per_us;
    }
    else /* /256 divider, 312.5kHz freq so need to scale up */
    {
        /* derived from naive floating point equation that we can't use:
        counts = (us/1000/1000)*_FREQ_DIV256;
        counts = (us/2000)*(_FREQ_DIV256/500);
        counts = us*(_FREQ_DIV256/500)/2000;
        */
        const uint32_t scalar = _FREQ_DIV256/500;
        if(us > 1+UINT32_MAX/scalar)
            return 0; /* Multiplying us by _FREQ_DIV256/500 will overflow uint32_t */

        uint32_t counts = (us*scalar)/2000;
        if(counts > TIMER_MAX)
            return 0; /* counts value too high for timer type */
        return counts;
    }
}

int timer_set_frequency(const timer_frc_t frc, uint32_t freq)
{
    uint32_t counts = 0;
    timer_clkdiv_t div = timer_freq_to_div(freq);

    if(freq == 0) //can't divide by 0
    {
        return -EINVAL;
    }

    counts = timer_freq_to_count(frc, freq, div);
    if(counts == 0)
    {
        return -EINVAL;
    }

    timer_set_divider(frc, div);
    if(frc == FRC1)
    {
        timer_set_load(frc, counts);
        timer_set_reload(frc, true);
    }
    else /* FRC2 */
    {
        /* assume that if this overflows it'll wrap, so we'll get desired behaviour */
        TIMER(1).ALARM = counts + TIMER(1).COUNT;
    }
    return 0;
}

int _timer_set_timeout_impl(const timer_frc_t frc, uint32_t us)
{
    uint32_t counts = 0;
    timer_clkdiv_t div = timer_time_to_div(us);

    counts = timer_time_to_count(frc, us, div);
    if(counts == 0)
        return -EINVAL; /* can't set frequency */

    timer_set_divider(frc, div);
    if(frc == FRC1)
    {
        timer_set_load(frc, counts);
    }
    else /* FRC2 */
    {
        TIMER(1).ALARM = counts + TIMER(1).COUNT;
    }

    return 0;
}

int timer_set_timeout(const timer_frc_t frc, uint32_t us)
{
    uint32_t counts = 0;
    timer_clkdiv_t div = timer_time_to_div(us);

    counts = timer_time_to_count(frc, us, div);
    if(counts == 0)
        return -EINVAL; /* can't set frequency */

    timer_set_divider(frc, div);
    if(frc == FRC1)
    {
        timer_set_load(frc, counts);
    }
    else /* FRC2 */
    {
        TIMER(1).ALARM = counts + TIMER(1).COUNT;
    }

    return 0;
}
