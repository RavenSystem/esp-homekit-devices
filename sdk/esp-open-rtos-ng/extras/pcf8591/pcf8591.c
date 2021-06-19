/**
 * Driver for 8-bit analog-to-digital conversion and
 * an 8-bit digital-to-analog conversion PCF8591
 *
 * Part of esp-open-rtos
 * Copyright (C) 2017 Pham Ngoc Thanh <pnt239@gmail.com>
 *               2017 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include "pcf8591.h"

#include <stddef.h>

#define BV(x) (1 << (x))

#define CTRL_AD_CH_MASK 0x03

#define CTRL_AD_IN_PRG 4
#define CTRL_AD_IN_PRG_MASK (0x03 << CTRL_AD_IN_PRG)

#define CTRL_DA_OUT_EN 6

uint8_t pcf8591_read(i2c_dev_t *dev, pcf8591_input_conf_t conf, uint8_t channel)
{
    uint8_t res = 0;
    uint8_t control_reg =
            ((conf << CTRL_AD_IN_PRG) & CTRL_AD_IN_PRG_MASK) |
            (channel & CTRL_AD_CH_MASK) |
            BV(CTRL_DA_OUT_EN);

    i2c_slave_read(dev->bus, dev->addr, &control_reg, &res, 1);

    return res;
}

void pcf8591_write(i2c_dev_t *dev, uint8_t value)
{
    uint8_t buf[2] = { BV(CTRL_DA_OUT_EN), value };

    i2c_slave_write(dev->bus, dev->addr, NULL, buf, 2);
}
