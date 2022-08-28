/* Implementation of Multi-Channel PWM support
 *
 * Copyright (c) Sashka Nochkin (https://github.com/nochkin)
 */
#ifndef EXTRAS_MULTIPWM_H_
#define EXTRAS_MULTIPWM_H_

#include <stdint.h>

#define MULTIPWM_MAX_CHANNELS 8
//#define MULTIPWM_MAX_PERIOD 0x7fffff
#define MULTIPWM_MAX_PERIOD UINT16_MAX

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t pin;
    uint32_t duty;
} pwm_pin_t;

typedef struct pwm_schedule {
    uint32_t pins_s;
    uint32_t pins_c;
    uint32_t ticks;
    struct pwm_schedule* next;
} pwm_schedule_t;

typedef struct {
    uint16_t freq;
    uint8_t channels;
    pwm_pin_t pins[MULTIPWM_MAX_CHANNELS];

    uint32_t _period;
    uint32_t _configured_pins;
    uint32_t _output;
    pwm_schedule_t _schedule[MULTIPWM_MAX_CHANNELS+1];
    pwm_schedule_t *_schedule_current;
    uint32_t _tick;
} pwm_info_t;

/**
 * Initialize multipwm
 * @param pwm_info Pointer to pwm_info
 */
void multipwm_init(pwm_info_t *pwm_info);

/**
 * Set channel configuration
 * @param pwm_info Pointer to pwm_info
 * @param channel Channel index
 * @param pin Pin number
 */
void multipwm_set_pin(pwm_info_t *pwm_info, uint8_t channel, uint8_t pin);

/**
 * Set PWM frequency
 * @param pwm_info Pointer to pwm_info
 * @param freq PWM frequency value in Hertz
 */
void multipwm_set_freq(pwm_info_t *pwm_info, uint16_t freq);

/**
 * Set Duty between 0 and UINT16_MAX for a specific channel
 * @param pwm_info Pointer to pwm_info
 * @param channel Channel index
 * @param duty Duty value
 */
void multipwm_set_duty(pwm_info_t *pwm_info, uint8_t channel, uint32_t duty);

/**
 * Set Duty for all channels between 0 and UINT16_MAX
 * @param pwm_info Pointer to pwm_info
 * @param duty Duty value
 */
void multipwm_set_duty_all(pwm_info_t *pwm_info, uint32_t duty);

/**
 * Start multipwm
 * @param pwm_info Pointer to pwm_info
 */
void multipwm_start(pwm_info_t *pwm_info);

/**
 * Stop multipwm
 * @param pwm_info Pointer to pwm_info
 */
void multipwm_stop(pwm_info_t *pwm_info);

#ifdef __cplusplus
}
#endif

#endif /* EXTRAS_MULTIPWM_H_ */
