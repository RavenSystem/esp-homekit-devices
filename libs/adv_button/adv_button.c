/*
 * Advanced Button Manager
 *
 * Copyright 2019-2023 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

/*
 * Based on Button library by Maxim Kulkin (@MaximKulkin), licensed under the MIT License.
 * https://github.com/maximkulkin/esp-homekit-demo/blob/master/examples/button/button.c
 */



#include <string.h>

#ifdef ESP_PLATFORM

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#define IRAM                        IRAM_ATTR

#define gpio_read(gpio)             gpio_get_level(gpio)
#define gpio_write(gpio, level)     gpio_set_level(gpio, level)

#else

#include <FreeRTOS.h>
#include <esplibs/libmain.h>
#include <espressif/esp_common.h>

#endif

#include <adv_i2c.h>
#include <timers_helper.h>
#include "adv_button.h"

#define ADV_BUTTON_DEFAULT_EVAL     (4)

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
#define ADV_BUTTON_MIN(x, y)        (((x) < (y)) ? (x) : (y))
#define ADV_BUTTON_MAX(x, y)        (((x) > (y)) ? (x) : (y))

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
    uint8_t press_count;    // 4 bits
    bool inverted;          // 1 bit
    bool state: 1;          // 1 bit
    bool old_state: 1;      // 1 bit

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
    uint8_t bus;
    uint8_t channels;
    
    uint8_t* value;
    
    struct _adv_button_mcp* next;
} adv_button_mcp_t;

typedef struct _adv_button_main_config {
    uint32_t disable_time;
    
    uint16_t button_evaluate_sleep_countdown;
    uint16_t button_evaluate_sleep_time;
    
    uint8_t button_evaluate_delay;
    bool continuos_mode;    // 1 bit
    
    TimerHandle_t button_evaluate_timer;

    adv_button_t* buttons;
    adv_button_mcp_t* mcps;
} adv_button_main_config_t;

static adv_button_main_config_t* adv_button_main_config = NULL;

static adv_button_t* IRAM button_find_by_gpio(const uint16_t gpio) {
    if (adv_button_main_config) {
        adv_button_t* button = adv_button_main_config->buttons;
        
        while (button && button->gpio != gpio) {
            button = button->next;
        }
        
        return button;
    }
    
    return NULL;
}

static adv_button_mcp_t* mcp_find_by_index(const uint8_t index) {
    if (adv_button_main_config) {
        adv_button_mcp_t* adv_button_mcp = adv_button_main_config->mcps;
        
        while (adv_button_mcp && adv_button_mcp->index != index) {
            adv_button_mcp = adv_button_mcp->next;
        }
        
        return adv_button_mcp;
    }
    
    return NULL;
}

static void adv_button_read_mcp_channels(adv_button_mcp_t* adv_button_mcp) {
    if (adv_button_mcp->channels != MCP_CHANNEL_B) {
        const uint8_t reg = 0x12;
        adv_i2c_slave_read(adv_button_mcp->bus, adv_button_mcp->addr, &reg, 1, &adv_button_mcp->value[0], 1);
    }
    
    if (adv_button_mcp->channels != MCP_CHANNEL_A) {
        const uint8_t reg = 0x13;
        adv_i2c_slave_read(adv_button_mcp->bus, adv_button_mcp->addr, &reg, 1, &adv_button_mcp->value[1], 1);
    }
}

static void adv_button_read_shift_register(adv_button_mcp_t* adv_button_mcp) {
    const uint8_t clock_gpio = adv_button_mcp->bus - 100;
    
    unsigned int bit_shift = 0;
    unsigned int out_group = 0;
    
    adv_button_mcp->value[0] = 0;
    
    for (unsigned int i = 0; i < adv_button_mcp->channels; i++) {
        adv_button_mcp->value[out_group] |= gpio_read(adv_button_mcp->addr) << bit_shift;
        
        gpio_write(clock_gpio, true);
        
        bit_shift++;
        if (bit_shift == 8) {
            bit_shift = 0;
            out_group++;
            
            adv_button_mcp->value[out_group] = 0;
        }
        
        gpio_write(clock_gpio, false);
    }
}

int adv_button_read_by_gpio(const uint16_t gpio) {
    unsigned int result = false;
    
    adv_button_t* button = button_find_by_gpio(gpio);
    if (button) {
        result = (bool) (button->state ^ button->inverted);
    }
    
    return result;
}

static bool adv_button_read_mcp_gpio(const uint16_t gpio) {
    adv_button_mcp_t* adv_button_mcp = mcp_find_by_index(gpio / 100);
    if (adv_button_mcp) {
        const unsigned int mcp_gpio = gpio % 100;
        const unsigned int bit_shift = mcp_gpio % 8;
        const unsigned int out_group = mcp_gpio / 8;
        
        return (bool) (adv_button_mcp->value[out_group] & (1 << bit_shift));
    }
    
    return false;
}

static void adv_button_run_callback_fn(adv_button_callback_fn_t* callbacks, const uint16_t gpio) {
    adv_button_callback_fn_t* adv_button_callback_fn = callbacks;
    
    while (adv_button_callback_fn) {
        adv_button_callback_fn->callback(gpio, adv_button_callback_fn->args, adv_button_callback_fn->param);
        adv_button_callback_fn = adv_button_callback_fn->next;
    }
}

static inline void push_down(const uint16_t used_gpio) {
    const uint32_t now = xTaskGetTickCount();
    
    if ((now - adv_button_main_config->disable_time) > (DISABLE_TIME / portTICK_PERIOD_MS)) {
        adv_button_t *button = button_find_by_gpio(used_gpio);
        if (button->singlepress0_callback_fn) {
            adv_button_run_callback_fn(button->singlepress0_callback_fn, button->gpio);
        }

        rs_esp_timer_start(button->hold_timer);

        button->last_event_time = now;
    }
}

static inline void push_up(const uint16_t used_gpio) {
    const uint32_t now = xTaskGetTickCount();
    
    if ((now - adv_button_main_config->disable_time) > (DISABLE_TIME / portTICK_PERIOD_MS)) {
        adv_button_t *button = button_find_by_gpio(used_gpio);
        
        if (button->press_count == DISABLE_PRESS_COUNT) {
            button->press_count = 0;
            return;
        }
        
        rs_esp_timer_stop(button->hold_timer);

        if ((now - button->last_event_time) > (VERYLONGPRESS_TIME / portTICK_PERIOD_MS)) {
            // Very Long button pressed
            button->press_count = 0;
            if (button->verylongpress_callback_fn) {
                adv_button_run_callback_fn(button->verylongpress_callback_fn, button->gpio);
            } else if (button->longpress_callback_fn) {
                adv_button_run_callback_fn(button->longpress_callback_fn, button->gpio);
            } else {
                adv_button_run_callback_fn(button->singlepress_callback_fn, button->gpio);
            }
        } else if ((now - button->last_event_time) > (LONGPRESS_TIME / portTICK_PERIOD_MS)) {
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
                rs_esp_timer_stop(button->press_timer);
                button->press_count = 0;
                adv_button_run_callback_fn(button->doublepress_callback_fn, button->gpio);
            } else {
                rs_esp_timer_start(button->press_timer);
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

#ifdef ESP_PLATFORM
static void IRAM_ATTR adv_button_interrupt_pulse(void* args) {
    const uint8_t gpio = (uint32_t) args;
#else
static void IRAM adv_button_interrupt_pulse(const uint8_t gpio) {
#endif

    adv_button_t *button = button_find_by_gpio(gpio);
    button->value = ADV_BUTTON_MIN(button->value++, button->max_eval);
}

#ifdef ESP_PLATFORM
static void IRAM_ATTR adv_button_interrupt_normal(void* args) {
    const uint8_t gpio = (uint32_t) args;
#else
static void IRAM adv_button_interrupt_normal(const uint8_t gpio) {
#endif
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xTimerStartFromISR(adv_button_main_config->button_evaluate_timer, &xHigherPriorityTaskWoken) == pdPASS) {
        adv_button_t* button = adv_button_main_config->buttons;
        while (button) {
#ifdef ESP_PLATFORM
            gpio_intr_disable(gpio);
#else
            gpio_set_interrupt(button->gpio, GPIO_INTTYPE_NONE, NULL);
#endif
            button = button->next;
        }
    }
    
#ifdef ESP_PLATFORM
    if (xHigherPriorityTaskWoken != pdFALSE) {
        portYIELD_FROM_ISR();
    }
#else
    portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
#endif
}

static void button_evaluate_fn() {
    if (!adv_button_main_config->continuos_mode) {
        if (adv_button_main_config->button_evaluate_sleep_countdown < adv_button_main_config->button_evaluate_sleep_time) {
            adv_button_main_config->button_evaluate_sleep_countdown++;
            
        } else if (rs_esp_timer_stop(adv_button_main_config->button_evaluate_timer) == pdPASS) {
            adv_button_main_config->button_evaluate_sleep_countdown = 0;
            adv_button_t* button = adv_button_main_config->buttons;
            while (button) {
#ifdef ESP_PLATFORM
                gpio_intr_enable(button->gpio);
#else
                gpio_set_interrupt(button->gpio, GPIO_INTTYPE_EDGE_ANY, adv_button_interrupt_normal);
#endif
                button = button->next;
            }
        }
    }
    
    adv_button_mcp_t* adv_button_mcp = adv_button_main_config->mcps;
    while (adv_button_mcp) {
        if (adv_button_mcp->bus < 100) {    // MCP23017
            adv_button_read_mcp_channels(adv_button_mcp);
        } else {    // Shift Register
            adv_button_read_shift_register(adv_button_mcp);
        }
        
        adv_button_mcp = adv_button_mcp->next;
    }
    
    adv_button_t* button = adv_button_main_config->buttons;
    while (button) {
        if (button->mode != ADV_BUTTON_PULSE_MODE) {
            unsigned int read_value;
            
            if (button->mode == ADV_BUTTON_NORMAL_MODE) {
#ifdef ESP_PLATFORM
                read_value = gpio_read(button->gpio);
#else
                if (button->gpio <= 16) {
                    read_value = gpio_read(button->gpio);
                } else {    // gpio == 17   (ESP8266 ADC pin)
                    read_value = (sdk_system_adc_read() > ADC_MID_VALUE);
                }
#endif
            } else {    // MCP23017
                read_value = adv_button_read_mcp_gpio(button->gpio);
            }
            
            if (read_value) {
                if (button->state) {
                    button->value = button->max_eval;
                } else {
                    button->value = ADV_BUTTON_MIN(button->value++, button->max_eval);
                    if (button->value == button->max_eval) {
                        button->state = true;
                    }
                }
            } else {
                if (!button->state) {
                    button->value = 0;
                } else {
                    button->value = ADV_BUTTON_MAX(button->value--, 0);
                    if (button->value == 0) {
                        button->state = false;
                    }
                }
            }
            
        } else {    // button->mode == ADV_BUTTON_PULSE_MODE
            if (button->value == button->max_eval) {
                button->value = button->value >> 1;
                button->state = true;
            } else {
                button->value = ADV_BUTTON_MAX(button->value--, 0);
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
}

void adv_button_init(const uint16_t new_delay_ms, const bool continuos_mode) {
    if (!adv_button_main_config) {
        adv_button_main_config = malloc(sizeof(adv_button_main_config_t));
        memset(adv_button_main_config, 0, sizeof(*adv_button_main_config));
        
        unsigned int new_delay = BUTTON_EVAL_DELAY_MIN;
        
        if (new_delay_ms > 0) {
            new_delay = new_delay_ms / portTICK_PERIOD_MS;
        }
        
        if (new_delay < BUTTON_EVAL_DELAY_MIN) {
            adv_button_main_config->button_evaluate_delay = BUTTON_EVAL_DELAY_MIN;
        } else {
            adv_button_main_config->button_evaluate_delay = new_delay;
        }
        
        adv_button_main_config->button_evaluate_sleep_time = ((HOLDPRESS_TIME + 1000) / portTICK_PERIOD_MS) / adv_button_main_config->button_evaluate_delay;
        adv_button_main_config->button_evaluate_timer = rs_esp_timer_create(adv_button_main_config->button_evaluate_delay * portTICK_PERIOD_MS, pdTRUE, NULL, button_evaluate_fn);
        
        adv_button_main_config->continuos_mode = continuos_mode;
        
        if (continuos_mode) {
            rs_esp_timer_start(adv_button_main_config->button_evaluate_timer);
#ifdef ESP_PLATFORM
        } else {
            gpio_install_isr_service(0);
#endif
        }
    }
}

void adv_button_set_disable_time() {
    if (adv_button_main_config) {
        adv_button_main_config->disable_time = xTaskGetTickCount();
    }
}

static adv_button_mcp_t* private_adv_button_new_mcp_data(const uint8_t index, const uint8_t mode, const uint8_t mcp_bus, uint8_t len) {
    adv_button_mcp_t* adv_button_mcp = malloc(sizeof(adv_button_mcp_t));
    memset(adv_button_mcp, 0, sizeof(*adv_button_mcp));
    
    adv_button_mcp->next = adv_button_main_config->mcps;
    adv_button_main_config->mcps = adv_button_mcp;
    
    adv_button_mcp->index = index;
    adv_button_mcp->addr = mode;
    adv_button_mcp->bus = mcp_bus;
    
    if (len > 0) {
        adv_button_mcp->channels = len;
    } else {
        len = 2;
    }
    
    adv_button_mcp->value = malloc(sizeof(uint8_t) * len);
    
    return adv_button_mcp;
}

void adv_button_create_shift_register(const uint8_t index, const uint8_t mode, const uint8_t mcp_bus, const uint8_t len) {
    private_adv_button_new_mcp_data(index, mode, mcp_bus, len);
}

int adv_button_create(const uint16_t gpio, const bool inverted, const uint8_t mode, const uint8_t mcp_bus, const uint8_t max_eval) {
    if (!adv_button_main_config) {
        adv_button_init(0, ADV_BUTTON_CONTINUOS_MODE_OFF);
    }
    
    adv_button_t* button = button_find_by_gpio(gpio);
    
    if (!button) {
        button = malloc(sizeof(adv_button_t));
        memset(button, 0, sizeof(*button));
        button->next = adv_button_main_config->buttons;
        adv_button_main_config->buttons = button;
        
        button->gpio = gpio;
        button->inverted = inverted;
        button->mode = mode;
        
        if (max_eval == 0) {
            button->max_eval = ADV_BUTTON_DEFAULT_EVAL;
        } else {
            button->max_eval = max_eval;
        }
        
        /*
        if (gpio <= 16) {
            if (gpio != 0) {
                gpio_enable(gpio, GPIO_INPUT);
            }
            
            gpio_set_pullup(gpio, pullup_resistor, pullup_resistor);
        }
        */
        
#ifdef ESP_PLATFORM
        int ret = 0;
#endif
        
        if (mode == ADV_BUTTON_NORMAL_MODE) {
            //vTaskDelay(1);
            
#ifdef ESP_PLATFORM
            button->state = gpio_read(gpio);
#else
            if (gpio <= 16) {
                button->state = gpio_read(gpio);
            } else {    // gpio == 17   (ESP8266 ADC pin)
                button->state = (sdk_system_adc_read() > ADC_MID_VALUE);
            }
#endif
            
            button->old_state = button->state;
            
            if (button->state) {
                button->value = button->max_eval;
            }
            
            if (!adv_button_main_config->continuos_mode) {
                
#ifdef ESP_PLATFORM
                gpio_set_intr_type(gpio, GPIO_INTR_ANYEDGE);
                ret = gpio_isr_handler_add(gpio, adv_button_interrupt_normal, (void*) ((uint32_t) gpio));
#else
                gpio_set_interrupt(gpio, GPIO_INTTYPE_EDGE_ANY, adv_button_interrupt_normal);
#endif
                
            }
            
        } else if (mode == ADV_BUTTON_PULSE_MODE) {
            
#ifdef ESP_PLATFORM
            gpio_set_intr_type(gpio, GPIO_INTR_NEGEDGE);
            ret = gpio_isr_handler_add(gpio, adv_button_interrupt_pulse, (void*) ((uint32_t) gpio));
#else
            gpio_set_interrupt(gpio, GPIO_INTTYPE_EDGE_NEG, adv_button_interrupt_pulse);
#endif
            
        } else {    // MCP23017
            unsigned int index = gpio / 100;
            unsigned int mcp_gpio = gpio % 100;
            
            adv_button_mcp_t* adv_button_mcp = mcp_find_by_index(index);
            if (!adv_button_mcp) {
                adv_button_mcp = private_adv_button_new_mcp_data(index, mode, mcp_bus, 0);
                
                if (mcp_gpio < 8) {
                    adv_button_mcp->channels = MCP_CHANNEL_A;
                } else {
                    adv_button_mcp->channels = MCP_CHANNEL_B;
                }
                
                adv_button_read_mcp_channels(adv_button_mcp);
                
            } else if (adv_button_mcp->bus < 100) {
                if ((adv_button_mcp->channels == MCP_CHANNEL_A && mcp_gpio >= 8) || (adv_button_mcp->channels == MCP_CHANNEL_B && mcp_gpio < 8)) {
                    adv_button_mcp->channels = MCP_CHANNEL_BOTH;
                    adv_button_read_mcp_channels(adv_button_mcp);
                }
                
            } else {    // Shift Register
                adv_button_read_shift_register(adv_button_mcp);
            }
            
            button->state = adv_button_read_mcp_gpio(gpio);
            button->old_state = button->state;
            
            if (button->state) {
                button->value = button->max_eval;
            }
        }
        
#ifdef ESP_PLATFORM
        return ret;
#else
        return 0;
#endif
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
                    button->press_timer = rs_esp_timer_create(DOUBLEPRESS_TIME, pdFALSE, (void*) button, adv_button_single_callback);
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
                    button->hold_timer = rs_esp_timer_create(HOLDPRESS_TIME, pdFALSE, (void*) button, adv_button_hold_callback);
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
            
            rs_esp_timer_delete(button->hold_timer);
            rs_esp_timer_delete(button->press_timer);
            
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
            rs_esp_timer_delete(adv_button_main_config->button_evaluate_timer);
            
            free(adv_button_main_config);
            adv_button_main_config = NULL;
 
 #ifdef ESP_PLATFORM
            gpio_uninstall_isr_service();
 #endif
            
        } else if (gpio == 16) {
            adv_button_t *button = adv_button_main_config->buttons;
            while (button) {
                if (button->mode == ADV_BUTTON_NORMAL_MODE) {
                    gpio_set_interrupt(button->gpio, GPIO_INTTYPE_EDGE_ANY, adv_button_interrupt_normal);
                }
                button = button->next;
            }
            
            rs_esp_timer_stop(adv_button_main_config->button_evaluate_timer);
            adv_button_main_config->continuos_mode = false;
        }
        
        return 0;
    }
    
    return -1;
}
*/
