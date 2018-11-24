/*
 * RavenCore
 * 
 * v0.4.10
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
  1. Switch Basic
  2. Switch Dual
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

#include <stdio.h>
#include <string.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_common.h>
#include <rboot-api.h>
#include <sysparam.h>
#include <task.h>

#include <etstimer.h>
#include <esplibs/libmain.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <led_codes.h>
#include <adv_button.h>

#include <dht/dht.h>
#include <ds18b20/ds18b20.h>

#include "../common/custom_characteristics.h"

#define LED_GPIO            13
#define SENSOR_GPIO         14
#define DOOR_OPENED_GPIO    4
#define DOOR_CLOSED_GPIO    14
#define SONOFF_TOGGLE_GPIO  14

#define BUTTON1_GPIO        0
#define BUTTON2_GPIO        9
#define BUTTON3_GPIO        10
#define BUTTON4_GPIO        14

#define RELAY2_GPIO         5
#define RELAY3_GPIO         4
#define RELAY4_GPIO         15

#define POLL_PERIOD         30000

#define DISABLED_TIME       50

static bool gd_is_moving = false;
static uint8_t device_type_static = 1, gd_time_state = 0, relay1_gpio = 12;
static volatile uint32_t last_press_time;
static volatile float old_humidity_value = 0.0, old_temperature_value = 0.0;
static ETSTimer device_restart_timer, change_settings_timer, extra_func_timer;

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

void change_settings_callback();
void show_setup_callback();
void ota_firmware_callback();
void gpio14_toggle_callback();

void on_wifi_ready();

homekit_characteristic_t switch1_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter=read_switch1_on_callback, .setter=switch1_on_callback);
homekit_characteristic_t switch2_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter=read_switch2_on_callback, .setter=switch2_on_callback);
homekit_characteristic_t switch3_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter=read_switch3_on_callback, .setter=switch3_on_callback);
homekit_characteristic_t switch4_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter=read_switch4_on_callback, .setter=switch4_on_callback);

homekit_characteristic_t switch_outlet_in_use = HOMEKIT_CHARACTERISTIC_(OUTLET_IN_USE, true);   // It has not a real use, but it is a mandatory characteristic
homekit_characteristic_t button_event = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);

homekit_characteristic_t current_temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t target_temperature  = HOMEKIT_CHARACTERISTIC_(TARGET_TEMPERATURE, 23, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(update_state));
homekit_characteristic_t units = HOMEKIT_CHARACTERISTIC_(TEMPERATURE_DISPLAY_UNITS, 0);
homekit_characteristic_t current_state = HOMEKIT_CHARACTERISTIC_(CURRENT_HEATING_COOLING_STATE, 0);
homekit_characteristic_t target_state = HOMEKIT_CHARACTERISTIC_(TARGET_HEATING_COOLING_STATE, 0, .getter=read_on_target, .setter=on_target);
homekit_characteristic_t current_humidity = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);

homekit_characteristic_t active = HOMEKIT_CHARACTERISTIC_(ACTIVE, 0, .getter=read_valve_on_callback, .setter=valve_on_callback);
homekit_characteristic_t in_use = HOMEKIT_CHARACTERISTIC_(IN_USE, 0, .getter=read_in_use_on_callback);
homekit_characteristic_t valve_type = HOMEKIT_CHARACTERISTIC_(VALVE_TYPE, 0);
homekit_characteristic_t set_duration = HOMEKIT_CHARACTERISTIC_(SET_DURATION, 900, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t remaining_duration = HOMEKIT_CHARACTERISTIC_(REMAINING_DURATION, 0, .getter=read_remaining_duration_on_callback);

homekit_characteristic_t current_door_state = HOMEKIT_CHARACTERISTIC_(CURRENT_DOOR_STATE, 1);
homekit_characteristic_t target_door_state = HOMEKIT_CHARACTERISTIC_(TARGET_DOOR_STATE, 1, .getter=read_garage_on_callback, .setter=garage_on_callback);
homekit_characteristic_t obstruction_detected = HOMEKIT_CHARACTERISTIC_(OBSTRUCTION_DETECTED, false);

homekit_characteristic_t show_setup = HOMEKIT_CHARACTERISTIC_(CUSTOM_SHOW_SETUP, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(show_setup_callback));

homekit_characteristic_t device_type = HOMEKIT_CHARACTERISTIC_(CUSTOM_DEVICE_TYPE, 1, .id=102, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t reboot_device = HOMEKIT_CHARACTERISTIC_(CUSTOM_REBOOT_DEVICE, false, .id=103, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(ota_firmware_callback));
homekit_characteristic_t ota_firmware = HOMEKIT_CHARACTERISTIC_(CUSTOM_OTA_UPDATE, false, .id=110, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(ota_firmware_callback));
homekit_characteristic_t gpio14_toggle = HOMEKIT_CHARACTERISTIC_(CUSTOM_GPIO14_TOGGLE, false, .id=111, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(gpio14_toggle_callback));
homekit_characteristic_t dht_sensor_type = HOMEKIT_CHARACTERISTIC_(CUSTOM_DHT_SENSOR_TYPE, 2, .id=112, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_valve_type = HOMEKIT_CHARACTERISTIC_(CUSTOM_VALVE_TYPE, 0, .id=113, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_has_stop = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_HAS_STOP, false, .id=114, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_sensor_close_nc = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_SENSOR_CLOSE_NC, false, .id=115, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_sensor_open_nc = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_SENSOR_OPEN_NC, false, .id=116, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_has_sensor_open = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_HAS_SENSOR_OPEN, false, .id=117, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_working_time = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_WORKINGTIME, 20, .id=118, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_control_with_button = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_CONTROL_WITH_BUTTON, false, .id=119, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

void relay_write(bool on, int gpio) {
    last_press_time = xTaskGetTickCountFromISR() + (DISABLED_TIME / portTICK_PERIOD_MS);
    adv_button_set_disable_time();
    gpio_write(gpio, on ? 1 : 0);
}

void led_write(bool on) {
    gpio_write(LED_GPIO, on ? 0 : 1);
}

void do_nothing(const uint8_t gpio) {
    printf(">>> Doing nothing\n");
}

void on_target(homekit_value_t value) {
    target_state.value = value;
    switch (target_state.value.int_value) {
        case 1:
            printf(">>> Set to HEAT\n");
            led_code(LED_GPIO, FUNCTION_B);
            break;
            
        case 2:
            printf(">>> Set to COOL\n");
            led_code(LED_GPIO, FUNCTION_C);
            break;
            
        default:
            printf(">>> Set to OFF\n");
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
}

void save_settings() {
    sysparam_status_t status;
    bool bool_value;
    int8_t int8_value;
    int32_t int32_value;
    
    status = sysparam_get_bool("show_setup", &bool_value);
    if (status == SYSPARAM_OK && bool_value != show_setup.value.bool_value) {
        sysparam_set_bool("show_setup", show_setup.value.bool_value);
    }
    
    status = sysparam_get_int8("device_type", &int8_value);
    if (status == SYSPARAM_OK && int8_value != device_type.value.int_value) {
        sysparam_set_int8("device_type", device_type.value.int_value);
    }
    
    switch (device_type_static) {
        case 1:
        case 11:
            status = sysparam_get_bool("gpio14_toggle", &bool_value);
            if (status == SYSPARAM_OK && bool_value != gpio14_toggle.value.bool_value) {
                sysparam_set_bool("gpio14_toggle", gpio14_toggle.value.bool_value);
            }
            break;
            
        case 5:
        case 6:
        case 9:
            status = sysparam_get_int8("dht_sensor_type", &int8_value);
            if (status == SYSPARAM_OK && int8_value != dht_sensor_type.value.int_value) {
                sysparam_set_int8("dht_sensor_type", dht_sensor_type.value.int_value);
            }
            break;
            
        case 7:
            status = sysparam_get_int8("valve_type", &int8_value);
            if (status == SYSPARAM_OK && int8_value != custom_valve_type.value.int_value) {
                sysparam_set_int8("valve_type", custom_valve_type.value.int_value);
                valve_type.value.int_value = int8_value;
                homekit_characteristic_notify(&valve_type, valve_type.value);
            }
            
            status = sysparam_get_int32("set_duration", &int32_value);
            if (status == SYSPARAM_OK && int32_value != set_duration.value.int_value) {
                sysparam_set_int32("set_duration", set_duration.value.int_value);
            }
            break;
            
        case 8:
            status = sysparam_get_int8("garagedoor_working_time", &int8_value);
            if (status == SYSPARAM_OK && int8_value != custom_garagedoor_working_time.value.int_value) {
                sysparam_set_int8("garagedoor_working_time", custom_garagedoor_working_time.value.int_value);
            }
            
            status = sysparam_get_bool("garagedoor_has_stop", &bool_value);
            if (status == SYSPARAM_OK && bool_value != custom_garagedoor_has_stop.value.bool_value) {
                sysparam_set_bool("garagedoor_has_stop", custom_garagedoor_has_stop.value.bool_value);
            }
            
            status = sysparam_get_bool("garagedoor_sensor_close_nc", &bool_value);
            if (status == SYSPARAM_OK && bool_value != custom_garagedoor_sensor_close_nc.value.bool_value) {
                sysparam_set_bool("garagedoor_sensor_close_nc", custom_garagedoor_sensor_close_nc.value.bool_value);
            }
            
            status = sysparam_get_bool("garagedoor_sensor_open_nc", &bool_value);
            if (status == SYSPARAM_OK && bool_value != custom_garagedoor_sensor_open_nc.value.bool_value) {
                sysparam_set_bool("garagedoor_sensor_open_nc", custom_garagedoor_sensor_open_nc.value.bool_value);
            }
            
            status = sysparam_get_bool("garagedoor_has_sensor_open", &bool_value);
            if (status == SYSPARAM_OK && bool_value != custom_garagedoor_has_sensor_open.value.bool_value) {
                sysparam_set_bool("garagedoor_has_sensor_open", custom_garagedoor_has_sensor_open.value.bool_value);
            }
            
            status = sysparam_get_bool("garagedoor_control_with_button", &bool_value);
            if (status == SYSPARAM_OK && bool_value != custom_garagedoor_control_with_button.value.bool_value) {
                sysparam_set_bool("garagedoor_control_with_button", custom_garagedoor_control_with_button.value.bool_value);
            }
            break;
            
        default:
            break;
    }
}

void device_restart_task() {
    vTaskDelay(5500 / portTICK_PERIOD_MS);
    
    if (ota_firmware.value.bool_value) {
        rboot_set_temp_rom(1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        sysparam_set_string("ota_repo", "RavenSystem/ravencore");
        vTaskDelay(100 / portTICK_PERIOD_MS);
        sysparam_set_string("ota_file", "main.bin");
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    sdk_system_restart();
    
    vTaskDelete(NULL);
}

void device_restart() {
    printf(">>> Restarting device\n");
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

void reset_call_task() {
    printf(">>> Resetting device to factory defaults\n");
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
    sysparam_set_bool("show_setup", false);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    sysparam_set_int8("device_type", 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    sysparam_set_bool("gpio14_toggle", false);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    sysparam_set_int8("dht_sensor_type", 2);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    sysparam_set_int8("valve_type", 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    sysparam_set_int32("set_duration", 900);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    sysparam_set_int8("garagedoor_working_time", 20);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    sysparam_set_bool("garagedoor_has_stop", false);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    sysparam_set_bool("garagedoor_sensor_close_nc", false);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    sysparam_set_bool("garagedoor_sensor_open_nc", false);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    sysparam_set_bool("garagedoor_has_sensor_open", false);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    sysparam_set_bool("garagedoor_control_with_button", false);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    homekit_server_reset();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    wifi_config_reset();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    led_code(LED_GPIO, EXTRA_CONFIG_RESET);
    vTaskDelay(2200 / portTICK_PERIOD_MS);
    
    sdk_system_restart();
    
    vTaskDelete(NULL);
}

void reset_call(const uint8_t gpio) {
    led_code(LED_GPIO, WIFI_CONFIG_RESET);
    relay_write(false, relay1_gpio);
    gpio_disable(relay1_gpio);
    xTaskCreate(reset_call_task, "reset_call_task", 256, NULL, 1, NULL);
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

void switch1_on_callback(homekit_value_t value) {
    printf(">>> Toggle Switch 1\n");
    led_code(LED_GPIO, FUNCTION_A);
    switch1_on.value = value;
    relay_write(switch1_on.value.bool_value, relay1_gpio);
    printf(">>> Relay 1 -> %i\n", switch1_on.value.bool_value);
}

homekit_value_t read_switch1_on_callback() {
    return switch1_on.value;
}

void switch2_on_callback(homekit_value_t value) {
    printf(">>> Toggle Switch 2\n");
    led_code(LED_GPIO, FUNCTION_A);
    switch2_on.value = value;
    relay_write(switch2_on.value.bool_value, RELAY2_GPIO);
    printf(">>> Relay 2 -> %i\n", switch2_on.value.bool_value);
}

homekit_value_t read_switch2_on_callback() {
    return switch2_on.value;
}

void switch3_on_callback(homekit_value_t value) {
    printf(">>> Toggle Switch 3\n");
    led_code(LED_GPIO, FUNCTION_A);
    switch3_on.value = value;
    relay_write(switch3_on.value.bool_value, RELAY3_GPIO);
    printf(">>> Relay 3 -> %i\n", switch3_on.value.bool_value);
}

homekit_value_t read_switch3_on_callback() {
    return switch3_on.value;
}

void switch4_on_callback(homekit_value_t value) {
    printf(">>> Toggle Switch 4\n");
    led_code(LED_GPIO, FUNCTION_A);
    switch4_on.value = value;
    relay_write(switch4_on.value.bool_value, RELAY4_GPIO);
    printf(">>> Relay 4 -> %i\n", switch4_on.value.bool_value);
}

homekit_value_t read_switch4_on_callback() {
    return switch4_on.value;
}

void toggle_switch(const uint8_t gpio) {
    printf(">>> Toggle Switch manual\n");
    led_code(LED_GPIO, FUNCTION_A);
    switch1_on.value.bool_value = !switch1_on.value.bool_value;
    relay_write(switch1_on.value.bool_value, relay1_gpio);
    printf(">>> Relay 1 -> %i\n", switch1_on.value.bool_value);
    homekit_characteristic_notify(&switch1_on, switch1_on.value);
}

void gpio14_toggle_callback() {
    if (gpio14_toggle.value.bool_value) {
        adv_toggle_create(SONOFF_TOGGLE_GPIO);
        adv_toggle_register_callback_fn(SONOFF_TOGGLE_GPIO, toggle_switch, 2);
    } else {
        adv_toggle_destroy(SONOFF_TOGGLE_GPIO);
    }
    
    change_settings_callback();
}

void toggle_valve() {
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
        printf(">>> Valve OFF\n");
        led_code(LED_GPIO, FUNCTION_D);
        
        sdk_os_timer_disarm(&extra_func_timer);
        relay_write(false, relay1_gpio);
        
        active.value.int_value = 0;
        in_use.value.int_value = 0;
        
        homekit_characteristic_notify(&active, active.value);
        homekit_characteristic_notify(&in_use, in_use.value);
    }
}

void valve_on_callback(homekit_value_t value) {
    led_code(LED_GPIO, FUNCTION_A);
    
    active.value = value;
    in_use.value.int_value = active.value.int_value;
    
    if (active.value.int_value == 1) {
        printf(">>> Valve ON\n");
        relay_write(true, relay1_gpio);
        remaining_duration.value = set_duration.value;
        sdk_os_timer_arm(&extra_func_timer, 1000, 1);
    } else {
        printf(">>> Valve manual OFF\n");
        sdk_os_timer_disarm(&extra_func_timer);
        relay_write(false, relay1_gpio);
        remaining_duration.value.int_value = 0;
    }
    
    homekit_characteristic_notify(&in_use, in_use.value);
    homekit_characteristic_notify(&remaining_duration, remaining_duration.value);
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
    if (!custom_garagedoor_has_sensor_open.value.bool_value) {
        sdk_os_timer_disarm(&extra_func_timer);
    }
    
    relay_write(true, relay1_gpio);
    vTaskDelay(250 / portTICK_PERIOD_MS);
    relay_write(false, relay1_gpio);

    if (custom_garagedoor_has_stop.value.bool_value && gd_is_moving) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        relay_write(true, relay1_gpio);
        vTaskDelay(250 / portTICK_PERIOD_MS);
        relay_write(false, relay1_gpio);
    }

    if (!custom_garagedoor_has_sensor_open.value.bool_value) {
        if (current_door_state.value.int_value == 0 || current_door_state.value.int_value == 2) {
            printf(">>> Garage Door -> CLOSING\n");
            current_door_state.value.int_value = 3;
            sdk_os_timer_arm(&extra_func_timer, 1000, 1);
        } else if (current_door_state.value.int_value == 3) {
            printf(">>> Garage Door -> OPENING\n");
            current_door_state.value.int_value = 2;
            sdk_os_timer_arm(&extra_func_timer, 1000, 1);
        }
    }

    homekit_characteristic_notify(&current_door_state, current_door_state.value);
    
    vTaskDelete(NULL);
}

void garage_on_callback(homekit_value_t value) {
    printf(">>> Garage Door activated from iOS: Target door state -> %i\n", value.int_value);
    led_code(LED_GPIO, FUNCTION_A);
    target_door_state.value = value;
    xTaskCreate(garage_button_task, "garage_button_task", 192, NULL, 1, NULL);
}

void garage_on_button(const uint8_t gpio) {
    if (custom_garagedoor_control_with_button.value.bool_value) {
        garage_on_callback(target_door_state.value);
    } else {
        led_code(LED_GPIO, FUNCTION_D);
    }
}

homekit_value_t read_garage_on_callback() {
    return target_door_state.value;
}

static void homekit_gd_notify() {
    homekit_characteristic_notify(&target_door_state, target_door_state.value);
    homekit_characteristic_notify(&current_door_state, current_door_state.value);
}

void door_opened_0_fn_callback(const uint8_t gpio) {
    printf(">>> Garage Door -> CLOSING\n");
    gd_is_moving = true;
    target_door_state.value.int_value = 1;
    current_door_state.value.int_value = 3;
    
    homekit_gd_notify();
}

void door_opened_1_fn_callback(const uint8_t gpio) {
    printf(">>> Garage Door -> OPENED\n");
    gd_is_moving = false;
    target_door_state.value.int_value = 0;
    current_door_state.value.int_value = 0;
    
    homekit_gd_notify();
}

void door_closed_0_fn_callback(const uint8_t gpio) {
    printf(">>> Garage Door -> OPENING\n");
    gd_is_moving = true;
    target_door_state.value.int_value = 0;
    current_door_state.value.int_value = 2;
    
    if (!custom_garagedoor_has_sensor_open.value.bool_value) {
        sdk_os_timer_arm(&extra_func_timer, 1000, 1);
    }
    
    homekit_gd_notify();
}

void door_closed_1_fn_callback(const uint8_t gpio) {
    printf(">>> Garage Door -> CLOSED\n");
    gd_time_state = 0;
    gd_is_moving = false;
    target_door_state.value.int_value = 1;
    current_door_state.value.int_value = 1;
    
    if (!custom_garagedoor_has_sensor_open.value.bool_value) {
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
            printf(">>> Garage Door -> OPENED\n");
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
             printf(">>> Garage Door -> CLOSED\n");
             sdk_os_timer_disarm(&extra_func_timer);
             gd_is_moving = false;
             target_door_state.value.int_value = 1;
             current_door_state.value.int_value = 1;
             
             homekit_gd_notify();
         }
    }
}

void button_simple1_intr_callback(const uint8_t gpio) {
    if (device_type_static == 7) {
        toggle_valve();
    } else {
        toggle_switch(gpio);
    }
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
    led_code(LED_GPIO, FUNCTION_A);
    homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(0));
}

void button_event2_intr_callback(const uint8_t gpio) {
    led_code(LED_GPIO, FUNCTION_B);
    homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(1));
}

void button_event3_intr_callback(const uint8_t gpio) {
    led_code(LED_GPIO, FUNCTION_C);
    homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(2));
}

void th_button_intr_callback(const uint8_t gpio) {
    uint8_t state = target_state.value.int_value + 1;
    switch (state) {
        case 1:
            printf(">>> Thermostat set to HEAT\n");
            led_code(LED_GPIO, FUNCTION_B);
            break;
            
        case 2:
            printf(">>> Thermostat set to COOL\n");
            led_code(LED_GPIO, FUNCTION_C);
            break;

        default:
            state = 0;
            printf(">>> Thermostat set to OFF\n");
            led_code(LED_GPIO, FUNCTION_A);
            break;
    }
    
    target_state.value = HOMEKIT_UINT8(state);
    homekit_characteristic_notify(&target_state, target_state.value);
    
    update_state();
}

void temperature_sensor_worker() {
    float humidity_value, temperature_value;
    dht_sensor_type_t current_sensor_type;
    
    if (dht_sensor_type.value.int_value == 1) {
        current_sensor_type = DHT_TYPE_DHT11;
    } else {
        current_sensor_type = DHT_TYPE_DHT22;
    }
    
    if (dht_read_float_data(current_sensor_type, SENSOR_GPIO, &humidity_value, &temperature_value)) {
        printf(">>> Sensor: temperature %g, humidity %g\n", temperature_value, humidity_value);
        
        if (temperature_value != old_temperature_value) {
            old_temperature_value = temperature_value;
            current_temperature.value = HOMEKIT_FLOAT(temperature_value);
            homekit_characteristic_notify(&current_temperature, current_temperature.value);
            
            if (humidity_value != old_humidity_value) {
                old_humidity_value = humidity_value;
                current_humidity.value = HOMEKIT_FLOAT(humidity_value);
                homekit_characteristic_notify(&current_humidity, current_humidity.value);
            }
            
            if (device_type_static == 5) {
                update_state();
            }
        }
    } else {
        printf(">>> Sensor: ERROR\n");
        led_code(LED_GPIO, SENSOR_ERROR);
        
        if (current_state.value.int_value != 0 && device_type_static == 5) {
            current_state.value = HOMEKIT_UINT8(0);
            homekit_characteristic_notify(&current_state, current_state.value);
            
            relay_write(false, relay1_gpio);
        }
    }
}

void identify(homekit_value_t _value) {
    led_code(LED_GPIO, IDENTIFY_ACCESSORY);
}

void gpio_init() {
    gpio_enable(LED_GPIO, GPIO_OUTPUT);
    led_write(false);
    
    adv_button_create(BUTTON1_GPIO);
    
    sysparam_status_t status;
    bool bool_value;
    int8_t int8_value;
    int32_t int32_value;
    
    status = sysparam_get_bool("show_setup", &bool_value);
    if (status == SYSPARAM_OK) {
        show_setup.value.bool_value = bool_value;
        printf(">>> Loading show_setup -> %i\n", show_setup.value.bool_value);
    } else {
        sysparam_set_bool("show_setup", false);
        printf(">>> Setting show_setup to default -> false\n");
    }
    
    status = sysparam_get_int8("device_type", &int8_value);
    if (status == SYSPARAM_OK) {
        device_type.value.int_value = int8_value;
        device_type_static = int8_value;
        printf(">>> Loading device_type -> %i\n", device_type.value.int_value);
    } else {
        sysparam_set_int8("device_type", 1);
        printf(">>> Setting device_type to default -> 1\n");
    }
    
    switch (device_type_static) {
        case 1:
            printf(">>> Loading device gpio settings for 1\n");
            status = sysparam_get_bool("gpio14_toggle", &bool_value);
            if (status == SYSPARAM_OK) {
                gpio14_toggle.value.bool_value = bool_value;
            } else {
                sysparam_set_bool("gpio14_toggle", false);
            }
            
            adv_button_register_callback_fn(BUTTON1_GPIO, button_simple1_intr_callback, 1);
            adv_button_register_callback_fn(BUTTON1_GPIO, reset_call, 5);
            
            gpio14_toggle_callback();
            break;
            
        case 2:
            printf(">>> Loading device gpio settings for 2\n");
            adv_button_create(BUTTON2_GPIO);
            adv_button_create(BUTTON3_GPIO);
            
            adv_button_register_callback_fn(BUTTON1_GPIO, button_simple1_intr_callback, 1);
            adv_button_register_callback_fn(BUTTON2_GPIO, button_simple2_intr_callback, 1);
            
            adv_button_register_callback_fn(BUTTON3_GPIO, button_simple1_intr_callback, 1);
            adv_button_register_callback_fn(BUTTON3_GPIO, button_simple2_intr_callback, 2);
            adv_button_register_callback_fn(BUTTON3_GPIO, reset_call, 5);
            
            gpio_enable(RELAY2_GPIO, GPIO_OUTPUT);
            relay_write(switch2_on.value.bool_value, RELAY2_GPIO);
            break;
            
        case 3:
            printf(">>> Loading device gpio settings for 3\n");
            adv_button_register_callback_fn(BUTTON1_GPIO, button_event1_intr_callback, 1);
            adv_button_register_callback_fn(BUTTON1_GPIO, button_event2_intr_callback, 2);
            adv_button_register_callback_fn(BUTTON1_GPIO, button_event3_intr_callback, 3);
            adv_button_register_callback_fn(BUTTON1_GPIO, button_simple1_intr_callback, 4);
            adv_button_register_callback_fn(BUTTON1_GPIO, reset_call, 5);
            break;
            
        case 4:
            printf(">>> Loading device gpio settings for 4\n");
            adv_button_create(BUTTON2_GPIO);
            adv_button_create(BUTTON3_GPIO);
            adv_button_create(BUTTON4_GPIO);

            adv_button_register_callback_fn(BUTTON1_GPIO, button_simple1_intr_callback, 1);
            adv_button_register_callback_fn(BUTTON1_GPIO, reset_call, 5);
            
            adv_button_register_callback_fn(BUTTON2_GPIO, button_simple2_intr_callback, 1);
            adv_button_register_callback_fn(BUTTON3_GPIO, button_simple3_intr_callback, 1);
            adv_button_register_callback_fn(BUTTON4_GPIO, button_simple4_intr_callback, 1);
            
            gpio_enable(RELAY2_GPIO, GPIO_OUTPUT);
            relay_write(switch2_on.value.bool_value, RELAY2_GPIO);
            
            gpio_enable(RELAY3_GPIO, GPIO_OUTPUT);
            relay_write(switch3_on.value.bool_value, RELAY3_GPIO);
            
            gpio_enable(RELAY4_GPIO, GPIO_OUTPUT);
            relay_write(switch4_on.value.bool_value, RELAY4_GPIO);
            break;
            
        case 5:
            printf(">>> Loading device gpio settings for 5\n");
            status = sysparam_get_int8("dht_sensor_type", &int8_value);
            if (status == SYSPARAM_OK) {
                dht_sensor_type.value.int_value = int8_value;
            } else {
                sysparam_set_int8("dht_sensor_type", 2);
            }
            
            adv_button_register_callback_fn(BUTTON1_GPIO, th_button_intr_callback, 1);
            adv_button_register_callback_fn(BUTTON1_GPIO, reset_call, 5);
            
            gpio_set_pullup(SENSOR_GPIO, false, false);
            
            sdk_os_timer_setfn(&extra_func_timer, temperature_sensor_worker, NULL);
            sdk_os_timer_arm(&extra_func_timer, POLL_PERIOD, 1);
            break;
            
        case 6:
            printf(">>> Loading device gpio settings for 6\n");
            status = sysparam_get_int8("dht_sensor_type", &int8_value);
            if (status == SYSPARAM_OK) {
                dht_sensor_type.value.int_value = int8_value;
            } else {
                sysparam_set_int8("dht_sensor_type", 2);
            }
            
            adv_button_register_callback_fn(BUTTON1_GPIO, button_simple1_intr_callback, 1);
            adv_button_register_callback_fn(BUTTON1_GPIO, reset_call, 5);
            
            gpio_set_pullup(SENSOR_GPIO, false, false);
            
            sdk_os_timer_setfn(&extra_func_timer, temperature_sensor_worker, NULL);
            sdk_os_timer_arm(&extra_func_timer, POLL_PERIOD, 1);
            break;
            
        case 7:
            printf(">>> Loading device gpio settings for 7\n");
            status = sysparam_get_int8("valve_type", &int8_value);
            if (status == SYSPARAM_OK) {
                valve_type.value.int_value = int8_value;
                custom_valve_type.value.int_value = int8_value;
            } else {
                sysparam_set_int8("valve_type", 0);
            }
            
            status = sysparam_get_int32("set_duration", &int32_value);
            if (status == SYSPARAM_OK) {
                set_duration.value.int_value = int32_value;
            } else {
                sysparam_set_int32("set_duration", 900);
            }

            adv_button_register_callback_fn(BUTTON1_GPIO, button_simple1_intr_callback, 1);
            adv_button_register_callback_fn(BUTTON1_GPIO, reset_call, 5);
            
            sdk_os_timer_setfn(&extra_func_timer, valve_control, NULL);
            break;
            
        case 8:
            printf(">>> Loading device gpio settings for 8\n");
            status = sysparam_get_int8("garagedoor_working_time", &int8_value);
            if (status == SYSPARAM_OK) {
                custom_garagedoor_working_time.value.int_value = int8_value;
            } else {
                sysparam_set_int8("garagedoor_working_time", 20);
            }
            
            status = sysparam_get_bool("garagedoor_has_stop", &bool_value);
            if (status == SYSPARAM_OK) {
                custom_garagedoor_has_stop.value.bool_value = bool_value;
            } else {
                sysparam_set_bool("garagedoor_has_stop", false);
            }
            
            status = sysparam_get_bool("garagedoor_sensor_close_nc", &bool_value);
            if (status == SYSPARAM_OK) {
                custom_garagedoor_sensor_close_nc.value.bool_value = bool_value;
            } else {
                sysparam_set_bool("garagedoor_sensor_close_nc", false);
            }
            
            status = sysparam_get_bool("garagedoor_sensor_open_nc", &bool_value);
            if (status == SYSPARAM_OK) {
                custom_garagedoor_sensor_open_nc.value.bool_value = bool_value;
            } else {
                sysparam_set_bool("garagedoor_sensor_open_nc", false);
            }
            
            status = sysparam_get_bool("garagedoor_has_sensor_open", &bool_value);
            if (status == SYSPARAM_OK) {
                custom_garagedoor_has_sensor_open.value.bool_value = bool_value;
            } else {
                sysparam_set_bool("garagedoor_has_sensor_open", false);
            }
            
            status = sysparam_get_bool("garagedoor_control_with_button", &bool_value);
            if (status == SYSPARAM_OK) {
                custom_garagedoor_control_with_button.value.bool_value = bool_value;
            } else {
                sysparam_set_bool("garagedoor_control_with_button", false);
            }
            
            adv_button_register_callback_fn(BUTTON1_GPIO, garage_on_button, 1);
            adv_button_register_callback_fn(BUTTON1_GPIO, reset_call, 5);
            
            adv_toggle_create(DOOR_CLOSED_GPIO);
            
            if (custom_garagedoor_sensor_close_nc.value.bool_value) {
                adv_toggle_register_callback_fn(DOOR_CLOSED_GPIO, door_closed_1_fn_callback, 0);
                adv_toggle_register_callback_fn(DOOR_CLOSED_GPIO, door_closed_0_fn_callback, 1);
            } else {
                adv_toggle_register_callback_fn(DOOR_CLOSED_GPIO, door_closed_0_fn_callback, 0);
                adv_toggle_register_callback_fn(DOOR_CLOSED_GPIO, door_closed_1_fn_callback, 1);
            }
            
            if (custom_garagedoor_has_sensor_open.value.bool_value) {
                adv_toggle_create(DOOR_OPENED_GPIO);
                
                if (custom_garagedoor_sensor_open_nc.value.bool_value) {
                    adv_toggle_register_callback_fn(DOOR_OPENED_GPIO, door_opened_1_fn_callback, 0);
                    adv_toggle_register_callback_fn(DOOR_OPENED_GPIO, door_opened_0_fn_callback, 1);
                } else {
                    adv_toggle_register_callback_fn(DOOR_OPENED_GPIO, door_opened_0_fn_callback, 0);
                    adv_toggle_register_callback_fn(DOOR_OPENED_GPIO, door_opened_1_fn_callback, 1);
                }

            } else {
                sdk_os_timer_setfn(&extra_func_timer, door_opened_countdown_timer, NULL);
            }
            
            break;
            
        case 9:
            printf(">>> Loading device gpio settings for 9\n");
            status = sysparam_get_int8("dht_sensor_type", &int8_value);
            if (status == SYSPARAM_OK) {
                dht_sensor_type.value.int_value = int8_value;
            } else {
                sysparam_set_int8("dht_sensor_type", 2);
            }
            
            adv_button_register_callback_fn(BUTTON1_GPIO, button_event1_intr_callback, 1);
            adv_button_register_callback_fn(BUTTON1_GPIO, button_event2_intr_callback, 2);
            adv_button_register_callback_fn(BUTTON1_GPIO, button_event3_intr_callback, 3);
            adv_button_register_callback_fn(BUTTON1_GPIO, button_simple1_intr_callback, 4);
            adv_button_register_callback_fn(BUTTON1_GPIO, reset_call, 5);
            
            gpio_set_pullup(SENSOR_GPIO, false, false);
            
            sdk_os_timer_setfn(&extra_func_timer, temperature_sensor_worker, NULL);
            sdk_os_timer_arm(&extra_func_timer, POLL_PERIOD, 1);
            break;
            
        case 10:
            printf(">>> Loading device gpio settings for 10\n");
            relay1_gpio = 2;
            
            adv_button_register_callback_fn(BUTTON1_GPIO, button_event1_intr_callback, 1);
            adv_button_register_callback_fn(BUTTON1_GPIO, button_event2_intr_callback, 2);
            adv_button_register_callback_fn(BUTTON1_GPIO, button_event3_intr_callback, 3);
            adv_button_register_callback_fn(BUTTON1_GPIO, button_simple1_intr_callback, 4);
            adv_button_register_callback_fn(BUTTON1_GPIO, reset_call, 5);
            break;
            
        case 11:
            printf(">>> Loading device gpio settings for 11\n");
            adv_button_create(BUTTON2_GPIO);
            adv_button_create(BUTTON3_GPIO);
            
            adv_button_register_callback_fn(BUTTON1_GPIO, button_simple1_intr_callback, 1);
            adv_button_register_callback_fn(BUTTON1_GPIO, reset_call, 5);
            
            adv_button_register_callback_fn(BUTTON2_GPIO, button_simple2_intr_callback, 1);
            adv_button_register_callback_fn(BUTTON3_GPIO, button_simple3_intr_callback, 1);
            
            gpio_enable(RELAY2_GPIO, GPIO_OUTPUT);
            relay_write(switch2_on.value.bool_value, RELAY2_GPIO);
            
            gpio_enable(RELAY3_GPIO, GPIO_OUTPUT);
            relay_write(switch3_on.value.bool_value, RELAY3_GPIO);
            break;
            
        default:
            // A wish from compiler
            break;
    }
    
    gpio_enable(relay1_gpio, GPIO_OUTPUT);
    relay_write(switch1_on.value.bool_value, relay1_gpio);
    
    sdk_os_timer_setfn(&change_settings_timer, save_settings, NULL);
    
    wifi_config_init("RavenCore", NULL, on_wifi_ready);
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
homekit_characteristic_t button_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Action Button");
homekit_characteristic_t th_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Thermostat");
homekit_characteristic_t temp_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Temperature");
homekit_characteristic_t hum_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Humidity");
homekit_characteristic_t valve_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Water Valve");
homekit_characteristic_t garage_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Garage Door");

homekit_characteristic_t setup_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Setup", .id=100);
homekit_characteristic_t device_type_name = HOMEKIT_CHARACTERISTIC_(CUSTOM_DEVICE_TYPE_NAME, "Switch Basic", .id=101);

homekit_characteristic_t firmware = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, "0.4.10");

homekit_accessory_category_t accessory_category = homekit_accessory_category_switch;

void create_accessory_name() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    char *name_value = malloc(17);
    snprintf(name_value, 17, "RavenCore %02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    name.value = HOMEKIT_STRING(name_value);
    
    char *serial_value = malloc(13);
    snprintf(serial_value, 13, "%02X%02X%02X%02X%02X%02X", macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);
    serial.value = HOMEKIT_STRING(serial_value);
}

homekit_server_config_t config;

void create_accessory() {
    uint8_t service_count = 3, service_number = 2;
    
    // Total service count
    if (show_setup.value.bool_value) {
        service_count++;
    }
    
    if (device_type_static == 2 || device_type_static == 3 || device_type_static == 10) {
        service_count++;
    } else if (device_type_static == 6) {
        service_count += 2;
    } else if (device_type_static == 4 || device_type_static == 9) {
        service_count += 3;
    }
    
    
    // Accessory Category selection
    if (device_type_static == 3 || device_type_static == 9) {
        accessory_category = homekit_accessory_category_outlet;
    } else if (device_type_static == 5) {
        accessory_category = homekit_accessory_category_thermostat;
    } else if (device_type_static == 7) {
        accessory_category = homekit_accessory_category_sprinkler;
    } else if (device_type_static == 8) {
        accessory_category = homekit_accessory_category_garage;
    }
    
    
    homekit_accessory_t **accessories = calloc(2, sizeof(homekit_accessory_t*));

    homekit_accessory_t *sonoff = accessories[0] = calloc(1, sizeof(homekit_accessory_t));
        sonoff->id = 1;
        sonoff->category = accessory_category;
        sonoff->config_number = 000412;   // Matches as example: firmware_revision 2.3.8 = 02.03.10 (octal) = config_number 020310
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

            if (device_type_static == 2) {
                printf(">>> Creating accessory for type 2\n");
                
                char *device_type_name_value = malloc(12);
                snprintf(device_type_name_value, 12, "Switch Dual");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                homekit_service_t *sonoff_switch1 = sonoff->services[1] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch1->id = 8;
                sonoff_switch1->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch1->primary = true;
                sonoff_switch1->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
                    sonoff_switch1->characteristics[0] = &switch1_service_name;
                    sonoff_switch1->characteristics[1] = &switch1_on;
                    sonoff_switch1->characteristics[2] = &show_setup;
                
                homekit_service_t *sonoff_switch2 = sonoff->services[2] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch2->id = 12;
                sonoff_switch2->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch2->primary = false;
                sonoff_switch2->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_switch2->characteristics[0] = &switch2_service_name;
                    sonoff_switch2->characteristics[1] = &switch2_on;
                
                service_number = 3;
            } else if (device_type_static == 3) {
                printf(">>> Creating accessory for type 3\n");
                
                char *device_type_name_value = malloc(14);
                snprintf(device_type_name_value, 14, "Socket Button");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                homekit_service_t *sonoff_outlet = sonoff->services[1] = calloc(1, sizeof(homekit_service_t));
                sonoff_outlet->id = 21;
                sonoff_outlet->type = HOMEKIT_SERVICE_OUTLET;
                sonoff_outlet->primary = true;
                sonoff_outlet->characteristics = calloc(5, sizeof(homekit_characteristic_t*));
                    sonoff_outlet->characteristics[0] = &outlet_service_name;
                    sonoff_outlet->characteristics[1] = &switch1_on;
                    sonoff_outlet->characteristics[2] = &switch_outlet_in_use;
                    sonoff_outlet->characteristics[3] = &show_setup;
                
                homekit_service_t *sonoff_button = sonoff->services[2] = calloc(1, sizeof(homekit_service_t));
                sonoff_button->id = 26;
                sonoff_button->type = HOMEKIT_SERVICE_STATELESS_PROGRAMMABLE_SWITCH;
                sonoff_button->primary = false;
                sonoff_button->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_button->characteristics[0] = &button_service_name;
                    sonoff_button->characteristics[1] = &button_event;
                
                service_number = 3;
            } else if (device_type_static == 4) {
                printf(">>> Creating accessory for type 4\n");
                
                char *device_type_name_value = malloc(11);
                snprintf(device_type_name_value, 11, "Switch 4ch");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                homekit_service_t *sonoff_switch1 = sonoff->services[1] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch1->id = 8;
                sonoff_switch1->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch1->primary = true;
                sonoff_switch1->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
                    sonoff_switch1->characteristics[0] = &switch1_service_name;
                    sonoff_switch1->characteristics[1] = &switch1_on;
                    sonoff_switch1->characteristics[2] = &show_setup;
                
                homekit_service_t *sonoff_switch2 = sonoff->services[2] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch2->id = 12;
                sonoff_switch2->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch2->primary = false;
                sonoff_switch2->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_switch2->characteristics[0] = &switch2_service_name;
                    sonoff_switch2->characteristics[1] = &switch2_on;
                
                homekit_service_t *sonoff_switch3 = sonoff->services[3] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch3->id = 15;
                sonoff_switch3->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch3->primary = false;
                sonoff_switch3->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_switch3->characteristics[0] = &switch3_service_name;
                    sonoff_switch3->characteristics[1] = &switch3_on;
                
                homekit_service_t *sonoff_switch4 = sonoff->services[4] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch4->id = 18;
                sonoff_switch4->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch4->primary = false;
                sonoff_switch4->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_switch4->characteristics[0] = &switch4_service_name;
                    sonoff_switch4->characteristics[1] = &switch4_on;
                
                service_number = 5;
            } else if (device_type_static == 5) {
                printf(">>> Creating accessory for type 5\n");
                
                char *device_type_name_value = malloc(11);
                snprintf(device_type_name_value, 11, "Thermostat");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                homekit_service_t *sonoff_th = sonoff->services[1] = calloc(1, sizeof(homekit_service_t));
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
            } else if (device_type_static == 6) {
                printf(">>> Creating accessory for type 6\n");
                
                char *device_type_name_value = malloc(17);
                snprintf(device_type_name_value, 17, "Switch TH Sensor");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                homekit_service_t *sonoff_switch1 = sonoff->services[1] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch1->id = 8;
                sonoff_switch1->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch1->primary = true;
                sonoff_switch1->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
                    sonoff_switch1->characteristics[0] = &switch1_service_name;
                    sonoff_switch1->characteristics[1] = &switch1_on;
                    sonoff_switch1->characteristics[2] = &show_setup;
                
                homekit_service_t *sonoff_temp = sonoff->services[2] = calloc(1, sizeof(homekit_service_t));
                sonoff_temp->id = 38;
                sonoff_temp->type = HOMEKIT_SERVICE_TEMPERATURE_SENSOR;
                sonoff_temp->primary = false;
                sonoff_temp->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_temp->characteristics[0] = &temp_service_name;
                    sonoff_temp->characteristics[1] = &current_temperature;
                
                homekit_service_t *sonoff_hum = sonoff->services[3] = calloc(1, sizeof(homekit_service_t));
                sonoff_hum->id = 41;
                sonoff_hum->type = HOMEKIT_SERVICE_HUMIDITY_SENSOR;
                sonoff_hum->primary = false;
                sonoff_hum->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_hum->characteristics[0] = &hum_service_name;
                    sonoff_hum->characteristics[1] = &current_humidity;
                
                service_number = 4;
            } else if (device_type_static == 7) {
                printf(">>> Creating accessory for type 7\n");
                
                char *device_type_name_value = malloc(12);
                snprintf(device_type_name_value, 12, "Water Valve");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                homekit_service_t *sonoff_valve = sonoff->services[1] = calloc(1, sizeof(homekit_service_t));
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
            } else if (device_type_static == 8) {
                printf(">>> Creating accessory for type 8\n");
                
                char *device_type_name_value = malloc(12);
                snprintf(device_type_name_value, 12, "Garage Door");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                homekit_service_t *sonoff_garage = sonoff->services[1] = calloc(1, sizeof(homekit_service_t));
                sonoff_garage->id = 52;
                sonoff_garage->type = HOMEKIT_SERVICE_GARAGE_DOOR_OPENER;
                sonoff_garage->primary = true;
                sonoff_garage->characteristics = calloc(6, sizeof(homekit_characteristic_t*));
                    sonoff_garage->characteristics[0] = &garage_service_name;
                    sonoff_garage->characteristics[1] = &current_door_state;
                    sonoff_garage->characteristics[2] = &target_door_state;
                    sonoff_garage->characteristics[3] = &obstruction_detected;
                    sonoff_garage->characteristics[4] = &show_setup;
            } else if (device_type_static == 9) {
                printf(">>> Creating accessory for type 9\n");
                
                char *device_type_name_value = malloc(17);
                snprintf(device_type_name_value, 17, "Socket Button TH");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                homekit_service_t *sonoff_outlet = sonoff->services[1] = calloc(1, sizeof(homekit_service_t));
                sonoff_outlet->id = 21;
                sonoff_outlet->type = HOMEKIT_SERVICE_OUTLET;
                sonoff_outlet->primary = true;
                sonoff_outlet->characteristics = calloc(5, sizeof(homekit_characteristic_t*));
                    sonoff_outlet->characteristics[0] = &outlet_service_name;
                    sonoff_outlet->characteristics[1] = &switch1_on;
                    sonoff_outlet->characteristics[2] = &switch_outlet_in_use;
                    sonoff_outlet->characteristics[3] = &show_setup;
                
                homekit_service_t *sonoff_button = sonoff->services[2] = calloc(1, sizeof(homekit_service_t));
                sonoff_button->id = 26;
                sonoff_button->type = HOMEKIT_SERVICE_STATELESS_PROGRAMMABLE_SWITCH;
                sonoff_button->primary = false;
                sonoff_button->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_button->characteristics[0] = &button_service_name;
                    sonoff_button->characteristics[1] = &button_event;
                
                homekit_service_t *sonoff_temp = sonoff->services[3] = calloc(1, sizeof(homekit_service_t));
                sonoff_temp->id = 38;
                sonoff_temp->type = HOMEKIT_SERVICE_TEMPERATURE_SENSOR;
                sonoff_temp->primary = false;
                sonoff_temp->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_temp->characteristics[0] = &temp_service_name;
                    sonoff_temp->characteristics[1] = &current_temperature;
                
                homekit_service_t *sonoff_hum = sonoff->services[4] = calloc(1, sizeof(homekit_service_t));
                sonoff_hum->id = 41;
                sonoff_hum->type = HOMEKIT_SERVICE_HUMIDITY_SENSOR;
                sonoff_hum->primary = false;
                sonoff_hum->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_hum->characteristics[0] = &hum_service_name;
                    sonoff_hum->characteristics[1] = &current_humidity;
                
                service_number = 5;
            } else if (device_type_static == 10) {
                printf(">>> Creating accessory for type 10\n");
                
                char *device_type_name_value = malloc(17);
                snprintf(device_type_name_value, 17, "ESP01 Switch Btn");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                homekit_service_t *sonoff_switch1 = sonoff->services[1] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch1->id = 8;
                sonoff_switch1->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch1->primary = true;
                sonoff_switch1->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
                    sonoff_switch1->characteristics[0] = &switch1_service_name;
                    sonoff_switch1->characteristics[1] = &switch1_on;
                    sonoff_switch1->characteristics[2] = &show_setup;
                
                homekit_service_t *sonoff_button = sonoff->services[2] = calloc(1, sizeof(homekit_service_t));
                sonoff_button->id = 26;
                sonoff_button->type = HOMEKIT_SERVICE_STATELESS_PROGRAMMABLE_SWITCH;
                sonoff_button->primary = false;
                sonoff_button->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_button->characteristics[0] = &button_service_name;
                    sonoff_button->characteristics[1] = &button_event;
                
                service_number = 3;
            } else if (device_type_static == 11) {
                printf(">>> Creating accessory for type 11\n");
                
                char *device_type_name_value = malloc(11);
                snprintf(device_type_name_value, 11, "Switch 3ch");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                homekit_service_t *sonoff_switch1 = sonoff->services[1] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch1->id = 8;
                sonoff_switch1->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch1->primary = true;
                sonoff_switch1->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
                sonoff_switch1->characteristics[0] = &switch1_service_name;
                sonoff_switch1->characteristics[1] = &switch1_on;
                sonoff_switch1->characteristics[2] = &show_setup;
                
                homekit_service_t *sonoff_switch2 = sonoff->services[2] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch2->id = 12;
                sonoff_switch2->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch2->primary = false;
                sonoff_switch2->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                sonoff_switch2->characteristics[0] = &switch2_service_name;
                sonoff_switch2->characteristics[1] = &switch2_on;
                
                homekit_service_t *sonoff_switch3 = sonoff->services[3] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch3->id = 15;
                sonoff_switch3->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch3->primary = false;
                sonoff_switch3->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                sonoff_switch3->characteristics[0] = &switch3_service_name;
                sonoff_switch3->characteristics[1] = &switch3_on;
                
                service_number = 4;
            } else { // device_type_static == 1
                printf(">>> Creating accessory for type 1\n");
                
                char *device_type_name_value = malloc(13);
                snprintf(device_type_name_value, 13, "Switch Basic");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                homekit_service_t *sonoff_switch1 = sonoff->services[1] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch1->id = 8;
                sonoff_switch1->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch1->primary = true;
                sonoff_switch1->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
                    sonoff_switch1->characteristics[0] = &switch1_service_name;
                    sonoff_switch1->characteristics[1] = &switch1_on;
                    sonoff_switch1->characteristics[2] = &show_setup;
            }

            // Setup Accessory, visible only in 3party Apps
            if (show_setup.value.bool_value) {
                homekit_service_t *sonoff_setup = sonoff->services[service_number] = calloc(1, sizeof(homekit_service_t));
                sonoff_setup->id = 99;
                sonoff_setup->type = HOMEKIT_SERVICE_CUSTOM_SETUP;
                sonoff_setup->primary = false;
        
                uint8_t setting_count = 5, setting_number = 4;
                
                sysparam_status_t status;
                char *char_value;
                
                status = sysparam_get_string("ota_repo", &char_value);
                if (status == SYSPARAM_OK) {
                    setting_count++;
                }
                
                if (device_type_static == 1 || device_type_static == 5 || device_type_static == 6 || device_type_static == 7 || device_type_static == 9) {
                    setting_count++;
                } else if (device_type_static == 8) {
                    setting_count += 6;
                }
        
                sonoff_setup->characteristics = calloc(setting_count, sizeof(homekit_characteristic_t*));
                    sonoff_setup->characteristics[0] = &setup_service_name;
                    sonoff_setup->characteristics[1] = &device_type_name;
                    sonoff_setup->characteristics[2] = &device_type;
                    sonoff_setup->characteristics[3] = &reboot_device;
                
                    if (status == SYSPARAM_OK) {
                        sonoff_setup->characteristics[setting_number] = &ota_firmware;
                        setting_number++;
                    }
                
                    if (device_type_static == 1) {
                        sonoff_setup->characteristics[setting_number] = &gpio14_toggle;
                        setting_number++;
                    } else if (device_type_static == 5 || device_type_static == 6 || device_type_static == 9) {
                        sonoff_setup->characteristics[setting_number] = &dht_sensor_type;
                        setting_number++;
                    } else if (device_type_static == 7) {
                        sonoff_setup->characteristics[setting_number] = &custom_valve_type;
                        setting_number++;
                    } else if (device_type_static == 8) {
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_has_stop;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_sensor_close_nc;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_sensor_open_nc;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_has_sensor_open;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_working_time;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_control_with_button;
                        setting_number++;
                    }
            }
    
    config.accessories = accessories;
    config.password = "021-82-017";
    
    homekit_server_init(&config);
}

void on_wifi_ready() {
    led_code(LED_GPIO, WIFI_CONNECTED);
    
    create_accessory_name();
    create_accessory();
}

void user_init(void) {
    uart_set_baud(0, 115200);
    
    printf(">>> RavenCore firmware loaded\n");
    printf(">>> Developed by RavenSystem - José Antonio Jiménez\n");
    printf(">>> Firmware revision: %s\n\n", firmware.value.string_value);
    
    gpio_init();
}
