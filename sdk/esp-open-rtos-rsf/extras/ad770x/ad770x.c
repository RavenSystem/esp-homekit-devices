/**
 * Driver for AD7705/AD7706 SPI ADC
 *
 * Part of esp-open-rtos
 * Copyright (C) 2017 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include "ad770x.h"

#include <errno.h>
#include <esp/gpio.h>
#include <esp/spi.h>
#include <espressif/esp_common.h>

#define AD770X_DEBUG

#define BUS 1
#define INIT_TIMEOUT 500000  // 500ms

#ifdef AD770X_DEBUG
#include <stdio.h>
#define debug(fmt, ...) printf("%s" fmt "\n", "AD770x: ", ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

#define timeout_expired(start, len) ((uint32_t)(sdk_system_get_time() - (start)) >= (len))

#define _BV(x) (1 << (x))

#define REG_COMM  0x00  // 8 bits
#define REG_SETUP 0x01  // 8 bits
#define REG_CLOCK 0x02  // 8 bits
#define REG_DATA  0x03  // 16 bits
#define REG_TEST  0x04  // 8 bits
#define REG_OFFS  0x06  // 24 bits
#define REG_GAIN  0x07  // 24 bits

#define BIT_COMM_CH0  0
#define BIT_COMM_CH1  1
#define BIT_COMM_STBY 2
#define BIT_COMM_RW   3
#define BIT_COMM_RS0  4
#define BIT_COMM_RS1  5
#define BIT_COMM_RS2  6
#define BIT_COMM_DRDY 7

#define MASK_COMM_CH 0x03
#define MASK_COMM_RS 0x70

#define BIT_CLOCK_FS0    0
#define BIT_CLOCK_FS1    1
#define BIT_CLOCK_CLK    2
#define BIT_CLOCK_CLKDIV 3
#define BIT_CLOCK_CLKDIS 4

#define MASK_CLOCK_FS  0x03
#define MASK_CLOCK_CLK 0x0c

#define BIT_SETUP_FSYNC 0
#define BIT_SETUP_BUF   1
#define BIT_SETUP_BU    2
#define BIT_SETUP_G0    3
#define BIT_SETUP_G1    4
#define BIT_SETUP_G2    5
#define BIT_SETUP_MD0   6
#define BIT_SETUP_MD1   7

#define MASK_SETUP_GAIN 0x38
#define MASK_SETUP_MODE 0xc0

static const spi_settings_t config = {
    .endianness = SPI_BIG_ENDIAN,
    .msb = true,
    .minimal_pins = true,
    .mode = SPI_MODE3,
    .freq_divider = SPI_FREQ_DIV_500K
};

static uint8_t write(uint8_t cs_pin, uint8_t value)
{
    spi_settings_t old;
    spi_get_settings(BUS, &old);
    spi_set_settings(BUS, &config);

    gpio_write(cs_pin, false);
    uint8_t res = spi_transfer_8(BUS, value);
    //debug("byte wr: 0x%02x", value);
    gpio_write(cs_pin, true);

    spi_set_settings(BUS, &old);
    return res;
}

inline static uint8_t read_byte(uint8_t cs_pin)
{
    return write(cs_pin, 0);
}

static uint16_t read_word(uint8_t cs_pin)
{
    spi_settings_t old;
    spi_get_settings(BUS, &old);
    spi_set_settings(BUS, &config);

    gpio_write(cs_pin, false);
    uint16_t res = spi_transfer_16(BUS, 0);
    gpio_write(cs_pin, true);

    spi_set_settings(BUS, &old);
    return res;
}

static void prepare(uint8_t channel, uint8_t reg, bool read, uint8_t cs_pin, bool standby)
{
    write(cs_pin,
        (channel & MASK_COMM_CH) |
        (read ? _BV(BIT_COMM_RW) : 0) |
        ((reg << BIT_COMM_RS0) & MASK_COMM_RS) |
        (standby ? _BV(BIT_COMM_STBY) : 0)
    );
}

int ad770x_init(const ad770x_params_t *params, uint8_t channel)
{
    if (!spi_set_settings(BUS, &config))
    {
        debug("Cannot init SPI");
        return -EIO;
    }

    if ((params->master_clock >= AD770X_MCLK_2_4576MHz && params->update_rate < AD770X_RATE_50)
        || (params->master_clock < AD770X_MCLK_2_4576MHz && params->update_rate > AD770X_RATE_200))
    {
        debug("Invalid update rate / master clock combination");
        return -EINVAL;
    }

    gpio_enable(params->cs_pin, GPIO_OUTPUT);
    gpio_write(params->cs_pin, true);

    prepare(channel, REG_CLOCK, false, params->cs_pin, false);
    write(params->cs_pin,
        ((params->master_clock << BIT_CLOCK_CLK) & MASK_CLOCK_CLK) |
        (params->update_rate & MASK_CLOCK_FS)
    );

    ad770x_set_mode(params, channel, AD770X_MODE_CALIBRATION);

    uint32_t start = sdk_system_get_time();
    while (!ad770x_data_ready(params, channel))
        if (timeout_expired(start, INIT_TIMEOUT))
        {
            debug("Timeout while calibration");
            return -EIO;
        }

    return 0;
}

void ad770x_set_mode(const ad770x_params_t *params, uint8_t channel, ad770x_mode_t mode)
{
    prepare(channel, REG_SETUP, false, params->cs_pin, false);
    write(params->cs_pin,
        ((params->gain << BIT_SETUP_G0) & MASK_SETUP_GAIN) |
        (params->bipolar ? 0 : _BV(BIT_SETUP_BU)) |
        ((mode << BIT_SETUP_MD0) & MASK_SETUP_MODE)
    );
}

bool ad770x_data_ready(const ad770x_params_t *params, uint8_t channel)
{
    prepare(channel, REG_COMM, true, params->cs_pin, false);
    return !(read_byte(params->cs_pin) & _BV(BIT_COMM_DRDY));
}

uint16_t ad770x_raw_adc_value(const ad770x_params_t *params, uint8_t channel)
{
    prepare(channel, REG_DATA, true, params->cs_pin, false);
    return read_word(params->cs_pin);
}
