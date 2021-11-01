/*
* Advanced NRZ LED Driver
*
* Copyright 2021 José Antonio Jiménez Campos (@RavenSystem)
*
*/

#include <esplibs/libmain.h>

#include "adv_nrzled.h"

static inline IRAM uint32_t get_cycle_count() {
    uint32_t cycles;
    asm volatile("rsr %0,ccount" : "=a" (cycles));
    return cycles;
}

void IRAM nrzled_set(const uint8_t gpio, const uint16_t time_0, const uint16_t time_1, const uint16_t period, uint8_t *colors, const uint16_t size) {
    uint32_t c, t;
    uint32_t start = 0;
    
    for (uint16_t i = 0; i < size; i++) {
        uint8_t color = *colors++;
        
        for (int8_t p = 7; p >= 0; p--) {
            t = time_0;
            if (color & (1 << p)) {
                t = time_1;
            }
            
            while (((c = get_cycle_count()) - start) < period);
            gpio_write(gpio, true);
            
            start = c;
            while (((c = get_cycle_count()) - start) < t);
            gpio_write(gpio, false);
        }
    }
}
