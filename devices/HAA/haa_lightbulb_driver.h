#ifndef HAA_LIGHTBULD_DRIVER_H
#define HAA_LIGHTBULD_DRIVER_H

#include <stdint.h>
typedef enum _driver_family {
        DRIVER_PWM = 0X00,
        DRIVER_MY92XX = 0X01,
} driver_family_t;


typedef struct _driver_descriptor 
{
    driver_family_t family;
    uint8_t model;
    uint8_t numChips;
    uint8_t numChannelsPerChip;
    uint8_t numChannels ;
    uint8_t di_pin;
    uint8_t dcki_pin;
    uint16_t* channel_values;
    uint8_t bit_width;
} driver_descriptor_t;

typedef void (*haa_driver_setter_callback_t)(driver_descriptor_t*, uint8_t, uint16_t);
typedef void (*haa_driver_updater_callback_t)(driver_descriptor_t*);

typedef struct _driver_interface
{
    driver_descriptor_t* descriptor;
    haa_driver_setter_callback_t setter;
    haa_driver_updater_callback_t updater;
} driver_interface_t;

void haa_lighbulb_driver_set_all(const driver_interface_t* interface, const uint16_t* multipwm_duty);
#endif
