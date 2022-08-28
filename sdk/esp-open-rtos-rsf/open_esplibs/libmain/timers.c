/* Recreated Espressif libmain timers.o contents.

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE
*/
#include "open_esplibs.h"
#if OPEN_LIBMAIN_TIMERS
// The contents of this file are only built if OPEN_LIBMAIN_TIMERS is set to true

#include "etstimer.h"
#include "stdio.h"

struct timer_list_st {
    struct timer_list_st *next;
    ETSTimer *timer;
};

static struct timer_list_st *timer_list;
static uint8_t armed_timer_count;

void sdk_os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg) {
    struct timer_list_st *entry = 0;
    struct timer_list_st *new_entry;
    struct timer_list_st **tailptr;

    if (timer_list) {
        for (entry = timer_list; ; entry = entry->next) {
            if (entry->timer == ptimer) {
                if (ptimer->timer_arg == parg && ptimer->timer_func == pfunction) {
                    return;
                }
                if (ptimer->timer_handle) {
                    if (!xTimerDelete(ptimer->timer_handle, 50)) {
                        printf("Timer Delete Failed\n");
                    }
                    armed_timer_count--;
                }
                ptimer->timer_func = pfunction;
                ptimer->timer_arg = parg;
                ptimer->timer_handle = 0;
                ptimer->timer_ms = 0;
                return;
            }
            if (!entry->next) {
                break;
            }
        }
    }
    ptimer->timer_func = pfunction;
    ptimer->timer_arg = parg;
    ptimer->timer_handle = 0;
    ptimer->timer_ms = 0;
    new_entry = (struct timer_list_st *)pvPortMalloc(8);
    new_entry->timer = ptimer;
    new_entry->next = 0;
    tailptr = &entry->next;
    if (!timer_list) {
        tailptr = &timer_list;
    }
    *tailptr = new_entry;
}

static void timer_tramp(TimerHandle_t xTimer)
{
    ETSTimer *ptimer = pvTimerGetTimerID(xTimer);
    ptimer->timer_func(ptimer->timer_arg);
}

void sdk_os_timer_arm(ETSTimer *ptimer, uint32_t milliseconds, bool repeat_flag) {
    if (!ptimer->timer_handle) {
        ptimer->timer_repeat = repeat_flag;
        ptimer->timer_ms = milliseconds;
        ptimer->timer_handle = xTimerCreate(0, milliseconds/10, repeat_flag, ptimer, timer_tramp);
        armed_timer_count++;
        if (!ptimer->timer_handle) {
            //FIXME: should print an error? (original code doesn't)
            return;
        }
    }
    if (ptimer->timer_repeat != repeat_flag) {
        ptimer->timer_repeat = repeat_flag;
        // FIXME: This is wrong.  The original code is directly modifying
        // internal FreeRTOS structures to try to change the uxAutoReload of an
        // existing timer.  The correct way to do this is probably to use
        // xTimerDelete and then xTimerCreate to recreate the timer with a
        // different uxAutoReload setting.
        ((uint32_t *)ptimer->timer_handle)[7] = repeat_flag;
    }
    if (ptimer->timer_ms != milliseconds) {
        ptimer->timer_ms = milliseconds;
        xTimerChangePeriod(ptimer->timer_handle, milliseconds/10, 10);
    }
    if (!xTimerStart(ptimer->timer_handle, 50)) {
        printf("Timer Start Failed\n");
    }
}

void sdk_os_timer_disarm(ETSTimer *ptimer) {
    if (ptimer->timer_handle) {
        if (!xTimerStop(ptimer->timer_handle, 50)) {
            printf("Timer Stop Failed\n");
        }
    }
}

#endif /* OPEN_LIBMAIN_TIMERS */
