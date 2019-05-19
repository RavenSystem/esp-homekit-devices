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

#define DISABLE_TIME                80
#define DOUBLEPRESS_TIME            400
#define LONGPRESS_TIME              410
#define VERYLONGPRESS_TIME          1500
#define HOLDPRESS_COUNT             5       // HOLDPRESS_TIME = HOLDPRESS_COUNT * 2000
#define BUTTON_EVAL_DELAY_MAX       110
#define BUTTON_EVAL_DELAY_MIN       10

typedef struct _adv_button {
    uint8_t gpio;
    bool inverted;
    
    button_callback_fn singlepress0_callback_fn;
    button_callback_fn singlepress_callback_fn;
    button_callback_fn doublepress_callback_fn;
    button_callback_fn longpress_callback_fn;
    button_callback_fn verylongpress_callback_fn;
    button_callback_fn holdpress_callback_fn;
    
    void *singlepress_args;
    void *doublepress_args;
    void *longpress_args;
    void *verylongpress_args;
    void *holdpress_args;
    
    bool state;
    bool old_state;
    volatile uint16_t value;

    uint8_t press_count;
    uint8_t hold_count;
    ETSTimer press_timer;
    ETSTimer hold_timer;
    volatile uint32_t last_event_time;

    struct _adv_button *next;
} adv_button_t;

static adv_button_t *buttons = NULL;
static uint32_t disable_time = 0;
static uint8_t button_evaluate_delay = BUTTON_EVAL_DELAY_MIN;

static adv_button_t *button_find_by_gpio(const uint8_t gpio) {
    adv_button_t *button = buttons;
    
    while (button && button->gpio != gpio) {
        button = button->next;
    }

    return button;
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

IRAM void adv_button_set_disable_time() {
    disable_time = xTaskGetTickCountFromISR();
}

IRAM static void push_down(const uint8_t used_gpio) {
    const uint32_t now = xTaskGetTickCountFromISR();
    
    if (now - disable_time > DISABLE_TIME / portTICK_PERIOD_MS) {
        adv_button_t *button = button_find_by_gpio(used_gpio);
        if (button->singlepress0_callback_fn) {
            button->singlepress0_callback_fn(used_gpio, button->holdpress_args);
        } else {
            sdk_os_timer_arm(&button->hold_timer, 2000, 1);
        }
        button->hold_count = 0;
        button->last_event_time = now;
    }
}

IRAM static void push_up(const uint8_t used_gpio) {
    const uint32_t now = xTaskGetTickCountFromISR();

    if (now - disable_time > DISABLE_TIME / portTICK_PERIOD_MS) {
        adv_button_t *button = button_find_by_gpio(used_gpio);
        sdk_os_timer_disarm(&button->hold_timer);
        button->hold_count = 0;
        
        if (now - button->last_event_time > VERYLONGPRESS_TIME / portTICK_PERIOD_MS) {
            // Very Long button pressed
            button->press_count = 0;
            if (button->verylongpress_callback_fn) {
                button->verylongpress_callback_fn(used_gpio, button->verylongpress_args);
            } else if (button->longpress_callback_fn) {
                button->longpress_callback_fn(used_gpio, button->longpress_args);
            } else {
                button->singlepress_callback_fn(used_gpio, button->singlepress_args);
            }
        } else if (now - button->last_event_time > LONGPRESS_TIME / portTICK_PERIOD_MS) {
            // Long button pressed
            button->press_count = 0;
            if (button->longpress_callback_fn) {
                button->longpress_callback_fn(used_gpio, button->longpress_args);
            } else {
                button->singlepress_callback_fn(used_gpio, button->singlepress_args);
            }
        } else if (button->doublepress_callback_fn) {
            button->press_count++;
            if (button->press_count > 1) {
                // Double button pressed
                sdk_os_timer_disarm(&button->press_timer);
                button->press_count = 0;
                button->doublepress_callback_fn(used_gpio, button->doublepress_args);
            } else {
                sdk_os_timer_arm(&button->press_timer, DOUBLEPRESS_TIME, 0);
            }
        } else {
            button->singlepress_callback_fn(used_gpio, button->singlepress_args);
        }
    }
}

static void no_function_callback(const uint8_t gpio, void *args) {
    printf("!!! AdvButton: No function defined\n");
}

static void adv_button_single_callback(void *arg) {
    adv_button_t *button = arg;
    // Single button pressed
    button->press_count = 0;
    button->singlepress_callback_fn(button->gpio, button->singlepress_args);
}

static void adv_button_hold_callback(void *arg) {
    adv_button_t *button = arg;
    
    // Hold button pressed
    if (gpio_read(button->gpio) == 0) {
        button->hold_count++;
        
        if (button->hold_count == HOLDPRESS_COUNT) {
            sdk_os_timer_disarm(&button->hold_timer);
            button->hold_count = 0;
            button->press_count = 0;
            if (button->holdpress_callback_fn) {
                button->holdpress_callback_fn(button->gpio, button->holdpress_args);
            } else {
                no_function_callback(button->gpio, NULL);
            }
            
            adv_button_set_disable_time();
        }
    } else {
        sdk_os_timer_disarm(&button->hold_timer);
        button->hold_count = 0;
    }
}

#define maxvalue_unsigned(x) ((1 << (8 * sizeof(x))) - 1)
IRAM static void button_evaluate_fn() {        // Based on https://github.com/pcsaito/esp-homekit-demo/blob/LPFToggle/examples/sonoff_basic_toggle/toggle.c
    const TickType_t delay = pdMS_TO_TICKS(button_evaluate_delay);
    TickType_t last_wake_time = xTaskGetTickCount();
    
    for (;;) {
        adv_button_t *button = buttons;
        
        if (!buttons) {
            vTaskDelete(NULL);
        }
        
        while (button) {
            button->value += ((gpio_read(button->gpio) * maxvalue_unsigned(button->value)) - button->value) >> 3;
            button->state = (button->value > (maxvalue_unsigned(button->value) >> 1));
            
            if (button->state != button->old_state) {
                button->old_state = button->state;
                
                if (gpio_read(button->gpio) ^ button->inverted) {   // 1
                    push_up(button->gpio);
                } else {                                            // 0
                    push_down(button->gpio);
                }
            }
            
            button = button->next;
        }
        
        vTaskDelayUntil(&last_wake_time, delay);
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
            xTaskCreate(button_evaluate_fn, "button_evaluate_fn", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
        }
        
        button->next = buttons;
        buttons = button;
        
        button->hold_count = 0;
        button->press_count = 0;
        
        if (button->gpio != 0) {
            gpio_enable(button->gpio, GPIO_INPUT);
        }
        
        gpio_set_pullup(button->gpio, pullup_resistor, pullup_resistor);
        
        button->state = gpio_read(button->gpio);
        
        button->old_state = button->state;
        button->value = 32764;
        
        sdk_os_timer_setfn(&button->hold_timer, adv_button_hold_callback, button);
        sdk_os_timer_setfn(&button->press_timer, adv_button_single_callback, button);
        
        button->singlepress_callback_fn = no_function_callback;
        
        return 0;
    }

    return -1;
}

int adv_button_register_callback_fn(const uint8_t gpio, const button_callback_fn callback, const uint8_t button_callback_type, void *args) {
    adv_button_t *button = button_find_by_gpio(gpio);
    
    if (button) {
        switch (button_callback_type) {
            case 0:
                button->singlepress0_callback_fn = callback;
                button->holdpress_callback_fn = NULL;
                button->holdpress_args = args;
                break;
            case 1:
                if (callback) {
                    button->singlepress_callback_fn = callback;
                    button->singlepress_args = args;
                } else {
                    button->singlepress_callback_fn = no_function_callback;
                }
                break;
                
            case 2:
                button->doublepress_callback_fn = callback;
                button->doublepress_args = args;
                break;
                
            case 3:
                button->longpress_callback_fn = callback;
                button->longpress_args = args;
                break;
                
            case 4:
                button->verylongpress_callback_fn = callback;
                button->verylongpress_args = args;
                break;
                
            case 5:
                button->holdpress_callback_fn = callback;
                button->singlepress0_callback_fn = NULL;
                button->holdpress_args = args;
                break;
                
            default:
                return -2;
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
            gpio_set_interrupt(button->gpio, GPIO_INTTYPE_EDGE_ANY, NULL);
            if (button->gpio != 0) {
                gpio_disable(button->gpio);
            }
        }
    }
}
