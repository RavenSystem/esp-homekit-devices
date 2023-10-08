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
//static uint8_t armed_timer_count;

static void ptimer_delete(ETSTimer* ptimer) {
    //printf("- DELETE %p\n", ptimer);
    
    if (ptimer->timer_handle && xTimerDelete(ptimer->timer_handle, 50) == pdPASS) {
        ptimer->timer_handle = NULL;
        ptimer->timer_ms = 0;
    }
}

void sdk_os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg) {
    //printf("* SETFN %p\n", ptimer);
    
    struct timer_list_st *entry = 0;
    struct timer_list_st *new_entry;
    struct timer_list_st **tailptr;

    if (timer_list) {
        for (entry = timer_list; ; entry = entry->next) {
            if (entry->timer == ptimer) {
                if (ptimer->timer_arg == parg && ptimer->timer_func == pfunction) {
                    return;
                }
                
                /*
                if (ptimer->timer_handle) {
                    if (!xTimerDelete(ptimer->timer_handle, 50)) {
                        printf("! Timer Delete\n");
                    }
                    
                    //armed_timer_count--;
                }
                */
                
                ptimer_delete(ptimer);
                ptimer->timer_func = pfunction;
                ptimer->timer_arg = parg;
                
                return;
            }
            if (!entry->next) {
                break;
            }
        }
    }
    ptimer->timer_func = pfunction;
    ptimer->timer_arg = parg;
    ptimer->timer_handle = NULL;
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

static void IRAM timer_tramp(TimerHandle_t xTimer) {
    ETSTimer *ptimer = pvTimerGetTimerID(xTimer);
    
    //printf("+ TRAMP %p\n", ptimer);
    
    ptimer->timer_func(ptimer->timer_arg);
    
    if (!ptimer->timer_repeat && xTimerDelete(ptimer->timer_handle, 0) == pdPASS) {
        ptimer->timer_handle = NULL;
        ptimer->timer_ms = 0;
        //printf("- TRAMP DELETE %p, t: %i\n", ptimer, ptimer->timer_ms);
    }
}

void IRAM sdk_os_timer_arm(ETSTimer *ptimer, uint32_t milliseconds, bool repeat_flag) {
    //printf("+ ARM %p, t: %i, r: %i\n", ptimer, milliseconds, repeat_flag);
    
    /*
    if (!ptimer->timer_handle) {
        ptimer->timer_repeat = repeat_flag;
        ptimer->timer_ms = milliseconds;
        ptimer->timer_handle = xTimerCreate(0, milliseconds / 10, repeat_flag, ptimer, timer_tramp);
        //armed_timer_count++;
        
        if (!ptimer->timer_handle) {
            //FIXME: should print an error? (original code doesn't)
            return;
        }
        
    } else {
        if (ptimer->timer_repeat != repeat_flag) {
            ptimer->timer_repeat = repeat_flag;
            
            // FIXME: This is wrong.  The original code is directly modifying
            // internal FreeRTOS structures to try to change the uxAutoReload of an
            // existing timer.  The correct way to do this is probably to use
            // xTimerDelete and then xTimerCreate to recreate the timer with a
            // different uxAutoReload setting.
            //((uint32_t *)ptimer->timer_handle)[7] = repeat_flag;
            vTimerSetReloadMode(ptimer->timer_handle, repeat_flag);
        }
        
        if (ptimer->timer_ms != milliseconds) {
            ptimer->timer_ms = milliseconds;
            xTimerChangePeriod(ptimer->timer_handle, milliseconds / 10, 10);
        }
    }
    
    if (!xTimerStart(ptimer->timer_handle, 0)) {
        printf("! Timer Start\n");
    }
    */
    
    if (ptimer->timer_repeat != repeat_flag ||
        ptimer->timer_ms != milliseconds) {
        ptimer_delete(ptimer);
    }
    
    if (!ptimer->timer_handle) {
        //printf("+ NEW %p\n", ptimer);
        
        ptimer->timer_repeat = repeat_flag;
        ptimer->timer_ms = milliseconds;
        ptimer->timer_handle = xTimerCreate(NULL, milliseconds / 10, repeat_flag, ptimer, timer_tramp);
    }
    
    xTimerStart(ptimer->timer_handle, 50);
}

void IRAM sdk_os_timer_disarm(ETSTimer* ptimer) {
    //printf("- DISARM %p\n", ptimer);
    
    /*
    if (ptimer->timer_handle) {
        if (!xTimerStop(ptimer->timer_handle, 50)) {
            printf("! Timer Stop\n");
        }
    }
    */
    
    if (ptimer->timer_handle) {
        xTimerStop(ptimer->timer_handle, 50);
    }
}

#endif /* OPEN_LIBMAIN_TIMERS */
