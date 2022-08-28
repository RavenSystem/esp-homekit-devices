/**
 * Driver for serial nonvolatile ferroelectric random access
 * memory or F-RAM.
 *
 * Part of esp-open-rtos
 * Copyright (C) 2017 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include "fram.h"

#include <esp/gpio.h>

#define CMD_WRSR  0x01 // 0b00000001
#define CMD_WRITE 0x02 // 0b00000010
#define CMD_READ  0x03 // 0b00000011
#define CMD_WRDI  0x04 // 0b00000100
#define CMD_RDSR  0x05 // 0b00000101
#define CMD_WREN  0x06 // 0b00000110
#define CMD_FSTRD 0x0b // 0b00001011
#define CMD_RDID  0x9f // 0b10011111
#define CMD_SLEEP 0xb9 // 0b10111001
#define CMD_SNR   0xc3 // 0b11000011

#define SR_BIT_WEL  1
#define SR_BIT_BP0  2
#define SR_BIT_BP1  3
#define SR_BIT_WPEN 7

#define SR_MASK_BP (0x03 << SR_BIT_BP0)

static const spi_settings_t defaults = {
    .endianness = SPI_BIG_ENDIAN,
    .msb = true,
    .mode = SPI_MODE0,
    .minimal_pins = true,
    .freq_divider = SPI_FREQ_DIV_40M
};

inline static void chip_select(const fram_t *dev)
{
    gpio_write(dev->cs_gpio, false);
}

inline static void chip_unselect(const fram_t *dev)
{
    gpio_write(dev->cs_gpio, true);
}

static uint8_t read_status_reg(const fram_t *dev)
{
    chip_select(dev);
    spi_transfer_8(dev->spi_bus, CMD_RDSR);
    uint8_t res = spi_transfer_8(dev->spi_bus, 0xff);
    chip_unselect(dev);
    return res;
}

static void write_status_reg(const fram_t *dev, uint8_t val)
{
    chip_select(dev);
    spi_transfer_8(dev->spi_bus, CMD_WREN);
    chip_unselect(dev);

    chip_select(dev);
    spi_transfer_8(dev->spi_bus, CMD_WRSR);
    spi_transfer_8(dev->spi_bus, val);
}

void fram_init(const fram_t *dev)
{
    gpio_enable(dev->cs_gpio, GPIO_OUTPUT);
    gpio_set_pullup(dev->cs_gpio, true, true);
    chip_unselect(dev);
}

static void begin(const fram_t *dev, spi_settings_t *s, spi_settings_t *old)
{
    spi_get_settings(dev->spi_bus, &old);
    memcpy(&s, &defaults, sizeof(spi_settings_t));
    s->freq_divider = dev->spi_freq_div;
    spi_set_settings(dev->spi_bus, &s);
    chip_select(dev);
}

static void end(const fram_t *dev, spi_settings_t *old)
{
    chip_unselect(dev);
    spi_set_settings(dev->spi_bus, &old);
}

void fram_read(const fram_t *dev, void *to, void *from, size_t size)
{
    spi_settings_t s, old;
    begin(dev, &s, &old);

    uint32_t header = ((uint32_t)CMD_READ << 24) | ((uint32_t)from & 0x00ffffff);
    spi_transfer_32(dev->spi_bus, header);

    spi_set_endianness(dev->spi_bus, SPI_LITTLE_ENDIAN);
    spi_read(dev->spi_bus, 0xff, to, size, SPI_8BIT);

    end(dev, &old);
}

void fram_write(const fram_t *dev, void *from, void *to, size_t size)
{
    spi_settings_t s, old;
    begin(dev, &s, &old);

    spi_transfer_8(dev->spi_bus, CMD_WREN);
    chip_unselect(dev);

    chip_select(dev);
    uint32_t header = ((uint32_t)CMD_WRITE << 24) | ((uint32_t)to & 0x00ffffff);
    spi_transfer_32(dev->spi_bus, header);

    spi_set_endianness(dev->spi_bus, SPI_LITTLE_ENDIAN);
    spi_transfer(dev->spi_bus, from, NULL, size, SPI_8BIT);

    end(dev, &old);
}

void fram_sleep(const fram_t *dev, bool sleep)
{
    if (!sleep)
    {
        chip_select(dev);
        chip_unselect(dev);
        return;
    }

    spi_settings_t s, old;
    begin(dev, &s, &old);

    spi_transfer_8(dev->spi_bus, CMD_SLEEP);

    end(dev, &old);
}

bool fram_busy(const fram_t *dev)
{
    gpio_enable(dev->cs_gpio, GPIO_INPUT);
    bool res = !gpio_read(dev->cs_gpio);
    fram_init(dev);
    return res;
}

void fram_id(const fram_t *dev, fram_id_t *id)
{
    spi_settings_t s, old;
    begin(dev, &s, &old);

    spi_transfer_8(dev->spi_bus, CMD_RDID);

    for (uint8_t i = 0; i < FRAM_ID_LEN; i ++)
        id->data[FRAM_ID_LEN - i - 1] = spi_transfer_8(dev->spi_bus, 0xff);

    end(dev, &old);
}

void fram_set_wp_mode(const fram_t *dev, fram_wp_mode_t mode)
{
    spi_settings_t s, old;
    begin(dev, &s, &old);

    write_status_reg(dev, (read_status_reg(dev) & ~SR_MASK_BP) | ((mode & 0x03) << SR_BIT_BP0));

    end(dev, &old);
}

fram_wp_mode_t fram_get_wp_mode(const fram_t *dev)
{
    spi_settings_t s, old;
    begin(dev, &s, &old);

    fram_wp_mode_t res = (fram_wp_mode_t)((read_status_reg(dev) & SR_MASK_BP) >> SR_BIT_BP0);

    end(dev, &old);

    return res;
}


