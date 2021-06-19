/**
 * @file   ws2812.h
 * @brief  ESP8266 driver for WS2812
 *
 * This is a simple bit-banging driver for WS2812.
 *
 * It will work for WS2812, WS2812B and possibly others,
 * with small alterations.
 *
 * The WS2812 protocol takes roughly 1µs per bit,
 * thus ~24µs per "pixel", plus 50µs to indicate
 * the end of data.
 *
 * @note
 * The GPIO must be configured for output before trying
 * to set colors!
 *
 * @attention
 * Due to the precise timing required, interrupts are
 * disabled until the data is sent. However, we currently
 * can't disable the NMI, so under some conditions
 * (heavy network load), the timing may be disturbed.
 *
 * @author Ondřej Hruška, (c) 2016
 *
 * MIT License
 */

#ifndef WS2812_DRV_H
#define WS2812_DRV_H

#include <stdint.h>
#include <stddef.h> // size_t

#include "espressif/esp_common.h" // sdk_os_delay_us
#include "esp/gpio.h"

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * @brief Struct for easy manipulation of RGB colors.
 *
 * Set components in the xrgb.r (etc.) and you will get
 * the hex in xrgb.num.
 */
typedef union {

    /** Struct for access to individual color components */
    struct __attribute__((packed)) {
        uint8_t b;
        uint8_t g;
        uint8_t r;
    };

    /** RGB color as a single uint32_t */
    uint32_t num;

} ws2812_rgb_t;


/**
 * @brief Macro to compose the RGB struct.
 *
 * You can also use {.num = 0xFF0000} to set the hex directly.
 */
#define WS2812_RGB(r, g, b) {.num = (((r) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((b) & 0xFF)}


/**
 * @brief Set a single RGB LED color, and display it.
 *
 * @param gpio_num : data line GPIO number
 * @param rgb : RGB color - 0x00RRGGBB
 */
void ws2812_set(uint8_t gpio_num, uint32_t rgb);


/**
 * @brief Set color of multiple RGB LEDs, and display it.
 *
 * @note
 * Due to the precise timing required, interrupts are
 * disabled until all data is sent (roughly 25*count + 50µs)
 *
 * @param gpio_num : data line GPIO number
 * @param rgbs  : array of RGB colors in the 0x00RRGGBB format
 * @param count : number of elements in the array
 */
void ws2812_set_many(uint8_t gpio_num, uint32_t *rgbs, size_t count);


/**
 * @brief Turn a single WS2812B off
 *
 * This is a companion function to ws2812_set().
 *
 * It sets the color to BLACK, thus effectively
 * turning the RGB LED off.
 *
 * @param gpio_num : data line GPIO number
 */
void ws2812_off(uint8_t gpio_num);



///////////////////////////////////////////////////////////////////
///
/// The following part of the driver is used internally, and can
/// also be used for "procedural generation" of colors.
///
/// - first call ws2812b_seq_start(),
/// - then repeatedly ws2812b_seq_rgb() with your colors
/// - and end the sequence with ws2812b_seq_end()
///
///////////////////////////////////////////////////////////////////



// Ugly way to get short delays. Works for 80 MHz.

// 400 ns
#ifndef WS2812_SHORT_DELAY
#define WS2812_SHORT_DELAY() for (volatile uint32_t __j = 1; __j > 0; __j--)
#endif

// 800 ns
#ifndef WS2812_LONG_DELAY
#define WS2812_LONG_DELAY()  for (volatile uint32_t __j = 3; __j > 0; __j--)
#endif


/**
 * @brief Send a byte on the data line.
 *
 * The WS2812B is a form of PWM:
 * - each bit takes roughly 1 µs (but can be longer)
 * - the duration of the ON part determines the value
 * - 800 ns -> "1"
 * - 400 ns -> "0"
 *
 * The timing must be very precise, you may need to adjust
 * it according for particular RGB model.
 *
 * @param gpio_num : data line GPIO number
 * @param byte : byte to send
 */
static inline
void ws2812_byte(uint8_t gpio_num, uint8_t byte)
{
    for (register volatile uint8_t i = 0; i < 8; i++) {
        gpio_write(gpio_num, 1);

        // duty cycle determines the bit value

        if (byte & 0x80) {
            WS2812_LONG_DELAY();
            gpio_write(gpio_num, 0);
            WS2812_SHORT_DELAY();
        } else {
            WS2812_SHORT_DELAY();
            gpio_write(gpio_num, 0);
            WS2812_LONG_DELAY();
        }

        byte <<= 1; // shift to next bit
    }
}


/**
 * @brief Send a color to the RGB strip.
 *
 * This function must be called inside the data sequence.
 *
 * @param gpio_num : data line GPIO number
 * @param rgb : 0xRRGGBB color
 */
static inline
void ws2812_seq_rgb(uint8_t gpio_num, uint32_t rgb)
{
    ws2812_byte(gpio_num, (rgb & 0x00FF00) >> 8);
    ws2812_byte(gpio_num, (rgb & 0xFF0000) >> 16);
    ws2812_byte(gpio_num, (rgb & 0x0000FF) >> 0);
}


/**
 * @brief Start a data sequence.
 */
static inline
void ws2812_seq_start(void)
{
    // interruption when sending data would break the timing
    taskENTER_CRITICAL();
}


/**
 * @brief End the data sequence and display the colors.
 */
static inline
void ws2812_seq_end(void)
{
    taskEXIT_CRITICAL();

    sdk_os_delay_us(50); // display the loaded colors
}

#ifdef	__cplusplus
}
#endif

#endif /* WS2812_DRV_H */
