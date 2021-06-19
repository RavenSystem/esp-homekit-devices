/* Structures and constants used by some SDK routines
 *
 * Part of esp-open-rtos
 * Copyright (C) 2015 Superhouse Automation Pty Ltd
 * BSD Licensed as described in the file LICENSE
 */

/* Note: The following definitions are normally found (in the non-RTOS SDK) in
 * the ets_sys.h distributed by Espressif.  Unfortunately, they are not found
 * anywhere in the RTOS SDK headers, and differ substantially from the non-RTOS
 * versions, so the structures defined here had to be obtained by careful
 * examination of the code found in the Espressif RTOS SDK.
 */

/* Note also: These cannot be included in esp8266/ets_sys.h, because it is
 * included from FreeRTOS.h, creating an (unnecessary) circular dependency.
 * They have therefore been put into their own header file instead.
 */

#ifndef _ETSTIMER_H
#define _ETSTIMER_H

#include "FreeRTOS.h"
#include "timers.h"
#include "esp/types.h"

typedef void ETSTimerFunc(void *);

typedef struct ETSTimer_st {
    struct ETSTimer_st  *timer_next;
    TimerHandle_t timer_handle;
    uint32_t _unknown;
    uint32_t timer_ms;
    ETSTimerFunc *timer_func;
    bool timer_repeat;
    void *timer_arg;
} ETSTimer;

void sdk_ets_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg);
void sdk_ets_timer_arm(ETSTimer *ptimer, uint32_t milliseconds, bool repeat_flag);
void sdk_ets_timer_disarm(ETSTimer *ptimer);

#endif /* _ETSTIMER_H */
