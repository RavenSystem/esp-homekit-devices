/*
 * ESP hardware SPI master driver
 *
 * Part of esp-open-rtos
 * Copyright (c) Ruslan V. Uss, 2016
 * BSD Licensed as described in the file LICENSE
 */
#include "esp/spi.h"

#include "esp/iomux.h"
#include "esp/gpio.h"
#include <string.h>

#define _SPI0_SCK_GPIO  6
#define _SPI0_MISO_GPIO 7
#define _SPI0_MOSI_GPIO 8
#define _SPI0_HD_GPIO   9
#define _SPI0_WP_GPIO   10
#define _SPI0_CS0_GPIO  11

#define _SPI1_MISO_GPIO 12
#define _SPI1_MOSI_GPIO 13
#define _SPI1_SCK_GPIO  14
#define _SPI1_CS0_GPIO  15

#define _SPI0_FUNC IOMUX_FUNC(1)
#define _SPI1_FUNC IOMUX_FUNC(2)

#define _SPI_BUF_SIZE 64
#define __min(a,b) ((a > b) ? (b):(a))

static bool _minimal_pins[2] = {false, false};

bool spi_init(uint8_t bus, spi_mode_t mode, uint32_t freq_divider, bool msb, spi_endianness_t endianness, bool minimal_pins)
{
    switch (bus)
    {
        case 0:
            gpio_set_iomux_function(_SPI0_MISO_GPIO, _SPI0_FUNC);
            gpio_set_iomux_function(_SPI0_MOSI_GPIO, _SPI0_FUNC);
            gpio_set_iomux_function(_SPI0_SCK_GPIO, _SPI0_FUNC);
            if (!minimal_pins)
            {
                gpio_set_iomux_function(_SPI0_HD_GPIO, _SPI0_FUNC);
                gpio_set_iomux_function(_SPI0_WP_GPIO, _SPI0_FUNC);
                gpio_set_iomux_function(_SPI0_CS0_GPIO, _SPI0_FUNC);
            }
            break;
        case 1:
            gpio_set_iomux_function(_SPI1_MISO_GPIO, _SPI1_FUNC);
            gpio_set_iomux_function(_SPI1_MOSI_GPIO, _SPI1_FUNC);
            gpio_set_iomux_function(_SPI1_SCK_GPIO, _SPI1_FUNC);
            if (!minimal_pins)
                gpio_set_iomux_function(_SPI1_CS0_GPIO, _SPI1_FUNC);
            break;
        default:
            return false;
    }

    _minimal_pins[bus] = minimal_pins;
    SPI(bus).USER0 = SPI_USER0_MOSI | SPI_USER0_CLOCK_IN_EDGE | SPI_USER0_DUPLEX |
        (minimal_pins ? 0 : (SPI_USER0_CS_HOLD | SPI_USER0_CS_SETUP));

    spi_set_frequency_div(bus, freq_divider);
    spi_set_mode(bus, mode);
    spi_set_msb(bus, msb);
    spi_set_endianness(bus, endianness);

    return true;
}

void spi_get_settings(uint8_t bus, spi_settings_t *s)
{
    s->mode = spi_get_mode(bus);
    s->freq_divider = spi_get_frequency_div(bus);
    s->msb = spi_get_msb(bus);
    s->endianness = spi_get_endianness(bus);
    s->minimal_pins = _minimal_pins[bus];
}

void spi_set_mode(uint8_t bus, spi_mode_t mode)
{
    bool cpha = (uint8_t)mode & 1;
    bool cpol = (uint8_t)mode & 2;
    if (cpol)
        cpha = !cpha;  // CPHA must be inverted when CPOL = 1, I have no idea why

    // CPHA
    if (cpha)
        SPI(bus).USER0 |= SPI_USER0_CLOCK_OUT_EDGE;
    else
        SPI(bus).USER0 &= ~SPI_USER0_CLOCK_OUT_EDGE;

    // CPOL - see http://bbs.espressif.com/viewtopic.php?t=342#p5384
    if (cpol)
        SPI(bus).PIN |= SPI_PIN_IDLE_EDGE;
    else
        SPI(bus).PIN &= ~SPI_PIN_IDLE_EDGE;
}

spi_mode_t spi_get_mode(uint8_t bus)
{
    uint8_t cpha = SPI(bus).USER0 & SPI_USER0_CLOCK_OUT_EDGE ? 1 : 0;
    uint8_t cpol = SPI(bus).PIN & SPI_PIN_IDLE_EDGE ? 2 : 0;

    return (spi_mode_t)(cpol | (cpol ? 1 - cpha : cpha)); // see spi_set_mode
}

void spi_set_msb(uint8_t bus, bool msb)
{
    if (msb)
        SPI(bus).CTRL0 &= ~(SPI_CTRL0_WR_BIT_ORDER | SPI_CTRL0_RD_BIT_ORDER);
    else
        SPI(bus).CTRL0 |= (SPI_CTRL0_WR_BIT_ORDER | SPI_CTRL0_RD_BIT_ORDER);
}

void spi_set_endianness(uint8_t bus, spi_endianness_t endianness)
{
    if (endianness == SPI_BIG_ENDIAN)
        SPI(bus).USER0 |= (SPI_USER0_WR_BYTE_ORDER | SPI_USER0_RD_BYTE_ORDER);
    else
        SPI(bus).USER0 &= ~(SPI_USER0_WR_BYTE_ORDER | SPI_USER0_RD_BYTE_ORDER);
}

void spi_set_frequency_div(uint8_t bus, uint32_t divider)
{
    uint32_t predivider = (divider & 0xffff) - 1;
    uint32_t count = (divider >> 16) - 1;
    if (count || predivider)
    {
        IOMUX.CONF &= ~(bus == 0 ? IOMUX_CONF_SPI0_CLOCK_EQU_SYS_CLOCK : IOMUX_CONF_SPI1_CLOCK_EQU_SYS_CLOCK);
        SPI(bus).CLOCK = VAL2FIELD_M(SPI_CLOCK_DIV_PRE, predivider) |
                         VAL2FIELD_M(SPI_CLOCK_COUNT_NUM, count) |
                         VAL2FIELD_M(SPI_CLOCK_COUNT_HIGH, count / 2) |
                         VAL2FIELD_M(SPI_CLOCK_COUNT_LOW, count);
    }
    else
    {
        IOMUX.CONF |= bus == 0 ? IOMUX_CONF_SPI0_CLOCK_EQU_SYS_CLOCK : IOMUX_CONF_SPI1_CLOCK_EQU_SYS_CLOCK;
        SPI(bus).CLOCK = SPI_CLOCK_EQU_SYS_CLOCK;
    }
}

inline static void _set_size(uint8_t bus, uint8_t bytes)
{
    uint32_t bits = ((uint32_t)bytes << 3) - 1;
    SPI(bus).USER1 = SET_FIELD(SPI(bus).USER1, SPI_USER1_MISO_BITLEN, bits);
    SPI(bus).USER1 = SET_FIELD(SPI(bus).USER1, SPI_USER1_MOSI_BITLEN, bits);
}

inline static void _wait(uint8_t bus)
{
    while (SPI(bus).CMD & SPI_CMD_USR)
        ;
}

inline static void _start(uint8_t bus)
{
    SPI(bus).CMD |= SPI_CMD_USR;
}

inline static void _store_data(uint8_t bus, const void *data, size_t len)
{
    uint8_t words = len / 4;
    uint8_t tail = len % 4;

    memcpy((void *)SPI(bus).W, data, len - tail);

    if (!tail) return;

    uint32_t last = 0;
    uint8_t *offs = (uint8_t *)data + len - tail;
    for (uint8_t i = 0; i < tail; i++)
        last = last | (offs[i] << (i * 8));
    SPI(bus).W[words] = last;
}

inline static uint32_t _swap_bytes(uint32_t value)
{
    return (value << 24) | ((value << 8) & 0x00ff0000) | ((value >> 8) & 0x0000ff00) | (value >> 24);
}

inline static uint32_t _swap_words(uint32_t value)
{
    return (value << 16) | (value >> 16);
}

static void _spi_buf_prepare(uint8_t bus, size_t len, spi_endianness_t e, spi_word_size_t word_size)
{
    if (e == SPI_LITTLE_ENDIAN || word_size == SPI_32BIT) return;

    size_t count = word_size == SPI_16BIT ? (len + 1) / 2 : (len + 3) / 4;
    uint32_t *data = (uint32_t *)SPI(bus).W;
    for (size_t i = 0; i < count; i ++)
    {
        data[i] = word_size == SPI_16BIT
            ? _swap_words(data[i])
            : _swap_bytes(data[i]);
    }
}

static void _spi_buf_transfer(uint8_t bus, const void *out_data, void *in_data,
    size_t len, spi_endianness_t e, spi_word_size_t word_size)
{
    _wait(bus);
    size_t bytes = len * (uint8_t)word_size;
    _set_size(bus, bytes);
    _store_data(bus, out_data, bytes);
    _spi_buf_prepare(bus, len, e, word_size);
    _start(bus);
    _wait(bus);
    if (in_data)
    {
        _spi_buf_prepare(bus, len, e, word_size);
        memcpy(in_data, (void *)SPI(bus).W, bytes);
    }
}

uint8_t spi_transfer_8(uint8_t bus, uint8_t data)
{
    uint8_t res;
    _spi_buf_transfer(bus, &data, &res, 1, spi_get_endianness(bus), SPI_8BIT);
    return res;
}

uint16_t spi_transfer_16(uint8_t bus, uint16_t data)
{
    uint16_t res;
    _spi_buf_transfer(bus, &data, &res, 1, spi_get_endianness(bus), SPI_16BIT);
    return res;
}

uint32_t spi_transfer_32(uint8_t bus, uint32_t data)
{
    uint32_t res;
    _spi_buf_transfer(bus, &data, &res, 1, spi_get_endianness(bus), SPI_32BIT);
    return res;
}

static void _rearm_extras_bit(uint8_t bus, bool arm)
{
    if (!_minimal_pins[bus]) return;
    static uint8_t status[2];

    if (arm)
    {
        if (status[bus] & 0x01) SPI(bus).USER0 |= (SPI_USER0_ADDR);
        if (status[bus] & 0x02) SPI(bus).USER0 |= (SPI_USER0_COMMAND);
        if (status[bus] & 0x04)
            SPI(bus).USER0 |= (SPI_USER0_DUMMY | SPI_USER0_MISO);
        status[bus] = 0;
    }
    else
    {
        if (SPI(bus).USER0 & SPI_USER0_ADDR)
        {
            SPI(bus).USER0 &= ~(SPI_USER0_ADDR);
            status[bus] |= 0x01;
        }
        if (SPI(bus).USER0 & SPI_USER0_COMMAND)
        {
            SPI(bus).USER0 &= ~(SPI_USER0_COMMAND);
            status[bus] |= 0x02;
        }
        if (SPI(bus).USER0 & SPI_USER0_DUMMY)
        {
            SPI(bus).USER0 &= ~(SPI_USER0_DUMMY | SPI_USER0_MISO);
            status[bus] |= 0x04;
        }
    }
}

size_t spi_transfer(uint8_t bus, const void *out_data, void *in_data, size_t len, spi_word_size_t word_size)
{
    if (!out_data || !len) return 0;

    spi_endianness_t e = spi_get_endianness(bus);
    uint8_t buf_size = _SPI_BUF_SIZE / (uint8_t)word_size;

    size_t blocks = len / buf_size;
    for (size_t i = 0; i < blocks; i++)
    {
        size_t offset = i * _SPI_BUF_SIZE;
        _spi_buf_transfer(bus, (const uint8_t *)out_data + offset,
            in_data ? (uint8_t *)in_data + offset : NULL, buf_size, e, word_size);
       _rearm_extras_bit(bus, false);
    }

    uint8_t tail = len % buf_size;
    if (tail)
    {
        _spi_buf_transfer(bus, (const uint8_t *)out_data + blocks * _SPI_BUF_SIZE,
            in_data ? (uint8_t *)in_data + blocks * _SPI_BUF_SIZE : NULL, tail, e, word_size);
    }

    if (blocks) _rearm_extras_bit(bus, true);
    return len;
}

static void _spi_buf_read(uint8_t bus, uint8_t b, void *in_data,
    size_t len, spi_endianness_t e, spi_word_size_t word_size)
{
    _wait(bus);
    size_t bytes = len * (uint8_t)word_size;
    _set_size(bus, bytes);
    uint32_t w = ((uint32_t)b << 24) | ((uint32_t)b << 16) | ((uint32_t)b << 8) | b;
    for (uint8_t i = 0; i < _SPI_BUF_SIZE / 4; i ++)
        SPI(bus).W[i] = w;
    _start(bus);
    _wait(bus);
    _spi_buf_prepare(bus, len, e, word_size);
    memcpy(in_data, (void *)SPI(bus).W, bytes);
}

void spi_read(uint8_t bus, uint8_t out_byte, void *in_data, size_t len, spi_word_size_t word_size)
{
    spi_endianness_t e = spi_get_endianness(bus);
    uint8_t buf_size = _SPI_BUF_SIZE / (uint8_t)word_size;

    size_t blocks = len / buf_size;
    for (size_t i = 0; i < blocks; i++)
    {
        size_t offset = i * _SPI_BUF_SIZE;
        _spi_buf_read(bus, out_byte, (uint8_t *)in_data + offset, buf_size, e, word_size);
       _rearm_extras_bit(bus, false);
    }

    uint8_t tail = len % buf_size;
    if (tail)
        _spi_buf_read(bus, out_byte, (uint8_t *)in_data + blocks * _SPI_BUF_SIZE, tail, e, word_size);

    if (blocks) _rearm_extras_bit(bus, true);
}

static void _repeat_send(uint8_t bus, uint32_t *dword, int32_t *repeats,
    spi_word_size_t size)
{
    uint8_t i = 0;
    while (*repeats > 0)
    {
        uint16_t bytes_to_transfer = __min(*repeats * size, _SPI_BUF_SIZE);
        _wait(bus);
        if (i) _rearm_extras_bit(bus, false);
        _set_size(bus, bytes_to_transfer);
        for (i = 0; i < (bytes_to_transfer + 3) / 4; i++)
            SPI(bus).W[i] = *dword; //need test with memcpy !
        _start(bus);
        *repeats -= (bytes_to_transfer / size);
    }
    _wait(bus);
    _rearm_extras_bit(bus, true);
}

void spi_repeat_send_8(uint8_t bus, uint8_t data, int32_t repeats)
{
    uint32_t dword = data << 24 | data << 16 | data << 8 | data;
    _repeat_send(bus, &dword, &repeats, SPI_8BIT);
}

void spi_repeat_send_16(uint8_t bus, uint16_t data, int32_t repeats)
{
    uint32_t dword = data << 16 | data;
    _repeat_send(bus, &dword, &repeats, SPI_16BIT);
}

void spi_repeat_send_32(uint8_t bus, uint32_t data, int32_t repeats)
{
    _repeat_send(bus, &data, &repeats, SPI_32BIT);
}
