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

typedef void mactimer_func_t(void *);

typedef struct mactimer_st {
    struct mactimer_st  *next;
    mactimer_func_t *callback;
    uint64_t trigger_usec;
    void *timer_arg;
} mactimer_t;

void mactimer_setfn(mactimer_t *timer, mactimer_func_t *func, void *parg);
uint64_t mactime_get_count(void);
void mactime_add_pending(mactimer_t *timer, uint64_t count);
void mactimer_arm(mactimer_t *timer, uint64_t count);
void mactimer_disarm(mactimer_t *timer);
void mactimer_init(void);
