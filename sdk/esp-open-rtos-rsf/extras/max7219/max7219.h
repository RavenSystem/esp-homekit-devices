/*
 * Driver for MAX7219/MAX7221
 * Serially Interfaced, 8-Digit LED Display Drivers
 *
 * Part of esp-open-rtos
 * Copyright (C) 2017 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#ifndef EXTRAS_MAX7219_H_
#define EXTRAS_MAX7219_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX7219_MAX_CASCADE_SIZE 8
#define MAX7219_MAX_BRIGHTNESS   31

/**
 * Display descriptor
 */
typedef struct
{
    uint8_t cs_pin;              //!< GPIO connected to CS/LOAD pin

    uint8_t digits;              //!< Accessible digits in 7seg. Up to cascade_size * 8

    uint8_t cascade_size;        //!< Up to 8 MAX721xx cascaded
    bool mirrored;               //!< true for horizontally mirrored displays

    bool bcd;
} max7219_display_t;

/**
 * Initialize display: switch it to normal operation from shutdown mode,
 * set scan limit to the max and clear
 * @param disp Pointer to display descriptor
 * @return false if error occured
 */
bool max7219_init(max7219_display_t *disp);

/**
 * Set decode mode and clear display
 * @param disp Pointer to display descriptor
 * @param bcd true to set BCD decode mode, false to normal
 */
void max7219_set_decode_mode(max7219_display_t *disp, bool bcd);

/**
 * Set display brightness
 * @param disp Pointer to display descriptor
 * @param value Brightness value, 0..MAX7219_MAX_BRIGHTNESS
 */
void max7219_set_brightness(const max7219_display_t *disp, uint8_t value);

/**
 * Shutdown display or set it to normal mode
 * @param disp Pointer to display descriptor
 * @param shutdown Shutdown display if true
 */
void max7219_set_shutdown_mode(const max7219_display_t *disp, bool shutdown);

/**
 * Write data to display digit
 * @param disp Pointer to display descriptor
 * @param digit Digit index, 0..disp->digits - 1
 * @param val Data
 * @return false if error occured
 */
bool max7219_set_digit(const max7219_display_t *disp, uint8_t digit, uint8_t val);

/**
 * Clear display
 * @param disp Pointer to display descriptor
 */
void max7219_clear(const max7219_display_t *disp);

/**
 * Draw text.
 * Currently only 7-segment displays supported
 * @param disp Pointer to display descriptor
 * @param pos Start digit
 * @param s Text
 */
void max7219_draw_text(const max7219_display_t *disp, uint8_t pos, const char *s);

#ifdef __cplusplus
}
#endif

#endif /* EXTRAS_MAX7219_H_ */
