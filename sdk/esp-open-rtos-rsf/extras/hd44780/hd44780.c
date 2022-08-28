#include "hd44780.h"

#if (HD44780_I2C)
#include <pcf8574/pcf8574.h>
#else
#include <esp/gpio.h>
#endif
#include <espressif/esp_common.h>

#define MS 1000

#define DELAY_CMD_LONG  (3 * MS) // >1.53ms according to datasheet
#define DELAY_CMD_SHORT (60)     // >39us according to datasheet
#define DELAY_TOGGLE    (10)
#define DELAY_INIT      (5 * MS)

#define CMD_CLEAR        0x01
#define CMD_RETURN_HOME  0x02
#define CMD_ENTRY_MODE   0x04
#define CMD_DISPLAY_CTRL 0x08
#define CMD_SHIFT        0x10
#define CMD_FUNC_SET     0x20
#define CMD_CGRAM_ADDR   0x40
#define CMD_DDRAM_ADDR   0x80

// CMD_ENTRY_MODE
#define ARG_EM_INCREMENT    (1 << 1)
#define ARG_EM_SHIFT        (1)

// CMD_DISPLAY_CTRL
#define ARG_DC_DISPLAY_ON   (1 << 2)
#define ARG_DC_CURSOR_ON    (1 << 1)
#define ARG_DC_CURSOR_BLINK (1)

// CMD_FUNC_SET
#define ARG_FS_8_BIT        (1 << 4)
#define ARG_FS_2_LINES      (1 << 3)
#define ARG_FS_FONT_5X10    (1 << 2)

#if (HD44780_I2C)
    #define init_delay()   do { sdk_os_delay_us(DELAY_INIT); } while (0)
    #define short_delay()
    #define long_delay()   do { sdk_os_delay_us(DELAY_CMD_LONG); } while (0)
#else
    #define init_delay()   do { sdk_os_delay_us(DELAY_INIT); } while (0)
    #define short_delay()  do { sdk_os_delay_us(DELAY_CMD_SHORT); } while (0)
    #define long_delay()   do { sdk_os_delay_us(DELAY_CMD_LONG); } while (0)
    #define toggle_delay() do { sdk_os_delay_us(DELAY_TOGGLE); } while (0)
#endif

static const uint8_t line_addr[] = { 0x00, 0x40, 0x14, 0x54 };

static void write_nibble(const hd44780_t *lcd, uint8_t b, bool rs)
{
#if (HD44780_I2C)
    uint8_t data = (((b >> 3) & 1) << lcd->pins.d7)
                 | (((b >> 2) & 1) << lcd->pins.d6)
                 | (((b >> 1) & 1) << lcd->pins.d5)
                 | ((b & 1) << lcd->pins.d4)
                 | (rs ? 1 << lcd->pins.rs : 0)
                 | (lcd->backlight ? 1 << lcd->pins.bl : 0);

    pcf8574_port_write(&lcd->i2c_dev, data | (1 << lcd->pins.e));
    pcf8574_port_write(&lcd->i2c_dev, data);
#else
    gpio_write(lcd->pins.d7, (b >> 3) & 1);
    gpio_write(lcd->pins.d6, (b >> 2) & 1);
    gpio_write(lcd->pins.d5, (b >> 1) & 1);
    gpio_write(lcd->pins.d4, b & 1);
    gpio_write(lcd->pins.rs, rs);
    gpio_write(lcd->pins.e, true);
    toggle_delay();
    gpio_write(lcd->pins.e, false);
#endif
}

static void write_byte(const hd44780_t *lcd, uint8_t b, bool rs)
{
    write_nibble(lcd, b >> 4, rs);
    write_nibble(lcd, b, rs);
}

void hd44780_init(const hd44780_t *lcd)
{
#if (!HD44780_I2C)
    gpio_enable(lcd->pins.rs, GPIO_OUTPUT);
    gpio_enable(lcd->pins.e, GPIO_OUTPUT);
    gpio_enable(lcd->pins.d4, GPIO_OUTPUT);
    gpio_enable(lcd->pins.d5, GPIO_OUTPUT);
    gpio_enable(lcd->pins.d6, GPIO_OUTPUT);
    gpio_enable(lcd->pins.d7, GPIO_OUTPUT);
    if (lcd->pins.bl != HD44780_NOT_USED)
        gpio_enable(lcd->pins.bl, GPIO_OUTPUT);
#endif
    // switch to 4 bit mode
    for (uint8_t i = 0; i < 3; i ++)
    {
        write_nibble(lcd, (CMD_FUNC_SET | ARG_FS_8_BIT) >> 4, false);
        init_delay();
    }
    write_nibble(lcd, CMD_FUNC_SET >> 4, false);

    // Specify the number of display lines and character font
    write_byte(lcd,
        CMD_FUNC_SET
            | (lcd->lines > 1 ? ARG_FS_2_LINES : 0)
            | (lcd->font == HD44780_FONT_5X10 ? ARG_FS_FONT_5X10 : 0),
        false);
    short_delay();
    // Display off
    hd44780_control(lcd, false, false, false);
    // Clear
    hd44780_clear(lcd);
    // Entry mode set
    write_byte(lcd, CMD_ENTRY_MODE | ARG_EM_INCREMENT, false);
    short_delay();
    // Display on
    hd44780_control(lcd, true, false, false);
}

void hd44780_control(const hd44780_t *lcd, bool on, bool cursor, bool cursor_blink)
{
    write_byte(lcd,
        CMD_DISPLAY_CTRL
            | (on ? ARG_DC_DISPLAY_ON : 0)
            | (cursor ? ARG_DC_CURSOR_ON : 0)
            | (cursor_blink ? ARG_DC_CURSOR_BLINK : 0),
        false);
    short_delay();
}

void hd44780_clear(const hd44780_t *lcd)
{
    write_byte(lcd, CMD_CLEAR, false);
    long_delay();
}

void hd44780_gotoxy(const hd44780_t *lcd, uint8_t col, uint8_t line)
{
    if (line >= lcd->lines) line = lcd->lines - 1;
    uint8_t addr = line < sizeof(line_addr) ? line_addr[line] : 0;
    write_byte(lcd, CMD_DDRAM_ADDR + addr + col, false);
    short_delay();
}

void hd44780_putc(const hd44780_t *lcd, char c)
{
    write_byte(lcd, c, true);
    short_delay();
}

void hd44780_puts(const hd44780_t *lcd, const char *s)
{
    while (*s)
    {
        hd44780_putc(lcd, *s);
        s++;
    }
}

void hd44780_set_backlight(hd44780_t *lcd, bool on)
{
    if (lcd->pins.bl == HD44780_NOT_USED)
        return;

#if (HD44780_I2C)
    pcf8574_gpio_write(&lcd->i2c_dev, lcd->pins.bl, on);
#else
     gpio_write(lcd->pins.bl, on);
#endif

     lcd->backlight = on;
}

void hd44780_upload_character(const hd44780_t *lcd, uint8_t num, const uint8_t *data)
{
    if (num > 7) return;

    uint8_t bytes = lcd->font == HD44780_FONT_5X8 ? 8 : 10;
    write_byte(lcd, CMD_CGRAM_ADDR + num * bytes, false);
    short_delay();
    for (uint8_t i = 0; i < bytes; i ++)
    {
        write_byte(lcd, data[i], true);
        short_delay();
    }

    hd44780_gotoxy(lcd, 0, 0);
}
