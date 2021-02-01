/*
 * Advanced PWM Driver
 *
 * Copyright 2021 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#include <string.h>
#include <espressif/esp_common.h>
#include <espressif/sdk_private.h>
#include <esp8266.h>
#include <FreeRTOS.h>

#include "adv_pwm.h"

#define ADV_PWM_FREQUENCY_DEFAULT           (305)

typedef struct _adv_pwm_channel {
    uint16_t duty[8];
    uint8_t gpio: 5;
    bool inverted: 1;

    struct _adv_pwm_channel* next;
} adv_pwm_channel_t;

typedef struct _adv_pwm_config {
    uint16_t current_duty;
    uint8_t cycle: 3;
    bool is_running: 1;

    uint32_t max_load;
    
    adv_pwm_channel_t* adv_pwm_channels;
} adv_pwm_config_t;

static adv_pwm_config_t* adv_pwm_config = NULL;

static adv_pwm_channel_t* adv_pwm_channel_find_by_gpio(const uint8_t gpio) {
    if (adv_pwm_config) {
        adv_pwm_channel_t* adv_pwm_channel = adv_pwm_config->adv_pwm_channels;
        
        while (adv_pwm_channel && adv_pwm_channel->gpio != gpio) {
            adv_pwm_channel = adv_pwm_channel->next;
        }

        return adv_pwm_channel;
    }
    
    return NULL;
}

uint16_t adv_pwm_get_duty(const uint8_t gpio) {
    if (adv_pwm_config) {
        adv_pwm_channel_t* adv_pwm_channel = adv_pwm_config->adv_pwm_channels;
        
        while (adv_pwm_channel && adv_pwm_channel->gpio != gpio) {
            adv_pwm_channel = adv_pwm_channel->next;
        }

        if (adv_pwm_channel) {
            return (adv_pwm_channel->duty[0] + adv_pwm_channel->duty[1] + adv_pwm_channel->duty[2] + adv_pwm_channel->duty[3]) >> 2;
        }
    }
    
    return 0;
}

IRAM static void adv_pwm_worker() {
    uint32_t next_load = adv_pwm_config->max_load;
    uint16_t next_duty = UINT16_MAX;
    
    adv_pwm_channel_t* adv_pwm_channel = adv_pwm_config->adv_pwm_channels;
    if (adv_pwm_config->current_duty == 0) {
        adv_pwm_config->cycle++;
        
        while (adv_pwm_channel) {
            if (adv_pwm_channel->duty[adv_pwm_config->cycle] == 0) {
                gpio_write(adv_pwm_channel->gpio, adv_pwm_channel->inverted);
            } else {
                gpio_write(adv_pwm_channel->gpio, 1 ^ adv_pwm_channel->inverted);
            }
            
            adv_pwm_channel = adv_pwm_channel->next;
        }
        
        adv_pwm_channel = adv_pwm_config->adv_pwm_channels;
    }
    
    while (adv_pwm_channel) {
        if (adv_pwm_channel->duty[adv_pwm_config->cycle] <= adv_pwm_config->current_duty) {
            gpio_write(adv_pwm_channel->gpio, adv_pwm_channel->inverted);
        } else if (adv_pwm_channel->duty[adv_pwm_config->cycle] > adv_pwm_config->current_duty && adv_pwm_channel->duty[adv_pwm_config->cycle] < next_duty) {
            next_duty = adv_pwm_channel->duty[adv_pwm_config->cycle];
        }
        
        adv_pwm_channel = adv_pwm_channel->next;
    }

    next_load = (next_duty - adv_pwm_config->current_duty) * adv_pwm_config->max_load / UINT16_MAX;
    if (!next_load) {
        next_load = 1;
    }
    
    if (next_duty == UINT16_MAX) {
        adv_pwm_config->current_duty = 0;
    } else {
        adv_pwm_config->current_duty = next_duty;
    }
    
    timer_set_load(FRC1, next_load);
}

void adv_pwm_start() {
    if (!adv_pwm_config->is_running) {
        adv_pwm_config->is_running = true;
        
        timer_set_load(FRC1, adv_pwm_config->max_load);
        timer_set_reload(FRC1, false);
        timer_set_interrupts(FRC1, true);
        timer_set_run(FRC1, true);
        
        adv_pwm_worker();
    }
}

void adv_pwm_stop() {
    if (adv_pwm_config->is_running) {
        timer_set_interrupts(FRC1, false);
        timer_set_run(FRC1, false);
        
        adv_pwm_config->current_duty = 0;
        adv_pwm_config->cycle = 0;
        
        adv_pwm_channel_t* adv_pwm_channel = adv_pwm_config->adv_pwm_channels;
        while (adv_pwm_channel) {
            gpio_write(adv_pwm_channel->gpio, adv_pwm_channel->inverted);
        }
    }
}

static void adv_pwm_init(const uint8_t mode) {
    if (!adv_pwm_config) {
        adv_pwm_config = malloc(sizeof(adv_pwm_config_t));
        memset(adv_pwm_config, 0, sizeof(*adv_pwm_config));

        _xt_isr_attach(INUM_TIMER_FRC1, adv_pwm_worker, NULL);
        
        if (mode == 0) {
            adv_pwm_set_freq(ADV_PWM_FREQUENCY_DEFAULT);
        }
    }
}

void adv_pwm_set_freq(const uint16_t freq) {
    adv_pwm_init(1);
    
    if (adv_pwm_config->is_running) {
        adv_pwm_stop();
    }
    
    timer_set_frequency(FRC1, freq);
    adv_pwm_config->max_load = timer_get_load(FRC1);
    
    if (adv_pwm_config->is_running) {
        adv_pwm_start();
    }
}

void adv_pwm_set_duty(const uint8_t gpio, const uint16_t duty, uint16_t dithering) {
    adv_pwm_channel_t* adv_pwm_channel = adv_pwm_channel_find_by_gpio(gpio);
    if (adv_pwm_channel) {
        if (dithering == 0 || duty == 0 || duty == UINT16_MAX) {
            for (uint8_t i = 0; i < 8; i++) {
                adv_pwm_channel->duty[i] = duty;
            }
        } else {
            if (duty >= (UINT16_MAX - dithering)) {
                dithering = UINT16_MAX - duty;
            } else if (duty <= dithering) {
                dithering = 0;
            } else {
                dithering = dithering * duty / UINT16_MAX;
            }

            /*
            const uint16_t dithering_sin67 = dithering * 0.9238f;
            const uint16_t dithering_sin45 = dithering * 0.7071f;
            const uint16_t dithering_sin22 = dithering * 0.3826f;
            
            adv_pwm_channel->duty[0] = duty + dithering;
            adv_pwm_channel->duty[1] = duty + dithering_sin67;
            adv_pwm_channel->duty[2] = duty + dithering_sin45;
            adv_pwm_channel->duty[3] = duty + dithering_sin22;
            adv_pwm_channel->duty[4] = duty;
            adv_pwm_channel->duty[5] = duty - dithering_sin22;
            adv_pwm_channel->duty[6] = duty - dithering_sin45;
            adv_pwm_channel->duty[7] = duty - dithering_sin67;
            adv_pwm_channel->duty[8] = duty - dithering;
            
            for (uint8_t i = 9; i < 16; i++) {
                adv_pwm_channel->duty[i] = adv_pwm_channel->duty[16 - i];
            }
            */
            
            const uint16_t dithering_sin45 = dithering * 0.7071f;
            adv_pwm_channel->duty[0] = duty + dithering;
            adv_pwm_channel->duty[1] = duty + dithering_sin45;
            adv_pwm_channel->duty[2] = duty;
            adv_pwm_channel->duty[3] = duty - dithering_sin45;
            adv_pwm_channel->duty[4] = duty - dithering;
            
            for (uint8_t i = 5; i < 8; i++) {
                adv_pwm_channel->duty[i] = adv_pwm_channel->duty[8 - i];
            }
        }
    }
}

void adv_pwm_new_channel(const uint8_t gpio, const bool inverted) {
    adv_pwm_init(0);
    
    if (!adv_pwm_channel_find_by_gpio(gpio)) {
        if (adv_pwm_config->is_running) {
            adv_pwm_stop();
        }
        
        adv_pwm_channel_t* adv_pwm_channel = malloc(sizeof(adv_pwm_channel_t));
        memset(adv_pwm_channel, 0, sizeof(*adv_pwm_channel));
        
        gpio_enable(gpio, GPIO_OUTPUT);
        
        adv_pwm_channel->gpio = gpio;
        adv_pwm_channel->inverted = inverted;
        
        adv_pwm_channel->next = adv_pwm_config->adv_pwm_channels;
        adv_pwm_config->adv_pwm_channels = adv_pwm_channel;
        
        if (adv_pwm_config->is_running) {
            adv_pwm_start();
        }
    }
}

