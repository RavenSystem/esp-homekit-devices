/*
 * LED Codes Library
 * 
 * Copyright 2018-2022 José Antonio Jiménez Campos (@RavenSystem)
 *  
 */

#include <string.h>
#include <stdio.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <timers_helper.h>
#include <esplibs/libmain.h>

#include "led_codes.h"

#define DURATION_OFF                (120)
#define DURATION_ON_MIN             (30)


typedef struct _led {
    uint8_t gpio: 5;
    bool inverted: 1;
    bool status: 1;
    uint8_t count;
    
    blinking_params_t blinking_params;
    
    TimerHandle_t timer;

    struct _led* next;
} led_t;

static led_t* leds = NULL;

static led_t* led_find_by_gpio(const uint8_t gpio) {
    led_t* led = leds;
    
    while (led && led->gpio != gpio) {
        led = led->next;
    }
    
    return led;
}

static void led_code_run(TimerHandle_t xTimer) {
    led_t* led = (led_t*) pvTimerGetTimerID(xTimer);
    int delay = DURATION_OFF;
    
    led->status = !led->status;
    gpio_write(led->gpio, led->status);
    
    if (led->status == led->inverted) {
        led->count++;
    } else {
        delay = (led->blinking_params.duration * 1000) + DURATION_ON_MIN;
    }
    
    if (led->count < led->blinking_params.times) {
        esp_timer_change_period(xTimer, delay);
    }
}

void led_code(const int gpio, blinking_params_t blinking_params) {
    led_t* led = led_find_by_gpio(gpio);
    
    if (led) {
        esp_timer_stop(led->timer);
        
        led->blinking_params = blinking_params;
        led->status = led->inverted;
        led->count = 0;
        
        led_code_run(led->timer);
    }
}

int led_create(const int gpio, const bool inverted) {
    led_t* led = led_find_by_gpio(gpio);
    
    if (!led) {
        led = calloc(1, sizeof(led_t));

        led->next = leds;
        leds = led;
        
        led->timer = esp_timer_create(10, false, (void*) led, led_code_run);
        
        led->gpio = gpio;
        led->inverted = inverted;
        led->status = inverted;
        
        gpio_enable(led->gpio, GPIO_OUTPUT);
        gpio_write(led->gpio, led->status);
        
        return 0;
    }

    return -1;
}

void led_destroy(const int gpio) {
    if (leds) {
        led_t* led = NULL;
        if (leds->gpio == gpio) {
            led = leds;
            leds = leds->next;
        } else {
            led_t* l = leds;
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
                //gpio_disable(led->gpio);
                gpio_enable(led->gpio, GPIO_INPUT);
            }
        }
    }
}
