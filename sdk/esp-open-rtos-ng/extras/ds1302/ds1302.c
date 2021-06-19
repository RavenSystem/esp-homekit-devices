/*
 * Driver for DS1302 RTC
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>,
 *                    Pavel Merzlyakov <merzlyakovpavel@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include "ds1302.h"
#include <esp/gpio.h>
#include <espressif/esp_common.h>

#define CH_REG   0x80
#define WP_REG   0x8e

#define CH_BIT     (1 << 7)
#define WP_BIT     (1 << 7)
#define HOUR12_BIT (1 << 7)
#define PM_BIT     (1 << 5)

#define CH_MASK ((uint8_t)(~CH_BIT))
#define WP_MASK ((uint8_t)(~WP_BIT))

#define CLOCK_BURST 0xbe
#define RAM_BURST   0xfe

#define SECONDS_MASK 0x7f
#define HOUR12_MASK  0x1f
#define HOUR24_MASK  0x3f

static uint8_t _ce_pin;
static uint8_t _io_pin;
static uint8_t _sclk_pin;
static bool _wp;
static uint8_t _ch;

static uint8_t bcd2dec(uint8_t val)
{
    return (val >> 4) * 10 + (val & 0x0f);
}

static uint8_t dec2bcd(uint8_t val)
{
    return ((val / 10) << 4) + (val % 10);
}

inline static void chip_enable()
{
    gpio_write(_ce_pin, true);
    sdk_os_delay_us(4);
}

inline static void chip_disable()
{
    gpio_write(_ce_pin, false);
}

inline static void prepare(gpio_direction_t dir)
{
    gpio_enable(_io_pin, dir);
    gpio_write(_sclk_pin, false);
    chip_enable();
}

inline static void toggle_clock()
{
    gpio_write(_sclk_pin, true);
    sdk_os_delay_us(1);
    gpio_write(_sclk_pin, false);
    sdk_os_delay_us(1);
}

static void write_byte(uint8_t b)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        gpio_write(_io_pin, (b >> i) & 1);
        toggle_clock();
    }
}

static uint8_t read_byte()
{
    uint8_t b = 0;
    for (uint8_t i = 0; i < 8; i++)
    {
        b |= gpio_read(_io_pin) << i;
        toggle_clock();
    }
    return b;
}

static uint8_t read_register(uint8_t reg)
{
    prepare(GPIO_OUTPUT);
    write_byte(reg | 0x01);
    prepare(GPIO_INPUT);
    uint8_t res = read_byte();
    chip_disable();
    return res;
}

static void write_register(uint8_t reg, uint8_t val)
{
    prepare(GPIO_OUTPUT);
    write_byte(reg);
    write_byte(val);
    chip_disable();
}

static void burst_read(uint8_t reg, uint8_t *dst, uint8_t len)
{
    prepare(GPIO_OUTPUT);
    write_byte(reg | 0x01);
    prepare(GPIO_INPUT);
    for (uint8_t i = 0; i < len; i++, dst++)
        *dst = read_byte();
    chip_disable();
}

static void burst_write(uint8_t reg, uint8_t *src, uint8_t len)
{
    prepare(GPIO_OUTPUT);
    write_byte(reg);
    for (uint8_t i = 0; i < len; i++, src++)
        write_byte(*src);
    chip_disable();
}

inline static void update_register(uint8_t reg, uint8_t mask, uint8_t val)
{
    write_register(reg, (read_register(reg) & mask) | val);
}

void ds1302_init(uint8_t ce_pin, uint8_t io_pin, uint8_t sclk_pin)
{
    _ce_pin = ce_pin;
    _io_pin = io_pin;
    _sclk_pin = sclk_pin;

    gpio_enable(_ce_pin, GPIO_OUTPUT);
    gpio_enable(_sclk_pin, GPIO_OUTPUT);

    _wp = ds1302_get_write_protect();
    _ch = read_register(CH_REG) & CH_BIT;
}

bool ds1302_start(bool start)
{
    if (_wp) return false;

    _ch = start ? 0 : CH_BIT;
    update_register(CH_REG, CH_MASK, _ch);

    return true;
}

bool ds1302_is_running()
{
    return !(read_register(CH_REG) & CH_BIT);
}

void ds1302_set_write_protect(bool protect)
{
    update_register(WP_REG, WP_MASK, protect ? WP_BIT : 0);
    _wp = protect;
}

bool ds1302_get_write_protect()
{
    return (read_register(WP_REG) & WP_BIT) != 0;
}

void ds1302_get_time(struct tm *time)
{
    uint8_t buf[7];
    burst_read(CLOCK_BURST, buf, 7);

    time->tm_sec = bcd2dec(buf[0] & SECONDS_MASK);
    time->tm_min = bcd2dec(buf[1]);
    if (buf[2] & HOUR12_BIT)
    {
        // RTC in 12-hour mode
        time->tm_hour = bcd2dec(buf[2] & HOUR12_MASK) - 1;
        if (buf[2] & PM_BIT)
            time->tm_hour += 12;
    }
    else time->tm_hour = bcd2dec(buf[2] & HOUR24_MASK);
    time->tm_mday = bcd2dec(buf[3]);
    time->tm_mon  = bcd2dec(buf[4]) - 1;
    time->tm_wday = bcd2dec(buf[5]) - 1;
    time->tm_year = bcd2dec(buf[6]) + 2000;
}

bool ds1302_set_time(const struct tm *time)
{
    if (_wp) return false;

    uint8_t buf[8] = {
        dec2bcd(time->tm_sec) | _ch,
        dec2bcd(time->tm_min),
        dec2bcd(time->tm_hour),
        dec2bcd(time->tm_mday),
        dec2bcd(time->tm_mon  + 1),
        dec2bcd(time->tm_wday + 1),
        dec2bcd(time->tm_year - 2000),
        0
    };
    burst_write(CLOCK_BURST, buf, 8);

    return true;
}

bool ds1302_read_sram(uint8_t offset, void *buf, uint8_t len)
{
    if (offset + len > DS1302_RAM_SIZE)
        return false;

    burst_read(RAM_BURST, (uint8_t *)buf, len);

    return true;
}

bool ds1302_write_sram(uint8_t offset, void *buf, uint8_t len)
{
    if (offset + len > DS1302_RAM_SIZE)
        return false;

    burst_write(RAM_BURST, (uint8_t *)buf, len);

    return true;
}
