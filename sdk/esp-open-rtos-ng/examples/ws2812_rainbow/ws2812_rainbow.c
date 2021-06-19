/**
 * @file   es2812_rainbow.c
 * @author Ondřej Hruška, 2016
 *
 * @brief  Example of a rainbow effect with
 *         WS2812 connected to GPIO2.
 *
 * This demo is in the public domain.
 */

#include "espressif/esp_common.h"
#include "FreeRTOS.h"
#include "task.h"
#include "esp/uart.h" // uart_set_baud
#include <stdio.h> // printf
#include <stdint.h>

#include "ws2812.h"


#define delay_ms(ms) vTaskDelay((ms) / portTICK_PERIOD_MS)


/** GPIO number used to control the RGBs */
static const uint8_t pin = 2;


/**
 * @brief "rainbow" animation with a single RGB led.
 */
void demo_single(void)
{
    // duration between color changes
    const uint8_t delay = 25;

    ws2812_rgb_t x = {.num = 0xFF0000}; // RED color

    while (1) {
        // iterate through the spectrum

        // note: This would be _WAY_ easier with HSL

        while(x.g < 0xFF) { x.g++; ws2812_set(pin, x.num); delay_ms(delay); } // R->RG
        while(x.r > 0x00) { x.r--; ws2812_set(pin, x.num); delay_ms(delay); } // RG->G
        while(x.b < 0xFF) { x.b++; ws2812_set(pin, x.num); delay_ms(delay); } // G->GB
        while(x.g > 0x00) { x.g--; ws2812_set(pin, x.num); delay_ms(delay); } // GB->B
        while(x.r < 0xFF) { x.r++; ws2812_set(pin, x.num); delay_ms(delay); } // B->BR
        while(x.b > 0x00) { x.b--; ws2812_set(pin, x.num); delay_ms(delay); } // BR->R
    }
}


/**
 * @brief "rainbow" effect on a RGB strip (30 pixels - can be adjusted)
 *
 * This example shows how to use the "procedural generation" of colors.
 *
 * The pixel colors are calculated on the fly, which saves RAM
 * (especially with large displays).
 */
void demo_strip(void *pvParameters)
{
    const uint8_t anim_step = 10;
    const uint8_t anim_max = 250;

    // Number of your "pixels"
    const uint8_t pixel_count = 30;

    // duration between color changes
    const uint8_t delay = 25;

    ws2812_rgb_t color = WS2812_RGB(anim_max, 0, 0);
    uint8_t step = 0;

    ws2812_rgb_t color2 = WS2812_RGB(anim_max, 0, 0);
    uint8_t step2 = 0;

    while (1) {

        color = color2;
        step = step2;

        // Start a data sequence (disables interrupts)
        ws2812_seq_start();

        for (uint8_t i = 0; i < pixel_count; i++) {

            // send a color
            ws2812_seq_rgb(pin, color.num);

            // now we have a few hundred nanoseconds
            // to calculate the next color

            if (i == 1) {
                color2 = color;
                step2 = step;
            }

            switch (step) {
                case 0: color.g += anim_step; if (color.g >= anim_max) step++;  break;
                case 1: color.r -= anim_step; if (color.r == 0) step++; break;
                case 2: color.b += anim_step; if (color.b >= anim_max) step++; break;
                case 3: color.g -= anim_step; if (color.g == 0) step++; break;
                case 4: color.r += anim_step; if (color.r >= anim_max) step++; break;
                case 5: color.b -= anim_step; if (color.b == 0) step = 0; break;
            }
        }

        // End the data sequence, display colors (interrupts are restored)
        ws2812_seq_end();

        // wait a bit
        delay_ms(delay);
    }
}


void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("--- RGB Rainbow demo ---");

    // Configure the GPIO
    gpio_enable(pin, GPIO_OUTPUT);

    // Select a demo function:

#if true
# define demo demo_strip
#else
# define demo demo_single
#endif


    // Choose how to run it:

#if true

    // Blocking function - works OK, because WiFi isn't
    // initialized yet & we're hogging the CPU.

    printf("Starting a blocking function.\r\n");
    demo(NULL);

#else

    // Start a task. This is a real-life example,
    // notice the glitches due to NMI.

    printf("Starting a task. There may be glitches!\r\n");
    xTaskCreate(&demo, "strip demo", 256, NULL, 10, NULL);
#endif
}
