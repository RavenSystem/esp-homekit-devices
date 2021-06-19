/**
 * Example of ws2812_i2s library usage.
 *
 * This example shows light that travels in circle with fading tail.
 * As ws2812_i2s library using hardware I2S the output pin is GPIO3 and
 * can not be changed.
 *
 * This sample code is in the public domain.,
 */
#include "espressif/esp_common.h"
#include "FreeRTOS.h"
#include "task.h"
#include "esp/uart.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "ws2812_i2s/ws2812_i2s.h"

const uint32_t led_number = 12;
const uint32_t tail_fade_factor = 2;
const uint32_t tail_length = 8;

static void fade_pixel(ws2812_pixel_t *pixel, uint32_t factor)
{
    pixel->red = pixel->red / factor;
    pixel->green = pixel->green / factor;
    pixel->blue = pixel->blue / factor;
}

static int fix_index(int index)
{
    if (index < 0) {
        return (int)led_number + index;
    } else if (index >= led_number) {
        return index - led_number;
    } else {
        return index;
    }
}

static ws2812_pixel_t next_colour()
{
    ws2812_pixel_t colour = { {0, 0, 0, 0} };
    colour.red = rand() % 256;
    colour.green = rand() % 256;
    colour.blue = rand() % 256;

    return colour;
}

static void demo(void *pvParameters)
{
    ws2812_pixel_t pixels[led_number];
    int head_index = 0;

    ws2812_i2s_init(led_number, PIXEL_RGB);

    memset(pixels, 0, sizeof(ws2812_pixel_t) * led_number);    

    while (1) {
        pixels[head_index] = next_colour();
        for (int i = 0; i < led_number; i++) {
            head_index = fix_index(head_index + 1);
            pixels[head_index] = pixels[fix_index(head_index-1)];
            for (int ii = 1; ii < tail_length; ii++) {
                fade_pixel(&pixels[fix_index(head_index - ii)], tail_fade_factor);
            }
            memset(&pixels[fix_index(head_index - tail_length)], 0, 
                    sizeof(ws2812_pixel_t));

            ws2812_i2s_update(pixels, PIXEL_RGB);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    struct sdk_station_config config = {
        .ssid = "Loading...",
        .password = "morays59924_howitzer",
    };

    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

    xTaskCreate(&demo, "ws2812_i2s", 256, NULL, 10, NULL);
}
