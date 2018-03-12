/*
 * LED Codes
 * 
 * Copyright 2018 José A. Jiménez (@RavenSystem)
 *  
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include "led_codes.h"

#define DURATION_OFF        150     / portTICK_PERIOD_MS

void led_code(uint8_t gpio, blinking_params params) {
    uint32_t duration = ((params.duration * 1000) + 90) / portTICK_PERIOD_MS;
    
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
