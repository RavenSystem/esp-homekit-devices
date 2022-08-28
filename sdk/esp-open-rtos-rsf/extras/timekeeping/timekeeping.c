/*
 * Basic timekeeping functions
 *
 * Independent of clock discipline
 */

/*-
 * Copyright (c) 2018, Jeff Kletsky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/reent.h>
#include <sys/time.h>
#include <sys/errno.h>

/* espressif/esp_system.h uses "bool" but fails to #include */
#include <stdbool.h>
#include <espressif/esp_system.h>

#include <time.h>


/* #define here for easier use of other system clocks */
#define GET_SYSTEM_CLOCK_US() sdk_system_get_time()

typedef uint32_t system_clock_t;
#define SYSTEM_CLOCK_WRAP_US 0x100000000LL

/* 
 * Define the slew rate in terms of microseconds of time to slew 1 microsecond
 * This allows the use of integer arithmetic
 * 2000 us to slew 1 us is 500 us of slew per sec, consistent with NTP usage
 */
#define ADJTIME_SLEW_PERIOD 2000

/* 
 * Try to prevent gross errors in calls to adjtime(),
 * such as calling with the desired time
 * NTP typically won't try to slew more than 128 ms
 * At 500 ppm, can slew 1.8 seconds/hour
 * A little less than what an int32_t can represent should be more than enough
 * (2**31)/1e6 < 2147
 */
#define ADJTIME_MAX_SECS_ALLOWED 2000


/* 
 * Assume that newlib was compiled to implement tz locking
 * See newlib-xtensa/newlib/libc/time/local.h
 * 
 * __tz_lock is implemented by newlib and calls
 * __tz_lock() calls __lock_acquire() which calls
 * xSemaphoreTake() -- NOT RECURSIVE
 * Use of __tz_lock() in applications may result in deadlock
 *
 * As that local.h is not available within esp-open-rtos
 * replicate the definitions and provide proto here
 */
#define TZ_LOCK     __tz_lock()
#define TZ_UNLOCK   __tz_unlock()

extern void __tz_lock(void);
extern void __tz_unlock(void);


/*
 * Multi-threading considerations
 * 
 * While the calls related to timekeeping are generally "tight"
 * there is the possiblity that the calling process will be swapped out.
 * Selection of proper task priority should mitigate this
 * 
 * taskENTER_CRITICAL() and taskEXIT_CRITICAL() might be useful
 * for specialized requirements. Before considering their use
 * determine if configMAX_SYSCALL_INTERRUPT_PRIORITY is enabled
 * and is at a level that permits at least ticks.
 * See also configMAX_API_CALL_INTERRUPT_PRIORITY
 * 
 * TIMEKEEPING_LOCK_USE_CRITICAL doesn't make much sense
 * if it prevents interrupts related to ticks!
 * 
 * TIMEKEEPING_LOCK_USE_CRITICAL is UNTESTED
 */

#ifdef TIMEKEEPING_LOCK_USE_CRITICAL
#define TIMEKEEPING_LOCK()   do {TZ_LOCK; taskENTER_CRITICAL();} while (0)
#define TIMEKEEPING_UNLOCK() do {taskEXIT_CRITICAL(); TZ_UNLOCK;} while (0)
#else
#define TIMEKEEPING_LOCK()   TZ_LOCK
#define TIMEKEEPING_UNLOCK() TZ_UNLOCK
#endif

#define SIGNED_ADJTIME_SLEW_PERIOD (timekeeping_state.adjtime_delta < 0 \
                                    ? -ADJTIME_SLEW_PERIOD : ADJTIME_SLEW_PERIOD)

#define UNUSED_PARAM(X) ((void)X)

/* 
 * All units in timekeeping_state are microseconds
 * Intentionally *signed* values as, for example, might want to reset to zero
 * for a time other than when the system clock started ticking
 */
static struct
{
    int64_t         clock_offset;
    system_clock_t  last_system_clock_value;
    int32_t         adjtime_delta;
    int64_t         slew_start_time;
    int64_t         slew_complete_time;
} timekeeping_state;


static inline void
_reset_adjtime_state() {

    /* This should only be called under TIMEKEEPING_LOCK */

    timekeeping_state.adjtime_delta = 0;
    timekeeping_state.slew_start_time = 0;
    timekeeping_state.slew_complete_time = 0;
}

static void
_check_system_clock(void) {

    /* This should only be called under TIMEKEEPING_LOCK */

    system_clock_t current_system_clock;

    current_system_clock = GET_SYSTEM_CLOCK_US();

    if (current_system_clock < timekeeping_state.last_system_clock_value) {
        timekeeping_state.clock_offset =
                timekeeping_state.clock_offset + SYSTEM_CLOCK_WRAP_US;
    }

    timekeeping_state.last_system_clock_value = current_system_clock;

    if (timekeeping_state.slew_complete_time
        && ((GET_SYSTEM_CLOCK_US() + timekeeping_state.clock_offset)
            >= timekeeping_state.slew_complete_time)) {
        timekeeping_state.clock_offset =
                timekeeping_state.clock_offset + timekeeping_state.adjtime_delta;
        _reset_adjtime_state();
    }
}


int
_settimeofday_r(struct _reent *r, const struct timeval *tv, const struct timezone *tz) {

    UNUSED_PARAM(tz);

    int retval = 0;
    system_clock_t current_system_clock;
    int64_t desired_internal_clock;

    /* Effectively a system call, be picky */
    if (tv && (tv->tv_usec < 0 || tv->tv_usec >= 1000000 || tv->tv_sec < 0)) {
       retval = -1;
        r->_errno = EINVAL;
    }

    TIMEKEEPING_LOCK();

    _check_system_clock();

    if (tv && !retval) {

        current_system_clock = GET_SYSTEM_CLOCK_US();
        desired_internal_clock = ((int64_t) tv->tv_sec * 1000000) + tv->tv_usec;

        timekeeping_state.clock_offset =
                desired_internal_clock - current_system_clock;

        _reset_adjtime_state();

    }

    TIMEKEEPING_UNLOCK();

    return (retval);
}


/* "Override" the newlib definition used for gettimeofday and variants */
int
_gettimeofday_r(struct _reent *r, struct timeval *tv, void *tz) {

    UNUSED_PARAM(r);
    UNUSED_PARAM(tz);

    system_clock_t current_system_clock;
    int64_t        system_plus_offset;
    int64_t        internal_clock;

    current_system_clock = GET_SYSTEM_CLOCK_US();

    TIMEKEEPING_LOCK();

    _check_system_clock();

    if (tv) {

        system_plus_offset =
                current_system_clock + timekeeping_state.clock_offset;
        if (!timekeeping_state.slew_complete_time) {
            internal_clock = system_plus_offset;
        } else {
            internal_clock =
                    (int64_t)(system_plus_offset
                              + (system_plus_offset
                                 - timekeeping_state.slew_start_time)
                                / SIGNED_ADJTIME_SLEW_PERIOD);
        }

        tv->tv_sec = internal_clock  / 1000000;
        tv->tv_usec = internal_clock % 1000000;

    }

    TIMEKEEPING_UNLOCK();

    return (0);
}


int
_adjtime_r(struct _reent *r, const struct timeval *delta, struct timeval *olddelta) {

    int retval = 0;
    system_clock_t current_system_clock;
    int64_t        system_plus_offset;
    int32_t        slew;

    current_system_clock = GET_SYSTEM_CLOCK_US();

    /* Effectively a system call, be picky */
    if (delta && (delta->tv_usec <= -1000000 || delta->tv_usec >= 1000000
                  || delta->tv_sec > ADJTIME_MAX_SECS_ALLOWED
                  || delta->tv_sec < -ADJTIME_MAX_SECS_ALLOWED)) {
        retval = -1;
        r->_errno = EINVAL;
    }

    TIMEKEEPING_LOCK();

    _check_system_clock();

    if (!retval) {

        system_plus_offset =
                current_system_clock + timekeeping_state.clock_offset;

        if (olddelta) {
            if (timekeeping_state.slew_complete_time) {
                slew = (int32_t)((timekeeping_state.slew_complete_time
                                  - system_plus_offset)
                                 / SIGNED_ADJTIME_SLEW_PERIOD);
                olddelta->tv_sec = slew  / 1000000;
                olddelta->tv_usec = slew % 1000000;
            } else {
                olddelta->tv_sec = 0;
                olddelta->tv_usec = 0;
            }
        }

        if (delta) {
            timekeeping_state.adjtime_delta = delta->tv_sec * 1000000 + delta->tv_usec;
            timekeeping_state.slew_start_time = system_plus_offset;
            timekeeping_state.slew_complete_time =
                    timekeeping_state.slew_start_time
                    + (int64_t) timekeeping_state.adjtime_delta
                      * SIGNED_ADJTIME_SLEW_PERIOD;
        }

    }  /* if (!retval) */

    TIMEKEEPING_UNLOCK();

    return (retval);
}



int
settimeofday(const struct timeval *tv, const struct timezone *tz) {

    return _settimeofday_r(_REENT, tv, tz);
}

int
adjtime(const struct timeval *delta, struct timeval *olddelta) {

    return _adjtime_r(_REENT, delta, olddelta);
}
