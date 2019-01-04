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
 * Based on Button library by Maxim Kulkin (@MaximKulkin), licensed under the MIT License.
 * https://github.com/maximkulkin/esp-homekit-demo/blob/master/examples/button/button.c
 */

#include <string.h>
#include <etstimer.h>
#include <esplibs/libmain.h>
#include "adv_button.h"

#define DEBOUNCE_TIME               40
#define DISABLE_TIME                80
#define DOUBLEPRESS_TIME            400
#define LONGPRESS_TIME              450
#define VERYLONGPRESS_TIME          1200
#define HOLDPRESS_COUNT             5       // HOLDPRESS_TIME = HOLDPRESS_COUNT * 2000
#define TOGGLE_EVALUATE_INTERVAL    25

typedef struct _adv_button {
    uint8_t gpio;
    
    button_callback_fn singlepress_callback_fn;
    button_callback_fn doublepress_callback_fn;
    button_callback_fn longpress_callback_fn;
    button_callback_fn verylongpress_callback_fn;
    button_callback_fn holdpress_callback_fn;

    bool is_down;
    uint8_t press_count;
    uint8_t hold_count;
    ETSTimer press_timer;
    ETSTimer hold_timer;
    volatile uint32_t last_event_time;

    struct _adv_button *next;
} adv_button_t;

typedef struct _adv_toggle {
    uint8_t gpio;
    
    button_callback_fn low_callback_fn;
    button_callback_fn high_callback_fn;
    button_callback_fn both_callback_fn;
    
    bool state;
    bool old_state;
    volatile uint16_t value;
    
    struct _adv_toggle *next;
} adv_toggle_t;

static adv_button_t *buttons = NULL;
static adv_toggle_t *toggles = NULL;
static uint8_t used_gpio;
static uint32_t disable_time = 0;
static ETSTimer push_down_timer, push_up_timer, toggle_evaluate;

static adv_button_t *button_find_by_gpio(const uint8_t gpio) {
    adv_button_t *button = buttons;
    
    while (button && button->gpio != gpio) {
        button = button->next;
    }

    return button;
}

static adv_toggle_t *toggle_find_by_gpio(const uint8_t gpio) {
    adv_toggle_t *toggle = toggles;
    
    while (toggle && toggle->gpio != gpio) {
        toggle = toggle->next;
    }
    
    return toggle;
}

IRAM void adv_button_set_disable_time() {
    disable_time = xTaskGetTickCountFromISR();
}

IRAM static void push_down_timer_callback() {
    sdk_os_timer_disarm(&push_down_timer);
    
    const uint32_t now = xTaskGetTickCountFromISR();
    
    if (now - disable_time > DISABLE_TIME / portTICK_PERIOD_MS) {
        if (gpio_read(used_gpio) == 0) {
            adv_button_t *button = button_find_by_gpio(used_gpio);
            sdk_os_timer_arm(&button->hold_timer, 2000, 1);
            button->hold_count = 0;
            button->last_event_time = now;
            button->is_down = true;
        }
    }
}

IRAM static void push_up_timer_callback() {
    sdk_os_timer_disarm(&push_up_timer);
    
    const uint32_t now = xTaskGetTickCountFromISR();

    if (now - disable_time > DISABLE_TIME / portTICK_PERIOD_MS) {
        adv_button_t *button = button_find_by_gpio(used_gpio);
        if (gpio_read(used_gpio) == 1 && button->is_down) {
            sdk_os_timer_disarm(&button->hold_timer);
            button->is_down = false;
            button->hold_count = 0;
            
            if (now - button->last_event_time > VERYLONGPRESS_TIME / portTICK_PERIOD_MS) {
                // Very Long button pressed
                button->press_count = 0;
                if (button->verylongpress_callback_fn) {
                    button->verylongpress_callback_fn(used_gpio);
                } else if (button->longpress_callback_fn) {
                    button->longpress_callback_fn(used_gpio);
                } else {
                    button->singlepress_callback_fn(used_gpio);
                }
            } else if (now - button->last_event_time > LONGPRESS_TIME / portTICK_PERIOD_MS) {
                // Long button pressed
                button->press_count = 0;
                if (button->longpress_callback_fn) {
                    button->longpress_callback_fn(used_gpio);
                } else {
                    button->singlepress_callback_fn(used_gpio);
                }
            } else if (button->doublepress_callback_fn) {
                button->press_count++;
                if (button->press_count > 1) {
                    // Double button pressed
                    sdk_os_timer_disarm(&button->press_timer);
                    button->press_count = 0;
                    button->doublepress_callback_fn(used_gpio);
                } else {
                    sdk_os_timer_arm(&button->press_timer, DOUBLEPRESS_TIME, 0);
                }
            } else {
                button->singlepress_callback_fn(used_gpio);
            }
        }
    }
}

IRAM static void adv_button_intr_callback(const uint8_t gpio) {
    used_gpio = gpio;
    
    if (gpio_read(used_gpio) == 1) {
        sdk_os_timer_arm(&push_up_timer, DEBOUNCE_TIME, 0);
    } else {
        sdk_os_timer_arm(&push_down_timer, DEBOUNCE_TIME, 0);
    }
}

static void no_function_callback(uint8_t gpio) {
    printf("!!! AdvButton: No function defined\n");
}

static void adv_button_single_callback(void *arg) {
    adv_button_t *button = arg;
    // Single button pressed
    button->press_count = 0;
    button->singlepress_callback_fn(button->gpio);
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
                button->holdpress_callback_fn(button->gpio);
            } else {
                no_function_callback(button->gpio);
            }
            
            adv_button_set_disable_time();
        }
    } else {
        sdk_os_timer_disarm(&button->hold_timer);
        button->hold_count = 0;
    }
}

int adv_button_create(const uint8_t gpio, bool pullup_resistor) {
    adv_button_t *button = button_find_by_gpio(gpio);
    adv_toggle_t *toggle = toggle_find_by_gpio(gpio);
    
    if (!button && !toggle) {
        button = malloc(sizeof(adv_button_t));
        memset(button, 0, sizeof(*button));
        button->gpio = gpio;
        
        if (!buttons) {
            sdk_os_timer_disarm(&push_down_timer);
            sdk_os_timer_setfn(&push_down_timer, push_down_timer_callback, NULL);
            sdk_os_timer_disarm(&push_up_timer);
            sdk_os_timer_setfn(&push_up_timer, push_up_timer_callback, NULL);
        }
        
        button->next = buttons;
        buttons = button;
        
        button->is_down = false;
        button->hold_count = 0;
        button->press_count = 0;
        
        if (button->gpio != 0) {
            gpio_enable(button->gpio, GPIO_INPUT);
        }
        
        gpio_set_pullup(button->gpio, pullup_resistor, pullup_resistor);
        gpio_set_interrupt(button->gpio, GPIO_INTTYPE_EDGE_ANY, adv_button_intr_callback);
        
        sdk_os_timer_disarm(&button->hold_timer);
        sdk_os_timer_setfn(&button->hold_timer, adv_button_hold_callback, button);
        sdk_os_timer_disarm(&button->press_timer);
        sdk_os_timer_setfn(&button->press_timer, adv_button_single_callback, button);
        
        button->singlepress_callback_fn = no_function_callback;
        
        return 0;
    }

    return -1;
}

#define maxvalue_unsigned(x) ((1 << (8 * sizeof(x))) - 1)
IRAM static void toggle_evaluate_fn() {        // Based on https://github.com/pcsaito/esp-homekit-demo/blob/LPFToggle/examples/sonoff_basic_toggle/toggle.c
    adv_toggle_t *toggle = toggles;
    
    while (toggle) {
        toggle->value += ((gpio_read(toggle->gpio) * maxvalue_unsigned(toggle->value)) - toggle->value) >> 3;
        toggle->state = (toggle->value > (maxvalue_unsigned(toggle->value) >> 1));
        
        if (toggle->state != toggle->old_state) {
            toggle->old_state = toggle->state;

            if (toggle->both_callback_fn) {
                toggle->both_callback_fn(toggle->gpio);
            }
            
            if (gpio_read(toggle->gpio)) {
                if (toggle->high_callback_fn) {
                    toggle->high_callback_fn(toggle->gpio);
                }
            } else {
                if (toggle->low_callback_fn) {
                    toggle->low_callback_fn(toggle->gpio);
                }
            }
            
        }
        
        toggle = toggle->next;
    }
}

int adv_toggle_create(const uint8_t gpio, bool pullup_resistor) {
    adv_button_t *button = button_find_by_gpio(gpio);
    adv_toggle_t *toggle = toggle_find_by_gpio(gpio);
    
    if (!button && !toggle) {
        toggle = malloc(sizeof(adv_toggle_t));
        memset(toggle, 0, sizeof(*toggle));
        toggle->gpio = gpio;
        
        if (!toggles) {
            sdk_os_timer_disarm(&toggle_evaluate);
            sdk_os_timer_setfn(&toggle_evaluate, toggle_evaluate_fn, NULL);
            sdk_os_timer_arm(&toggle_evaluate, TOGGLE_EVALUATE_INTERVAL, 1);
        }
        
        toggle->next = toggles;
        toggles = toggle;
        
        if (toggle->gpio != 0) {
            gpio_enable(toggle->gpio, GPIO_INPUT);
        }
        
        gpio_set_pullup(toggle->gpio, pullup_resistor, pullup_resistor);
        
        toggle->state = gpio_read(toggle->gpio);
        
        toggle->old_state = toggle->state;
        toggle->value = 32764;
        
        return 0;
    }
    
    return -1;
}

int adv_button_register_callback_fn(const uint8_t gpio, button_callback_fn callback, const uint8_t button_callback_type) {
    adv_button_t *button = button_find_by_gpio(gpio);
    
    if (button) {
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
    
    return -1;
}

int adv_toggle_register_callback_fn(const uint8_t gpio, button_callback_fn callback, const uint8_t toggle_callback_type) {
    adv_toggle_t *toggle = toggle_find_by_gpio(gpio);
    
    if (toggle) {
        switch (toggle_callback_type) {
            case 0:
                toggle->low_callback_fn = callback;
                break;
                
            case 1:
                toggle->high_callback_fn = callback;
                break;
                
            case 2:
                toggle->both_callback_fn = callback;
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
            sdk_os_timer_disarm(&button->hold_timer);
            sdk_os_timer_disarm(&button->press_timer);
            gpio_set_interrupt(button->gpio, GPIO_INTTYPE_EDGE_ANY, NULL);
            if (button->gpio != 0) {
                gpio_disable(button->gpio);
            }
        }
    }
}

void adv_toggle_destroy(const uint8_t gpio) {
    if (toggles) {
        adv_toggle_t *toggle = NULL;
        if (toggles->gpio == gpio) {
            toggle = toggles;
            toggles = toggles->next;
        } else {
            adv_toggle_t *b = toggles;
            while (b->next) {
                if (b->next->gpio == gpio) {
                    toggle = b->next;
                    b->next = b->next->next;
                    break;
                }
            }
        }
        
        if (toggle) {
            if (toggle->gpio != 0) {
                gpio_disable(toggle->gpio);
            }
        }
        
        if (!toggles) {
            sdk_os_timer_disarm(&toggle_evaluate);
        }
    }
}
