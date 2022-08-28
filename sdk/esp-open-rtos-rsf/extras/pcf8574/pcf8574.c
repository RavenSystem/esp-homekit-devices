#include "pcf8574.h"

uint8_t pcf8574_port_read(i2c_dev_t *dev)
{
    uint8_t res;
    if (i2c_slave_read(dev->bus, dev->addr, NULL, &res, 1))
        return 0;
    return res;
}

size_t pcf8574_port_read_buf(i2c_dev_t *dev, void *buf, size_t len)
{
    if (!len || !buf) return 0;
    uint8_t *_buf = (uint8_t *)buf;

    if (i2c_slave_read(dev->bus, dev->addr, NULL, _buf, len))
        return 0;
    return len;
}

size_t pcf8574_port_write_buf(const i2c_dev_t *dev, void *buf, size_t len)
{
    if (!len || !buf) return 0;
    uint8_t *_buf = (uint8_t *)buf;

    if (i2c_slave_write(dev->bus, dev->addr, NULL, _buf, len))
        return 0;
    return len;
}

void pcf8574_port_write(const i2c_dev_t *dev, uint8_t value)
{
    i2c_slave_write(dev->bus, dev->addr, NULL, &value, 1);
}

bool pcf8574_gpio_read(i2c_dev_t *dev, uint8_t num)
{
    return (bool)((pcf8574_port_read(dev) >> num) & 1);
}

void pcf8574_gpio_write(i2c_dev_t *dev, uint8_t num, bool value)
{
    uint8_t bit = (uint8_t)value << num;
    uint8_t mask = ~(1 << num);
    pcf8574_port_write(dev, (pcf8574_port_read(dev) & mask) | bit);
}
