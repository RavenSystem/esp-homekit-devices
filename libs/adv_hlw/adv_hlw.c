/*
 * Advanced HLW8012 Driver
 *
 * Copyright 2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#include <string.h>
#include <espressif/esp_common.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <math.h>

#include "adv_hlw.h"

#define MODE_CURRENT            (0)
#define MODE_VOLTAGE            (1)
#define PERIOD_TIMEOUT          (1500000)

typedef struct _adv_hlw_unit {
    int8_t gpio_cf;
    int8_t gpio_cf1;
    int8_t gpio_sel;
    uint8_t current_mode: 1;
    uint8_t mode: 1;
    
    uint32_t last_cf;
    uint32_t period_cf;
    
    uint32_t first_cf1;
    uint32_t last_cf1;
    uint32_t period_cf1_v;
    uint32_t period_cf1_c;

    struct _adv_hlw_unit* next;
} adv_hlw_unit_t;

static adv_hlw_unit_t* adv_hlw_units = NULL;

IRAM static adv_hlw_unit_t* adv_hlw_find_by_gpio(const uint8_t gpio) {
    adv_hlw_unit_t* adv_hlw_unit = adv_hlw_units;
    while (adv_hlw_unit &&
           adv_hlw_unit->gpio_cf != gpio &&
           adv_hlw_unit->gpio_cf1 != gpio) {
        adv_hlw_unit = adv_hlw_unit->next;
    }
    
    return adv_hlw_unit;
}

static void normalize_cf1(adv_hlw_unit_t* adv_hlw_unit) {
    if ((sdk_system_get_time() - adv_hlw_unit->last_cf1) > PERIOD_TIMEOUT) {
        
        if (adv_hlw_unit->mode == adv_hlw_unit->current_mode) {
            adv_hlw_unit->period_cf1_c = 0;
        } else {
            adv_hlw_unit->period_cf1_v = 0;
        }
        
        if (adv_hlw_unit->gpio_sel >= 0) {
            uint8_t mode = MODE_CURRENT;
            
            if (adv_hlw_unit->mode == adv_hlw_unit->current_mode) {
                mode = MODE_VOLTAGE;
            }

            if (mode == MODE_CURRENT) {
                adv_hlw_unit->mode = adv_hlw_unit->current_mode;
            } else {
                adv_hlw_unit->mode = 1 - adv_hlw_unit->current_mode;
            }
            
            gpio_write(adv_hlw_unit->gpio_sel, mode);
        }
        
        adv_hlw_unit->last_cf1 = adv_hlw_unit->first_cf1;
    }
}

double adv_hlw_get_voltage_freq(const uint8_t gpio) {
    adv_hlw_unit_t* adv_hlw_unit = adv_hlw_find_by_gpio(gpio);
    
    if (adv_hlw_unit && adv_hlw_unit->period_cf1_v > 0) {
        normalize_cf1(adv_hlw_unit);

        return (double) 1000000 / adv_hlw_unit->period_cf1_v;
    }
    
    return 0;
}

double adv_hlw_get_current_freq(const uint8_t gpio) {
    adv_hlw_unit_t* adv_hlw_unit = adv_hlw_find_by_gpio(gpio);
    
    if (adv_hlw_unit && adv_hlw_unit->period_cf1_c > 0) {
        normalize_cf1(adv_hlw_unit);

        return (double) 1000000 / adv_hlw_unit->period_cf1_c;
    }
    
    return 0;
}

double adv_hlw_get_power_freq(const uint8_t gpio) {
    adv_hlw_unit_t* adv_hlw_unit = adv_hlw_find_by_gpio(gpio);
    
    if (adv_hlw_unit && adv_hlw_unit->period_cf > 0) {
        if ((sdk_system_get_time() - adv_hlw_unit->last_cf) > PERIOD_TIMEOUT) {
            adv_hlw_unit->period_cf = 0;
            return 0;
        }
        
        return (double) 1000000 / adv_hlw_unit->period_cf;
    }
    
    return 0;
}

IRAM static void adv_hlw_cf1_callback(const uint8_t gpio) {
    gpio_set_interrupt(gpio, GPIO_INTTYPE_NONE, adv_hlw_cf1_callback);
    
    adv_hlw_unit_t* adv_hlw_unit = adv_hlw_find_by_gpio(gpio);
    
    const uint32_t now = sdk_system_get_time();

    if ((now - adv_hlw_unit->first_cf1) > PERIOD_TIMEOUT) {
        uint32_t period;
        
        if (adv_hlw_unit->last_cf1 == adv_hlw_unit->first_cf1) {
            period = 0;
        } else {
            period = now - adv_hlw_unit->last_cf1;
        }

        if (adv_hlw_unit->mode == adv_hlw_unit->current_mode) {
            adv_hlw_unit->period_cf1_c = period;
        } else {
            adv_hlw_unit->period_cf1_v = period;
        }

        if (adv_hlw_unit->gpio_sel >= 0) {
            adv_hlw_unit->mode = 1 - adv_hlw_unit->mode;
            gpio_write(adv_hlw_unit->gpio_sel, adv_hlw_unit->mode);
        }
            
        adv_hlw_unit->first_cf1 = now;
    }

    adv_hlw_unit->last_cf1 = now;
    
    //printf("ADV_HLW_CF: gpio: %i, period_cf_v: %i _c: %i\n", gpio, adv_hlw_unit->period_cf1_v, adv_hlw_unit->period_cf1_c);
    
    gpio_set_interrupt(gpio, GPIO_INTTYPE_EDGE_NEG, adv_hlw_cf1_callback);
}

IRAM static void adv_hlw_cf_callback(const uint8_t gpio) {
    gpio_set_interrupt(gpio, GPIO_INTTYPE_NONE, adv_hlw_cf_callback);
    
    adv_hlw_unit_t* adv_hlw_unit = adv_hlw_find_by_gpio(gpio);
    
    const uint32_t now = sdk_system_get_time();
    adv_hlw_unit->period_cf = now - adv_hlw_unit->last_cf;
    adv_hlw_unit->last_cf = now;
    
    //printf("ADV_HLW_CF: gpio: %i, period_cf: %i\n", gpio, adv_hlw_unit->period_cf);
    
    gpio_set_interrupt(gpio, GPIO_INTTYPE_EDGE_NEG, adv_hlw_cf_callback);
}

int adv_hlw_unit_create(const int8_t gpio_cf, const int8_t gpio_cf1, const int8_t gpio_sel, const uint8_t current_mode) {
    adv_hlw_unit_t* adv_hlw_unit = adv_hlw_find_by_gpio(gpio_cf);
    
    if (!adv_hlw_unit) {
        adv_hlw_unit = adv_hlw_find_by_gpio(gpio_cf1);
        
        if (!adv_hlw_unit) {
            adv_hlw_unit = adv_hlw_find_by_gpio(gpio_sel);
        
            if (!adv_hlw_unit) {

                adv_hlw_unit = malloc(sizeof(adv_hlw_unit_t));
                if (adv_hlw_unit == NULL) {
                    return;
                }
                memset(adv_hlw_unit, 0, sizeof(*adv_hlw_unit));
                
                adv_hlw_unit->gpio_cf = gpio_cf;
                adv_hlw_unit->gpio_cf1 = gpio_cf1;
                adv_hlw_unit->gpio_sel = gpio_sel;
                adv_hlw_unit->current_mode = current_mode;
                adv_hlw_unit->mode = current_mode;
                
                adv_hlw_unit->next = adv_hlw_units;
                adv_hlw_units = adv_hlw_unit;
                
                if (adv_hlw_unit->gpio_cf >= 0) {
                    gpio_enable(adv_hlw_unit->gpio_cf, GPIO_INPUT);
                    gpio_set_pullup(adv_hlw_unit->gpio_cf, true, true);
                    gpio_set_interrupt(adv_hlw_unit->gpio_cf, GPIO_INTTYPE_EDGE_NEG, adv_hlw_cf_callback);
                }
                
                if (adv_hlw_unit->gpio_cf1 >= 0) {
                    gpio_enable(adv_hlw_unit->gpio_cf1, GPIO_INPUT);
                    gpio_set_pullup(adv_hlw_unit->gpio_cf1, true, true);
                    gpio_set_interrupt(adv_hlw_unit->gpio_cf1, GPIO_INTTYPE_EDGE_NEG, adv_hlw_cf1_callback);
                    
                    if (adv_hlw_unit->gpio_sel >= 0) {
                        gpio_enable(adv_hlw_unit->gpio_sel, GPIO_OUTPUT);
                        gpio_write(adv_hlw_unit->gpio_sel, adv_hlw_unit->current_mode);
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
