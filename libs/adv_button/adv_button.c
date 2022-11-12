/*
 * Advanced Button Manager
 *
 * Copyright 2019-2022 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

/*
 * Based on Button library by Maxim Kulkin (@MaximKulkin), licensed under the MIT License.
 * https://github.com/maximkulkin/esp-homekit-demo/blob/master/examples/button/button.c
 */

#include <string.h>
#include <FreeRTOS.h>
#include <timers_helper.h>
#include <esplibs/libmain.h>
#include <espressif/esp_common.h>
#include <adv_i2c.h>

#include "adv_button.h"

#define ADV_BUTTON_DEFAULT_EVAL     (5)

#define DOUBLEPRESS_TIME            (450)
#define LONGPRESS_TIME              (DOUBLEPRESS_TIME + 10)
#define VERYLONGPRESS_TIME          (1500)
#define HOLDPRESS_TIME              (8000)

#define BUTTON_EVAL_DELAY_MIN       (1)

#define DISABLE_PRESS_COUNT         (15)

#define MCP_CHANNEL_A               (0)
#define MCP_CHANNEL_B               (1)
#define MCP_CHANNEL_BOTH            (2)

#define DISABLE_TIME                (ADV_BUTTON_DEFAULT_EVAL * 10)
#define MIN(x, y)                   (((x) < (y)) ? (x) : (y))
#define MAX(x, y)                   (((x) > (y)) ? (x) : (y))

#define ADC_MID_VALUE               (511)


typedef struct _adv_button_callback_fn {
    uint8_t param;
    button_callback_fn callback;
    
    void* args;

    struct _adv_button_callback_fn* next;
} adv_button_callback_fn_t;

typedef struct _adv_button {
    uint16_t gpio;
    uint8_t mode;
    uint8_t max_eval;
    
    volatile uint8_t value;
    uint8_t press_count: 4;
    bool inverted: 1;
    bool state: 1;
    bool old_state: 1;

    TimerHandle_t press_timer;
    TimerHandle_t hold_timer;
    
    uint32_t last_event_time;
    
    adv_button_callback_fn_t* singlepress0_callback_fn;
    adv_button_callback_fn_t* singlepress_callback_fn;
    adv_button_callback_fn_t* doublepress_callback_fn;
    adv_button_callback_fn_t* longpress_callback_fn;
    adv_button_callback_fn_t* verylongpress_callback_fn;
    adv_button_callback_fn_t* holdpress_callback_fn;

    struct _adv_button* next;
} adv_button_t;

typedef struct _adv_button_mcp {
    uint8_t index;
    uint8_t addr;
    uint8_t value_a;
    uint8_t value_b;
    
    uint8_t bus: 1;
    uint8_t channels: 2;

    struct _adv_button_mcp* next;
} adv_button_mcp_t;

typedef struct _adv_button_main_config {
    uint32_t disable_time;
    
    uint16_t button_evaluate_sleep_countdown;
    uint16_t button_evaluate_sleep_time;
    
    uint8_t button_evaluate_delay;
    bool button_evaluate_is_working: 1;
    bool continuos_mode: 1;
    
    TimerHandle_t button_evaluate_timer;

    adv_button_t* buttons;
    adv_button_mcp_t* mcps;
} adv_button_main_config_t;

static adv_button_main_config_t* adv_button_main_config = NULL;

static adv_button_t* IRAM button_find_by_gpio(const unsigned int gpio) {
    if (adv_button_main_config) {
        adv_button_t* button = adv_button_main_config->buttons;
        
        while (button && button->gpio != gpio) {
            button = button->next;
        }
        
        return button;
    }
    
    return NULL;
}

static adv_button_mcp_t* mcp_find_by_index(const unsigned int index) {
    if (adv_button_main_config) {
        adv_button_mcp_t* mcp = adv_button_main_config->mcps;
        
        while (mcp && mcp->index != index) {
            mcp = mcp->next;
        }
        
        return mcp;
    }
    
    return NULL;
}

int adv_button_read_by_gpio(const unsigned int gpio) {
    int result = false;
    
    adv_button_t* button = button_find_by_gpio(gpio);
    if (button) {
        result = (bool) (button->state ^ button->inverted);
    }
    
    return result;
}

static int adv_button_read_mcp_gpio(const unsigned int gpio) {
    adv_button_mcp_t* adv_button_mcp = mcp_find_by_index(gpio / 100);
    if (adv_button_mcp) {
        const int mcp_gpio = gpio % 100;
        
        if (mcp_gpio >= 8) {    // Channel B
            return (bool) ((1 << (mcp_gpio - 8)) & adv_button_mcp->value_b);
        }
        
        // Channel A
        return (bool) ((1 << mcp_gpio) & adv_button_mcp->value_a);
    }
    
    return false;
}

static void adv_button_run_callback_fn(adv_button_callback_fn_t* callbacks, const unsigned int gpio) {
    adv_button_callback_fn_t* adv_button_callback_fn = callbacks;
    
    while (adv_button_callback_fn) {
        adv_button_callback_fn->callback(gpio, adv_button_callback_fn->args, adv_button_callback_fn->param);
        adv_button_callback_fn = adv_button_callback_fn->next;
    }
}

static void push_down(const unsigned int used_gpio) {
    const uint32_t now = xTaskGetTickCount();
    
    if (now - adv_button_main_config->disable_time > DISABLE_TIME / portTICK_PERIOD_MS) {
        adv_button_t *button = button_find_by_gpio(used_gpio);
        if (button->singlepress0_callback_fn) {
            adv_button_run_callback_fn(button->singlepress0_callback_fn, button->gpio);
        }

        esp_timer_start(button->hold_timer);

        button->last_event_time = now;
    }
}

static inline void push_up(const unsigned int used_gpio) {
    const uint32_t now = xTaskGetTickCount();
    
    if (now - adv_button_main_config->disable_time > DISABLE_TIME / portTICK_PERIOD_MS) {
        adv_button_t *button = button_find_by_gpio(used_gpio);
        
        if (button->press_count == DISABLE_PRESS_COUNT) {
            button->press_count = 0;
            return;
        }
        
        esp_timer_stop(button->hold_timer);

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
                esp_timer_stop(button->press_timer);
                button->press_count = 0;
                adv_button_run_callback_fn(button->doublepress_callback_fn, button->gpio);
            } else {
                esp_timer_start(button->press_timer);
            }
        } else {
            adv_button_run_callback_fn(button->singlepress_callback_fn, button->gpio);
        }
    }
}

static inline void adv_button_single_callback(TimerHandle_t xTimer) {
    adv_button_t *button = (adv_button_t*) pvTimerGetTimerID(xTimer);
    // Single button pressed
    button->press_count = 0;
    adv_button_run_callback_fn(button->singlepress_callback_fn, button->gpio);
}

static inline void adv_button_hold_callback(TimerHandle_t xTimer) {
    adv_button_t* button = (adv_button_t*) pvTimerGetTimerID(xTimer);
    // Hold button pressed
    button->press_count = DISABLE_PRESS_COUNT;
    adv_button_run_callback_fn(button->holdpress_callback_fn, button->gpio);
}

static void IRAM adv_button_interrupt_pulse(const uint8_t gpio) {
    gpio_set_interrupt(gpio, GPIO_INTTYPE_NONE, NULL);

    adv_button_t *button = button_find_by_gpio(gpio);
    button->value = MIN(button->value++, button->max_eval);
    
    gpio_set_interrupt(gpio, GPIO_INTTYPE_EDGE_NEG, adv_button_interrupt_pulse);
}

static void IRAM adv_button_interrupt_normal(const uint8_t gpio) {
    gpio_set_interrupt(gpio, GPIO_INTTYPE_NONE, NULL);
    
    adv_button_main_config->button_evaluate_sleep_countdown = 0;
    
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTimerStartFromISR(adv_button_main_config->button_evaluate_timer, &xHigherPriorityTaskWoken);
    
    adv_button_t* button = adv_button_main_config->buttons;
    while (button) {
        if (button->mode == ADV_BUTTON_NORMAL_MODE) {
            gpio_set_interrupt(button->gpio, GPIO_INTTYPE_NONE, NULL);
        }
        button = button->next;
    }
    
    if (xHigherPriorityTaskWoken) {
        portYIELD();
    }
}

static void button_evaluate_fn() {
    if (!adv_button_main_config->button_evaluate_is_working) {
        adv_button_main_config->button_evaluate_is_working = true;
        
        if (!adv_button_main_config->continuos_mode) {
            if (adv_button_main_config->button_evaluate_sleep_countdown < adv_button_main_config->button_evaluate_sleep_time) {
                adv_button_main_config->button_evaluate_sleep_countdown++;
            } else if (adv_button_main_config->button_evaluate_sleep_countdown == adv_button_main_config->button_evaluate_sleep_time) {
                adv_button_main_config->button_evaluate_sleep_countdown++;
                
                adv_button_t* button = adv_button_main_config->buttons;
                while (button) {
                    gpio_set_interrupt(button->gpio, GPIO_INTTYPE_EDGE_ANY, adv_button_interrupt_normal);
                    button = button->next;
                }
            } else {
                esp_timer_stop(adv_button_main_config->button_evaluate_timer);
            }
        }
        
        adv_button_mcp_t* mcp = adv_button_main_config->mcps;
        while (mcp) {
            if (mcp->channels == MCP_CHANNEL_A || mcp->channels == MCP_CHANNEL_BOTH) {
                const uint8_t reg = 0x12;
                i2c_slave_read(mcp->bus, mcp->addr, &reg, 1, &mcp->value_a, 1);
            }
            
            if (mcp->channels > MCP_CHANNEL_A) {
                const uint8_t reg = 0x13;
                i2c_slave_read(mcp->bus, mcp->addr, &reg, 1, &mcp->value_b, 1);
            }
            
            mcp = mcp->next;
        }

        adv_button_t* button = adv_button_main_config->buttons;
        while (button) {
            if (button->mode == ADV_BUTTON_NORMAL_MODE) {
                int read_value = false;
                if (button->gpio <= 16) {
                    read_value = gpio_read(button->gpio);
                } else {
                    read_value = (sdk_system_adc_read() > ADC_MID_VALUE);
                }
                
                if (read_value) {
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
            } else if (button->mode == ADV_BUTTON_PULSE_MODE) {
                if (button->value == button->max_eval) {
                    button->value = button->value >> 1;
                    button->state = true;
                } else {
                    button->value = MAX(button->value--, 0);
                    if (button->value == 0) {
                        button->state = false;
                    }
                }
            } else {    // MCP23017
                if (adv_button_read_mcp_gpio(button->gpio) != 0) {
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
            }
            
            if (button->state != button->old_state) {
                button->old_state = button->state;
                adv_button_main_config->button_evaluate_sleep_countdown = 0;
                
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

static void adv_button_init() {
    if (!adv_button_main_config) {
        adv_button_main_config = malloc(sizeof(adv_button_main_config_t));
        memset(adv_button_main_config, 0, sizeof(*adv_button_main_config));
        
        adv_button_main_config->button_evaluate_delay = BUTTON_EVAL_DELAY_MIN;
        adv_button_main_config->button_evaluate_sleep_time = (HOLDPRESS_TIME + 1000) / (BUTTON_EVAL_DELAY_MIN * portTICK_PERIOD_MS);
        
        adv_button_main_config->button_evaluate_timer = esp_timer_create(BUTTON_EVAL_DELAY_MIN * portTICK_PERIOD_MS, true, NULL, button_evaluate_fn);
    }
}

void adv_button_set_gpio_probes(const unsigned int gpio, const unsigned int max_eval) {
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

void adv_button_set_evaluate_delay(unsigned int new_delay_ms) {
    const unsigned int new_delay = new_delay_ms / portTICK_PERIOD_MS;
    
    adv_button_init();
    
    if (new_delay < BUTTON_EVAL_DELAY_MIN) {
        adv_button_main_config->button_evaluate_delay = BUTTON_EVAL_DELAY_MIN;
    } else {
        adv_button_main_config->button_evaluate_delay = new_delay;
    }
    
    adv_button_main_config->button_evaluate_sleep_time = ((HOLDPRESS_TIME + 1000) / portTICK_PERIOD_MS) / adv_button_main_config->button_evaluate_delay;
    esp_timer_change_period(adv_button_main_config->button_evaluate_timer, adv_button_main_config->button_evaluate_delay * portTICK_PERIOD_MS);
}

void adv_button_set_disable_time() {
    if (adv_button_main_config) {
        adv_button_main_config->disable_time = xTaskGetTickCount();
    }
}

int adv_button_create(const uint16_t gpio, const uint8_t pullup_resistor, const bool inverted, const uint8_t mode) {
    adv_button_init();
    
    adv_button_t* button = button_find_by_gpio(gpio);
    
    if (!button) {
        button = malloc(sizeof(adv_button_t));
        memset(button, 0, sizeof(*button));
        button->next = adv_button_main_config->buttons;
        adv_button_main_config->buttons = button;
        
        button->gpio = gpio;
        button->max_eval = ADV_BUTTON_DEFAULT_EVAL;
        button->inverted = inverted;
        button->mode = mode;
        
        if (gpio <= 16) {
            if (gpio != 0) {
                gpio_enable(gpio, GPIO_INPUT);
            }
            
            gpio_set_pullup(gpio, pullup_resistor, pullup_resistor);
        }
        
        if (mode == ADV_BUTTON_NORMAL_MODE) {
            vTaskDelay(pdMS_TO_TICKS(20));
            
            if (gpio <= 16) {
                button->state = gpio_read(gpio);
            } else {
                button->state = (sdk_system_adc_read() > ADC_MID_VALUE);
            }
            
            button->old_state = button->state;
            
            if (button->state) {
                button->value = ADV_BUTTON_DEFAULT_EVAL;
            }
            
            if (gpio < 16 && adv_button_main_config->continuos_mode == false) {
                gpio_set_interrupt(gpio, GPIO_INTTYPE_EDGE_ANY, adv_button_interrupt_normal);
            } else {
                adv_button_main_config->continuos_mode = true;
            }
            
        } else if (mode == ADV_BUTTON_PULSE_MODE) {
            adv_button_main_config->continuos_mode = true;

            gpio_set_interrupt(gpio, GPIO_INTTYPE_EDGE_NEG, adv_button_interrupt_pulse);
            
        } else {    // MCP23017
            adv_button_main_config->continuos_mode = true;
            
            const unsigned int index = gpio / 100;
            const unsigned int mcp_gpio = gpio % 100;
            adv_button_mcp_t* mcp = mcp_find_by_index(index);
            if (!mcp) {
                mcp = malloc(sizeof(adv_button_mcp_t));
                memset(mcp, 0, sizeof(*mcp));
                mcp->next = adv_button_main_config->mcps;
                adv_button_main_config->mcps = mcp;
                
                mcp->index = index;
                mcp->addr = mode;
                mcp->bus = pullup_resistor;
                
                if (mcp_gpio < 8) {
                    mcp->channels = MCP_CHANNEL_A;
                    const uint8_t reg = 0x12;
                    i2c_slave_read(mcp->bus, mcp->addr, &reg, 1, &mcp->value_a, 1);
                } else {
                    mcp->channels = MCP_CHANNEL_B;
                    const uint8_t reg = 0x13;
                    i2c_slave_read(mcp->bus, mcp->addr, &reg, 1, &mcp->value_b, 1);
                }
            } else if (mcp->channels != MCP_CHANNEL_BOTH) {
                if (mcp->channels == MCP_CHANNEL_A && mcp_gpio >= 8) {
                    mcp->channels = MCP_CHANNEL_BOTH;
                    const uint8_t reg = 0x13;
                    i2c_slave_read(mcp->bus, mcp->addr, &reg, 1, &mcp->value_b, 1);
                } else if (mcp->channels == MCP_CHANNEL_B && mcp_gpio < 8) {
                    mcp->channels = MCP_CHANNEL_BOTH;
                    const uint8_t reg = 0x12;
                    i2c_slave_read(mcp->bus, mcp->addr, &reg, 1, &mcp->value_a, 1);
                }
            }
            
            button->state = adv_button_read_mcp_gpio(gpio);
            button->old_state = button->state;
            
            if (button->state) {
                button->value = ADV_BUTTON_DEFAULT_EVAL;
            }
        }
        
        if (adv_button_main_config->continuos_mode) {
            esp_timer_start(adv_button_main_config->button_evaluate_timer);
            
            adv_button_t *button = adv_button_main_config->buttons;
            while (button) {
                if (button->mode == ADV_BUTTON_NORMAL_MODE) {
                    gpio_set_interrupt(button->gpio, GPIO_INTTYPE_NONE, NULL);
                }
                button = button->next;
            }
        }
        
        return 0;
    }

    return -1;
}

int adv_button_register_callback_fn(const uint16_t gpio, const button_callback_fn callback, const uint8_t button_callback_type, void *args, const uint8_t param) {
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
                if (!button->press_timer) {
                    button->press_timer = esp_timer_create(DOUBLEPRESS_TIME, false, (void*) button, adv_button_single_callback);
                }
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
                if (!button->hold_timer) {
                    button->hold_timer = esp_timer_create(HOLDPRESS_TIME, false, (void*) button, adv_button_hold_callback);
                }
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

/*
int adv_button_destroy(const uint16_t gpio) {
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
            
            esp_timer_delete(button->hold_timer);
            esp_timer_delete(button->press_timer);
            
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
            esp_timer_delete(adv_button_main_config->button_evaluate_timer);
            
            free(adv_button_main_config);
            adv_button_main_config = NULL;
            
        } else if (gpio == 16) {
            adv_button_t *button = adv_button_main_config->buttons;
            while (button) {
                if (button->mode == ADV_BUTTON_NORMAL_MODE) {
                    gpio_set_interrupt(button->gpio, GPIO_INTTYPE_EDGE_ANY, adv_button_interrupt_normal);
                }
                button = button->next;
            }
            
            esp_timer_stop(adv_button_main_config->button_evaluate_timer);
            adv_button_main_config->continuos_mode = false;
        }
        
        return 0;
    }
    
    return -1;
}
*/
