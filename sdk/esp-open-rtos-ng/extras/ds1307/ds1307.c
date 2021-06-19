/*
 * Driver for DS1307 RTC
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include "ds1307.h"
#include <stdio.h>

#define RAM_SIZE 56

#define TIME_REG    0
#define CONTROL_REG 7
#define RAM_REG     8

#define CH_BIT      (1 << 7)
#define HOUR12_BIT  (1 << 6)
#define PM_BIT      (1 << 5)
#define SQWE_BIT    (1 << 4)
#define OUT_BIT     (1 << 7)

#define CH_MASK      0x7f
#define SECONDS_MASK 0x7f
#define HOUR12_MASK  0x1f
#define HOUR24_MASK  0x3f
#define SQWEF_MASK   0x03
#define SQWE_MASK    0xef
#define OUT_MASK     0x7f

static uint8_t bcd2dec(uint8_t val)
{
    return (val >> 4) * 10 + (val & 0x0f);
}

static uint8_t dec2bcd(uint8_t val)
{
    return ((val / 10) << 4) + (val % 10);
}

static uint8_t read_register(i2c_dev_t *dev, uint8_t reg)
{
    uint8_t val;
    i2c_slave_read(dev->bus, dev->addr, &reg, &val, 1);
    return val;
}

static void update_register(i2c_dev_t *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
    uint8_t buf = (read_register(dev, reg) & mask) | val;

    i2c_slave_write(dev->bus, dev->addr, &reg, &buf, 1);
}

void ds1307_start(i2c_dev_t *dev, bool start)
{
    update_register(dev, TIME_REG, CH_MASK, start ? 0 : CH_BIT);
}

bool ds1307_is_running(i2c_dev_t *dev)
{
    return !(read_register(dev, TIME_REG) & CH_BIT);
}

void ds1307_get_time(i2c_dev_t *dev, struct tm *time)
{
    uint8_t buf[7];
    uint8_t reg = TIME_REG;

    i2c_slave_read(dev->bus, dev->addr, &reg, buf, 7);

    time->tm_sec = bcd2dec(buf[0] & SECONDS_MASK);
    time->tm_min = bcd2dec(buf[1]);
    if (buf[2] & HOUR12_BIT)
    {
        // RTC in 12-hour mode
        time->tm_hour = bcd2dec(buf[2] & HOUR12_MASK) - 1;
        if (buf[2] & PM_BIT)
            time->tm_hour += 12;
    }
    else
        time->tm_hour = bcd2dec(buf[2] & HOUR24_MASK);
    time->tm_wday = bcd2dec(buf[3]) - 1;
    time->tm_mday = bcd2dec(buf[4]);
    time->tm_mon = bcd2dec(buf[5]) - 1;
    time->tm_year = bcd2dec(buf[6]) + 2000;
}

void ds1307_set_time(i2c_dev_t *dev, const struct tm *time)
{
    uint8_t buf[8];
    buf[0] = TIME_REG;
    buf[1] = dec2bcd(time->tm_sec);
    buf[2] = dec2bcd(time->tm_min);
    buf[3] = dec2bcd(time->tm_hour);
    buf[4] = dec2bcd(time->tm_wday + 1);
    buf[5] = dec2bcd(time->tm_mday);
    buf[6] = dec2bcd(time->tm_mon + 1);
    buf[7] = dec2bcd(time->tm_year - 2000);

    i2c_slave_write(dev->bus, dev->addr, &buf[0], &buf[1], 7);
}

void ds1307_enable_squarewave(i2c_dev_t *dev, bool enable)
{
    update_register(dev, CONTROL_REG, SQWE_MASK, enable ? SQWE_BIT : 0);
}

bool ds1307_is_squarewave_enabled(i2c_dev_t *dev)
{
    return read_register(dev, CONTROL_REG) & SQWE_BIT;
}

void ds1307_set_squarewave_freq(i2c_dev_t *dev, ds1307_squarewave_freq_t freq)
{
    update_register(dev, CONTROL_REG, SQWEF_MASK, (uint8_t)freq);
}

ds1307_squarewave_freq_t ds1307_get_squarewave_freq(i2c_dev_t *dev)
{
    return (ds1307_squarewave_freq_t)(read_register(dev, CONTROL_REG) & SQWEF_MASK);
}

bool ds1307_get_output(i2c_dev_t *dev)
{
    return read_register(dev, CONTROL_REG) & OUT_BIT;
}

void ds1307_set_output(i2c_dev_t *dev, bool value)
{
    update_register(dev, CONTROL_REG, OUT_MASK, value ? OUT_BIT : 0);
}

int ds1307_read_ram(i2c_dev_t *dev, uint8_t offset, uint8_t *buf, uint8_t len)
{
    if (offset + len > RAM_SIZE)
        return -EINVAL;

    uint8_t reg = RAM_REG + offset;

    return i2c_slave_read(dev->bus, dev->addr, &reg, buf, len);
}

int ds1307_write_ram(i2c_dev_t *dev, uint8_t offset, uint8_t *buf, uint8_t len)
{
    if (offset + len > RAM_SIZE)
        return -EINVAL;

    uint8_t reg = RAM_REG + offset;

    return i2c_slave_write(dev->bus, dev->addr, &reg, buf, len);
}
