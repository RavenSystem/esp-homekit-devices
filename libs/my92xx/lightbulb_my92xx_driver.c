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

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "lightbulb_my92xx_driver.h"
#include <FreeRTOS.h>
#include <espressif/esp_common.h>
#include <string.h>


 #define MY92XX_DEFAULT_FREQUENCY  (2000)

typedef struct _my92xx_channel {
    uint16_t duty;
    uint8_t channel;
    struct _my92xx_channel* next;
} my92xx_channel_t;

typedef struct _my92xx_driver_info{
    uint8_t di_pin;
    uint8_t dcki_pin;
    uint8_t numChips;
    my92xx_channel_t* channels;
} my92xx_driver_info_t;

void my92xx_send_pulses(uint8_t pin, unsigned int times);
void my92xx_write(unsigned int data, unsigned char bit_length);
void my92xx_send();

static my92xx_cmd_t config = {
    .bit_width = MY92XX_CMD_BIT_WIDTH_16,
    .scatter = MY92XX_CMD_SCATTER_APDM,
    .frequency = MY92XX_CMD_FREQUENCY_DIVIDE_1,
    .reaction = MY92XX_CMD_REACTION_FAST,
    .one_shot = MY92XX_CMD_ONE_SHOT_DISABLE,
    .resv = 0
};
static my92xx_driver_info_t info;


void my92xx_configure(uint8_t di_pin, uint8_t dcki_pin, uint8_t numChips)
{
    info.di_pin = di_pin;
    info.dcki_pin = dcki_pin;
    info.numChips = numChips;
}

void my92xx_start()
{
    // Init GPIO
    gpio_enable(info.di_pin, GPIO_OUTPUT);
    gpio_enable(info.dcki_pin, GPIO_OUTPUT);
    gpio_write(info.di_pin, false);
    gpio_write(info.dcki_pin, false);

    // Clear all duty register
    my92xx_send_pulses(info.dcki_pin, 32 * info.numChips);

    // Send init command
    // TStop > 12us.
	sdk_os_delay_us(12);

    // Send 12 DI pulse, after 6 pulse's falling edge store duty data, and 12
	// pulse's rising edge convert to command mode.
	my92xx_send_pulses(info.di_pin, 12);

    // Delay >12us, begin send CMD data
	sdk_os_delay_us(12);

    // Send CMD data
    unsigned char command_data = *(unsigned char *) (&config);
    for (unsigned char i=0; i < info.numChips; i++)
    {
        my92xx_write(command_data, 16 - (1 << MY92XX_CMD_BIT_WIDTH_8));
    }

	// TStart > 12us. Delay 12 us.
	sdk_os_delay_us(12);

    // Send 16 DI pulse，at 14 pulse's falling edge store CMD data, and
	// at 16 pulse's falling edge convert to duty mode.
	my92xx_send_pulses(info.di_pin, 16);

    // TStop > 12us.
	sdk_os_delay_us(12);
}
void my92xx_stop()
{

}
void my92xx_set_freq(const uint16_t freq)
{
    uint16_t min_ind = 0;
    uint16_t min_diff = UINT16_MAX;
    for(uint16_t i = MY92XX_CMD_FREQUENCY_DIVIDE_1; i <= MY92XX_CMD_FREQUENCY_DIVIDE_64; i++)
    {
        uint16_t diff = abs((0x01<<(2*i)) * freq - MY92XX_DEFAULT_FREQUENCY); 
        if(diff < min_diff)
        {
            min_diff = diff;
            min_ind = i;    
        }
    }
    config.frequency = min_ind;
}

my92xx_channel_t* my92xx_channel_find_by_channel(const uint8_t channel) 
{
    my92xx_channel_t* my92xx_channel = info.channels;
    
    while (my92xx_channel && my92xx_channel->channel != channel) 
    {
        my92xx_channel = my92xx_channel->next;
    }
    return my92xx_channel;
}

void my92xx_set_duty(const uint8_t channel, const uint16_t duty, uint16_t dithering)
{
    my92xx_channel_t* my92xx_channel = my92xx_channel_find_by_channel(channel);
    if (my92xx_channel) 
    {
        my92xx_channel->duty = duty;
        my92xx_send();
    }
}

uint16_t my92xx_get_duty(const uint8_t channel) 
{
    my92xx_channel_t* my92xx_channel = my92xx_channel_find_by_channel(channel);
    if (my92xx_channel)
    {
        return my92xx_channel->duty;
    }
    return 0;
}

void my92xx_new_channel(const uint8_t gpio, const bool inverted) {
    
    if (!my92xx_channel_find_by_channel(gpio)) {
        
        my92xx_channel_t* my92xx_channel = malloc(sizeof(my92xx_channel_t));
        memset(my92xx_channel, 0, sizeof(*my92xx_channel));
        
        my92xx_channel->channel = gpio;
        my92xx_channel->next = info.channels;
        info.channels = my92xx_channel;
    }
}

void my92xx_send_pulses(uint8_t pin, unsigned int times)
{
	for (unsigned int i = 0; i < times; i++)
    {
		gpio_write(pin, true);
		gpio_write(pin, false);
	}
}

void my92xx_write(unsigned int data, unsigned char bit_length)
{
    unsigned int mask = (0x01 << (bit_length - 1));

    for (unsigned int i = 0; i < bit_length / 2; i++) 
    {
        gpio_write(info.dcki_pin, false);
        gpio_write(info.di_pin, (data & mask));
        gpio_write(info.dcki_pin, true);
        data = data << 1;
        gpio_write(info.di_pin, (data & mask));
        gpio_write(info.dcki_pin, false);
        gpio_write(info.di_pin, false);
        data = data << 1;
    }

}

void my92xx_send()
{
    // TStop > 12us.
    sdk_os_delay_us(12);

    // Send color data

    uint8_t channel_ind = 0;
    my92xx_channel_t* channel = my92xx_channel_find_by_channel(channel_ind);
    while (channel != NULL)
    {
        my92xx_write(channel->duty, 16 - (1 << MY92XX_CMD_BIT_WIDTH_16));
        channel++;
    }

    // TStart > 12us. Ready for send DI pulse.
    sdk_os_delay_us(12);

    // Send 8 DI pulse. After 8 pulse falling edge, store old data.
    my92xx_send_pulses(info.di_pin, 8);

    // TStop > 12us.
    sdk_os_delay_us(12);

}