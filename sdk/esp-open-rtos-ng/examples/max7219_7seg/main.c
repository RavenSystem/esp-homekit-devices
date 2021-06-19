/*
 * Example of using MAX7219 driver with 7 segment displays
 *
 * Part of esp-open-rtos
 * Copyright (C) 2017 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include <esp/uart.h>
#include <espressif/esp_common.h>
#include <stdio.h>
#include <max7219/max7219.h>
#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include <esp/hwrand.h>

#define CS_PIN 5
#define DELAY 2000

static max7219_display_t disp = {
    .cs_pin       = CS_PIN,
    .digits       = 8,
    .cascade_size = 1,
    .mirrored     = true
};

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    max7219_init(&disp);
    //max7219_set_decode_mode(&disp, true);

    char buf[9];
    while (true)
    {
        max7219_clear(&disp);
        max7219_draw_text(&disp, 0, "7219LEDS");
        vTaskDelay(DELAY / portTICK_PERIOD_MS);

        max7219_clear(&disp);
        sprintf(buf, "%2.4f A", 34.6782);
        max7219_draw_text(&disp, 0, buf);
        vTaskDelay(DELAY / portTICK_PERIOD_MS);

        max7219_clear(&disp);
        sprintf(buf, "%08x", hwrand());
        max7219_draw_text(&disp, 0, buf);
        vTaskDelay(DELAY / portTICK_PERIOD_MS);
    }
}
