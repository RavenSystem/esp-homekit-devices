/*
 * Auxiliary functions to handle date/time along with lwIP sntp implementation.
 *
 * Jesus Alonso (doragasu)
 */

#include <sys/reent.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <stdio.h>
#include <sys/time.h>
#include <espressif/esp_common.h>
#include <esp/timer.h>
#include <esp/rtc_regs.h>
#include <sntp.h>


#ifdef  SNTP_LOGD_WITH_PRINTF
#define SNTP_LOGD(FMT, ...) printf(FMT "\n", ##__VA_ARGS__)
#endif

#ifndef SNTP_LOGD
#define SNTP_LOGD(...)
#define SKIP_DIAGNOSTICS
#endif

#define TIMER_COUNT			RTC.COUNTER

// daylight settings
// Base calculated with value obtained from NTP server (64 bits)
#define sntp_base	(*((uint64_t*)RTC.SCRATCH))
// Timer value when base was obtained
#define tim_ref 	(RTC.SCRATCH[2])
// Calibration value -- ( microseconds / RTC tick ) * 2^12
#define cal 		(RTC.SCRATCH[3])

#ifndef SKIP_DIAGNOSTICS
// Keep the last time SNTP updated the time
static struct timeval last_update_time = {0, 0};
#endif

// Timezone related data.
static struct timezone stz;

// Implemented in sntp.c
void sntp_init(void);

// Sets time zone.
// NOTE: Settings do not take effect until SNTP time is updated.
void sntp_set_timezone(const struct timezone *tz) {
	if (tz) {
		stz = *tz;
	} else {
		stz.tz_minuteswest = 0;
		stz.tz_dsttime = 0;
	}
}

// Initialization
void sntp_initialize(const struct timezone *tz) {
	if (tz) {
		stz = *tz;
	} else {
		stz.tz_minuteswest = 0;
		stz.tz_dsttime = 0;
	}
	sntp_base = 0;
	// To avoid div by 0 exceptions if requesting time before SNTP config
	cal = sdk_system_rtc_clock_cali_proc();
	tim_ref = TIMER_COUNT;
	sntp_init();
}

// Check if a timer wrap has occurred. Compensate sntp_base reference
// if affirmative.
// TODO: think about multitasking and race conditions
static inline void sntp_check_timer_wrap(uint32_t current_value) {
	if (current_value < tim_ref) {
		// Timer wrap has occurred, compensate by subtracting 2^32 to ref.
		sntp_base -= 1LLU<<32;
		// DEBUG
		SNTP_LOGD("SNTP RTC counter wrapped");
	}
}

// Return secs. If us is not a null pointer, fill it with usecs
inline time_t sntp_get_rtc_time(int32_t *us) {
	time_t secs;
	uint32_t tim;
	uint64_t base;

	tim = TIMER_COUNT;
	// Check for timer wrap
	sntp_check_timer_wrap(tim);
	base = sntp_base + tim - tim_ref;
	secs = (base * cal) / (1000000U<<12);
	if (us) {
		*us = ((base * cal) % (1000000U<<12)) >>12;
	}
	return secs;
}

// Syscall implementation. doesn't seem to use tzp.
int _gettimeofday_r(struct _reent *r, struct timeval *tp, void *tzp) {
	(void)r;
	// Syscall defined by xtensa newlib defines tzp as void*
	// So it looks like it is not used. Also check tp is not NULL
	if (tzp || !tp) return EINVAL;

	tp->tv_sec = sntp_get_rtc_time((int32_t*)&tp->tv_usec);
	return 0;
}

// Update RTC timer. Called by SNTP module each time it receives an update.
void sntp_update_rtc(time_t t, uint32_t us) {

    uint32_t now_rtc = TIMER_COUNT;

    // Apply daylight and timezone correction
    t += (stz.tz_minuteswest + stz.tz_dsttime * 60) * 60;

#ifndef SKIP_DIAGNOSTICS

    int64_t sntp_reference_time, local_clock_time, clock_difference;
    struct timeval this_update_time, elapsed_since_update;
    double ppm;

    // Calculate in diagnostics in microseconds
    sntp_reference_time = (uint64_t)us + (uint64_t)t * 1000000U;
    local_clock_time = ((sntp_base + now_rtc - tim_ref) * cal) / (1U<<12);
    clock_difference = sntp_reference_time - local_clock_time;

    this_update_time.tv_sec  = t;
    this_update_time.tv_usec = us;

    timersub(&this_update_time, &last_update_time, &elapsed_since_update);

    // If over a day since last update, don't trust the last_update_time
    if (elapsed_since_update.tv_sec < 24 * 60 * 60 ) {
        ppm = ((double)clock_difference /
               ((double)elapsed_since_update.tv_sec * 1000000
                        + elapsed_since_update.tv_usec))
              * 1000000;
        SNTP_LOGD("SNTP RTC adjust: %0.3Lf s; %0.3f ppm over %0.3f s; cal: %u\n",
                  ((long double)clock_difference)/1000000,
                  ppm,
                  (double)elapsed_since_update.tv_sec
                  + ((double)elapsed_since_update.tv_usec)/1000000,
                  cal);
    } else {
        SNTP_LOGD("SNTP RTC adjust: %0.3Lf s; cal: %u\n",
                  ((long double)clock_difference)/1000000, cal);
    }

    last_update_time.tv_sec  = t;
    last_update_time.tv_usec = us;

#endif // SKIP_DIAGNOSTICS

	cal = sdk_system_rtc_clock_cali_proc();
    tim_ref = now_rtc;
	sntp_base = (((uint64_t)us + (uint64_t)t * 1000000U) <<12) / cal;

}

