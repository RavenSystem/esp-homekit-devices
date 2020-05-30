/*
 * Advanced Button Manager
 *
 * Copyright 2019-2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

/*
 * Based on Button library by Maxim Kulkin (@MaximKulkin), licensed under the MIT License.
 * https://github.com/maximkulkin/esp-homekit-demo/blob/master/examples/button/button.c
 */

#ifndef __ADVANCED_BUTTON__
#define __ADVANCED_BUTTON__

#define INVSINGLEPRESS_TYPE         (0)
#define SINGLEPRESS_TYPE            (1)
#define DOUBLEPRESS_TYPE            (2)
#define LONGPRESS_TYPE              (3)
#define VERYLONGPRESS_TYPE          (4)
#define HOLDPRESS_TYPE              (5)

typedef void (*button_callback_fn)(uint8_t gpio, void *args, uint8_t param);

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
int adv_button_register_callback_fn(const uint8_t gpio, const button_callback_fn callback, const uint8_t button_callback_type, void *args, const uint8_t param);

#endif // __ADVANCED_BUTTON__
