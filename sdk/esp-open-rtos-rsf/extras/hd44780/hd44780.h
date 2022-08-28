/*
 * Driver for LCD text displays on LCD connected to I2C by PCF8574
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#ifndef _EXTRAS_HD44780_H_
#define _EXTRAS_HD44780_H_

#include <stdint.h>
#include <stdbool.h>

#ifndef HD44780_I2C
#define HD44780_I2C 1
#endif
#if (HD44780_I2C)
#include <i2c/i2c.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define HD44780_NOT_USED 0xff

/**
 * LCD font type. Please refer to the datasheet
 * of your module.
 */
typedef enum
{
    HD44780_FONT_5X8 = 0,
    HD44780_FONT_5X10
} hd44780_font_t;

/**
 * LCD descriptor. Fill it before use.
 */
typedef struct
{
#if (HD44780_I2C)
      i2c_dev_t i2c_dev;          //!< PCF8574 device settings (0b0100<A2><A1><A0>)
#endif
    struct
    {
        uint8_t rs;        //!< gpio/register bit used for RS pin
        uint8_t e;         //!< register bit used for E pin
        uint8_t d4;        //!< register bit used for D4 pin
        uint8_t d5;        //!< register bit used for D5 pin
        uint8_t d6;        //!< register bit used for D5 pin
        uint8_t d7;        //!< register bit used for D5 pin
        uint8_t bl;        //!< register bit used for backlight, 0xFF if not used
    } pins;
    hd44780_font_t font;   //!< LCD Font type
    uint8_t lines;         //!< Number of lines for LCD. Many 16x1 LCD has two lines (like 8x2)
    bool backlight;        //!< Current backlight state
} hd44780_t;

/**
 * Init LCD. Set poition to (0, 0)
 * \param lcd Pointer to the LCD descriptor
 */
void hd44780_init(const hd44780_t *lcd);

/**
 * On/off LCD, show/hide cursor, set cursor blink
 * \param lcd Pointer to the LCD descriptor
 */
void hd44780_control(const hd44780_t *lcd, bool on, bool cursor, bool cursor_blink);

/**
 * Clear LCD memory and move char position to (0, 0)
 * \param lcd Pointer to the LCD descriptor
 */
void hd44780_clear(const hd44780_t *lcd);

/**
 * Set current char position
 * \param lcd Pointer to the LCD descriptor
 * \param col Column
 * \param line Line
 */
void hd44780_gotoxy(const hd44780_t *lcd, uint8_t col, uint8_t line);

/**
 * Print character
 * \param lcd Pointer to the LCD descriptor
 * \param c Character
 */
void hd44780_putc(const hd44780_t *lcd, char c);

/**
 * Print string
 * \param lcd Pointer to the LCD descriptor
 * \param s String
 */
void hd44780_puts(const hd44780_t *lcd, const char *s);

/**
 * Switch backlight
 * \param lcd Pointer to the LCD descriptor
 * \param on Turn backlight on if true
 */
void hd44780_set_backlight(hd44780_t *lcd, bool on);

/**
 * Upload character data to the CGRAM.
 * Current position will be set to (0, 0) after uploading
 * \param lcd Pointer to the LCD descriptor
 * \param num Character number (0..7)
 * \param data Character data: 8 or 10 bytes depending on the font
 */
void hd44780_upload_character(const hd44780_t *lcd, uint8_t num, const uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif /* _EXTRAS_HD44780_H_ */
