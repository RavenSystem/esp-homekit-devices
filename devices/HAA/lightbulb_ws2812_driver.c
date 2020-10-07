/** Based on the example from here 
 * https://github.com/SuperHouse/esp-open-rtos/blob/master/examples/ws2812_rainbow/ws2812_rainbow.c
*/
#include <stdlib.h>
#include <cJSON.h>
#include "lightbulb_ws2812_driver.h"
#include <string.h>
#include "header.h"
#include <FreeRTOS.h>
#include <task.h>
#include <ws2812/ws2812.h>


void haa_ws2812_init(driver_interface_t** interface, const cJSON * const object, lightbulb_channels_t* channels)
{
    if (cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_DRIVER_GPIO_DI) != NULL) 
    {
        *interface = malloc( sizeof(driver_interface_t));
        if(interface != NULL)
        {
            ws2812_driver_info_t* descriptor = malloc( sizeof(ws2812_driver_info_t));
            if(descriptor != NULL)
            {
                (*interface)->driver_info = (void*) descriptor; 
                (*interface)->family = DRIVER_WS2812;
                descriptor->pin = (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_DRIVER_GPIO_DI)->valuedouble;
                (*interface)->updater = haa_ws2812_send;
                
                descriptor->numPixels = 1;
                if (cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_DRIVER_NUM_CHIPS) != NULL)
                {
                    descriptor->numPixels = (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_DRIVER_NUM_CHIPS)->valuedouble;
                }
                (*interface)->numChips = descriptor->numPixels;

                descriptor->numChannelsPerPixel = 3;
                (*interface)->numChannelsPerChip = descriptor->numChannelsPerPixel;

                descriptor->converationFactor = (float)(UINT8_MAX) / (float)(UINT16_MAX);
                channels->r = 0;
                channels->g = 1;
                channels->b = 2;
                /*
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
                */
                // Init GPIO
                gpio_enable(descriptor->pin, GPIO_OUTPUT);
                gpio_write(descriptor->pin, false);

            }
        }
    }

}
void haa_ws2812_send(const void* driver_info, const uint16_t* multipwm_duty)
{
    if(driver_info != NULL)
    {
        ws2812_driver_info_t* descriptor = (ws2812_driver_info_t*) driver_info;
        ws2812_rgb_t color = WS2812_RGB( (uint8_t)(multipwm_duty[0] * descriptor->converationFactor), 
                                        (uint8_t)(multipwm_duty[1] * descriptor->converationFactor), 
                                        (uint8_t)(multipwm_duty[2] * descriptor->converationFactor));
        // Start a data sequence (disables interrupts)
        ws2812_seq_start();

        for (uint8_t i = 0; i < descriptor->numPixels; i++) 
        {
            // send a color
            ws2812_seq_rgb(descriptor->pin, color.num);


        }
        // End the data sequence, display colors (interrupts are restored)
        ws2812_seq_end();
    }
}