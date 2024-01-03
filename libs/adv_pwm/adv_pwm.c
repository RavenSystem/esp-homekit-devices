/*
 * Advanced PWM Driver
 *
 * Copyright 2021-2024 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#include <string.h>

#ifdef ESP_PLATFORM

#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_attr.h"

#define gpio_write(gpio, value)             gpio_set_level(gpio, value)

#define ADV_PWM_TIMER_RESOLUTION            (8 * 1000 * 1000)  // 8MHz

#else

#include <espressif/esp_common.h>
#include <espressif/sdk_private.h>
#include <esp8266.h>
#include <FreeRTOS.h>

#endif

#include "adv_pwm.h"

#define ADV_PWM_FREQUENCY_DEFAULT           (305)

typedef struct _adv_pwm_channel {
    uint16_t duty[8];
    
    uint16_t dithering;
    uint8_t gpio: 6;    // 5 bits
    bool inverted: 1;
    bool leading: 1;
    uint8_t _align;
    
    struct _adv_pwm_channel* next;
} adv_pwm_channel_t;

typedef struct _adv_pwm_config {
    uint16_t current_duty;
    uint8_t cycle: 3;
    bool is_running: 1;
    uint8_t zc_status: 4;   // 2 bits
    uint8_t _align;
    
    uint32_t max_load;
    
#ifdef ESP_PLATFORM
    gptimer_handle_t gptimer;
#endif
    
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

int adv_pwm_get_dithering(const uint8_t gpio) {
    adv_pwm_channel_t* adv_pwm_channel = adv_pwm_channel_find_by_gpio(gpio);
    if (adv_pwm_channel) {
        return adv_pwm_channel->dithering;
    }
    
    return -1;
}

int adv_pwm_get_duty(const uint8_t gpio) {
    adv_pwm_channel_t* adv_pwm_channel = adv_pwm_channel_find_by_gpio(gpio);
    if (adv_pwm_channel) {
        return ((UINT16_MAX - adv_pwm_channel->duty[0]) + (UINT16_MAX - adv_pwm_channel->duty[1]) + (UINT16_MAX - adv_pwm_channel->duty[2]) + (UINT16_MAX - adv_pwm_channel->duty[3])) >> 2;
    }
    
    return -1;
}

#ifdef ESP_PLATFORM
static void IRAM_ATTR zero_crossing_interrupt(void* args) {
    gptimer_stop(adv_pwm_config->gptimer);
#else
static void IRAM zero_crossing_interrupt(const uint8_t gpio) {
#endif
    adv_pwm_config->current_duty = 0;
    adv_pwm_config->zc_status = 1;
    
    // Restart timer
#ifdef ESP_PLATFORM
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = 1,
    };
    gptimer_set_alarm_action(adv_pwm_config->gptimer, &alarm_config);
    gptimer_set_raw_count(adv_pwm_config->gptimer, 0);
    gptimer_start(adv_pwm_config->gptimer);
#else
    timer_set_load(FRC1, 1);
#endif
}

#ifdef ESP_PLATFORM
static bool IRAM_ATTR adv_pwm_worker(gptimer_handle_t gptimer, const gptimer_alarm_event_data_t *edata, void *args) {
    gptimer_stop(gptimer);
#else
static void IRAM adv_pwm_worker() {
#endif
    unsigned int next_load = adv_pwm_config->max_load;
    unsigned int next_duty = UINT16_MAX;
    
    adv_pwm_channel_t* adv_pwm_channel = adv_pwm_config->adv_pwm_channels;
    if (adv_pwm_config->current_duty == 0) {
        adv_pwm_config->cycle++;
        
        while (adv_pwm_channel) {
            if (adv_pwm_channel->duty[adv_pwm_config->cycle] == 0 ||
                (!adv_pwm_channel->leading && adv_pwm_config->zc_status == 2 && adv_pwm_channel->duty[adv_pwm_config->cycle] < UINT16_MAX)) {
                gpio_write(adv_pwm_channel->gpio, adv_pwm_channel->inverted);
            } else {
                gpio_write(adv_pwm_channel->gpio, 1 ^ adv_pwm_channel->inverted);
            }
            
            adv_pwm_channel = adv_pwm_channel->next;
        }
        
        adv_pwm_channel = adv_pwm_config->adv_pwm_channels;
    }
    
    if (adv_pwm_config->zc_status == 2) {
#ifdef ESP_PLATFORM
        return 0;
#else
        return;
#endif
    }
    
    while (adv_pwm_channel) {
        if (adv_pwm_channel->duty[adv_pwm_config->cycle] <= adv_pwm_config->current_duty) {
            gpio_write(adv_pwm_channel->gpio, adv_pwm_channel->inverted);
        } else if (adv_pwm_channel->duty[adv_pwm_config->cycle] > adv_pwm_config->current_duty &&
                   adv_pwm_channel->duty[adv_pwm_config->cycle] < next_duty) {
            next_duty = adv_pwm_channel->duty[adv_pwm_config->cycle];
        }
        
        adv_pwm_channel = adv_pwm_channel->next;
    }
    
    next_load = (next_duty - adv_pwm_config->current_duty) * adv_pwm_config->max_load / UINT16_MAX;
    
    if (next_duty == UINT16_MAX) {
        adv_pwm_config->current_duty = 0;
        if (adv_pwm_config->zc_status == 1) {
            adv_pwm_config->zc_status = 2;
            //next_load = next_load >> 1;
        }
    } else {
        adv_pwm_config->current_duty = next_duty;
    }
    
    if (next_load == 0) {
        next_load = 1;
    }
    
#ifdef ESP_PLATFORM
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = next_load,
    };
    gptimer_set_alarm_action(gptimer, &alarm_config);
    gptimer_set_raw_count(gptimer, 0);
    gptimer_start(gptimer);
    
    return 0;
#else
    timer_set_load(FRC1, next_load);
#endif
}

void adv_pwm_start() {
    if (!adv_pwm_config->is_running) {
#ifdef ESP_PLATFORM
        gptimer_alarm_config_t alarm_config = {
            .alarm_count = 1,
        };
        gptimer_set_alarm_action(adv_pwm_config->gptimer, &alarm_config);
        gptimer_set_raw_count(adv_pwm_config->gptimer, 0);
        gptimer_start(adv_pwm_config->gptimer);
#else
        timer_set_load(FRC1, 1);
        timer_set_reload(FRC1, false);
        timer_set_interrupts(FRC1, true);
        timer_set_run(FRC1, true);
#endif
        
        adv_pwm_config->is_running = true;
    }
}

void adv_pwm_stop() {
    if (adv_pwm_config->is_running) {
#ifdef ESP_PLATFORM
        gptimer_stop(adv_pwm_config->gptimer);
#else
        timer_set_interrupts(FRC1, false);
        timer_set_run(FRC1, false);
#endif
        
        adv_pwm_config->current_duty = 0;
        adv_pwm_config->cycle = 0;
        
        adv_pwm_channel_t* adv_pwm_channel = adv_pwm_config->adv_pwm_channels;
        while (adv_pwm_channel) {
            gpio_write(adv_pwm_channel->gpio, adv_pwm_channel->inverted);
            adv_pwm_channel = adv_pwm_channel->next;
        }
        
        adv_pwm_config->is_running = false;
    }
}

static void adv_pwm_init(const unsigned int mode) {
    if (!adv_pwm_config) {
        adv_pwm_config = malloc(sizeof(adv_pwm_config_t));
        memset(adv_pwm_config, 0, sizeof(*adv_pwm_config));

#ifdef ESP_PLATFORM
        gptimer_config_t timer_config = {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
            .direction = GPTIMER_COUNT_UP,
            .resolution_hz = ADV_PWM_TIMER_RESOLUTION,
        };
        
        gptimer_new_timer(&timer_config, &adv_pwm_config->gptimer);
        gptimer_event_callbacks_t cbs = {
            .on_alarm = adv_pwm_worker,
        };
        gptimer_register_event_callbacks(adv_pwm_config->gptimer, &cbs, NULL);
        gptimer_enable(adv_pwm_config->gptimer);
#else
        _xt_isr_attach(INUM_TIMER_FRC1, adv_pwm_worker, NULL);
#endif
        
        if (mode == 0) {
            adv_pwm_set_freq(ADV_PWM_FREQUENCY_DEFAULT);
        }
    }
}

void adv_pwm_set_freq(const uint16_t freq) {
    adv_pwm_init(1);
    
    const unsigned int pwm_was_running = adv_pwm_config->is_running;
    adv_pwm_stop();
    
#ifdef ESP_PLATFORM
    adv_pwm_config->max_load = ADV_PWM_TIMER_RESOLUTION / freq;
#else
    //timer_set_frequency(FRC1, freq);
    const timer_clkdiv_t timer_clkdiv = freq > 100 ? TIMER_CLKDIV_16 : TIMER_CLKDIV_256;
    timer_set_divider(FRC1, timer_clkdiv);
    const uint32_t counts = timer_freq_to_count(FRC1, freq, timer_clkdiv);
    adv_pwm_config->max_load = counts;
    timer_set_load(FRC1, counts);
#endif
    
    if (pwm_was_running) {
        adv_pwm_start();
    }
}

void adv_pwm_set_dithering(const uint8_t gpio, const uint16_t dithering) {
    adv_pwm_channel_t* adv_pwm_channel = adv_pwm_channel_find_by_gpio(gpio);
    if (adv_pwm_channel) {
        adv_pwm_channel->dithering = dithering;
    }
}

void adv_pwm_set_duty(const uint8_t gpio, uint16_t duty) {
    adv_pwm_channel_t* adv_pwm_channel = adv_pwm_channel_find_by_gpio(gpio);
    if (adv_pwm_channel) {
        if (adv_pwm_channel->leading) {
            duty = UINT16_MAX - duty;
        }
        
        unsigned int _dithering = adv_pwm_channel->dithering;
        
        if (_dithering == 0 || duty == 0 || duty == UINT16_MAX) {
            for (unsigned int i = 0; i < 8; i++) {
                adv_pwm_channel->duty[i] = duty;
            }
        } else {
            if (duty >= (UINT16_MAX - _dithering)) {
                _dithering = UINT16_MAX - duty;
            } else if (duty <= _dithering) {
                _dithering = 0;
            } else {
                _dithering = _dithering * duty / UINT16_MAX;
            }

            adv_pwm_channel->duty[0] = duty + _dithering;
            adv_pwm_channel->duty[1] = duty + (_dithering >> 1);
            adv_pwm_channel->duty[2] = duty;
            adv_pwm_channel->duty[3] = duty - (_dithering >> 1);
            adv_pwm_channel->duty[4] = duty - _dithering;
            
            for (unsigned int i = 5; i < 8; i++) {
                adv_pwm_channel->duty[i] = adv_pwm_channel->duty[8 - i];
            }
        }
    }
}

void adv_pwm_new_channel(const uint8_t gpio, const bool inverted, const bool leading, const uint16_t dithering, const uint16_t duty) {
    adv_pwm_init(0);
    
    if (!adv_pwm_channel_find_by_gpio(gpio)) {
        const unsigned int is_running = adv_pwm_config->is_running;
        if (is_running) {
            adv_pwm_stop();
        }
        
        adv_pwm_channel_t* adv_pwm_channel = malloc(sizeof(adv_pwm_channel_t));
        memset(adv_pwm_channel, 0, sizeof(*adv_pwm_channel));
        
        adv_pwm_channel->gpio = gpio;
        adv_pwm_channel->leading = leading;
        adv_pwm_channel->inverted = inverted ^ leading;
        adv_pwm_channel->dithering = dithering;
        
        adv_pwm_channel->next = adv_pwm_config->adv_pwm_channels;
        adv_pwm_config->adv_pwm_channels = adv_pwm_channel;
        
        adv_pwm_set_duty(gpio, duty);
        
        if (is_running) {
            adv_pwm_start();
        }
    }
}

void adv_pwm_set_zc_gpio(const uint8_t gpio, const uint8_t int_type) {
    adv_pwm_init(0);
    
    adv_pwm_config->zc_status = 1;
    
#ifdef ESP_PLATFORM
    gpio_install_isr_service(0);
    gpio_set_intr_type(gpio, int_type);
    gpio_isr_handler_add(gpio, zero_crossing_interrupt, NULL);
    gpio_intr_enable(gpio);
#else
    gpio_set_interrupt(gpio, int_type, zero_crossing_interrupt);
#endif
}
