/**
 * my92xx LED Driver
 * Based on the C++ driver by Xose Pérez
 * Copyright (c) 2016 - 2026 MaiKe Labs
 * Copyright (C) 2017 - 2018 Xose Pérez for the Arduino compatible library
 *  
 * This code is a C port of the my92xx LED Driver Library by Xose Perez
 * the original C++ implementation is https://github.com/xoseperez/my92xx
 * Some functions were adjusted but mainly it's a copy paste of the library mentioned above.
 * Original comments are kept.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdlib.h>
#include <cJSON.h>
#include "lightbulb_my92xx_driver.h"
#include <string.h>
#include <FreeRTOS.h>
#include <espressif/esp_common.h>
#include "header.h"

void haa_my92xx_send_pulses(uint8_t pin, unsigned int times);
void haa_my92xx_set_cmd(const my92xx_driver_info_t* descriptor, const my92xx_cmd_t* command);
void haa_my92xx_write(const my92xx_driver_info_t* descriptor, unsigned int data, unsigned char bit_length);

void haa_my92xx_init(driver_interface_t** interface, my92xx_model_t model, const cJSON * const object, const my92xx_cmd_t* command, lightbulb_channels_t* channels)
{

    if ((cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_DRIVER_GPIO_DI) != NULL) && 
        (cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_DRIVER_GPIO_DCKI) != NULL))
    {
        *interface = malloc( sizeof(driver_interface_t));
        if(interface != NULL)
        {
            my92xx_driver_info_t* descriptor = malloc( sizeof(my92xx_driver_info_t));
            if(descriptor != NULL)
            {
                (*interface)->driver_info = (void*) descriptor; 
                (*interface)->family = DRIVER_MY92XX;
                descriptor->model = model;
                descriptor->di_pin = (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_DRIVER_GPIO_DI)->valuedouble;
                descriptor->dcki_pin = (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_DRIVER_GPIO_DCKI)->valuedouble;
                (*interface)->updater = haa_my92xx_send;
                
                descriptor->numChips = 1;
                if (cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_DRIVER_NUM_CHIPS) != NULL)
                {
                    descriptor->numChips = (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_DRIVER_NUM_CHIPS)->valuedouble;
                }
                (*interface)->numChips = descriptor->numChips;

                switch (command->bit_width) 
                {
                    case MY92XX_CMD_BIT_WIDTH_16:
                        descriptor->bit_width = 16;
                        break;
                    case MY92XX_CMD_BIT_WIDTH_14:
                        descriptor->bit_width = 14;
                        break;
                    case MY92XX_CMD_BIT_WIDTH_12:
                        descriptor->bit_width = 12;
                        break;
                    case MY92XX_CMD_BIT_WIDTH_8:
                        descriptor->bit_width = 8;
                        break;
                    default:
                        descriptor->bit_width = 8;
                        break;
                }
                uint8_t numChannels = 0;  
                if(descriptor->model == MY92XX_MODEL_MY9291) 
                {
                    numChannels = 4;
                } else if (descriptor->model == MY92XX_MODEL_MY9231) 
                {
                    numChannels = 3;
                }
                descriptor->numChannelsPerChip = numChannels;
                (*interface)->numChannelsPerChip = descriptor->numChannelsPerChip;
                if ((numChannels > 0) && (cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_R) != NULL)) 
                {
                    channels->r = (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_R)->valuedouble;
                    numChannels--;
                }
                
                if ((numChannels > 0) && (cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_G) != NULL)) 
                {
                    channels->g = (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_G)->valuedouble;
                    numChannels--;
                }
                
                if ((numChannels > 0) && (cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_B) != NULL))
                {
                    channels->b = (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_B)->valuedouble;
                    numChannels--;
                }
                
                if ((numChannels > 0) && (cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_W) != NULL))
                {
                    channels->w = (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_W)->valuedouble;
                    numChannels--;
                }
                
                if ((numChannels > 0) &&  (cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_CW) != NULL))
                {
                    channels->cw = (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_CW)->valuedouble;
                    numChannels--;
                }
                
                if ((numChannels > 0) && (cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_WW) != NULL))
                {
                    channels->ww = (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_WW)->valuedouble;
                    numChannels--;
                }

                
                // Init GPIO
                gpio_enable(descriptor->di_pin, GPIO_OUTPUT);
                gpio_enable(descriptor->dcki_pin, GPIO_OUTPUT);
                gpio_write(descriptor->di_pin, false);
                gpio_write(descriptor->dcki_pin, false);

                // Clear all duty register
                haa_my92xx_send_pulses(descriptor->dcki_pin, 32 * descriptor->numChips);

                // Send init command
                haa_my92xx_set_cmd(descriptor, command);
            }
        }
    }
}

void haa_my92xx_send_pulses(uint8_t pin, unsigned int times)
{
	for (unsigned int i = 0; i < times; i++)
    {
		gpio_write(pin, true);
		gpio_write(pin, false);
	}
}


void haa_my92xx_set_cmd(const my92xx_driver_info_t* descriptor, const my92xx_cmd_t* command)
{
    // TStop > 12us.
	sdk_os_delay_us(12);

    // Send 12 DI pulse, after 6 pulse's falling edge store duty data, and 12
	// pulse's rising edge convert to command mode.
	haa_my92xx_send_pulses(descriptor->di_pin, 12);

    // Delay >12us, begin send CMD data
	sdk_os_delay_us(12);

    // Send CMD data
    unsigned char command_data = *(unsigned char *) (command);
    for (unsigned char i=0; i<descriptor->numChips; i++)
    {
        haa_my92xx_write(descriptor, command_data, 8);
    }

	// TStart > 12us. Delay 12 us.
	sdk_os_delay_us(12);

    // Send 16 DI pulse，at 14 pulse's falling edge store CMD data, and
	// at 16 pulse's falling edge convert to duty mode.
	haa_my92xx_send_pulses(descriptor->di_pin, 16);

    // TStop > 12us.
	sdk_os_delay_us(12);

}

void haa_my92xx_write(const my92xx_driver_info_t* descriptor, unsigned int data, unsigned char bit_length)
{
    unsigned int mask = (0x01 << (bit_length - 1));

    for (unsigned int i = 0; i < bit_length / 2; i++) 
    {
        gpio_write(descriptor->dcki_pin, false);
        gpio_write(descriptor->di_pin, (data & mask));
        gpio_write(descriptor->dcki_pin, true);
        data = data << 1;
        gpio_write(descriptor->di_pin, (data & mask));
        gpio_write(descriptor->dcki_pin, false);
        gpio_write(descriptor->di_pin, false);
        data = data << 1;
    }

}

void haa_my92xx_send(const void* driver_info, const uint16_t* multipwm_duty)
{
    if(driver_info != NULL)
    {
        my92xx_driver_info_t* descriptor = (my92xx_driver_info_t*)driver_info;
        // TStop > 12us.
        sdk_os_delay_us(12);

        // Send color data

        for(unsigned char chip = 0; chip < descriptor->numChips; chip++)
        {
            for (unsigned char channel = 0; channel <descriptor->numChannelsPerChip; channel++) 
            {
                haa_my92xx_write(descriptor, multipwm_duty[channel], descriptor->bit_width);
            }
        }

        // TStart > 12us. Ready for send DI pulse.
        sdk_os_delay_us(12);

        // Send 8 DI pulse. After 8 pulse falling edge, store old data.
        haa_my92xx_send_pulses(descriptor->di_pin, 8);

        // TStop > 12us.
        sdk_os_delay_us(12);
    }

}