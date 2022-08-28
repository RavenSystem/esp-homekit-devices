/*
 * Multi-channel PWM support for esp-open-rtos
 *
 * (c) 2017-2018 Sashka Nochkin https://github.com/nochkin/pwm
 */

#include <string.h>

#include "espressif/esp_common.h"
#include "FreeRTOS.h"
#include "task.h"

#include "multipwm.h"

#ifdef PWM_DEBUG
#define debug(fmt, ...) printf("%s: " fmt "\n", "PWM", ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
      (byte & 0x80 ? '1' : '0'), \
      (byte & 0x40 ? '1' : '0'), \
      (byte & 0x20 ? '1' : '0'), \
      (byte & 0x10 ? '1' : '0'), \
      (byte & 0x08 ? '1' : '0'), \
      (byte & 0x04 ? '1' : '0'), \
      (byte & 0x02 ? '1' : '0'), \
      (byte & 0x01 ? '1' : '0') 

static void IRAM multipwm_interrupt_handler(void *arg) {
    pwm_info_t *pwm_info = arg;
    pwm_schedule_t *curr = pwm_info->_schedule_current;
    pwm_schedule_t *curr_next = curr->next;
    uint32_t tick_next = curr_next->ticks;

    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, curr->pins_s);
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, curr->pins_c);

    if (tick_next == 0) {
        tick_next = pwm_info->_period;
    }

    pwm_info->_tick = curr->ticks;
    pwm_info->_schedule_current = curr_next;

    timer_set_load(FRC1, tick_next - pwm_info->_tick);
}

void multipwm_init(pwm_info_t *pwm_info) {
    pwm_info->_configured_pins = 0;

    pwm_info->_output = 0;
    bzero(pwm_info->_schedule, MULTIPWM_MAX_CHANNELS * sizeof(pwm_schedule_t));
    bzero(pwm_info->pins, MULTIPWM_MAX_CHANNELS * sizeof(pwm_pin_t));
    pwm_info->_tick = 0;
    pwm_info->_period = MULTIPWM_MAX_PERIOD;

    pwm_info->_schedule[0].pins_s = 0;
    pwm_info->_schedule[0].pins_c = pwm_info->_configured_pins;
    pwm_info->_schedule[0].ticks = 0;
    pwm_info->_schedule[0].next = &pwm_info->_schedule[0];

    pwm_info->_schedule_current = &pwm_info->_schedule[0];

    multipwm_stop(pwm_info);

    _xt_isr_attach(INUM_TIMER_FRC1, multipwm_interrupt_handler, (_xt_isr)pwm_info);

    debug("PWM Init");
}

void multipwm_set_pin(pwm_info_t *pwm_info, uint8_t channel, uint8_t pin) {
    if (channel >= pwm_info->channels) return;

    pwm_info->pins[channel].pin = pin;
    pwm_info->pins[channel].duty = 0;

    gpio_enable(pin, GPIO_OUTPUT);
    pwm_info->_configured_pins |= 1 << pin;
}

void multipwm_set_freq(pwm_info_t *pwm_info, uint16_t freq) {
    if (!timer_set_frequency(FRC1, freq)) {
        pwm_info->freq = freq;
        debug("Frequency set at %u", pwm_info->freq);
    }
}

void multipwm_dump_schedule(pwm_info_t *pwm_info) {
#ifdef PWM_DEBUG
    debug("Schedule:\n");
    uint8_t ii = 0;
    pwm_schedule_t *pwm_schedule = &pwm_info->_schedule[0];
    do {
        printf(" - %i:%5i: s: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n            c: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n",
                ii,
                pwm_schedule->ticks,
                BYTE_TO_BINARY(pwm_schedule->pins_s >> 24),
                BYTE_TO_BINARY(pwm_schedule->pins_s >> 16),
                BYTE_TO_BINARY(pwm_schedule->pins_s >> 8),
                BYTE_TO_BINARY(pwm_schedule->pins_s),
                BYTE_TO_BINARY(pwm_schedule->pins_c >> 24),
                BYTE_TO_BINARY(pwm_schedule->pins_c >> 16),
                BYTE_TO_BINARY(pwm_schedule->pins_c >> 8),
                BYTE_TO_BINARY(pwm_schedule->pins_c)
                );
        pwm_schedule = pwm_schedule->next;
    } while (pwm_schedule->ticks > 0);
#endif
}

void multipwm_set_duty(pwm_info_t *pwm_info, uint8_t channel, uint32_t duty) {
    if (channel >= pwm_info->channels) return;

    pwm_info->pins[channel].duty = duty;
    uint8_t pin_n = pwm_info->pins[channel].pin;

    pwm_schedule_t *sched_prev = &pwm_info->_schedule[0];
    pwm_schedule_t *sched = sched_prev->next;

    if (duty == 0) {
        sched_prev->pins_s &= ~(1 << pin_n);
        sched_prev->pins_c |= (1 << pin_n);
    } else {
        sched_prev->pins_c &= ~(1 << pin_n);
        sched_prev->pins_s |= (1 << pin_n);
    }

    bool new_sched = false;
    pwm_schedule_t *new_sched_prev = NULL;
    pwm_schedule_t *new_sched_next = NULL;

    do {
        if (duty == sched->ticks) { // set when existing slice found
            sched->pins_s &= ~(1 << pin_n);
            sched->pins_c |= (1 << pin_n);
        } else {                    // remove from other slices
            if (sched->ticks > 0) {
                sched->pins_s &= ~(1 << pin_n);
                sched->pins_c &= ~(1 << pin_n);
            }
        }
        if ((duty > sched_prev->ticks) && (duty < sched->ticks)) {  // prepare to insert a new slice
            new_sched = true;
            new_sched_next = sched;
            new_sched_prev = sched_prev;
        } else if ((sched->next->ticks == 0) && (duty > sched->ticks)) {
            new_sched = true;
            new_sched_next = sched->next;
            new_sched_prev = sched;
        }
        sched_prev = sched;
        sched = sched->next;
    } while (sched->ticks > 0);

    if (new_sched) {
        for (uint8_t ii=1; ii<MULTIPWM_MAX_CHANNELS+1; ii++) {
            if (pwm_info->_schedule[ii].ticks == 0) {
                pwm_schedule_t *new_sched_pos = &pwm_info->_schedule[ii];
                new_sched_pos->pins_c |= (1 << pin_n);
                new_sched_pos->pins_s = 0;
                new_sched_pos->ticks = duty;
                new_sched_pos->next = new_sched_next;
                new_sched_prev->next = new_sched_pos;
                break;
            }
        }
    }

    // cleanup

    bool running = timer_get_run(FRC1);
    if (running) {
        multipwm_stop(pwm_info);
    }

    sched_prev = &pwm_info->_schedule[0];
    sched = sched_prev->next;
    do {
        if ((sched->pins_s == 0) && (sched->pins_c == 0)) {
            pwm_info->_schedule_current = &pwm_info->_schedule[0];
            pwm_info->_tick = 0;
            sched_prev->next = sched->next;
            sched->ticks = 0;
            break;
        }
        sched_prev = sched;
        sched = sched->next;
    } while (sched->ticks > 0);

    if (running) {
        multipwm_start(pwm_info);
    }

}

void multipwm_set_duty_all(pwm_info_t *pwm_info, uint32_t duty) {
    multipwm_stop(pwm_info);
    for (uint8_t ii=0; ii<pwm_info->channels; ii++) {
        multipwm_set_duty(pwm_info, ii, duty);
    }
    multipwm_start(pwm_info);
}

void multipwm_start(pwm_info_t *pwm_info) {
    timer_set_load(FRC1, 1);
    timer_set_reload(FRC1, false);
    timer_set_interrupts(FRC1, true);
    timer_set_run(FRC1, true);

    debug("PWM started");
}

void multipwm_stop(pwm_info_t *pwm_info) {
    timer_set_interrupts(FRC1, false);
    timer_set_run(FRC1, false);
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pwm_info->_configured_pins);

    debug("PWM stopped");
}

