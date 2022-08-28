/**
 * @file   ws2812b.c
 * @brief  ESP8266 driver for WS2812B
 * @author Ondřej Hruška, (c) 2016
 *
 * MIT License
 */

#include "espressif/esp_common.h" // sdk_os_delay_us
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

#include "ws2812.h"


/** Set one RGB LED color */
void ws2812_set(uint8_t gpio_num, uint32_t rgb)
{
    ws2812_seq_start();
    ws2812_seq_rgb(gpio_num, rgb);
    ws2812_seq_end();
}


/** Set many RGBs */
void ws2812_set_many(uint8_t gpio_num, uint32_t *rgbs, size_t count)
{
    ws2812_seq_start();

    for (size_t i = 0; i < count; i++) {
        uint32_t rgb = *rgbs++;
        ws2812_seq_rgb(gpio_num, rgb);
    }

    ws2812_seq_end();
}


/** Set one RGB to black (when used as indicator) */
void ws2812_off(uint8_t gpio_num)
{
    ws2812_set(gpio_num, 0x000000);
}
