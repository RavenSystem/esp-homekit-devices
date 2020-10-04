#include <stdlib.h>
#include "my92xx.h"
#include <string.h>
#include <FreeRTOS.h>
#include <espressif/esp_common.h>

void my92xx_send_pulses(uint8_t pin, unsigned int times);
void my92xx_set_cmd(const driver_descriptor_t* descriptor, const my92xx_cmd_t* command);
void my92xx_write(const driver_descriptor_t* descriptor, unsigned int data, unsigned char bit_length);

void my92xx_init(driver_interface_t** interface, my92xx_model_t model, uint8_t numChips, uint8_t di_pin, uint8_t dcki_pin, const my92xx_cmd_t* command)
{

    *interface = malloc( sizeof(driver_interface_t));
    if(interface != NULL)
    {
        (*interface)->descriptor = malloc( sizeof(driver_descriptor_t));
        if((*interface)->descriptor != NULL)
        {
            driver_descriptor_t* descriptor = (*interface)->descriptor; 
            descriptor->family = DRIVER_MY92XX;
            descriptor->model = model;
            descriptor->numChips = numChips;
            descriptor->di_pin = di_pin;
            descriptor->dcki_pin = dcki_pin;
            (*interface)->updater = my92xx_send;
            (*interface)->setter = my92xx_setChannel;

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
            if(descriptor->model == MY92XX_MODEL_MY9291) 
            {
                descriptor->numChannelsPerChip = 4;
            } else if (descriptor->model == MY92XX_MODEL_MY9231) 
            {
                descriptor->numChannelsPerChip = 3;
            }
            descriptor->numChannels = descriptor->numChannelsPerChip * descriptor->numChips;
            descriptor->channel_values = malloc(descriptor->numChannels * sizeof(uint16_t));

            if(descriptor->channel_values != NULL)
            {
                memset((void*) descriptor->channel_values, 0, descriptor->numChannels*sizeof(uint16_t));
        
                // Init GPIO
                gpio_enable(descriptor->di_pin, GPIO_OUTPUT);
                gpio_enable(descriptor->dcki_pin, GPIO_OUTPUT);
                gpio_write(descriptor->di_pin, false);
                gpio_write(descriptor->dcki_pin, false);

                // Clear all duty register
                my92xx_send_pulses(descriptor->dcki_pin, 32 * descriptor->numChips);

                // Send init command
                my92xx_set_cmd(descriptor, command);
            }
        }
    }
}

void my92xx_send(driver_descriptor_t* descriptor)
{
    // TStop > 12us.
    sdk_os_delay_us(12);

    // Send color data

    for (unsigned char channel = 0; channel <descriptor->numChannels  ; channel++) 
    {
        my92xx_write(descriptor, descriptor->channel_values[channel], descriptor->bit_width);
	}

	// TStart > 12us. Ready for send DI pulse.
	sdk_os_delay_us(12);

	// Send 8 DI pulse. After 8 pulse falling edge, store old data.
	my92xx_send_pulses(descriptor->di_pin, 8);

	// TStop > 12us.
	sdk_os_delay_us(12);

}


void my92xx_send_pulses(uint8_t pin, unsigned int times)
{
	for (unsigned int i = 0; i < times; i++)
    {
		gpio_write(pin, true);
		gpio_write(pin, false);
	}
}


void my92xx_set_cmd(const driver_descriptor_t* descriptor, const my92xx_cmd_t* command)
{
    // TStop > 12us.
	sdk_os_delay_us(12);

    // Send 12 DI pulse, after 6 pulse's falling edge store duty data, and 12
	// pulse's rising edge convert to command mode.
	my92xx_send_pulses(descriptor->di_pin, 12);

    // Delay >12us, begin send CMD data
	sdk_os_delay_us(12);

    // Send CMD data
    unsigned char command_data = *(unsigned char *) (command);
    for (unsigned char i=0; i<descriptor->numChips; i++)
    {
        my92xx_write(descriptor, command_data, 8);
    }

	// TStart > 12us. Delay 12 us.
	sdk_os_delay_us(12);

    // Send 16 DI pulse，at 14 pulse's falling edge store CMD data, and
	// at 16 pulse's falling edge convert to duty mode.
	my92xx_send_pulses(descriptor->di_pin, 16);

    // TStop > 12us.
	sdk_os_delay_us(12);

}

void my92xx_write(const driver_descriptor_t* descriptor, unsigned int data, unsigned char bit_length)
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

void my92xx_setChannel(driver_descriptor_t* descriptor, uint8_t channel, uint16_t value) {
    if (channel < descriptor->numChannels) 
    {
        descriptor->channel_values[channel] = value;
    }
}