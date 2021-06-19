/**
 * @file SSD1963.c
 * @date   2016-2018
 * @author zaltora  (https://github.com/Zaltora)
 * @author urx (https://github.com/urx)
 * @author Ruslan V. Uss (https://github.com/UncleRus)
 * @copyright MIT License.
 */

/*********************
 *      INCLUDES
 *********************/
#include "SSD1306.h"
#if USE_SSD1306 != 0

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lv_core/lv_vdb.h"
#include "lvgl/lv_core/lv_refr.h"

/*********************
 *      DEFINES
 *********************/
#ifndef SSD1306_I2C_SUPPORT
#define SSD1306_I2C_SUPPORT 1
#endif

#ifndef SSD1306_SPI_4_WIRE_SUPPORT
#define SSD1306_SPI_4_WIRE_SUPPORT 1
#endif

#ifndef SSD1306_SPI_3_WIRE_SUPPORT
#define SSD1306_SPI_3_WIRE_SUPPORT 1
#endif

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

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void inline _rounder(lv_area_t *a);
static int inline _load_frame_buffer(const ssd1306_t *dev, uint8_t* buf, uint8_t x1, uint8_t y1,  uint8_t x2, uint8_t y2);

/**********************
 *  STATIC VARIABLES
 **********************/
static const ssd1306_t* _dev;

/**********************
 *      MACROS
 **********************/
#if SSD1306_DEBUG
#include <stdio.h>
#define debug(fmt, ...) printf("%s: " fmt "\n", "SSD1306", ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

#if SSD1306_ERR_CHECK
#define err_control(fn) do { int err; if((err = fn)) return err; } while (0)
#else
#define err_control(fn) (fn)
#endif

#if LV_COLOR_DEPTH != 1 || LV_VDB_PX_BPP != 1
#error "LV_COLOR_DEPTH and LV_VDB_PX_BPP need to be set to 1"
#endif
#if LV_VDB_SIZE < ( 8 * LV_HOR_RES)
#error "LV_VDB_SIZE with 8*LV_HOR_RES minimum is needed (MAX: LV_HOR_RES*LV_VER_RES)"
#endif
/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void ssd1306_vdb_wr(uint8_t * buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y, lv_color_t color, lv_opa_t opa)
{
    buf += buf_w * (y >> 3) + x;
    if(color.full)
    {
        (*buf) |= (1 << (y % 8));
    }
    else
    {
        (*buf) &= ~(1 << (y % 8));
    }
}

void ssd1306_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const lv_color_t * color_p)
{
    /*Return if the area is out the screen*/
    if (x2 < 0 || y2 < 0 || x1 > LV_HOR_RES - 1 || y1 > LV_VER_RES - 1)
    {
        lv_flush_ready();
        return;
    }
    /*Return if the screen is uninitialized*/
    if(!_dev)
    {
        lv_flush_ready();
        return;
    }

    _load_frame_buffer(_dev, (uint8_t*)color_p, x1, y1, x2, y2);
    lv_flush_ready();
}

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
    switch (dev->protocol)
    {
#if (SSD1306_I2C_SUPPORT)
    case SSD1306_PROTO_I2C:
        err_control(i2c_send(dev->i2c_dev, 0x00, &cmd, 1));
        break;
#endif
#if (SSD1306_SPI_4_WIRE_SUPPORT)
    case SSD1306_PROTO_SPI4:
        err_control(spi4wire_send(dev->spi_dev, 0,  &cmd, 1, 1));
        break;
#endif
#if (SSD1306_SPI_3_WIRE_SUPPORT)
    case SSD1306_PROTO_SPI3:
        err_control(spi3wire_send(dev->spi_dev, 0, &cmd, 1, 1));
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
    if(_dev) //already init
    {
        return -ENXIO;
    }
    //Reset pin setup if needed
    if(dev->rst_pin != LV_DRIVER_NOPIN)
    {
        lv_gpio_write(dev->rst_pin, 0);
        lv_delay_us(10);
        lv_gpio_write(dev->rst_pin, 1);
    }

    uint8_t pin_cfg;
    switch (dev->height)
    {
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
    switch (dev->width)
    {
    case 128:
        break;
    case 96:
        break;
    default:
        debug("Unsupported screen witdh");
        return -ENOTSUP;
    }

    //We send 8 pixel at once (page coordinate ). If VDB is not set correctly,
    //we need round the y coordinate
    lv_refr_set_round_cb(_rounder);

    switch (dev->protocol)
    {
#if (SSD1306_I2C_SUPPORT)
    case SSD1306_PROTO_I2C:
        break;
#endif
#if (SSD1306_SPI_4_WIRE_SUPPORT)
    case SSD1306_PROTO_SPI4:
        break;
#endif
#if (SSD1306_SPI_3_WIRE_SUPPORT)
    case SSD1306_PROTO_SPI3:
        break;
#endif
    default:
        debug("Unsupported protocol");
        return -EPROTONOSUPPORT;
    }
    switch (dev->screen)
    {
    case SSD1306_SCREEN:
        if (!ssd1306_display_on(dev, false)
            && !ssd1306_set_osc_freq(dev, 0x80)
            && !ssd1306_set_mux_ratio(dev, dev->height - 1)
            && !ssd1306_set_display_offset(dev, 0x0)
            && !ssd1306_set_display_start_line(dev, 0x0)
            && !ssd1306_set_charge_pump_enabled(dev, true)
            && !ssd1306_set_mem_addr_mode(dev, SSD1306_ADDR_MODE_HORIZONTAL)
            && !ssd1306_set_segment_remapping_enabled(dev, false)
            && !ssd1306_set_scan_direction_fwd(dev, true)
            && !ssd1306_set_com_pin_hw_config(dev, pin_cfg)
            && !ssd1306_set_contrast(dev, 0x9f)
            && !ssd1306_set_precharge_period(dev, 0xf1)
            && !ssd1306_set_deseltct_lvl(dev, 0x40)
            && !ssd1306_set_whole_display_lighting(dev, true)
            && !ssd1306_set_inversion(dev, false)
            && !ssd1306_display_on(dev, true))
        {
            _dev = dev;
            return 0;
        }
        break;
    case SH1106_SCREEN:
        if (!ssd1306_display_on(dev, false)
            && !ssd1306_set_charge_pump_enabled(dev, true)
            && !sh1106_set_charge_pump_voltage(dev, SH1106_VOLTAGE_74)
            && !ssd1306_set_osc_freq(dev, 0x80)
            && !ssd1306_set_mux_ratio(dev, dev->height - 1)
            && !ssd1306_set_display_offset(dev, 0x0)
            && !ssd1306_set_display_start_line(dev, 0x0)
            && !ssd1306_set_segment_remapping_enabled(dev, true)
            && !ssd1306_set_scan_direction_fwd(dev, true)
            && !ssd1306_set_com_pin_hw_config(dev, pin_cfg)
            && !ssd1306_set_contrast(dev, 0x9f)
            && !ssd1306_set_precharge_period(dev, 0xf1)
            && !ssd1306_set_deseltct_lvl(dev, 0x40)
            && !ssd1306_set_whole_display_lighting(dev, true)
            && !ssd1306_set_inversion(dev, false)
            && !ssd1306_display_on(dev, true))
        {
            _dev = dev;
            return 0;
        }
        break;
    }
    return -EIO;
}

int ssd1306_deinit(const ssd1306_t *dev)
{
    int err;
    if(!dev)
    {
        return -ENXIO;
    }
    //Reset pin to clear all configs
    if(dev->rst_pin != LV_DRIVER_NOPIN)
    {
        lv_gpio_write(dev->rst_pin, 0);
        lv_delay_us(10);
        lv_gpio_write(dev->rst_pin, 1);
    }
    //Display off
    err = ssd1306_display_on(dev, false);
    if(err)
    {
        return err;
    }
    _dev = NULL;
    return 0;
}

static int sh1106_go_coordinate(const ssd1306_t *dev, uint8_t x, uint8_t y)
{
    if (x >= dev->width || y >= (dev->height / 8))
        return -EINVAL;
    int err = 0;
    x += 2; /* offset : panel is 128 ; RAM is 132 for sh1106 */
    if ((err = ssd1306_command(dev, SH1106_SET_PAGE_ADDRESS + y ))) /* Set row */
        return err;
    if ((err = ssd1306_command(dev, SH1106_SET_LOW_COL_ADDR | (x & 0xf)))) /* Set lower column address */
        return err;
    return ssd1306_command(dev, SH1106_SET_HIGH_COL_ADDR | (x >> 4)); /* Set higher column address */
}

int ssd1306_display_on(const ssd1306_t *dev, bool on)
{
    return ssd1306_command(dev, on ? SSD1306_SET_DISPLAY_ON : SSD1306_SET_DISPLAY_OFF);
}

int ssd1306_set_display_start_line(const ssd1306_t *dev, uint8_t start)
{
    if (start > dev->height - 1)
        return -EINVAL;

    return ssd1306_command(dev, SSD1306_SET_DISP_START_LINE | start);
}

int ssd1306_set_display_offset(const ssd1306_t *dev, uint8_t offset)
{
    if (offset > dev->height - 1)
        return -EINVAL;
    int err = 0;
    if ((err = ssd1306_command(dev, SSD1306_SET_DISPLAY_OFFSET)))
        return err;

    return ssd1306_command(dev, offset);
}

int sh1106_set_charge_pump_voltage(const ssd1306_t *dev, sh1106_voltage_t select)
{
    if (dev->screen == SSD1306_SCREEN)
    {
        debug("Unsupported screen type");
        return -ENOTSUP;
    }
    return ssd1306_command(dev, select | SH1106_CHARGE_PUMP_VALUE);
}

int ssd1306_set_charge_pump_enabled(const ssd1306_t *dev, bool enabled)
{
    int err = 0;
    switch (dev->screen)
    {
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
    if (dev->screen == SH1106_SCREEN)
    {
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

int ssd1306_stop_scroll(const ssd1306_t *dev)
{
    if (dev->screen == SH1106_SCREEN)
    {
        return -ENOTSUP;
    }
    return ssd1306_command(dev, SSD1306_SCROLL_DISABLE);
}

int ssd1306_start_scroll_hori(const ssd1306_t *dev, bool way, uint8_t start, uint8_t stop, ssd1306_scroll_t frame)
{
    int err;

    if (dev->screen == SH1106_SCREEN)
    {
        return -ENOTSUP;
    }
    if (way)
    {
        if ((err = ssd1306_command(dev, SSD1306_SCROLL_HOR_LEFT)))
            return err;
    }
    else
    {
        if ((err = ssd1306_command(dev, SSD1306_SCROLL_HOR_RIGHT)))
            return err;
    }
    if (!ssd1306_command(dev, 0x00) && /* dummy */
        !ssd1306_command(dev, (start & 0x07)) &&
        !ssd1306_command(dev, frame) &&
        !ssd1306_command(dev, (stop & 0x07)) &&
        !ssd1306_command(dev, 0x00) && /* dummy */
        !ssd1306_command(dev, 0xFF) && /* dummy */
        !ssd1306_command(dev, SSD1306_SCROLL_ENABLE))
    {
        return 0;
    }
    return -EIO;
}

int ssd1306_start_scroll_hori_vert(const ssd1306_t *dev, bool way, uint8_t start, uint8_t stop, uint8_t dy,
    ssd1306_scroll_t frame)
{
    if (dev->screen == SH1106_SCREEN)
    {
        return -ENOTSUP;
    }
    /* this function dont work well if no vertical setting. */
    if ((!dy) || (dy > 63))
        return -EINVAL;
    int err;

    /* vertical scrolling selection (all screen) */
    if ((err = ssd1306_command(dev, 0xA3)))
        return err;
    if ((err = ssd1306_command(dev, 0)))
        return err;
    if ((err = ssd1306_command(dev, dev->height)))
        return err;

    if (way)
    {
        if ((err = ssd1306_command(dev, SSD1306_SCROLL_HOR_VER_LEFT)))
            return err;
    }
    else
    {
        if ((err = ssd1306_command(dev, SSD1306_SCROLL_HOR_VER_RIGHT)))
            return err;
    }
    if (!ssd1306_command(dev, 0x00) && /* dummy */
        !ssd1306_command(dev, (start & 0x07)) &&
        !ssd1306_command(dev, frame) &&
        !ssd1306_command(dev, (stop & 0x07)) &&
        !ssd1306_command(dev, dy) &&
        !ssd1306_command(dev, SSD1306_SCROLL_ENABLE))
    {
        return 0;
    }
    return -EIO;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void inline _rounder(lv_area_t *a)
{
    /*Make the area bigger in y*/
    a->y1 = a->y1 & ~(0x7);
    a->y2 = a->y2 |  (0x7);
}

static int inline _load_frame_buffer(const ssd1306_t *dev, uint8_t* buf, uint8_t x1, uint8_t y1,  uint8_t x2, uint8_t y2)
{
    uint8_t i;
    uint16_t j;
    //convert to page
    y1 = y1 >> 3;
    y2 = y2 >> 3;
    //if(y1 != 0) y1/=8;
    //if(y2 != 0) y2/=8;
    uint8_t xlen = x2-x1+1;
    uint8_t plen = y2-y1+1;

    if (dev->screen == SSD1306_SCREEN)
    {
        err_control(ssd1306_set_column_addr(dev, x1, x2));
        err_control(ssd1306_set_page_addr(dev, y1, y2));
    }

    switch (dev->protocol)
    {
#if (SSD1306_I2C_SUPPORT)
    case SSD1306_PROTO_I2C:
        if (dev->screen == SSD1306_SCREEN)
        {
            for (j = 0; j < plen; j++)
            {
                for (i = 0; i < xlen; ) //column
                {
                    if((x2-i) >= 15) //(x2-i+1) >= 16
                    {
                        err_control(i2c_send(dev->i2c_dev, 0x40, &buf[j*xlen+i], 16));
                        i+=16;
                    }
                    else
                    {
                        err_control(i2c_send(dev->i2c_dev, 0x40, &buf[j*xlen+i], x2-i+1));
                        break; //Last data for this page
                    }
                }
            }
        }
        else
        {
            for (j = 0; j < plen ; j++)
            {
                err_control(sh1106_go_coordinate(dev, x1, j+y1));
                for (i = 0; i < xlen ; ) //column
                {
                     if((x2-i) >= 15) //(x2-i+1) >= 16
                    {
                        err_control(i2c_send(dev->i2c_dev, 0x40, &buf[j*xlen+i], 16));
                        i+=16;
                    }
                    else
                    {
                        err_control(i2c_send(dev->i2c_dev, 0x40, &buf[j*xlen+i], x2-i+1));
                        break; //Last data send for this page
                    }
                }
            }
        }
        break;
#endif
#if (SSD1306_SPI_4_WIRE_SUPPORT)
    case SSD1306_PROTO_SPI4:
        if (dev->screen == SSD1306_SCREEN)
        {
            for(j = 0;  j < plen ; j++) //page
            {
                err_control(spi4wire_send(dev->spi_dev, 1, &buf[j*xlen+x1], xlen, 1));
            }
        }
        else
        {
            for (j = 0; j < plen; j++)
            {
                err_control(sh1106_go_coordinate(dev, x1, j+y1));
                err_control(spi4wire_send(dev->spi_dev, 1, &buf[j*xlen + x1], xlen, 1));
            }
        }
        break;
#endif
#if (SSD1306_SPI_3_WIRE_SUPPORT)
    case SSD1306_PROTO_SPI3:
        if (dev->screen == SSD1306_SCREEN)
        {
            for(j = 0;  j < plen ; j++) //page
            {
                for (i = 0; i < xlen ; i++) //column
                {
                    err_control(spi3wire_send(dev->spi_dev, 1,  &buf[j*xlen+i], 1, 1));
                }
            }
        }
        else
        {
            for (j = 0; j < plen; j++)
            {
                err_control(sh1106_go_coordinate(dev, x1, j+y1));
                for (i = 0; i < xlen ; i++) //column
                {
                    err_control(spi3wire_send(dev->spi_dev, 1,  &buf[j*xlen+i], 1, 1));
                }
            }
        }
        break;
#endif
    default:
        debug("Unsupported protocol");
        return -EPROTONOSUPPORT;
    }
    return 0;
}

#endif
