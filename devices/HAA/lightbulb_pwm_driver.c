/**
MIT License

Copyright (c) 2020 Alexander Lougovski

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <stdlib.h>
#include <cJSON.h>
#include "lightbulb_pwm_driver.h"
#include <string.h>
#include <multipwm/multipwm.h>
#include "header.h"

void haa_pwm_init(driver_interface_t** interface, const cJSON * const object, lightbulb_channels_t* channels)
{
    *interface = malloc( sizeof(driver_interface_t));
    if(interface != NULL)
    {
        pwm_info_t* pwm_info = malloc(sizeof(pwm_info_t));
        if(pwm_info != NULL)
        {
            memset(pwm_info, 0, sizeof(*pwm_info));
            multipwm_init(pwm_info);
            pwm_info->channels = 0;
            (*interface)->driver_info = (void*) pwm_info; 
            (*interface)->family = DRIVER_PWM;
            (*interface)->numChips = 1;
            (*interface)->updater = haa_pwm_send;
            
            // PWM frequency
            if (cJSON_GetObjectItemCaseSensitive(object, PWM_FREQ) != NULL) 
            {
                uint16_t pwm_freq = (uint16_t) cJSON_GetObjectItemCaseSensitive(object, PWM_FREQ)->valuedouble;
                if (pwm_freq > 0) 
                {
                    multipwm_set_freq(pwm_info, pwm_freq);
                }
            }
            
            if (cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_R) != NULL && pwm_info->channels < MULTIPWM_MAX_CHANNELS) {
                channels->r = pwm_info->channels++;
                multipwm_set_pin(pwm_info, channels->r, (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_R)->valuedouble);
            }

            if (cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_G) != NULL && pwm_info->channels < MULTIPWM_MAX_CHANNELS) {
                channels->g = pwm_info->channels++;
                multipwm_set_pin(pwm_info, channels->g, (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_G)->valuedouble);
            }

            if (cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_B) != NULL && pwm_info->channels < MULTIPWM_MAX_CHANNELS) {
                channels->b = pwm_info->channels++;
                multipwm_set_pin(pwm_info, channels->b, (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_B)->valuedouble);
            }

            if (cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_W) != NULL && pwm_info->channels < MULTIPWM_MAX_CHANNELS) {
                channels->w = pwm_info->channels++;
                multipwm_set_pin(pwm_info, channels->w, (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_W)->valuedouble);
            }

            if (cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_CW) != NULL && pwm_info->channels < MULTIPWM_MAX_CHANNELS) {
                channels->cw = pwm_info->channels++;
                multipwm_set_pin(pwm_info, channels->cw, (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_CW)->valuedouble);
            }

            if (cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_WW) != NULL && pwm_info->channels < MULTIPWM_MAX_CHANNELS) {
                channels->ww = pwm_info->channels++;
                multipwm_set_pin(pwm_info, channels->ww, (uint8_t) cJSON_GetObjectItemCaseSensitive(object, LIGHTBULB_PWM_GPIO_WW)->valuedouble);
            }
            (*interface)->numChannelsPerChip = pwm_info->channels;
        }
    }

}

void haa_pwm_send(const void* driver_info, const uint16_t* multipwm_duty)
{
    if(driver_info != NULL)
    {
        pwm_info_t* pwm_info = (pwm_info_t*) driver_info;   
        multipwm_stop(pwm_info);
        for (uint8_t i = 0; i < pwm_info->channels; i++)
        {
            multipwm_set_duty(pwm_info, i, multipwm_duty[i]);
        }
        multipwm_start(pwm_info);
    }
}