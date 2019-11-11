/*
 * Advanced Button Manager
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

/*
 * Based on Button library by Maxim Kulkin (@MaximKulkin), licensed under the MIT License.
 * https://github.com/maximkulkin/esp-homekit-demo/blob/master/examples/button/button.c
 */

#include <string.h>
#include <etstimer.h>
#include <esplibs/libmain.h>
#include "adv_button.h"

#ifndef ADV_BUTTON_MAX_EVAL
#define ADV_BUTTON_MAX_EVAL         10
#endif

#define DOUBLEPRESS_TIME            400
#define LONGPRESS_TIME              (DOUBLEPRESS_TIME + 10)
#define VERYLONGPRESS_TIME          1500
#define HOLDPRESS_TIME              10000

#define BUTTON_EVAL_DELAY_MIN       10
#define BUTTON_EVAL_DELAY_MAX       (BUTTON_EVAL_DELAY_MIN + 200)

#define DISABLE_PRESS_COUNT         200

#define EVALUATE_MID                (ADV_BUTTON_MAX_EVAL >> 1)
#define DISABLE_TIME                (ADV_BUTTON_MAX_EVAL * 10)
#define MIN(x, y)                   (((x) < (y)) ? (x) : (y))
#define MAX(x, y)                   (((x) > (y)) ? (x) : (y))

typedef struct _adv_button_callback_fn {
    button_callback_fn callback;
    void *args;
    uint8_t param;
    
    struct _adv_button_callback_fn *next;
} adv_button_callback_fn_t;

typedef struct _adv_button {
    uint8_t gpio;
    bool inverted;
    
    adv_button_callback_fn_t *singlepress0_callback_fn;
    adv_button_callback_fn_t *singlepress_callback_fn;
    adv_button_callback_fn_t *doublepress_callback_fn;
    adv_button_callback_fn_t *longpress_callback_fn;
    adv_button_callback_fn_t *verylongpress_callback_fn;
    adv_button_callback_fn_t *holdpress_callback_fn;
    
    bool state;
    bool old_state;
    volatile uint8_t value;

    uint8_t press_count;
    ETSTimer press_timer;
    ETSTimer hold_timer;
    volatile uint32_t last_event_time;

    struct _adv_button *next;
} adv_button_t;

static uint32_t disable_time = 0;
static uint8_t button_evaluate_delay = BUTTON_EVAL_DELAY_MIN;
static bool button_evaluate_is_working = false;
static ETSTimer button_evaluate_timer;

static adv_button_t *buttons = NULL;

static adv_button_t *button_find_by_gpio(const uint8_t gpio) {
    adv_button_t *button = buttons;
    
    while (button && button->gpio != gpio) {
        button = button->next;
    }

    return button;
}

static void adv_button_run_callback_fn(adv_button_callback_fn_t *callbacks, const uint8_t gpio) {
    adv_button_callback_fn_t *adv_button_callback_fn = callbacks;
    
    while (adv_button_callback_fn) {
        adv_button_callback_fn->callback(gpio, adv_button_callback_fn->args, adv_button_callback_fn->param);
        adv_button_callback_fn = adv_button_callback_fn->next;
    }
}

void adv_button_set_evaluate_delay(const uint8_t new_delay) {
    if (new_delay < BUTTON_EVAL_DELAY_MIN) {
        button_evaluate_delay = BUTTON_EVAL_DELAY_MIN;
    } else if (new_delay > BUTTON_EVAL_DELAY_MAX) {
        button_evaluate_delay = BUTTON_EVAL_DELAY_MAX;
    } else {
        button_evaluate_delay = new_delay;
    }
}

void adv_button_set_disable_time() {
    disable_time = xTaskGetTickCountFromISR();
}

IRAM static void push_down(const uint8_t used_gpio) {
    const uint32_t now = xTaskGetTickCountFromISR();
    
    if (now - disable_time > DISABLE_TIME / portTICK_PERIOD_MS) {
        adv_button_t *button = button_find_by_gpio(used_gpio);
        if (button->singlepress0_callback_fn) {
            adv_button_run_callback_fn(button->singlepress0_callback_fn, button->gpio);
        } else {
            sdk_os_timer_arm(&button->hold_timer, HOLDPRESS_TIME, 0);
        }
        button->last_event_time = now;
    }
}

IRAM static void push_up(const uint8_t used_gpio) {
    const uint32_t now = xTaskGetTickCountFromISR();
    
    if (now - disable_time > DISABLE_TIME / portTICK_PERIOD_MS) {
        adv_button_t *button = button_find_by_gpio(used_gpio);
        
        if (button->press_count == DISABLE_PRESS_COUNT) {
            button->press_count = 0;
            return;
        }
        
        sdk_os_timer_disarm(&button->hold_timer);
        if (now - button->last_event_time > VERYLONGPRESS_TIME / portTICK_PERIOD_MS) {
            // Very Long button pressed
            button->press_count = 0;
            if (button->verylongpress_callback_fn) {
                adv_button_run_callback_fn(button->verylongpress_callback_fn, button->gpio);
            } else if (button->longpress_callback_fn) {
                adv_button_run_callback_fn(button->longpress_callback_fn, button->gpio);
            } else {
                adv_button_run_callback_fn(button->singlepress_callback_fn, button->gpio);
            }
        } else if (now - button->last_event_time > LONGPRESS_TIME / portTICK_PERIOD_MS) {
            // Long button pressed
            button->press_count = 0;
            if (button->longpress_callback_fn) {
                adv_button_run_callback_fn(button->longpress_callback_fn, button->gpio);
            } else {
                adv_button_run_callback_fn(button->singlepress_callback_fn, button->gpio);
            }
        } else if (button->doublepress_callback_fn) {
            button->press_count++;
            if (button->press_count > 1) {
                // Double button pressed
                sdk_os_timer_disarm(&button->press_timer);
                button->press_count = 0;
                adv_button_run_callback_fn(button->doublepress_callback_fn, button->gpio);
            } else {
                sdk_os_timer_arm(&button->press_timer, DOUBLEPRESS_TIME, 0);
            }
        } else {
            adv_button_run_callback_fn(button->singlepress_callback_fn, button->gpio);
        }
    }
}

static void adv_button_single_callback(void *arg) {
    adv_button_t *button = arg;
    // Single button pressed
    button->press_count = 0;
    adv_button_run_callback_fn(button->singlepress_callback_fn, button->gpio);
}

static void adv_button_hold_callback(void *arg) {
    adv_button_t *button = arg;
    
    // Hold button pressed
    button->press_count = DISABLE_PRESS_COUNT;
    
    adv_button_run_callback_fn(button->holdpress_callback_fn, button->gpio);
}

IRAM static void button_evaluate_fn() {
    if (!button_evaluate_is_working) {
        button_evaluate_is_working = true;
        adv_button_t *button = buttons;
        
        while (button) {
            if (gpio_read(button->gpio)) {
                button->value = MIN(button->value++, ADV_BUTTON_MAX_EVAL);
            } else {
                button->value = MAX(button->value--, 0);
            }
            
            button->state = (button->value > EVALUATE_MID);
            
            if (button->state != button->old_state) {
                button->old_state = button->state;
                
                if (gpio_read(button->gpio) ^ button->inverted) {   // 1 HIGH
                    push_up(button->gpio);
                } else {                                            // 0 LOW
                    push_down(button->gpio);
                }
            }
            
            button = button->next;
        }
        
        button_evaluate_is_working = false;
    }
}

int adv_button_create(const uint8_t gpio, const bool pullup_resistor, const bool inverted) {
    adv_button_t *button = button_find_by_gpio(gpio);
    
    if (!button) {
        button = malloc(sizeof(adv_button_t));
        memset(button, 0, sizeof(*button));
        button->gpio = gpio;
        button->inverted = inverted;
        
        if (!buttons) {
            sdk_os_timer_setfn(&button_evaluate_timer, button_evaluate_fn, NULL);
            sdk_os_timer_arm(&button_evaluate_timer, button_evaluate_delay, 1);
        }
        
        button->next = buttons;
        buttons = button;
        
        button->press_count = 0;
        
        if (button->gpio != 0) {
            gpio_enable(button->gpio, GPIO_INPUT);
        }
        
        gpio_set_pullup(button->gpio, pullup_resistor, pullup_resistor);
        
        button->state = gpio_read(button->gpio);
        
        button->old_state = button->state;
        button->value = EVALUATE_MID;
        
        sdk_os_timer_setfn(&button->hold_timer, adv_button_hold_callback, button);
        sdk_os_timer_setfn(&button->press_timer, adv_button_single_callback, button);
        
        return 0;
    }

    return -1;
}

int adv_button_register_callback_fn(const uint8_t gpio, const button_callback_fn callback, const uint8_t button_callback_type, void *args, const uint8_t param) {
    adv_button_t *button = button_find_by_gpio(gpio);
    
    if (button) {
        adv_button_callback_fn_t *adv_button_callback_fn;
        adv_button_callback_fn = malloc(sizeof(adv_button_callback_fn_t));
        memset(adv_button_callback_fn, 0, sizeof(*adv_button_callback_fn));
        
        adv_button_callback_fn->callback = callback;
        adv_button_callback_fn->args = args;
        adv_button_callback_fn->param = param;
        
        switch (button_callback_type) {
            case INVSINGLEPRESS_TYPE:
                adv_button_callback_fn->next = button->singlepress0_callback_fn;
                button->singlepress0_callback_fn = adv_button_callback_fn;
                break;
                
            case DOUBLEPRESS_TYPE:
                adv_button_callback_fn->next = button->doublepress_callback_fn;
                button->doublepress_callback_fn = adv_button_callback_fn;
                break;
                
            case LONGPRESS_TYPE:
                adv_button_callback_fn->next = button->longpress_callback_fn;
                button->longpress_callback_fn = adv_button_callback_fn;
                break;
                
            case VERYLONGPRESS_TYPE:
                adv_button_callback_fn->next = button->verylongpress_callback_fn;
                button->verylongpress_callback_fn = adv_button_callback_fn;
                break;
                
            case HOLDPRESS_TYPE:
                adv_button_callback_fn->next = button->holdpress_callback_fn;
                button->holdpress_callback_fn = adv_button_callback_fn;
                break;
                
            default:    // case SINGLEPRESS_TYPE:
                adv_button_callback_fn->next = button->singlepress_callback_fn;
                button->singlepress_callback_fn = adv_button_callback_fn;
                break;
        }
        
        return 0;
    }
    
    return -1;
}

void adv_button_destroy(const uint8_t gpio) {
    if (buttons) {
        adv_button_t *button = NULL;
        if (buttons->gpio == gpio) {
            
            if (button->gpio != 0) {
                gpio_disable(button->gpio);
            }
            
            button = buttons;
            buttons = buttons->next;
        } else {
            adv_button_t *b = buttons;
            while (b->next) {
                if (b->next->gpio == gpio) {
                    
                    if (b->next->gpio != 0) {
                        gpio_disable(b->next->gpio);
                    }
                    
                    button = b->next;
                    b->next = b->next->next;
                    break;
                }
            }
        }

        if (!buttons) {
            sdk_os_timer_disarm(&button_evaluate_timer);
        }
    }
}
