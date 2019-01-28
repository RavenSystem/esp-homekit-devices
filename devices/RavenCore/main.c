/*
 * RavenCore
 * 
 * v0.6.1
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

/* Device Types
  1. Switch 1ch
  2. Switch 2ch
  3. Socket + Button
  4. Switch 4ch
  5. Thermostat
  6. Switch Basic + TH Sensor
  7. Water Valve
  8. Garage Door
  9. Socket + Button + TH Sensor
 10. ESP01 Switch + Button
 11. Switch 3ch
 */

//#include <stdio.h>
//#include <string.h>
#include <esp/uart.h>
//#include <esp8266.h>
//#include <FreeRTOS.h>
//#include <espressif/esp_wifi.h>
//#include <espressif/esp_common.h>
#include <rboot-api.h>
#include <sysparam.h>
//#include <task.h>

//#include <etstimer.h>
#include <esplibs/libmain.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <led_codes.h>
#include <adv_button.h>

#include <dht/dht.h>
#include <ds18b20/ds18b20.h>

#include "../common/custom_characteristics.h"

// Shelly
#define S1_TOGGLE_GPIO                  5
#define S1_RELAY_GPIO                   4

#define S2_TOGGLE1_GPIO                 14
#define S2_TOGGLE2_GPIO                 12
#define S2_RELAY1_GPIO                  4
//#define S2_RELAY2_GPIO                5   // Same as RELAY2_GPIO

// Sonoff
#define LED_GPIO                        13

#define DOOR_OPENED_GPIO                4
#define DOOR_OBSTRUCTION_GPIO           5
//#define DOOR_CLOSED_GPIO              extra_gpio = 14

//#define TOGGLE_GPIO                   extra_gpio = 14

//#define BUTTON1_GPIO                  button1_gpio = 0
//#define BUTTON2_GPIO                  button2_gpio = 9
#define BUTTON3_GPIO                    10
//#define BUTTON4_GPIO                  extra_gpio = 14

//#define RELAY1_GPIO                   relay1_gpio = 12
#define RELAY2_GPIO                     5
#define RELAY3_GPIO                     4
#define RELAY4_GPIO                     15

#define DISABLED_TIME                   60
#define ALLOWED_FACTORY_RESET_TIME      120000

// SysParam
#define OTA_REPO_SYSPARAM                               "ota_repo"
#define OTA_BETA_SYSPARAM                               "ota_beta"
#define SHOW_SETUP_SYSPARAM                             "a"
#define DEVICE_TYPE_SYSPARAM                            "b"
#define BOARD_TYPE_SYSPARAM                             "c"
#define EXTERNAL_TOGGLE1_SYSPARAM                       "d"
#define EXTERNAL_TOGGLE2_SYSPARAM                       "e"
#define DHT_SENSOR_TYPE_SYSPARAM                        "f"
#define POLL_PERIOD_SYSPARAM                            "g"
#define VALVE_TYPE_SYSPARAM                             "h"
#define VALVE_SET_DURATION_SYSPARAM                     "i"
#define GARAGEDOOR_WORKING_TIME_SYSPARAM                "j"
#define GARAGEDOOR_HAS_STOP_SYSPARAM                    "k"
#define GARAGEDOOR_SENSOR_CLOSE_NC_SYSPARAM             "l"
#define GARAGEDOOR_SENSOR_OPEN_SYSPARAM                 "m"
#define GARAGEDOOR_SENSOR_OBSTRUCTION_SYSPARAM          "n"
#define GARAGEDOOR_CONTROL_WITH_BUTTON_SYSPARAM         "o"
#define GARAGEDOOR_BUTTON_TIME_SYSPARAM                 "p"
#define TARGET_TEMPERATURE_SYSPARAM                     "q"
#define INIT_STATE_SW1_SYSPARAM                         "r"
#define INIT_STATE_SW2_SYSPARAM                         "s"
#define INIT_STATE_SW3_SYSPARAM                         "t"
#define INIT_STATE_SW4_SYSPARAM                         "u"
#define INIT_STATE_TH_SYSPARAM                          "v"
#define LAST_STATE_SW1_SYSPARAM                         "w"
#define LAST_STATE_SW2_SYSPARAM                         "x"
#define LAST_STATE_SW3_SYSPARAM                         "y"
#define LAST_STATE_SW4_SYSPARAM                         "z"
#define LAST_TARGET_STATE_TH_SYSPARAM                   "0"
#define LOG_OUTPUT_SYSPARAM                             "1"
#define HUM_OFFSET_SYSPARAM                             "2"
#define TEMP_OFFSET_SYSPARAM                            "3"
#define REVERSE_SW1_SYSPARAM                            "4"
#define REVERSE_SW2_SYSPARAM                            "5"
#define REVERSE_SW3_SYSPARAM                            "6"
#define REVERSE_SW4_SYSPARAM                            "7"

bool gd_is_moving = false;
uint8_t device_type_static = 1, reset_toggle_counter = 0, gd_time_state = 0, button1_gpio = 0, button2_gpio = 9, relay1_gpio = 12, extra_gpio = 14;
volatile uint32_t last_press_time;
volatile float old_humidity_value = 0.0, old_temperature_value = 0.0;
ETSTimer device_restart_timer, factory_default_toggle_timer, change_settings_timer, save_states_timer, extra_func_timer;

void switch1_on_callback(homekit_value_t value);
homekit_value_t read_switch1_on_callback();
void switch2_on_callback(homekit_value_t value);
homekit_value_t read_switch2_on_callback();
void switch3_on_callback(homekit_value_t value);
homekit_value_t read_switch3_on_callback();
void switch4_on_callback(homekit_value_t value);
homekit_value_t read_switch4_on_callback();

void on_target(homekit_value_t value);
homekit_value_t read_on_target();
void update_state();

void valve_on_callback(homekit_value_t value);
homekit_value_t read_valve_on_callback();
homekit_value_t read_in_use_on_callback();
homekit_value_t read_remaining_duration_on_callback();

void garage_on_callback(homekit_value_t value);
homekit_value_t read_garage_on_callback();

void show_setup_callback();
void ota_firmware_callback();
void change_settings_callback();
void save_states_callback();
homekit_value_t read_ip_addr();

void on_wifi_ready();

// ---------- CHARACTERISTICS ----------
// Switches
homekit_characteristic_t switch1_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter=read_switch1_on_callback, .setter=switch1_on_callback);
homekit_characteristic_t switch2_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter=read_switch2_on_callback, .setter=switch2_on_callback);
homekit_characteristic_t switch3_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter=read_switch3_on_callback, .setter=switch3_on_callback);
homekit_characteristic_t switch4_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter=read_switch4_on_callback, .setter=switch4_on_callback);

// For Outlets
homekit_characteristic_t switch_outlet_in_use = HOMEKIT_CHARACTERISTIC_(OUTLET_IN_USE, true);   // It has not a real use, but it is a mandatory characteristic

// Stateless Button
homekit_characteristic_t button_event = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);

// Thermostat
homekit_characteristic_t current_temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t target_temperature  = HOMEKIT_CHARACTERISTIC_(TARGET_TEMPERATURE, 23, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(update_state));
homekit_characteristic_t units = HOMEKIT_CHARACTERISTIC_(TEMPERATURE_DISPLAY_UNITS, 0);
homekit_characteristic_t current_state = HOMEKIT_CHARACTERISTIC_(CURRENT_HEATING_COOLING_STATE, 0);
homekit_characteristic_t target_state = HOMEKIT_CHARACTERISTIC_(TARGET_HEATING_COOLING_STATE, 0, .getter=read_on_target, .setter=on_target);
homekit_characteristic_t current_humidity = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);

// Water Valve
homekit_characteristic_t active = HOMEKIT_CHARACTERISTIC_(ACTIVE, 0, .getter=read_valve_on_callback, .setter=valve_on_callback);
homekit_characteristic_t in_use = HOMEKIT_CHARACTERISTIC_(IN_USE, 0, .getter=read_in_use_on_callback);
homekit_characteristic_t valve_type = HOMEKIT_CHARACTERISTIC_(VALVE_TYPE, 0);
homekit_characteristic_t set_duration = HOMEKIT_CHARACTERISTIC_(SET_DURATION, 900, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(save_states_callback));
homekit_characteristic_t remaining_duration = HOMEKIT_CHARACTERISTIC_(REMAINING_DURATION, 0, .getter=read_remaining_duration_on_callback);

// Garage Door
homekit_characteristic_t current_door_state = HOMEKIT_CHARACTERISTIC_(CURRENT_DOOR_STATE, 1);
homekit_characteristic_t target_door_state = HOMEKIT_CHARACTERISTIC_(TARGET_DOOR_STATE, 1, .getter=read_garage_on_callback, .setter=garage_on_callback);
homekit_characteristic_t obstruction_detected = HOMEKIT_CHARACTERISTIC_(OBSTRUCTION_DETECTED, false);

// ---------- SETUP ----------
// General Setup
homekit_characteristic_t show_setup = HOMEKIT_CHARACTERISTIC_(CUSTOM_SHOW_SETUP, true, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(show_setup_callback));
homekit_characteristic_t device_type = HOMEKIT_CHARACTERISTIC_(CUSTOM_DEVICE_TYPE, 1, .id=102, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t board_type = HOMEKIT_CHARACTERISTIC_(CUSTOM_BOARD_TYPE, 1, .id=126, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t reboot_device = HOMEKIT_CHARACTERISTIC_(CUSTOM_REBOOT_DEVICE, false, .id=103, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(ota_firmware_callback));
homekit_characteristic_t ota_firmware = HOMEKIT_CHARACTERISTIC_(CUSTOM_OTA_UPDATE, false, .id=110, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(ota_firmware_callback));
homekit_characteristic_t ota_beta = HOMEKIT_CHARACTERISTIC_(CUSTOM_OTA_BETA, false, .id=134, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t log_output = HOMEKIT_CHARACTERISTIC_(CUSTOM_LOG_OUTPUT, 1, .id=129, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t ip_addr = HOMEKIT_CHARACTERISTIC_(CUSTOM_IP_ADDR, "", .id=130, .getter=read_ip_addr);
homekit_characteristic_t wifi_reset = HOMEKIT_CHARACTERISTIC_(CUSTOM_WIFI_RESET, false, .id=131, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

// Switch Setup
homekit_characteristic_t external_toggle1 = HOMEKIT_CHARACTERISTIC_(CUSTOM_EXTERNAL_TOGGLE1, 0, .id=111, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t external_toggle2 = HOMEKIT_CHARACTERISTIC_(CUSTOM_EXTERNAL_TOGGLE2, 0, .id=128, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

// Thermostat Setup
homekit_characteristic_t dht_sensor_type = HOMEKIT_CHARACTERISTIC_(CUSTOM_DHT_SENSOR_TYPE, 2, .id=112, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t hum_offset = HOMEKIT_CHARACTERISTIC_(CUSTOM_HUMIDITY_OFFSET, 0, .id=132, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t temp_offset = HOMEKIT_CHARACTERISTIC_(CUSTOM_TEMPERATURE_OFFSET, 0.0, .id=133, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t poll_period = HOMEKIT_CHARACTERISTIC_(CUSTOM_TH_PERIOD, 30, .id=125, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

// Water Valve Setup
homekit_characteristic_t custom_valve_type = HOMEKIT_CHARACTERISTIC_(CUSTOM_VALVE_TYPE, 0, .id=113, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

// Garage Door Setup
homekit_characteristic_t custom_garagedoor_has_stop = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_HAS_STOP, false, .id=114, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_sensor_close_nc = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_SENSOR_CLOSE_NC, false, .id=115, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_sensor_open = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_SENSOR_OPEN, 0, .id=116, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_button_time = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_BUTTON_TIME, 0.3, .id=117, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_working_time = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_WORKINGTIME, 20, .id=118, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_control_with_button = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_CONTROL_WITH_BUTTON, false, .id=119, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_sensor_obstruction = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_SENSOR_OBSTRUCTION, 0, .id=127, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

// Initial State Setup
homekit_characteristic_t custom_init_state_sw1 = HOMEKIT_CHARACTERISTIC_(CUSTOM_INIT_STATE_SW1, 0, .id=120, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_init_state_sw2 = HOMEKIT_CHARACTERISTIC_(CUSTOM_INIT_STATE_SW2, 0, .id=121, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_init_state_sw3 = HOMEKIT_CHARACTERISTIC_(CUSTOM_INIT_STATE_SW3, 0, .id=122, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_init_state_sw4 = HOMEKIT_CHARACTERISTIC_(CUSTOM_INIT_STATE_SW4, 0, .id=123, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_init_state_th = HOMEKIT_CHARACTERISTIC_(CUSTOM_INIT_STATE_TH, 0, .id=124, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

// Reverse relays
homekit_characteristic_t custom_reverse_sw1 = HOMEKIT_CHARACTERISTIC_(CUSTOM_REVERSE_SW1, false, .id=135, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_reverse_sw2 = HOMEKIT_CHARACTERISTIC_(CUSTOM_REVERSE_SW2, false, .id=136, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_reverse_sw3 = HOMEKIT_CHARACTERISTIC_(CUSTOM_REVERSE_SW3, false, .id=137, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_reverse_sw4 = HOMEKIT_CHARACTERISTIC_(CUSTOM_REVERSE_SW4, false, .id=138, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

// Last used ID = 138
// ---------------------------

void relay_write(bool on, const uint8_t gpio) {
    if (custom_reverse_sw1.value.bool_value && gpio == relay1_gpio ) {
        on = !on;
    } else if (custom_reverse_sw2.value.bool_value && gpio == RELAY2_GPIO ) {
        on = !on;
    } else if (custom_reverse_sw3.value.bool_value && gpio == RELAY3_GPIO ) {
        on = !on;
    } else if (custom_reverse_sw4.value.bool_value && gpio == RELAY4_GPIO ) {
        on = !on;
    }
    
    last_press_time = xTaskGetTickCountFromISR() + (DISABLED_TIME / portTICK_PERIOD_MS);
    adv_button_set_disable_time();
    gpio_write(gpio, on ? 1 : 0);
}

void on_target(homekit_value_t value) {
    target_state.value = value;
    switch (target_state.value.int_value) {
        case 1:
            printf("RC > HEAT\n");
            led_code(LED_GPIO, FUNCTION_B);
            break;
            
        case 2:
            printf("RC > COOL\n");
            led_code(LED_GPIO, FUNCTION_C);
            break;
            
        default:
            printf("RC > OFF\n");
            led_code(LED_GPIO, FUNCTION_A);
            break;
    }

    update_state();
}

homekit_value_t read_on_target() {
    return target_state.value;
}

void update_state() {
    uint8_t state = target_state.value.int_value;
    if (state == 3) {
        state = 0;
        target_state.value = HOMEKIT_UINT8(0);
        homekit_characteristic_notify(&target_state, target_state.value);
    }
    if (state == 1 && current_temperature.value.float_value < target_temperature.value.float_value) {
        if (current_state.value.int_value != 1) {
            current_state.value = HOMEKIT_UINT8(1);
            homekit_characteristic_notify(&current_state, current_state.value);
            
            relay_write(true, relay1_gpio);
        }
    } else if (state == 2 && current_temperature.value.float_value > target_temperature.value.float_value) {
        if (current_state.value.int_value != 2) {
            current_state.value = HOMEKIT_UINT8(2);
            homekit_characteristic_notify(&current_state, current_state.value);
            
            relay_write(true, relay1_gpio);
        }
    } else if (current_state.value.int_value != 0) {
        current_state.value = HOMEKIT_UINT8(0);
        homekit_characteristic_notify(&current_state, current_state.value);
        
        relay_write(false, relay1_gpio);
    }
    
    save_states_callback();
}

void save_settings() {
    printf("RC > Saving settings\n");
    
    sysparam_status_t status, flash_error = SYSPARAM_OK;
    
    status = sysparam_set_bool(SHOW_SETUP_SYSPARAM, show_setup.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_bool(OTA_BETA_SYSPARAM, ota_beta.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(DEVICE_TYPE_SYSPARAM, device_type.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(BOARD_TYPE_SYSPARAM, board_type.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    if (board_type.value.int_value == 4) {
        log_output.value.int_value = 0;
    }
    
    status = sysparam_set_int8(LOG_OUTPUT_SYSPARAM, log_output.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(EXTERNAL_TOGGLE1_SYSPARAM, external_toggle1.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(EXTERNAL_TOGGLE2_SYSPARAM, external_toggle2.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }

    status = sysparam_set_int8(DHT_SENSOR_TYPE_SYSPARAM, dht_sensor_type.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(HUM_OFFSET_SYSPARAM, hum_offset.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int32(TEMP_OFFSET_SYSPARAM, temp_offset.value.float_value * 100);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(POLL_PERIOD_SYSPARAM, poll_period.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }

    status = sysparam_set_int8(VALVE_TYPE_SYSPARAM, custom_valve_type.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }

    status = sysparam_set_int8(GARAGEDOOR_WORKING_TIME_SYSPARAM, custom_garagedoor_working_time.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_bool(GARAGEDOOR_HAS_STOP_SYSPARAM, custom_garagedoor_has_stop.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_bool(GARAGEDOOR_SENSOR_CLOSE_NC_SYSPARAM, custom_garagedoor_sensor_close_nc.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(GARAGEDOOR_SENSOR_OPEN_SYSPARAM, custom_garagedoor_sensor_open.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(GARAGEDOOR_SENSOR_OBSTRUCTION_SYSPARAM, custom_garagedoor_sensor_obstruction.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_bool(GARAGEDOOR_CONTROL_WITH_BUTTON_SYSPARAM, custom_garagedoor_control_with_button.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int32(GARAGEDOOR_BUTTON_TIME_SYSPARAM, custom_garagedoor_button_time.value.float_value * 100);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(INIT_STATE_SW1_SYSPARAM, custom_init_state_sw1.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(INIT_STATE_SW2_SYSPARAM, custom_init_state_sw2.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(INIT_STATE_SW3_SYSPARAM, custom_init_state_sw3.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(INIT_STATE_SW4_SYSPARAM, custom_init_state_sw4.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(INIT_STATE_TH_SYSPARAM, custom_init_state_th.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_bool(REVERSE_SW1_SYSPARAM, custom_reverse_sw1.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_bool(REVERSE_SW2_SYSPARAM, custom_reverse_sw2.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_bool(REVERSE_SW3_SYSPARAM, custom_reverse_sw3.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_bool(REVERSE_SW4_SYSPARAM, custom_reverse_sw4.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    if (flash_error != SYSPARAM_OK) {
        printf("RC ! Saving settings error -> %i\n", flash_error);
    }
}

void save_states() {
    printf("RC > Saving last states\n");
    
    sysparam_status_t status, flash_error = SYSPARAM_OK;
    
    if (custom_init_state_sw1.value.int_value > 1) {
        status = sysparam_set_bool(LAST_STATE_SW1_SYSPARAM, switch1_on.value.bool_value);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    if (custom_init_state_sw2.value.int_value > 1) {
        status = sysparam_set_bool(LAST_STATE_SW2_SYSPARAM, switch2_on.value.bool_value);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    if (custom_init_state_sw3.value.int_value > 1) {
        status = sysparam_set_bool(LAST_STATE_SW3_SYSPARAM, switch3_on.value.bool_value);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    if (custom_init_state_sw4.value.int_value > 1) {
        status = sysparam_set_bool(LAST_STATE_SW4_SYSPARAM, switch4_on.value.bool_value);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    if (custom_init_state_th.value.int_value > 2) {
        status = sysparam_set_int8(LAST_TARGET_STATE_TH_SYSPARAM, target_state.value.int_value);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_set_int32(TARGET_TEMPERATURE_SYSPARAM, target_temperature.value.float_value * 100);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int32(VALVE_SET_DURATION_SYSPARAM, set_duration.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    if (flash_error != SYSPARAM_OK) {
        printf("RC ! Saving last states error -> %i\n", flash_error);
    }
}

void device_restart_task() {
    vTaskDelay(5500 / portTICK_PERIOD_MS);
    
    if (wifi_reset.value.bool_value) {
        wifi_config_reset();
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    
    if (ota_firmware.value.bool_value) {
        rboot_set_temp_rom(1);
        vTaskDelay(150 / portTICK_PERIOD_MS);
    }
    
    sdk_system_restart();
    
    vTaskDelete(NULL);
}

void device_restart() {
    printf("RC > Restarting device\n");
    led_code(LED_GPIO, RESTART_DEVICE);
    xTaskCreate(device_restart_task, "device_restart_task", 256, NULL, 1, NULL);
}

void show_setup_callback() {
    if (show_setup.value.bool_value) {
        sdk_os_timer_setfn(&device_restart_timer, device_restart, NULL);
        sdk_os_timer_arm(&device_restart_timer, 5000, 0);
    } else {
        sdk_os_timer_disarm(&device_restart_timer);
    }
    
    save_settings();
}

void factory_default_task() {
    printf("RC > Resetting to factory defaults\n");
    
    relay_write(false, relay1_gpio);
    gpio_disable(relay1_gpio);
    
    sysparam_status_t status;
    
    status = sysparam_set_bool(SHOW_SETUP_SYSPARAM, true);
    status = sysparam_set_bool(OTA_BETA_SYSPARAM, false);
    //status = sysparam_set_int8(BOARD_TYPE_SYSPARAM, 1);   // This value is not reseted
    
    if (board_type.value.int_value == 4) {
        status = sysparam_set_int8(DEVICE_TYPE_SYSPARAM, 0);
    } else {
        status = sysparam_set_int8(DEVICE_TYPE_SYSPARAM, 1);
    }
    
    status = sysparam_set_int8(LOG_OUTPUT_SYSPARAM, 1);
    
    status = sysparam_set_int8(EXTERNAL_TOGGLE1_SYSPARAM, 0);
    status = sysparam_set_int8(EXTERNAL_TOGGLE2_SYSPARAM, 0);
    
    status = sysparam_set_int8(DHT_SENSOR_TYPE_SYSPARAM, 2);
    status = sysparam_set_int8(HUM_OFFSET_SYSPARAM, 0);
    status = sysparam_set_int32(TEMP_OFFSET_SYSPARAM, 0 * 100);
    status = sysparam_set_int8(POLL_PERIOD_SYSPARAM, 30);
    
    status = sysparam_set_int8(VALVE_TYPE_SYSPARAM, 0);
    status = sysparam_set_int32(VALVE_SET_DURATION_SYSPARAM, 900);
    
    status = sysparam_set_int8(GARAGEDOOR_WORKING_TIME_SYSPARAM, 20);
    status = sysparam_set_bool(GARAGEDOOR_HAS_STOP_SYSPARAM, false);
    status = sysparam_set_bool(GARAGEDOOR_SENSOR_CLOSE_NC_SYSPARAM, false);
    status = sysparam_set_int8(GARAGEDOOR_SENSOR_OPEN_SYSPARAM, 0);
    status = sysparam_set_int8(GARAGEDOOR_SENSOR_OBSTRUCTION_SYSPARAM, 0);
    status = sysparam_set_bool(GARAGEDOOR_CONTROL_WITH_BUTTON_SYSPARAM, false);
    status = sysparam_set_int32(GARAGEDOOR_BUTTON_TIME_SYSPARAM, 0.3 * 100);
    
    status = sysparam_set_int32(TARGET_TEMPERATURE_SYSPARAM, 23 * 100);
    status = sysparam_set_int8(INIT_STATE_SW1_SYSPARAM, 0);
    status = sysparam_set_int8(INIT_STATE_SW2_SYSPARAM, 0);
    status = sysparam_set_int8(INIT_STATE_SW3_SYSPARAM, 0);
    status = sysparam_set_int8(INIT_STATE_SW4_SYSPARAM, 0);
    status = sysparam_set_int8(INIT_STATE_TH_SYSPARAM, 0);
    
    status = sysparam_set_bool(REVERSE_SW1_SYSPARAM, false);
    status = sysparam_set_bool(REVERSE_SW2_SYSPARAM, false);
    status = sysparam_set_bool(REVERSE_SW3_SYSPARAM, false);
    status = sysparam_set_bool(REVERSE_SW4_SYSPARAM, false);
    
    status = sysparam_set_bool(LAST_STATE_SW1_SYSPARAM, false);
    status = sysparam_set_bool(LAST_STATE_SW2_SYSPARAM, false);
    status = sysparam_set_bool(LAST_STATE_SW3_SYSPARAM, false);
    status = sysparam_set_bool(LAST_STATE_SW4_SYSPARAM, false);
    status = sysparam_set_int8(LAST_TARGET_STATE_TH_SYSPARAM, 0);
    
    if (status != SYSPARAM_OK) {
        printf("RC ! ERROR Flash problem\n");
    }
    
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    homekit_server_reset();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    wifi_config_reset();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    led_code(LED_GPIO, EXTRA_CONFIG_RESET);
    vTaskDelay(2200 / portTICK_PERIOD_MS);
    
    sdk_system_restart();
    
    vTaskDelete(NULL);
}

void factory_default_call(const uint8_t gpio) {
    printf("RC > Checking factory default call\n");
    
    if (xTaskGetTickCountFromISR() < ALLOWED_FACTORY_RESET_TIME / portTICK_PERIOD_MS) {
        led_code(LED_GPIO, WIFI_CONFIG_RESET);
        xTaskCreate(factory_default_task, "factory_default_task", 256, NULL, 1, NULL);
    } else {
        printf("RC ! Reset to factory defaults not allowed after %i msecs since boot. Repower device and try again\n", ALLOWED_FACTORY_RESET_TIME);
    }
}

void factory_default_toggle() {
    if (reset_toggle_counter > 10) {
        factory_default_call(0);
    }
    
    reset_toggle_counter = 0;
}

void ota_firmware_callback() {
    if (ota_firmware.value.bool_value || reboot_device.value.bool_value) {
        sdk_os_timer_setfn(&device_restart_timer, device_restart, NULL);
        sdk_os_timer_arm(&device_restart_timer, 5000, 0);
    } else {
        sdk_os_timer_disarm(&device_restart_timer);
    }
}

void change_settings_callback() {
    sdk_os_timer_disarm(&change_settings_timer);
    sdk_os_timer_arm(&change_settings_timer, 3000, 0);
}

void save_states_callback() {
    sdk_os_timer_disarm(&save_states_timer);
    sdk_os_timer_arm(&save_states_timer, 3000, 0);
}

void switch1_on_callback(homekit_value_t value) {
    printf("RC > Toggle SW 1\n");
    switch1_on.value = value;
    relay_write(switch1_on.value.bool_value, relay1_gpio);
    led_code(LED_GPIO, FUNCTION_A);
    printf("RC > Relay 1 -> %i\n", switch1_on.value.bool_value);
    save_states_callback();
}

homekit_value_t read_switch1_on_callback() {
    return switch1_on.value;
}

void switch2_on_callback(homekit_value_t value) {
    printf("RC > Toggle SW 2\n");
    switch2_on.value = value;
    relay_write(switch2_on.value.bool_value, RELAY2_GPIO);
    led_code(LED_GPIO, FUNCTION_A);
    printf("RC > Relay 2 -> %i\n", switch2_on.value.bool_value);
    save_states_callback();
}

homekit_value_t read_switch2_on_callback() {
    return switch2_on.value;
}

void switch3_on_callback(homekit_value_t value) {
    printf("RC > Toggle SW 3\n");
    switch3_on.value = value;
    relay_write(switch3_on.value.bool_value, RELAY3_GPIO);
    led_code(LED_GPIO, FUNCTION_A);
    printf("RC > Relay 3 -> %i\n", switch3_on.value.bool_value);
    save_states_callback();
}

homekit_value_t read_switch3_on_callback() {
    return switch3_on.value;
}

void switch4_on_callback(homekit_value_t value) {
    printf("RC > Toggle SW 4\n");
    switch4_on.value = value;
    relay_write(switch4_on.value.bool_value, RELAY4_GPIO);
    led_code(LED_GPIO, FUNCTION_A);
    printf("RC > Relay 4 -> %i\n", switch4_on.value.bool_value);
    save_states_callback();
}

homekit_value_t read_switch4_on_callback() {
    return switch4_on.value;
}

void toggle_valve(const uint8_t gpio) {
    if (active.value.int_value == 1) {
        active.value.int_value = 0;
    } else {
        active.value.int_value = 1;
    }
    
    homekit_characteristic_notify(&active, active.value);
    valve_on_callback(active.value);
}

void valve_control() {
    remaining_duration.value.int_value--;
    if (remaining_duration.value.int_value == 0) {
        printf("RC > Valve OFF\n");
        
        sdk_os_timer_disarm(&extra_func_timer);
        relay_write(false, relay1_gpio);
        
        active.value.int_value = 0;
        in_use.value.int_value = 0;
        
        homekit_characteristic_notify(&active, active.value);
        homekit_characteristic_notify(&in_use, in_use.value);
        
        led_code(LED_GPIO, FUNCTION_D);
    }
}

void valve_on_callback(homekit_value_t value) {
    active.value = value;
    in_use.value.int_value = active.value.int_value;
    
    if (active.value.int_value == 1) {
        printf("RC > Valve ON\n");
        relay_write(true, relay1_gpio);
        remaining_duration.value = set_duration.value;
        sdk_os_timer_arm(&extra_func_timer, 1000, 1);
    } else {
        printf("RC > Valve manual OFF\n");
        sdk_os_timer_disarm(&extra_func_timer);
        relay_write(false, relay1_gpio);
        remaining_duration.value.int_value = 0;
    }
    
    homekit_characteristic_notify(&in_use, in_use.value);
    homekit_characteristic_notify(&remaining_duration, remaining_duration.value);
    
    led_code(LED_GPIO, FUNCTION_A);
}

homekit_value_t read_valve_on_callback() {
    return active.value;
}

homekit_value_t read_in_use_on_callback() {
    return in_use.value;
}

homekit_value_t read_remaining_duration_on_callback() {
    return remaining_duration.value;
}

void garage_button_task() {
    printf("RC > Garage Door relay working\n");
    if (custom_garagedoor_sensor_open.value.int_value == 0) {
        sdk_os_timer_disarm(&extra_func_timer);
    }
    
    if (obstruction_detected.value.bool_value == false) {
        relay_write(true, relay1_gpio);
        vTaskDelay(custom_garagedoor_button_time.value.float_value * 1000 / portTICK_PERIOD_MS);
        relay_write(false, relay1_gpio);

        if (custom_garagedoor_has_stop.value.bool_value && gd_is_moving) {
            vTaskDelay(2500 / portTICK_PERIOD_MS);
            relay_write(true, relay1_gpio);
            vTaskDelay(custom_garagedoor_button_time.value.float_value * 1000 / portTICK_PERIOD_MS);
            relay_write(false, relay1_gpio);
        }

        if (custom_garagedoor_sensor_open.value.int_value == 0) {
            if (current_door_state.value.int_value == 0 || current_door_state.value.int_value == 2) {
                printf("RC > Garage Door -> CLOSING\n");
                current_door_state.value.int_value = 3;
                sdk_os_timer_arm(&extra_func_timer, 1000, 1);
            } else if (current_door_state.value.int_value == 3) {
                printf("RC > Garage Door -> OPENING\n");
                current_door_state.value.int_value = 2;
                sdk_os_timer_arm(&extra_func_timer, 1000, 1);
            }
        }
    } else {
        printf("RC ! Garage Door -> OBSTRUCTION DETECTED - RELAY IS OFF\n");
    }

    homekit_characteristic_notify(&current_door_state, current_door_state.value);
    
    vTaskDelete(NULL);
}

void garage_on_callback(homekit_value_t value) {
    printf("RC > Garage Door activated: Current state -> %i, Target state -> %i\n", current_door_state.value.int_value, value.int_value);
    
    uint8_t current_door_state_simple = current_door_state.value.int_value;
    if (current_door_state_simple > 1) {
        current_door_state_simple -= 2;
    }
    
    if (value.int_value != current_door_state_simple) {
        xTaskCreate(garage_button_task, "garage_button_task", 192, NULL, 1, NULL);
        led_code(LED_GPIO, FUNCTION_A);
    } else {
        led_code(LED_GPIO, FUNCTION_D);
    }
    
    target_door_state.value = value;
}

void garage_on_button(const uint8_t gpio) {
    if (custom_garagedoor_control_with_button.value.bool_value) {
        printf("RC > Garage Door: built-in button PRESSED\n");
        
        if (target_door_state.value.int_value == 0) {
            garage_on_callback(HOMEKIT_UINT8(1));
        } else {
            garage_on_callback(HOMEKIT_UINT8(0));
        }
    } else {
        printf("RC > Garage Door: built-in button DISABLED\n");
        led_code(LED_GPIO, FUNCTION_D);
    }
}

homekit_value_t read_garage_on_callback() {
    printf("RC > Garage Door: returning target_door_state -> %i\n", target_door_state.value.int_value);
    return target_door_state.value;
}

static void homekit_gd_notify() {
    homekit_characteristic_notify(&target_door_state, target_door_state.value);
    homekit_characteristic_notify(&current_door_state, current_door_state.value);
}

void door_opened_0_fn_callback(const uint8_t gpio) {
    printf("RC > Garage Door -> CLOSING\n");
    gd_is_moving = true;
    target_door_state.value.int_value = 1;
    current_door_state.value.int_value = 3;
    
    homekit_gd_notify();
}

void door_opened_1_fn_callback(const uint8_t gpio) {
    printf("RC > Garage Door -> OPENED\n");
    gd_is_moving = false;
    target_door_state.value.int_value = 0;
    current_door_state.value.int_value = 0;
    
    homekit_gd_notify();
}

void door_closed_0_fn_callback(const uint8_t gpio) {
    printf("RC > Garage Door -> OPENING\n");
    gd_is_moving = true;
    target_door_state.value.int_value = 0;
    current_door_state.value.int_value = 2;
    
    if (custom_garagedoor_sensor_open.value.int_value == 0) {
        sdk_os_timer_arm(&extra_func_timer, 1000, 1);
    }
    
    homekit_gd_notify();
}

void door_closed_1_fn_callback(const uint8_t gpio) {
    printf("RC > Garage Door -> CLOSED\n");
    gd_time_state = 0;
    gd_is_moving = false;
    target_door_state.value.int_value = 1;
    current_door_state.value.int_value = 1;
    
    if (custom_garagedoor_sensor_open.value.int_value == 0) {
        sdk_os_timer_disarm(&extra_func_timer);
    }
    
    homekit_gd_notify();
}

void door_opened_countdown_timer() {
    if (gd_time_state > custom_garagedoor_working_time.value.int_value) {
        gd_time_state = custom_garagedoor_working_time.value.int_value - 1;
    }
    
    if (current_door_state.value.int_value == 2) {
        gd_time_state++;
    
        if (gd_time_state == custom_garagedoor_working_time.value.int_value) {
            printf("RC > Garage Door -> OPENED\n");
            sdk_os_timer_disarm(&extra_func_timer);
            gd_is_moving = false;
            gd_time_state = custom_garagedoor_working_time.value.int_value;
            target_door_state.value.int_value = 0;
            current_door_state.value.int_value = 0;
            
            homekit_gd_notify();
        }
    } else if (current_door_state.value.int_value == 3) {
        gd_time_state--;
         if (gd_time_state == 0) {
             printf("RC > Garage Door -> CLOSED\n");
             sdk_os_timer_disarm(&extra_func_timer);
             gd_is_moving = false;
             target_door_state.value.int_value = 1;
             current_door_state.value.int_value = 1;
             
             homekit_gd_notify();
         }
    }
}

void door_obstructed_0_fn_callback(const uint8_t gpio) {
    printf("RC > Garage Door -> OBSTRUCTION REMOVED\n");
    obstruction_detected.value.bool_value = false;
    homekit_characteristic_notify(&obstruction_detected, obstruction_detected.value);
}

void door_obstructed_1_fn_callback(const uint8_t gpio) {
    printf("RC > Garage Door -> OBSTRUCTED\n");
    obstruction_detected.value.bool_value = true;
    homekit_characteristic_notify(&obstruction_detected, obstruction_detected.value);
}

void button_simple1_intr_callback(const uint8_t gpio) {
    switch1_on.value.bool_value = !switch1_on.value.bool_value;
    switch1_on_callback(switch1_on.value);
    homekit_characteristic_notify(&switch1_on, switch1_on.value);
    
    reset_toggle_counter++;
    sdk_os_timer_arm(&factory_default_toggle_timer, 3100, 0);
}

void button_simple2_intr_callback(const uint8_t gpio) {
    switch2_on.value.bool_value = !switch2_on.value.bool_value;
    switch2_on_callback(switch2_on.value);
    homekit_characteristic_notify(&switch2_on, switch2_on.value);
}

void button_simple3_intr_callback(const uint8_t gpio) {
    switch3_on.value.bool_value = !switch3_on.value.bool_value;
    switch3_on_callback(switch3_on.value);
    homekit_characteristic_notify(&switch3_on, switch3_on.value);
}

void button_simple4_intr_callback(const uint8_t gpio) {
    switch4_on.value.bool_value = !switch4_on.value.bool_value;
    switch4_on_callback(switch4_on.value);
    homekit_characteristic_notify(&switch4_on, switch4_on.value);
}

void button_event1_intr_callback(const uint8_t gpio) {
    homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(0));
    led_code(LED_GPIO, FUNCTION_A);
}

void button_event2_intr_callback(const uint8_t gpio) {
    homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(1));
    led_code(LED_GPIO, FUNCTION_B);
}

void button_event3_intr_callback(const uint8_t gpio) {
    homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(2));
    led_code(LED_GPIO, FUNCTION_C);
}

void th_button_intr_callback(const uint8_t gpio) {
    uint8_t state = target_state.value.int_value + 1;
    switch (state) {
        case 1:
            printf("RC > HEAT\n");
            led_code(LED_GPIO, FUNCTION_B);
            break;
            
        case 2:
            printf("RC > COOL\n");
            led_code(LED_GPIO, FUNCTION_C);
            break;

        default:
            state = 0;
            printf("RC > OFF\n");
            led_code(LED_GPIO, FUNCTION_A);
            break;
    }
    
    target_state.value = HOMEKIT_UINT8(state);
    homekit_characteristic_notify(&target_state, target_state.value);
    
    update_state();
}

void temperature_sensor_worker() {
    float humidity_value, temperature_value;
    bool get_temp = false;
    
    if (dht_sensor_type.value.int_value < 3) {
        dht_sensor_type_t current_sensor_type = DHT_TYPE_DHT22;
        
        if (dht_sensor_type.value.int_value == 1) {
            current_sensor_type = DHT_TYPE_DHT11;
        }
        
        get_temp = dht_read_float_data(current_sensor_type, extra_gpio, &humidity_value, &temperature_value);
    } else {    // dht_sensor_type.value.int_value == 3
        ds18b20_addr_t ds18b20_addr[1];
        
        if (ds18b20_scan_devices(extra_gpio, ds18b20_addr, 1) == 1) {
            temperature_value = ds18b20_read_single(extra_gpio);
            humidity_value = 0.0;
            get_temp = true;
        }
    }
    
    if (get_temp) {
        temperature_value += temp_offset.value.float_value;
        if (temperature_value < -100) {
            temperature_value = -100;
        } else if (temperature_value > 200) {
            temperature_value = 200;
        }
        
        if (temperature_value != old_temperature_value) {
            old_temperature_value = temperature_value;
            current_temperature.value = HOMEKIT_FLOAT(temperature_value);
            homekit_characteristic_notify(&current_temperature, current_temperature.value);

            if (device_type_static == 5) {
                update_state();
            }
        }
        
        humidity_value += hum_offset.value.float_value;
        if (humidity_value < 0) {
            humidity_value = 0;
        } else if (humidity_value > 100) {
            humidity_value = 100;
        }
        
        if (humidity_value != old_humidity_value) {
            old_humidity_value = humidity_value;
            current_humidity.value = HOMEKIT_FLOAT(humidity_value);
            homekit_characteristic_notify(&current_humidity, current_humidity.value);
        }
        
        printf("RC > TEMP %g, HUM %g\n", temperature_value, humidity_value);
    } else {
        printf("RC ! ERROR Sensor\n");
        led_code(LED_GPIO, SENSOR_ERROR);
        
        if (current_state.value.int_value != 0 && device_type_static == 5) {
            current_state.value = HOMEKIT_UINT8(0);
            homekit_characteristic_notify(&current_state, current_state.value);
            
            relay_write(false, relay1_gpio);
        }
    }
}

void identify_task() {
    relay_write(false, relay1_gpio);
    
    for (uint8_t i = 0; i < 3; i++) {
        relay_write(true, relay1_gpio);
        vTaskDelay(400 / portTICK_PERIOD_MS);
        relay_write(false, relay1_gpio);
        vTaskDelay(400 / portTICK_PERIOD_MS);
    }
    
    vTaskDelete(NULL);
}

void identify(homekit_value_t _value) {
    printf("RC > Identifying\n");

    switch (device_type_static) {
        case 12:
        case 13:
            xTaskCreate(identify_task, "identify_task", 256, NULL, 1, NULL);
            break;
            
        default:
            led_code(LED_GPIO, IDENTIFY_ACCESSORY);
            break;
    }
}

homekit_value_t read_ip_addr() {
    struct ip_info info;
    if (sdk_wifi_get_ip_info(STATION_IF, &info)) {
        char *buffer = malloc(16);
        snprintf(buffer, 16, IPSTR, IP2STR(&info.ip));
        
        return HOMEKIT_STRING(buffer);
    }

    return HOMEKIT_STRING("");
}

void hardware_init() {
    printf("RC > Initializing hardware...\n");
    
    void enable_sonoff_device() {
        if (board_type.value.int_value == 2) {
            extra_gpio = 2;
        } else if (board_type.value.int_value == 4) {
            extra_gpio = 3;
            log_output.value.int_value = 0;
        }
        
        gpio_enable(LED_GPIO, GPIO_OUTPUT);
        gpio_write(LED_GPIO, false);
        
        adv_button_create(button1_gpio, true);
    }
    
    void register_button_event(const uint8_t gpio) {
        adv_button_register_callback_fn(gpio, button_event1_intr_callback, 1);
        adv_button_register_callback_fn(gpio, button_event2_intr_callback, 2);
        adv_button_register_callback_fn(gpio, button_event3_intr_callback, 3);
        adv_button_register_callback_fn(gpio, button_simple1_intr_callback, 4);
        adv_button_register_callback_fn(gpio, factory_default_call, 5);
    }
    
    bool pullup = true;
    switch (device_type_static) {
        case 1:
            if (board_type.value.int_value == 3) {  // It is a Shelly1
                relay1_gpio = S1_RELAY_GPIO;
                extra_gpio = S1_TOGGLE_GPIO;
                pullup = false;
            } else {                                // It is a Sonoff
                enable_sonoff_device();
                
                adv_button_register_callback_fn(button1_gpio, button_simple1_intr_callback, 1);
                adv_button_register_callback_fn(button1_gpio, factory_default_call, 5);
            }
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            relay_write(switch1_on.value.bool_value, relay1_gpio);
            
            switch (external_toggle1.value.int_value) {
                case 1:
                    adv_toggle_create(extra_gpio, pullup);
                    adv_toggle_register_callback_fn(extra_gpio, button_simple1_intr_callback, 0);
                    break;
                    
                case 2:
                    adv_toggle_create(extra_gpio, pullup);
                    adv_toggle_register_callback_fn(extra_gpio, button_simple1_intr_callback, 2);
                    break;
                    
                default:
                    break;
            }

            break;
            
        case 2:
            if (board_type.value.int_value == 3) {  // It is a Shelly2
                relay1_gpio = S2_RELAY1_GPIO;
                button1_gpio = S2_TOGGLE1_GPIO;
                button2_gpio = S2_TOGGLE2_GPIO;
                pullup = false;
            } else {                                // It is a Sonoff
                enable_sonoff_device();
                
                adv_button_destroy(button1_gpio);
                
                adv_button_create(BUTTON3_GPIO, true);
                
                adv_button_register_callback_fn(BUTTON3_GPIO, button_simple1_intr_callback, 1);
                adv_button_register_callback_fn(BUTTON3_GPIO, button_simple2_intr_callback, 2);
                adv_button_register_callback_fn(BUTTON3_GPIO, factory_default_call, 5);
                
                if (board_type.value.int_value == 4) {
                    button1_gpio = 3;
                }
            }
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            relay_write(switch1_on.value.bool_value, relay1_gpio);
            
            gpio_enable(RELAY2_GPIO, GPIO_OUTPUT);
            relay_write(switch2_on.value.bool_value, RELAY2_GPIO);
            
            switch (external_toggle1.value.int_value) {
                case 1:
                    adv_toggle_create(button1_gpio, pullup);
                    adv_toggle_register_callback_fn(button1_gpio, button_simple1_intr_callback, 0);
                    break;
                    
                case 2:
                    adv_toggle_create(button1_gpio, pullup);
                    adv_toggle_register_callback_fn(button1_gpio, button_simple1_intr_callback, 2);
                    break;
                    
                default:
                    break;
            }
            
            switch (external_toggle2.value.int_value) {
                case 1:
                    adv_toggle_create(button2_gpio, false);
                    adv_toggle_register_callback_fn(button2_gpio, button_simple2_intr_callback, 0);
                    break;
                    
                case 2:
                    adv_toggle_create(button2_gpio, false);
                    adv_toggle_register_callback_fn(button2_gpio, button_simple2_intr_callback, 2);
                    break;
                    
                default:
                    break;
            }
            
            break;
            
        case 3:
            enable_sonoff_device();
            
            register_button_event(button1_gpio);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            relay_write(switch1_on.value.bool_value, relay1_gpio);
            break;
            
        case 4:
            enable_sonoff_device();
            
            adv_button_create(button2_gpio, true);
            adv_button_create(BUTTON3_GPIO, true);
            adv_button_create(extra_gpio, true);
            
            adv_button_register_callback_fn(button1_gpio, button_simple1_intr_callback, 1);
            adv_button_register_callback_fn(button1_gpio, factory_default_call, 5);
            
            adv_button_register_callback_fn(button2_gpio, button_simple2_intr_callback, 1);
            adv_button_register_callback_fn(BUTTON3_GPIO, button_simple3_intr_callback, 1);
            adv_button_register_callback_fn(extra_gpio, button_simple4_intr_callback, 1);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            relay_write(switch1_on.value.bool_value, relay1_gpio);
            
            gpio_enable(RELAY2_GPIO, GPIO_OUTPUT);
            relay_write(switch2_on.value.bool_value, RELAY2_GPIO);
            
            gpio_enable(RELAY3_GPIO, GPIO_OUTPUT);
            relay_write(switch3_on.value.bool_value, RELAY3_GPIO);
            
            gpio_enable(RELAY4_GPIO, GPIO_OUTPUT);
            relay_write(switch4_on.value.bool_value, RELAY4_GPIO);
            break;
            
        case 5:
            enable_sonoff_device();
            
            adv_button_register_callback_fn(button1_gpio, th_button_intr_callback, 1);
            adv_button_register_callback_fn(button1_gpio, factory_default_call, 5);
            
            gpio_set_pullup(extra_gpio, false, false);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            relay_write(false, relay1_gpio);
            
            sdk_os_timer_setfn(&extra_func_timer, temperature_sensor_worker, NULL);
            sdk_os_timer_arm(&extra_func_timer, poll_period.value.int_value * 1000, 1);
            
            temperature_sensor_worker();
            break;
            
        case 6:
            enable_sonoff_device();
            
            adv_button_register_callback_fn(button1_gpio, button_simple1_intr_callback, 1);
            adv_button_register_callback_fn(button1_gpio, factory_default_call, 5);
            
            gpio_set_pullup(extra_gpio, false, false);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            relay_write(switch1_on.value.bool_value, relay1_gpio);
            
            sdk_os_timer_setfn(&extra_func_timer, temperature_sensor_worker, NULL);
            sdk_os_timer_arm(&extra_func_timer, poll_period.value.int_value * 1000, 1);
            
            temperature_sensor_worker();
            break;
            
        case 7:
            enable_sonoff_device();
            
            adv_button_register_callback_fn(button1_gpio, toggle_valve, 1);
            adv_button_register_callback_fn(button1_gpio, factory_default_call, 5);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            relay_write(false, relay1_gpio);
            
            sdk_os_timer_setfn(&extra_func_timer, valve_control, NULL);
            break;
            
        case 8:
            enable_sonoff_device();
            
            adv_button_register_callback_fn(button1_gpio, garage_on_button, 1);
            adv_button_register_callback_fn(button1_gpio, factory_default_call, 5);
            
            adv_toggle_create(extra_gpio, true);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            relay_write(false, relay1_gpio);
            
            if (custom_garagedoor_sensor_close_nc.value.bool_value) {
                adv_toggle_register_callback_fn(extra_gpio, door_closed_1_fn_callback, 0);
                adv_toggle_register_callback_fn(extra_gpio, door_closed_0_fn_callback, 1);
            } else {
                adv_toggle_register_callback_fn(extra_gpio, door_closed_0_fn_callback, 0);
                adv_toggle_register_callback_fn(extra_gpio, door_closed_1_fn_callback, 1);
            }
            
            if (custom_garagedoor_sensor_open.value.int_value > 0) {
                adv_toggle_create(DOOR_OPENED_GPIO, true);
                
                if (custom_garagedoor_sensor_open.value.int_value == 2) {
                    adv_toggle_register_callback_fn(DOOR_OPENED_GPIO, door_opened_1_fn_callback, 0);
                    adv_toggle_register_callback_fn(DOOR_OPENED_GPIO, door_opened_0_fn_callback, 1);
                } else {
                    adv_toggle_register_callback_fn(DOOR_OPENED_GPIO, door_opened_0_fn_callback, 0);
                    adv_toggle_register_callback_fn(DOOR_OPENED_GPIO, door_opened_1_fn_callback, 1);
                }
                
            } else {
                sdk_os_timer_setfn(&extra_func_timer, door_opened_countdown_timer, NULL);
            }
            
            if (custom_garagedoor_sensor_obstruction.value.int_value > 0) {
                adv_toggle_create(DOOR_OBSTRUCTION_GPIO, true);
                
                if (custom_garagedoor_sensor_obstruction.value.int_value == 2) {
                    adv_toggle_register_callback_fn(DOOR_OBSTRUCTION_GPIO, door_obstructed_1_fn_callback, 0);
                    adv_toggle_register_callback_fn(DOOR_OBSTRUCTION_GPIO, door_obstructed_0_fn_callback, 1);
                } else {
                    adv_toggle_register_callback_fn(DOOR_OBSTRUCTION_GPIO, door_obstructed_0_fn_callback, 0);
                    adv_toggle_register_callback_fn(DOOR_OBSTRUCTION_GPIO, door_obstructed_1_fn_callback, 1);
                }
                
                if (gpio_read(DOOR_OBSTRUCTION_GPIO) == custom_garagedoor_sensor_obstruction.value.int_value -1) {
                    door_obstructed_1_fn_callback(DOOR_OBSTRUCTION_GPIO);
                }
            }
            
            break;
            
        case 9:
            enable_sonoff_device();
            
            register_button_event(button1_gpio);
            
            gpio_set_pullup(extra_gpio, false, false);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            relay_write(switch1_on.value.bool_value, relay1_gpio);
            
            sdk_os_timer_setfn(&extra_func_timer, temperature_sensor_worker, NULL);
            sdk_os_timer_arm(&extra_func_timer, poll_period.value.int_value * 1000, 1);
            
            temperature_sensor_worker();
            break;
            
        case 10:
            enable_sonoff_device();
            
            if (board_type.value.int_value == 2) {
                adv_button_destroy(button1_gpio);
                button1_gpio = 2;
                relay1_gpio = 0;
                adv_button_create(button1_gpio, true);
            } else {
                relay1_gpio = 2;
            }
            
            register_button_event(button1_gpio);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            relay_write(switch1_on.value.bool_value, relay1_gpio);
            break;
            
        case 11:
            enable_sonoff_device();
            
            adv_button_create(button2_gpio, true);
            adv_button_create(BUTTON3_GPIO, true);
            
            adv_button_register_callback_fn(button1_gpio, button_simple1_intr_callback, 1);
            adv_button_register_callback_fn(button1_gpio, factory_default_call, 5);
            
            adv_button_register_callback_fn(button2_gpio, button_simple2_intr_callback, 1);
            adv_button_register_callback_fn(BUTTON3_GPIO, button_simple3_intr_callback, 1);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            relay_write(switch1_on.value.bool_value, relay1_gpio);
            
            gpio_enable(RELAY2_GPIO, GPIO_OUTPUT);
            relay_write(switch2_on.value.bool_value, RELAY2_GPIO);
            
            gpio_enable(RELAY3_GPIO, GPIO_OUTPUT);
            relay_write(switch3_on.value.bool_value, RELAY3_GPIO);
            break;
            
        default:
            // A wish from compiler
            break;
    }
    
    sdk_os_timer_setfn(&factory_default_toggle_timer, factory_default_toggle, NULL);
    sdk_os_timer_setfn(&change_settings_timer, save_settings, NULL);
    sdk_os_timer_setfn(&save_states_timer, save_states, NULL);
    
    printf("RC > HW ready\n");
    
    wifi_config_init("RavenCore", NULL, on_wifi_ready);
}

void settings_init() {
    // Initial Setup
    sysparam_status_t status, flash_error = SYSPARAM_OK;
    bool bool_value;
    int8_t int8_value;
    int32_t int32_value;
    
    // Load Saved Settings and set factory values for missing settings
    printf("RC > Loading settings\n");
    
    status = sysparam_get_bool(SHOW_SETUP_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        show_setup.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(SHOW_SETUP_SYSPARAM, true);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(OTA_BETA_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        ota_beta.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(OTA_BETA_SYSPARAM, false);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(BOARD_TYPE_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        board_type.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(BOARD_TYPE_SYSPARAM, 1);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(DEVICE_TYPE_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        // For old Shelly device types (device type 12 and 13)
        if (int8_value == 12) {
            int8_value = 1;
            board_type.value.int_value = 3;
        }
        
        if (int8_value == 13) {
            int8_value = 2;
            board_type.value.int_value = 3;
        }
        // END - For old Shelly device types (device type 12 and 13)
        
        device_type.value.int_value = int8_value;
        device_type_static = int8_value;
    } else {
        status = sysparam_set_int8(DEVICE_TYPE_SYSPARAM, 1);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(EXTERNAL_TOGGLE1_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        external_toggle1.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(EXTERNAL_TOGGLE1_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(EXTERNAL_TOGGLE2_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        external_toggle2.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(EXTERNAL_TOGGLE2_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(DHT_SENSOR_TYPE_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        dht_sensor_type.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(DHT_SENSOR_TYPE_SYSPARAM, 2);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(HUM_OFFSET_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        hum_offset.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(HUM_OFFSET_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int32(TEMP_OFFSET_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        temp_offset.value.float_value = int32_value / 100.00f;
    } else {
        status = sysparam_set_int32(TEMP_OFFSET_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(POLL_PERIOD_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        poll_period.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(POLL_PERIOD_SYSPARAM, 30);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(VALVE_TYPE_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        valve_type.value.int_value = int8_value;
        custom_valve_type.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(VALVE_TYPE_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(GARAGEDOOR_WORKING_TIME_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_working_time.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(GARAGEDOOR_WORKING_TIME_SYSPARAM, 20);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(GARAGEDOOR_HAS_STOP_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_has_stop.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(GARAGEDOOR_HAS_STOP_SYSPARAM, false);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(GARAGEDOOR_SENSOR_CLOSE_NC_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_sensor_close_nc.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(GARAGEDOOR_SENSOR_CLOSE_NC_SYSPARAM, false);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(GARAGEDOOR_SENSOR_OPEN_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_sensor_open.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(GARAGEDOOR_SENSOR_OPEN_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(GARAGEDOOR_SENSOR_OBSTRUCTION_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_sensor_obstruction.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(GARAGEDOOR_SENSOR_OBSTRUCTION_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(GARAGEDOOR_CONTROL_WITH_BUTTON_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_control_with_button.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(GARAGEDOOR_CONTROL_WITH_BUTTON_SYSPARAM, false);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int32(GARAGEDOOR_BUTTON_TIME_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_button_time.value.float_value = int32_value / 100.00f;
    } else {
        status = sysparam_set_int32(GARAGEDOOR_BUTTON_TIME_SYSPARAM, 0.30 * 100);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(INIT_STATE_SW1_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw1.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(INIT_STATE_SW1_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(INIT_STATE_SW2_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw2.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(INIT_STATE_SW2_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(INIT_STATE_SW3_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw3.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(INIT_STATE_SW3_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(INIT_STATE_SW4_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw4.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(INIT_STATE_SW4_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(INIT_STATE_TH_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_th.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(INIT_STATE_TH_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(REVERSE_SW1_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        custom_reverse_sw1.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(REVERSE_SW1_SYSPARAM, false);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(REVERSE_SW2_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        custom_reverse_sw2.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(REVERSE_SW2_SYSPARAM, false);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(REVERSE_SW3_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        custom_reverse_sw3.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(REVERSE_SW3_SYSPARAM, false);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(REVERSE_SW4_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        custom_reverse_sw4.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(REVERSE_SW4_SYSPARAM, false);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    // Load Saved States
    printf("RC > Loading saved states\n");
    
    if (custom_init_state_sw1.value.int_value > 1) {
        status = sysparam_get_bool(LAST_STATE_SW1_SYSPARAM, &bool_value);
        if (status == SYSPARAM_OK) {
            if (custom_init_state_sw1.value.int_value == 2) {
                switch1_on.value.bool_value = bool_value;
            } else {
                switch1_on.value.bool_value = !bool_value;
            }
        } else {
            status = sysparam_set_bool(LAST_STATE_SW1_SYSPARAM, false);
            if (status != SYSPARAM_OK) {
                flash_error = status;
            }
        }
    } else if (custom_init_state_sw1.value.int_value == 1) {
        switch1_on.value.bool_value = true;
    }
    
    if (custom_init_state_sw2.value.int_value > 1) {
        status = sysparam_get_bool(LAST_STATE_SW2_SYSPARAM, &bool_value);
        if (status == SYSPARAM_OK) {
            if (custom_init_state_sw2.value.int_value == 2) {
                switch2_on.value.bool_value = bool_value;
            } else {
                switch2_on.value.bool_value = !bool_value;
            }
        } else {
            status = sysparam_set_bool(LAST_STATE_SW2_SYSPARAM, false);
            if (status != SYSPARAM_OK) {
                flash_error = status;
            }
        }
    } else if (custom_init_state_sw2.value.int_value == 1) {
        switch2_on.value.bool_value = true;
    }
    
    if (custom_init_state_sw3.value.int_value > 1) {
        status = sysparam_get_bool(LAST_STATE_SW3_SYSPARAM, &bool_value);
        if (status == SYSPARAM_OK) {
            if (custom_init_state_sw3.value.int_value == 2) {
                switch3_on.value.bool_value = bool_value;
            } else {
                switch3_on.value.bool_value = !bool_value;
            }
        } else {
            status = sysparam_set_bool(LAST_STATE_SW3_SYSPARAM, false);
            if (status != SYSPARAM_OK) {
                flash_error = status;
            }
        }
    } else if (custom_init_state_sw3.value.int_value == 1) {
        switch3_on.value.bool_value = true;
    }
    
    if (custom_init_state_sw4.value.int_value > 1) {
        status = sysparam_get_bool(LAST_STATE_SW4_SYSPARAM, &bool_value);
        if (status == SYSPARAM_OK) {
            if (custom_init_state_sw4.value.int_value == 2) {
                switch4_on.value.bool_value = bool_value;
            } else {
                switch4_on.value.bool_value = !bool_value;
            }
        } else {
            status = sysparam_set_bool(LAST_STATE_SW4_SYSPARAM, false);
            if (status != SYSPARAM_OK) {
                flash_error = status;
            }
        }
    } else if (custom_init_state_sw4.value.int_value == 1) {
        switch4_on.value.bool_value = true;
    }
    
    if (custom_init_state_th.value.int_value < 3) {
        target_state.value.int_value = custom_init_state_th.value.int_value;
    } else {
        status = sysparam_get_int8(LAST_TARGET_STATE_TH_SYSPARAM, &int8_value);
        if (status == SYSPARAM_OK) {
            target_state.value.int_value = int8_value;
        } else {
            status = sysparam_set_int8(LAST_TARGET_STATE_TH_SYSPARAM, 0);
            if (status != SYSPARAM_OK) {
                flash_error = status;
            }
        }
    }
    
    status = sysparam_get_int32(TARGET_TEMPERATURE_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        target_temperature.value.float_value = int32_value / 100.00f;
    } else {
        status = sysparam_set_int32(TARGET_TEMPERATURE_SYSPARAM, 23 * 100);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int32(VALVE_SET_DURATION_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        set_duration.value.int_value = int32_value;
    } else {
        status = sysparam_set_int32(VALVE_SET_DURATION_SYSPARAM, 900);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    if (flash_error == SYSPARAM_OK) {
        hardware_init();
    } else {
        printf("RC ! ERROR loading settings -> %i\n", flash_error);
        printf("RC ! SYSTEM HALTED\n\n");
    }
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, NULL);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER, "RavenSystem");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, NULL);
homekit_characteristic_t model = HOMEKIT_CHARACTERISTIC_(MODEL, "RavenCore");
homekit_characteristic_t identify_function = HOMEKIT_CHARACTERISTIC_(IDENTIFY, identify);

homekit_characteristic_t switch1_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Switch 1");
homekit_characteristic_t switch2_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Switch 2");
homekit_characteristic_t switch3_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Switch 3");
homekit_characteristic_t switch4_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Switch 4");

homekit_characteristic_t outlet_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Outlet");
homekit_characteristic_t button_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Button");
homekit_characteristic_t th_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Thermostat");
homekit_characteristic_t temp_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Temperature");
homekit_characteristic_t hum_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Humidity");
homekit_characteristic_t valve_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Water Valve");
homekit_characteristic_t garage_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Garage Door");

homekit_characteristic_t setup_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Setup", .id=100);
homekit_characteristic_t device_type_name = HOMEKIT_CHARACTERISTIC_(CUSTOM_DEVICE_TYPE_NAME, "", .id=101);

homekit_characteristic_t firmware = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, "0.6.1");

homekit_accessory_category_t accessory_category;

void create_accessory_name() {
    printf("RC > Creating accessory name\n");
    
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    char *name_value = malloc(17);
    snprintf(name_value, 17, "RavenCore-%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    name.value = HOMEKIT_STRING(name_value);
    
    char *serial_value = malloc(13);
    snprintf(serial_value, 13, "%02X%02X%02X%02X%02X%02X", macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);
    serial.value = HOMEKIT_STRING(serial_value);
}

homekit_server_config_t config;

void create_accessory() {
    printf("RC > Creating HomeKit accessory\n");
    
    uint8_t service_count = 3, service_number = 2;
    
    // Total service count and Accessory Category selection
    if (show_setup.value.bool_value) {
        service_count++;
    }
    
    switch (device_type_static) {
        case 2:
            service_count += 1;
            accessory_category = homekit_accessory_category_switch;
            break;
            
        case 3:
            service_count += 1;
            accessory_category = homekit_accessory_category_outlet;
            break;
            
        case 4:
            service_count += 3;
            accessory_category = homekit_accessory_category_switch;
            break;
            
        case 5:
            service_count += 0;
            accessory_category = homekit_accessory_category_thermostat;
            break;
            
        case 6:
            service_count += 1;
            if (dht_sensor_type.value.int_value != 3) {
                service_count += 1;
            }
            accessory_category = homekit_accessory_category_switch;
            break;
            
        case 7:
            service_count += 0;
            accessory_category = homekit_accessory_category_sprinkler;
            break;
            
        case 8:
            service_count += 0;
            accessory_category = homekit_accessory_category_garage;
            break;
            
        case 9:
            service_count += 2;
            if (dht_sensor_type.value.int_value != 3) {
                service_count += 1;
            }
            accessory_category = homekit_accessory_category_outlet;
            break;
            
        case 10:
            service_count += 1;
            accessory_category = homekit_accessory_category_switch;
            break;
            
        case 11:
            service_count += 2;
            accessory_category = homekit_accessory_category_switch;
            break;
            
        default:    // case 1
            service_count += 0;
            accessory_category = homekit_accessory_category_switch;
            break;
    }
    
    
    homekit_accessory_t **accessories = calloc(2, sizeof(homekit_accessory_t*));

    homekit_accessory_t *sonoff = accessories[0] = calloc(1, sizeof(homekit_accessory_t));
        sonoff->id = 1;
        sonoff->category = accessory_category;
        sonoff->config_number = 000601;   // Matches as example: firmware_revision 2.3.8 = 02.03.10 (octal) = config_number 020310
        sonoff->services = calloc(service_count, sizeof(homekit_service_t*));

            homekit_service_t *sonoff_info = sonoff->services[0] = calloc(1, sizeof(homekit_service_t));
            sonoff_info->id = 1;
            sonoff_info->type = HOMEKIT_SERVICE_ACCESSORY_INFORMATION;
            sonoff_info->characteristics = calloc(7, sizeof(homekit_characteristic_t*));
                sonoff_info->characteristics[0] = &name;
                sonoff_info->characteristics[1] = &manufacturer;
                sonoff_info->characteristics[2] = &serial;
                sonoff_info->characteristics[3] = &model;
                sonoff_info->characteristics[4] = &firmware;
                sonoff_info->characteristics[5] = &identify_function;
    
            // Define services and characteristics
            void charac_switch_1(const uint8_t service) {
                homekit_service_t *sonoff_switch1 = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch1->id = 8;
                sonoff_switch1->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch1->primary = true;
                sonoff_switch1->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
                    sonoff_switch1->characteristics[0] = &switch1_service_name;
                    sonoff_switch1->characteristics[1] = &switch1_on;
                    sonoff_switch1->characteristics[2] = &show_setup;
            }
    
            void charac_switch_2(const uint8_t service) {
                homekit_service_t *sonoff_switch2 = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch2->id = 12;
                sonoff_switch2->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch2->primary = false;
                sonoff_switch2->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_switch2->characteristics[0] = &switch2_service_name;
                    sonoff_switch2->characteristics[1] = &switch2_on;
            }
    
            void charac_switch_3(const uint8_t service) {
                homekit_service_t *sonoff_switch3 = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch3->id = 15;
                sonoff_switch3->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch3->primary = false;
                sonoff_switch3->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_switch3->characteristics[0] = &switch3_service_name;
                    sonoff_switch3->characteristics[1] = &switch3_on;
            }
    
            void charac_switch_4(const uint8_t service) {
                homekit_service_t *sonoff_switch4 = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch4->id = 18;
                sonoff_switch4->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch4->primary = false;
                sonoff_switch4->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_switch4->characteristics[0] = &switch4_service_name;
                    sonoff_switch4->characteristics[1] = &switch4_on;
            }
    
            void charac_socket(const uint8_t service) {
                homekit_service_t *sonoff_outlet = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_outlet->id = 21;
                sonoff_outlet->type = HOMEKIT_SERVICE_OUTLET;
                sonoff_outlet->primary = true;
                sonoff_outlet->characteristics = calloc(5, sizeof(homekit_characteristic_t*));
                    sonoff_outlet->characteristics[0] = &outlet_service_name;
                    sonoff_outlet->characteristics[1] = &switch1_on;
                    sonoff_outlet->characteristics[2] = &switch_outlet_in_use;
                    sonoff_outlet->characteristics[3] = &show_setup;
            }
    
            void charac_prog_button(const uint8_t service) {
                homekit_service_t *sonoff_button = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_button->id = 26;
                sonoff_button->type = HOMEKIT_SERVICE_STATELESS_PROGRAMMABLE_SWITCH;
                sonoff_button->primary = false;
                sonoff_button->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_button->characteristics[0] = &button_service_name;
                    sonoff_button->characteristics[1] = &button_event;
            }
    
            void charac_thermostat(const uint8_t service) {
                homekit_service_t *sonoff_th = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_th->id = 29;
                sonoff_th->type = HOMEKIT_SERVICE_THERMOSTAT;
                sonoff_th->primary = true;
                sonoff_th->characteristics = calloc(9, sizeof(homekit_characteristic_t*));
                    sonoff_th->characteristics[0] = &th_service_name;
                    sonoff_th->characteristics[1] = &current_temperature;
                    sonoff_th->characteristics[2] = &target_temperature;
                    sonoff_th->characteristics[3] = &current_state;
                    sonoff_th->characteristics[4] = &target_state;
                    sonoff_th->characteristics[5] = &units;
                    sonoff_th->characteristics[6] = &current_humidity;
                    sonoff_th->characteristics[7] = &show_setup;
            }
    
            void charac_temperature(const uint8_t service) {
                homekit_service_t *sonoff_temp = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_temp->id = 38;
                sonoff_temp->type = HOMEKIT_SERVICE_TEMPERATURE_SENSOR;
                sonoff_temp->primary = false;
                sonoff_temp->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_temp->characteristics[0] = &temp_service_name;
                    sonoff_temp->characteristics[1] = &current_temperature;
            }
    
            void charac_humidity(const uint8_t service) {
                homekit_service_t *sonoff_hum = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_hum->id = 41;
                sonoff_hum->type = HOMEKIT_SERVICE_HUMIDITY_SENSOR;
                sonoff_hum->primary = false;
                sonoff_hum->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_hum->characteristics[0] = &hum_service_name;
                    sonoff_hum->characteristics[1] = &current_humidity;
            }
    
            void charac_valve(const uint8_t service) {
                homekit_service_t *sonoff_valve = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_valve->id = 44;
                sonoff_valve->type = HOMEKIT_SERVICE_VALVE;
                sonoff_valve->primary = true;
                sonoff_valve->characteristics = calloc(8, sizeof(homekit_characteristic_t*));
                    sonoff_valve->characteristics[0] = &valve_service_name;
                    sonoff_valve->characteristics[1] = &active;
                    sonoff_valve->characteristics[2] = &in_use;
                    sonoff_valve->characteristics[3] = &valve_type;
                    sonoff_valve->characteristics[4] = &set_duration;
                    sonoff_valve->characteristics[5] = &remaining_duration;
                    sonoff_valve->characteristics[6] = &show_setup;
            }
    
            void charac_garagedoor(const uint8_t service) {
                homekit_service_t *sonoff_garage = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_garage->id = 52;
                sonoff_garage->type = HOMEKIT_SERVICE_GARAGE_DOOR_OPENER;
                sonoff_garage->primary = true;
                sonoff_garage->characteristics = calloc(6, sizeof(homekit_characteristic_t*));
                    sonoff_garage->characteristics[0] = &garage_service_name;
                    sonoff_garage->characteristics[1] = &current_door_state;
                    sonoff_garage->characteristics[2] = &target_door_state;
                    sonoff_garage->characteristics[3] = &obstruction_detected;
                    sonoff_garage->characteristics[4] = &show_setup;
            }
            // --------
    
            // Create accessory for selected device type
            if (device_type_static == 2) {
                char *device_type_name_value = malloc(11);
                snprintf(device_type_name_value, 11, "Switch 2ch");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_switch_1(1);
                charac_switch_2(2);
                
                service_number = 3;
                
            } else if (device_type_static == 3) {
                char *device_type_name_value = malloc(14);
                snprintf(device_type_name_value, 14, "Socket Button");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_socket(1);
                charac_prog_button(2);
                
                service_number = 3;
                
            } else if (device_type_static == 4) {
                char *device_type_name_value = malloc(11);
                snprintf(device_type_name_value, 11, "Switch 4ch");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_switch_1(1);
                charac_switch_2(2);
                charac_switch_3(3);
                charac_switch_4(4);
                
                service_number = 5;
                
            } else if (device_type_static == 5) {
                char *device_type_name_value = malloc(11);
                snprintf(device_type_name_value, 11, "Thermostat");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_thermostat(1);
                
            } else if (device_type_static == 6) {
                char *device_type_name_value = malloc(17);
                snprintf(device_type_name_value, 17, "Switch TH Sensor");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_switch_1(1);
                charac_temperature(2);
                
                service_number = 3;
                
                if (dht_sensor_type.value.int_value != 3) {
                    charac_humidity(3);
                    
                    service_number += 1;
                }
                
            } else if (device_type_static == 7) {
                char *device_type_name_value = malloc(12);
                snprintf(device_type_name_value, 12, "Water Valve");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_valve(1);
                
            } else if (device_type_static == 8) {
                char *device_type_name_value = malloc(12);
                snprintf(device_type_name_value, 12, "Garage Door");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_garagedoor(1);
                
            } else if (device_type_static == 9) {
                char *device_type_name_value = malloc(17);
                snprintf(device_type_name_value, 17, "Socket Button TH");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_socket(1);
                charac_prog_button(2);
                charac_temperature(3);
                
                service_number = 4;
                
                if (dht_sensor_type.value.int_value != 3) {
                    charac_humidity(4);
                    
                    service_number += 1;
                }
                
            } else if (device_type_static == 10) {
                char *device_type_name_value = malloc(13);
                snprintf(device_type_name_value, 13, "ESP01 Sw Btn");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_switch_1(1);
                charac_prog_button(2);
                
                service_number = 3;
                
            } else if (device_type_static == 11) {
                char *device_type_name_value = malloc(11);
                snprintf(device_type_name_value, 11, "Switch 3ch");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_switch_1(1);
                charac_switch_2(2);
                charac_switch_3(3);
                
                service_number = 4;
                
            } else { // device_type_static == 1
                char *device_type_name_value = malloc(11);
                snprintf(device_type_name_value, 11, "Switch 1ch");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_switch_1(1);
            }

            // Setup Accessory, visible only in 3party Apps
            if (show_setup.value.bool_value) {
                printf("RC > Creating Setup accessory\n");
                
                homekit_service_t *sonoff_setup = sonoff->services[service_number] = calloc(1, sizeof(homekit_service_t));
                sonoff_setup->id = 99;
                sonoff_setup->type = HOMEKIT_SERVICE_CUSTOM_SETUP;
                sonoff_setup->primary = false;
        
                uint8_t setting_count = 10, setting_number = 9;

                switch (device_type_static) {
                    case 2:
                        setting_count += 5;
                        break;
                        
                    case 3:
                        setting_count += 1;
                        break;
                        
                    case 4:
                        setting_count += 7;
                        break;
                        
                    case 5:
                        setting_count += 5;
                        break;
                        
                    case 6:
                        setting_count += 5;
                        break;
                        
                    case 7:
                        setting_count += 1;
                        break;
                        
                    case 8:
                        setting_count += 7;
                        break;
                        
                    case 9:
                        setting_count += 5;
                        break;
                        
                    case 10:
                        setting_count += 1;
                        break;
                        
                    case 11:
                        setting_count += 5;
                        break;
                        
                    default:    // device_type_static == 1
                        setting_count += 2;
                        break;
                }
                
                sysparam_status_t status;
                char *char_value;
                
                status = sysparam_get_string(OTA_REPO_SYSPARAM, &char_value);
                if (status == SYSPARAM_OK) {
                    setting_count += 2;
                }
        
                sonoff_setup->characteristics = calloc(setting_count, sizeof(homekit_characteristic_t*));
                    sonoff_setup->characteristics[0] = &setup_service_name;
                    sonoff_setup->characteristics[1] = &device_type_name;
                    sonoff_setup->characteristics[2] = &device_type;
                    sonoff_setup->characteristics[3] = &reboot_device;
                    sonoff_setup->characteristics[4] = &board_type;
                    sonoff_setup->characteristics[5] = &log_output;
                    sonoff_setup->characteristics[6] = &ip_addr;
                    sonoff_setup->characteristics[7] = &wifi_reset;
                    sonoff_setup->characteristics[8] = &custom_reverse_sw1;
                
                    if (status == SYSPARAM_OK) {
                        sonoff_setup->characteristics[setting_number] = &ota_firmware;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &ota_beta;
                        setting_number++;
                    }
                
                    if (device_type_static == 1) {
                        sonoff_setup->characteristics[setting_number] = &external_toggle1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        
                    } else if (device_type_static == 2) {
                        sonoff_setup->characteristics[setting_number] = &external_toggle1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &external_toggle2;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw2;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_reverse_sw2;
                        
                    } else if (device_type_static == 3) {
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        
                    } else if (device_type_static == 4) {
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw2;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw3;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw4;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_reverse_sw2;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_reverse_sw3;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_reverse_sw4;
                        
                    } else if (device_type_static == 5) {
                        sonoff_setup->characteristics[setting_number] = &dht_sensor_type;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &hum_offset;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &temp_offset;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_th;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &poll_period;
                        
                    } else if (device_type_static == 6) {
                        sonoff_setup->characteristics[setting_number] = &dht_sensor_type;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &hum_offset;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &temp_offset;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &poll_period;
                        
                    } else if (device_type_static == 7) {
                        sonoff_setup->characteristics[setting_number] = &custom_valve_type;
                    
                    } else if (device_type_static == 8) {
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_has_stop;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_sensor_close_nc;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_sensor_open;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_sensor_obstruction;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_working_time;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_control_with_button;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_button_time;
                        
                    } else if (device_type_static == 9) {
                        sonoff_setup->characteristics[setting_number] = &dht_sensor_type;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &hum_offset;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &temp_offset;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &poll_period;
                        
                    } else if (device_type_static == 10) {
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        
                    } else if (device_type_static == 11) {
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw2;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw3;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_reverse_sw2;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_reverse_sw3;
                        
                    }
            }
    
    config.accessories = accessories;
    config.password = "021-82-017";
    
    printf("RC > Starting HomeKit Server\n");
    homekit_server_init(&config);
}

void on_wifi_ready() {
    led_code(LED_GPIO, WIFI_CONNECTED);
    
    create_accessory_name();
    create_accessory();
}

void old_settings_init() {
    sysparam_status_t status;
    bool bool_value;
    int8_t int8_value;
    int32_t int32_value;
    
    // Load old Saved Settings and set factory values for missing settings
    printf("RC > Loading old settings and cleaning\n");
    
    status = sysparam_get_bool("show_setup", &bool_value);
    if (status == SYSPARAM_OK) {
        show_setup.value.bool_value = bool_value;
        status = sysparam_set_data("show_setup", NULL, 0, false);
    }
    
    status = sysparam_get_int8("board_type", &int8_value);
    if (status == SYSPARAM_OK) {
        board_type.value.int_value = int8_value;
        status = sysparam_set_data("board_type", NULL, 0, false);
    }
    
    status = sysparam_get_bool("gpio14_toggle", &bool_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("gpio14_toggle", NULL, 0, false);
    }
    
    status = sysparam_get_int8("dht_sensor_type", &int8_value);
    if (status == SYSPARAM_OK) {
        dht_sensor_type.value.int_value = int8_value;
        status = sysparam_set_data("dht_sensor_type", NULL, 0, false);
    }
    
    status = sysparam_get_int8("poll_period", &int8_value);
    if (status == SYSPARAM_OK) {
        poll_period.value.int_value = int8_value;
        status = sysparam_set_data("poll_period", NULL, 0, false);
    }
    
    status = sysparam_get_int8("valve_type", &int8_value);
    if (status == SYSPARAM_OK) {
        valve_type.value.int_value = int8_value;
        custom_valve_type.value.int_value = int8_value;
        status = sysparam_set_data("valve_type", NULL, 0, false);
    }
    
    status = sysparam_get_int8("garagedoor_working_time", &int8_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_working_time.value.int_value = int8_value;
        status = sysparam_set_data("garagedoor_working_time", NULL, 0, false);
    }
    
    status = sysparam_get_bool("garagedoor_has_stop", &bool_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_has_stop.value.bool_value = bool_value;
        status = sysparam_set_data("garagedoor_has_stop", NULL, 0, false);
    }
    
    status = sysparam_get_bool("garagedoor_sensor_close_nc", &bool_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_sensor_close_nc.value.bool_value = bool_value;
        status = sysparam_set_data("garagedoor_sensor_close_nc", NULL, 0, false);
    }
    
    status = sysparam_get_bool("garagedoor_sensor_open_nc", &bool_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("garagedoor_sensor_open_nc", NULL, 0, false);
    }
    
    status = sysparam_get_bool("garagedoor_has_sensor_open", &bool_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("garagedoor_has_sensor_open", NULL, 0, false);
    }
    
    status = sysparam_get_bool("garagedoor_control_with_button", &bool_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_control_with_button.value.bool_value = bool_value;
        status = sysparam_set_data("garagedoor_control_with_button", NULL, 0, false);
    }
    
    status = sysparam_get_int8("init_state_sw1", &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw1.value.int_value = int8_value;
        status = sysparam_set_data("init_state_sw1", NULL, 0, false);
    }
    
    status = sysparam_get_int8("init_state_sw2", &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw2.value.int_value = int8_value;
        status = sysparam_set_data("init_state_sw2", NULL, 0, false);
    }
    
    status = sysparam_get_int8("init_state_sw3", &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw3.value.int_value = int8_value;
        status = sysparam_set_data("init_state_sw3", NULL, 0, false);
    }
    
    status = sysparam_get_int8("init_state_sw4", &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw4.value.int_value = int8_value;
        status = sysparam_set_data("init_state_sw4", NULL, 0, false);
    }
    
    status = sysparam_get_int8("init_state_th", &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_th.value.int_value = int8_value;
        status = sysparam_set_data("init_state_th", NULL, 0, false);
    }
    
    printf("RC > Removing old saved states\n");
    
    status = sysparam_get_bool("last_state_sw1", &bool_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("last_state_sw1", NULL, 0, false);
    }
    
    status = sysparam_get_bool("last_state_sw2", &bool_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("last_state_sw2", NULL, 0, false);
    }
    
    status = sysparam_get_bool("last_state_sw3", &bool_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("last_state_sw3", NULL, 0, false);
    }
    
    status = sysparam_get_bool("last_state_sw4", &bool_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("last_state_sw4", NULL, 0, false);
    }
    
    status = sysparam_get_int8("last_target_state_th", &int8_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("last_target_state_th", NULL, 0, false);
    }
    
    status = sysparam_get_int32("target_temp", &int32_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("target_temp", NULL, 0, false);
    }
    
    status = sysparam_get_int32("set_duration", &int32_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("set_duration", NULL, 0, false);
    }
    
    sdk_os_timer_disarm(&change_settings_timer);
    
    save_settings();
    status = sysparam_compact();
    
    if (status == SYSPARAM_OK) {
        device_restart();
    } else {
        printf("RC ! ERROR in old_settings_init loading old settings. Flash problem\n");
    }
}

void user_init(void) {
    sysparam_status_t status;
    int8_t int8_value;
    
    status = sysparam_get_int8(LOG_OUTPUT_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        log_output.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(LOG_OUTPUT_SYSPARAM, 1);
    }
    
    switch (log_output.value.int_value) {
        case 1:
            uart_set_baud(0, 115200);
            break;
            
        default:
            break;
    }
    
    printf("\n\nRC > RavenCore\nRC > Developed by RavenSystem - José Antonio Jiménez\nRC > Version: %s\n\n", firmware.value.string_value);
    
    // Old settings check
    status = sysparam_get_int8("device_type", &int8_value);
    if (status != SYSPARAM_OK) {
        settings_init();
    } else {
        device_type.value.int_value = int8_value;
        device_type_static = int8_value;
        status = sysparam_set_data("device_type", NULL, 0, false);
        old_settings_init();
    }
    // END Old settings check
}
