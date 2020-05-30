/*
 * Advanced Button Manager Example
 *
 * Copyright 2019-2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#include <stdio.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>

#include "adv_button.h"


#ifndef BUTTON_GPIO
#define BUTTON_GPIO     0
#endif

#ifndef TOGGLE_GPIO
#define TOGGLE_GPIO     2
#endif

void singlepress_callback(const uint8_t gpio, void *args, const uint8_t param) {
    printf(">>>>> Example button: Single Press function called using GPIO->%i\n", gpio);
}

void doublepress_callback(const uint8_t gpio, void *args, const uint8_t param) {
    printf(">>>>> Example button: Double Press function called using GPIO->%i\n", gpio);
}

void longpress_callback(const uint8_t gpio, void *args, const uint8_t param) {
    printf(">>>>> Example button: Long Press function called using GPIO->%i\n", gpio);
}

void verylongpress_callback(const uint8_t gpio, void *args, const uint8_t param) {
    printf(">>>>> Example button: Very Long Press function called using GPIO->%i\n", gpio);
}

void holdpress_callback(const uint8_t gpio, void *args, const uint8_t param) {
    printf(">>>>> Example button: Hold Press function called using GPIO->%i\n", gpio);
}

void toggle_callback(const uint8_t gpio, void *args, const uint8_t param) {
    printf(">>>>> Example button: Toggle function called using GPIO->%i\n", gpio);
}

void user_init(void) {
    uart_set_baud(0, 115200);
    
    printf("\n>>>>> ADV BUTTON EXAMPLE\n\n");
    
    adv_button_set_evaluate_delay(10);
    
    adv_button_create(BUTTON_GPIO, true, false);
    
    adv_button_register_callback_fn(BUTTON_GPIO, singlepress_callback, SINGLEPRESS_TYPE, NULL, 0);
    adv_button_register_callback_fn(BUTTON_GPIO, doublepress_callback, DOUBLEPRESS_TYPE, NULL, 0);
    adv_button_register_callback_fn(BUTTON_GPIO, longpress_callback, LONGPRESS_TYPE, NULL, 0);
    adv_button_register_callback_fn(BUTTON_GPIO, verylongpress_callback, VERYLONGPRESS_TYPE, NULL, 0);
    adv_button_register_callback_fn(BUTTON_GPIO, holdpress_callback, HOLDPRESS_TYPE, NULL, 0);
    
    adv_button_create(TOGGLE_GPIO, true, false);
    adv_button_register_callback_fn(TOGGLE_GPIO, toggle_callback, INVSINGLEPRESS_TYPE, NULL, 0);    // Low
    adv_button_register_callback_fn(TOGGLE_GPIO, toggle_callback, SINGLEPRESS_TYPE, NULL, 0);       // High
}
