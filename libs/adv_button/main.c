/*
 * Advanced Button Manager Example
 *
 * Copyright 2018 José A. Jiménez (@RavenSystem)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>

#include "adv_button.h"


#ifndef BUTTON_GPIO
#define BUTTON_GPIO     0
#endif

void singlepress_callback(uint8_t gpio) {
    printf(">>>>> Example button: Single Press function called using GPIO->%i\n", gpio);
}

void doublepress_callback(uint8_t gpio) {
    printf(">>>>> Example button: Double Press function called using GPIO->%i\n", gpio);
}

void longpress_callback(uint8_t gpio) {
    printf(">>>>> Example button: Long Press function called using GPIO->%i\n", gpio);
}

void verylongpress_callback(uint8_t gpio) {
    printf(">>>>> Example button: Very Long Press function called using GPIO->%i\n", gpio);
}

void holdpress_callback(uint8_t gpio) {
    printf(">>>>> Example button: Hold Press function called using GPIO->%i\n", gpio);
}

void user_init(void) {
    uart_set_baud(0, 115200);
    
    printf("\n>>>>> ADV BUTTON EXAMPLE\n\n");
    
    adv_button_create(BUTTON_GPIO, true);
    
    adv_button_register_callback_fn(BUTTON_GPIO, singlepress_callback, 1);
    adv_button_register_callback_fn(BUTTON_GPIO, doublepress_callback, 2);
    adv_button_register_callback_fn(BUTTON_GPIO, longpress_callback, 3);
    adv_button_register_callback_fn(BUTTON_GPIO, verylongpress_callback, 4);
    adv_button_register_callback_fn(BUTTON_GPIO, holdpress_callback, 5);
}
