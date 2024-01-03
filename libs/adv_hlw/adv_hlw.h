/*
* Advanced HLW8012 Driver
*
* Copyright 2020-2024 José Antonio Jiménez Campos (@RavenSystem)
*
*/

#ifndef __ADV_HLW_H__
#define __ADV_HLW_H__

#ifdef __cplusplus
extern "C" {
#endif

int adv_hlw_unit_create(const int gpio_cf, const int gpio_cf1, const int gpio_sel, const bool current_mode, const uint8_t interrupt_type);

float adv_hlw_get_voltage_freq(const int gpio);
float adv_hlw_get_current_freq(const int gpio);
float adv_hlw_get_power_freq(const int gpio);

#ifdef __cplusplus
}
#endif

#endif  // __ADV_HLW_H__
