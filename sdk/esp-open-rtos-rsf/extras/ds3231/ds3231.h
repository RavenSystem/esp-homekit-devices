/* Driver for DS3231 high precision RTC module
 *
 * Part of esp-open-rtos
 * Copyright (C) 2015 Richard A Burton <richardaburton@gmail.com>
 * Copyright (C) 2016 Bhuvanchandra DV <bhuvanchandra.dv@gmail.com>
 * MIT Licensed as described in the file LICENSE
*/

#ifndef __DS3231_H__
#define __DS3231_H__

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "i2c/i2c.h"


#ifdef	__cplusplus
extern "C" {
#endif

#define DS3231_ADDR            0x68

#define DS3231_STAT_OSCILLATOR 0x80
#define DS3231_STAT_32KHZ      0x08
#define DS3231_STAT_BUSY       0x04
#define DS3231_STAT_ALARM_2    0x02
#define DS3231_STAT_ALARM_1    0x01

#define DS3231_CTRL_OSCILLATOR    0x80
#define DS3231_CTRL_SQUAREWAVE_BB 0x40
#define DS3231_CTRL_TEMPCONV      0x20
#define DS3231_CTRL_SQWAVE_4096HZ 0x10
#define DS3231_CTRL_SQWAVE_1024HZ 0x08
#define DS3231_CTRL_SQWAVE_8192HZ 0x18
#define DS3231_CTRL_SQWAVE_1HZ    0x00
#define DS3231_CTRL_ALARM_INTS    0x04
#define DS3231_CTRL_ALARM2_INT    0x02
#define DS3231_CTRL_ALARM1_INT    0x01

#define DS3231_ALARM_WDAY   0x40
#define DS3231_ALARM_NOTSET 0x80

#define DS3231_ADDR_TIME    0x00
#define DS3231_ADDR_ALARM1  0x07
#define DS3231_ADDR_ALARM2  0x0b
#define DS3231_ADDR_CONTROL 0x0e
#define DS3231_ADDR_STATUS  0x0f
#define DS3231_ADDR_AGING   0x10
#define DS3231_ADDR_TEMP    0x11

#define DS3231_12HOUR_FLAG  0x40
#define DS3231_12HOUR_MASK  0x1f
#define DS3231_PM_FLAG      0x20
#define DS3231_MONTH_MASK   0x1f

enum {
    DS3231_SET = 0,
    DS3231_CLEAR,
    DS3231_REPLACE
};

enum {
    DS3231_ALARM_NONE = 0,
    DS3231_ALARM_1,
    DS3231_ALARM_2,
    DS3231_ALARM_BOTH
};

enum {
    DS3231_ALARM1_EVERY_SECOND = 0,
    DS3231_ALARM1_MATCH_SEC,
    DS3231_ALARM1_MATCH_SECMIN,
    DS3231_ALARM1_MATCH_SECMINHOUR,
    DS3231_ALARM1_MATCH_SECMINHOURDAY,
    DS3231_ALARM1_MATCH_SECMINHOURDATE
};

enum {
    DS3231_ALARM2_EVERY_MIN = 0,
    DS3231_ALARM2_MATCH_MIN,
    DS3231_ALARM2_MATCH_MINHOUR,
    DS3231_ALARM2_MATCH_MINHOURDAY,
    DS3231_ALARM2_MATCH_MINHOURDATE
};

/* Set the time on the rtc
 * timezone agnostic, pass whatever you like
 * I suggest using GMT and applying timezone and DST when read back
 * returns true to indicate success
 */
int ds3231_setTime(i2c_dev_t *dev, struct tm *time);

/* Set alarms
 * alarm1 works with seconds, minutes, hours and day of week/month, or fires every second
 * alarm2 works with minutes, hours and day of week/month, or fires every minute
 * not all combinations are supported, see DS3231_ALARM1_* and DS3231_ALARM2_* defines
 * for valid options you only need to populate the fields you are using in the tm struct,
 * and you can set both alarms at the same time (pass DS3231_ALARM_1/DS3231_ALARM_2/DS3231_ALARM_BOTH)
 * if only setting one alarm just pass 0 for tm struct and option field for the other alarm
 * if using DS3231_ALARM1_EVERY_SECOND/DS3231_ALARM2_EVERY_MIN you can pass 0 for tm stuct
 * if you want to enable interrupts for the alarms you need to do that separately
 * returns true to indicate success
 */
int ds3231_setAlarm(i2c_dev_t *dev, uint8_t alarms, struct tm *time1, uint8_t option1, struct tm *time2, uint8_t option2);

/* Check if oscillator has previously stopped, e.g. no power/battery or disabled
 * sets flag to true if there has been a stop
 * returns true to indicate success
 */
bool ds3231_getOscillatorStopFlag(i2c_dev_t *dev, bool *flag);

/* Clear the oscillator stopped flag
 * returns true to indicate success
 */
bool ds3231_clearOscillatorStopFlag(i2c_dev_t *dev);

/* Check which alarm(s) have past
 * sets alarms to DS3231_ALARM_NONE/DS3231_ALARM_1/DS3231_ALARM_2/DS3231_ALARM_BOTH
 * returns true to indicate success
 */
bool ds3231_getAlarmFlags(i2c_dev_t *dev, uint8_t *alarms);

/* Clear alarm past flag(s)
 * pass DS3231_ALARM_1/DS3231_ALARM_2/DS3231_ALARM_BOTH
 * returns true to indicate success
 */
bool ds3231_clearAlarmFlags(i2c_dev_t *dev, uint8_t alarm);

/* enable alarm interrupts (and disables squarewave)
 * pass DS3231_ALARM_1/DS3231_ALARM_2/DS3231_ALARM_BOTH
 * if you set only one alarm the status of the other is not changed
 * you must also clear any alarm past flag(s) for alarms with
 * interrupt enabled, else it will trigger immediately
 * returns true to indicate success
 */
bool ds3231_enableAlarmInts(i2c_dev_t *dev, uint8_t alarms);

/* Disable alarm interrupts (does not (re-)enable squarewave)
 * pass DS3231_ALARM_1/DS3231_ALARM_2/DS3231_ALARM_BOTH
 * returns true to indicate success
 */
bool ds3231_disableAlarmInts(i2c_dev_t *dev, uint8_t alarms);

/* Enable the output of 32khz signal
 * returns true to indicate success
 */
bool ds3231_enable32khz(i2c_dev_t *dev);

/* Disable the output of 32khz signal
 * returns true to indicate success
 */
bool ds3231_disable32khz(i2c_dev_t *dev);

/* Enable the squarewave output (disables alarm interrupt functionality)
 * returns true to indicate success
 */
bool ds3231_enableSquarewave(i2c_dev_t *dev);

/* Disable the squarewave output (which re-enables alarm interrupts, but individual
 * alarm interrupts also need to be enabled, if not already, before they will trigger)
 * returns true to indicate success
 */
bool ds3231_disableSquarewave(i2c_dev_t *dev);

/* Set the frequency of the squarewave output (but does not enable it)
 * pass DS3231_SQUAREWAVE_RATE_1HZ/DS3231_SQUAREWAVE_RATE_1024HZ/DS3231_SQUAREWAVE_RATE_4096HZ/DS3231_SQUAREWAVE_RATE_8192HZ
 * returns true to indicate success
 */
bool ds3231_setSquarewaveFreq(i2c_dev_t *dev, uint8_t freq);

/* Get the raw value
 * returns true to indicate success
 */
bool ds3231_getRawTemp(i2c_dev_t *dev, int16_t *temp);

/* Get the temperature as an integer
 * returns true to indicate success
 */
bool ds3231_getTempInteger(i2c_dev_t *dev, int8_t *temp);

/* Get the temerapture as a float (in quarter degree increments)
 * returns true to indicate success
 */
bool ds3231_getTempFloat(i2c_dev_t *dev, float *temp);

/* Get the time from the rtc, populates a supplied tm struct
 * returns true to indicate success
 */
bool ds3231_getTime(i2c_dev_t *dev, struct tm *time);

#ifdef	__cplusplus
}
#endif

#endif  /* __DS3231_H__ */
