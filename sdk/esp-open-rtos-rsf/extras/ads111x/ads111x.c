/*
 * Driver for ADS1113/ADS1114/ADS1115 I2C ADC
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include "ads111x.h"

#define ADS111X_DEBUG

#ifdef ADS111X_DEBUG
#include <stdio.h>
#define debug(fmt, ...) printf("%s" fmt "\n", "ADS111x: ", ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

#define REG_CONVERSION 0
#define REG_CONFIG     1
#define REG_THRESH_L   2
#define REG_THRESH_H   3

#define COMP_QUE_OFFSET  1
#define COMP_QUE_MASK    0x03
#define COMP_LAT_OFFSET  2
#define COMP_LAT_MASK    0x01
#define COMP_POL_OFFSET  3
#define COMP_POL_MASK    0x01
#define COMP_MODE_OFFSET 4
#define COMP_MODE_MASK   0x01
#define DR_OFFSET        5
#define DR_MASK          0x07
#define MODE_OFFSET      8
#define MODE_MASK        0x01
#define PGA_OFFSET       9
#define PGA_MASK         0x07
#define MUX_OFFSET       12
#define MUX_MASK         0x07
#define OS_OFFSET        15
#define OS_MASK          0x01

const float ads111x_gain_values[] = {
    [ADS111X_GAIN_6V144]   = 6.144,
    [ADS111X_GAIN_4V096]   = 4.096,
    [ADS111X_GAIN_2V048]   = 2.048,
    [ADS111X_GAIN_1V024]   = 1.024,
    [ADS111X_GAIN_0V512]   = 0.512,
    [ADS111X_GAIN_0V256]   = 0.256,
    [ADS111X_GAIN_0V256_2] = 0.256,
    [ADS111X_GAIN_0V256_3] = 0.256
};

static uint16_t read_reg(i2c_dev_t *dev, uint8_t reg)
{
    uint16_t res = 0;
    if (i2c_slave_read(dev->bus, dev->addr, &reg, (uint8_t *)&res, 2))
        debug("Could not read register %d", reg);
    //debug("Read %d: 0x%04x", reg, res);
    return res;
}

static void write_reg(i2c_dev_t *dev, uint8_t reg, uint16_t val)
{
    //debug("Write %d: 0x%04x", reg, val);
    uint8_t buf[2] = { val >> 8, val};
    if (i2c_slave_write(dev->bus, dev->addr, &reg, buf, 2))
        debug("Could not write 0x%04x to register %d", val, reg);
}

static uint16_t read_conf_bits(i2c_dev_t *dev, uint8_t offs, uint16_t mask)
{
    return (read_reg(dev, REG_CONFIG) >> offs) & mask;
}

static void write_conf_bits(i2c_dev_t *dev, uint16_t val, uint8_t offs, uint16_t mask)
{
    write_reg(dev, REG_CONFIG, (read_reg(dev, REG_CONFIG) & ~(mask << offs)) | (val << offs));
}

bool ads111x_busy(i2c_dev_t *dev)
{
    return read_conf_bits(dev, OS_OFFSET, OS_MASK);
}

void ads111x_start_conversion(i2c_dev_t *dev)
{
    write_conf_bits(dev, 1, OS_OFFSET, OS_MASK);
}

int16_t ads111x_get_value(i2c_dev_t *dev)
{
    return read_reg(dev, REG_CONVERSION);
}

int16_t ads101x_get_value(i2c_dev_t *dev)
{
	uint16_t res = read_reg(dev, REG_CONVERSION) >> 4;
	if (res > 0x07FF)
	{
		// negative number - extend the sign to 16th bit
		res |= 0xF000;
	}
	return (int16_t)res;
}


ads111x_gain_t ads111x_get_gain(i2c_dev_t *dev)
{
    return read_conf_bits(dev, PGA_OFFSET, PGA_MASK);
}

void ads111x_set_gain(i2c_dev_t *dev, ads111x_gain_t gain)
{
    write_conf_bits(dev, gain, PGA_OFFSET, PGA_MASK);
}

ads111x_mux_t ads111x_get_input_mux(i2c_dev_t *dev)
{
    return read_conf_bits(dev, MUX_OFFSET, MUX_MASK);
}

void ads111x_set_input_mux(i2c_dev_t *dev, ads111x_mux_t mux)
{
    write_conf_bits(dev, mux, MUX_OFFSET, MUX_MASK);
}

ads111x_mode_t ads111x_get_mode(i2c_dev_t *dev)
{
    return read_conf_bits(dev, MODE_OFFSET, MODE_MASK);
}

void ads111x_set_mode(i2c_dev_t *dev, ads111x_mode_t mode)
{
    write_conf_bits(dev, mode, MODE_OFFSET, MODE_MASK);
}

ads111x_data_rate_t ads111x_get_data_rate(i2c_dev_t *dev)
{
    return read_conf_bits(dev, DR_OFFSET, DR_MASK);
}

void ads111x_set_data_rate(i2c_dev_t *dev, ads111x_data_rate_t rate)
{
    write_conf_bits(dev, rate, DR_OFFSET, DR_MASK);
}

ads111x_comp_mode_t ads111x_get_comp_mode(i2c_dev_t *dev)
{
    return read_conf_bits(dev, COMP_MODE_OFFSET, COMP_MODE_MASK);
}

void ads111x_set_comp_mode(i2c_dev_t *dev, ads111x_comp_mode_t mode)
{
    write_conf_bits(dev, mode, COMP_MODE_OFFSET, COMP_MODE_MASK);
}

ads111x_comp_polarity_t ads111x_get_comp_polarity(i2c_dev_t *dev)
{
    return read_conf_bits(dev, COMP_POL_OFFSET, COMP_POL_MASK);
}

void ads111x_set_comp_polarity(i2c_dev_t *dev, ads111x_comp_polarity_t polarity)
{
    write_conf_bits(dev, polarity, COMP_POL_OFFSET, COMP_POL_MASK);
}

ads111x_comp_latch_t ads111x_get_comp_latch(i2c_dev_t *dev)
{
    return read_conf_bits(dev, COMP_LAT_OFFSET, COMP_LAT_MASK);
}

void ads111x_set_comp_latch(i2c_dev_t *dev, ads111x_comp_latch_t latch)
{
    write_conf_bits(dev, latch, COMP_LAT_OFFSET, COMP_LAT_MASK);
}

ads111x_comp_queue_t ads111x_get_comp_queue(i2c_dev_t *dev)
{
    return read_conf_bits(dev, COMP_QUE_OFFSET, COMP_QUE_MASK);
}

void ads111x_set_comp_queue(i2c_dev_t *dev, ads111x_comp_queue_t queue)
{
    write_conf_bits(dev, queue, COMP_QUE_OFFSET, COMP_QUE_MASK);
}

int16_t ads111x_get_comp_low_thresh(i2c_dev_t *dev)
{
    return read_reg(dev, REG_THRESH_L);
}

void ads111x_set_comp_low_thresh(i2c_dev_t *dev, int16_t thresh)
{
    write_reg(dev, REG_THRESH_L, thresh);
}

int16_t ads111x_get_comp_high_thresh(i2c_dev_t *dev)
{
    return read_reg(dev, REG_THRESH_H);
}

void ads111x_set_comp_high_thresh(i2c_dev_t *dev, int16_t thresh)
{
    write_reg(dev, REG_THRESH_H, thresh);
}
