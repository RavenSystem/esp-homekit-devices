#include "haa_lightbulb_driver.h"
#include "stdlib.h"
#include "header.h"

void haa_lighbulb_driver_set_all(const driver_interface_t* interface, const uint16_t* multipwm_duty)
{
    INFO("WWWW 2.1\n");
    if((interface->setter != NULL) && (interface->updater != NULL))
    {
        INFO("WWWW 2.2\n");
        for (uint8_t channel = 0; channel < interface->descriptor->numChannelsPerChip; channel++)
        {
            for (uint8_t chip = 0; chip < interface->descriptor->numChips; chip++)
            {
                interface->setter(interface->descriptor, channel + chip * interface->descriptor->numChannelsPerChip, multipwm_duty[channel]);        
            }
        }
        interface->updater(interface->descriptor);
    }        
}

