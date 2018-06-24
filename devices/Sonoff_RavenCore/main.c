/*
 * Sonoff RavenCore
 * 
 * v0.1.0
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
#include <string.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_common.h>
#include <rboot-api.h>
#include <sysparam.h>

#include <etstimer.h>
#include <esplibs/libmain.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <led_codes.h>

#include <dht/dht.h>

#include "custom_characteristics.h"

#define BUTTON_GPIO         0
#define LED_GPIO            13
#define RELAY_GPIO          12
#define SWITCH_GPIO         14

#define DEBOUNCE_TIME       500     / portTICK_PERIOD_MS
#define RESET_TIME          10000

#define POLL_PERIOD         30000

uint8_t switch_old_state, switch_state;
uint16_t switch_value = 65535;
uint32_t last_button_event_time;
float old_humidity_value = 0.0, old_temperature_value = 0.0;
static ETSTimer switch_timer, reset_timer, device_restart_timer, change_settings_timer, thermostat_timer;

void switch1_on_callback();
void button_intr_callback(uint8_t gpio);
void switch_worker();

void change_settings_callback();
void ota_firmware_callback();
void gpio14_toggle_callback();

homekit_characteristic_t switch1_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch1_on_callback));
homekit_characteristic_t switch2_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch1_on_callback));
homekit_characteristic_t switch3_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch1_on_callback));
homekit_characteristic_t switch4_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch1_on_callback));

homekit_characteristic_t show_setup = HOMEKIT_CHARACTERISTIC_(CUSTOM_SHOW_SETUP, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t ota_firmware = HOMEKIT_CHARACTERISTIC_(CUSTOM_OTA_UPDATE, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(ota_firmware_callback));
homekit_characteristic_t device_type = HOMEKIT_CHARACTERISTIC_(CUSTOM_DEVICE_TYPE, 1, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t gpio14_toggle = HOMEKIT_CHARACTERISTIC_(CUSTOM_GPIO14_TOGGLE, true, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(gpio14_toggle_callback));

void relay_write(bool on) {
    gpio_write(RELAY_GPIO, on ? 1 : 0);
}

void led_write(bool on) {
    gpio_write(LED_GPIO, on ? 0 : 1);
}

void save_settings() {
    sysparam_status_t status;
    bool bool_value;
    int8_t int8_value;
    
    status = sysparam_get_bool("show_setup", &bool_value);
    if (status == SYSPARAM_OK) {
        if (bool_value != show_setup.value.bool_value) {
            status = sysparam_set_bool("show_setup", show_setup.value.bool_value);
        }
    }
    
    status = sysparam_get_int8("device_type", &int8_value);
    if (status == SYSPARAM_OK) {
        if (int8_value != device_type.value.int_value) {
            status = sysparam_set_int8("device_type", device_type.value.int_value);
        }
    }
    
    switch (device_type.value.int_value) {
        case 1:
            status = sysparam_get_bool("gpio14_toggle", &bool_value);
            if (status == SYSPARAM_OK) {
                if (bool_value != gpio14_toggle.value.bool_value) {
                    status = sysparam_set_bool("gpio14_toggle", gpio14_toggle.value.bool_value);
                }
            }
            break;
            
        default:
            break;
    }
    
}

void device_restart() {
    if (ota_firmware.value.bool_value) {
        rboot_set_temp_rom(1);
    }
    sdk_system_restart();
}

void set_restart_countdown() {
    led_code(LED_GPIO, RESTART_DEVICE);
    sdk_os_timer_setfn(&device_restart_timer, device_restart, NULL);
    sdk_os_timer_arm(&device_restart_timer, 5500, 0);
}

void reset_call() {
    homekit_server_reset();
    wifi_config_reset();

    set_restart_countdown();
}

void ota_firmware_callback() {
    if (ota_firmware.value.bool_value) {
        sdk_os_timer_setfn(&device_restart_timer, set_restart_countdown, NULL);
        sdk_os_timer_arm(&device_restart_timer, 10000, 0);
    } else {
        sdk_os_timer_disarm(&device_restart_timer);
    }
}

void change_settings_callback() {
    sdk_os_timer_disarm(&change_settings_timer);
    sdk_os_timer_arm(&change_settings_timer, 3000, 0);
}

#define maxvalue_unsigned(x) ((1 << (8 * sizeof(x))) - 1)
void switch_evaluate() {        // Based on https://github.com/pcsaito/esp-homekit-demo/tree/LPFToggle
    switch_value += ((gpio_read(SWITCH_GPIO) * maxvalue_unsigned(switch_value)) - switch_value) >> 3;
    switch_state = (switch_value > (maxvalue_unsigned(switch_value) >> 1));
}

void gpio14_toggle_callback() {
    if (gpio14_toggle.value.bool_value) {
        gpio_enable(SWITCH_GPIO, GPIO_INPUT);
        gpio_set_pullup(SWITCH_GPIO, true, true);
        sdk_os_timer_arm(&switch_timer, 30, 1);
        
        switch_evaluate();
        switch_old_state = switch_state;
    } else {
        sdk_os_timer_disarm(&switch_timer);
        gpio_disable(SWITCH_GPIO);
    }
    
    change_settings_callback();
}

void gpio_init() {
    gpio_enable(LED_GPIO, GPIO_OUTPUT);
    led_write(false);
    
    gpio_enable(RELAY_GPIO, GPIO_OUTPUT);
    relay_write(switch1_on.value.bool_value);
    
    gpio_set_pullup(BUTTON_GPIO, true, true);
    gpio_set_interrupt(BUTTON_GPIO, GPIO_INTTYPE_EDGE_ANY, button_intr_callback);
    
    sdk_os_timer_setfn(&switch_timer, switch_worker, NULL);
    
    sdk_os_timer_setfn(&reset_timer, reset_call, NULL);
    
    sdk_os_timer_setfn(&change_settings_timer, save_settings, NULL);
    
    last_button_event_time = xTaskGetTickCountFromISR();
}

void switch1_on_callback() {
    led_code(LED_GPIO, FUNCTION_A);
    relay_write(switch1_on.value.bool_value);
}

void toggle_switch() {
    switch1_on.value.bool_value = !switch1_on.value.bool_value;
    switch1_on_callback();
    homekit_characteristic_notify(&switch1_on, switch1_on.value);
}

void button_intr_callback(uint8_t gpio) {
    uint32_t now = xTaskGetTickCountFromISR();
    
    if (gpio_read(BUTTON_GPIO) == 0) {
        sdk_os_timer_arm(&reset_timer, RESET_TIME, 0);
    } else {
        sdk_os_timer_disarm(&reset_timer);
        
        if ((now - last_button_event_time) > DEBOUNCE_TIME) {
            last_button_event_time = now;
            toggle_switch();
        }
    }
}

void switch_worker() {
    switch_evaluate();
    
    if (switch_state != switch_old_state) {
        switch_old_state = switch_state;
        toggle_switch();
    }
}

void identify(homekit_value_t _value) {
    led_code(LED_GPIO, IDENTIFY_ACCESSORY);
}

void extra_init() {
    if (gpio14_toggle.value.bool_value) {
        gpio_enable(SWITCH_GPIO, GPIO_INPUT);
        gpio_set_pullup(SWITCH_GPIO, true, true);
        sdk_os_timer_arm(&switch_timer, 40, 1);
        
        switch_evaluate();
        switch_old_state = switch_state;
    }
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Sonoff RavenCore");
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER, "iTEAD");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "Sonoff N/A");
homekit_characteristic_t model = HOMEKIT_CHARACTERISTIC_(MODEL, "Sonoff RavenCore");
homekit_characteristic_t firmware = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, "0.1.0");
homekit_characteristic_t identify_function = HOMEKIT_CHARACTERISTIC_(IDENTIFY, identify);

homekit_characteristic_t switch1_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Switch 1");
homekit_characteristic_t switch2_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Switch 2");
homekit_characteristic_t switch3_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Switch 3");
homekit_characteristic_t switch4_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Switch 4");

homekit_characteristic_t setup_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Setup");
homekit_characteristic_t device_type_name = HOMEKIT_CHARACTERISTIC_(CUSTOM_DEVICE_TYPE_NAME, "Switch Basic");

void create_accessory_name() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    char *name_value = malloc(15);
    snprintf(name_value, 15, "Sonoff %02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    
    name.value = HOMEKIT_STRING(name_value);
    serial.value = name.value;
}

homekit_server_config_t config;

void create_accessory() {
    // Load saved settings
    sysparam_status_t status;
    bool bool_value;
    int8_t int8_value;
    char *char_value;
    
    status = sysparam_get_bool("show_setup", &bool_value);
    if (status == SYSPARAM_OK) {
        show_setup.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool("show_setup", false);
    }

    status = sysparam_get_int8("device_type", &int8_value);
    if (status == SYSPARAM_OK) {
        device_type.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8("device_type", 1);
    }
    
    status = sysparam_get_bool("gpio14_toggle", &bool_value);
    if (status == SYSPARAM_OK) {
        gpio14_toggle.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool("gpio14_toggle", true);
    }
    
    
    uint8_t service_count = 3;
    if (show_setup.value.bool_value) {
        service_count++;
    }
    
    homekit_accessory_t **accessories = calloc(2, sizeof(homekit_accessory_t*));

    homekit_accessory_t *sonoff = accessories[0] = calloc(1, sizeof(homekit_accessory_t));
        sonoff->id = 1;
        sonoff->category = homekit_accessory_category_switch;
        sonoff->config_number = 000100;   // Matches as example: firmware_revision 2.3.7 = 02.03.07 = config_number 020307
        sonoff->services = calloc(service_count, sizeof(homekit_service_t*));

            homekit_service_t *sonoff_info = sonoff->services[0] = calloc(1, sizeof(homekit_service_t));
            sonoff_info->type = HOMEKIT_SERVICE_ACCESSORY_INFORMATION;
            sonoff_info->characteristics = calloc(7, sizeof(homekit_characteristic_t*));
                sonoff_info->characteristics[0] = &name;
                sonoff_info->characteristics[1] = &manufacturer;
                sonoff_info->characteristics[2] = &serial;
                sonoff_info->characteristics[3] = &model;
                sonoff_info->characteristics[4] = &firmware;
                sonoff_info->characteristics[5] = &identify_function;

            /* Device Types
             1. Switch Basic
             2. Switch Dual
             3. Socket + Button
             4. Switch 4ch
             5. Thermostat
             6. Switch Basic + TH Sensor
             */
            if (device_type.value.int_value == 1) {
                char *device_type_name_value = malloc(13);
                snprintf(device_type_name_value, 13, "Basic Switch");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                homekit_service_t *sonoff_switch = sonoff->services[1] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch->primary = true;
                sonoff_switch->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
                sonoff_switch->characteristics[0] = &switch1_service_name;
                sonoff_switch->characteristics[1] = &switch1_on;
                sonoff_switch->characteristics[2] = &show_setup;
            }

            // Setup Accessory
            if (show_setup.value.bool_value) {
                homekit_service_t *sonoff_setup = sonoff->services[2] = calloc(1, sizeof(homekit_service_t));
                sonoff_setup->type = HOMEKIT_SERVICE_CUSTOM_SETUP;
                sonoff_setup->primary = false;
        
                uint8_t setting_count = 4, setting_number = 1;
                
                status = sysparam_get_string("ota_repo", &char_value);
                if (status == SYSPARAM_OK) {
                    setting_count++;
                }
                
                if (gpio14_toggle.value.bool_value) {
                    setting_count++;
                }
        
                sonoff_setup->characteristics = calloc(setting_count, sizeof(homekit_characteristic_t*));
                    sonoff_setup->characteristics[0] = &setup_service_name;
                    if (status == SYSPARAM_OK) {
                        sonoff_setup->characteristics[setting_number] = &ota_firmware;
                        setting_number++;
                    }
                    sonoff_setup->characteristics[setting_number] = &device_type_name;
                    setting_number++;
                    sonoff_setup->characteristics[setting_number] = &device_type;
                    setting_number++;
                    if (gpio14_toggle.value.bool_value) {
                        sonoff_setup->characteristics[setting_number] = &gpio14_toggle;
                    }
            }
    
    
    config.accessories = accessories;
    config.password = "021-82-017";
    
    homekit_server_init(&config);
    
    extra_init();
}

void on_wifi_ready() {
    led_code(LED_GPIO, WIFI_CONNECTED);
    
    create_accessory_name();
    create_accessory();
}

void user_init(void) {
    uart_set_baud(0, 115200);
    
    wifi_config_init("Sonoff", NULL, on_wifi_ready);
    
    gpio_init();
}
