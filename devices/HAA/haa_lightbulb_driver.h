#ifndef HAA_LIGHTBULD_DRIVER_H
#define HAA_LIGHTBULD_DRIVER_H

#include <stdint.h>
typedef enum _driver_family {
        DRIVER_PWM = 0X00,
        DRIVER_MY92XX = 0X01,
} driver_family_t;

typedef void (*haa_driver_updater_callback_t)(const void* driver_info, const uint16_t* multipwm_duty);

typedef struct 
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
    uint8_t cw;
    uint8_t ww;
} lightbulb_channels_t;

typedef struct _driver_interface
{
    driver_family_t family;
    uint8_t numChips;
    uint8_t numChannelsPerChip;
    void* driver_info;
    haa_driver_updater_callback_t updater;
} driver_interface_t;

#endif
