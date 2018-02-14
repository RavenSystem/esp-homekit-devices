#pragma once

typedef struct blinking_params {
    uint8_t times;
    uint8_t duration;
} blinking_params;

#define GENERIC_ERROR               (blinking_params){6,0}
#define SENSOR_ERROR                (blinking_params){5,0}
#define WIFI_CONNECTED              (blinking_params){1,2}
#define IDENTIFY_ACCESSORY          (blinking_params){1,3}
#define RESTART_DEVICE              (blinking_params){2,2}
#define WIFI_CONFIG_RESET           (blinking_params){2,0}
#define EXTRA_CONFIG_RESET          (blinking_params){2,1}
#define FUNCTION_A                  (blinking_params){1,0}
#define FUNCTION_B                  (blinking_params){2,0}
#define FUNCTION_C                  (blinking_params){3,0}
#define FUNCTION_D                  (blinking_params){4,0}

void led_code(uint8_t gpio, blinking_params params);
