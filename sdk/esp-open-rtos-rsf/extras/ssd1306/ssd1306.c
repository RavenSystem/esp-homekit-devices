/**
 * SSD1306 OLED display driver for esp-open-rtos.
 *
 * Copyright (c) 2016 urx (https://github.com/urx),
 *                    Ruslan V. Uss (https://github.com/UncleRus)
 *                    Zaltora (https://github.com/Zaltora)
 *
 * MIT Licensed as described in the file LICENSE
 *
 * @todo HW scrolling, sprites
 */
#include "ssd1306.h"
#include <stdio.h>
#if (SSD1306_SPI4_SUPPORT) || (SSD1306_SPI3_SUPPORT)
    #include <esp/spi.h>
#endif
#include <esp/gpio.h>
#include <FreeRTOS.h>
#include <task.h>

#define SPI_BUS 1

//#define SSD1306_DEBUG

/* SSD1306 commands */
#define SSD1306_SET_MEM_ADDR_MODE    (0x20)

#define SSD1306_SET_COL_ADDR         (0x21)
#define SSD1306_SET_PAGE_ADDR        (0x22)
#define SSD1306_SET_DISP_START_LINE  (0x40)
#define SSD1306_SET_CONTRAST         (0x81)
#define SSD1306_SET_SEGMENT_REMAP0   (0xA0)
#define SSD1306_SET_SEGMENT_REMAP1   (0xA1)
#define SSD1306_SET_ENTIRE_DISP_ON   (0xA5)
#define SSD1306_SET_ENTIRE_DISP_OFF  (0xA4)
#define SSD1306_SET_INVERSION_OFF    (0xA6)
#define SSD1306_SET_INVERSION_ON     (0xA7)

#define SSD1306_SET_MUX_RATIO        (0xA8)
#define SSD1306_MUX_RATIO_MASK       (0x3F)
#define SSD1306_SET_DISPLAY_OFF      (0xAE)
#define SSD1306_SET_DISPLAY_ON       (0xAF)
#define SSD1306_SET_SCAN_DIR_FWD     (0xC0)
#define SSD1306_SET_SCAN_DIR_BWD     (0xC8)
#define SSD1306_SET_DISPLAY_OFFSET   (0xD3)
#define SSD1306_SET_OSC_FREQ         (0xD5)
#define SSD1306_SET_PRE_CHRG_PER     (0xD9)

#define SSD1306_SET_COM_PINS_HW_CFG  (0xDA)
#define SSD1306_COM_PINS_HW_CFG_MASK (0x32)
#define SSD1306_SEQ_COM_PINS_CFG     (0x02)
#define SSD1306_ALT_COM_PINS_CFG     (0x12)
#define SSD1306_COM_LR_REMAP_OFF     (0x02)
#define SSD1306_COM_LR_REMAP_ON      (0x22)

#define SSD1306_SET_DESEL_LVL        (0xDB)
#define SSD1306_SET_NOP              (0xE3)

#define SSD1306_SET_CHARGE_PUMP      (0x8D)
#define SSD1306_CHARGE_PUMP_EN       (0x14)
#define SSD1306_CHARGE_PUMP_DIS      (0x10)

#define SSD1306_SCROLL_HOR_LEFT      (0x27)
#define SSD1306_SCROLL_HOR_RIGHT     (0x26)
#define SSD1306_SCROLL_HOR_VER_LEFT  (0x2A)
#define SSD1306_SCROLL_HOR_VER_RIGHT (0x29)
#define SSD1306_SCROLL_ENABLE        (0x2F)
#define SSD1306_SCROLL_DISABLE       (0x2E)

#define SH1106_SET_CHARGE_PUMP       (0xAD)
#define SH1106_CHARGE_PUMP_EN        (0x8B)
#define SH1106_CHARGE_PUMP_DIS       (0x8A)
#define SH1106_CHARGE_PUMP_VALUE     (0x30)

#define SH1106_SET_PAGE_ADDRESS      (0xB0)
#define SH1106_SET_LOW_COL_ADDR      (0x00)
#define SH1106_SET_HIGH_COL_ADDR     (0x10)

#ifdef SSD1306_DEBUG
#define debug(fmt, ...) printf("%s: " fmt "\n", "SSD1306", ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

#define abs(x) ((x)<0 ? -(x) : (x))
#define swap(x, y) do { typeof(x) temp##x##y = x; x = y; y = temp##x##y; } while (0)

#if (SSD1306_I2C_SUPPORT)
static int inline i2c_send(const ssd1306_t *dev, uint8_t reg, uint8_t* data, uint8_t len)
{
    return i2c_slave_write(dev->i2c_dev.bus, dev->i2c_dev.addr, &reg, data, len);
}
#endif

/* Issue a command to SSD1306 device
 * I2C proto format:
 * |S|Slave Address|W|ACK|0x00|Command|Ack|P|
 *
 * in case of two-bytes command here will be Data byte
 * right after the command byte.
 */
int ssd1306_command(const ssd1306_t *dev, uint8_t cmd)
{
    debug("Command: 0x%02x", cmd);
    switch (dev->protocol) {
#if (SSD1306_I2C_SUPPORT)
        case SSD1306_PROTO_I2C:
            return i2c_send(dev, 0x00, &cmd, 1);
            break;
#endif
#if (SSD1306_SPI4_SUPPORT)
        case SSD1306_PROTO_SPI4:
            gpio_write(dev->dc_pin, false); // command mode
            gpio_write(dev->cs_pin, false);
            spi_transfer_8(SPI_BUS, cmd);
            gpio_write(dev->cs_pin, true);
            break;
#endif
#if (SSD1306_SPI3_SUPPORT)
        case SSD1306_PROTO_SPI3:
            gpio_write(dev->cs_pin, false);
            spi_set_command(SPI_BUS, 1, 0); // command mode
            spi_transfer_8(SPI_BUS, cmd);
            spi_clear_command(SPI_BUS);
            gpio_write(dev->cs_pin, true);
            break;
#endif
        default:
            debug("Unsupported protocol");
            return -EPROTONOSUPPORT;
    }

    return 0;
}

/* Perform default init routine according
 * to SSD1306 datasheet from adafruit.com */
int ssd1306_init(const ssd1306_t *dev)
{
    uint8_t pin_cfg;
    switch (dev->height) {
        case 16:
        case 32:
            pin_cfg = 0x02;
            break;
        case 64:
            pin_cfg = 0x12;
            break;
        default:
            debug("Unsupported screen height");
            return -ENOTSUP;
    }

    switch (dev->protocol) {
#if (SSD1306_I2C_SUPPORT)
        case SSD1306_PROTO_I2C:
            break;
#endif
#if (SSD1306_SPI4_SUPPORT)
        case SSD1306_PROTO_SPI4:
            gpio_enable(dev->cs_pin, GPIO_OUTPUT);
            gpio_write(dev->cs_pin, true);
            gpio_enable(dev->dc_pin, GPIO_OUTPUT);
            spi_init(SPI_BUS, SPI_MODE0, SPI_FREQ_DIV_8M, true, SPI_LITTLE_ENDIAN, true);
            break;
#endif
#if (SSD1306_SPI3_SUPPORT)
        case SSD1306_PROTO_SPI3:
            gpio_enable(dev->cs_pin, GPIO_OUTPUT);
            gpio_write(dev->cs_pin, true);
            spi_init(SPI_BUS, SPI_MODE0, SPI_FREQ_DIV_8M, true, SPI_LITTLE_ENDIAN, true);
            break;
#endif
        default:
            debug("Unsupported protocol");
            return -EPROTONOSUPPORT;
    }
    switch (dev->screen) {
    case SSD1306_SCREEN:
        if (!ssd1306_display_on(dev, false)                                  &&
                !ssd1306_set_osc_freq(dev, 0x80)                                 &&
                !ssd1306_set_mux_ratio(dev, dev->height - 1)                     &&
                !ssd1306_set_display_offset(dev, 0x0)                            &&
                !ssd1306_set_display_start_line(dev, 0x0)                        &&
                !ssd1306_set_charge_pump_enabled(dev, true)                      &&
                !ssd1306_set_mem_addr_mode(dev, SSD1306_ADDR_MODE_HORIZONTAL)    &&
                !ssd1306_set_segment_remapping_enabled(dev, false)               &&
                !ssd1306_set_scan_direction_fwd(dev, true)                       &&
                !ssd1306_set_com_pin_hw_config(dev, pin_cfg)                     &&
                !ssd1306_set_contrast(dev, 0x9f)                                 &&
                !ssd1306_set_precharge_period(dev, 0xf1)                         &&
                !ssd1306_set_deseltct_lvl(dev, 0x40)                             &&
                !ssd1306_set_whole_display_lighting(dev, true)                   &&
                !ssd1306_set_inversion(dev, false)                               &&
                !ssd1306_display_on(dev, true)) {
            return 0;
        }
        break;
    case SH1106_SCREEN:
        if (!ssd1306_display_on(dev, false)                                  &&
                !ssd1306_set_charge_pump_enabled(dev, true)                      &&
                !sh1106_set_charge_pump_voltage(dev,SH1106_VOLTAGE_74)           &&
                !ssd1306_set_osc_freq(dev, 0x80)                                 &&
                !ssd1306_set_mux_ratio(dev, dev->height - 1)                     &&
                !ssd1306_set_display_offset(dev, 0x0)                            &&
                !ssd1306_set_display_start_line(dev, 0x0)                        &&
                !ssd1306_set_segment_remapping_enabled(dev, true)                &&
                !ssd1306_set_scan_direction_fwd(dev, true)                       &&
                !ssd1306_set_com_pin_hw_config(dev, pin_cfg)                     &&
                !ssd1306_set_contrast(dev, 0x9f)                                 &&
                !ssd1306_set_precharge_period(dev, 0xf1)                         &&
                !ssd1306_set_deseltct_lvl(dev, 0x40)                             &&
                !ssd1306_set_whole_display_lighting(dev, true)                   &&
                !ssd1306_set_inversion(dev, false)                               &&
                !ssd1306_display_on(dev, true)) {
            return 0;
        }
        break;
    }
    return -EIO;
}

static int sh1106_go_coordinate(const ssd1306_t *dev, uint8_t x, uint8_t y)
{
    if (x >= dev->width || y >= (dev->height / 8))
        return -EINVAL;
    int err = 0;
    x += 2; //offset : panel is 128 ; RAM is 132 for sh1106
    if ((err = ssd1306_command(dev, SH1106_SET_PAGE_ADDRESS + y))) // Set row
        return err;
    if ((err = ssd1306_command(dev, SH1106_SET_LOW_COL_ADDR | (x & 0xf))))  // Set lower column address
        return err;
    return ssd1306_command(dev, SH1106_SET_HIGH_COL_ADDR | (x >> 4)); //Set higher column address
}

int ssd1306_load_frame_buffer(const ssd1306_t *dev, uint8_t buf[])
{
    uint16_t i;
#if (SSD1306_SPI3_SUPPORT)
    uint8_t j;
#endif
#if (SSD1306_I2C_SUPPORT)
    uint8_t tab[16] = { 0 };
#endif
    size_t len = dev->width * dev->height / 8;
    if (dev->screen == SSD1306_SCREEN) {
        ssd1306_set_column_addr(dev, 0, dev->width - 1);
        ssd1306_set_page_addr(dev, 0, dev->height / 8 - 1);
    }

    switch (dev->protocol) {
#if (SSD1306_I2C_SUPPORT)
        case SSD1306_PROTO_I2C:
            for (i = 0; i < len; i++) {
                if (dev->screen == SH1106_SCREEN && i % dev->width == 0)
                    sh1106_go_coordinate(dev, 0, i / dev->width);
                i2c_send(dev, 0x40, buf ? &buf[i] : tab, 16);
                i += 15;
            }
            break;
#endif
#if (SSD1306_SPI4_SUPPORT)
        case SSD1306_PROTO_SPI4:
            gpio_write(dev->cs_pin, false);
            if (dev->screen == SSD1306_SCREEN) {
                gpio_write(dev->dc_pin, true); // data mode
                if (buf)
                    spi_transfer(SPI_BUS, buf, NULL, len, SPI_8BIT);
                else
                    spi_repeat_send_8(SPI_BUS, 0, len);
            }
            else {
                for (i = 0; i < (dev->height / 8); i++) {
                    sh1106_go_coordinate(dev, 0, i);
                    gpio_write(dev->dc_pin, true); // data mode
                    gpio_write(dev->cs_pin, false);
                    if (buf)
                        spi_transfer(SPI_BUS, &buf[dev->width * i], NULL, dev->width, SPI_8BIT);
                    else
                        spi_repeat_send_8(SPI_BUS, 0, dev->width);
                }
            }
            gpio_write(dev->cs_pin, true);
            break;
#endif
#if (SSD1306_SPI3_SUPPORT)
        case SSD1306_PROTO_SPI3:
            gpio_write(dev->cs_pin, false);
            if (dev->screen == SSD1306_SCREEN) {
                spi_set_command(SPI_BUS, 1, 1); // data mode
                if (buf) {
                    for (i = 0; i < len; i++) {
                        spi_transfer(SPI_BUS, &buf[i], NULL, 1, SPI_8BIT);
                    }
                }
                else {
                    for (i = 0; i < len; i++) {
                        spi_transfer_8(SPI_BUS, 0);
                    }
                }
            }
            else {
                for (i = 0; i < (dev->height / 8); i++) {
                    sh1106_go_coordinate(dev, 0, i);
                    spi_set_command(SPI_BUS, 1, 1); // data mode
                    gpio_write(dev->cs_pin, false);
                    if (buf)
                        for (j = 0; j < dev->width; j++)
                            spi_transfer_8(SPI_BUS, buf[dev->width * i + j]);
                    else
                        for (j = 0; j < dev->width; j++)
                            spi_transfer_8(SPI_BUS, buf[dev->width * i + j]);
                }
            }
            spi_clear_command(SPI_BUS);
            gpio_write(dev->cs_pin, true);
            break;
#endif
        default:
            debug("Unsupported protocol");
            return -EPROTONOSUPPORT;
    }

    return 0;
}

int ssd1306_display_on(const ssd1306_t *dev, bool on)
{
    return ssd1306_command(dev, on ? SSD1306_SET_DISPLAY_ON : SSD1306_SET_DISPLAY_OFF);
}

int ssd1306_set_display_start_line(const ssd1306_t *dev, uint8_t start)
{
    if (start > 63)
        return -EINVAL;

    return ssd1306_command(dev, SSD1306_SET_DISP_START_LINE | start);
}

int ssd1306_set_display_offset(const ssd1306_t *dev, uint8_t offset)
{
    if (offset > 63)
        return -EINVAL;

    int err = 0;
    if ((err = ssd1306_command(dev, SSD1306_SET_DISPLAY_OFFSET)))
        return err;

    return ssd1306_command(dev, offset);
}

int sh1106_set_charge_pump_voltage(const ssd1306_t *dev, sh1106_voltage_t select)
{
    if (dev->screen == SSD1306_SCREEN) {
        debug("Unsupported screen type");
        return -ENOTSUP;
    }
    return ssd1306_command(dev, select | SH1106_CHARGE_PUMP_VALUE);
}

int ssd1306_set_charge_pump_enabled(const ssd1306_t *dev, bool enabled)
{
    int err = 0;
    switch (dev->screen) {
        case SH1106_SCREEN:
            if ((err = ssd1306_command(dev, SH1106_SET_CHARGE_PUMP)))
                return err;
            return ssd1306_command(dev, enabled ? SH1106_CHARGE_PUMP_EN : SH1106_CHARGE_PUMP_DIS);
            break;
        case SSD1306_SCREEN:
            if ((err = ssd1306_command(dev, SSD1306_SET_CHARGE_PUMP)))
                return err;
            return ssd1306_command(dev, enabled ? SSD1306_CHARGE_PUMP_EN : SSD1306_CHARGE_PUMP_DIS);
            break;
        default:
            debug("Unsupported screen type");
            return -ENOTSUP;
    }
}

int ssd1306_set_mem_addr_mode(const ssd1306_t *dev, ssd1306_mem_addr_mode_t mode)
{
    if (dev->screen == SH1106_SCREEN) {
        debug("Unsupported screen type");
        return -ENOTSUP;
    }
    int err = 0;
    if ((err = ssd1306_command(dev, SSD1306_SET_MEM_ADDR_MODE)))
        return err;

    return ssd1306_command(dev, mode);
}

int ssd1306_set_segment_remapping_enabled(const ssd1306_t *dev, bool on)
{
    return ssd1306_command(dev, on ? SSD1306_SET_SEGMENT_REMAP1 : SSD1306_SET_SEGMENT_REMAP0);
}

int ssd1306_set_scan_direction_fwd(const ssd1306_t *dev, bool fwd)
{
    return ssd1306_command(dev, fwd ? SSD1306_SET_SCAN_DIR_FWD : SSD1306_SET_SCAN_DIR_BWD);
}

int ssd1306_set_com_pin_hw_config(const ssd1306_t *dev, uint8_t config)
{
    int err = 0;
    if ((err = ssd1306_command(dev, SSD1306_SET_COM_PINS_HW_CFG)))
        return err;

    return ssd1306_command(dev, config & SSD1306_COM_PINS_HW_CFG_MASK);
}

int ssd1306_set_contrast(const ssd1306_t *dev, uint8_t contrast)
{
    int err = 0;
    if ((err = ssd1306_command(dev, SSD1306_SET_CONTRAST)))
        return err;

    return ssd1306_command(dev, contrast);
}

int ssd1306_set_inversion(const ssd1306_t *dev, bool on)
{
    return ssd1306_command(dev, on ? SSD1306_SET_INVERSION_ON : SSD1306_SET_INVERSION_OFF);
}

int ssd1306_set_osc_freq(const ssd1306_t *dev, uint8_t osc_freq)
{
    int err = 0;
    if ((err = ssd1306_command(dev, SSD1306_SET_OSC_FREQ)))
        return err;

    return ssd1306_command(dev, osc_freq);
}

int ssd1306_set_mux_ratio(const ssd1306_t *dev, uint8_t ratio)
{
    if (ratio < 15 || ratio > 63)
        return -EINVAL;

    int err = 0;
    if ((err = ssd1306_command(dev, SSD1306_SET_MUX_RATIO)))
        return err;

    return ssd1306_command(dev, ratio);
}

int ssd1306_set_column_addr(const ssd1306_t *dev, uint8_t start, uint8_t stop)
{
    int err = 0;
    if ((err = ssd1306_command(dev, SSD1306_SET_COL_ADDR)))
        return err;

    if ((err = ssd1306_command(dev, start)))
        return err;

    return ssd1306_command(dev, stop);
}

int ssd1306_set_page_addr(const ssd1306_t *dev, uint8_t start, uint8_t stop)
{
    int err = 0;
    if ((err = ssd1306_command(dev, SSD1306_SET_PAGE_ADDR)))
        return err;

    if ((err = ssd1306_command(dev, start)))
        return err;

    return ssd1306_command(dev, stop);
}

int ssd1306_set_precharge_period(const ssd1306_t *dev, uint8_t prchrg)
{
    int err = 0;
    if ((err = ssd1306_command(dev, SSD1306_SET_PRE_CHRG_PER)))
        return err;

    return ssd1306_command(dev, prchrg);
}

int ssd1306_set_deseltct_lvl(const ssd1306_t *dev, uint8_t lvl)
{
    int err = 0;
    if ((err = ssd1306_command(dev, SSD1306_SET_DESEL_LVL)))
        return err;

    return ssd1306_command(dev, lvl);
}

int ssd1306_set_whole_display_lighting(const ssd1306_t *dev, bool light)
{
    return ssd1306_command(dev, light ? SSD1306_SET_ENTIRE_DISP_ON : SSD1306_SET_ENTIRE_DISP_OFF);
}

/* one byte of xbm - 8 dots in line of picture source
 * one byte of fb - 8 rows for 1 column of screen
 */
int ssd1306_load_xbm(const ssd1306_t *dev, uint8_t *xbm, uint8_t *fb)
{
    uint8_t bit = 0;

    int row = 0;
    int column = 0;
    for (row = 0; row < dev->height; row++) {
        for (column = 0; column < dev->width / 8; column++) {
            uint16_t xbm_offset = row * 16 + column;
            for (bit = 0; bit < 8; bit++) {
                if (*(xbm + xbm_offset) & 1 << bit) {
                    *(fb + dev->width * (row / 8) + column * 8 + bit) |= 1 << row % 8;
                }
            }
        }
    }

    return ssd1306_load_frame_buffer(dev, fb);
}

int ssd1306_draw_pixel(const ssd1306_t *dev, uint8_t *fb, int8_t x, int8_t y, ssd1306_color_t color)
{
    uint16_t index;

    if ((x >= dev->width) || (x < 0) || (y >= dev->height) || (y < 0))
        return -EINVAL;

    index = x + (y / 8) * dev->width;
    switch (color) {
        case OLED_COLOR_WHITE:
            fb[index] |= (1 << (y & 7));
            break;
        case OLED_COLOR_BLACK:
            fb[index] &= ~(1 << (y & 7));
            break;
        case OLED_COLOR_INVERT:
            fb[index] ^= (1 << (y & 7));
            break;
        default:
            break;
    }
    return 0;
}

int ssd1306_draw_hline(const ssd1306_t *dev, uint8_t *fb, int8_t x, int8_t y, uint8_t w, ssd1306_color_t color)
{
    uint16_t index;
    uint8_t mask, t;

    // boundary check
    if ((x >= dev->width) || (x < 0) || (y >= dev->height) || (y < 0))
        return -EINVAL;
    if (w == 0)
        return -EINVAL;
    if (x + w > dev->width)
        w = dev->width - x;

    t = w;
    index = x + (y / 8) * dev->width;
    mask = 1 << (y & 7);
    switch (color) {
        case OLED_COLOR_WHITE:
            while (t--) {
                fb[index] |= mask;
                ++index;
            }
            break;
        case OLED_COLOR_BLACK:
            mask = ~mask;
            while (t--) {
                fb[index] &= mask;
                ++index;
            }
            break;
        case OLED_COLOR_INVERT:
            while (t--) {
                fb[index] ^= mask;
                ++index;
            }
            break;
        default:
            break;
    }
    return 0;
}

int ssd1306_draw_vline(const ssd1306_t *dev, uint8_t *fb, int8_t x, int8_t y, uint8_t h, ssd1306_color_t color)
{
    uint16_t index;
    uint8_t mask, mod, t;

    // boundary check
    if ((x >= dev->width) || (x < 0) || (y >= dev->height) || (y < 0))
        return -EINVAL;
    if (h == 0)
        return -EINVAL;
    if (y + h > dev->height)
        h = dev->height - y;

    t = h;
    index = x + (y / 8) * dev->width;
    mod = y & 7;
    if (mod) // partial line that does not fit into byte at top
    {
        // Magic from Adafruit
        mod = 8 - mod;
        static const uint8_t premask[8] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };
        mask = premask[mod];
        if (t < mod)
            mask &= (0xFF >> (mod - t));
        switch (color) {
            case OLED_COLOR_WHITE:
                fb[index] |= mask;
                break;
            case OLED_COLOR_BLACK:
                fb[index] &= ~mask;
                break;
            case OLED_COLOR_INVERT:
                fb[index] ^= mask;
                break;
            default:
                break;
        }

        if (t < mod)
            return 0;
        t -= mod;
        index += dev->width;
    }
    if (t >= 8) // byte aligned line at middle
            {
        switch (color) {
            case OLED_COLOR_WHITE:
                do {
                    fb[index] = 0xff;
                    index += dev->width;
                    t -= 8;
                } while (t >= 8);
                break;
            case OLED_COLOR_BLACK:
                do {
                    fb[index] = 0x00;
                    index += dev->width;
                    t -= 8;
                } while (t >= 8);
                break;
            case OLED_COLOR_INVERT:
                do {
                    fb[index] = ~fb[index];
                    index += dev->width;
                    t -= 8;
                } while (t >= 8);
                break;
            default:
                break;
        }
    }
    if (t) // partial line at bottom
    {
        mod = t & 7;
        static const uint8_t postmask[8] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F };
        mask = postmask[mod];
        switch (color) {
            case OLED_COLOR_WHITE:
                fb[index] |= mask;
                break;
            case OLED_COLOR_BLACK:
                fb[index] &= ~mask;
                break;
            case OLED_COLOR_INVERT:
                fb[index] ^= mask;
                break;
            default:
                break;
        }
    }
    return 0;
}

int ssd1306_draw_rectangle(const ssd1306_t *dev, uint8_t *fb, int8_t x, int8_t y, uint8_t w, uint8_t h, ssd1306_color_t color)
{
    int err = 0;
    if ((err = ssd1306_draw_hline(dev, fb, x, y, w, color)))
        return err;
    if ((err = ssd1306_draw_hline(dev, fb, x, y + h - 1, w, color)))
        return err;
    if ((err = ssd1306_draw_vline(dev, fb, x, y, h, color)))
        return err;
    return ssd1306_draw_vline(dev, fb, x + w - 1, y, h, color);
}

int ssd1306_fill_rectangle(const ssd1306_t *dev, uint8_t *fb, int8_t x, int8_t y, uint8_t w, uint8_t h, ssd1306_color_t color)
{
    // Can be optimized?
    uint8_t i;
    int err = 0;
    for (i = x; i < x + w; ++i)
        if ((err = ssd1306_draw_vline(dev, fb, i, y, h, color)))
            return err;
    return 0;
}

int ssd1306_draw_circle(const ssd1306_t *dev, uint8_t *fb, int8_t x0, int8_t y0, uint8_t r, ssd1306_color_t color)
{
    // Refer to http://en.wikipedia.org/wiki/Midpoint_circle_algorithm for the algorithm
    int8_t x = r;
    int8_t y = 1;
    int16_t radius_err = 1 - x;
    int err = 0;

    if (r == 0)
        return -EINVAL;

    if ((err = ssd1306_draw_pixel(dev, fb, x0 - r, y0,     color)))
        return err;
    if ((err = ssd1306_draw_pixel(dev, fb, x0 + r, y0,     color)))
        return err;
    if ((err = ssd1306_draw_pixel(dev, fb, x0,     y0 - r, color)))
        return err;
    if ((err = ssd1306_draw_pixel(dev, fb, x0,     y0 + r, color)))
        return err;

    while (x >= y) {
        if ((err = ssd1306_draw_pixel(dev, fb, x0 + x, y0 + y, color)))
            return err;
        if ((err = ssd1306_draw_pixel(dev, fb, x0 - x, y0 + y, color)))
            return err;
        if ((err = ssd1306_draw_pixel(dev, fb, x0 + x, y0 - y, color)))
            return err;
        if ((err = ssd1306_draw_pixel(dev, fb, x0 - x, y0 - y, color)))
            return err;
        if (x != y) {
            /* Otherwise the 4 drawings below are the same as above, causing
             * problem when color is INVERT
             */
            if ((err = ssd1306_draw_pixel(dev, fb, x0 + y, y0 + x, color)))
                return err;
            if ((err = ssd1306_draw_pixel(dev, fb, x0 - y, y0 + x, color)))
                return err;
            if ((err = ssd1306_draw_pixel(dev, fb, x0 + y, y0 - x, color)))
                return err;
            if ((err = ssd1306_draw_pixel(dev, fb, x0 - y, y0 - x, color)))
                return err;
        }
        ++y;
        if (radius_err < 0) {
            radius_err += 2 * y + 1;
        }
        else {
            --x;
            radius_err += 2 * (y - x + 1);
        }

    }
    return 0;
}

int ssd1306_fill_circle(const ssd1306_t *dev, uint8_t *fb, int8_t x0, int8_t y0, uint8_t r, ssd1306_color_t color)
{
    int8_t x = 1;
    int8_t y = r;
    int16_t radius_err = 1 - y;
    int8_t x1;
    int err = 0;

    if (r == 0)
        return -EINVAL;

    if ((err = ssd1306_draw_vline(dev, fb, x0, y0 - r, 2 * r + 1, color))) // Center vertical line
        return err;
    while (y >= x) {
        if ((err = ssd1306_draw_vline(dev, fb, x0 - x, y0 - y, 2 * y + 1, color)))
            return err;
        if ((err = ssd1306_draw_vline(dev, fb, x0 + x, y0 - y, 2 * y + 1, color)))
            return err;
        if (color != OLED_COLOR_INVERT) {
            if ((err = ssd1306_draw_vline(dev, fb, x0 - y, y0 - x, 2 * x + 1, color)))
                return err;
            if ((err = ssd1306_draw_vline(dev, fb, x0 + y, y0 - x, 2 * x + 1, color)))
                return err;
        }
        ++x;
        if (radius_err < 0) {
            radius_err += 2 * x + 1;
        }
        else {
            --y;
            radius_err += 2 * (x - y + 1);
        }
    }

    if (color == OLED_COLOR_INVERT) {
        x1 = x; // Save where we stopped

        y = 1;
        x = r;
        radius_err = 1 - x;
        if ((err = ssd1306_draw_hline(dev, fb, x0 + x1, y0, r - x1 + 1, color)))
            return err;
        if ((err = ssd1306_draw_hline(dev, fb, x0 - r, y0, r - x1 + 1, color)))
            return err;
        while (x >= y) {
            if ((err = ssd1306_draw_hline(dev, fb, x0 + x1, y0 - y, x - x1 + 1, color)))
                return err;
            if ((err = ssd1306_draw_hline(dev, fb, x0 + x1, y0 + y, x - x1 + 1, color)))
                return err;
            if ((err = ssd1306_draw_hline(dev, fb, x0 - x, y0 - y, x - x1 + 1, color)))
                return err;
            if ((err = ssd1306_draw_hline(dev, fb, x0 - x, y0 + y, x - x1 + 1, color)))
                return err;
            ++y;
            if (radius_err < 0) {
                radius_err += 2 * y + 1;
            }
            else {
                --x;
                radius_err += 2 * (y - x + 1);
            }
        }
    }
    return 0;
}

int ssd1306_draw_line(const ssd1306_t *dev, uint8_t *fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, ssd1306_color_t color)
{
    if ((x0 >= dev->width) || (x0 < 0) || (y0 >= dev->height) || (y0 < 0))
        return -EINVAL;
    if ((x1 >= dev->width) || (x1 < 0) || (y1 >= dev->height) || (y1 < 0))
        return -EINVAL;

    int err;
    bool steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        swap(x0, y0);
        swap(x1, y1);
    }

    if (x0 > x1) {
        swap(x0, x1);
        swap(y0, y1);
    }

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int16_t errx = dx / 2;
    int16_t ystep;

    if (y0 < y1) {
        ystep = 1;
    }
    else {
        ystep = -1;
    }

    for (; x0 <= x1; x0++) {
        if (steep) {
            if ((err = ssd1306_draw_pixel(dev, fb, y0, x0, color)))
                return err;
        }
        else {
            if ((err = ssd1306_draw_pixel(dev, fb, x0, y0, color)))
                return err;
        }
        errx -= dy;
        if (errx < 0) {
            y0 += ystep;
            errx += dx;
        }
    }
    return 0;
}

int ssd1306_draw_triangle(const ssd1306_t *dev, uint8_t *fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
        ssd1306_color_t color)
{
    int err;
    if ((err = ssd1306_draw_line(dev, fb, x0, y0, x1, y1, color)))
        return err;
    if ((err = ssd1306_draw_line(dev, fb, x1, y1, x2, y2, color)))
        return err;
    return ssd1306_draw_line(dev, fb, x2, y2, x0, y0, color);
}

int ssd1306_fill_triangle(const ssd1306_t *dev, uint8_t *fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
        ssd1306_color_t color)
{
    int16_t a, b, y, last;
    int err;

    // Sort coordinates by Y order (y2 >= y1 >= y0)
    if (y0 > y1) {
        swap(y0, y1);swap(x0, x1);
    }
    if (y1 > y2) {
        swap(y2, y1);swap(x2, x1);
    }
    if (y0 > y1) {
        swap(y0, y1);swap(x0, x1);
    }

    if (y0 == y2) { // Handle awkward all-on-same-line case as its own thing
        a = b = x0;
        if (x1 < a)      a = x1;
        else if (x1 > b) b = x1;
        if (x2 < a)      a = x2;
        else if (x2 > b) b = x2;
        if ((err = ssd1306_draw_hline(dev, fb, a, y0, b - a + 1, color)))
            return err;
        return 0;
    }

    int16_t
    dx01 = x1 - x0,
    dy01 = y1 - y0,
    dx02 = x2 - x0,
    dy02 = y2 - y0,
    dx12 = x2 - x1,
    dy12 = y2 - y1,
    sa   = 0,
    sb   = 0;

    // For upper part of triangle, find scanline crossings for segments
    // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
    // is included here (and second loop will be skipped, avoiding a /0
    // error there), otherwise scanline y1 is skipped here and handled
    // in the second loop...which also avoids a /0 error here if y0=y1
    // (flat-topped triangle).
    if (y1 == y2) last = y1;     // Include y1 scanline
    else          last = y1 - 1; // Skip it

    for (y = y0; y <= last; y++) {
        a = x0 + sa / dy01;
        b = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        /* longhand:
        a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
        b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        */
        if (a > b) swap(a, b);
        if ((err = ssd1306_draw_hline(dev, fb, a, y, b - a + 1, color)))
            return err;
    }

    // For lower part of triangle, find scanline crossings for segments
    // 0-2 and 1-2.  This loop is skipped if y1=y2.
    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for (; y <= y2; y++) {
        a = x1 + sa / dy12;
        b = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        /* longhand:
        a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
        b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        */
        if (a > b) swap(a, b);
        if ((err = ssd1306_draw_hline(dev, fb, a, y, b - a + 1, color)))
            return err;
    }
    return 0;
}

int ssd1306_draw_char(const ssd1306_t *dev, uint8_t *fb, const font_info_t *font, uint8_t x, uint8_t y, char c, ssd1306_color_t foreground,
        ssd1306_color_t background)
{
    uint8_t i, j;
    const uint8_t *bitmap;
    uint8_t line = 0;
    int err;

    if (font == NULL)
        return 0;

    const font_char_desc_t *d = font_get_char_desc(font, c);
    if (d == NULL)
        return 0;

    bitmap = font->bitmap + d->offset;
    for (j = 0; j < font->height; ++j) {
        for (i = 0; i < d->width; ++i) {
            if (i % 8 == 0) {
                line = bitmap[(d->width + 7) / 8 * j + i / 8]; // line data
            }
            if (line & 0x80) {
                err = ssd1306_draw_pixel(dev, fb, x + i, y + j, foreground);
            }
            else {
                switch (background) {
                    case OLED_COLOR_TRANSPARENT:
                        // Not drawing for transparent background
                        break;
                    case OLED_COLOR_WHITE:
                    case OLED_COLOR_BLACK:
                        err = ssd1306_draw_pixel(dev, fb, x + i, y + j, background);
                        break;
                    case OLED_COLOR_INVERT:
                        // I don't know why I need invert background
                        break;
                }
            }
            if (err)
                return -ERANGE;
            line = line << 1;
        }
    }
    return d->width;
}

int ssd1306_draw_string(const ssd1306_t *dev, uint8_t *fb, const font_info_t *font, uint8_t x, uint8_t y, const char *str,
        ssd1306_color_t foreground, ssd1306_color_t background)
{
    uint8_t t = x;
    int err;

    if (font == NULL || str == NULL)
        return 0;

    while (*str) {
        if ((err = ssd1306_draw_char(dev, fb, font, x, y, *str, foreground, background)) < 0)
            return err;
        x += err;
        ++str;
        if (*str)
            x += font->c;
    }
    return x - t;
}

int ssd1306_stop_scroll(const ssd1306_t *dev)
{
    return ssd1306_command(dev, SSD1306_SCROLL_DISABLE);
}

int ssd1306_start_scroll_hori(const ssd1306_t *dev, bool way, uint8_t start, uint8_t stop, ssd1306_scroll_t frame)
{
    int err;

    if (way) {
        if ((err = ssd1306_command(dev, SSD1306_SCROLL_HOR_LEFT)))
            return err;
    }
    else {
        if ((err = ssd1306_command(dev, SSD1306_SCROLL_HOR_RIGHT)))
            return err;
    }
    if (!ssd1306_command(dev, 0x00)             &&   //dummy
            !ssd1306_command(dev, (start&0x07))     &&
            !ssd1306_command(dev, frame)            &&
            !ssd1306_command(dev, (stop&0x07))      &&
            !ssd1306_command(dev, 0x00)             && //dummy
            !ssd1306_command(dev, 0xFF)             && //dummy
            !ssd1306_command(dev, SSD1306_SCROLL_ENABLE)) {
        return 0;
    }
    return -EIO;
}

int ssd1306_start_scroll_hori_vert(const ssd1306_t *dev, bool way, uint8_t start, uint8_t stop, uint8_t dy, ssd1306_scroll_t frame)
{
    //this function dont work well if no vertical setting.
    if ((!dy) || (dy > 63))
        return -EINVAL;
    int err;

    //vertical scrolling selection (all screen)
    if ((err = ssd1306_command(dev, 0xA3)))
        return err;
    if ((err = ssd1306_command(dev, 0)))
        return err;
    if ((err = ssd1306_command(dev, dev->height)))
        return err;

    if (way) {
        if ((err = ssd1306_command(dev, SSD1306_SCROLL_HOR_VER_LEFT)))
            return err;
    }
    else {
        if ((err = ssd1306_command(dev, SSD1306_SCROLL_HOR_VER_RIGHT)))
            return err;
    }
    if (!ssd1306_command(dev, 0x00)             &&   //dummy
            !ssd1306_command(dev, (start&0x07))     &&
            !ssd1306_command(dev, frame)            &&
            !ssd1306_command(dev, (stop&0x07))      &&
            !ssd1306_command(dev, dy)               &&
            !ssd1306_command(dev, SSD1306_SCROLL_ENABLE)) {
        return 0;
    }
    return -EIO;
}
