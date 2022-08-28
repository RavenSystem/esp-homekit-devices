/**
 * Recreated Espressif libmain ets_timer.o contents.
 *
 * Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
 * BSD Licensed as described in the file LICENSE
 *
 * Copyright (c) 2016 sheinz (https://github.com/sheinz)
 *
 * This module seems to be adapted from NONOS SDK by Espressif to fit into
 * RTOS SDK. Function sdk_ets_timer_handler_isr is no longer an ISR handler
 * but still holds its name. Espressif just added a task that receives events
 * from the real FRC2 timer ISR handler and calls former ISR handler.
 * So, timer callbacks are called from the task context rather than an interrupt.
 *
 * Modifications from the original reverse engineered version:
 *  - FreeRTOS queue is replaced with Task notifications.
 *  - Removed unknown queue length monitoring and parameters allocation.
 *  - Removed unused debug variables
 *  - xTaskGenericCreate is replaced with xTaskCreate
 *  - simplified time to ticks conversion (simply multiply by 5)
 *
 * This timer should be used with caution together with other tasks. As the
 * timer callback is executed within timer task context, access to data that
 * other tasks accessing should be protected.
 */
#include "open_esplibs.h"

#if OPEN_LIBMAIN_ETS_TIMER

#if 0

#include "etstimer.h"
#include "espressif/osapi.h"

void sdk_ets_timer_setfn(ETSTimer *timer, ETSTimerFunc *func, void *parg) {
    sdk_os_timer_setfn(timer, func, parg);
}

void sdk_ets_timer_arm(ETSTimer *timer, uint32_t value, bool repeat_flag) {
    sdk_os_timer_arm(timer, value, repeat_flag);
}

void sdk_ets_timer_arm_ms_us(ETSTimer *timer, uint32_t value,
                             bool repeat_flag, bool value_in_ms) {
    sdk_os_timer_arm(timer, value * 1000 + value_in_ms, repeat_flag);
}

void sdk_ets_timer_disarm(ETSTimer *timer) {
    sdk_os_timer_disarm(timer);
}

void sdk_ets_timer_init() {
}

#else

#include "open_esplibs.h"
#include <stdint.h>
#include <stdbool.h>
#include <esp/timer_regs.h>
#include <FreeRTOS.h>
#include <timers.h>
#include <queue.h>
#include <stdio.h>

typedef void ets_timer_func_t(void *);

/**
 * This structure is used for both timers: ets_timer.c and timer.c
 */
typedef struct ets_timer_st {
    struct ets_timer_st  *next;
    TimerHandle_t timer_handle;  // not used in ets_timer.c
    uint32_t fire_ticks;         // FRC2 timer value when timer should fire
    uint32_t period_ticks;       // timer value in FRC2 ticks for rpeating timers
    ets_timer_func_t *callback;
    bool repeat;                 // not used in ets_timer.c
    void *timer_arg;
} ets_timer_t;

/**
 * Special values of ets_timer_t::next field
 */
#define ETS_TIMER_NOT_ARMED (ets_timer_t*)(0xffffffff)
#define ETS_TIMER_LIST_END  (ets_timer_t*)(0)

/**
 * Linked list of timers
 */
static ets_timer_t* timer_list = 0;

static TaskHandle_t task_handle = NULL;

void sdk_ets_timer_setfn(ets_timer_t *timer, ets_timer_func_t *func, void *parg)
{
    timer->callback = func;
    timer->timer_arg = parg;
    timer->fire_ticks = 0;
    timer->period_ticks = 0;
    timer->next = ETS_TIMER_NOT_ARMED;
}

static inline void set_alarm_value(uint32_t value)
{
    TIMER_FRC2.ALARM = value;
}

/**
 * Set timer alarm and make sure the alarm is set in the future
 * and will not be missed by the timer.
 */
static void set_alarm(uint32_t ticks)
{
    uint32_t curr_time = TIMER_FRC2.COUNT;
    int32_t delta = (int32_t)ticks - curr_time;
    if ((delta - 40) < 1) {
        if (delta < 1) {
            set_alarm_value(curr_time + 40);
        } else {
            set_alarm_value(ticks + 44);
        }
    } else {
        set_alarm_value(ticks);
    }
}

/**
 *
 * Pending timer list example:
 *
 * | Timer:      | T0 | T1 | T2 | T3 |
 * |-------------|----|----|----|----|
 * | fire_ticks: | 10 | 20 | 30 | 40 |
 * | next:       | T1 | T2 | T3 | 0  |
 *
 *
 * For example we need to add a timer that should fire at 25 ticks:
 *
 * | Timer:      | T0 | T1  | new | T2 | T3 |
 * |-------------|----|-----|-----|----|----|
 * | fire_ticks: | 10 | 20  | 25  | 30 | 40 |
 * | next:       | T1 | new | T2  | T3 | 0  |
 *
 * We squeeze the timer into the list so the list will always remain sorted
 *
 * Note: if add the same timer twice the system halts
*/
static void add_pending_timer(uint32_t ticks, ets_timer_t *timer)
{
    ets_timer_t *prev = 0;
    ets_timer_t *curr = timer_list;
    while (curr) {
        if (((int32_t)ticks - (int32_t)curr->fire_ticks) < 1) {
            // found a timer that should fire later
            // so our timer should fire earlier
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    timer->next = curr;
    timer->fire_ticks = ticks;

    if (prev != 0) {
        prev->next = timer;
    } else {
        // Our timer is the first in the line to fire
        timer_list = timer;
        set_alarm(ticks);
    }

    // This situation might happen if adding the same timer twice
    if (timer == timer->next) {
        // This seems like an error: %s is used for line number
        // In the recent SDK Espressif fixed the format to "%s %u\n"
        printf("%s %s \n", "ets_timer.c", (char*)209);
        while (1);
    }
}

/**
 * In the Espressif SDK 0.9.9 if try to arm already armed timer the system halts
 * with error message. In the later SDK version Espressif changed the behavior.
 * If the timer was previously armed it is disarmed and then armed without errors.
 * This version recreates behavior of SDK 0.9.9
 */
void sdk_ets_timer_arm_ms_us(ets_timer_t *timer, uint32_t value,
        bool repeat_flag, bool value_in_ms)
{
    uint32_t ticks = 0;

    if (timer->next != ETS_TIMER_NOT_ARMED) {
        // The error message doesn't tell what is wrong
        printf("arm new %x %x\n", (uint32_t)timer, (uint32_t)timer->next);
        while(1);  // halt
    }

    if (value_in_ms) {
        ticks = value * 5000;
    } else {
        ticks = value * 5;
    }

    if (repeat_flag) {
        timer->period_ticks = ticks;
    }
    vPortEnterCritical();
    add_pending_timer(TIMER_FRC2.COUNT + ticks, timer);
    vPortExitCritical();
}

void sdk_ets_timer_arm(ets_timer_t *timer, uint32_t milliseconds,
        bool repeat_flag)
{
    sdk_ets_timer_arm_ms_us(timer, milliseconds, repeat_flag,
            /*value in ms=*/true);
}

void sdk_ets_timer_arm_us(ets_timer_t *timer, uint32_t useconds,
        bool repeat_flag)
{
    sdk_ets_timer_arm_ms_us(timer, useconds, repeat_flag,
            /*value in ms=*/false);
}

/**
 * Function removes a timer from the pending timers list.
 */
void sdk_ets_timer_disarm(ets_timer_t *timer)
{
    vPortEnterCritical();
    ets_timer_t *curr = timer_list;
    ets_timer_t *prev = 0;
    while (curr) {
        if (curr == timer) {
            if (prev) {
                prev->next = curr->next;
            } else {
                timer_list = curr->next;
            }
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    timer->next = ETS_TIMER_NOT_ARMED;
    timer->period_ticks = 0;
    vPortExitCritical();
}

/**
 * Check the list of pending timers for expired ones and process them.
 */
static inline void process_pending_timers()
{
    vPortEnterCritical();
    int32_t ticks = TIMER_FRC2.COUNT;
    while (timer_list) {
        if (((int32_t)timer_list->fire_ticks - ticks) < 1) {
            ets_timer_t *timer = timer_list;
            timer_list = timer->next;
            timer->next = ETS_TIMER_NOT_ARMED;

            vPortExitCritical();
            timer->callback(timer->timer_arg);
            vPortEnterCritical();

            if (timer->next == ETS_TIMER_NOT_ARMED) {
                if (timer->period_ticks) {
                    timer->fire_ticks = timer->fire_ticks + timer->period_ticks;
                    add_pending_timer(timer->fire_ticks, timer);
                }
            }
            ticks = TIMER_FRC2.COUNT;
        } else {
            if (timer_list) {
                set_alarm(timer_list->fire_ticks);
            }
            break;
        }
    }
    vPortExitCritical();
}

/**
 * .Lfunc002
 */
static void IRAM frc2_isr(void *arg)
{
    BaseType_t task_woken = 0;

    BaseType_t result = xTaskNotifyFromISR(task_handle, 0, eNoAction, &task_woken);
    if (result != pdTRUE) {
        printf("TIMQ_FL:%d!!", (uint32_t)result);
    }

    portEND_SWITCHING_ISR(task_woken);
}

/**
 * .Lfunc007
 *
 * Timer task
 */
static void timer_task(void* param)
{
    while (true) {
        if (xTaskNotifyWait(0, 0, NULL, portMAX_DELAY) == pdTRUE) {
            process_pending_timers();
        }
    }
}

void sdk_ets_timer_init()
{
    timer_list = 0;

    _xt_isr_attach(INUM_TIMER_FRC2, frc2_isr, NULL);

    /* Original code calls xTaskGenericCreate:
     * xTaskGenericCreate(task_handle, "rtc_timer_task", 200, 0, 12, &handle,
     *     NULL, NULL);
     */
    xTaskCreate(timer_task, "rtc_timer_task", 200, 0, 12, &task_handle);
    printf("frc2_timer_task_hdl:%p, prio:%d, stack:%d\n", task_handle, 12, 200);

    TIMER_FRC2.ALARM = 0;
    TIMER_FRC2.CTRL =  VAL2FIELD(TIMER_CTRL_CLKDIV, TIMER_CLKDIV_16)
        | TIMER_CTRL_RUN;
    TIMER_FRC2.LOAD = 0;

    DPORT.INT_ENABLE |= DPORT_INT_ENABLE_TIMER1;

   _xt_isr_unmask(BIT(INUM_TIMER_FRC2));
}

#endif

#endif /* OPEN_LIBMAIN_ETS_TIMER */
