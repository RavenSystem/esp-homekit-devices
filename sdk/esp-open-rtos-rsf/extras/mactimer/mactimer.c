/*
 * MAC NMI interrupt based timer support.
 *
 * Copyright (C) 2018 to 2019 OurAirQuality.org
 *
 * Licensed under the Apache License, Version 2.0, January 2004 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *      http://www.apache.org/licenses/
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE CONTRIBUTORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS WITH THE SOFTWARE.
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include "esp/dport_regs.h"
#include "mactimer/mactimer.h"

/* The MAC timer handler offers a higher priority timer interrupt, but comes
 * with some significant practical limitations.
 */

static mactimer_t* timer_list = 0;

void mactimer_setfn(mactimer_t *timer, mactimer_func_t *func, void *parg)
{
    timer->callback = func;
    timer->timer_arg = parg;
    timer->trigger_usec = 0;
    timer->next = NULL;
}

/* Return the Mac timer count, a 64 bit value.
 * This can be called without the NMI interrupt disabled. */
uint64_t IRAM mactime_get_count(void) {
    uint32_t high1 = *(uint32_t volatile *)0x3FF21008;
    uint32_t low = *(uint32_t volatile *)0x3FF21004;
    uint32_t high2 = *(uint32_t volatile *)0x3FF21008;

    if (high1 != high2) {
        /* The high word just clocked over, so resample the low value to
         * match. It will not change for some time now so the new low word
         * matches high2. */
        low = *(uint32_t volatile *)0x3FF21004;
    }

    return ((uint64_t)high2 << 32UL) | (uint32_t)low;
}

/* Set the Mac timer to trigger at the given absolute count.  This is expected
 * to be called with the NMI disabled, such as from a handler. */
static inline void IRAM mactime_set_trigger(uint64_t count) {
    *(uint32_t volatile *)0x3FF2109C = (uint32_t)count;
    *(uint32_t volatile *)0x3FF210A0 = (uint32_t)(count >> 32UL);
    *(uint32_t volatile *)0x3FF21098 |= 0x80000000;
}

/* Insert the timer into the queue to trigger at the given absolute
 * count. This does not actually set the timer trigger, and the caller is
 * expected to do so. This is typically called from a handler to set the next
 * trigger time, and the MAC timer handler sets the next trigger count if
 * necessary before returning. */
void IRAM mactime_add_pending(mactimer_t *timer, uint64_t count)
{
    mactimer_t *prev = NULL;
    mactimer_t *curr = timer_list;
    while (curr) {
        if (((int64_t)count - (int64_t)curr->trigger_usec) < 1) {
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    timer->next = curr;
    timer->trigger_usec = count;

    if (prev != NULL) {
        prev->next = timer;
    } else {
        timer_list = timer;
    }
}

/* This is called outside the NMI context, with the NMI enabled, and it
 * disables the NMI to synchronize access to the data structures. If a MAC
 * timer handler wishes to set another timeout, such as for a periodic timer,
 * then it need only call mactime_add_pending() before returning.
 */
void mactimer_arm(mactimer_t *timer, uint64_t count)
{
    /* Guard against being called withing the NMI handler. */
    if (sdk_NMIIrqIsOn == 0) {
        /* Disable the maskable interrupts. */
        vPortEnterCritical();
        /* Disable the NMI. */
        do {
            DPORT.DPORT0 &= 0xFFFFFFE0;
        } while (DPORT.DPORT0 & 1);
    }

    mactime_add_pending(timer, mactime_get_count() + count);
    mactime_set_trigger(timer_list->trigger_usec);

    if (sdk_NMIIrqIsOn == 0) {
        /* Reenable the NMI. */
        DPORT.DPORT0 = (DPORT.DPORT0 & 0xFFFFFFE0) | 1;
        /* Enable the maskable interrupts. */
        vPortExitCritical();
    }
}

/* This is called outside the NMI context, with the NMI enabled, and it
 * disables the NMI to synchronize access to the data structures.
 */
void mactimer_disarm(mactimer_t *timer)
{
    /* Guard against being called withing the NMI handler. */
    if (sdk_NMIIrqIsOn == 0) {
        /* Disable the maskable interrupts. */
        vPortEnterCritical();
        /* Disable the NMI. */
        do {
            DPORT.DPORT0 &= 0xFFFFFFE0;
        } while (DPORT.DPORT0 & 1);
    }

    /* Remove timer from the timer_list. */
    mactimer_t *timers = timer_list;
    if (timers == timer) {
        timer_list = timers->next;
    } else {
        while (timers) {
            mactimer_t *next = timers->next;
            if (next == timer) {
                timers->next = next->next;
                break;
            }
            timers = next;
        }
    }

    if (sdk_NMIIrqIsOn == 0) {
        /* Reenable the NMI. */
        DPORT.DPORT0 = (DPORT.DPORT0 & 0xFFFFFFE0) | 1;
        /* Enable the maskable interrupts. */
        vPortExitCritical();
    }
}

/*
 * NMI handler. The callbacks are called in this NMI context. If there are
 * pending timers remaining when done then a new timeout is set.
 *
 * This is a fragile context that can be called even when processor interrupts
 * are masked, so it can not touch data synchronized by disabling maskable
 * interrupts. So don't expect to be able call into the FreeRTOS functions or
 * the C library etc.
 *
 * It can be called with a flash operation in progress, so that the flash is
 * not readable, so handlers can not depend on code or data stored in
 * flash. Keep handlers in IRAM, and watch our for constant data that might be
 * linked into flash.
 *
 * It might delay handling of MAC interrupts which could compromise the Wifi
 * handling, so keep any handlers as quick as possible.
 */
static IRAM void mactimer_handler()
{
    while (timer_list) {
        if (((int64_t)timer_list->trigger_usec - (int64_t)mactime_get_count()) > 0) {
            /* Nothing remaining to handle now. */
            break;
        }

        mactimer_t *timer = timer_list;
        timer_list = timer->next;
        timer->next = NULL;
        timer->callback(timer->timer_arg);
    }

    if (timer_list) {
        /* Reset the trigger. */
        mactime_set_trigger(timer_list->trigger_usec);
    }
}

extern void IRAM sdk_wDev_MacTimSetFunc(void * arg0);

void mactimer_init()
{
    timer_list = NULL;

    sdk_wDev_MacTimSetFunc(mactimer_handler);
}
