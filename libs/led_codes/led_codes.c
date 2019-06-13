/*
 * LED Codes Library
 * 
 * Copyright 2018-2019 José A. Jiménez (@RavenSystem)
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

#include <string.h>
#include <stdio.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <etstimer.h>
#include <esplibs/libmain.h>

#include "led_codes.h"

#define DURATION_OFF        130
#define DURATION_ON_MIN     20

typedef struct _led {
    uint8_t gpio;
    bool inverted;
    bool status;
    
    blinking_params_t blinking_params;
    
    ETSTimer timer;
    uint8_t count;
    uint32_t delay;
    
    struct _led *next;
} led_t;

static led_t *leds = NULL;

static led_t *led_find_by_gpio(const uint8_t gpio) {
    led_t *led = leds;
    
    while (led && led->gpio != gpio) {
        led = led->next;
    }
    
    return led;
}

static void led_code_run(void *params) {
    led_t *led = params;
    
    led->status = !led->status;
    gpio_write(led->gpio, led->status);
    
    if (led->status == led->inverted) {
        led->delay = DURATION_OFF;
        led->count++;
    } else {
        led->delay = (led->blinking_params.duration * 1000) + DURATION_ON_MIN;
    }
    
    if (led->count < led->blinking_params.times) {
        sdk_os_timer_arm(&led->timer, led->delay, 0);
    }
}

void led_code(const uint8_t gpio, blinking_params_t blinking_params) {
    led_t *led = led_find_by_gpio(gpio);
    
    if (led) {
        sdk_os_timer_disarm(&led->timer);
        
        led->blinking_params = blinking_params;
        led->status = led->inverted;
        led->count = 0;
        
        led_code_run(led);
    }
}

int led_create(const uint8_t gpio, const bool inverted) {
    led_t *led = led_find_by_gpio(gpio);
    
    if (!led) {
        led = malloc(sizeof(led_t));
        memset(led, 0, sizeof(*led));

        led->next = leds;
        leds = led;
        
        sdk_os_timer_setfn(&led->timer, led_code_run, led);
        
        led->gpio = gpio;
        led->inverted = inverted;
        led->status = inverted;
        
        gpio_enable(led->gpio, GPIO_OUTPUT);
        gpio_write(led->gpio, led->status);
        
        return 0;
    }

    return -1;
}

void led_destroy(const uint8_t gpio) {
    if (leds) {
        led_t *led = NULL;
        if (leds->gpio == gpio) {
            led = leds;
            leds = leds->next;
        } else {
            led_t *l = leds;
            while (l->next) {
                if (l->next->gpio == gpio) {
                    led = l->next;
                    l->next = l->next->next;
                    break;
                }
            }
        }
        
        if (led) {
            if (led->gpio != 0) {
                gpio_disable(led->gpio);
            }
        }
    }
}
