/**
 * SSD1306 OLED display driver for esp-open-rtos.
 *
 * Copyright (c) 2016 urx (https://github.com/urx),
 *                    Ruslan V. Uss (https://github.com/UncleRus)
 *                    Zaltora (https://github.com/Zaltora)
 *
 * MIT Licensed as described in the file LICENSE
 */
#ifndef _SSD1306__H_
#define _SSD1306__H_

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <fonts/fonts.h>

#include "config.h"

// shifted
#if (SSD1306_I2C_SUPPORT)
    #include <i2c/i2c.h>
    #define SSD1306_I2C_ADDR_0    (0x3C)
    #define SSD1306_I2C_ADDR_1    (0x3D)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * SH1106 pump voltage value
 */
typedef enum
{
    SH1106_VOLTAGE_74 = 0, // 7.4 Volt
    SH1106_VOLTAGE_80,     // 8.0 Volt
    SH1106_VOLTAGE_84,     // 8.4 Volt
    SH1106_VOLTAGE_90      // 9.0 Volt
} sh1106_voltage_t;

/**
 * I/O protocols
 */
typedef enum
{
    SSD1306_PROTO_I2C = 0, //!< I2C
    SSD1306_PROTO_SPI4,    //!< SPI 8 bits + D/C pin
    SSD1306_PROTO_SPI3     //!< SPI 9 bits
} ssd1306_protocol_t;

/**
 * Screen type
 */
typedef enum
{
    SSD1306_SCREEN = 0,
    SH1106_SCREEN
} ssd1306_screen_t;

/**
 * Device descriptor
 */
typedef struct
{
    ssd1306_protocol_t protocol;
    ssd1306_screen_t screen;
    union
    {
#if (SSD1306_I2C_SUPPORT)
        i2c_dev_t i2c_dev;         //!< I2C devuce descriptor, used by SSD1306_PROTO_I2C
#endif
#if (SSD1306_SPI4_SUPPORT) || (SSD1306_SPI3_SUPPORT)
        uint8_t cs_pin;            //!< Chip Select GPIO pin, used by SSD1306_PROTO_SPI3, SSD1306_PROTO_SPI4
#endif
    };
#if (SSD1306_SPI4_SUPPORT)
    uint8_t dc_pin;               //!< Data/Command GPIO pin, used by SSD1306_PROTO_SPI4
#endif
    uint8_t width;                //!< Screen width, currently supported 128px, 96px
    uint8_t height;               //!< Screen height, currently supported 16px, 32px, 64px
} ssd1306_t;

/**
 * Addressing mode, see datasheet
 */
typedef enum
{
    SSD1306_ADDR_MODE_HORIZONTAL = 0,
    SSD1306_ADDR_MODE_VERTICAL,
    SSD1306_ADDR_MODE_PAGE
} ssd1306_mem_addr_mode_t;

/**
 * Drawing color
 */
typedef enum
{
    OLED_COLOR_TRANSPARENT = -1, //!< Transparent (not drawing)
    OLED_COLOR_BLACK = 0,        //!< Black (pixel off)
    OLED_COLOR_WHITE = 1,        //!< White (or blue, yellow, pixel on)
    OLED_COLOR_INVERT = 2,       //!< Invert pixel (XOR)
} ssd1306_color_t;

/**
 * Scrolling time frame interval
 */
typedef enum
{
    FRAME_5 = 0,
    FRAME_64,
    FRAME_128,
    FRAME_256,
    FRAME_3,
    FRAME_4,
    FRAME_25,
    FRAME_2

} ssd1306_scroll_t;

/**
 * Issue a single command on SSD1306.
 * @param dev Pointer to device descriptor
 * @param cmd Command
 * @return Non-zero if error occured
 */
int ssd1306_command(const ssd1306_t *dev, uint8_t cmd);

/**
 * Default init for SSD1306
 * @param dev Pointer to device descriptor
 * @return Non-zero if error occured
 */
int ssd1306_init(const ssd1306_t *dev);

/**
 * Load picture in xbm format into the SSD1306 RAM.
 * @param dev Pointer to device descriptor
 * @param xbm Pointer to xbm picture array
 * @param fb Pointer to local buffer for storing converted xbm image
 * @return Non-zero if error occured
 */
int ssd1306_load_xbm(const ssd1306_t *dev, uint8_t *xbm, uint8_t *fb);

/**
 * Load local framebuffer into the SSD1306 RAM.
 * @param dev Pointer to device descriptor
 * @param buf Pointer to framebuffer or NULL for clear RAM. Framebuffer size = width * height / 8
 * @return Non-zero if error occured
 */
int ssd1306_load_frame_buffer(const ssd1306_t *dev, uint8_t buf[]);

/**
 * Clear SSD1306 RAM.
 * @param dev Pointer to device descriptor
 * @return Non-zero if error occured
 */
inline int ssd1306_clear_screen(const ssd1306_t *dev)
{
    return ssd1306_load_frame_buffer(dev, NULL);
}

/**
 * Turn display on or off.
 * @param dev Pointer to device descriptor
 * @param on Turn on if true
 * @return Non-zero if error occured
 */
int ssd1306_display_on(const ssd1306_t *dev, bool on);

/**
 * Set the Display Start Line register to determine starting address of
 * display RAM, by selecting a value from 0 to 63. With value equal to 0,
 * RAM row 0 is mapped to COM0. With value equal to 1, RAM row 1 is mapped
 * to COM0 and so on.
 * @param dev Pointer to device descriptor
 * @param start Start line, 0..63
 * @return Non-zero if error occured
 */
int ssd1306_set_display_start_line(const ssd1306_t *dev, uint8_t start);

/**
 * Set display offset (see datasheet).
 * @param dev Pointer to device descriptor
 * @param offset Offset, 0..63
 * @return Non-zero if error occured
 */
int ssd1306_set_display_offset(const ssd1306_t *dev, uint8_t offset);

/**
 * Select charge pump voltage. See value in datasheet.
 * @param dev Pointer to device descriptor
 * @param select Select charge pump voltage value
 * @return Non-zero if error occured
 */
int sh1106_set_charge_pump_voltage(const ssd1306_t *dev, sh1106_voltage_t select);

/**
 * Enable or disable the charge pump. See application note in datasheet.
 * @param dev Pointer to device descriptor
 * @param enabled Enable charge pump if true
 * @return Non-zero if error occured
 */
int ssd1306_set_charge_pump_enabled(const ssd1306_t *dev, bool enabled);

/**
 * Set memory addressing mode. See datasheet.
 * @param dev Pointer to device descriptor
 * @param mode Addressing mode
 * @return Non-zero if error occured
 */
int ssd1306_set_mem_addr_mode(const ssd1306_t *dev, ssd1306_mem_addr_mode_t mode);

/**
 * Change the mapping between the display data column address and the
 * segment driver. See datasheet.
 * @param dev Pointer to device descriptor
 * @param on Enable segment remapping if true
 * @return Non-zero if error occured
 */
int ssd1306_set_segment_remapping_enabled(const ssd1306_t *dev, bool on);

/**
 * Set the scan direction of the COM output, allowing layout flexibility
 * in the OLED module design. Additionally, the display will show once
 * this command is issued. For example, if this command is sent during
 * normal display then the graphic display will be vertically flipped
 * immediately.
 * @param dev Pointer to device descriptor
 * @param fwd Forward direction if true, backward otherwise
 * @return Non-zero if error occured
 */
int ssd1306_set_scan_direction_fwd(const ssd1306_t *dev, bool fwd);

/**
 * Set the COM signals pin configuration to match the OLED panel
 * hardware layout. See datasheet.
 * @param dev Pointer to device descriptor
 * @param config Sequential COM pin configuration
 * @return Non-zero if error occured
 */
int ssd1306_set_com_pin_hw_config(const ssd1306_t *dev, uint8_t config);

/**
 * Set the display contrast.
 * @param dev Pointer to device descriptor
 * @param contrast Contrast increases as the value increases.
 * @return Non-zero if error occured
 */
int ssd1306_set_contrast(const ssd1306_t *dev, uint8_t contrast);

/**
 * Set the display to be either normal or inverse. In normal display
 * a RAM data of 1 indicates an “ON” pixel while in inverse display a
 * RAM data of 0 indicates an “ON” pixel.
 * @param dev Pointer to device descriptor
 * @param on Inverse display if true
 * @return Non-zero if error occured
 */
int ssd1306_set_inversion(const ssd1306_t *dev, bool on);

/**
 * Set the divide ratio of display clock and oscillator frequency.
 * See datasheet.
 * @param dev Pointer to device descriptor
 * @param osc_freq Lower nibble - DCLK divide ratio, high
 *     nibble - oscillator frequency
 * @return Non-zero if error occured
 */
int ssd1306_set_osc_freq(const ssd1306_t *dev, uint8_t osc_freq);

/**
 * Switch the default 63 multiplex mode to any multiplex ratio,
 * ranging from 16 to 63. The output pads COM0~COM63 will be switched
 * to the corresponding COM signal.
 * @param dev Pointer to device descriptor
 * @param ratio Multiplex ratio, 16..63
 * @return Non-zero if error occured
 */
int ssd1306_set_mux_ratio(const ssd1306_t *dev, uint8_t ratio);

/**
 * Specify column start address and end address of the display data RAM.
 * This command also sets the column address pointer to column start
 * address. This pointer is used to define the current read/write column
 * address in graphic display data RAM. If horizontal address increment mode
 * is enabled by ssd1306_set_mem_addr_mode(), after finishing read/write
 * one column data, it is incremented automatically to the next column
 * address. Whenever the column address pointer finishes accessing the
 * end column address, it is reset back to start column address and the
 * row address is incremented to the next row.
 * @param dev Pointer to device descriptor
 * @param start Start RAM address
 * @param stop End RAM address
 * @return Non-zero if error occured
 */
int ssd1306_set_column_addr(const ssd1306_t *dev, uint8_t start, uint8_t stop);

/**
 * Specify page start address and end address of the display data RAM.
 * This command also sets the page address pointer to page start address.
 * This pointer is used to define the current read/write page address in
 * graphic display data RAM. If vertical address increment mode is enabled by
 * ssd1306_set_mem_addr_mode(), after finishing read/write one page data,
 * it is incremented automatically to the next page address. Whenever the page
 * address pointer finishes accessing the end page address, it is reset back
 * to start page address.
 * @param dev Pointer to device descriptor
 * @param start Start RAM address
 * @param stop End RAM address
 * @return Non-zero if error occured
 */
int ssd1306_set_page_addr(const ssd1306_t *dev, uint8_t start, uint8_t stop);

/**
 * Set the duration of the pre-charge period. The interval is counted in
 * number of DCLK, where RESET equals 2 DCLKs.
 * @param dev Pointer to device descriptor
 * @param prchrg Pre-charge period
 * @return Non-zero if error occured
 */
int ssd1306_set_precharge_period(const ssd1306_t *dev, uint8_t prchrg);

/**
 * Adjust the VCOMH regulator output. See datasheet.
 * @param dev Pointer to device descriptor
 * @param lvl Deselect level
 * @return Non-zero if error occured
 */
int ssd1306_set_deseltct_lvl(const ssd1306_t *dev, uint8_t lvl);

/**
 * Force the entire display to be “ON”, regardless of the contents of
 * the display data RAM.
 * @param dev Pointer to device descriptor
 * @param light Force the entire display to be “ON if true
 * @return Non-zero if error occured
 */
int ssd1306_set_whole_display_lighting(const ssd1306_t *dev, bool light);

/**
 * Draw one pixel
 * @param dev Pointer to device descriptor
 * @param fb Pointer to framebuffer. Framebuffer size = width * height / 8
 * @param x X coordinate
 * @param y Y coordinate
 * @param color Color of the pixel
 * @return Non-zero if error occured
 */
int ssd1306_draw_pixel(const ssd1306_t *dev, uint8_t *fb, int8_t x, int8_t y, ssd1306_color_t color);

/**
 * Draw a horizontal line
 * @param dev Pointer to device descriptor
 * @param fb Pointer to framebuffer. Framebuffer size = width * height / 8
 * @param x X coordinate or starting (left) point
 * @param y Y coordinate or starting (left) point
 * @param w Line width
 * @param color Color of the line
 * @return Non-zero if error occured
 */
int ssd1306_draw_hline(const ssd1306_t *dev, uint8_t *fb, int8_t x, int8_t y, uint8_t w, ssd1306_color_t color);

/**
 * Draw a vertical line
 * @param dev Pointer to device descriptor
 * @param fb Pointer to framebuffer. Framebuffer size = width * height / 8
 * @param x X coordinate or starting (top) point
 * @param y Y coordinate or starting (top) point
 * @param h Line height
 * @param color Color of the line
 * @return Non-zero if error occured
 */
int ssd1306_draw_vline(const ssd1306_t *dev, uint8_t *fb, int8_t x, int8_t y, uint8_t h, ssd1306_color_t color);

/**
 * Draw a line
 * @param dev Pointer to device descriptor
 * @param fb Pointer to framebuffer. Framebuffer size = width * height / 8
 * @param x0 First x point coordinate
 * @param y0 First y point coordinate
 * @param x1 Second x point coordinate
 * @param y1 Second y point coordinate
 * @param color Color of the line
 * @return Non-zero if error occured
 */
int ssd1306_draw_line(const ssd1306_t *dev, uint8_t *fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, ssd1306_color_t color);

/**
 * Draw a rectangle
 * @param dev Pointer to device descriptor
 * @param fb Pointer to framebuffer. Framebuffer size = width * height / 8
 * @param x X coordinate or starting (top left) point
 * @param y Y coordinate or starting (top left) point
 * @param w Rectangle width
 * @param h Rectangle height
 * @param color Color of the rectangle border
 * @return Non-zero if error occured
 */
int ssd1306_draw_rectangle(const ssd1306_t *dev, uint8_t *fb, int8_t x, int8_t y, uint8_t w, uint8_t h, ssd1306_color_t color);

/**
 * Draw a filled rectangle
 * @param dev Pointer to device descriptor
 * @param fb Pointer to framebuffer. Framebuffer size = width * height / 8
 * @param x X coordinate or starting (top left) point
 * @param y Y coordinate or starting (top left) point
 * @param w Rectangle width
 * @param h Rectangle height
 * @param color Color of the rectangle
 * @return Non-zero if error occured
 */
int ssd1306_fill_rectangle(const ssd1306_t *dev, uint8_t *fb, int8_t x, int8_t y, uint8_t w, uint8_t h, ssd1306_color_t color);

/**
 * Draw a circle
 * @param dev Pointer to device descriptor
 * @param fb Pointer to framebuffer. Framebuffer size = width * height / 8
 * @param x0 X coordinate or center
 * @param y0 Y coordinate or center
 * @param r Radius
 * @param color Color of the circle border
 * @return Non-zero if error occured
 */
int ssd1306_draw_circle(const ssd1306_t *dev, uint8_t *fb, int8_t x0, int8_t y0, uint8_t r, ssd1306_color_t color);

/**
 * Draw a filled circle
 * @param dev Pointer to device descriptor
 * @param fb Pointer to framebuffer. Framebuffer size = width * height / 8
 * @param x0 X coordinate or center
 * @param y0 Y coordinate or center
 * @param r Radius
 * @param color Color of the circle
 * @return Non-zero if error occured
 */
int ssd1306_fill_circle(const ssd1306_t *dev, uint8_t *fb, int8_t x0, int8_t y0, uint8_t r, ssd1306_color_t color);

/**
 * Draw a triangle
 * @param dev Pointer to device descriptor
 * @param fb Pointer to framebuffer. Framebuffer size = width * height / 8
 * @param x0 First x point coordinate
 * @param y0 First y point coordinate
 * @param x1 Second x point coordinate
 * @param y1 Second y point coordinate
 * @param x2 third x point coordinate
 * @param y2 third y point coordinate
 * @param color Color of the triangle border
 * @return Non-zero if error occured
 */
int ssd1306_draw_triangle(const ssd1306_t *dev, uint8_t *fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, ssd1306_color_t color);

/**
 * Draw a filled triangle
 * @param dev Pointer to device descriptor
 * @param fb Pointer to framebuffer. Framebuffer size = width * height / 8
 * @param x0 First x point coordinate
 * @param y0 First y point coordinate
 * @param x1 Second x point coordinate
 * @param y1 Second y point coordinate
 * @param x2 third x point coordinate
 * @param y2 third y point coordinate
 * @param color Color of the triangle
 * @return Non-zero if error occured
 */
int ssd1306_fill_triangle(const ssd1306_t *dev, uint8_t *fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, ssd1306_color_t color);

/**
 * Draw one character using currently selected font
 * @param dev Pointer to device descriptor
 * @param fb Pointer to framebuffer. Framebuffer size = width * height / 8
 * @param font Pointer to font info structure
 * @param x X position of character (top-left corner)
 * @param y Y position of character (top-left corner)
 * @param c The character to draw
 * @param foreground Character color
 * @param background Background color
 * @return Width of the character or negative value if error occured
 */
int ssd1306_draw_char(const ssd1306_t *dev, uint8_t *fb, const font_info_t *font, uint8_t x, uint8_t y, char c, ssd1306_color_t foreground, ssd1306_color_t background);

/**
 * Draw one character using currently selected font
 * @param dev Pointer to device descriptor
 * @param fb Pointer to framebuffer. Framebuffer size = width * height / 8
 * @param font Pointer to font info structure
 * @param x X position of character (top-left corner)
 * @param y Y position of character (top-left corner)
 * @param str The string to draw
 * @param foreground Character color
 * @param background Background color
 * @return Width of the string  or negative value if error occured
 */
int ssd1306_draw_string(const ssd1306_t *dev, uint8_t *fb, const font_info_t *font, uint8_t x, uint8_t y, const char *str, ssd1306_color_t foreground, ssd1306_color_t background);

/**
 * Stop scrolling (the ram data needs to be rewritten)
 * @param dev Pointer to device descriptor
 * @return Non-zero if error occured
 */
int ssd1306_stop_scroll(const ssd1306_t *dev);

/**
 * Start horizontal scrolling
 * @param dev Pointer to device descriptor
 * @param way Orientation ( true: left // false: right )
 * @param start Page address start
 * @param stop Page address stop
 * @param frame Time interval between each scroll
 * @return Non-zero if error occured
 */
int ssd1306_start_scroll_hori(const ssd1306_t *dev, bool way, uint8_t start, uint8_t stop, ssd1306_scroll_t frame);

/**
 * Start horizontal+vertical scrolling (cant vertical scrolling)
 * @param dev Pointer to device descriptor
 * @param way Orientation ( true: left // false: right )
 * @param start Page address start
 * @param stop Page address stop
 * @param dy vertical size shifting (min : 1 // max: 63 )
 * @param frame Time interval between each scroll
 * @return Non-zero if error occured
 */
int ssd1306_start_scroll_hori_vert(const ssd1306_t *dev, bool way, uint8_t start, uint8_t stop, uint8_t dy, ssd1306_scroll_t frame);

#ifdef __cplusplus
}
#endif

#endif // _SSD1306__H_
