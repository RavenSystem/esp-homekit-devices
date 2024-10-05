/*
 * Advanced HLW8012 Driver
 *
 * Copyright 2020-2024 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#include <string.h>

#ifdef ESP_PLATFORM

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_attr.h"
#define IRAM                            IRAM_ATTR
#define sdk_system_get_time_raw()       ((uint32_t) esp_timer_get_time())
#define gpio_write(gpio, value)         gpio_set_level(gpio, value)

#else

#include <espressif/esp_common.h>
#include <esp8266.h>
#include <esplibs/libmain.h>
#include <FreeRTOS.h>
#include <task.h>

#endif

#include <math.h>

#include "adv_hlw.h"

#define MODE_CURRENT            (false)     // 0
#define MODE_VOLTAGE            (true)      // 1

#define ADV_HLW_FREQ_BASE_HZ    (1000000.f)
#define PERIOD_TIMEOUT_US       (1500000)

typedef struct _adv_hlw_unit {
    int8_t gpio_cf;
    int8_t gpio_cf1;
    int8_t gpio_sel;
    bool current_mode: 1;    // 1 bit
    bool mode: 1;            // 1 bit
    uint8_t _padding: 6;
    
    uint32_t last_cf;
    uint32_t period_cf;
    
    uint32_t first_cf1;
    uint32_t last_cf1;
    uint32_t period_cf1_v;
    uint32_t period_cf1_c;

    struct _adv_hlw_unit* next;
} adv_hlw_unit_t;

static adv_hlw_unit_t* adv_hlw_units = NULL;

static adv_hlw_unit_t* IRAM adv_hlw_find_by_gpio(const int gpio) {
    adv_hlw_unit_t* adv_hlw_unit = adv_hlw_units;
    while (adv_hlw_unit &&
           adv_hlw_unit->gpio_cf != gpio &&
           adv_hlw_unit->gpio_cf1 != gpio) {
        adv_hlw_unit = adv_hlw_unit->next;
    }
    
    return adv_hlw_unit;
}

static void IRAM change_mode(adv_hlw_unit_t* adv_hlw_unit) {
    if (adv_hlw_unit->gpio_sel >= 0) {
        adv_hlw_unit->mode = !adv_hlw_unit->mode;
        gpio_write(adv_hlw_unit->gpio_sel, adv_hlw_unit->mode);
    }
}

static void normalize_cf1(adv_hlw_unit_t* adv_hlw_unit) {
    const uint32_t last_cf1 = adv_hlw_unit->last_cf1;
    const uint32_t now = sdk_system_get_time_raw();
    const uint32_t diff = now - last_cf1;
    if (diff > PERIOD_TIMEOUT_US) {
        adv_hlw_unit->last_cf1 = now;
        adv_hlw_unit->first_cf1 = now;
        
        if (adv_hlw_unit->mode == adv_hlw_unit->current_mode) {
            adv_hlw_unit->period_cf1_c = 0;
        } else {
            adv_hlw_unit->period_cf1_v = 0;
        }
        
        change_mode(adv_hlw_unit);
    }
}

float adv_hlw_get_voltage_freq(const int gpio) {
    adv_hlw_unit_t* adv_hlw_unit = adv_hlw_find_by_gpio(gpio);
    
    if (adv_hlw_unit) {
        normalize_cf1(adv_hlw_unit);
        
        if (adv_hlw_unit->period_cf1_v > 0) {
            return (ADV_HLW_FREQ_BASE_HZ / (float) adv_hlw_unit->period_cf1_v);
        }
    }
    
    return 0;
}

float adv_hlw_get_current_freq(const int gpio) {
    adv_hlw_unit_t* adv_hlw_unit = adv_hlw_find_by_gpio(gpio);
    
    if (adv_hlw_unit) {
        normalize_cf1(adv_hlw_unit);
        
        if (adv_hlw_unit->period_cf1_c > 0) {
            return (ADV_HLW_FREQ_BASE_HZ / (float) adv_hlw_unit->period_cf1_c);
        }
    }
    
    return 0;
}

float adv_hlw_get_power_freq(const int gpio) {
    adv_hlw_unit_t* adv_hlw_unit = adv_hlw_find_by_gpio(gpio);
    if (adv_hlw_unit) {
        const uint32_t last_cf = adv_hlw_unit->last_cf;
        const uint32_t diff = sdk_system_get_time_raw() - last_cf;
        
        if (diff < PERIOD_TIMEOUT_US) {
            return (ADV_HLW_FREQ_BASE_HZ / (float) adv_hlw_unit->period_cf);
        }
    }
    
    return 0;
}


#ifdef ESP_PLATFORM
static void IRAM adv_hlw_cf1_callback(void* args) {
    const uint8_t gpio = (uint32_t) args;
#else
static void IRAM adv_hlw_cf1_callback(const uint8_t gpio) {
#endif
    const uint32_t now = sdk_system_get_time_raw();
    
    adv_hlw_unit_t* adv_hlw_unit = adv_hlw_find_by_gpio(gpio);
    
    if ((now - adv_hlw_unit->first_cf1) > PERIOD_TIMEOUT_US) {
        uint32_t new_period = 0;
        
        if (adv_hlw_unit->last_cf1 != adv_hlw_unit->first_cf1) {
            new_period = now - adv_hlw_unit->last_cf1;
        }
        
        if (adv_hlw_unit->mode == adv_hlw_unit->current_mode) {
            adv_hlw_unit->period_cf1_c = new_period;
        } else {
            adv_hlw_unit->period_cf1_v = new_period;
        }
        
        change_mode(adv_hlw_unit);
        
        adv_hlw_unit->first_cf1 = now;
    }
    
    adv_hlw_unit->last_cf1 = now;
    
    //printf("ADV_HLW_CF: gpio: %i, period_cf_v: %i _c: %i\n", gpio, adv_hlw_unit->period_cf1_v, adv_hlw_unit->period_cf1_c);
}

#ifdef ESP_PLATFORM
static void IRAM adv_hlw_cf_callback(void* args) {
    const uint8_t gpio = (uint32_t) args;
#else
static void IRAM adv_hlw_cf_callback(const uint8_t gpio) {
#endif
    const uint32_t now = sdk_system_get_time_raw();
    
    adv_hlw_unit_t* adv_hlw_unit = adv_hlw_find_by_gpio(gpio);
    
    adv_hlw_unit->period_cf = now - adv_hlw_unit->last_cf;
    adv_hlw_unit->last_cf = now;
    
    //printf("ADV_HLW_CF: gpio: %i, period_cf: %i\n", gpio, adv_hlw_unit->period_cf);
}

int adv_hlw_unit_create(const int gpio_cf, const int gpio_cf1, const int gpio_sel, const bool current_mode, const uint8_t interrupt_type) {
    adv_hlw_unit_t* adv_hlw_unit = adv_hlw_find_by_gpio(gpio_cf);
    
    if (!adv_hlw_unit) {
        adv_hlw_unit = adv_hlw_find_by_gpio(gpio_cf1);
        
        if (!adv_hlw_unit) {
            adv_hlw_unit = adv_hlw_find_by_gpio(gpio_sel);
            
            if (!adv_hlw_unit) {
#ifdef ESP_PLATFORM
                if (!adv_hlw_units) {
                    gpio_install_isr_service(0);
                }
#endif
                adv_hlw_unit = calloc(1, sizeof(adv_hlw_unit_t));
                
                adv_hlw_unit->current_mode = current_mode;
                adv_hlw_unit->mode = current_mode;
                
                adv_hlw_unit->next = adv_hlw_units;
                adv_hlw_units = adv_hlw_unit;
                
                adv_hlw_unit->gpio_cf = gpio_cf;
                adv_hlw_unit->gpio_cf1 = gpio_cf1;
                adv_hlw_unit->gpio_sel = gpio_sel;
                
                if (gpio_cf > -1) {
#ifdef ESP_PLATFORM
                    gpio_set_intr_type(gpio_cf, interrupt_type);
                    gpio_isr_handler_add(gpio_cf, adv_hlw_cf_callback, (void*) ((uint32_t) gpio_cf));
#else
                    gpio_set_interrupt(gpio_cf, interrupt_type, adv_hlw_cf_callback);
#endif
                }
                
                if (gpio_cf1 > -1) {
#ifdef ESP_PLATFORM
                    gpio_set_intr_type(gpio_cf1, interrupt_type);
                    gpio_isr_handler_add(gpio_cf1, adv_hlw_cf1_callback, (void*) ((uint32_t) gpio_cf1));
#else
                    gpio_set_interrupt(gpio_cf1, interrupt_type, adv_hlw_cf1_callback);
#endif
                    
                    if (gpio_sel > -1) {
                        gpio_write(gpio_sel, adv_hlw_unit->current_mode);
                    }
                }

                return 0;
            }
            
            return -3;
        }
        
        return -2;
    }

    return -1;
}
