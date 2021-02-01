/*
* Advanced PWM Driver
*
* Copyright 2021 José Antonio Jiménez Campos (@RavenSystem)
*
*/

#ifndef __ADV_PWM_H__
#define __ADV_PWM_H__

#ifdef __cplusplus
extern "C" {
#endif

void adv_pwm_start();
void adv_pwm_stop();
void adv_pwm_set_freq(const uint16_t freq);
void adv_pwm_set_duty(const uint8_t gpio, const uint16_t duty, uint16_t dithering);
uint16_t adv_pwm_get_duty(const uint8_t gpio);
void adv_pwm_new_channel(const uint8_t gpio, const bool inverted);

#ifdef __cplusplus
}
#endif

#endif  // __ADV_PWM_H__
