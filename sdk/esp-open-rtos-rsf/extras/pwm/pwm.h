/* Implementation of PWM support for the Espressif SDK.
 *
 * Part of esp-open-rtos
 * Copyright (C) 2015 Guillem Pascual Ginovart (https://github.com/gpascualg)
 * Copyright (C) 2015 Javier Cardona (https://github.com/jcard0na)
 * BSD Licensed as described in the file LICENSE
 */
#ifndef EXTRAS_PWM_H_
#define EXTRAS_PWM_H_

#include <stdint.h>

#define MAX_PWM_PINS    8

#ifdef __cplusplus
extern "C" {
#endif

//Warning: Printf disturb pwm. You can use "uart_putc" instead.

/**
 * Initialize pwm
 * @param npins Number of pwm pin used
 * @param pins Array pointer to the pins
 * @param reverse If true, the pwm work in reverse mode
 */    
void pwm_init(uint8_t npins, const uint8_t* pins, uint8_t reverse);

/**
 * Set PWM frequency. If error, frequency not set
 * @param freq PWM frequency value in Hertz
 */  
void pwm_set_freq(uint16_t freq);

/**
 * Set Duty between 0 and UINT16_MAX
 * @param duty Duty value
 */  
void pwm_set_duty(uint16_t duty);

/**
 * Restart the pwm signal
 */  
void pwm_restart();

/**
 * Start the pwm signal
 */  
void pwm_start();

/**
 * Stop the pwm signal
 */  
void pwm_stop();

#ifdef __cplusplus
}
#endif

#endif /* EXTRAS_PWM_H_ */
