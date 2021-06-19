/************************************************************************//**
 * RTC time keeping and synchronization using SNTP for esp-open-rtos.
 * Uses SNTP lwIP contribution.
 *
 * 2016, Jesus Alonso (doragasu)
 ****************************************************************************/

#ifndef _SNTP_H_
#define _SNTP_H_

#include <stdint.h>
#include <sys/time.h>
#include <time.h>

/*
 * Function used by lwIP sntp module to update the date/time,
 * with microseconds resolution.
 */
#define SNTP_SET_SYSTEM_TIME_US(sec, us) sntp_update_rtc(sec, us)

/*
 * For the lwIP implementation of SNTP to allow using names for NTP servers.
 */
#define SNTP_SERVER_DNS             1

/*
 * Number of supported NTP servers
 */
#define SNTP_NUM_SERVERS_SUPPORTED 	4

/*
 * Initialize the module, and start requesting SNTP updates. This function
 * must be called only once.
 * WARNING: tz->tz_dsttime doesn't have the same meaning as the standard 
 * implementation. If it is set to 1, a dst hour will be applied. If set
 * to zero, time will not be modified.
 */
void sntp_initialize(const struct timezone *tz);

/*
 * Sets time zone. Allowed values are in the range [-11, 13].
 * NOTE: Settings do not take effect until SNTP time is updated. It is
 * recommended to set these parameters only during initialization.
 * WARNING: tz->tz_dsttime doesn't have the same meaning as the standard 
 * implementation. If it is set to 1, a dst hour will be applied. If set
 * to zero, time will not be modified.
 */
void sntp_set_timezone(const struct timezone *tz);

/*
 * Set SNTP servers. Up to SNTP_NUM_SERVERS_SUPPORTED can be set.
 * Returns 0 if OK, less than 0 if error.
 * NOTE: This function must NOT be called before sntp_initialize().
 */
int sntp_set_servers(const char *server_url[], int num_servers);

/*
 * Sets the update delay in ms. If requested value is less than 15s,
 * a 15s update interval will be set.
 */
void sntp_set_update_delay(uint32_t ms);

/*
 * Returns the time read from RTC counter, in seconds from Epoch. If
 * us is not null, it will be filled with the microseconds.
 */
time_t sntp_get_rtc_time(int32_t *us);

/*
 * Update RTC timer. This function is called by the SNTP module each time
 * an SNTP update is received.
 */
void sntp_update_rtc(time_t t, uint32_t us);

#endif /* _SNTP_H_ */

