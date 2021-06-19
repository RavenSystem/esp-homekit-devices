/* An example for the multipwm library
 * Connect LEDs to pins 12, 13, 15 to see the light changes
 *
 * Part of esp-open-rtos
 * Copyright (c) Sashka Nochkin (https://github.com/nochkin)
 * MIT Licensed
 */

#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"

#include "multipwm.h"

void multipwm_task(void *pvParameters)
{
    uint32_t counts[] = {500, 2100, 3000};
    int32_t   steps[] = {100,  300,  200};
    uint8_t    pins[] = { 12,   13,   15};

    pwm_info_t pwm_info;
    pwm_info.channels = 3;

    multipwm_init(&pwm_info);
    for (uint8_t ii=0; ii<pwm_info.channels; ii++) {
        multipwm_set_pin(&pwm_info, ii, pins[ii]);
    }

    while(1) {
        multipwm_stop(&pwm_info);
        for (uint8_t ii=0; ii<pwm_info.channels; ii++) {
            multipwm_set_duty(&pwm_info, ii, counts[ii]);
            counts[ii] += steps[ii];
            if ((counts[ii] >= 10000) || (counts[ii] == 0)) {
                steps[ii] = -steps[ii];
            }
        }
        multipwm_start(&pwm_info);
        vTaskDelay(1);
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    xTaskCreate(multipwm_task, "multipwm", 256, NULL, 2, NULL);
}
