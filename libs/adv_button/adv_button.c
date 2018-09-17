/*
 * Advanced Button Manager
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

/*
 * Based on Buton library by Maxin Kulkin (@MaximKulkin), licensed under the MIT License.
 * https://github.com/maximkulkin/esp-homekit-demo/blob/master/examples/button/button.c
 */

#include <string.h>
#include <etstimer.h>
#include <esplibs/libmain.h>
#include "adv_button.h"

#define DEBOUNCE_TIME       60
#define DOUBLEPRESS_TIME    400
#define LONGPRESS_TIME      450
#define VERYLONGPRESS_TIME  1200
#define HOLDPRESS_TIME      10000

typedef struct _adv_button {
    uint8_t gpio;
    
    button_callback_fn singlepress_callback_fn;
    button_callback_fn doublepress_callback_fn;
    button_callback_fn longpress_callback_fn;
    button_callback_fn verylongpress_callback_fn;
    button_callback_fn holdpress_callback_fn;

    uint8_t press_count;
    ETSTimer press_timer;
    ETSTimer hold_timer;
    uint32_t last_press_time;
    uint32_t last_event_time;

    struct _adv_button *next;
} adv_button_t;

adv_button_t *buttons = NULL;

static adv_button_t *button_find_by_gpio(const uint8_t gpio) {
    adv_button_t *button = buttons;
    while (button && button->gpio != gpio) {
        button = button->next;
    }

    return button;
}

void adv_button_timing_reset() {
    adv_button_t *button = buttons;
    
    uint32_t now = xTaskGetTickCountFromISR() + (20 / portTICK_PERIOD_MS);
    
    while (button) {
        button->press_count = 0;
        button->last_press_time = now;
        button->last_event_time = now;
        sdk_os_timer_disarm(&button->press_timer);
        sdk_os_timer_disarm(&button->hold_timer);
        
        button = button->next;
    }
}

void adv_button_intr_callback(const uint8_t gpio) {
    adv_button_t *button = button_find_by_gpio(gpio);
    if (!button) {
        return;
    }
    
    uint32_t now = xTaskGetTickCountFromISR();
    
    if (gpio_read(gpio) == 1) {
        sdk_os_timer_disarm(&button->hold_timer);
        
        if ((now - button->last_press_time) * portTICK_PERIOD_MS > DEBOUNCE_TIME) {
            button->last_press_time = now;
            
            if ((now - button->last_event_time) * portTICK_PERIOD_MS > VERYLONGPRESS_TIME) {
                printf(">>> AdvButton: Very Long button pressed\n");
                button->press_count = 0;
                if (button->verylongpress_callback_fn) {
                    button->verylongpress_callback_fn(gpio);
                } else if (button->longpress_callback_fn) {
                    button->longpress_callback_fn(gpio);
                } else {
                    button->singlepress_callback_fn(gpio);
                }
            } else if ((now - button->last_event_time) * portTICK_PERIOD_MS > LONGPRESS_TIME) {
                printf(">>> AdvButton: Long button pressed\n");
                button->press_count = 0;
                if (button->longpress_callback_fn) {
                    button->longpress_callback_fn(gpio);
                } else {
                    button->singlepress_callback_fn(gpio);
                }
            } else if (button->doublepress_callback_fn) {
                button->press_count++;
                if (button->press_count > 1) {
                    printf(">>> AdvButton: Double button pressed\n");
                    sdk_os_timer_disarm(&button->press_timer);
                    button->press_count = 0;
                    button->doublepress_callback_fn(gpio);
                } else {
                    sdk_os_timer_arm(&button->press_timer, DOUBLEPRESS_TIME, 0);
                }
            } else {
                button->singlepress_callback_fn(gpio);
            }
        }
    } else {
        sdk_os_timer_arm(&button->hold_timer, HOLDPRESS_TIME, 0);
        button->last_event_time = now;
    }
}

void no_function_callback(uint8_t gpio) {
    printf("!!! AdvButton: No function defined\n");
}

void adv_button_single_callback(void *arg) {
    adv_button_t *button = arg;
    printf(">>> AdvButton: Single button pressed\n");
    button->press_count = 0;
    button->singlepress_callback_fn(button->gpio);
}

void adv_button_hold_callback(void *arg) {
    adv_button_t *button = arg;
    printf(">>> AdvButton: Hold button pressed\n");
    button->press_count = 0;
    if (button->holdpress_callback_fn) {
        button->holdpress_callback_fn(button->gpio);
    } else {
        no_function_callback(button->gpio);
    }
}

int adv_button_create(const uint8_t gpio) {
    adv_button_t *button = button_find_by_gpio(gpio);
    if (button) {
        return -1;
    }

    button = malloc(sizeof(adv_button_t));
    memset(button, 0, sizeof(*button));
    button->gpio = gpio;

    button->next = buttons;
    buttons = button;

    if (button->gpio != 0) {
        gpio_enable(button->gpio, GPIO_INPUT);
    }
    
    gpio_set_pullup(button->gpio, true, true);
    gpio_set_interrupt(button->gpio, GPIO_INTTYPE_EDGE_ANY, adv_button_intr_callback);

    sdk_os_timer_disarm(&button->hold_timer);
    sdk_os_timer_setfn(&button->hold_timer, adv_button_hold_callback, button);
    sdk_os_timer_disarm(&button->press_timer);
    sdk_os_timer_setfn(&button->press_timer, adv_button_single_callback, button);
    
    button->singlepress_callback_fn = no_function_callback;
    
    return 0;
}

int adv_button_register_callback_fn(const uint8_t gpio, button_callback_fn callback, const uint8_t button_callback_type) {
    adv_button_t *button = button_find_by_gpio(gpio);
    if (!button) {
        return -1;
    }
    
    switch (button_callback_type) {
        case 1:
            if (callback) {
                button->singlepress_callback_fn = callback;
            } else {
                button->singlepress_callback_fn = no_function_callback;
            }
            break;
            
        case 2:
            button->doublepress_callback_fn = callback;
            break;
            
        case 3:
            button->longpress_callback_fn = callback;
            break;
            
        case 4:
            button->verylongpress_callback_fn = callback;
            break;
            
        case 5:
            button->holdpress_callback_fn = callback;
            break;
            
        default:
            return -2;
            break;
    }
    
    return 0;
}

void adv_button_destroy(const uint8_t gpio) {
    if (!buttons)
        return;

    adv_button_t *button = NULL;
    if (buttons->gpio == gpio) {
        button = buttons;
        buttons = buttons->next;
    } else {
        adv_button_t *b = buttons;
        while (b->next) {
            if (b->next->gpio == gpio) {
                button = b->next;
                b->next = b->next->next;
                break;
            }
        }
    }

    if (button) {
        sdk_os_timer_disarm(&button->hold_timer);
        sdk_os_timer_disarm(&button->press_timer);
        gpio_set_interrupt(button->gpio, GPIO_INTTYPE_EDGE_ANY, NULL);
        if (button->gpio != 0) {
            gpio_disable(button->gpio);
        }
    }
}
