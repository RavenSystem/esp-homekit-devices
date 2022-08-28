/*
 * Driver for 3-axis digital compass HMC5883L
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include "hmc5883l.h"
#include <espressif/esp_common.h>


#define REG_CR_A 0x00
#define REG_CR_B 0x01
#define REG_MODE 0x02
#define REG_DX_H 0x03
#define REG_DX_L 0x04
#define REG_DZ_H 0x05
#define REG_DZ_L 0x06
#define REG_DY_H 0x07
#define REG_DY_L 0x08
#define REG_STAT 0x09
#define REG_ID_A 0x0a
#define REG_ID_B 0x0b
#define REG_ID_C 0x0c

#define BIT_MA  5
#define BIT_DO  2
#define BIT_GN  5

#define MASK_MD 0x03
#define MASK_MA 0x60
#define MASK_DO 0x1c
#define MASK_MS 0x03
#define MASK_DR 0x01
#define MASK_DL 0x02

#define MEASUREMENT_TIMEOUT 6000

#define timeout_expired(start, len) ((uint32_t)(sdk_system_get_time() - (start)) >= (len))

static const float gain_values [] = {
    [HMC5883L_GAIN_1370] = 0.73,
    [HMC5883L_GAIN_1090] = 0.92,
    [HMC5883L_GAIN_820]  = 1.22,
    [HMC5883L_GAIN_660]  = 1.52,
    [HMC5883L_GAIN_440]  = 2.27,
    [HMC5883L_GAIN_390]  = 2.56,
    [HMC5883L_GAIN_330]  = 3.03,
    [HMC5883L_GAIN_230]  = 4.35
};

static float current_gain;
static hmc5883l_operating_mode_t current_mode;

static inline void write_register(i2c_dev_t *dev, uint8_t reg, uint8_t val)
{
    i2c_slave_write(dev->bus, dev->addr, &reg, &val, 1);
}

static inline uint8_t read_register(i2c_dev_t *dev, uint8_t reg)
{
    uint8_t res;
    i2c_slave_read(dev->bus, dev->addr, &reg, &res, 1);
    return res;
}

static inline void update_register(i2c_dev_t *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
    write_register(dev, reg, (read_register(dev, reg) & mask) | val);
}

bool hmc5883l_init(i2c_dev_t *dev)
{
    if (hmc5883l_get_id(dev) != HMC5883L_ID)
        return false;
    current_gain = gain_values[hmc5883l_get_gain(dev)];
    current_mode = hmc5883l_get_operating_mode(dev);
    return true;
}

uint32_t hmc5883l_get_id(i2c_dev_t *dev)
{
    uint32_t res = 0;
    uint8_t reg = REG_ID_A;
    i2c_slave_read(dev->bus, dev->addr, &reg, (uint8_t *)&res, 3);
    return res;
}

hmc5883l_operating_mode_t hmc5883l_get_operating_mode(i2c_dev_t *dev)
{
    uint8_t res = read_register(dev, REG_MODE) & MASK_MD;
    return res == 0 ? HMC5883L_MODE_CONTINUOUS : HMC5883L_MODE_SINGLE;
}

void hmc5883l_set_operating_mode(i2c_dev_t *dev, hmc5883l_operating_mode_t mode)
{
    write_register(dev, REG_MODE, mode);
    current_mode = mode;
}

hmc5883l_samples_averaged_t hmc5883l_get_samples_averaged(i2c_dev_t *dev)
{
    return (read_register(dev, REG_CR_A) & MASK_MA) >> BIT_MA;
}

void hmc5883l_set_samples_averaged(i2c_dev_t *dev, hmc5883l_samples_averaged_t samples)
{
    update_register(dev, REG_CR_A, MASK_MA, samples << BIT_MA);
}

hmc5883l_data_rate_t hmc5883l_get_data_rate(i2c_dev_t *dev)
{
    return (read_register(dev, REG_CR_A) & MASK_DO) >> BIT_DO;
}

void hmc5883l_set_data_rate(i2c_dev_t *dev, hmc5883l_data_rate_t rate)
{
    update_register(dev, REG_CR_A, MASK_DO, rate << BIT_DO);
}

hmc5883l_bias_t hmc5883l_get_bias(i2c_dev_t *dev)
{
    return read_register(dev, REG_CR_A) & MASK_MS;
}

void hmc5883l_set_bias(i2c_dev_t *dev, hmc5883l_bias_t bias)
{
    update_register(dev, REG_CR_A, MASK_MS, bias);
}

hmc5883l_gain_t hmc5883l_get_gain(i2c_dev_t *dev)
{
    return read_register(dev, REG_CR_B) >> BIT_GN;
}

void hmc5883l_set_gain(i2c_dev_t *dev, hmc5883l_gain_t gain)
{
    write_register(dev, REG_CR_B, gain << BIT_GN);
    current_gain = gain_values[gain];
}

bool hmc5883l_data_is_locked(i2c_dev_t *dev)
{
    return read_register(dev, REG_STAT) & MASK_DL;
}

bool hmc5883l_data_is_ready(i2c_dev_t *dev)
{
    return read_register(dev, REG_STAT) & MASK_DR;
}

bool hmc5883l_get_raw_data(i2c_dev_t *dev, hmc5883l_raw_data_t *data)
{
    if (current_mode == HMC5883L_MODE_SINGLE)
    {
        // overwrite mode register for measurement
        hmc5883l_set_operating_mode(dev, current_mode);
        // wait for data
        uint32_t start = sdk_system_get_time();
        while (!hmc5883l_data_is_ready(dev))
        {
            if (timeout_expired(start, MEASUREMENT_TIMEOUT))
                return false;
        }
    }
    uint8_t buf[6];
    uint8_t reg = REG_DX_H;
    i2c_slave_read(dev->bus, dev->addr, &reg, buf, 6);
    data->x = ((int16_t)buf[REG_DX_H - REG_DX_H] << 8) | buf[REG_DX_L - REG_DX_H];
    data->y = ((int16_t)buf[REG_DY_H - REG_DX_H] << 8) | buf[REG_DY_L - REG_DX_H];
    data->z = ((int16_t)buf[REG_DZ_H - REG_DX_H] << 8) | buf[REG_DZ_L - REG_DX_H];
    return true;
}

void hmc5883l_raw_to_mg(const hmc5883l_raw_data_t *raw, hmc5883l_data_t *mg)
{
    mg->x = raw->x * current_gain;
    mg->y = raw->y * current_gain;
    mg->z = raw->z * current_gain;
}

bool hmc5883l_get_data(i2c_dev_t *dev, hmc5883l_data_t *data)
{
    hmc5883l_raw_data_t raw;

    if (!hmc5883l_get_raw_data(dev, &raw))
        return false;
    hmc5883l_raw_to_mg(&raw, data);
    return true;
}
