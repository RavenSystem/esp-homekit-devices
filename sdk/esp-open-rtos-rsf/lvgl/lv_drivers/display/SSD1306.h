/**
 * @file SSD1306.h
 * @date   2016-2018
 * @author zaltora  (https://github.com/Zaltora)
 * @author urx (https://github.com/urx)
 * @author Ruslan V. Uss (https://github.com/UncleRus)
 * @copyright MIT License.
 */

#ifndef SSD1306_H
#define SSD1306_H
#ifdef __cplusplus
extern "C" {
#endif
/*********************
 *      INCLUDES
 *********************/
#include "../../lv_drv_conf.h"
#if USE_SSD1306 != 0

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include "../lv_drv_common.h"
#include "lvgl/lv_misc/lv_color.h"

/*********************
 *      DEFINES
 *********************/
#define SSD1306_I2C_ADDR_0    (0x3C)
#define SSD1306_I2C_ADDR_1    (0x3D)
/**********************
 *      TYPEDEFS
 **********************/
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
        lv_i2c_handle_t i2c_dev;  //!< I2C device descriptor, used by SSD1306_PROTO_I2C
#endif
#if (SSD1306_SPI_4_WIRE_SUPPORT) || (SSD1306_SPI_3_WIRE_SUPPORT)
        lv_spi_handle_t spi_dev;  //!< SPI device descriptor
#endif
    };
    lv_gpio_handle_t rst_pin;     //!< Reset pin
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

/**********************
 *      MACROS
 **********************/


/**********************
 * GLOBAL PROTOTYPES
 **********************/

/* Custom VDB write to use 1 bit by pixel
 * @param buf Buffer pointer
 * @param buf_w Buffer size
 * @param x X coordinate
 * @param y Y coordinate
 * @param color Pixel color
 * @param opa Pixel opacity
 */
void ssd1306_vdb_wr(uint8_t * buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y, lv_color_t color, lv_opa_t opa);

/* Flush the content of the internal buffer the specific area on the display
 * This function is required only when LV_VDB_SIZE != 0 in lv_conf.h
 * @param x1 First x point coordinate
 * @param y1 First y point coordinate
 * @param x2 Second x point coordinate
 * @param y2 Second y point coordinate
 * @param color_p Pointer to VDB buffer (color sized)
 */
void ssd1306_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const lv_color_t * color_p);

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
 * Default deinit for SSD1306
 * @param dev Pointer to device descriptor
 * @return Non-zero if error occured
 */
int ssd1306_deinit(const ssd1306_t *dev);

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
#endif /* USE_SSD1306 */
#endif /* SSD1306_H */
