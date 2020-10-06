/*
 * Advanced Button Manager
 *
 * Copyright 2019-2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

/*
 * Based on Button library by Maxim Kulkin (@MaximKulkin), licensed under the MIT License.
 * https://github.com/maximkulkin/esp-homekit-demo/blob/master/examples/button/button.c
 */

#include <string.h>
#include <FreeRTOS.h>
#include <timers.h>
#include <esplibs/libmain.h>

#include "adv_button.h"

#define ADV_BUTTON_DEFAULT_EVAL     (6)

#define DOUBLEPRESS_TIME            (400)
#define LONGPRESS_TIME              (DOUBLEPRESS_TIME + 10)
#define VERYLONGPRESS_TIME          (1500)
#define HOLDPRESS_TIME              (8000)

#define BUTTON_EVAL_DELAY_MIN       (10)
#define BUTTON_EVAL_DELAY_DEFAULT   (10)
#define BUTTON_EVAL_DELAY_MAX       (BUTTON_EVAL_DELAY_MIN + 245)

#define DISABLE_PRESS_COUNT         (31)

#define DISABLE_TIME                (ADV_BUTTON_DEFAULT_EVAL * 10)
#define MIN(x, y)                   (((x) < (y)) ? (x) : (y))
#define MAX(x, y)                   (((x) > (y)) ? (x) : (y))

#define XTIMER_BLOCK_TIME           (50)
#define XTIMER_PERIOD_BLOCK_TIME    (10)

typedef struct _adv_button_callback_fn {
    uint8_t param;
    button_callback_fn callback;
    
    void* args;

    struct _adv_button_callback_fn* next;
} adv_button_callback_fn_t;

typedef struct _adv_button {
    uint8_t gpio;
    uint8_t max_eval;
    volatile uint8_t value;
    
    uint8_t press_count: 5;
    
    bool inverted: 1;
    bool state: 1;
    bool old_state: 1;
    
    TimerHandle_t press_timer;
    TimerHandle_t hold_timer;
    
    volatile uint32_t last_event_time;
    
    adv_button_callback_fn_t* singlepress0_callback_fn;
    adv_button_callback_fn_t* singlepress_callback_fn;
    adv_button_callback_fn_t* doublepress_callback_fn;
    adv_button_callback_fn_t* longpress_callback_fn;
    adv_button_callback_fn_t* verylongpress_callback_fn;
    adv_button_callback_fn_t* holdpress_callback_fn;

    struct _adv_button* next;
} adv_button_t;

typedef struct _adv_button_main_config {
    uint32_t disable_time;
    uint16_t button_evaluate_sleep_countdown;
    uint8_t button_evaluate_delay;
    bool button_evaluate_is_working: 1;
    bool is_gpio16: 1;
    
    TimerHandle_t button_evaluate_timer;

    adv_button_t* buttons;
} adv_button_main_config_t;

static adv_button_main_config_t* adv_button_main_config = NULL;


static adv_button_t* button_find_by_gpio(const uint8_t gpio) {
    if (adv_button_main_config) {
        adv_button_t* button = adv_button_main_config->buttons;
        
        while (button && button->gpio != gpio) {
            button = button->next;
        }

        return button;
    }
    
    return NULL;
}

static void adv_button_run_callback_fn(adv_button_callback_fn_t* callbacks, const uint8_t gpio) {
    adv_button_callback_fn_t* adv_button_callback_fn = callbacks;
    
    while (adv_button_callback_fn) {
        adv_button_callback_fn->callback(gpio, adv_button_callback_fn->args, adv_button_callback_fn->param);
        adv_button_callback_fn = adv_button_callback_fn->next;
    }
}

IRAM static void push_down(const uint8_t used_gpio) {
    const uint32_t now = xTaskGetTickCountFromISR();
    
    if (now - adv_button_main_config->disable_time > DISABLE_TIME / portTICK_PERIOD_MS) {
        adv_button_t *button = button_find_by_gpio(used_gpio);
        if (button->singlepress0_callback_fn) {
            adv_button_run_callback_fn(button->singlepress0_callback_fn, button->gpio);
        } else {
            xTimerStart(button->hold_timer, XTIMER_BLOCK_TIME);
        }
        button->last_event_time = now;
    }
}

IRAM static void push_up(const uint8_t used_gpio) {
    const uint32_t now = xTaskGetTickCountFromISR();
    
    if (now - adv_button_main_config->disable_time > DISABLE_TIME / portTICK_PERIOD_MS) {
        adv_button_t *button = button_find_by_gpio(used_gpio);
        
        if (button->press_count == DISABLE_PRESS_COUNT) {
            button->press_count = 0;
            return;
        }
        
        xTimerStop(button->hold_timer, XTIMER_BLOCK_TIME);

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
                xTimerStop(button->press_timer, XTIMER_BLOCK_TIME);
                button->press_count = 0;
                adv_button_run_callback_fn(button->doublepress_callback_fn, button->gpio);
            } else {
                xTimerStart(button->press_timer, XTIMER_BLOCK_TIME);
            }
        } else {
            adv_button_run_callback_fn(button->singlepress_callback_fn, button->gpio);
        }
    }
}

static void adv_button_single_callback(TimerHandle_t xTimer) {
    adv_button_t *button = (adv_button_t*) pvTimerGetTimerID(xTimer);
    // Single button pressed
    button->press_count = 0;
    adv_button_run_callback_fn(button->singlepress_callback_fn, button->gpio);
}

static void adv_button_hold_callback(TimerHandle_t xTimer) {
    adv_button_t* button = (adv_button_t*) pvTimerGetTimerID(xTimer);
    // Hold button pressed
    button->press_count = DISABLE_PRESS_COUNT;
    adv_button_run_callback_fn(button->holdpress_callback_fn, button->gpio);
}

IRAM static void button_evaluate_fn() {
    if (!adv_button_main_config->button_evaluate_is_working) {
        if (!adv_button_main_config->is_gpio16) {
            adv_button_main_config->button_evaluate_sleep_countdown -= 1;
            
            if (adv_button_main_config->button_evaluate_sleep_countdown == 0) {
                xTimerStop(adv_button_main_config->button_evaluate_timer, XTIMER_BLOCK_TIME);
            }
        }
        
        adv_button_main_config->button_evaluate_is_working = true;
        adv_button_t* button = adv_button_main_config->buttons;
        
        while (button) {
            if (gpio_read(button->gpio)) {
                button->value = MIN(button->value++, button->max_eval);
                if (button->value == button->max_eval) {
                    button->state = true;
                }
            } else {
                button->value = MAX(button->value--, 0);
                if (button->value == 0) {
                    button->state = false;
                }
            }

            if (button->state != button->old_state) {
                button->old_state = button->state;
                
                if (button->state ^ button->inverted) {     // 1 HIGH
                    push_up(button->gpio);
                } else {                                    // 0 LOW
                    push_down(button->gpio);
                }
            }
            
            button = button->next;
        }
        
        adv_button_main_config->button_evaluate_is_working = false;
    }
}

IRAM static void adv_button_interrupt(const uint8_t gpio) {
    gpio_set_interrupt(gpio, GPIO_INTTYPE_NONE, adv_button_interrupt);
    
    if (adv_button_main_config->button_evaluate_sleep_countdown == 0) {
        adv_button_main_config->button_evaluate_sleep_countdown = (HOLDPRESS_TIME + 1000) / adv_button_main_config->button_evaluate_delay;
        button_evaluate_fn();
        
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTimerStartFromISR(adv_button_main_config->button_evaluate_timer, &xHigherPriorityTaskWoken);
    }

    adv_button_main_config->button_evaluate_sleep_countdown = (HOLDPRESS_TIME + 1000) / adv_button_main_config->button_evaluate_delay;
    
    gpio_set_interrupt(gpio, GPIO_INTTYPE_EDGE_ANY, adv_button_interrupt);
}

void adv_button_init() {
    if (!adv_button_main_config) {
        adv_button_main_config = malloc(sizeof(adv_button_main_config_t));
        memset(adv_button_main_config, 0, sizeof(*adv_button_main_config));
        
        adv_button_main_config->disable_time = 0;
        adv_button_main_config->button_evaluate_delay = BUTTON_EVAL_DELAY_DEFAULT;
        adv_button_main_config->button_evaluate_is_working = false;
        adv_button_main_config->button_evaluate_sleep_countdown = 0;
        adv_button_main_config->is_gpio16 = false;
        adv_button_main_config->buttons = NULL;
        
        adv_button_main_config->button_evaluate_timer = xTimerCreate(0, pdMS_TO_TICKS(adv_button_main_config->button_evaluate_delay), pdTRUE, NULL, button_evaluate_fn);
    }
}

void adv_button_set_gpio_probes(const uint8_t gpio, const uint8_t max_eval) {
    adv_button_t *button = button_find_by_gpio(gpio);
    if (button) {
        if (max_eval == 0) {
            button->max_eval = ADV_BUTTON_DEFAULT_EVAL;
        } else {
            button->max_eval = max_eval;
        }
        
        if (button->state) {
            button->value = button->max_eval;
        }
    }
}

void adv_button_set_evaluate_delay(const uint8_t new_delay) {
    adv_button_init();
    
    if (new_delay < BUTTON_EVAL_DELAY_MIN) {
        adv_button_main_config->button_evaluate_delay = BUTTON_EVAL_DELAY_MIN;
    } else if (new_delay > BUTTON_EVAL_DELAY_MAX) {
        adv_button_main_config->button_evaluate_delay = BUTTON_EVAL_DELAY_MAX;
    } else {
        adv_button_main_config->button_evaluate_delay = new_delay;
    }
    
    xTimerChangePeriod(adv_button_main_config->button_evaluate_timer, pdMS_TO_TICKS(adv_button_main_config->button_evaluate_delay), XTIMER_PERIOD_BLOCK_TIME);
}

void adv_button_set_disable_time() {
    if (adv_button_main_config) {
        adv_button_main_config->disable_time = xTaskGetTickCountFromISR();
    }
}

int adv_button_create(const uint8_t gpio, const bool pullup_resistor, const bool inverted) {
    adv_button_init();
    
    adv_button_t* button = button_find_by_gpio(gpio);
    
    if (!button) {
        button = malloc(sizeof(adv_button_t));
        memset(button, 0, sizeof(*button));
        button->gpio = gpio;
        button->max_eval = ADV_BUTTON_DEFAULT_EVAL;
        button->inverted = inverted;
        button->press_count = 0;
        adv_button_main_config->button_evaluate_sleep_countdown = 0;
        
        button->next = adv_button_main_config->buttons;
        adv_button_main_config->buttons = button;
 
        if (button->gpio != 0) {
            gpio_enable(gpio, GPIO_INPUT);
        }
        
        gpio_set_pullup(gpio, pullup_resistor, pullup_resistor);
        
        button->state = gpio_read(gpio);
        
        button->old_state = button->state;
        
        if (button->state) {
            button->value = button->max_eval;
        } else {
            button->value = 0;
        }

        button->hold_timer = xTimerCreate(0, pdMS_TO_TICKS(HOLDPRESS_TIME), pdFALSE, (void*) button, adv_button_hold_callback);
        button->press_timer = xTimerCreate(0, pdMS_TO_TICKS(DOUBLEPRESS_TIME), pdFALSE, (void*) button, adv_button_single_callback);
        
        if (gpio != 16) {
            gpio_set_interrupt(gpio, GPIO_INTTYPE_EDGE_ANY, adv_button_interrupt);
            
        } else if (!adv_button_main_config->is_gpio16) {
            adv_button_main_config->is_gpio16 = true;
            xTimerStart(adv_button_main_config->button_evaluate_timer, XTIMER_BLOCK_TIME);
            
            adv_button_t *button = adv_button_main_config->buttons;
            while (button) {
                gpio_set_interrupt(button->gpio, GPIO_INTTYPE_NONE, NULL);
                button = button->next;
            }
        }
        
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

int adv_button_destroy(const uint8_t gpio) {
    if (!adv_button_main_config) {
        return -2;
    }
    
    if (adv_button_main_config->buttons) {
        adv_button_t *button = NULL;
        if (adv_button_main_config->buttons->gpio == gpio) {
            
            gpio_set_interrupt(gpio, GPIO_INTTYPE_NONE, NULL);
            
            if (button->gpio != 0) {
                gpio_disable(gpio);
            }
            
            xTimerDelete(button->hold_timer, XTIMER_BLOCK_TIME);
            xTimerDelete(button->press_timer, XTIMER_BLOCK_TIME);
            
            button = adv_button_main_config->buttons;
            adv_button_main_config->buttons = adv_button_main_config->buttons->next;
        } else {
            adv_button_t *b = adv_button_main_config->buttons;
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

        if (!adv_button_main_config->buttons) {
            xTimerDelete(adv_button_main_config->button_evaluate_timer, XTIMER_BLOCK_TIME);
            
            free(adv_button_main_config);
            adv_button_main_config = NULL;
            
        } else if (gpio == 16) {
            adv_button_t *button = adv_button_main_config->buttons;
            while (button) {
                gpio_set_interrupt(button->gpio, GPIO_INTTYPE_EDGE_ANY, adv_button_interrupt);
                button = button->next;
            }
            
            xTimerStop(adv_button_main_config->button_evaluate_timer, XTIMER_BLOCK_TIME);
            adv_button_main_config->is_gpio16 = false;
        }
        
        return 0;
    }
    
    return -1;
}
