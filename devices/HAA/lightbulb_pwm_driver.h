#ifndef HAA_LIGHTBULD_PWM_DRIVER_H
#define HAA_LIGHTBULD_PWM_DRIVER_H
#include "haa_lightbulb_driver.h"


void haa_pwm_init(driver_interface_t** interface, const cJSON * const object, lightbulb_channels_t* channels);
void haa_pwm_send(const void* driver_info, const uint16_t* multipwm_duty);

#endif