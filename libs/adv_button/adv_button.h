/*
 * Advanced Button Manager
 *
 * Copyright 2018-2019 José A. Jiménez (@RavenSystem)
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

/*
 * Based on Button library by Maxim Kulkin (@MaximKulkin), licensed under the MIT License.
 * https://github.com/maximkulkin/esp-homekit-demo/blob/master/examples/button/button.c
 */

#ifndef __ADVANCED_BUTTON__
#define __ADVANCED_BUTTON__

typedef void (*button_callback_fn)(uint8_t gpio, void *args);

void adv_button_set_evaluate_delay(const uint8_t new_delay);
int adv_button_create(const uint8_t gpio, const bool pullup_resistor, const bool inverted);
void adv_button_destroy(const uint8_t gpio);
void adv_button_set_disable_time();

/*
 * Button callback types:
 * 0 Single press (inverted to 1)
 * 1 Single press
 * 2 Double press
 * 3 Long press
 * 4 Very long press
 * 5 Hold press
 */
int adv_button_register_callback_fn(const uint8_t gpio, const button_callback_fn callback, const uint8_t button_callback_type, void *args);

#endif // __ADVANCED_BUTTON__
