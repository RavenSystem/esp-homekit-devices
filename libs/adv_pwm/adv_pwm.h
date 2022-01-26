/*
* Advanced PWM Driver
*
* Copyright 2021-2022 José Antonio Jiménez Campos (@RavenSystem)
*
*/

#ifndef __ADV_PWM_H__
#define __ADV_PWM_H__

#ifdef __cplusplus
extern "C" {
#endif

void adv_pwm_start();
void adv_pwm_stop();
void adv_pwm_set_freq(const unsigned int freq);
void adv_pwm_set_duty(const unsigned int gpio, const unsigned int duty, unsigned int dithering);
int adv_pwm_get_duty(const unsigned int gpio);
void adv_pwm_new_channel(const unsigned int gpio, const bool inverted);
//void adv_pwm_set_zc_gpio(const unsigned int gpio);

#ifdef __cplusplus
}
#endif

#endif  // __ADV_PWM_H__
