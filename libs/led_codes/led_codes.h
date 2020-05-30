/*
 * LED Codes Library
 * 
 * Copyright 2018-2020 José A. Jiménez (@RavenSystem)
 *
 */

#pragma once

typedef struct blinking_params_t {
    uint8_t times;
    uint8_t duration;
} blinking_params_t;

#define GENERIC_ERROR               (blinking_params_t){6,0}
#define SENSOR_ERROR                (blinking_params_t){8,0}
#define WIFI_CONNECTED              (blinking_params_t){1,2}
#define IDENTIFY_ACCESSORY          (blinking_params_t){1,3}
#define RESTART_DEVICE              (blinking_params_t){2,2}
#define WIFI_CONFIG_RESET           (blinking_params_t){2,0}
#define EXTRA_CONFIG_RESET          (blinking_params_t){2,1}
#define FUNCTION_A                  (blinking_params_t){1,0}
#define FUNCTION_B                  (blinking_params_t){2,0}
#define FUNCTION_C                  (blinking_params_t){3,0}
#define FUNCTION_D                  (blinking_params_t){4,0}
#define FUNCTION_E                  (blinking_params_t){5,0}
#define FUNCTION_F                  (blinking_params_t){6,0}

int led_create(const uint8_t gpio, const bool inverted);
void led_destroy(const uint8_t gpio);

void led_code(const uint8_t gpio, blinking_params_t blinking_params);
