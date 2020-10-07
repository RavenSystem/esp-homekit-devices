#ifndef HAA_LIGHTBULD_WSS2812_DRIVER_H
#define HAA_LIGHTBULD_WSS2812_DRIVER_H
#include "haa_lightbulb_driver.h"

typedef struct 
{
    uint8_t model;
    uint8_t pin;
    uint8_t numPixels;
    uint8_t numChannelsPerPixel;
    float converationFactor;
} ws2812_driver_info_t;

void haa_ws2812_init(driver_interface_t** interface, const cJSON * const object, lightbulb_channels_t* channels);
void haa_ws2812_send(const void* driver_info, const uint16_t* multipwm_duty);

#endif