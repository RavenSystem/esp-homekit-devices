/*
* Advanced HLW8012 Driver
*
* Copyright 2020 José Antonio Jiménez Campos (@RavenSystem)
*
*/

#ifndef __ADV_HLW_H__
#define __ADV_HLW_H__

#ifdef __cplusplus
extern "C" {
#endif

int adv_hlw_unit_create(const int8_t gpio_cf, const int8_t gpio_cf1, const int8_t gpio_sel, const uint8_t chip_type);

double adv_hlw_get_voltage_freq(const uint8_t gpio);
double adv_hlw_get_current_freq(const uint8_t gpio);
double adv_hlw_get_power_freq(const uint8_t gpio);

#ifdef __cplusplus
}
#endif

#endif  // __ADV_HLW_H__
