/*
 * Home Accessory Architect
 *
 * v0.8.9
 * 
 * Copyright 2019 José Antonio Jiménez Campos (@RavenSystem)
 *  
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#include <stdio.h>
#include <string.h>
#include <esp/uart.h>
//#include <esp8266.h>
//#include <FreeRTOS.h>
//#include <espressif/esp_wifi.h>
#include <espressif/esp_common.h>
#include <rboot-api.h>
#include <sysparam.h>
//#include <task.h>
#include <math.h>

//#include <etstimer.h>
#include <esplibs/libmain.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <adv_button.h>

#include <multipwm/multipwm.h>

#include <dht.h>
#include <ds18b20/ds18b20.h>

#include <cJSON.h>

// Version
#define FIRMWARE_VERSION                    "0.8.9"
#define FIRMWARE_VERSION_OCTAL              001011      // Matches as example: firmware_revision 2.3.8 = 02.03.10 (octal) = config_number 020310

// Characteristic types (ch_type)
#define CH_TYPE_BOOL                        0
#define CH_TYPE_INT8                        1
#define CH_TYPE_INT32                       2
#define CH_TYPE_FLOAT                       3

// Auto-off types (type)
#define TYPE_ON                             0
#define TYPE_LOCK                           1
#define TYPE_SENSOR                         2
#define TYPE_SENSOR_BOOL                    3
#define TYPE_VALVE                          4
#define TYPE_LIGHTBULB                      5
#define TYPE_GARAGE_DOOR                    6

// Button Events
#define SINGLEPRESS_EVENT                   0
#define DOUBLEPRESS_EVENT                   1
#define LONGPRESS_EVENT                     2

// Initial states
#define INIT_STATE_FIXED_INPUT              4
#define INIT_STATE_LAST                     5
#define INIT_STATE_INV_LAST                 6
#define INIT_STATE_LAST_STR                 "{\"s\":5}"

// JSON
#define GENERAL_CONFIG                      "c"
#define CUSTOM_HOSTNAME                     "n"
#define LOG_OUTPUT                          "o"
#define ALLOWED_SETUP_MODE_TIME             "m"
#define STATUS_LED_GPIO                     "l"
#define INVERTED                            "i"
#define BUTTON_FILTER                       "f"
#define PWM_FREQ                            "q"
#define ENABLE_HOMEKIT_SERVER               "h"
#define ALLOW_INSECURE_CONNECTIONS          "u"
#define ACCESSORIES                         "a"
#define BUTTONS_ARRAY                       "b"
#define FIXED_BUTTONS_ARRAY_0               "f0"
#define FIXED_BUTTONS_ARRAY_1               "f1"
#define FIXED_BUTTONS_ARRAY_2               "f2"
#define FIXED_BUTTONS_ARRAY_3               "f3"
#define FIXED_BUTTONS_ARRAY_4               "f4"
#define FIXED_BUTTONS_ARRAY_5               "f5"
#define FIXED_BUTTONS_ARRAY_6               "f6"
#define FIXED_BUTTONS_ARRAY_7               "f7"
#define FIXED_BUTTONS_ARRAY_8               "f8"
#define BUTTON_PRESS_TYPE                   "t"
#define PULLUP_RESISTOR                     "p"
#define VALUE                               "v"
#define DIGITAL_OUTPUTS_ARRAY               "r"
#define MANAGE_OTHERS_ACC_ARRAY             "m"
#define ACCESSORY_INDEX                     "g"
#define AUTOSWITCH_TIME                     "i"
#define PIN_GPIO                            "g"
#define INITIAL_STATE                       "s"
#define KILL_SWITCH                         "k"

#define VALVE_SYSTEM_TYPE                   "w"
#define VALVE_MAX_DURATION                  "d"
#define VALVE_DEFAULT_MAX_DURATION          3600

#define THERMOSTAT_TYPE                     "w"
#define THERMOSTAT_TYPE_HEATER              1
#define THERMOSTAT_TYPE_COOLER              2
#define THERMOSTAT_TYPE_HEATERCOOLER        3
#define THERMOSTAT_MIN_TEMP                 "m"
#define THERMOSTAT_DEFAULT_MIN_TEMP         10
#define THERMOSTAT_MAX_TEMP                 "x"
#define THERMOSTAT_DEFAULT_MAX_TEMP         38
#define THERMOSTAT_DEADBAND                 "d"
#define THERMOSTAT_POLL_PERIOD              "j"
#define THERMOSTAT_DEFAULT_POLL_PERIOD      30
#define THERMOSTAT_MODE_OFF                 0
#define THERMOSTAT_MODE_IDLE                1
#define THERMOSTAT_MODE_HEATER              2
#define THERMOSTAT_MODE_COOLER              3
#define THERMOSTAT_TARGET_MODE_AUTO         0
#define THERMOSTAT_TARGET_MODE_HEATER       1
#define THERMOSTAT_TARGET_MODE_COOLER       2
#define THERMOSTAT_ACTION_TOTAL_OFF         0
#define THERMOSTAT_ACTION_HEATER_IDLE       1
#define THERMOSTAT_ACTION_COOLER_IDLE       2
#define THERMOSTAT_ACTION_HEATER_ON         3
#define THERMOSTAT_ACTION_COOLER_ON         4
#define THERMOSTAT_ACTION_SENSOR_ERROR      5
#define THERMOSTAT_TEMP_UP                  0
#define THERMOSTAT_TEMP_DOWN                1

#define TEMPERATURE_SENSOR_GPIO             "g"
#define TEMPERATURE_SENSOR_TYPE             "n"
#define TEMPERATURE_OFFSET                  "z"
#define HUMIDITY_OFFSET                     "h"

#define LIGHTBULB_PWM_GPIO_R                "r"
#define LIGHTBULB_PWM_GPIO_G                "g"
#define LIGHTBULB_PWM_GPIO_B                "v"
#define LIGHTBULB_PWM_GPIO_W                "w"
#define LIGHTBULB_FACTOR_R                  "fr"
#define LIGHTBULB_FACTOR_G                  "fg"
#define LIGHTBULB_FACTOR_B                  "fv"
#define LIGHTBULB_FACTOR_W                  "fw"
#define RGBW_PERIOD                         10
#define RGBW_STEP                           "p"
#define RGBW_STEP_DEFAULT                   1024
#define RGBW_SET_DELAY                      111
#define PWM_SCALE                           (UINT16_MAX - 1)
#define COLOR_TEMP_MIN                      71
#define COLOR_TEMP_MAX                      400
#define LIGHTBULB_BRIGHTNESS_UP             0
#define LIGHTBULB_BRIGHTNESS_DOWN           1
#define AUTODIMMER_DELAY                    500
#define AUTODIMMER_TASK_DELAY               "d"
#define AUTODIMMER_TASK_DELAY_DEFAULT       1000
#define AUTODIMMER_TASK_STEP                "e"
#define AUTODIMMER_TASK_STEP_DEFAULT        10

#define GARAGE_DOOR_OPENED                  0
#define GARAGE_DOOR_CLOSED                  1
#define GARAGE_DOOR_OPENING                 2
#define GARAGE_DOOR_CLOSING                 3
#define GARAGE_DOOR_STOPPED                 4
#define GARAGE_DOOR_WORKING_TIME            "d"
#define GARAGE_DOOR_DEFAULT_WORKING_TIME    30
#define GARAGE_DOOR_TIME_MARGIN             "e"
#define GARAGE_DOOR_DEFAULT_TIME_MARGIN     0

#define MAX_ACTIONS                         10   // from 0 to ...
#define COPY_ACTIONS                        "a"
#define SYSTEM_ACTIONS_ARRAY                "s"
#define SYSTEM_ACTION                       "a"
#define SYSTEM_ACTION_REBOOT                0
#define SYSTEM_ACTION_SETUP_MODE            1
#define SYSTEM_ACTION_OTA_UPDATE            2

#define ACCESSORY_TYPE                      "t"
#define ACC_TYPE_SWITCH                     1
#define ACC_TYPE_OUTLET                     2
#define ACC_TYPE_BUTTON                     3
#define ACC_TYPE_LOCK                       4
#define ACC_TYPE_CONTACT_SENSOR             5
#define ACC_TYPE_OCCUPANCY_SENSOR           6
#define ACC_TYPE_LEAK_SENSOR                7
#define ACC_TYPE_SMOKE_SENSOR               8
#define ACC_TYPE_CARBON_MONOXIDE_SENSOR     9
#define ACC_TYPE_CARBON_DIOXIDE_SENSOR      10
#define ACC_TYPE_FILTER_CHANGE_SENSOR       11
#define ACC_TYPE_MOTION_SENSOR              12
#define ACC_TYPE_WATER_VALVE                20
#define ACC_TYPE_THERMOSTAT                 21
#define ACC_TYPE_TEMP_SENSOR                22
#define ACC_TYPE_HUM_SENSOR                 23
#define ACC_TYPE_TH_SENSOR                  24
#define ACC_TYPE_LIGHTBULB                  30
#define ACC_TYPE_GARAGE_DOOR                40

#define ACC_CREATION_DELAY                  30
#define EXIT_EMERGENCY_SETUP_MODE_TIME      2400
#define SETUP_MODE_ACTIVATE_COUNT           "z"
#define SETUP_MODE_DEFAULT_ACTIVATE_COUNT   8

#define ACCESSORIES_WITHOUT_BRIDGE          4   // Max number of accessories before using a bridge

#define DEBUG(message, ...)                 printf("HAA > %s: " message "\n", __func__, ##__VA_ARGS__);
#define INFO(message, ...)                  printf("HAA > " message "\n", ##__VA_ARGS__);
#define ERROR(message, ...)                 printf("HAA ! " message "\n", ##__VA_ARGS__);

#define FREEHEAP()                          printf("HAA > Free Heap: %d\n", xPortGetFreeHeapSize())

#ifdef HAA_DEBUG
ETSTimer free_heap_timer;
void free_heap_watchdog() {
    FREEHEAP();
}
#endif

typedef struct _autoswitch_params {
    uint8_t gpio;
    bool value;
    double time;
} autoswitch_params_t;

typedef struct _autooff_setter_params {
    homekit_characteristic_t *ch;
    uint8_t type;
    double time;
} autooff_setter_params_t;

typedef struct _last_state {
    char *id;
    homekit_characteristic_t *ch;
    uint8_t ch_type;
    struct _last_state *next;
} last_state_t;

typedef struct _ch_group {
    uint8_t accessory;
    uint8_t acc_type;
    
    homekit_characteristic_t *ch0;
    homekit_characteristic_t *ch1;
    homekit_characteristic_t *ch2;
    homekit_characteristic_t *ch3;
    homekit_characteristic_t *ch4;
    homekit_characteristic_t *ch5;
    homekit_characteristic_t *ch6;
    homekit_characteristic_t *ch_child;
    homekit_characteristic_t *ch_sec;
    
    float num0;
    float num1;
    float num2;
    
    ETSTimer *timer;
    
    struct _ch_group *next;
} ch_group_t;

typedef struct _lightbulb_group {
    homekit_characteristic_t *ch0;

    uint8_t pwm_r;
    uint8_t pwm_g;
    uint8_t pwm_b;
    uint8_t pwm_w;
    
    uint16_t target_r;
    uint16_t target_g;
    uint16_t target_b;
    uint16_t target_w;
    
    float factor_r;
    float factor_g;
    float factor_b;
    float factor_w;
    
    uint16_t step;
    
    ETSTimer autodimmer_timer;
    uint8_t autodimmer;
    bool armed_autodimmer;
    uint16_t autodimmer_task_delay;
    uint8_t autodimmer_task_step;
    
    struct _lightbulb_group *next;
} lightbulb_group_t;

int8_t setup_mode_toggle_counter = INT8_MIN;
int8_t setup_mode_toggle_counter_max = SETUP_MODE_DEFAULT_ACTIVATE_COUNT;
uint8_t led_gpio = 255;
uint16_t setup_mode_time = 0;
ETSTimer *setup_mode_toggle_timer;
ETSTimer save_states_timer;
bool used_gpio[18];
bool led_inverted = true;
bool enable_homekit_server = true;
bool allow_insecure = false;

bool setpwm_is_running = false;
bool setpwm_bool_semaphore = true;
ETSTimer *pwm_timer;
pwm_info_t *pwm_info;
uint16_t multipwm_duty[MULTIPWM_MAX_CHANNELS];
uint16_t pwm_freq = 0;

char name_value[11];
char serial_value[13];

last_state_t *last_states = NULL;
ch_group_t *ch_groups = NULL;
lightbulb_group_t *lightbulb_groups = NULL;

ch_group_t *ch_group_find(homekit_characteristic_t *ch) {
    ch_group_t *ch_group = ch_groups;
    while (ch_group &&
           ch_group->ch0 != ch &&
           ch_group->ch1 != ch &&
           ch_group->ch2 != ch &&
           ch_group->ch3 != ch &&
           ch_group->ch4 != ch &&
           ch_group->ch5 != ch &&
           ch_group->ch6 != ch &&
           ch_group->ch_child != ch &&
           ch_group->ch_sec != ch) {
        ch_group = ch_group->next;
    }

    return ch_group;
}

ch_group_t *ch_group_find_by_acc(uint8_t accessory) {
    ch_group_t *ch_group = ch_groups;
    while (ch_group &&
           ch_group->accessory != accessory) {
        ch_group = ch_group->next;
    }

    return ch_group;
}

lightbulb_group_t *lightbulb_group_find(homekit_characteristic_t *ch) {
    lightbulb_group_t *lightbulb_group = lightbulb_groups;
    while (lightbulb_group &&
           lightbulb_group->ch0 != ch) {
        lightbulb_group = lightbulb_group->next;
    }

    return lightbulb_group;
}

void led_task(void *pvParameters) {
    const uint8_t times = (int) pvParameters;
    
    for (uint8_t i=0; i<times; i++) {
        gpio_write(led_gpio, true ^ led_inverted);
        vTaskDelay(30 / portTICK_PERIOD_MS);
        gpio_write(led_gpio, false ^ led_inverted);
        vTaskDelay(130 / portTICK_PERIOD_MS);
    }
    
    vTaskDelete(NULL);
}

void led_blink(const int blinks) {
    if (led_gpio != 255) {
        xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) blinks, 1, NULL);
    }
}

// -----
void reboot_task() {
    led_blink(5);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    sdk_system_restart();
}

void setup_mode_call(const uint8_t gpio, void *args, const uint8_t param) {
    INFO("Checking setup mode call");
    
    if (setup_mode_time == 0 || xTaskGetTickCountFromISR() < setup_mode_time * 1000 / portTICK_PERIOD_MS) {
        sysparam_set_bool("setup", true);
        xTaskCreate(reboot_task, "reboot_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    } else {
        ERROR("Setup mode not allowed after %i secs since boot. Repower device and try again", setup_mode_time);
    }
}

void setup_mode_toggle_upcount() {
    if (setup_mode_toggle_counter_max > 0) {
        setup_mode_toggle_counter++;
        sdk_os_timer_arm(setup_mode_toggle_timer, 1000, 0);
    }
}

void setup_mode_toggle() {
    if (setup_mode_toggle_counter >= setup_mode_toggle_counter_max) {
        setup_mode_call(0, NULL, 0);
    }
    
    setup_mode_toggle_counter = 0;
}

void exit_emergency_setup_mode_task() {
    vTaskDelay(EXIT_EMERGENCY_SETUP_MODE_TIME / portTICK_PERIOD_MS);
    
    INFO("Disarming Emergency Setup Mode");
    sysparam_set_bool("setup", false);

    vTaskDelete(NULL);
}

// -----
void save_states() {
    INFO("Saving states");
    last_state_t *last_state = last_states;
    sysparam_status_t status;
    
    while (last_state) {
        switch (last_state->ch_type) {
            case CH_TYPE_INT8:
                status = sysparam_set_int8(last_state->id, last_state->ch->value.int_value);
                break;
                
            case CH_TYPE_INT32:
                status = sysparam_set_int32(last_state->id, last_state->ch->value.int_value);
                break;
                
            case CH_TYPE_FLOAT:
                status = sysparam_set_int32(last_state->id, last_state->ch->value.float_value * 100);
                break;
                
            default:    // case CH_TYPE_BOOL
                status = sysparam_set_bool(last_state->id, last_state->ch->value.bool_value);
                break;
        }
        
        if (status != SYSPARAM_OK) {
            ERROR("Flash error saving states");
        }
        
        last_state = last_state->next;
    }
}

void save_states_callback() {
    sdk_os_timer_arm(&save_states_timer, 5000, 0);
}

void hkc_group_notify(homekit_characteristic_t *ch) {
    ch_group_t *ch_group = ch_group_find(ch);
    if (ch_group->ch0) {
        homekit_characteristic_notify(ch_group->ch0, ch_group->ch0->value);
    }
    
    if (ch_group->ch1) {
        homekit_characteristic_notify(ch_group->ch1, ch_group->ch1->value);
        if (ch_group->ch2) {
            homekit_characteristic_notify(ch_group->ch2, ch_group->ch2->value);
            if (ch_group->ch3) {
                homekit_characteristic_notify(ch_group->ch3, ch_group->ch3->value);
                if (ch_group->ch4) {
                    homekit_characteristic_notify(ch_group->ch4, ch_group->ch4->value);
                    if (ch_group->ch5) {
                        homekit_characteristic_notify(ch_group->ch5, ch_group->ch5->value);
                        if (ch_group->ch6) {
                            homekit_characteristic_notify(ch_group->ch6, ch_group->ch6->value);
                        }
                    }
                }
            }
        }
    }
    
    if (ch_group->ch_child) {
        homekit_characteristic_notify(ch_group->ch_child, ch_group->ch_child->value);
    }
    if (ch_group->ch_sec) {
        homekit_characteristic_notify(ch_group->ch_sec, ch_group->ch_sec->value);
    }
}

homekit_value_t hkc_getter(const homekit_characteristic_t *ch) {
    INFO("Getter");
    return ch->value;
}

void hkc_setter(homekit_characteristic_t *ch, const homekit_value_t value) {
    INFO("Setter");
    ch->value = value;
    hkc_group_notify(ch);
    
    save_states_callback();
}

void hkc_setter_with_setup(homekit_characteristic_t *ch, const homekit_value_t value) {
    hkc_setter(ch, value);
    
    setup_mode_toggle_upcount();
}

void hkc_autooff_setter_task(void *pvParameters);
void do_actions(cJSON *json_context, const uint8_t int_action);

// --- ON
void hkc_on_setter(homekit_characteristic_t *ch, const homekit_value_t value) {
    ch_group_t *ch_group = ch_group_find(ch);
    if (!ch_group->ch_sec || ch_group->ch_sec->value.bool_value) {
        if (ch->value.bool_value != value.bool_value) {
            led_blink(1);
            INFO("Setter ON");
            
            ch->value = value;
            
            cJSON *json_context = ch->context;
            do_actions(json_context, (uint8_t) ch->value.bool_value);
            
            if (ch->value.bool_value && cJSON_GetObjectItemCaseSensitive(json_context, AUTOSWITCH_TIME) != NULL) {
                const double autoswitch_time = cJSON_GetObjectItemCaseSensitive(json_context, AUTOSWITCH_TIME)->valuedouble;
                if (autoswitch_time > 0) {
                    autooff_setter_params_t *autooff_setter_params = malloc(sizeof(autooff_setter_params_t));
                    autooff_setter_params->ch = ch;
                    autooff_setter_params->type = TYPE_ON;
                    autooff_setter_params->time = autoswitch_time;
                    xTaskCreate(hkc_autooff_setter_task, "hkc_autooff_setter_task", configMINIMAL_STACK_SIZE, autooff_setter_params, 1, NULL);
                }
            }
            
            setup_mode_toggle_upcount();
            save_states_callback();
            
            if (ch_group->ch2) {
                if (value.bool_value) {
                    ch_group->ch2->value = ch_group->ch1->value;
                    sdk_os_timer_arm(ch_group->timer, 1000, 1);
                } else {
                    ch_group->ch2->value.int_value = 0;
                    sdk_os_timer_disarm(ch_group->timer);
                }
            }
        }
    }
    
    hkc_group_notify(ch_group->ch0);
}

void on_timer_worker(void *args) {
    homekit_characteristic_t *ch = args;
    ch_group_t *ch_group = ch_group_find(ch);
    
    ch_group->ch2->value.int_value--;
    
    if (ch_group->ch2->value.int_value == 0) {
        sdk_os_timer_disarm(ch_group->timer);
        
        hkc_on_setter(ch, HOMEKIT_BOOL(false));
    }
}

// --- LOCK MECHANISM
void hkc_lock_setter(homekit_characteristic_t *ch, const homekit_value_t value) {
    ch_group_t *ch_group = ch_group_find(ch);
    if (!ch_group->ch_sec || ch_group->ch_sec->value.bool_value) {
        if (ch->value.int_value != value.int_value) {
            led_blink(1);
            INFO("Setter LOCK");
            
            ch->value = value;
            ch_group->ch0->value = value;
            
            cJSON *json_context = ch->context;
            do_actions(json_context, (uint8_t) ch->value.int_value);
            
            if (ch->value.int_value == 0 && cJSON_GetObjectItemCaseSensitive(json_context, AUTOSWITCH_TIME) != NULL) {
                const double autoswitch_time = cJSON_GetObjectItemCaseSensitive(json_context, AUTOSWITCH_TIME)->valuedouble;
                if (autoswitch_time > 0) {
                    autooff_setter_params_t *autooff_setter_params = malloc(sizeof(autooff_setter_params_t));
                    autooff_setter_params->ch = ch;
                    autooff_setter_params->type = TYPE_LOCK;
                    autooff_setter_params->time = autoswitch_time;
                    xTaskCreate(hkc_autooff_setter_task, "hkc_autooff_setter_task", configMINIMAL_STACK_SIZE, autooff_setter_params, 1, NULL);
                }
            }
            
            setup_mode_toggle_upcount();
            save_states_callback();
        }
    }
    
    hkc_group_notify(ch_group->ch0);
}

// --- BUTTON EVENT
void button_event(const uint8_t gpio, void *args, const uint8_t event_type) {
    homekit_characteristic_t *ch = args;
    
    ch_group_t *ch_group = ch_group_find(ch);
    if (!ch_group->ch_child || ch_group->ch_child->value.bool_value) {
        led_blink(event_type + 1);
        INFO("Setter EVENT %i", event_type);
        
        homekit_characteristic_notify(ch, HOMEKIT_UINT8(event_type));
        
        cJSON *json_context = ch->context;
        do_actions(json_context, event_type);
        
        setup_mode_toggle_upcount();
    }
}

// --- SENSORS
void sensor_1(const uint8_t gpio, void *args, const uint8_t type) {
    homekit_characteristic_t *ch = args;

    if ((type == TYPE_SENSOR &&
        ch->value.int_value == 0) ||
        (type == TYPE_SENSOR_BOOL &&
        ch->value.bool_value == false)) {
        led_blink(1);
        INFO("Sensor ON");
        
        if (type == TYPE_SENSOR) {
            ch->value = HOMEKIT_UINT8(1);
        } else {
            ch->value = HOMEKIT_BOOL(true);
        }
        
        homekit_characteristic_notify(ch, ch->value);
        
        cJSON *json_context = ch->context;
        do_actions(json_context, 1);
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, AUTOSWITCH_TIME) != NULL) {
            const double autoswitch_time = cJSON_GetObjectItemCaseSensitive(json_context, AUTOSWITCH_TIME)->valuedouble;
            if (autoswitch_time > 0) {
                autooff_setter_params_t *autooff_setter_params = malloc(sizeof(autooff_setter_params_t));
                autooff_setter_params->ch = ch;
                autooff_setter_params->type = type;
                autooff_setter_params->time = autoswitch_time;
                xTaskCreate(hkc_autooff_setter_task, "hkc_autooff_setter_task", configMINIMAL_STACK_SIZE, autooff_setter_params, 1, NULL);
            }
        }
    }
}

void sensor_0(const uint8_t gpio, void *args, const uint8_t type) {
    homekit_characteristic_t *ch = args;

    if ((type == TYPE_SENSOR &&
        ch->value.int_value == 1) ||
        (type == TYPE_SENSOR_BOOL &&
        ch->value.bool_value == true)) {
        led_blink(1);
        INFO("Sensor OFF");
        
        if (type == TYPE_SENSOR) {
            ch->value = HOMEKIT_UINT8(0);
        } else {
            ch->value = HOMEKIT_BOOL(false);
        }
        
        homekit_characteristic_notify(ch, ch->value);
        
        cJSON *json_context = ch->context;
        do_actions(json_context, 0);
    }
}

// --- WATER VALVE
void hkc_valve_setter(homekit_characteristic_t *ch, const homekit_value_t value) {
    ch_group_t *ch_group = ch_group_find(ch);
    if (!ch_group->ch_sec || ch_group->ch_sec->value.bool_value) {
        if (ch->value.int_value != value.int_value) {
            led_blink(1);
            INFO("Setter VALVE");
            
            ch->value = value;
            ch_group->ch1->value = value;
            
            cJSON *json_context = ch->context;
            do_actions(json_context, (uint8_t) ch->value.int_value);
            
            if (ch->value.int_value == 1 && cJSON_GetObjectItemCaseSensitive(json_context, AUTOSWITCH_TIME) != NULL) {
                const double autoswitch_time = cJSON_GetObjectItemCaseSensitive(json_context, AUTOSWITCH_TIME)->valuedouble;
                if (autoswitch_time > 0) {
                    autooff_setter_params_t *autooff_setter_params = malloc(sizeof(autooff_setter_params_t));
                    autooff_setter_params->ch = ch;
                    autooff_setter_params->type = TYPE_VALVE;
                    autooff_setter_params->time = autoswitch_time;
                    xTaskCreate(hkc_autooff_setter_task, "hkc_autooff_setter_task", configMINIMAL_STACK_SIZE, autooff_setter_params, 1, NULL);
                }
            }
            
            setup_mode_toggle_upcount();
            save_states_callback();
            
            if (ch_group->ch3) {
                if (value.int_value == 0) {
                    ch_group->ch3->value.int_value = 0;
                    sdk_os_timer_disarm(ch_group->timer);
                } else {
                    ch_group->ch3->value = ch_group->ch2->value;
                    sdk_os_timer_arm(ch_group->timer, 1000, 1);
                }
            }
        }
    }
    
    hkc_group_notify(ch_group->ch0);
}

void valve_timer_worker(void *args) {
    homekit_characteristic_t *ch = args;
    ch_group_t *ch_group = ch_group_find(ch);
    
    ch_group->ch3->value.int_value--;
    
    if (ch_group->ch3->value.int_value == 0) {
        sdk_os_timer_disarm(ch_group->timer);
        
        hkc_valve_setter(ch, HOMEKIT_UINT8(0));
    }
}

// --- THERMOSTAT
void update_th(homekit_characteristic_t *ch, const homekit_value_t value) {
    ch_group_t *ch_group = ch_group_find(ch);
    if (!ch_group->ch_sec || ch_group->ch_sec->value.bool_value) {
        led_blink(1);
        INFO("Setter TH");
        
        ch->value = value;
        
        cJSON *json_context = ch->context;
        
        float temp_deadband = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND) != NULL) {
            temp_deadband = (float) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND)->valuedouble;
        }
        
        if (ch_group->ch1->value.bool_value) {
            const float mid_target_temp = (ch_group->ch5->value.float_value + ch_group->ch6->value.float_value) / 2;
            
            switch (ch_group->ch4->value.int_value) {
                case THERMOSTAT_TARGET_MODE_HEATER:
                    if (ch_group->ch3->value.int_value <= THERMOSTAT_MODE_IDLE) {
                        if (ch_group->ch0->value.float_value < (ch_group->ch5->value.float_value - temp_deadband)) {
                            ch_group->ch3->value.int_value = THERMOSTAT_MODE_HEATER;
                            do_actions(json_context, THERMOSTAT_ACTION_HEATER_ON);
                        }
                    } else if (ch_group->ch0->value.float_value >= ch_group->ch5->value.float_value) {
                        ch_group->ch3->value.int_value = THERMOSTAT_MODE_IDLE;
                        do_actions(json_context, THERMOSTAT_ACTION_HEATER_IDLE);
                    }
                    break;
                
                case THERMOSTAT_TARGET_MODE_COOLER:
                    if (ch_group->ch3->value.int_value <= THERMOSTAT_MODE_IDLE) {
                        if (ch_group->ch0->value.float_value > (ch_group->ch6->value.float_value + temp_deadband)) {
                            ch_group->ch3->value.int_value = THERMOSTAT_MODE_COOLER;
                            do_actions(json_context, THERMOSTAT_ACTION_COOLER_ON);
                        }
                    } else if (ch_group->ch0->value.float_value <= ch_group->ch6->value.float_value) {
                        ch_group->ch3->value.int_value = THERMOSTAT_MODE_IDLE;
                        do_actions(json_context, THERMOSTAT_ACTION_COOLER_IDLE);
                    }
                    break;
                
                default:    // case THERMOSTAT_TARGET_MODE_AUTO:
                    switch (ch_group->ch3->value.int_value) {
                        case THERMOSTAT_MODE_HEATER:
                            if (ch_group->ch0->value.float_value >= mid_target_temp) {
                                ch_group->ch3->value.int_value = THERMOSTAT_MODE_IDLE;
                                do_actions(json_context, THERMOSTAT_ACTION_HEATER_IDLE);
                            }
                            break;
                            
                        case THERMOSTAT_MODE_COOLER:
                            if (ch_group->ch0->value.float_value <= mid_target_temp) {
                                ch_group->ch3->value.int_value = THERMOSTAT_MODE_IDLE;
                                do_actions(json_context, THERMOSTAT_ACTION_COOLER_IDLE);
                            }
                            break;
                            
                        default:    // cases THERMOSTAT_MODE_IDLE, THERMOSTAT_MODE_OFF:
                            if (ch_group->ch0->value.float_value < ch_group->ch5->value.float_value) {
                                ch_group->ch3->value.int_value = THERMOSTAT_MODE_HEATER;
                                do_actions(json_context, THERMOSTAT_ACTION_HEATER_ON);
                            } else if (ch_group->ch0->value.float_value > ch_group->ch6->value.float_value) {
                                ch_group->ch3->value.int_value = THERMOSTAT_MODE_COOLER;
                                do_actions(json_context, THERMOSTAT_ACTION_COOLER_ON);
                            }
                            break;
                    }
                    break;
            }
            
        } else {
            ch_group->ch3->value.int_value = THERMOSTAT_MODE_OFF;
            do_actions(json_context, THERMOSTAT_ACTION_TOTAL_OFF);
        }
        
        save_states_callback();
    }
    
    hkc_group_notify(ch_group->ch0);
}

void hkc_th_target_setter(homekit_characteristic_t *ch, const homekit_value_t value) {
    setup_mode_toggle_upcount();
    
    update_th(ch, value);
}

void th_input(const uint8_t gpio, void *args, const uint8_t type) {
    homekit_characteristic_t *ch = args;
    ch_group_t *ch_group = ch_group_find(ch);
    if (!ch_group->ch_child || ch_group->ch_child->value.bool_value) {
        // Thermostat Type
        cJSON *json_context = ch->context;
        uint8_t th_type = THERMOSTAT_TYPE_HEATER;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_TYPE) != NULL) {
            th_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_TYPE)->valuedouble;
        }
        
        switch (type) {
            case 0:
                ch_group->ch1->value.bool_value = false;
                break;
                
            case 1:
                ch_group->ch1->value.bool_value = true;
                break;
                
            case 5:
                ch_group->ch1->value.bool_value = true;
                ch_group->ch4->value.int_value = 2;
                break;
                
            case 6:
                ch_group->ch1->value.bool_value = true;
                ch_group->ch4->value.int_value = 1;
                break;
                
            case 7:
                ch_group->ch1->value.bool_value = true;
                ch_group->ch4->value.int_value = 0;
                break;
                
            default:    // case 9:  // Cyclic
                if (ch_group->ch1->value.bool_value) {
                    if (th_type == THERMOSTAT_TYPE_HEATERCOOLER) {
                        if (ch_group->ch4->value.int_value > 0) {
                            ch_group->ch4->value.int_value--;
                        } else {
                            ch_group->ch1->value.bool_value = false;
                        }
                    } else {
                        ch_group->ch1->value.bool_value = false;
                    }
                } else {
                    ch_group->ch1->value.bool_value = true;
                    if (th_type == THERMOSTAT_TYPE_HEATERCOOLER) {
                        ch_group->ch4->value.int_value = THERMOSTAT_TARGET_MODE_COOLER;
                    }
                }
                break;
        }
        
        hkc_th_target_setter(ch_group->ch0, ch_group->ch0->value);
    }
}

void th_input_temp(const uint8_t gpio, void *args, const uint8_t type) {
    homekit_characteristic_t *ch = args;
    ch_group_t *ch_group = ch_group_find(ch);
    if (!ch_group->ch_child || ch_group->ch_child->value.bool_value) {
        cJSON *json_context = ch->context;
        
        float th_min_temp = THERMOSTAT_DEFAULT_MIN_TEMP;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MIN_TEMP) != NULL) {
            th_min_temp = (float) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MIN_TEMP)->valuedouble;
        }
        
        float th_max_temp = THERMOSTAT_DEFAULT_MAX_TEMP;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MAX_TEMP) != NULL) {
            th_max_temp = (float) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MAX_TEMP)->valuedouble;
        }
        
        float set_h_temp = ch_group->ch5->value.float_value;
        float set_c_temp = ch_group->ch6->value.float_value;
        if (type == THERMOSTAT_TEMP_UP) {
            set_h_temp += 0.5;
            set_c_temp += 0.5;
            if (set_h_temp > th_max_temp) {
                set_h_temp = th_max_temp;
            }
            if (set_c_temp > th_max_temp) {
                set_c_temp = th_max_temp;
            }
        } else {    // type == THERMOSTAT_TEMP_DOWN
            set_h_temp -= 0.5;
            set_c_temp -= 0.5;
            if (set_h_temp < th_min_temp) {
                set_h_temp = th_min_temp;
            }
            if (set_c_temp < th_min_temp) {
                set_c_temp = th_min_temp;
            }
        }
        
        ch_group->ch5->value.float_value = set_h_temp;
        ch_group->ch6->value.float_value = set_c_temp;
        
        update_th(ch_group->ch0, ch_group->ch0->value);
    }
}

// --- TEMPERATURE
void temperature_timer_worker(void *args) {
    homekit_characteristic_t *ch = args;
    ch_group_t *ch_group = ch_group_find(ch);
    
    cJSON *json_context = ch->context;
    
    const uint8_t sensor_gpio = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, TEMPERATURE_SENSOR_GPIO)->valuedouble;
    
    uint8_t sensor_type = 2;
    if (cJSON_GetObjectItemCaseSensitive(json_context, TEMPERATURE_SENSOR_TYPE) != NULL) {
        sensor_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, TEMPERATURE_SENSOR_TYPE)->valuedouble;
    }
    
    float temp_offset = 0;
    if (cJSON_GetObjectItemCaseSensitive(json_context, TEMPERATURE_OFFSET) != NULL) {
        temp_offset = (float) cJSON_GetObjectItemCaseSensitive(json_context, TEMPERATURE_OFFSET)->valuedouble;
    }
    
    float hum_offset = 0;
    if (cJSON_GetObjectItemCaseSensitive(json_context, HUMIDITY_OFFSET) != NULL) {
        hum_offset = (float) cJSON_GetObjectItemCaseSensitive(json_context, HUMIDITY_OFFSET)->valuedouble;
    }
    
    float humidity_value, temperature_value;
    bool get_temp = false;
    
    if (sensor_type != 3) {
        dht_sensor_type_t current_sensor_type = DHT_TYPE_DHT22; // sensor_type == 2
        
        if (sensor_type == 1) {
            current_sensor_type = DHT_TYPE_DHT11;
        } else if (sensor_type == 4) {
            current_sensor_type = DHT_TYPE_SI7021;
        }
        
        get_temp = dht_read_float_data(current_sensor_type, sensor_gpio, &humidity_value, &temperature_value);
    } else {    // sensor_type == 3
        ds18b20_addr_t ds18b20_addr[1];
        
        if (ds18b20_scan_devices(sensor_gpio, ds18b20_addr, 1) == 1) {
            float temps[1];
            ds18b20_measure_and_read_multi(sensor_gpio, ds18b20_addr, 1, temps);
            temperature_value = temps[0];
            humidity_value = 0.0;
            get_temp = true;
        }
    }
    
    //get_temp = true;          // Only for tests. Keep comment for releases
    //temperature_value = 21;   // Only for tests. Keep comment for releases
    
    if (get_temp) {
        if (ch_group->ch0) {
            temperature_value += temp_offset;
            if (temperature_value < -100) {
                temperature_value = -100;
            } else if (temperature_value > 200) {
                temperature_value = 200;
            }
            
            if (temperature_value != ch_group->ch0->value.float_value) {
                ch_group->ch0->value = HOMEKIT_FLOAT(temperature_value);
                
                if (ch_group->ch5) {
                    update_th(ch_group->ch0, ch_group->ch0->value);
                }
            }
        }
        
        if (ch_group->ch1 && !ch_group->ch5) {
            humidity_value += hum_offset;
            if (humidity_value < 0) {
                humidity_value = 0;
            } else if (humidity_value > 100) {
                humidity_value = 100;
            }

            if (humidity_value != ch_group->ch1->value.float_value) {
                ch_group->ch1->value = HOMEKIT_FLOAT(humidity_value);
            }
        }
        
        INFO("TEMP %g, HUM %g", temperature_value, humidity_value);
    } else {
        led_blink(5);
        ERROR("ERROR Sensor");
        
        if (ch_group->ch5) {
            ch_group->ch3->value = HOMEKIT_UINT8(THERMOSTAT_MODE_OFF);
            
            do_actions(json_context, THERMOSTAT_ACTION_SENSOR_ERROR);
        }
    }
    
    if (ch_group->ch1) {
        hkc_group_notify(ch_group->ch1);
    } else {
        hkc_group_notify(ch_group->ch0);
    }
}

// --- LIGHTBULBS
//http://blog.saikoled.com/post/44677718712/how-to-convert-from-hsi-to-rgb-white
void hsi2rgbw(float h, float s, float i, lightbulb_group_t *lightbulb_group) {
    while (h < 0) {
        h += 360.0F;
    }
    while (h >= 360) {
        h -= 360.0F;
    }
    
    h = 3.141592653F * h / 180.0F;
    s /= 100.0F;
    i /= 100.0F;
    s = s > 0 ? (s < 1 ? s : 1) : 0;
    i = i > 0 ? (i < 1 ? i : 1) : 0;
    
    uint32_t r, g, b;
    
    if (h < 2.094393334) {
        r = lightbulb_group->factor_r * PWM_SCALE * i / 3 * (1 + s * cos(h) / cos(1.047196667 - h));
        g = lightbulb_group->factor_g * PWM_SCALE * i / 3 * (1 + s * (1 - cos(h) / cos(1.047196667 - h)));
        b = lightbulb_group->factor_b * PWM_SCALE * i / 3 * (1 - s);
    } else if (h < 4.188786668) {
        h = h - 2.094393334;
        g = lightbulb_group->factor_g * PWM_SCALE * i / 3 * (1 + s * cos(h) / cos(1.047196667 - h));
        b = lightbulb_group->factor_b * PWM_SCALE * i / 3 * (1 + s * (1 - cos(h) / cos(1.047196667 - h)));
        r = lightbulb_group->factor_r * PWM_SCALE * i / 3 * (1 - s);
    } else {
        h = h - 4.188786668;
        b = lightbulb_group->factor_b * PWM_SCALE * i / 3 * (1 + s * cos(h) / cos(1.047196667 - h));
        r = lightbulb_group->factor_r * PWM_SCALE * i / 3 * (1 + s * (1 - cos(h) / cos(1.047196667 - h)));
        g = lightbulb_group->factor_g * PWM_SCALE * i / 3 * (1 - s);
    }
    const uint32_t w = lightbulb_group->factor_w * PWM_SCALE * i * (1 - s);
    
    lightbulb_group->target_r = ((r > PWM_SCALE) ? PWM_SCALE : r);
    lightbulb_group->target_g = ((g > PWM_SCALE) ? PWM_SCALE : g);
    lightbulb_group->target_b = ((b > PWM_SCALE) ? PWM_SCALE : b);
    lightbulb_group->target_w = ((w > PWM_SCALE) ? PWM_SCALE : w);
}

void multipwm_set_all() {
    multipwm_stop(pwm_info);
    for (uint8_t i = 0; i < pwm_info->channels; i++) {
        multipwm_set_duty(pwm_info, i, multipwm_duty[i]);
    }
    multipwm_start(pwm_info);
}

void rgbw_set_timer_worker() {
    if (!setpwm_bool_semaphore) {
        setpwm_bool_semaphore = true;
        
        uint8_t channels_to_set = pwm_info->channels;
        lightbulb_group_t *lightbulb_group = lightbulb_groups;
        
        while (lightbulb_group) {
            if (lightbulb_group->pwm_r != 255) {
                if (lightbulb_group->target_r - multipwm_duty[lightbulb_group->pwm_r] >= lightbulb_group->step) {
                    multipwm_duty[lightbulb_group->pwm_r] += lightbulb_group->step;
                } else if (multipwm_duty[lightbulb_group->pwm_r] - lightbulb_group->target_r >= lightbulb_group->step) {
                    multipwm_duty[lightbulb_group->pwm_r] -= lightbulb_group->step;
                } else {
                    multipwm_duty[lightbulb_group->pwm_r] = lightbulb_group->target_r;
                    channels_to_set--;
                }
            }
            
            if (lightbulb_group->pwm_g != 255) {
                if (lightbulb_group->target_g - multipwm_duty[lightbulb_group->pwm_g] >= lightbulb_group->step) {
                    multipwm_duty[lightbulb_group->pwm_g] += lightbulb_group->step;
                } else if (multipwm_duty[lightbulb_group->pwm_g] - lightbulb_group->target_g >= lightbulb_group->step) {
                    multipwm_duty[lightbulb_group->pwm_g] -= lightbulb_group->step;
                } else {
                    multipwm_duty[lightbulb_group->pwm_g] = lightbulb_group->target_g;
                    channels_to_set--;
                }
            }
            
            if (lightbulb_group->pwm_b != 255) {
                if (lightbulb_group->target_b - multipwm_duty[lightbulb_group->pwm_b] >= lightbulb_group->step) {
                    multipwm_duty[lightbulb_group->pwm_b] += lightbulb_group->step;
                } else if (multipwm_duty[lightbulb_group->pwm_b] - lightbulb_group->target_b >= lightbulb_group->step) {
                    multipwm_duty[lightbulb_group->pwm_b] -= lightbulb_group->step;
                } else {
                    multipwm_duty[lightbulb_group->pwm_b] = lightbulb_group->target_b;
                    channels_to_set--;
                }
            }
            
            if (lightbulb_group->pwm_w != 255) {
                if (lightbulb_group->target_w - multipwm_duty[lightbulb_group->pwm_w] >= lightbulb_group->step) {
                    multipwm_duty[lightbulb_group->pwm_w] += lightbulb_group->step;
                } else if (multipwm_duty[lightbulb_group->pwm_w] - lightbulb_group->target_w >= lightbulb_group->step) {
                    multipwm_duty[lightbulb_group->pwm_w] -= lightbulb_group->step;
                } else {
                    multipwm_duty[lightbulb_group->pwm_w] = lightbulb_group->target_w;
                    channels_to_set--;
                }
            }
            
            //INFO("RGBW -> %i, %i, %i, %i", multipwm_duty[lightbulb_group->pwm_r], multipwm_duty[lightbulb_group->pwm_g], multipwm_duty[lightbulb_group->pwm_g], multipwm_duty[lightbulb_group->pwm_w]);
            
            lightbulb_group = lightbulb_group->next;
            
            if (channels_to_set == 0) {
                INFO("Color established");
                sdk_os_timer_disarm(pwm_timer);
                setpwm_is_running = false;
                lightbulb_group = NULL;
            }
        }
        
        multipwm_set_all();

        setpwm_bool_semaphore = false;
    } else {
        ERROR("MISSED Color set");
    }
}

void hkc_rgbw_setter_delayed(void *args) {
    homekit_characteristic_t *ch = args;
    ch_group_t *ch_group = ch_group_find(ch);
    lightbulb_group_t *lightbulb_group = lightbulb_group_find(ch_group->ch0);
    
    if (ch_group->ch0->value.bool_value) {
        if (lightbulb_group->target_r == 0 &&
            lightbulb_group->target_g == 0 &&
            lightbulb_group->target_b == 0 &&
            lightbulb_group->target_w == 0) {
            setup_mode_toggle_upcount();
        }
        
        if (lightbulb_group->pwm_r != 255) {            // RGB/W
            hsi2rgbw(ch_group->ch2->value.float_value, ch_group->ch3->value.float_value, ch_group->ch1->value.int_value, lightbulb_group);
        } else if (lightbulb_group->pwm_b != 255) {     // Custom Color Temperature
            uint16_t target_color = 0;
            if (ch_group->ch2->value.int_value >= COLOR_TEMP_MAX - 5) {
                target_color = PWM_SCALE;
            } else if (ch_group->ch2->value.int_value > COLOR_TEMP_MIN + 1) { // Conversion based on @seritos curve
                target_color = PWM_SCALE * (((0.09 + sqrt(0.18 + (0.1352 * (ch_group->ch2->value.int_value - COLOR_TEMP_MIN - 1)))) / 0.0676) - 1) / 100;
            }
            const uint32_t w = lightbulb_group->factor_w * target_color * ch_group->ch1->value.int_value / 100;
            const uint32_t b = lightbulb_group->factor_b * (PWM_SCALE - target_color) * ch_group->ch1->value.int_value / 100;
            lightbulb_group->target_w = ((w > PWM_SCALE) ? PWM_SCALE : w);
            lightbulb_group->target_b = ((b > PWM_SCALE) ? PWM_SCALE : b);
        } else {                                        // One Color Dimmer
            const uint32_t w = lightbulb_group->factor_w * PWM_SCALE * ch_group->ch1->value.int_value / 100;
            lightbulb_group->target_w = ((w > PWM_SCALE) ? PWM_SCALE : w);
        }
    } else {
        lightbulb_group->autodimmer = 0;
        lightbulb_group->target_r = 0;
        lightbulb_group->target_g = 0;
        lightbulb_group->target_b = 0;
        lightbulb_group->target_w = 0;
        
        setup_mode_toggle_upcount();
    }
    
    INFO("Target RGBW = %i, %i, %i, %i", lightbulb_group->target_r, lightbulb_group->target_g, lightbulb_group->target_b, lightbulb_group->target_w);
    
    if (!setpwm_is_running) {
        sdk_os_timer_arm(pwm_timer, RGBW_PERIOD, true);
        setpwm_is_running = true;
    }
    
    cJSON *json_context = ch_group->ch0->context;
    do_actions(json_context, (uint8_t) ch_group->ch0->value.bool_value);
    
    hkc_group_notify(ch_group->ch0);
    
    save_states_callback();
}

void hkc_rgbw_setter(homekit_characteristic_t *ch, const homekit_value_t value) {
    ch_group_t *ch_group = ch_group_find(ch);
    if (ch_group->ch_sec && !ch_group->ch_sec->value.bool_value) {
        hkc_group_notify(ch_group->ch0);
        
    } else {
        ch->value = value;
        sdk_os_timer_arm(ch_group->timer, RGBW_SET_DELAY, false);
    }
}

void rgbw_brightness(const uint8_t gpio, void *args, const uint8_t type) {
    homekit_characteristic_t *ch = args;
    ch_group_t *ch_group = ch_group_find(ch);
    if (!(ch_group->ch_child && !ch_group->ch_child->value.bool_value)) {
        if (type == LIGHTBULB_BRIGHTNESS_UP) {
            if (ch->value.int_value + 10 < 100) {
                ch->value.int_value += 10;
            } else {
                ch->value.int_value = 100;
            }
        } else {    // type == LIGHTBULB_BRIGHTNESS_DOWN
            if (ch->value.int_value - 10 > 0) {
                ch->value.int_value -= 10;
            } else {
                ch->value.int_value = 0;
            }
        }
        
        hkc_rgbw_setter(ch, ch->value);
    }
}

void autodimmer_task(void *args) {
    INFO("AUTODimmer started");
    
    homekit_characteristic_t *ch = args;
    ch_group_t *ch_group = ch_group_find(ch);
    lightbulb_group_t *lightbulb_group = lightbulb_group_find(ch_group->ch0);
    
    lightbulb_group->autodimmer = 4 * 100 / lightbulb_group->autodimmer_task_step;
    while(lightbulb_group->autodimmer > 0) {
        lightbulb_group->autodimmer--;
        if (ch_group->ch1->value.int_value < 100) {
            if (ch_group->ch1->value.int_value + lightbulb_group->autodimmer_task_step < 100) {
                ch_group->ch1->value.int_value += lightbulb_group->autodimmer_task_step;
            } else {
                ch_group->ch1->value.int_value = 100;
            }
        } else {
            ch_group->ch1->value.int_value = lightbulb_group->autodimmer_task_step;
        }
        hkc_rgbw_setter(ch_group->ch1, ch_group->ch1->value);
        
        vTaskDelay(lightbulb_group->autodimmer_task_delay);
        
        if (ch_group->ch1->value.int_value == 100) {    // Double wait when brightness is 100%
            vTaskDelay(lightbulb_group->autodimmer_task_delay);
        }
    }
    
    INFO("AUTODimmer stopped");
    
    vTaskDelete(NULL);
}

void no_autodimmer_called(void *args) {
    homekit_characteristic_t *ch0 = args;
    lightbulb_group_t *lightbulb_group = lightbulb_group_find(ch0);
    lightbulb_group->armed_autodimmer = false;
    ch0->value.bool_value = !ch0->value.bool_value;
    hkc_rgbw_setter(ch0, ch0->value);
}

void autodimmer_call(homekit_characteristic_t *ch0, const homekit_value_t value) {
    lightbulb_group_t *lightbulb_group = lightbulb_group_find(ch0);
    if (lightbulb_group->autodimmer_task_step == 0 || (value.bool_value && lightbulb_group->autodimmer == 0)) {
        hkc_rgbw_setter(ch0, value);
    } else if (lightbulb_group->autodimmer > 0) {
        lightbulb_group->autodimmer = 0;
    } else {
        lightbulb_group->autodimmer = 0;
        if (lightbulb_group->armed_autodimmer) {
            lightbulb_group->armed_autodimmer = false;
            sdk_os_timer_disarm(&lightbulb_group->autodimmer_timer);
            xTaskCreate(autodimmer_task, "autodimmer_task", configMINIMAL_STACK_SIZE, (void*) ch0, 1, NULL);
        } else {
            sdk_os_timer_arm(&lightbulb_group->autodimmer_timer, AUTODIMMER_DELAY, 0);
            lightbulb_group->armed_autodimmer = true;
        }
    }
}

// --- GARAGE DOOR
void garage_door_stop(const uint8_t gpio, void *args, const uint8_t type) {
    homekit_characteristic_t *ch0 = args;
    ch_group_t *ch_group = ch_group_find(ch0);
    
    if (ch0->value.int_value == GARAGE_DOOR_OPENING || ch0->value.int_value == GARAGE_DOOR_CLOSING) {
        led_blink(1);
        INFO("GD stop");
        
        ch0->value.int_value = GARAGE_DOOR_STOPPED;
        
        sdk_os_timer_disarm(ch_group->timer);

        cJSON *json_context = ch0->context;
        do_actions(json_context, 10);
        
        hkc_group_notify(ch0);
    }
}

void garage_door_obstruction(const uint8_t gpio, void *args, const uint8_t type) {
    homekit_characteristic_t *ch = args;
    ch_group_t *ch_group = ch_group_find(ch);
    
    led_blink(1);
    INFO("GD obstruction: %i", type);
    
    ch_group->ch2->value.bool_value = (bool) type;
    
    hkc_group_notify(ch);
    
    cJSON *json_context = ch->context;
    do_actions(json_context, type + 8);
}

void garage_door_sensor(const uint8_t gpio, void *args, const uint8_t type) {
    homekit_characteristic_t *ch = args;
    ch_group_t *ch_group = ch_group_find(ch);
    
    led_blink(1);
    INFO("GD sensor: %i", type);
    
    ch->value.int_value = type;
    
    if (type > 1) {
        ch_group->ch1->value.int_value = type - 2;
        sdk_os_timer_arm(ch_group->timer, 1000, 1);
    } else {
        ch_group->ch1->value.int_value = type;
        sdk_os_timer_disarm(ch_group->timer);
        
        if (type == 0) {
            ch_group->num0 = ch_group->num1 - ch_group->num2;
        } else {
            ch_group->num0 = ch_group->num2;
        }
        
        if (ch_group->ch2->value.bool_value) {
            garage_door_obstruction(0, ch, 0);
        }
    }
    
    hkc_group_notify(ch);
    
    cJSON *json_context = ch->context;
    do_actions(json_context, type + 4);
}

void hkc_garage_door_setter(homekit_characteristic_t *ch1, const homekit_value_t value) {
    ch_group_t *ch_group = ch_group_find(ch1);
    if (!ch_group->ch2->value.bool_value && (!ch_group->ch_sec || ch_group->ch_sec->value.bool_value)) {
        uint8_t current_door_state = ch_group->ch0->value.int_value;
        if (current_door_state == GARAGE_DOOR_STOPPED) {
            current_door_state = ch_group->ch1->value.int_value;
        } else if (current_door_state > 1) {
            current_door_state -= 2;
        }
        
        if (value.int_value != current_door_state) {
            led_blink(1);
            INFO("Setter GD");
            
            ch1->value = value;

            cJSON *json_context = ch1->context;
            do_actions(json_context, (uint8_t) ch_group->ch0->value.int_value);
            
            if (value.int_value == GARAGE_DOOR_OPENED && cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_4) == NULL) {
                garage_door_sensor(0, ch_group->ch0, GARAGE_DOOR_OPENING);
            } else if (value.int_value == GARAGE_DOOR_CLOSED && cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_4) == NULL) {
                garage_door_sensor(0, ch_group->ch0, GARAGE_DOOR_CLOSING);
            }
            
            setup_mode_toggle_upcount();
        }
    }
    
    hkc_group_notify(ch_group->ch0);
}

void garage_door_timer_worker(void *args) {
    homekit_characteristic_t *ch = args;
    ch_group_t *ch_group = ch_group_find(ch);
    
    cJSON *json_context = ch->context;

    if (ch->value.int_value == GARAGE_DOOR_OPENING) {
        ch_group->num0++;

        if (ch_group->num0 >= ch_group->num1 - ch_group->num2 && cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2) == NULL) {
            sdk_os_timer_disarm(ch_group->timer);
            garage_door_sensor(0, ch, GARAGE_DOOR_OPENED);
            
        } else if (ch_group->num0 >= ch_group->num1 && cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2) != NULL) {
            sdk_os_timer_disarm(ch_group->timer);
            garage_door_obstruction(0, ch, 1);
        }
        
    } else {    // GARAGE_DOOR_CLOSING
        ch_group->num0--;
        
        if (ch_group->num0 <= ch_group->num2 && cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3) == NULL) {
            sdk_os_timer_disarm(ch_group->timer);
            garage_door_sensor(0, ch, GARAGE_DOOR_CLOSED);
            
        } else if (ch_group->num0 <= 0 && cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3) != NULL) {
            sdk_os_timer_disarm(ch_group->timer);
            garage_door_obstruction(0, ch, 1);
        }
    }
}

// --- DIGITAL INPUTS
void diginput(const uint8_t gpio, void *args, const uint8_t type) {
    homekit_characteristic_t *ch = args;
    
    ch_group_t *ch_group = ch_group_find(ch);
    if (!ch_group->ch_child || ch_group->ch_child->value.bool_value) {
        switch (type) {
            case TYPE_LOCK:
                if (ch->value.int_value == 1) {
                    hkc_lock_setter(ch, HOMEKIT_UINT8(0));
                } else {
                    hkc_lock_setter(ch, HOMEKIT_UINT8(1));
                }
                break;
                
            case TYPE_VALVE:
                if (ch->value.int_value == 1) {
                    hkc_valve_setter(ch, HOMEKIT_UINT8(0));
                } else {
                    hkc_valve_setter(ch, HOMEKIT_UINT8(1));
                }
                break;
                
            case TYPE_LIGHTBULB:
                autodimmer_call(ch, HOMEKIT_BOOL(!ch->value.bool_value));
                break;
                
            case TYPE_GARAGE_DOOR:
                if (ch->value.int_value == 1) {
                    hkc_garage_door_setter(ch, HOMEKIT_UINT8(0));
                } else {
                    hkc_garage_door_setter(ch, HOMEKIT_UINT8(1));
                }
                break;
                
            default:    // case TYPE_ON:
                hkc_on_setter(ch, HOMEKIT_BOOL(!ch->value.bool_value));
                break;
        }
    }
}

void diginput_1(const uint8_t gpio, void *args, const uint8_t type) {
    homekit_characteristic_t *ch = args;
    ch_group_t *ch_group = ch_group_find(ch);
    if (!ch_group->ch_child || ch_group->ch_child->value.bool_value) {
        switch (type) {
            case TYPE_LOCK:
                if (ch->value.int_value == 0) {
                    hkc_lock_setter(ch, HOMEKIT_UINT8(1));
                }
                break;
                
            case TYPE_VALVE:
                if (ch->value.int_value == 0) {
                    hkc_valve_setter(ch, HOMEKIT_UINT8(1));
                }
                break;
                
            case TYPE_LIGHTBULB:
                if (ch->value.bool_value == false) {
                    autodimmer_call(ch, HOMEKIT_BOOL(true));
                }
                break;
                
            case TYPE_GARAGE_DOOR:
                if (ch->value.int_value == 0) {
                    hkc_garage_door_setter(ch, HOMEKIT_UINT8(1));
                }
                break;
                
            default:    // case TYPE_ON:
                if (ch->value.bool_value == false) {
                    hkc_on_setter(ch, HOMEKIT_BOOL(true));
                }
                break;
        }
    }
}

void diginput_0(const uint8_t gpio, void *args, const uint8_t type) {
    homekit_characteristic_t *ch = args;
    ch_group_t *ch_group = ch_group_find(ch);
    if (!ch_group->ch_child || ch_group->ch_child->value.bool_value) {
        switch (type) {
            case TYPE_LOCK:
                if (ch->value.int_value == 1) {
                    hkc_lock_setter(ch, HOMEKIT_UINT8(0));
                }
                break;
                
            case TYPE_VALVE:
                if (ch->value.int_value == 1) {
                    hkc_valve_setter(ch, HOMEKIT_UINT8(0));
                }
                break;
                
            case TYPE_LIGHTBULB:
                if (ch->value.bool_value == true) {
                    autodimmer_call(ch, HOMEKIT_BOOL(false));
                }
                break;
                
            case TYPE_GARAGE_DOOR:
                if (ch->value.int_value == 1) {
                    hkc_garage_door_setter(ch, HOMEKIT_UINT8(0));
                }
                break;
                
            default:    // case TYPE_ON:
                if (ch->value.bool_value == true) {
                    hkc_on_setter(ch, HOMEKIT_BOOL(false));
                }
                break;
        }
    }
}

// --- AUTO-OFF
void hkc_autooff_setter_task(void *pvParameters) {
    autooff_setter_params_t *autooff_setter_params = pvParameters;
    
    vTaskDelay(autooff_setter_params->time * 1000 / portTICK_PERIOD_MS);
    
    switch (autooff_setter_params->type) {
        case TYPE_LOCK:
            hkc_lock_setter(autooff_setter_params->ch, HOMEKIT_UINT8(1));
            break;
            
        case TYPE_SENSOR:
        case TYPE_SENSOR_BOOL:
            sensor_0(0, autooff_setter_params->ch, autooff_setter_params->type);
            break;
            
        case TYPE_VALVE:
            hkc_valve_setter(autooff_setter_params->ch, HOMEKIT_UINT8(0));
            break;
            
        default:    // case TYPE_ON:
            hkc_on_setter(autooff_setter_params->ch, HOMEKIT_BOOL(false));
            break;
    }
    
    free(autooff_setter_params);
    vTaskDelete(NULL);
}

// --- ACTIONS
void autoswitch_task(void *pvParameters) {
    autoswitch_params_t *autoswitch_params = pvParameters;

    vTaskDelay(autoswitch_params->time * 1000 / portTICK_PERIOD_MS);
    
    gpio_write(autoswitch_params->gpio, autoswitch_params->value);
    INFO("Autoswitch digital output GPIO %i -> %i", autoswitch_params->gpio, autoswitch_params->value);
    
    free(autoswitch_params);
    vTaskDelete(NULL);
}

void do_actions(cJSON *json_context, const uint8_t int_action) {
    char action[3];
    itoa(int_action, action, 10);
    
    if (cJSON_GetObjectItemCaseSensitive(json_context, action) != NULL) {
        cJSON *actions = cJSON_GetObjectItemCaseSensitive(json_context, action);
        
        // Copy actions
        if(cJSON_GetObjectItemCaseSensitive(actions, COPY_ACTIONS) != NULL) {
            const uint8_t new_action = (uint8_t) cJSON_GetObjectItemCaseSensitive(actions, COPY_ACTIONS)->valuedouble;
            itoa(new_action, action, 10);
            
            if (cJSON_GetObjectItemCaseSensitive(json_context, action) != NULL) {
                actions = cJSON_GetObjectItemCaseSensitive(json_context, action);
            }
        }
        
        // Digital outputs
        cJSON *json_relays = cJSON_GetObjectItemCaseSensitive(actions, DIGITAL_OUTPUTS_ARRAY);
        for(uint8_t i=0; i<cJSON_GetArraySize(json_relays); i++) {
            cJSON *json_relay = cJSON_GetArrayItem(json_relays, i);
            
            const uint8_t gpio = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_relay, PIN_GPIO)->valuedouble;
            
            bool value = false;
            if (cJSON_GetObjectItemCaseSensitive(json_relay, VALUE) != NULL) {
                value = (bool) cJSON_GetObjectItemCaseSensitive(json_relay, VALUE)->valuedouble;
            }

            gpio_write(gpio, value);
            INFO("Digital output GPIO %i -> %i", gpio, value);
            
            if (cJSON_GetObjectItemCaseSensitive(json_relay, AUTOSWITCH_TIME) != NULL) {
                const double autoswitch_time = cJSON_GetObjectItemCaseSensitive(json_relay, AUTOSWITCH_TIME)->valuedouble;
                if (autoswitch_time > 0) {
                    autoswitch_params_t *autoswitch_params = malloc(sizeof(autoswitch_params_t));
                    autoswitch_params->gpio = gpio;
                    autoswitch_params->value = !value;
                    autoswitch_params->time = autoswitch_time;
                    xTaskCreate(autoswitch_task, "autoswitch_task", configMINIMAL_STACK_SIZE, autoswitch_params, 1, NULL);
                }
            }
        }
        
        // Accessory Manager
        cJSON *json_acc_managers = cJSON_GetObjectItemCaseSensitive(actions, MANAGE_OTHERS_ACC_ARRAY);
        for(uint8_t i=0; i<cJSON_GetArraySize(json_acc_managers); i++) {
            cJSON *json_acc_manager = cJSON_GetArrayItem(json_acc_managers, i);
            
            const uint8_t accessory = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_acc_manager, ACCESSORY_INDEX)->valuedouble;
            
            ch_group_t *ch_group = ch_group_find_by_acc(accessory);
            if (ch_group) {
                float value = 0.0;
                if (cJSON_GetObjectItemCaseSensitive(json_acc_manager, VALUE) != NULL) {
                    value = (float) cJSON_GetObjectItemCaseSensitive(json_acc_manager, VALUE)->valuedouble;
                }

                switch (ch_group->acc_type) {
                    case ACC_TYPE_BUTTON:
                        button_event(0, ch_group->ch0, (uint8_t) value);
                        break;
                        
                    case ACC_TYPE_LOCK:
                        hkc_lock_setter(ch_group->ch1, HOMEKIT_UINT8((uint8_t) value));
                        break;
                        
                    case ACC_TYPE_CONTACT_SENSOR:
                        if ((bool) value) {
                            sensor_1(0, ch_group->ch0, TYPE_SENSOR);
                        } else {
                            sensor_0(0, ch_group->ch0, TYPE_SENSOR);
                        }
                        break;
                        
                    case ACC_TYPE_MOTION_SENSOR:
                        if ((bool) value) {
                            sensor_1(0, ch_group->ch0, TYPE_SENSOR_BOOL);
                        } else {
                            sensor_0(0, ch_group->ch0, TYPE_SENSOR_BOOL);
                        }
                        break;
                        
                    case ACC_TYPE_WATER_VALVE:
                        hkc_valve_setter(ch_group->ch0, HOMEKIT_UINT8((uint8_t) value));
                        break;
                        
                    case ACC_TYPE_THERMOSTAT:
                        if (value == 0.02 || value == 0.03) {
                            update_th(ch_group->ch1, HOMEKIT_BOOL((bool) ((value - 0.02) * 100)));
                        } else if (value == 0.04 || value == 0.05 || value == 0.06) {
                            update_th(ch_group->ch4, HOMEKIT_UINT8((uint8_t) ((value - 0.04) * 100)));
                        } else {
                            if (((uint16_t) (value * 100) % 2) == 0) {
                                update_th(ch_group->ch5, HOMEKIT_FLOAT(value));
                            } else {
                                update_th(ch_group->ch6, HOMEKIT_FLOAT(value - 0.01));
                            }
                        }
                        break;
                        
                    case ACC_TYPE_GARAGE_DOOR:
                        if (value < 2) {
                            hkc_garage_door_setter(ch_group->ch1, HOMEKIT_UINT8((uint8_t) value));
                        } else if (value == 2) {
                            garage_door_stop(0, ch_group->ch0, 0);
                        } else {
                            garage_door_obstruction(0, ch_group->ch0, value - 3);
                        }
                        break;
                        
                    case ACC_TYPE_LIGHTBULB:
                        hkc_rgbw_setter(ch_group->ch0, HOMEKIT_BOOL((bool) value));
                        break;
                        
                    default:    // ON Type ch
                        hkc_on_setter(ch_group->ch0, HOMEKIT_BOOL((bool) value));
                        break;
                }
                
                INFO("Accessory Manager %i -> %.2f", accessory, value);
            }
        }
        
        // System actions
        cJSON *json_system_actions = cJSON_GetObjectItemCaseSensitive(actions, SYSTEM_ACTIONS_ARRAY);
        for(uint8_t i=0; i<cJSON_GetArraySize(json_system_actions); i++) {
            cJSON *json_system_action = cJSON_GetArrayItem(json_system_actions, i);
            
            const uint8_t system_action = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_system_action, SYSTEM_ACTION)->valuedouble;

            INFO("System Action %i", system_action);
            
            char *ota = NULL;
            
            switch (system_action) {
                case SYSTEM_ACTION_SETUP_MODE:
                    setup_mode_call(0, NULL, 0);
                    break;
                    
                case SYSTEM_ACTION_OTA_UPDATE:
                    if (sysparam_get_string("ota_repo", &ota) == SYSPARAM_OK) {
                        rboot_set_temp_rom(1);
                        xTaskCreate(reboot_task, "reboot_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
                    }
                    break;
                    
                default:    // case SYSTEM_ACTION_REBOOT:
                    xTaskCreate(reboot_task, "reboot_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
                    break;
            }
        }
        
    }
}

// --- IDENTIFY
void identify(homekit_value_t _value) {
    led_blink(6);
    INFO("Identifying");
}

// ---------

void delayed_sensor_starter_task(void *args) {
    INFO("Starting delayed sensor");
    homekit_characteristic_t *ch = args;
    ch_group_t *ch_group = ch_group_find(ch);
    
    cJSON *json_context = ch->context;
    
    uint16_t th_poll_period = THERMOSTAT_DEFAULT_POLL_PERIOD;
    if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_POLL_PERIOD) != NULL) {
        th_poll_period = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_POLL_PERIOD)->valuedouble;
    }
    
    if (th_poll_period < 3) {
        th_poll_period = 3;
    }
    
    vTaskDelay(ch_group->accessory * (3000 / portTICK_PERIOD_MS));
    
    temperature_timer_worker(ch);
    sdk_os_timer_arm(ch_group->timer, th_poll_period * 1000, 1);
    
    vTaskDelete(NULL);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, NULL);
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, NULL);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER, "RavenSystem");
homekit_characteristic_t model = HOMEKIT_CHARACTERISTIC_(MODEL, "HAA");
homekit_characteristic_t identify_function = HOMEKIT_CHARACTERISTIC_(IDENTIFY, identify);
homekit_characteristic_t firmware = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, FIRMWARE_VERSION);

homekit_server_config_t config;

void run_homekit_server() {
    if (enable_homekit_server) {
        INFO("Start HK Server");

        homekit_server_init(&config);
    }
    
    led_blink(6);
}

void printf_header() {
    printf("\n\n\n");
    INFO("Home Accessory Architect v%s", FIRMWARE_VERSION);
    INFO("Developed by José Antonio Jiménez Campos (@RavenSystem)\n");
}

void normal_mode_init() {
    char *txt_config = NULL;
    sysparam_get_string("haa_conf", &txt_config);
    
    cJSON *json_config = cJSON_GetObjectItemCaseSensitive(cJSON_Parse(txt_config), GENERAL_CONFIG);
    cJSON *json_accessories = cJSON_GetObjectItemCaseSensitive(cJSON_Parse(txt_config), ACCESSORIES);
    
    const uint8_t total_accessories = cJSON_GetArraySize(json_accessories);
    
    if (total_accessories == 0) {
        uart_set_baud(0, 115200);
        ERROR("Invalid JSON");
        sysparam_set_bool("setup", true);
        xTaskCreate(reboot_task, "reboot_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
        
        free(txt_config);
        
        return;
    }
    
    // Buttons GPIO Setup function
    bool diginput_register(cJSON *json_buttons, void *callback, homekit_characteristic_t *hk_ch, const uint8_t param) {
        bool run_at_launch = false;
        
        for(uint8_t j=0; j<cJSON_GetArraySize(json_buttons); j++) {
            const uint8_t gpio = (uint8_t) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), PIN_GPIO)->valuedouble;
            bool pullup_resistor = true;
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), PULLUP_RESISTOR) != NULL &&
                cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), PULLUP_RESISTOR)->valuedouble == 0) {
                pullup_resistor = false;
            }
            
            bool inverted = false;
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), INVERTED) != NULL &&
                cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), INVERTED)->valuedouble == 1) {
                inverted = true;
            }
            
            uint8_t button_type = 1;
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), BUTTON_PRESS_TYPE) != NULL) {
                button_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), BUTTON_PRESS_TYPE)->valuedouble;
            }
            
            if (!used_gpio[gpio]) {
                adv_button_create(gpio, pullup_resistor, inverted);
                used_gpio[gpio] = true;
            }
            adv_button_register_callback_fn(gpio, callback, button_type, (void*) hk_ch, param);
            
            INFO("Digital input GPIO: %i, type: %i, inv: %i", gpio, button_type, inverted);
             
            if (gpio_read(gpio) == button_type) {
                run_at_launch = true;
            }
        }
        
        return run_at_launch;
    }
    
    // Initial state function
    float set_initial_state(const uint8_t accessory, const uint8_t ch_number, cJSON *json_context, homekit_characteristic_t *ch, const uint8_t ch_type, const float default_value) {
        float state = default_value;
        INFO("Setting initial state");
        if (cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE) != NULL) {
            const uint8_t initial_state = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE)->valuedouble;
            if (initial_state < INIT_STATE_LAST) {
                    state = initial_state;
            } else {
                char *saved_state_id = malloc(3);
                uint16_t int_saved_state_id = ((accessory + 10) * 10) + ch_number;
                
                itoa(int_saved_state_id, saved_state_id, 10);
                last_state_t *last_state = malloc(sizeof(last_state_t));
                memset(last_state, 0, sizeof(*last_state));
                last_state->id = saved_state_id;
                last_state->ch = ch;
                last_state->ch_type = ch_type;
                last_state->next = last_states;
                last_states = last_state;
                
                sysparam_status_t status;
                bool saved_state_bool = false;
                int8_t saved_state_int8;
                int32_t saved_state_int32;
                
                
                switch (ch_type) {
                    case CH_TYPE_INT8:
                        status = sysparam_get_int8(saved_state_id, &saved_state_int8);
                        
                        if (status == SYSPARAM_OK) {
                            state = saved_state_int8;
                        }
                        break;
                        
                    case CH_TYPE_INT32:
                        status = sysparam_get_int32(saved_state_id, &saved_state_int32);
                        
                        if (status == SYSPARAM_OK) {
                            state = saved_state_int32;
                        }
                        break;
                        
                    case CH_TYPE_FLOAT:
                        status = sysparam_get_int32(saved_state_id, &saved_state_int32);
                        
                        if (status == SYSPARAM_OK) {
                            state = saved_state_int32 / 100.00f;
                        }
                        break;
                        
                    default:    // case CH_TYPE_BOOL
                        status = sysparam_get_bool(saved_state_id, &saved_state_bool);
                        
                        if (status == SYSPARAM_OK) {
                            if (initial_state == INIT_STATE_LAST) {
                                state = saved_state_bool;
                            } else if (ch_type == CH_TYPE_BOOL) {    // initial_state == INIT_STATE_INV_LAST
                                state = !saved_state_bool;
                            }
                        }
                        break;
                }
                
                if (status != SYSPARAM_OK) {
                    ERROR("No previous state found");
                }
                
                INFO("Init state = %.2f", state);
                
            }
        }
        
        return state;
    }
    
    // ----- CONFIG SECTION
    
    // Log output type
    if (cJSON_GetObjectItemCaseSensitive(json_config, LOG_OUTPUT) != NULL &&
        cJSON_GetObjectItemCaseSensitive(json_config, LOG_OUTPUT)->valuedouble == 1) {
        uart_set_baud(0, 115200);
        printf_header();
        INFO("NORMAL MODE");
        INFO("JSON: %s", txt_config);
        INFO("-- PROCESSING JSON --");
    }

    free(txt_config);
    
    // Custom Hostname
    char *custom_hostname = name.value.string_value;
    if (cJSON_GetObjectItemCaseSensitive(json_config, CUSTOM_HOSTNAME) != NULL) {
        uint8_t custom_hostname_len = strlen(cJSON_GetObjectItemCaseSensitive(json_config, CUSTOM_HOSTNAME)->valuestring) + 1;
        custom_hostname = malloc(custom_hostname_len);
        snprintf(custom_hostname, custom_hostname_len, "%s", cJSON_GetObjectItemCaseSensitive(json_config, CUSTOM_HOSTNAME)->valuestring);
        INFO("Hostname: %s", custom_hostname);
    }
    
    // Status LED
    if (cJSON_GetObjectItemCaseSensitive(json_config, STATUS_LED_GPIO) != NULL) {
        led_gpio = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, STATUS_LED_GPIO)->valuedouble;

        if (cJSON_GetObjectItemCaseSensitive(json_config, INVERTED) != NULL) {
                led_inverted = (bool) cJSON_GetObjectItemCaseSensitive(json_config, INVERTED)->valuedouble;
        }
        
        gpio_enable(led_gpio, GPIO_OUTPUT);
        used_gpio[led_gpio] = true;
        gpio_write(led_gpio, false ^ led_inverted);
        INFO("Status LED GPIO: %i, inv: %i", led_gpio, led_inverted);
    }
    
    // Button filter
    if (cJSON_GetObjectItemCaseSensitive(json_config, BUTTON_FILTER) != NULL) {
        uint8_t button_filter_value = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, BUTTON_FILTER)->valuedouble;
        adv_button_set_evaluate_delay(button_filter_value);
        INFO("Button filter: %i", button_filter_value);
    }
    
    // PWM Frequency
    if (cJSON_GetObjectItemCaseSensitive(json_config, PWM_FREQ) != NULL) {
        pwm_freq = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_config, PWM_FREQ)->valuedouble;
        INFO("PWM Frequency: %i", pwm_freq);
    }
    
    // Allowed Setup Mode Time
    if (cJSON_GetObjectItemCaseSensitive(json_config, ALLOWED_SETUP_MODE_TIME) != NULL) {
        setup_mode_time = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_config, ALLOWED_SETUP_MODE_TIME)->valuedouble;
        INFO("Setup mode time: %i secs", setup_mode_time);
    }
    
    // Run HomeKit Server
    if (cJSON_GetObjectItemCaseSensitive(json_config, ENABLE_HOMEKIT_SERVER) != NULL) {
        enable_homekit_server = (bool) cJSON_GetObjectItemCaseSensitive(json_config, ENABLE_HOMEKIT_SERVER)->valuedouble;
        INFO("Run HomeKit Server: %i", enable_homekit_server);
    }
    
    // Allow unsecure connections
    if (cJSON_GetObjectItemCaseSensitive(json_config, ALLOW_INSECURE_CONNECTIONS) != NULL) {
        bool allow_insecure = (bool) cJSON_GetObjectItemCaseSensitive(json_config, ALLOW_INSECURE_CONNECTIONS)->valuedouble;
        config.insecure = allow_insecure;
        INFO("Unsecure connections: %i", allow_insecure);
    }
    
    // Times to toggle quickly an accessory status to enter setup mode
    if (cJSON_GetObjectItemCaseSensitive(json_config, SETUP_MODE_ACTIVATE_COUNT) != NULL) {
        setup_mode_toggle_counter_max = (int8_t) cJSON_GetObjectItemCaseSensitive(json_config, SETUP_MODE_ACTIVATE_COUNT)->valuedouble;
        INFO("Toggles to enter setup mode: %i", setup_mode_toggle_counter_max);
    }
    
    if (setup_mode_toggle_counter_max > 0) {
        setup_mode_toggle_timer = malloc(sizeof(ETSTimer));
        memset(setup_mode_toggle_timer, 0, sizeof(*setup_mode_toggle_timer));
        sdk_os_timer_setfn(setup_mode_toggle_timer, setup_mode_toggle, NULL);
    }
    
    // Buttons to enter setup mode
    diginput_register(cJSON_GetObjectItemCaseSensitive(json_config, BUTTONS_ARRAY), setup_mode_call, NULL, 0);
    
    // ----- END CONFIG SECTION
    
    uint8_t hk_total_ac = 1;
    bool bridge_needed = false;

    for(uint8_t i=0; i<total_accessories; i++) {
        // Kill Switch Accessory count
        if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_accessories, i), KILL_SWITCH) != NULL) {
        const uint8_t kill_switch = (uint8_t) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_accessories, i), KILL_SWITCH)->valuedouble;
            switch (kill_switch) {
                case 1:
                case 2:
                    hk_total_ac += 1;
                    break;
                    
                case 3:
                    hk_total_ac += 2;
                    break;
                    
                default:    // case 0:
                    break;
            }
        }
        
        // Accessory Type Accessory count
        uint8_t acc_type = ACC_TYPE_SWITCH;     // Default accessory type
        if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_accessories, i), ACCESSORY_TYPE) != NULL) {
            acc_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_accessories, i), ACCESSORY_TYPE)->valuedouble;
        }

        switch (acc_type) {
            default:
                hk_total_ac += 1;
                break;
        }
    }
    
    if (total_accessories > ACCESSORIES_WITHOUT_BRIDGE || bridge_needed) {
        // Bridge needed
        bridge_needed = true;
        hk_total_ac += 1;
    }
    
    // Saved States Timer Function
    sdk_os_timer_setfn(&save_states_timer, save_states, NULL);
    
    homekit_accessory_t **accessories = calloc(hk_total_ac, sizeof(homekit_accessory_t*));
    
    // Define services and characteristics
    uint8_t accessory_numerator = 1;
    
    void new_accessory(const uint8_t accessory, const uint8_t services) {
        accessories[accessory] = calloc(1, sizeof(homekit_accessory_t));
        accessories[accessory]->id = accessory + 1;
        accessories[accessory]->services = calloc(services, sizeof(homekit_service_t*));
        
        accessories[accessory]->services[0] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[0]->id = 1;
        accessories[accessory]->services[0]->type = HOMEKIT_SERVICE_ACCESSORY_INFORMATION;
        accessories[accessory]->services[0]->characteristics = calloc(7, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[0]->characteristics[0] = &name;
        accessories[accessory]->services[0]->characteristics[1] = &manufacturer;
        accessories[accessory]->services[0]->characteristics[2] = &serial;
        accessories[accessory]->services[0]->characteristics[3] = &model;
        accessories[accessory]->services[0]->characteristics[4] = &firmware;
        accessories[accessory]->services[0]->characteristics[5] = &identify_function;
    }
    
    homekit_characteristic_t *new_kill_switch(const uint8_t accessory) {
        new_accessory(accessory, 3);
        
        homekit_characteristic_t *ch = NEW_HOMEKIT_CHARACTERISTIC(ON, false, .getter_ex=hkc_getter, .setter_ex=hkc_setter_with_setup);
        
        accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[1]->id = 8;
        accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_SWITCH;
        accessories[accessory]->services[1]->primary = true;
        accessories[accessory]->services[1]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[1]->characteristics[0] = ch;
        
        ch->value.bool_value = (bool) set_initial_state(accessory, 0, cJSON_Parse(INIT_STATE_LAST_STR), ch, CH_TYPE_BOOL, 0);
        
        return ch;
    }
    
    uint8_t build_kill_switches(const uint8_t accessory, ch_group_t *ch_group, cJSON *json_context) {
        if (cJSON_GetObjectItemCaseSensitive(json_context, KILL_SWITCH) != NULL) {
            const uint8_t kill_switch = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, KILL_SWITCH)->valuedouble;
            
            if (kill_switch == 1) {
                INFO("Secure Switch");
                ch_group->ch_sec = new_kill_switch(accessory);
                return accessory + 1;
                
            } else if (kill_switch == 2) {
                INFO("Kids Switch");
                ch_group->ch_child = new_kill_switch(accessory);
                return accessory + 1;
                
            } else if (kill_switch == 3) {
                INFO("Secure Switch");
                ch_group->ch_sec = new_kill_switch(accessory);
                INFO("Kids Switch");
                ch_group->ch_child = new_kill_switch(accessory + 1);
                return accessory + 2;
            }
        }
        
        return accessory;
    }

    uint8_t new_switch(uint8_t accessory, cJSON *json_context, const uint8_t acc_type) {
        new_accessory(accessory, 3);
        
        homekit_characteristic_t *ch0 = NEW_HOMEKIT_CHARACTERISTIC(ON, false, .getter_ex=hkc_getter, .setter_ex=hkc_on_setter, .context=json_context);
        
        uint32_t max_duration = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, VALVE_MAX_DURATION) != NULL) {
            max_duration = (uint32_t) cJSON_GetObjectItemCaseSensitive(json_context, VALVE_MAX_DURATION)->valuedouble;
        }
        
        ch_group_t *ch_group = malloc(sizeof(ch_group_t));
        memset(ch_group, 0, sizeof(*ch_group));
        ch_group->accessory = accessory_numerator;
        accessory_numerator++;
        ch_group->acc_type = ACC_TYPE_SWITCH;
        ch_group->ch0 = ch0;
        ch_group->next = ch_groups;
        ch_groups = ch_group;
        
        accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[1]->id = 8;
        accessories[accessory]->services[1]->primary = true;
        
        uint8_t ch_calloc = 2;
        if (max_duration > 0) {
            ch_calloc += 2;
        }
        
        if (acc_type == ACC_TYPE_SWITCH) {
            accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_SWITCH;
            accessories[accessory]->services[1]->characteristics = calloc(ch_calloc, sizeof(homekit_characteristic_t*));
            
        } else {    // acc_type == ACC_TYPE_OUTLET
            ch_calloc++;
            
            homekit_characteristic_t *ch3 = NEW_HOMEKIT_CHARACTERISTIC(OUTLET_IN_USE, true, .getter_ex=hkc_getter, .context=json_context);
            
            accessories[accessory]->services[1]->characteristics = calloc(ch_calloc, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_OUTLET;
            accessories[accessory]->services[1]->characteristics[1] = ch3;
            
            if (max_duration > 0) {
                ch_group->ch3 = ch3;
            } else {
                ch_group->ch1 = ch3;
            }
        }
        
        accessories[accessory]->services[1]->characteristics[0] = ch0;
        
        if (max_duration > 0) {
            homekit_characteristic_t *ch1 = NEW_HOMEKIT_CHARACTERISTIC(SET_DURATION, max_duration, .max_value=(float[]) {max_duration}, .getter_ex=hkc_getter, .setter_ex=hkc_setter);
            homekit_characteristic_t *ch2 = NEW_HOMEKIT_CHARACTERISTIC(REMAINING_DURATION, 0, .max_value=(float[]) {max_duration}, .getter_ex=hkc_getter);
            
            ch_group->ch1 = ch1;
            ch_group->ch2 = ch2;
            
            accessories[accessory]->services[1]->characteristics[ch_calloc - 3] = ch1;
            accessories[accessory]->services[1]->characteristics[ch_calloc - 2] = ch2;
            
            const uint32_t initial_time = (uint32_t) set_initial_state(accessory, 1, cJSON_Parse(INIT_STATE_LAST_STR), ch1, CH_TYPE_INT32, 900);
            if (initial_time > max_duration) {
                ch1->value.int_value = max_duration;
            } else {
                ch1->value.int_value = initial_time;
            }
            
            ch_group->timer = malloc(sizeof(ETSTimer));
            memset(ch_group->timer, 0, sizeof(*ch_group->timer));
            sdk_os_timer_setfn(ch_group->timer, on_timer_worker, ch0);
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch0, TYPE_ON);
        
        uint8_t initial_state = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE) != NULL) {
            initial_state = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE)->valuedouble;
        }
        
        if (initial_state != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch0, TYPE_ON);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch0, TYPE_ON);
            
            ch0->value.bool_value = !((bool) set_initial_state(accessory, 0, json_context, ch0, CH_TYPE_BOOL, 0));
            hkc_on_setter(ch0, HOMEKIT_BOOL(!ch0->value.bool_value));
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch0, TYPE_ON)) {
                diginput_1(0, ch0, TYPE_ON);
            }
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch0, TYPE_ON)) {
                ch0->value = HOMEKIT_BOOL(true);
                diginput_0(0, ch0, TYPE_ON);
            }
        }
        
        const uint8_t new_accessory_count = build_kill_switches(accessory + 1, ch_group, json_context);
        return new_accessory_count;
    }
    
    uint8_t new_button_event(uint8_t accessory, cJSON *json_context) {
        new_accessory(accessory, 3);
        
        homekit_characteristic_t *ch0 = NEW_HOMEKIT_CHARACTERISTIC(PROGRAMMABLE_SWITCH_EVENT, 0, .context=json_context);
        
        ch_group_t *ch_group = malloc(sizeof(ch_group_t));
        memset(ch_group, 0, sizeof(*ch_group));
        ch_group->accessory = accessory_numerator;
        accessory_numerator++;
        ch_group->acc_type = ACC_TYPE_BUTTON;
        ch_group->ch0 = ch0;
        ch_group->next = ch_groups;
        ch_groups = ch_group;
        
        accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[1]->id = 8;
        accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_STATELESS_PROGRAMMABLE_SWITCH;
        accessories[accessory]->services[1]->primary = true;
        accessories[accessory]->services[1]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[1]->characteristics[0] = ch0;
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), button_event, ch0, SINGLEPRESS_EVENT);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), button_event, ch0, DOUBLEPRESS_EVENT);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2), button_event, ch0, LONGPRESS_EVENT);
        
        const uint8_t new_accessory_count = build_kill_switches(accessory + 1, ch_group, json_context);
        return new_accessory_count;
    }
    
    uint8_t new_lock(const uint8_t accessory, cJSON *json_context) {
        new_accessory(accessory, 3);
        
        homekit_characteristic_t *ch0 = NEW_HOMEKIT_CHARACTERISTIC(LOCK_CURRENT_STATE, 1, .getter_ex=hkc_getter, .context=json_context);
        homekit_characteristic_t *ch1 = NEW_HOMEKIT_CHARACTERISTIC(LOCK_TARGET_STATE, 1, .getter_ex=hkc_getter, .setter_ex=hkc_lock_setter, .context=json_context);
        
        ch_group_t *ch_group = malloc(sizeof(ch_group_t));
        memset(ch_group, 0, sizeof(*ch_group));
        ch_group->accessory = accessory_numerator;
        accessory_numerator++;
        ch_group->acc_type = ACC_TYPE_LOCK;
        ch_group->ch0 = ch0;
        ch_group->ch1 = ch1;
        ch_group->next = ch_groups;
        ch_groups = ch_group;
        
        accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[1]->id = 8;
        accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_LOCK_MECHANISM;
        accessories[accessory]->services[1]->primary = true;
        accessories[accessory]->services[1]->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[1]->characteristics[0] = ch0;
        accessories[accessory]->services[1]->characteristics[1] = ch1;
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch1, TYPE_LOCK);
        
        uint8_t initial_state = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE) != NULL) {
            initial_state = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE)->valuedouble;
        }
        
        if (initial_state != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch1, TYPE_LOCK);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch1, TYPE_LOCK);
            
            ch1->value.int_value = !((uint8_t) set_initial_state(accessory, 0, json_context, ch1, CH_TYPE_INT8, 1));
            hkc_lock_setter(ch1, HOMEKIT_UINT8(!ch1->value.int_value));
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch1, TYPE_LOCK)) {
                diginput_1(0, ch1, TYPE_LOCK);
            }
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch1, TYPE_LOCK)) {
                ch1->value = HOMEKIT_UINT8(0);
                diginput_0(0, ch1, TYPE_LOCK);
            }
        }
        
        const uint8_t new_accessory_count = build_kill_switches(accessory + 1, ch_group, json_context);
        return new_accessory_count;
    }
    
    uint8_t new_sensor(const uint8_t accessory, cJSON *json_context, const uint8_t acc_type) {
        new_accessory(accessory, 3);
        
        homekit_characteristic_t *ch0;
        
        accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[1]->id = 8;
        accessories[accessory]->services[1]->primary = true;
        
        switch (acc_type) {
            case ACC_TYPE_OCCUPANCY_SENSOR:
                accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_OCCUPANCY_SENSOR;
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(OCCUPANCY_DETECTED, 0, .getter_ex=hkc_getter, .context=json_context);
                break;
                
            case ACC_TYPE_LEAK_SENSOR:
                accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_LEAK_SENSOR;
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(LEAK_DETECTED, 0, .getter_ex=hkc_getter, .context=json_context);
                break;
                
            case ACC_TYPE_SMOKE_SENSOR:
                accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_SMOKE_SENSOR;
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(SMOKE_DETECTED, 0, .getter_ex=hkc_getter, .context=json_context);
                break;
                
            case ACC_TYPE_CARBON_MONOXIDE_SENSOR:
                accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_CARBON_MONOXIDE_SENSOR;
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(CARBON_MONOXIDE_DETECTED, 0, .getter_ex=hkc_getter, .context=json_context);
                break;
                
            case ACC_TYPE_CARBON_DIOXIDE_SENSOR:
                accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_CARBON_DIOXIDE_SENSOR;
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(CARBON_DIOXIDE_DETECTED, 0, .getter_ex=hkc_getter, .context=json_context);
                break;
                
            case ACC_TYPE_FILTER_CHANGE_SENSOR:
                accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_FILTER_MAINTENANCE;
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(FILTER_CHANGE_INDICATION, 0, .getter_ex=hkc_getter, .context=json_context);
                break;
                
            case ACC_TYPE_MOTION_SENSOR:
                accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_MOTION_SENSOR;
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(MOTION_DETECTED, 0, .getter_ex=hkc_getter, .context=json_context);
                break;
                
            default:    // case ACC_TYPE_CONTACT_SENSOR:
                accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_CONTACT_SENSOR;
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(CONTACT_SENSOR_STATE, 0, .getter_ex=hkc_getter, .context=json_context);
                break;
        }
        
        accessories[accessory]->services[1]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[1]->characteristics[0] = ch0;
        
        ch_group_t *ch_group = malloc(sizeof(ch_group_t));
        memset(ch_group, 0, sizeof(*ch_group));
        ch_group->accessory = accessory_numerator;
        accessory_numerator++;
        ch_group->acc_type = ACC_TYPE_CONTACT_SENSOR;
        ch_group->ch0 = ch0;
        ch_group->next = ch_groups;
        ch_groups = ch_group;
        
        if (acc_type == ACC_TYPE_MOTION_SENSOR) {
            ch_group->acc_type = ACC_TYPE_MOTION_SENSOR;
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), sensor_0, ch0, TYPE_SENSOR_BOOL)) {
                sensor_0(0, ch0, TYPE_SENSOR_BOOL);
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), sensor_1, ch0, TYPE_SENSOR_BOOL)) {
                sensor_1(0, ch0, TYPE_SENSOR_BOOL);
            }
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), sensor_0, ch0, TYPE_SENSOR)) {
                sensor_0(0, ch0, TYPE_SENSOR);
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), sensor_1, ch0, TYPE_SENSOR)) {
                sensor_1(0, ch0, TYPE_SENSOR);
            }
        }
        
        return accessory + 1;
    }
    
    uint8_t new_water_valve(uint8_t accessory, cJSON *json_context) {
        new_accessory(accessory, 3);
        
        uint8_t valve_type = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, VALVE_SYSTEM_TYPE) != NULL) {
            valve_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, VALVE_SYSTEM_TYPE)->valuedouble;
        }
        
        uint32_t valve_max_duration = VALVE_DEFAULT_MAX_DURATION;
        if (cJSON_GetObjectItemCaseSensitive(json_context, VALVE_MAX_DURATION) != NULL) {
            valve_max_duration = (uint32_t) cJSON_GetObjectItemCaseSensitive(json_context, VALVE_MAX_DURATION)->valuedouble;
        }
        
        homekit_characteristic_t *ch0 = NEW_HOMEKIT_CHARACTERISTIC(ACTIVE, 0, .getter_ex=hkc_getter, .setter_ex=hkc_valve_setter, .context=json_context);
        homekit_characteristic_t *ch1 = NEW_HOMEKIT_CHARACTERISTIC(IN_USE, 0, .getter_ex=hkc_getter, .context=json_context);
        homekit_characteristic_t *ch2, *ch3;
        
        ch_group_t *ch_group = malloc(sizeof(ch_group_t));
        memset(ch_group, 0, sizeof(*ch_group));
        ch_group->accessory = accessory_numerator;
        accessory_numerator++;
        ch_group->acc_type = ACC_TYPE_WATER_VALVE;
        ch_group->ch0 = ch0;
        ch_group->ch1 = ch1;
        ch_group->next = ch_groups;
        ch_groups = ch_group;
        
        accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[1]->id = 8;
        accessories[accessory]->services[1]->primary = true;
        accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_VALVE;
        
        if (valve_max_duration == 0) {
            accessories[accessory]->services[1]->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
        } else {
            ch2 = NEW_HOMEKIT_CHARACTERISTIC(SET_DURATION, valve_max_duration, .max_value=(float[]) {valve_max_duration}, .getter_ex=hkc_getter, .setter_ex=hkc_setter);
            ch3 = NEW_HOMEKIT_CHARACTERISTIC(REMAINING_DURATION, 0, .max_value=(float[]) {valve_max_duration}, .getter_ex=hkc_getter);
            
            ch_group->ch2 = ch2;
            ch_group->ch3 = ch3;
            
            accessories[accessory]->services[1]->characteristics = calloc(6, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[1]->characteristics[3] = ch2;
            accessories[accessory]->services[1]->characteristics[4] = ch3;
            
            const uint32_t initial_time = (uint32_t) set_initial_state(accessory, 2, cJSON_Parse(INIT_STATE_LAST_STR), ch2, CH_TYPE_INT32, 900);
            if (initial_time > valve_max_duration) {
                ch2->value.int_value = valve_max_duration;
            } else {
                ch2->value.int_value = initial_time;
            }
            
            ch_group->timer = malloc(sizeof(ETSTimer));
            memset(ch_group->timer, 0, sizeof(*ch_group->timer));
            sdk_os_timer_setfn(ch_group->timer, valve_timer_worker, ch0);
        }
        
        accessories[accessory]->services[1]->characteristics[0] = ch0;
        accessories[accessory]->services[1]->characteristics[1] = ch1;
        accessories[accessory]->services[1]->characteristics[2] = NEW_HOMEKIT_CHARACTERISTIC(VALVE_TYPE, valve_type, .getter_ex=hkc_getter);
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch0, TYPE_VALVE);
        
        uint8_t initial_state = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE) != NULL) {
            initial_state = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE)->valuedouble;
        }
        
        if (initial_state != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch0, TYPE_VALVE);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch0, TYPE_VALVE);
            
            ch0->value.int_value = !((uint8_t) set_initial_state(accessory, 0, json_context, ch0, CH_TYPE_INT8, 0));
            hkc_valve_setter(ch0, HOMEKIT_UINT8(!ch0->value.int_value));
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch0, TYPE_VALVE)) {
                diginput_1(0, ch0, TYPE_VALVE);
            }
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch0, TYPE_VALVE)) {
                ch0->value = HOMEKIT_UINT8(1);
                diginput_0(0, ch0, TYPE_VALVE);
            }
        }
        
        const uint8_t new_accessory_count = build_kill_switches(accessory + 1, ch_group, json_context);
        return new_accessory_count;
    }
    
    uint8_t new_thermostat(uint8_t accessory, cJSON *json_context) {
        new_accessory(accessory, 3);
        
        // Custom ranges of Target Temperatures
        float th_min_temp = THERMOSTAT_DEFAULT_MIN_TEMP;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MIN_TEMP) != NULL) {
            th_min_temp = (float) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MIN_TEMP)->valuedouble;
        }
        
        if (th_min_temp < -100) {
            th_min_temp = -100;
        }
        
        float th_max_temp = THERMOSTAT_DEFAULT_MAX_TEMP;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MAX_TEMP) != NULL) {
            th_max_temp = (float) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MAX_TEMP)->valuedouble;
        }
        
        if (th_max_temp > 200) {
            th_max_temp = 200;
        }
        
        const float default_target_temp = (th_min_temp + th_max_temp) / 2;
        
        // Sensor poll period
        uint16_t th_poll_period = THERMOSTAT_DEFAULT_POLL_PERIOD;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_POLL_PERIOD) != NULL) {
            th_poll_period = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_POLL_PERIOD)->valuedouble;
        }
        
        if (th_poll_period < 3) {
            th_poll_period = 3;
        }
        
        // Thermostat Type
        uint8_t th_type = THERMOSTAT_TYPE_HEATER;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_TYPE) != NULL) {
            th_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_TYPE)->valuedouble;
        }
        
        // HomeKit Characteristics
        homekit_characteristic_t *ch0 = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_TEMPERATURE, 0, .min_value=(float[]) {-100}, .max_value=(float[]) {200}, .getter_ex=hkc_getter, .context=json_context);
        homekit_characteristic_t *ch1 = NEW_HOMEKIT_CHARACTERISTIC(ACTIVE, false, .getter_ex=hkc_getter, .setter_ex=hkc_th_target_setter, .context=json_context);
        homekit_characteristic_t *ch2 = NEW_HOMEKIT_CHARACTERISTIC(TEMPERATURE_DISPLAY_UNITS, 0, .getter_ex=hkc_getter, .setter_ex=hkc_setter);
        homekit_characteristic_t *ch3 = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_HEATER_COOLER_STATE, 0, .getter_ex=hkc_getter);
        homekit_characteristic_t *ch5 = NEW_HOMEKIT_CHARACTERISTIC(HEATING_THRESHOLD_TEMPERATURE, default_target_temp -1, .min_value=(float[]) {th_min_temp}, .max_value=(float[]) {th_max_temp}, .getter_ex=hkc_getter, .setter_ex=update_th, .context=json_context);
        homekit_characteristic_t *ch6 = NEW_HOMEKIT_CHARACTERISTIC(COOLING_THRESHOLD_TEMPERATURE, default_target_temp +1, .min_value=(float[]) {th_min_temp}, .max_value=(float[]) {th_max_temp}, .getter_ex=hkc_getter, .setter_ex=update_th, .context=json_context);
        
        uint8_t ch_calloc = 7;
        if (th_type == THERMOSTAT_TYPE_HEATERCOOLER) {
            ch_calloc += 1;
        }
        accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[1]->id = 8;
        accessories[accessory]->services[1]->primary = true;
        accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_HEATER_COOLER;
        accessories[accessory]->services[1]->characteristics = calloc(ch_calloc, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[1]->characteristics[0] = ch1;
        accessories[accessory]->services[1]->characteristics[1] = ch0;
        accessories[accessory]->services[1]->characteristics[2] = ch2;
        accessories[accessory]->services[1]->characteristics[3] = ch3;
        
        homekit_characteristic_t *ch4;
        
        const float initial_h_target_temp = set_initial_state(accessory, 5, cJSON_Parse(INIT_STATE_LAST_STR), ch5, CH_TYPE_FLOAT, default_target_temp -1);
        if (initial_h_target_temp > th_max_temp || initial_h_target_temp < th_min_temp) {
            ch5->value.float_value = default_target_temp -1;
        } else {
            ch5->value.float_value = initial_h_target_temp;
        }
        
        const float initial_c_target_temp = set_initial_state(accessory, 6, cJSON_Parse(INIT_STATE_LAST_STR), ch6, CH_TYPE_FLOAT, default_target_temp +1);
        if (initial_c_target_temp > th_max_temp || initial_c_target_temp < th_min_temp) {
            ch6->value.float_value = default_target_temp +1;
        } else {
            ch6->value.float_value = initial_c_target_temp;
        }
        
        switch (th_type) {
            case THERMOSTAT_TYPE_COOLER:
                ch4 = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HEATER_COOLER_STATE, THERMOSTAT_TARGET_MODE_COOLER, .min_value=(float[]) {THERMOSTAT_TARGET_MODE_COOLER}, .max_value=(float[]) {THERMOSTAT_TARGET_MODE_COOLER}, .valid_values={.count=1, .values=(uint8_t[]) {THERMOSTAT_TARGET_MODE_COOLER}}, .getter_ex=hkc_getter, .context=json_context);
                
                accessories[accessory]->services[1]->characteristics[5] = ch6;
                break;
                
            case THERMOSTAT_TYPE_HEATERCOOLER:
                ch4 = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HEATER_COOLER_STATE, THERMOSTAT_TARGET_MODE_AUTO, .getter_ex=hkc_getter, .setter_ex=update_th, .context=json_context);
                
                accessories[accessory]->services[1]->characteristics[5] = ch5;
                accessories[accessory]->services[1]->characteristics[6] = ch6;
                break;
                
            default:        // case THERMOSTAT_TYPE_HEATER:
                ch4 = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HEATER_COOLER_STATE, THERMOSTAT_TARGET_MODE_HEATER, .min_value=(float[]) {THERMOSTAT_TARGET_MODE_HEATER}, .max_value=(float[]) {THERMOSTAT_TARGET_MODE_HEATER}, .valid_values={.count=1, .values=(uint8_t[]) {THERMOSTAT_TARGET_MODE_HEATER}}, .getter_ex=hkc_getter, .context=json_context);

                accessories[accessory]->services[1]->characteristics[5] = ch5;
                break;
        }
        
        accessories[accessory]->services[1]->characteristics[4] = ch4;
        
        ch_group_t *ch_group = malloc(sizeof(ch_group_t));
        memset(ch_group, 0, sizeof(*ch_group));
        ch_group->accessory = accessory_numerator;
        accessory_numerator++;
        ch_group->acc_type = ACC_TYPE_THERMOSTAT;
        ch_group->ch0 = ch0;
        ch_group->ch1 = ch1;
        ch_group->ch2 = ch2;
        ch_group->ch3 = ch3;
        ch_group->ch4 = ch4;
        ch_group->ch5 = ch5;
        ch_group->ch6 = ch6;
        ch_group->next = ch_groups;
        ch_groups = ch_group;
            
        ch_group->timer = malloc(sizeof(ETSTimer));
        memset(ch_group->timer, 0, sizeof(*ch_group->timer));
        sdk_os_timer_setfn(ch_group->timer, temperature_timer_worker, ch0);
        
        xTaskCreate(delayed_sensor_starter_task, "delayed_sensor_starter_task", configMINIMAL_STACK_SIZE, ch0, 1, NULL);
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), th_input, ch1, 9);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3), th_input_temp, ch0, THERMOSTAT_TEMP_UP);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_4), th_input_temp, ch0, THERMOSTAT_TEMP_DOWN);
        
        if (th_type == THERMOSTAT_TYPE_HEATERCOOLER) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_5), th_input, ch0, 5);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_6), th_input, ch0, 6);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_7), th_input, ch0, 7);
            
            ch4->value.int_value = set_initial_state(accessory, 4, cJSON_Parse(INIT_STATE_LAST_STR), ch4, CH_TYPE_INT8, 0);
        }
        
        uint8_t initial_state = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE) != NULL) {
            initial_state = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE)->valuedouble;
        }
        
        if (initial_state != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), th_input, ch0, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), th_input, ch0, 0);
            
            ch1->value.bool_value = !((bool) set_initial_state(accessory, 1, json_context, ch1, CH_TYPE_BOOL, false));
            update_th(ch1, HOMEKIT_BOOL(!ch1->value.bool_value));
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), th_input, ch0, 1)) {
                th_input(0, ch1, 1);
            }
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), th_input, ch0, 0)) {
                ch1->value = HOMEKIT_BOOL(true);
                th_input(0, ch1, 0);
            }
        }
        
        const uint8_t new_accessory_count = build_kill_switches(accessory + 1, ch_group, json_context);
        return new_accessory_count;
    }
    
    uint8_t new_temp_sensor(uint8_t accessory, cJSON *json_context) {
        new_accessory(accessory, 3);
        
        uint16_t th_poll_period = THERMOSTAT_DEFAULT_POLL_PERIOD;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_POLL_PERIOD) != NULL) {
            th_poll_period = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_POLL_PERIOD)->valuedouble;
        }
        
        if (th_poll_period < 3) {
            th_poll_period = 3;
        }
        
        homekit_characteristic_t *ch0 = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_TEMPERATURE, 0, .min_value=(float[]) {-100}, .max_value=(float[]) {200}, .getter_ex=hkc_getter, .context=json_context);
        
        ch_group_t *ch_group = malloc(sizeof(ch_group_t));
        memset(ch_group, 0, sizeof(*ch_group));
        ch_group->accessory = accessory_numerator;
        accessory_numerator++;
        ch_group->ch0 = ch0;
        ch_group->next = ch_groups;
        ch_groups = ch_group;
        
        accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[1]->id = 8;
        accessories[accessory]->services[1]->primary = true;
        accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_TEMPERATURE_SENSOR;
        accessories[accessory]->services[1]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[1]->characteristics[0] = ch0;
            
        ch_group->timer = malloc(sizeof(ETSTimer));
        memset(ch_group->timer, 0, sizeof(*ch_group->timer));
        sdk_os_timer_setfn(ch_group->timer, temperature_timer_worker, ch0);
        
        xTaskCreate(delayed_sensor_starter_task, "delayed_sensor_starter_task", configMINIMAL_STACK_SIZE, ch0, 1, NULL);
        
        return accessory + 1;
    }
    
    uint8_t new_hum_sensor(uint8_t accessory, cJSON *json_context) {
        new_accessory(accessory, 3);
        
        homekit_characteristic_t *ch1 = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_RELATIVE_HUMIDITY, 0, .getter_ex=hkc_getter, .context=json_context);
        
        ch_group_t *ch_group = malloc(sizeof(ch_group_t));
        memset(ch_group, 0, sizeof(*ch_group));
        ch_group->accessory = accessory_numerator;
        accessory_numerator++;
        ch_group->ch1 = ch1;
        ch_group->next = ch_groups;
        ch_groups = ch_group;
        
        accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[1]->id = 8;
        accessories[accessory]->services[1]->primary = true;
        accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_HUMIDITY_SENSOR;
        accessories[accessory]->services[1]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[1]->characteristics[0] = ch1;
            
        ch_group->timer = malloc(sizeof(ETSTimer));
        memset(ch_group->timer, 0, sizeof(*ch_group->timer));
        sdk_os_timer_setfn(ch_group->timer, temperature_timer_worker, ch1);
        
        xTaskCreate(delayed_sensor_starter_task, "delayed_sensor_starter_task", configMINIMAL_STACK_SIZE, ch1, 1, NULL);
        
        return accessory + 1;
    }
    
    uint8_t new_th_sensor(uint8_t accessory, cJSON *json_context) {
        new_accessory(accessory, 4);
        
        uint16_t th_poll_period = THERMOSTAT_DEFAULT_POLL_PERIOD;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_POLL_PERIOD) != NULL) {
            th_poll_period = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_POLL_PERIOD)->valuedouble;
        }
        
        if (th_poll_period < 3) {
            th_poll_period = 3;
        }
        
        homekit_characteristic_t *ch0 = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_TEMPERATURE, 0, .min_value=(float[]) {-100}, .max_value=(float[]) {200}, .getter_ex=hkc_getter, .context=json_context);
        homekit_characteristic_t *ch1 = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_RELATIVE_HUMIDITY, 0, .getter_ex=hkc_getter, .context=json_context);
        
        ch_group_t *ch_group = malloc(sizeof(ch_group_t));
        memset(ch_group, 0, sizeof(*ch_group));
        ch_group->accessory = accessory_numerator;
        accessory_numerator++;
        ch_group->ch0 = ch0;
        ch_group->ch1 = ch1;
        ch_group->next = ch_groups;
        ch_groups = ch_group;
        
        accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[1]->id = 8;
        accessories[accessory]->services[1]->primary = true;
        accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_TEMPERATURE_SENSOR;
        accessories[accessory]->services[1]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[1]->characteristics[0] = ch0;
        
        accessories[accessory]->services[2] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[2]->id = 11;
        accessories[accessory]->services[2]->primary = false;
        accessories[accessory]->services[2]->type = HOMEKIT_SERVICE_HUMIDITY_SENSOR;
        accessories[accessory]->services[2]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[2]->characteristics[0] = ch1;
            
        ch_group->timer = malloc(sizeof(ETSTimer));
        memset(ch_group->timer, 0, sizeof(*ch_group->timer));
        sdk_os_timer_setfn(ch_group->timer, temperature_timer_worker, ch0);
        
        xTaskCreate(delayed_sensor_starter_task, "delayed_sensor_starter_task", configMINIMAL_STACK_SIZE, ch0, 1, NULL);
        
        return accessory + 1;
    }
    
    uint8_t new_lightbulb(uint8_t accessory, cJSON *json_context) {
        new_accessory(accessory, 3);
        
        if (!lightbulb_groups) {
            INFO("PWM Init");
            pwm_timer = malloc(sizeof(ETSTimer));
            memset(pwm_timer, 0, sizeof(*pwm_timer));
            sdk_os_timer_setfn(pwm_timer, rgbw_set_timer_worker, NULL);
            
            pwm_info = malloc(sizeof(pwm_info_t));
            memset(pwm_info, 0, sizeof(*pwm_info));
            
            multipwm_init(pwm_info);
            if (pwm_freq > 0) {
                multipwm_set_freq(pwm_info, pwm_freq);
            }
            pwm_info->channels = 0;
        }
        
        homekit_characteristic_t *ch0 = NEW_HOMEKIT_CHARACTERISTIC(ON, false, .getter_ex=hkc_getter, .setter_ex=hkc_rgbw_setter, .context=json_context);
        homekit_characteristic_t *ch1 = NEW_HOMEKIT_CHARACTERISTIC(BRIGHTNESS, 100, .getter_ex=hkc_getter, .setter_ex=hkc_rgbw_setter, .context=json_context);
        
        ch_group_t *ch_group = malloc(sizeof(ch_group_t));
        memset(ch_group, 0, sizeof(*ch_group));
        ch_group->accessory = accessory_numerator;
        accessory_numerator++;
        ch_group->acc_type = ACC_TYPE_LIGHTBULB;
        ch_group->ch0 = ch0;
        ch_group->ch1 = ch1;
        ch_group->next = ch_groups;
        ch_groups = ch_group;
        
        lightbulb_group_t *lightbulb_group = malloc(sizeof(lightbulb_group_t));
        memset(lightbulb_group, 0, sizeof(*lightbulb_group));
        lightbulb_group->ch0 = ch0;
        lightbulb_group->pwm_r = 255;
        lightbulb_group->pwm_g = 255;
        lightbulb_group->pwm_b = 255;
        lightbulb_group->pwm_w = 255;
        lightbulb_group->target_r = 0;
        lightbulb_group->target_g = 0;
        lightbulb_group->target_b = 0;
        lightbulb_group->target_w = 0;
        lightbulb_group->factor_r = 1;
        lightbulb_group->factor_g = 1;
        lightbulb_group->factor_b = 1;
        lightbulb_group->factor_w = 1;
        lightbulb_group->step = RGBW_STEP_DEFAULT;
        lightbulb_group->autodimmer = 0;
        lightbulb_group->armed_autodimmer = false;
        lightbulb_group->autodimmer_task_delay = AUTODIMMER_TASK_DELAY_DEFAULT;
        lightbulb_group->autodimmer_task_step = AUTODIMMER_TASK_STEP_DEFAULT;
        lightbulb_group->next = lightbulb_groups;
        lightbulb_groups = lightbulb_group;

        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_R) != NULL && pwm_info->channels < MULTIPWM_MAX_CHANNELS) {
            lightbulb_group->pwm_r = pwm_info->channels;
            pwm_info->channels++;
            multipwm_set_pin(pwm_info, lightbulb_group->pwm_r, (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_R)->valuedouble);
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_R) != NULL) {
            lightbulb_group->factor_r = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_R)->valuedouble;
        }

        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_G) != NULL && pwm_info->channels < MULTIPWM_MAX_CHANNELS) {
            lightbulb_group->pwm_g = pwm_info->channels;
            pwm_info->channels++;
            multipwm_set_pin(pwm_info, lightbulb_group->pwm_g, (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_G)->valuedouble);
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_G) != NULL) {
            lightbulb_group->factor_g = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_G)->valuedouble;
        }

        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_B) != NULL && pwm_info->channels < MULTIPWM_MAX_CHANNELS) {
            lightbulb_group->pwm_b = pwm_info->channels;
            pwm_info->channels++;
            multipwm_set_pin(pwm_info, lightbulb_group->pwm_b, (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_B)->valuedouble);
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_B) != NULL) {
            lightbulb_group->factor_b = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_B)->valuedouble;
        }

        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_W) != NULL && pwm_info->channels < MULTIPWM_MAX_CHANNELS) {
            lightbulb_group->pwm_w = pwm_info->channels;
            pwm_info->channels++;
            multipwm_set_pin(pwm_info, lightbulb_group->pwm_w, (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_W)->valuedouble);
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_W) != NULL) {
            lightbulb_group->factor_w = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_W)->valuedouble;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, RGBW_STEP) != NULL) {
            lightbulb_group->step = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_context, RGBW_STEP)->valuedouble;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, AUTODIMMER_TASK_DELAY) != NULL) {
            lightbulb_group->autodimmer_task_delay = cJSON_GetObjectItemCaseSensitive(json_context, AUTODIMMER_TASK_DELAY)->valuedouble * (1000 / portTICK_PERIOD_MS);
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, AUTODIMMER_TASK_STEP) != NULL) {
            lightbulb_group->autodimmer_task_step = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, AUTODIMMER_TASK_STEP)->valuedouble;
        }

        accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[1]->id = 8;
        accessories[accessory]->services[1]->primary = true;
        accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_LIGHTBULB;
        
        if (lightbulb_group->pwm_r != 255) {
            homekit_characteristic_t *ch2 = NEW_HOMEKIT_CHARACTERISTIC(HUE, 0, .getter_ex=hkc_getter, .setter_ex=hkc_rgbw_setter, .context=json_context);
            homekit_characteristic_t *ch3 = NEW_HOMEKIT_CHARACTERISTIC(SATURATION, 0, .getter_ex=hkc_getter, .setter_ex=hkc_rgbw_setter, .context=json_context);
            
            ch_group->ch2 = ch2;
            ch_group->ch3 = ch3;
            
            accessories[accessory]->services[1]->characteristics = calloc(5, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[1]->characteristics[0] = ch0;
            accessories[accessory]->services[1]->characteristics[1] = ch1;
            accessories[accessory]->services[1]->characteristics[2] = ch2;
            accessories[accessory]->services[1]->characteristics[3] = ch3;
            
            ch2->value.float_value = set_initial_state(accessory, 2, cJSON_Parse(INIT_STATE_LAST_STR), ch2, CH_TYPE_FLOAT, 0);
            ch3->value.float_value = set_initial_state(accessory, 3, cJSON_Parse(INIT_STATE_LAST_STR), ch3, CH_TYPE_FLOAT, 0);
        } else if (lightbulb_group->pwm_b != 255) {
            homekit_characteristic_t *ch2 = NEW_HOMEKIT_CHARACTERISTIC(COLOR_TEMPERATURE, 152, .getter_ex=hkc_getter, .setter_ex=hkc_rgbw_setter, .context=json_context);
            
            ch_group->ch2 = ch2;
            
            accessories[accessory]->services[1]->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[1]->characteristics[0] = ch0;
            accessories[accessory]->services[1]->characteristics[1] = ch1;
            accessories[accessory]->services[1]->characteristics[2] = ch2;
            
            ch2->value.int_value = set_initial_state(accessory, 2, cJSON_Parse(INIT_STATE_LAST_STR), ch2, CH_TYPE_INT32, 152);
        } else {
            accessories[accessory]->services[1]->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[1]->characteristics[0] = ch0;
            accessories[accessory]->services[1]->characteristics[1] = ch1;
        }

        ch1->value.int_value = set_initial_state(accessory, 1, cJSON_Parse(INIT_STATE_LAST_STR), ch1, CH_TYPE_INT8, 100);
        
        ch_group->timer = malloc(sizeof(ETSTimer));
        memset(ch_group->timer, 0, sizeof(*ch_group->timer));
        sdk_os_timer_setfn(ch_group->timer, hkc_rgbw_setter_delayed, ch0);

        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch0, TYPE_LIGHTBULB);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2), rgbw_brightness, ch1, LIGHTBULB_BRIGHTNESS_UP);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3), rgbw_brightness, ch1, LIGHTBULB_BRIGHTNESS_DOWN);
        
        uint8_t initial_state = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE) != NULL) {
            initial_state = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE)->valuedouble;
        }
        
        if (initial_state != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch0, TYPE_LIGHTBULB);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch0, TYPE_LIGHTBULB);
            
            ch0->value = HOMEKIT_BOOL((bool) set_initial_state(accessory, 0, json_context, ch0, CH_TYPE_BOOL, 0));
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch0, TYPE_LIGHTBULB)) {
                ch0->value = HOMEKIT_BOOL(true);
            }
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch0, TYPE_LIGHTBULB)) {
                ch0->value = HOMEKIT_BOOL(false);
            }
        }
        
        sdk_os_timer_setfn(&lightbulb_group->autodimmer_timer, no_autodimmer_called, ch0);
        
        const uint8_t new_accessory_count = build_kill_switches(accessory + 1, ch_group, json_context);
        return new_accessory_count;
    }
    
    uint8_t new_garage_door(uint8_t accessory, cJSON *json_context) {
        new_accessory(accessory, 3);
        
        homekit_characteristic_t *ch0 = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_DOOR_STATE, 1, .getter_ex=hkc_getter, .context=json_context);
        homekit_characteristic_t *ch1 = NEW_HOMEKIT_CHARACTERISTIC(TARGET_DOOR_STATE, 1, .getter_ex=hkc_getter, .setter_ex=hkc_garage_door_setter, .context=json_context);
        homekit_characteristic_t *ch2 = NEW_HOMEKIT_CHARACTERISTIC(OBSTRUCTION_DETECTED, false, .getter_ex=hkc_getter, .context=json_context);
        
        accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[1]->id = 8;
        accessories[accessory]->services[1]->primary = true;
        accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_GARAGE_DOOR_OPENER;
        accessories[accessory]->services[1]->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[1]->characteristics[0] = ch0;
        accessories[accessory]->services[1]->characteristics[1] = ch1;
        accessories[accessory]->services[1]->characteristics[2] = ch2;
        
        ch_group_t *ch_group = malloc(sizeof(ch_group_t));
        memset(ch_group, 0, sizeof(*ch_group));
        ch_group->accessory = accessory_numerator;
        accessory_numerator++;
        ch_group->acc_type = ACC_TYPE_GARAGE_DOOR;
        ch_group->ch0 = ch0;
        ch_group->ch1 = ch1;
        ch_group->ch2 = ch2;
        ch_group->num0 = GARAGE_DOOR_DEFAULT_TIME_MARGIN;
        ch_group->num1 = GARAGE_DOOR_DEFAULT_WORKING_TIME;
        ch_group->num2 = GARAGE_DOOR_DEFAULT_TIME_MARGIN;
        ch_group->next = ch_groups;
        ch_groups = ch_group;
        
        ch_group->timer = malloc(sizeof(ETSTimer));
        memset(ch_group->timer, 0, sizeof(*ch_group->timer));
        sdk_os_timer_setfn(ch_group->timer, garage_door_timer_worker, ch0);
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_MARGIN) != NULL) {
            ch_group->num2 = cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_MARGIN)->valuedouble;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_WORKING_TIME) != NULL) {
            ch_group->num1 = cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_WORKING_TIME)->valuedouble + (ch_group->num2 * 2);
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch1, TYPE_GARAGE_DOOR);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch1, TYPE_GARAGE_DOOR);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch1, TYPE_GARAGE_DOOR);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_8), garage_door_stop, ch0, 0);
        
        ch0->value.int_value = (uint8_t) set_initial_state(accessory, 0, json_context, ch0, CH_TYPE_INT8, 1);
        if (ch0->value.int_value > 1) {
            ch1->value.int_value = ch0->value.int_value - 2;
        } else {
            ch1->value.int_value = ch0->value.int_value;
        }
        
        if (ch0->value.int_value == 0) {
            ch_group->num0 = ch_group->num1 - ch_group->num2;
        }

        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3), garage_door_sensor, ch0, GARAGE_DOOR_CLOSED)) {
            garage_door_sensor(0, ch0, GARAGE_DOOR_CLOSED);
        }
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_5), garage_door_sensor, ch0, GARAGE_DOOR_CLOSING)) {
            garage_door_sensor(0, ch0, GARAGE_DOOR_OPENING);
        }
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_4), garage_door_sensor, ch0, GARAGE_DOOR_OPENING)) {
            garage_door_sensor(0, ch0, GARAGE_DOOR_OPENING);
        }
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2), garage_door_sensor, ch0, GARAGE_DOOR_OPENED)) {
            garage_door_sensor(0, ch0, GARAGE_DOOR_OPENED);
        }

        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_6), garage_door_obstruction, ch0, 0)) {
            garage_door_obstruction(0, ch0, 0);
        }
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_7), garage_door_obstruction, ch0, 1)) {
            garage_door_obstruction(0, ch0, 1);
        }
        
        const uint8_t new_accessory_count = build_kill_switches(accessory + 1, ch_group, json_context);
        return new_accessory_count;
    }
    
    uint8_t acc_count = 0;
    
    // Accessory Builder
    if (bridge_needed) {
        INFO("BRIDGE CREATED");
        new_accessory(0, 2);
        acc_count++;
    }
    
    for(uint8_t i=0; i<total_accessories; i++) {
        INFO("");
        INFO("ACCESSORY %i", accessory_numerator);
        
        uint8_t acc_type = ACC_TYPE_SWITCH;
        if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_accessories, i), ACCESSORY_TYPE) != NULL) {
            acc_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_accessories, i), ACCESSORY_TYPE)->valuedouble;
        }
        
        cJSON *json_accessory = cJSON_GetArrayItem(json_accessories, i);

        // Digital outputs GPIO Setup
        char action[3];
        for (uint8_t int_action=0; int_action<MAX_ACTIONS; int_action++) {
            itoa(int_action, action, 10);
            
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), DIGITAL_OUTPUTS_ARRAY) != NULL) {
                    cJSON *json_relays = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), DIGITAL_OUTPUTS_ARRAY);
                    
                    for(uint8_t j=0; j<cJSON_GetArraySize(json_relays); j++) {
                        const uint8_t gpio = (uint8_t) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_relays, j), PIN_GPIO)->valuedouble;
                        if (!used_gpio[gpio]) {
                            gpio_enable(gpio, GPIO_OUTPUT);
                            used_gpio[gpio] = true;
                            INFO("Digital output GPIO: %i", gpio);
                        }
                    }
                }
            }
        }
        
        // Creating HomeKit Accessory
        INFO("Type: %i", acc_type);
        if (acc_type == ACC_TYPE_BUTTON) {
            acc_count = new_button_event(acc_count, json_accessory);
            
        } else if (acc_type == ACC_TYPE_LOCK) {
            acc_count = new_lock(acc_count, json_accessory);
            
        } else if (acc_type >= ACC_TYPE_CONTACT_SENSOR && acc_type < ACC_TYPE_WATER_VALVE) {
            acc_count = new_sensor(acc_count, json_accessory, acc_type);
            
        } else if (acc_type == ACC_TYPE_WATER_VALVE) {
            acc_count = new_water_valve(acc_count, json_accessory);
        
        } else if (acc_type == ACC_TYPE_THERMOSTAT) {
            acc_count = new_thermostat(acc_count, json_accessory);
            
        } else if (acc_type == ACC_TYPE_TEMP_SENSOR) {
            acc_count = new_temp_sensor(acc_count, json_accessory);
            
        } else if (acc_type == ACC_TYPE_HUM_SENSOR) {
            acc_count = new_hum_sensor(acc_count, json_accessory);
            
        } else if (acc_type == ACC_TYPE_TH_SENSOR) {
            acc_count = new_th_sensor(acc_count, json_accessory);
            
        } else if (acc_type == ACC_TYPE_LIGHTBULB) {
            acc_count = new_lightbulb(acc_count, json_accessory);
            
        } else if (acc_type == ACC_TYPE_GARAGE_DOOR) {
            acc_count = new_garage_door(acc_count, json_accessory);
        
        } else {    // acc_type == ACC_TYPE_SWITCH || acc_type == ACC_TYPE_OUTLET
            acc_count = new_switch(acc_count, json_accessory, acc_type);
        }
        
        setup_mode_toggle_counter = INT8_MIN;
        
        vTaskDelay(ACC_CREATION_DELAY / portTICK_PERIOD_MS);
    }
    
    cJSON_Delete(json_config);
    
    // --- LIGHTBULBS INIT
    if (lightbulb_groups) {
        setpwm_bool_semaphore = false;
        
        lightbulb_group_t *lightbulb_group = lightbulb_groups;
        while (lightbulb_group) {
            hkc_rgbw_setter_delayed(lightbulb_group->ch0);
            
            lightbulb_group = lightbulb_group->next;
        }
    }
    
    // --- HOMEKIT SET CONFIG
    config.accessories = accessories;
    config.password = "021-82-017";
    config.setupId = "JOSE";
    config.category = homekit_accessory_category_other;
    config.config_number = FIRMWARE_VERSION_OCTAL;
    
    INFO("");
    FREEHEAP();
    INFO("---------------------\n");
    
    setup_mode_toggle_counter = 0;
    
    wifi_config_init("HAA", NULL, run_homekit_server, custom_hostname);
}

void user_init(void) {
#ifdef HAA_DEBUG
    sdk_os_timer_setfn(&free_heap_timer, free_heap_watchdog, NULL);
    sdk_os_timer_arm(&free_heap_timer, 2000, 1);
#endif
    
    sdk_wifi_station_disconnect();
    
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    snprintf(name_value, 11, "HAA-%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    name.value = HOMEKIT_STRING(name_value);
    
    bool haa_setup = false;
    
    //sysparam_set_bool("setup", true);    // Force to enter always in setup mode. Only for tests. Keep comment for releases
    
    sysparam_get_bool("setup", &haa_setup);
    if (haa_setup) {
        uart_set_baud(0, 115200);
        printf_header();
        INFO("SETUP MODE");
        wifi_config_init("HAA", NULL, NULL, name.value.string_value);
        
    } else {
        // Arming emergency Setup Mode
        sysparam_set_bool("setup", true);
        xTaskCreate(exit_emergency_setup_mode_task, "exit_emergency_setup_mode_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
        
        // Filling Used GPIO Array
        for (uint8_t g=0; g<18; g++) {
            used_gpio[g] = false;
        }
        
        snprintf(serial_value, 13, "%02X%02X%02X%02X%02X%02X", macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);
        serial.value = HOMEKIT_STRING(serial_value);
        
        normal_mode_init();
    }
}
