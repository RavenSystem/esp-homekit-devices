/**
 * Driver for 16-channel, 12-bit PWM PCA9685
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include "pca9685.h"

#include <espressif/esp_common.h>

#define REG_MODE1      0x00
#define REG_MODE2      0x01
#define REG_SUBADR1    0x02
#define REG_SUBADR2    0x03
#define REG_SUBADR3    0x04
#define REG_ALLCALLADR 0x05
#define REG_LEDX       0x06
#define REG_ALL_LED    0xfa
#define REG_PRE_SCALE  0xfe

#define MODE1_RESTART (1 << 7)
#define MODE1_EXTCLK  (1 << 6)
#define MODE1_AI      (1 << 5)
#define MODE1_SLEEP   (1 << 4)

#define MODE1_SUB_BIT 3

#define MODE2_INVRT   (1 << 4)
#define MODE2_OUTDRV  (1 << 2)

#define LED_FULL_ON_OFF (1 << 4)

#define REG_LED_N(x)  (REG_LEDX + (x) * 4)
#define OFFS_REG_LED_ON  1
#define OFFS_REG_LED_OFF 3

#define INTERNAL_FREQ 25000000

#define MIN_PRESCALER 0x03
#define MAX_PRESCALER 0xff
#define MAX_CHANNEL   15
#define MAX_SUBADDR   2

#define WAKEUP_DELAY_US 500

//#define PCA9685_DEBUG

#ifdef PCA9685_DEBUG
#include <stdio.h>
#define debug(fmt, ...) printf("%s: " fmt "\n", "PCA9685", ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

inline static uint32_t round_div(uint32_t x, uint32_t y)
{
    return (x + y / 2) / y;
}

inline static void write_reg(i2c_dev_t *dev, uint8_t reg, uint8_t val)
{
    if (i2c_slave_write(dev->bus, dev->addr, &reg, &val, 1))
        debug("Could not write 0x%02x to 0x%02x, bus %u, addr = 0x%02x", reg, val, dev->bus, dev->addr);
}

inline static uint8_t read_reg(i2c_dev_t *dev, uint8_t reg)
{
    uint8_t res = 0;
    if (i2c_slave_read(dev->bus, dev->addr, &reg, &res, 1))
        debug("Could not read from 0x%02x, bus %u, addr = 0x%02x", reg, dev->bus, dev->addr);
    return res;
}

inline static void update_reg(i2c_dev_t *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
    write_reg(dev, reg, (read_reg(dev, reg) & ~mask) | val);
}

void pca9685_init(i2c_dev_t *dev)
{
    // Enable autoincrement
    update_reg(dev, REG_MODE1, MODE1_AI, MODE1_AI);
}

bool pca9685_set_subaddr(i2c_dev_t *dev, uint8_t num, uint8_t subaddr, bool enable)
{
    if (num > MAX_SUBADDR)
    {
        debug("Invalid subaddress number: %d", num);
        return false;
    }

    write_reg(dev, REG_SUBADR1 + num, subaddr << 1);

    uint8_t mask = 1 << (MODE1_SUB_BIT - num);
    update_reg(dev, REG_MODE1, mask, enable ? mask : 0);

    return true;
}

bool pca9685_is_sleeping(i2c_dev_t *dev)
{
    return (read_reg(dev, REG_MODE1) & MODE1_SLEEP) != 0;
}

void pca9685_sleep(i2c_dev_t *dev, bool sleep)
{
    update_reg(dev, REG_MODE1, MODE1_SLEEP, sleep ? MODE1_SLEEP : 0);
    if (!sleep)
        sdk_os_delay_us(WAKEUP_DELAY_US);
}

void pca9685_restart(i2c_dev_t *dev)
{
    uint8_t mode = read_reg(dev, REG_MODE1);
    if (mode & MODE1_RESTART)
    {
        write_reg(dev, REG_MODE1, mode & ~MODE1_SLEEP);
        sdk_os_delay_us(WAKEUP_DELAY_US);
    }
    write_reg(dev, REG_MODE1, (mode & ~MODE1_SLEEP) | MODE1_RESTART);
}

bool pca9685_is_output_inverted(i2c_dev_t *dev)
{
    return (read_reg(dev, REG_MODE2) & MODE2_INVRT) != 0;
}

void pca9685_set_output_inverted(i2c_dev_t *dev, bool inverted)
{
    update_reg(dev, REG_MODE2, MODE2_INVRT, inverted ? MODE2_INVRT : 0);
}

bool pca9685_get_output_open_drain(i2c_dev_t *dev)
{
    return (read_reg(dev, REG_MODE2) & MODE2_OUTDRV) == 0;
}

void pca9685_set_output_open_drain(i2c_dev_t *dev, bool open_drain)
{
    update_reg(dev, REG_MODE2, MODE2_OUTDRV, open_drain ? 0 : MODE2_OUTDRV);
}

uint8_t pca9685_get_prescaler(i2c_dev_t *dev)
{
    return read_reg(dev, REG_PRE_SCALE);
}

bool pca9685_set_prescaler(i2c_dev_t *dev, uint8_t prescaler)
{
    if (prescaler < MIN_PRESCALER)
    {
        debug("Inavlid prescaler value: %d", prescaler);
        return false;
    }

    pca9685_sleep(dev, true);
    write_reg(dev, REG_PRE_SCALE, prescaler);
    pca9685_sleep(dev, false);
    return true;
}

uint16_t pca9685_get_pwm_frequency(i2c_dev_t *dev)
{
    return INTERNAL_FREQ / ((uint32_t)4096 * (read_reg(dev, REG_PRE_SCALE) + 1));
}

bool pca9685_set_pwm_frequency(i2c_dev_t *dev, uint16_t freq)
{
    uint16_t prescaler = round_div(INTERNAL_FREQ, (uint32_t)4096 * freq) - 1;
    if (prescaler < MIN_PRESCALER || prescaler > MAX_PRESCALER)
    {
        debug("Inavlid prescaler value: %d", prescaler);
        return false;
    }

    return pca9685_set_prescaler(dev, prescaler);
}

void pca9685_set_pwm_value(i2c_dev_t *dev, uint8_t channel, uint16_t val)
{
    uint8_t reg = channel > MAX_CHANNEL ? REG_ALL_LED : REG_LED_N(channel);

    if (val == 0)
    {
        // Full off
        write_reg(dev, reg + OFFS_REG_LED_OFF, LED_FULL_ON_OFF);
    }
    else if (val < 4096)
    {
        // Normal
        uint8_t buf[4] = { 0, 0, val, val >> 8 };
        i2c_slave_write(dev->bus, dev->addr, &reg, buf, 4);
    }
    else
    {
        // Full on
        write_reg(dev, reg + OFFS_REG_LED_ON, LED_FULL_ON_OFF);
    }
}

bool pca9685_set_pwm_values(i2c_dev_t *dev, uint8_t first_ch, uint8_t channels, const uint16_t *values)
{
    if (channels == 0 || first_ch + channels - 1 > MAX_CHANNEL)
    {
        debug("Invalid channels");
        return false;
    }

    for (uint8_t i = 0; i < channels; i ++)
        pca9685_set_pwm_value(dev, first_ch + i, values [i]);

    return true;
}
