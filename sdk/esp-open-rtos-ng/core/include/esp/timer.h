/** esp/timer.h
 *
 * Timer (FRC1 & FRC2) functions.
 *
 * Part of esp-open-rtos
 * Copyright (C) 2015 Superhouse Automation Pty Ltd
 * BSD Licensed as described in the file LICENSE
 */
#ifndef _ESP_TIMER_H
#define _ESP_TIMER_H

#include <stdbool.h>
#include <errno.h>
#include "esp/timer_regs.h"
#include "esp/interrupts.h"

#ifdef	__cplusplus
extern "C" {
#endif

typedef enum {
    FRC1 = 0,
    FRC2 = 1,
} timer_frc_t;

/* Return current count value for timer. */
static inline uint32_t timer_get_count(const timer_frc_t frc)
{
    return TIMER(frc).COUNT;
}

/* Return current load value for timer. */
static inline uint32_t timer_get_load(const timer_frc_t frc)
{
    return TIMER(frc).LOAD;
}

/* Write load value for timer. */
static inline void timer_set_load(const timer_frc_t frc, const uint32_t load)
{
    TIMER(frc).LOAD = load;
}

/* Returns maximum load value for timer. */
static inline uint32_t timer_max_load(const timer_frc_t frc)
{
    return (frc == FRC1) ? TIMER_FRC1_MAX_LOAD : UINT32_MAX;
}

/* Set the timer divider value */
static inline void timer_set_divider(const timer_frc_t frc, const timer_clkdiv_t div)
{
    if(div < TIMER_CLKDIV_1 || div > TIMER_CLKDIV_256)
        return;
    TIMER(frc).CTRL = SET_FIELD(TIMER(frc).CTRL, TIMER_CTRL_CLKDIV, div);
}

/* Enable or disable timer interrupts

   This both sets the xtensa interrupt mask and writes to the DPORT register
   that allows timer interrupts.
*/
void timer_set_interrupts(const timer_frc_t frc, bool enable);

/* Turn the timer on or off */
static inline void timer_set_run(const timer_frc_t frc, const bool run)
{
    if (run)
        TIMER(frc).CTRL |= TIMER_CTRL_RUN;
    else
        TIMER(frc).CTRL &= ~TIMER_CTRL_RUN;
}

/* Get the run state of the timer (on or off) */
static inline bool timer_get_run(const timer_frc_t frc)
{
    return TIMER(frc).CTRL & TIMER_CTRL_RUN;
}

/* Set timer auto-reload on or off */
static inline void timer_set_reload(const timer_frc_t frc, const bool reload)
{
    if (reload)
        TIMER(frc).CTRL |= TIMER_CTRL_RELOAD;
    else
        TIMER(frc).CTRL &= ~TIMER_CTRL_RELOAD;
}

/* Get the auto-reload state of the timer (on or off) */
static inline bool timer_get_reload(const timer_frc_t frc)
{
    return TIMER(frc).CTRL & TIMER_CTRL_RELOAD;
}

/* Return the number of timer counts to achieve the specified
 * frequency with the specified divisor.
 *
 * frc parameter is used to check out-of-range values for timer size.
 *
 * Returns 0 if the given freq/divisor combo cannot be achieved.
 *
 */
uint32_t timer_freq_to_count(const timer_frc_t frc, uint32_t freq, const timer_clkdiv_t div);

/* Return a suitable timer divider for the specified frequency,
   or -1 if none is found.
 */
static inline timer_clkdiv_t timer_freq_to_div(uint32_t freq)
{
    /*
      try to maintain resolution without risking overflows.
      these values are a bit arbitrary at the moment! */
    if(freq > 100*1000)
        return TIMER_CLKDIV_1;
    else if(freq > 100)
        return TIMER_CLKDIV_16;
    else
        return TIMER_CLKDIV_256;
}

/* Return a suitable timer divider for the specified duration in
   microseconds or -1 if none is found.
 */
static inline timer_clkdiv_t timer_time_to_div(uint32_t us)
{
    /*
      try to maintain resolution without risking overflows. Similar to
      timer_freq_to_div, these values are a bit arbitrary at the
      moment! */
    if(us < 1000)
        return TIMER_CLKDIV_1;
    else if(us < 10*1000)
        return TIMER_CLKDIV_16;
    else
        return TIMER_CLKDIV_256;
}

/* Return the number of timer counts for the specified timer duration
 * in microseconds, when using the specified divisor.
 *
 * frc paraemter is used to check out-of-range values for timer size.
 *
 * Returns 0 if the given time/divisor combo cannot be achieved.
 *
 */
uint32_t timer_time_to_count(const timer_frc_t frc, uint32_t us, const timer_clkdiv_t div);

/* Set a target timer interrupt frequency in Hz.

   For FRC1 this sets the timer load value and enables autoreload so
   the interrupt will fire regularly with the target frequency.

   For FRC2 this sets the timer match value so the next interrupt
   comes in line with the target frequency. However this won't repeat
   automatically, you have to call timer_set_frequency again when the
   timer interrupt runs.

   Will change the timer divisor value to suit the target frequency.

   Does not start/stop the timer, you have to do this manually via
   timer_set_run.

   Returns 0 on success, or -EINVAL if given frequency could not be set.
*/
int timer_set_frequency(const timer_frc_t frc, uint32_t freq);

/* Sets the timer for a oneshot interrupt in 'us' microseconds.

   Will change the timer divisor value to suit the target time.

   Does not change the autoreload setting.

   For FRC2 this sets the timer match value relative to the current
   load value.

   Note that for a true "one shot" timeout with FRC1 then you need to
   also disable FRC1 in the timer interrupt handler by calling
   timer_set_run(TIMER_FRC1, false);

   Returns 0 on success, or -EINVAL if the given timeout could not be set.
*/
int timer_set_timeout(const timer_frc_t frc, uint32_t us);

#ifdef	__cplusplus
}
#endif

#endif
