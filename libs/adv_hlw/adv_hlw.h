/*
* Advanced HLW8012 Driver
*
* Copyright 2020-2023 José Antonio Jiménez Campos (@RavenSystem)
*
*/

#ifndef __ADV_HLW_H__
#define __ADV_HLW_H__

#ifdef __cplusplus
extern "C" {
#endif

int adv_hlw_unit_create(int gpio_cf, int gpio_cf1, int gpio_sel, const unsigned int current_mode, const unsigned int interrupt_type);

double adv_hlw_get_voltage_freq(const int gpio);
double adv_hlw_get_current_freq(const int gpio);
double adv_hlw_get_power_freq(const int gpio);

#ifdef __cplusplus
}
#endif

#endif  // __ADV_HLW_H__
