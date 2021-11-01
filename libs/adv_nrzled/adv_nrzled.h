/*
* Advanced NRZ LED Driver
*
* Copyright 2021 José Antonio Jiménez Campos (@RavenSystem)
*
*/

#ifndef __ADVANCED_NRZ_LED__
#define __ADVANCED_NRZ_LED__

void IRAM nrzled_set(const uint8_t gpio, const uint16_t time_0, const uint16_t time_1, const uint16_t period, uint8_t *colors, const uint16_t size);

#endif // __ADVANCED_NRZ_LED__
