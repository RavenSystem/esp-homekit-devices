/*
* Advanced NRZ LED Driver
*
* Copyright 2021-2022 José Antonio Jiménez Campos (@RavenSystem)
*
*/

#ifndef __ADVANCED_NRZ_LED__
#define __ADVANCED_NRZ_LED__

int nrz_ticks(const float time_us);
void IRAM nrzled_set(const uint8_t gpio, const uint16_t ticks_0, const uint16_t ticks_1, const uint16_t period, uint8_t *colors, const uint16_t size);

#endif // __ADVANCED_NRZ_LED__
