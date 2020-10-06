/*
 * LED Codes Library
 * 
 * Copyright 2018-2020 José Antonio Jiménez Campos (@RavenSystem)
 *  
 */

#include <string.h>
#include <stdio.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <timers.h>
#include <esplibs/libmain.h>

#include "led_codes.h"

#define DURATION_OFF                (120)
#define DURATION_ON_MIN             (30)

#define XTIMER_BLOCK_TIME           (50)
#define XTIMER_PERIOD_BLOCK_TIME    (10)

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
    uint16_t delay = DURATION_OFF;
    
    led->status = !led->status;
    gpio_write(led->gpio, led->status);
    
    if (led->status == led->inverted) {
        led->count++;
    } else {
        delay = (led->blinking_params.duration * 1000) + DURATION_ON_MIN;
    }
    
    if (led->count < led->blinking_params.times) {
        xTimerChangePeriod(led->timer, pdMS_TO_TICKS(delay), XTIMER_PERIOD_BLOCK_TIME);
    }
}

void led_code(const uint8_t gpio, blinking_params_t blinking_params) {
    led_t* led = led_find_by_gpio(gpio);
    
    if (led) {
        xTimerStop(led->timer, XTIMER_BLOCK_TIME);
        
        led->blinking_params = blinking_params;
        led->status = led->inverted;
        led->count = 0;
        
        led_code_run(led->timer);
    }
}

int led_create(const uint8_t gpio, const bool inverted) {
    led_t* led = led_find_by_gpio(gpio);
    
    if (!led) {
        led = malloc(sizeof(led_t));
        memset(led, 0, sizeof(*led));

        led->next = leds;
        leds = led;
        
        led->timer = xTimerCreate(0, pdMS_TO_TICKS(10), pdFALSE, (void*) led, led_code_run);
        
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
                gpio_disable(led->gpio);
            }
        }
    }
}
