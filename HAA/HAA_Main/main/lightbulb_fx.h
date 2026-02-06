/*
 * Lightbulb FX for Home Accessory Architect
 *
 * Copyright 2025-2026 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#include <stdint.h>

#ifdef ESP_PLATFORM
#include <stdbool.h>
#endif

typedef struct _lightbulb_fx_data_t {
    uint8_t effect;
    uint8_t channels;       // 3 bits
    uint16_t leds_array_size;
    
    uint16_t speed;
    uint8_t options;
    uint8_t aux_param;   // auxilary param (usually stores a color_wheel index)
    
    uint32_t counter_mode_step;
    uint32_t counter_mode_call;
    
    uint32_t next_time;
    
    uint8_t *leds_array;
    
    uint32_t colors[3];
    
    uint16_t aux_param3;  // auxilary param (usually stores a segment index)
    uint8_t last_effect;
} lightbulb_fx_data_t;

lightbulb_fx_data_t* new_lightbulb_fx_data(uint16_t size, uint16_t channels);
bool set_lightbulb_fx_effect(lightbulb_fx_data_t* lightbulb_fx_data);
uint32_t get_lightbulb_fx_effect_now_ms();
void set_fx_speed(lightbulb_fx_data_t* lightbulb_fx_data, const uint8_t new_speed);
void set_fx_reverse(lightbulb_fx_data_t* lightbulb_fx_data, const bool is_reverse);
void set_fx_size(lightbulb_fx_data_t* lightbulb_fx_data, const uint8_t new_size);
