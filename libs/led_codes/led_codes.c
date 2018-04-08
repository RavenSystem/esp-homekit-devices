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
#include <etstimer.h>
#include <esplibs/libmain.h>

#include "led_codes.h"

#define DURATION_OFF        150

typedef struct led_params_t {
    blinking_params_t blinking_params;
    uint8_t gpio;
    bool status;
    ETSTimer timer;
    uint8_t count;
    uint32_t delay;
} led_params_t;

led_params_t led_params;

void led_code_run() {
    led_params.status = !led_params.status;
    
    gpio_write(led_params.gpio, led_params.status);
    
    if (led_params.status == 0) {
        led_params.delay = (led_params.blinking_params.duration * 1000) + 90;
    } else {
        led_params.delay = DURATION_OFF;
        led_params.count++;
    }
    
    sdk_os_timer_disarm(&led_params.timer);
    
    if (led_params.count < led_params.blinking_params.times) {
        sdk_os_timer_arm(&led_params.timer, led_params.delay, 0);
    }
}

void led_code(uint8_t gpio, blinking_params_t blinking_params) {
    led_params.gpio = gpio;
    led_params.blinking_params = blinking_params;
    
    sdk_os_timer_disarm(&led_params.timer);
    sdk_os_timer_setfn(&led_params.timer, led_code_run, NULL);
    
    led_params.status = 1;
    led_params.count = 0;
    
    led_code_run();
}
