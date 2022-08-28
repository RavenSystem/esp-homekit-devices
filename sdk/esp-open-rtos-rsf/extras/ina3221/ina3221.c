/**
 * INA3221 driver for esp-open-rtos.
 *
 * Copyright (c) 2016 Zaltora (https://github.com/Zaltora)
 *
 * MIT Licensed as described in the file LICENSE
 *
 * @todo Interupt system for critical and warning alert pin
 */

#include "ina3221.h"

//#define INA3221_DEBUG true

#ifdef INA3221_DEBUG
#define debug(fmt, ...) printf("%s: " fmt "\n", "INA3221", ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

static int _wireWriteRegister(const i2c_dev_t *dev, uint8_t reg, uint16_t value)
{
    uint8_t d[2] = { 0, 0 };
    d[1] = value & 0x00FF;
    d[0] = (value >> 8) & 0x00FF;
    debug("Data write to bus %u at %02X : %02X+%04X\n", dev->bus, dev->addr, reg, value);
    return i2c_slave_write(dev->bus, dev->addr, &reg, d, sizeof(d));
}

static int _wireReadRegister(const i2c_dev_t *dev, uint8_t reg, uint16_t *value)
{
    uint8_t d[] = { 0, 0 };
    int error = i2c_slave_read(dev->bus, dev->addr, &reg, d, sizeof(d));
    debug("Data read from bus %u at %02X: %02X+%04X\n", dev->bus, dev->addr, reg, *value);
    *value = d[1] | (d[0] << 8);
    return error;
}

int ina3221_trigger(ina3221_t *dev)
{
    return _wireWriteRegister(&dev->i2c_dev, INA3221_REG_CONFIG, dev->config.config_register);
}

int ina3221_getStatus(ina3221_t *dev)
{
    return _wireReadRegister(&dev->i2c_dev, INA3221_REG_MASK, &dev->mask.mask_register);
}

int ina3221_sync(ina3221_t *dev)
{
    uint16_t ptr_data;
    int err = 0;
    //////////////////////// Sync config register
    if ((err = _wireReadRegister(&dev->i2c_dev, INA3221_REG_CONFIG, &ptr_data))) // Read config
        return err;
    if (ptr_data != dev->config.config_register)
    {
        if ((err = _wireWriteRegister(&dev->i2c_dev, INA3221_REG_CONFIG, dev->config.config_register))) // Update config
            return err;
    }
    //////////////////////// Sync mask register config
    if ((err = _wireReadRegister(&dev->i2c_dev, INA3221_REG_MASK, &ptr_data))) // Read mask
        return err;
    if ((ptr_data & INA3221_MASK_CONFIG) != (dev->mask.mask_register & INA3221_MASK_CONFIG))
    {
        if ((err = _wireWriteRegister(&dev->i2c_dev, INA3221_REG_MASK, dev->mask.mask_register & INA3221_MASK_CONFIG))) // Update config
            return err;
    }
    return 0;
}

int ina3221_setting(ina3221_t *dev, bool mode, bool bus, bool shunt)
{
    dev->config.mode = mode;
    dev->config.ebus = bus;
    dev->config.esht = shunt;
    return _wireWriteRegister(&dev->i2c_dev, INA3221_REG_CONFIG, dev->config.config_register);
}

int ina3221_enableChannel(ina3221_t *dev, bool ch1, bool ch2, bool ch3)
{
    dev->config.ch1 = ch1;
    dev->config.ch2 = ch2;
    dev->config.ch3 = ch3;
    return _wireWriteRegister(&dev->i2c_dev, INA3221_REG_CONFIG, dev->config.config_register);
}

int ina3221_enableChannelSum(ina3221_t *dev, bool ch1, bool ch2, bool ch3)
{
    dev->mask.scc1 = ch1;
    dev->mask.scc2 = ch2;
    dev->mask.scc3 = ch3;
    return _wireWriteRegister(&dev->i2c_dev, INA3221_REG_MASK, dev->mask.mask_register & INA3221_MASK_CONFIG);
}

int ina3221_enableLatchPin(ina3221_t *dev, bool warning, bool critical)
{
    dev->mask.wen = warning;
    dev->mask.cen = critical;
    return _wireWriteRegister(&dev->i2c_dev, INA3221_REG_MASK, dev->mask.mask_register & INA3221_MASK_CONFIG);
}

int ina3221_setAverage(ina3221_t *dev, ina3221_avg_t avg)
{
    dev->config.avg = avg;
    return _wireWriteRegister(&dev->i2c_dev, INA3221_REG_CONFIG, dev->config.config_register);
}

int ina3221_setBusConversionTime(ina3221_t *dev, ina3221_ct_t ct)
{
    dev->config.vbus = ct;
    return _wireWriteRegister(&dev->i2c_dev, INA3221_REG_CONFIG, dev->config.config_register);
}

int ina3221_setShuntConversionTime(ina3221_t *dev, ina3221_ct_t ct)
{
    dev->config.vsht = ct;
    return _wireWriteRegister(&dev->i2c_dev, INA3221_REG_CONFIG, dev->config.config_register);
}

int ina3221_reset(ina3221_t *dev)
{
    dev->config.config_register = INA3221_DEFAULT_CONFIG; //dev reset
    dev->mask.mask_register = INA3221_DEFAULT_CONFIG; //dev reset
    dev->config.rst = 1;
    return _wireWriteRegister(&dev->i2c_dev, INA3221_REG_CONFIG, dev->config.config_register); // send reset to device
}

int ina3221_getBusVoltage(ina3221_t *dev, ina3221_channel_t channel, float *voltage)
{
    int16_t raw_value;
    int err = 0;
    if ((err = _wireReadRegister(&dev->i2c_dev, INA3221_REG_BUSVOLTAGE_1 + channel * 2, (uint16_t*)&raw_value)))
        return err;
    if (voltage)
        *voltage = raw_value * 0.001; //V    8mV step
    return 0;
}

int ina3221_getShuntValue(ina3221_t *dev, ina3221_channel_t channel, float *voltage, float *current)
{
    int16_t raw_value;
    int err = 0;
    if ((err = _wireReadRegister(&dev->i2c_dev,INA3221_REG_SHUNTVOLTAGE_1+channel*2, (uint16_t*)&raw_value)))
        return err;
    float compute = raw_value*0.005; //mV   40uV step
    if (voltage)
        *voltage = compute;
    if(!dev->shunt[channel])
    {
        debug("No shunt configured for channel %u. Dev:%u:%X\n", channel+1, dev->bus, dev->addr);
        return -EINVAL;
    }
    if (current)
        *current = (compute*1000.0)/dev->shunt[channel];  //mA
    return 0;
}

int ina3221_getSumShuntValue(ina3221_t *dev, float *voltage)
{
    int16_t raw_value;
    int err = 0;
    if ((err = _wireReadRegister(&dev->i2c_dev, INA3221_REG_SHUNT_VOLTAGE_SUM, (uint16_t*)&raw_value)))
        return err;
    *voltage = raw_value * 0.02; //uV   40uV step
    return 0;
}

int ina3221_setCriticalAlert(ina3221_t *dev, ina3221_channel_t channel, float current)
{
    int16_t raw_value = current * dev->shunt[channel] * 0.2; // format
    return _wireWriteRegister(&dev->i2c_dev, INA3221_REG_CRITICAL_ALERT_1 + channel * 2, *(uint16_t*)&raw_value);
}

int ina3221_setWarningAlert(ina3221_t *dev, ina3221_channel_t channel, float current)
{
    int16_t raw_value = current * dev->shunt[channel] * 0.2; // format
    return _wireWriteRegister(&dev->i2c_dev, INA3221_REG_WARNING_ALERT_1 + channel * 2, *(uint16_t*)&raw_value);
}

int ina3221_setSumWarningAlert(ina3221_t *dev, float voltage)
{
    int16_t raw_value = voltage * 50.0; // format
    return _wireWriteRegister(&dev->i2c_dev, INA3221_REG_SHUNT_VOLTAGE_SUM_LIMIT, *(uint16_t*)&raw_value);
}

int ina3221_setPowerValidUpperLimit(ina3221_t *dev, float voltage)
{
    if (!dev->config.ebus)
    {
        debug("Bus not enable. Dev:%u:%X\n", dev->bus, dev->addr);
        return -ENOTSUP;
    }
    int16_t raw_value = voltage * 1000.0; //format
    return _wireWriteRegister(&dev->i2c_dev, INA3221_REG_VALID_POWER_UPPER_LIMIT, *(uint16_t*)&raw_value);
}

int ina3221_setPowerValidLowerLimit(ina3221_t *dev, float voltage)
{
    if (!dev->config.ebus)
    {
        debug("Bus not enable. Dev:%u:%X\n", dev->bus, dev->addr);
        return -ENOTSUP;
    }
    int16_t raw_value = voltage * 1000.0; // round and format
    return _wireWriteRegister(&dev->i2c_dev, INA3221_REG_VALID_POWER_LOWER_LIMIT, *(uint16_t*)&raw_value);
}
