# Overview
`timekeeping` provides an implementation of a clock that can provide monotonic time with microsecond resolution and supports many of the common time-of-day functions in a POSIX-like manner through `gettimeofday()`. It does not supply a clock *discipline*, such as NTP or SNTP, but does implement `settimeofday()` and `adjtime()` to allow implementation of clock discipline. 

The system clock is used to as the time reference. Time is available from boot or wake, referenced to the system clock's "zero" until `settimeofday()` is called. 

Timezone functionality is provided through `tzset()`, with the assumption that the `timekeeping` internal clock is set to UTC. 

**Important Note**

This code does not alter OS-level behavior related to "ticks". For example, `vTaskDelay(const TickType_t xTicksToDelay)` will continue to result in a delay of `xTicksToDelay` *ticks,* which may not be the same length of time as indicated by calls to `gettimeofday()` or related functions.

# Supported Functionality
## Standard C Library Calls
### Implemented or Updated

     #include <sys/time.h>

     int
     gettimeofday(struct timeval *restrict tp, void *restrict tzp);

     int
     settimeofday(const struct timeval *tp, const struct timezone *tzp);
          
     int
     adjtime(const struct timeval *delta, struct timeval *olddelta);

### Existing, Unmodified
     
     #include <stdlib.h>
     
     int
     setenv(const char *name, const char *value, int overwrite);     
<br/>

     #include <time.h>

     void
     tzset(void);

Note that POSIX-style clock selection and timers have not been modified at this time.

## Implementation-specific Details

See [notes below](#managing-system-clock-wrap) on the need to call `gettimeofday()` or `adjtime()` at least once an hour to detect and compensate for clock "wrap".

Note that, in keeping with current practice, calls to `settimeofday()` and `gettimeofday()` do not utilize incoming values in the timezone argument to modify the value referenced by the time argument. In this implementation, the time argument for `gettimeofday()` and `settimeofday()` is *always* relative to the internal clock's datum (assumed by this implementation to be UTC). 

With the prototypes in `<sys/time.h>`, it is either not possible (`const`) or unsafe (`void`) to modify the timezone argument. As a result, this implementation ignores any value passes in the timezone argument entirely. This implementation does not consider it an error to pass a non-null value for the timezone argument.

It is suggested that time-critical functions be executed from high-priority threads to reduce the likelihood of timing errors that may occur if the thread is swapped out of execution during the calls. 

# Examples

## Set Time of Day (UTC)

    struct timeval tv;
    tv->tv_sec = 1518798027;  /* 2018-02-16T16:20:27+00:00 */
    tv->tv_usec = 0;
    settimeofday(&tv, NULL);
## Get Time of Day (UTC)
    struct timeval tv;
    gettimeofday(&tv, NULL);
## Slew Time
    struct timeval tv;
    tv->tv_sec = 0;
    tv->tv_usec = -50 * 1000;  /* -50 ms */
    adjtime(&tv, NULL);
## Set Local Time Zone to US Pacific
    setenv("TZ", "PST8PDT7,M3.1.0,M11.1.0", 1);
    tzset();
## Set Local Time Zone to UTC
    setenv("TZ", "UTC0UTC0", 1);
    tzset();
    

# Timezone Management

As newlib is typically compiled and supplied with for esp-open-rtos, the timezone can be managed through `setenv()` and `tzset()` using POSIX-style timezone strings.

For example

    setenv("TZ", "PST8PDT7,M3.1.0,M11.1.0", 1);
    tzset();

will set United States' Pacific Time rules (as of 2018) for both standard and daylight savings time. As only two rules are implemented in newlib, calculations across changes in timezone rules, or localities that have more than two changes per year are not directly supported by newlib itself.

Although the source code of `setenv()` appears to call `tzset()` when `TZ` is set, the combination of empirical experience and the POSIX description of `tzset()`
> However, portable applications should call `tzset()` explicitly before using `ctime_r()` or `localtime_r()` because setting timezone information is optional for those functions.

strongly suggest an explicit call to `tzset()` after changing `TZ`.

Note that `unsetenv("TZ")` will not "reset" all the internal variables related to timezone implementation. One approach to setting a zero-offset timezone is

    setenv("TZ", "UTC0UTC0");
    tzset();

While the timezone rules will still be populated, the global `_daylight` is set to `0` (`DST_NONE`) so that the rules should not be consulted by "compliant" code. Further, the offset in the two rules is set to `0` so that even if the rules are consulted by other code that does not respect the `_daylight` setting there is no offset applied.

Direct manipulation of the timezone-related variables is not recommended due to multi-threading issues.

# Implementation Approach
## Hardware Clock
The system clock is used as the underlying clock. A discussion of why the ESP8266 RTC is *not* used may be found in an [appendix](#why-not-the-rtc). Espressif specifies a 15-ppm or better crystal, so the system clock should be accurate to better than 1 ms per minute, or 1.3 seconds per day. For comparison, color-burst crystals are typically in the 30-100 ppm range.

The system clock is a 32-bit value with one-microsecond resolution. As a result, it will "wrap" every hour and a few minutes. This 32-bit limitation is accommodated for in software. The system clock is allowed to free run; all compensation and adjustments are done in software. 

## Hardware Clock to Internal Clock Conversion

The "internal clock" is the system's estimate of microseconds since the epoch. As the ESP8266 is a self-contained system and the only consumer of the clock, there is no implementation of a "wall-time CMOS" functionality. 

The value of the internal clock is referred to as `internal_clock` and `system_clock` refers to the current value that would be read using `sdk_system_get_time()`. 

<a name="without-adjtime-slew-in-process"></a>
### Without `adjtime()` Slew in Process
When there is not a pending slew from a call to `adjtime()`, the internal clock is calculated as

    internal_clock = system_clock + clock_offset
    
`clock_offset` is a 64-bit, signed integer in units of microseconds and should not overflow until well past the useful lifespan of the ESP8266. `clock_offset` includes the aggregated offset specified by calls to `settimeofday()`, `adjtime()`, as well as the overflow from the [system clock wrapping](#managing-system-clock-wrap).

### `adjtime()` Implementation

`adjtime()` is generally used after the initial clock set to slowly slew the time, rather than step it. The reference implementation of NTPv4 limits its slew rate requests to 500 us/s (500 ppm). This rate of slew should be sufficient for the crystal-controlled system clock of the ESP8266.

The slew rate of the current implementation is fixed 500 us/s by the macro `ADJTIME_SLEW_PERIOD`, measured in units of us of elapsed time per us of slew. By defining it in this way, integer arithmetic can be used, providing significant speed advantages over floating-point calculations.

When a call to `adjtime()` is made, the current time is captured in `slew_start_time`. The elapsed time in microseconds to slew the requested amount is calculated, added to the current `internal_clock`, and saved as `slew_complete_time`. Until `system_clock + clock_offset` passes `slew_complete_time` there is slew in process and the internal clock is calculated as

    internal_clock = (system_clock + clock_offset) 
                     + ( (system_clock + clock_offset)
                        - slew_start_time ) 
                       / SIGNED_ADJTIME_SLEW_PERIOD

where `SIGNED_ADJTIME_SLEW_PERIOD` adopts the sign of the requested `delta`.   Use of the un-slewed clock is intentional as it simplifies calculations.

Once the slew is complete, the amount of slew is added to `clock_offset` and calculation of `internal_clock` resumes as described in the [previous section](#without-adjtime-slew-in-process). See [*Managing System-clock Wrap*](#managing-system-clock-wrap) for how and when this adjustment is made.

Note that `settimeofday()` is implemented as a "hard set" of the internal clock and will abort any in-progress clock slew.

Any remaining slew from prior calls to `adjtime()` can be returned by the call in its second argument, *but are overridden by the new value, not added to them.* The remaining slew is calculated as

    olddelta_in_us = (slew_complete_time - (system_clock + clock_offset)) 
                     / SIGNED_ADJTIME_SLEW_PERIOD

and returned in `struct timeval *olddelta` in normalized form. `adjtime()` can be called with a NULL `delta` and will not modify the slew in that case.

Note that the magnitude of `delta` cannot exceed `ADJTIME_MAX_SECS_ALLOWED`, presently defined as 2000 (seconds). This is significantly greater than the 128-ms limit within the NTPv4 reference implementation, small enough to prevent overflow of the 32-bit `adjtime_delta` internal, and hopefully larger than any rational use of `adjtime()`.

<a name="managing-system-clock-wrap"></a>
### Managing System-clock Wrap
As previously discussed, the system clock wraps in a little bit more than hour (2<sup>32</sup> microseconds, ~ 71 minutes). This needs to be detected and accounted for prior to any call to obtain internal time, as well as before it wraps a second time. the internal `_check_system_clock()` manages this, as well as the incorporation of completed slew into `clock_offset`. It is called within the implementations of `settimeofday()`, `adjtime()`, and `gettimeofday()`.

`_check_system_clock()` operates by recording the last value of the system clock and comparing it to the current value. If the current value is less than the last value, then it is assumed that a single wrap has occurred and the value of `clock_offset` is increased by 2<sup>32</sup>. It also checks to see if there is a completed clock slew. If so, adds the (signed) value of the slew in microseconds to `clock_offset` and resets the internal state variables associated with slew.

In many situations either or both `adjtime()` or `getttimeofday()` are called at least once an hour. If this is not the case, `gettimeofday(NULL, NULL)` should be called once an hour, using a repeating timer or other appropriate method. 


# License

All files in this directory

	   Copyright (c) 2018, Jeff Kletsky
	   All rights reserved.
	  
	   Redistribution and use in source and binary forms, with or without
	   modification, are permitted provided that the following conditions are met:
	   
	       * Redistributions of source code must retain the above copyright
	         notice, this list of conditions and the following disclaimer.
	  
	       * Redistributions in binary form must reproduce the above copyright
	         notice, this list of conditions and the following disclaimer in the
	         documentation and/or other materials provided with the distribution.
	  
	       * Neither the name of the copyright holder nor the
	         names of its contributors may be used to endorse or promote products
	         derived from this software without specific prior written permission.
	   
	   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
	   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
	   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
	   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
	   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
	   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
	  
Some files may have additional copyright and/or licenses. Consult those files for details. At a minimum, this includes:

*  `lwipopts.h` -- Copyright (c) 2001-2003 Swedish Institute of Computer Science.

# Appendix
<a name="why-not-the-rtc"></a>
## Why Not the RTC?

While the ESP8266 RTC seems a plausible choice for time keeping, it has several chip-level implementation details that make it less desirable than the system clock.

One issue is resolution. The RTC has only a nominal 6-ms resolution, compared to the 1-ms resolution of the system clock. 

A second issue is stability. It is believed that the ESP8266 RTC is a simple RC oscillator. As such, it is likely to be highly temperature sensitive. 

Accuracy is another concern. While the SDK can provide an estimate of the RTC's period, it is likely measured with respect to the system clock and, as such, can be no better than the accuracy of the system clock. Further, it is a very noisy estimate, with values showing short-term standard deviation in the 500-1000 ppm range, compared to the system clock crystal which is spec-ed by Espressif to be 15 ppm or better. As the system clock crystal is likely used to derive the RF, units that have been FCC certified are likely to meet this specification or better (10 ppm is a common value).

Finally, according to the Espressif SDK documentation, the RTC is reset under many situations that one would have hoped a "true" RTC would maintain time keeping. It notes that "CHIP_EN (including the deep-sleep wakeup)" results in "RTC memory is random value, RTC timer starts from zero". 

While the RTC's longer period means that counter wrap occurs less frequently (~7 hours for the RTC, compared to a little over an hour for the system clock), this is not deemed a significant advantage.

As a result, the system clock (or an external RTC) is preferred over the ESP8266 internal RTC.

Normal memory is used for all state variables, rather than the internal RTC memory. If clock-state information needs to be preserved during sleep, it can be obtained through `gettimeofday(&tv, NULL)` and, if desired, any outstanding slew obtained through `adjtime(NULL, &olddelta)`. No accessors to internal `timekeeping` data are believed required.