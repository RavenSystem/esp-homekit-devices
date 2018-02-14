/*
 * LED Codes
 *
 */

#include <stdio.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include "led_codes.h"

#define DURATION_OFF        200     / portTICK_PERIOD_MS

void led_code(uint8_t gpio, blinking_params params) {
    uint32_t duration = ((params.duration * 1000) + 100) / portTICK_PERIOD_MS;
    
    uint8_t i;
    for (i = 0; i < params.times; i++) {
        if (i != 0) {
            vTaskDelay(DURATION_OFF);
        }
        gpio_write(gpio, 0);
        vTaskDelay(duration);
        gpio_write(gpio, 1);
    }
}
