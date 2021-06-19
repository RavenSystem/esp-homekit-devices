/*
 * Driver for barometic pressure sensor MS5611-01BA03
 *
 * Copyright (C) 2016 Bernhard Guillon <Bernhard.Guillon@web.de>
 *
 * Loosely based on hmc5831 with:
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include "ms561101ba03.h"
#include <espressif/esp_common.h>
#include "FreeRTOS.h"
#include "task.h"

#define CONVERT_D1  0x40
#define CONVERT_D2  0x50
#define ADC_READ    0x00
#define RESET       0x1E

/*
 * FIXME:
 * The chip has different response times for the different oversampling rates
 * (0.5 ms/1.1ms/2.1ms/4.1ms/8.22ms)
 * For now use a save value.
 */
#define CONVERSION_TIME     20 / portTICK_PERIOD_MS // milliseconds

static inline int reset(i2c_dev_t *i2c_dev)
{
    uint8_t buf[1] = { RESET };
    return i2c_slave_write(i2c_dev->bus, i2c_dev->addr, NULL, buf, 1);
}

static inline bool read_prom(ms561101ba03_t *dev)
{
    uint8_t tmp[2] = { 0, 0 };
    uint8_t reg = 0xA2;

    if (i2c_slave_read(dev->i2c_dev.bus, dev->i2c_dev.addr, &reg, tmp, 2))
        return false;
    dev->config_data.sens = tmp[0] << 8 | tmp[1];

    reg = 0xA4;
    if (i2c_slave_read(dev->i2c_dev.bus, dev->i2c_dev.addr, &reg, tmp, 2))
        return false;
    dev->config_data.off = tmp[0] << 8 | tmp[1];

    reg = 0xA6;
    if (i2c_slave_read(dev->i2c_dev.bus, dev->i2c_dev.addr, &reg, tmp, 2))
        return false;
    dev->config_data.tcs = tmp[0] << 8 | tmp[1];

    reg = 0xA8;
    if (i2c_slave_read(dev->i2c_dev.bus, dev->i2c_dev.addr, &reg, tmp, 2))
        return false;
    dev->config_data.tco = tmp[0] << 8 | tmp[1];

    reg = 0xAA;
    if (i2c_slave_read(dev->i2c_dev.bus, dev->i2c_dev.addr, &reg, tmp, 2))
        return false;
    dev->config_data.t_ref = tmp[0] << 8 | tmp[1];

    reg = 0xAC;
    if (i2c_slave_read(dev->i2c_dev.bus, dev->i2c_dev.addr, &reg, tmp, 2))
        return false;
    dev->config_data.tempsens = tmp[0] << 8 | tmp[1];

    return true;
}

static inline int start_pressure_conversion(ms561101ba03_t *dev) //D1
{
    uint8_t buf = CONVERT_D1 + dev->osr;
    return i2c_slave_write(dev->i2c_dev.bus, dev->i2c_dev.addr, NULL, &buf, 1);
}

static inline int start_temperature_conversion(ms561101ba03_t *dev) //D2
{
    uint8_t buf = CONVERT_D2 + dev->osr;
    return i2c_slave_write(dev->i2c_dev.bus, dev->i2c_dev.addr, NULL, &buf, 1);
}

static inline bool read_adc(i2c_dev_t *i2c_dev, uint32_t *result)
{
    *result = 0;
    uint8_t tmp[3];
    uint8_t reg = 0x00;
    if (i2c_slave_read(i2c_dev->bus, i2c_dev->addr, &reg, tmp, 3))
        return false;

    *result = (tmp[0] << 16) | (tmp[1] << 8) | tmp[2];

    // If we are to fast the ADC will return 0 instead of the actual result
    if (*result == 0)
        return false;

    return true;
}

static inline void calc_dt(ms561101ba03_t *dev, uint32_t digital_temperature)
{
    // Difference between actual and reference digital_temperature
    // dT = D2 - T_ref = D2 - C5 *2^8
    dev->dT = digital_temperature - ((int32_t)dev->config_data.t_ref << 8);
}

static inline int32_t calc_temp(ms561101ba03_t *dev)
{
    // Actual temerature (-40...85C with 0.01 resulution)
    // TEMP = 20C +dT * TEMPSENSE =2000 + dT * C6 / 2^23
    return (int32_t)(2000 + (int64_t)dev->dT * dev->config_data.tempsens / 8388608);
}

static inline int64_t calc_off(ms561101ba03_t *dev)
{
    // Offset at actual temperature
    // OFF=OFF_t1 + TCO * dT = OFF_t1(C2) * 2^16 + (C4*dT)/2^7
    return (int64_t)((int64_t)dev->config_data.off * (int64_t)65536)
        + (((int64_t)dev->config_data.tco * (int64_t)dev->dT) / (int64_t)128);
}

static inline int64_t calc_sens(ms561101ba03_t *dev)
{
    // Senisitivity at actual temperature
    // SENS=SENS_t1 + TCS *dT = SENS_t1(C1) *2^15 + (TCS(C3) *dT)/2^8
    return (int64_t)(((int64_t)dev->config_data.sens) * (int64_t)32768)
        + (((int64_t)dev->config_data.tcs * (int64_t)dev->dT) / (int64_t)256);
}

static inline int32_t calc_p(uint32_t digital_pressure, int64_t sens, int64_t off)
{
    // Temperature compensated pressure (10...1200mbar with 0.01mbar resolution
    // P = digital pressure value  * SENS - OFF = (D1 * SENS/2^21 -OFF)/2^15
    return (int32_t)(((int64_t)digital_pressure
        * (int64_t)((int64_t)sens / (int64_t)0x200000) - (int64_t)off)
        / (int64_t)32768);
}

static inline bool get_raw_temperature(ms561101ba03_t *dev, uint32_t *result)
{
    if (start_temperature_conversion(dev))
        return false;

    vTaskDelay(CONVERSION_TIME);

    if (!read_adc(&dev->i2c_dev, result))
        return false;

    return true;
}

static inline bool get_raw_pressure(ms561101ba03_t *dev, uint32_t *result)
{
    if (start_pressure_conversion(dev))
    	return false;

    vTaskDelay(CONVERSION_TIME);

    if (!read_adc(&dev->i2c_dev, result))
    	return false;

    return true;
}

/////////////////////////Public//////////////////////////////////////

bool ms561101ba03_get_sensor_data(ms561101ba03_t *dev)
{
    // Second order temperature compensation see datasheet p8
    uint32_t raw_pressure = 0;
    if (!get_raw_pressure(dev, &raw_pressure))
    	return false;

    uint32_t raw_temperature = 0;
    if (!get_raw_temperature(dev, &raw_temperature))
    	return false;

    calc_dt(dev, raw_temperature);
    int64_t temp = calc_temp(dev);
    int64_t off = calc_off(dev);
    int64_t sens = calc_sens(dev);

    //Set defaults for temp >= 2000
    int64_t t_2 = 0;
    int64_t off_2 = 0;
    int64_t sens_2 = 0;
    int64_t help = 0;
    if (temp < 2000)
    {
        //Low temperature
        t_2 = ((dev->dT * dev->dT) >> 31);           // T2 = dT^2/2^31
        help = (temp - 2000);
        help = 5 * help * help;
        off_2 = help >> 1;                    // OFF_2 = 5 * (TEMP - 2000)^2/2^1
        sens_2 = help >> 2;                  // SENS_2 = 5 * (TEMP - 2000)^2/2^2
        if (temp < -1500)
        {
            // Very low temperature
            help = (temp + 1500);
            help = help * help;
            off_2 = off_2 + 7 * help;     // OFF_2 = OFF_2 + 7 * (TEMP + 1500)^2
            sens_2 = sens_2 + ((11 * help) >> 1); // SENS_2 = SENS_2 + 7 * (TEMP + 1500)^2/2^1
        }
    }

    temp = temp - t_2;
    off = off - off_2;
    sens = sens - sens_2;

    dev->result.pressure = calc_p(raw_pressure, sens, off);
    dev->result.temperature = (int32_t)temp;
    return true;
}

bool ms561101ba03_init(ms561101ba03_t *dev)
{
    // First of all we need to reset the chip
    if (reset(&dev->i2c_dev))
    	return false;
    // Wait a bit for the device to reset
    vTaskDelay(CONVERSION_TIME);
    // Get the config
    if (!read_prom(dev))
    	return false;
    // Every thing went fine
    return true;
}
