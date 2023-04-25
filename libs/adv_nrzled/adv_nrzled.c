/*
* Advanced NRZ LED Driver
*
* Copyright 2021-2023 José Antonio Jiménez Campos (@RavenSystem)
*
*/

#include <esplibs/libmain.h>

#include "adv_nrzled.h"

#define ADV_NRZ_CPU_FREQ_MHZ                (80)
#define ADV_NRZ_NRZ_TICKS_OFFSET            (4)

static inline IRAM uint32_t get_cycle_count() {
    uint32_t cycles;
    asm volatile("rsr %0,ccount" : "=a" (cycles));
    return cycles;
}

uint32_t nrz_ticks(const float time_us) {
    uint32_t cycles = time_us * ADV_NRZ_CPU_FREQ_MHZ;
    
    if (cycles < ADV_NRZ_NRZ_TICKS_OFFSET) {
        cycles = ADV_NRZ_NRZ_TICKS_OFFSET;
    }
    
    return cycles - ADV_NRZ_NRZ_TICKS_OFFSET;
}

void IRAM nrzled_set(const uint8_t gpio, const uint16_t ticks_0, const uint16_t ticks_1, const uint16_t period, uint8_t *colors, const uint16_t size) {
    uint32_t c, t;
    uint32_t start = 0;
    
    uint8_t color;
    
    for (int i = 0; i < size; i++) {
        color = colors[i];
        
        for (int p = 7; p >= 0; p--) {
            t = ticks_0;
            if (color & (1 << p)) {
                t = ticks_1;
            }
            
            while (((c = get_cycle_count()) - start) < period);
            gpio_write(gpio, 1);
            
            start = c;
            while (((c = get_cycle_count()) - start) < t);
            gpio_write(gpio, 0);
        }
    }
}
