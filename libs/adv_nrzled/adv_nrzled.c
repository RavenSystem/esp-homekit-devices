/*
* Advanced NRZ LED Driver
*
* Copyright 2021-2022 José Antonio Jiménez Campos (@RavenSystem)
*
*/

#include <esplibs/libmain.h>

#include "adv_nrzled.h"

#define CPU_FREQ_MHZ                        (80)
#define NRZ_TICKS_OFFSET                    (4)

int nrz_ticks(const float time_us) {
    int cycles = CPU_FREQ_MHZ * time_us;
    
    if (cycles < NRZ_TICKS_OFFSET) {
        cycles = NRZ_TICKS_OFFSET;
    }
    
    return cycles - NRZ_TICKS_OFFSET;
}

static inline IRAM uint32_t get_cycle_count() {
    uint32_t cycles;
    asm volatile("rsr %0,ccount" : "=a" (cycles));
    return cycles;
}

void IRAM nrzled_set(const uint8_t gpio, const uint16_t ticks_0, const uint16_t ticks_1, const uint16_t period, uint8_t *colors, const uint16_t size) {
    uint32_t c, t;
    uint32_t start = 0;
    uint8_t color = colors[1];
    
    for (int i = 0; i < size; i++) {
        color = colors[i];
        
        for (int p = 7; p >= 0; p--) {
            t = ticks_0;
            if (color & (1 << p)) {
                t = ticks_1;
            }
            
            while (((c = get_cycle_count()) - start) < period);
            gpio_write(gpio, true);
            
            start = c;
            while (((c = get_cycle_count()) - start) < t);
            gpio_write(gpio, false);
        }
    }
}
