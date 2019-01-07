/*
 * KellyTron
 *
 * v0.0.3
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

#include "../common/custom_characteristics.h"

#define TOGGLE_GPIO                 5
#define RELAY_GPIO                  4
#define DEBOUNCE_TIME               40
#define DISABLE_TIME                80
#define ALLOWED_FACTORY_RESET_TIME  120000

bool gpio_state;
uint8_t reset_toggle_counter = 0;
volatile uint32_t disable_time;
ETSTimer device_restart_timer, change_settings_timer, save_states_timer, toggle_switch_timer, factory_default_timer;

void switch1_on_callback(homekit_value_t value);
homekit_value_t read_switch1_on_callback();

void change_settings_callback();
void show_setup_callback();
void ota_firmware_callback();

void on_wifi_ready();

homekit_characteristic_t switch1_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter=read_switch1_on_callback, .setter=switch1_on_callback);
homekit_characteristic_t show_setup = HOMEKIT_CHARACTERISTIC_(CUSTOM_SHOW_SETUP, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(show_setup_callback));
homekit_characteristic_t reboot_device = HOMEKIT_CHARACTERISTIC_(CUSTOM_REBOOT_DEVICE, false, .id=103, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(ota_firmware_callback));
homekit_characteristic_t ota_firmware = HOMEKIT_CHARACTERISTIC_(CUSTOM_OTA_UPDATE, false, .id=110, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(ota_firmware_callback));
homekit_characteristic_t custom_init_state_sw1 = HOMEKIT_CHARACTERISTIC_(CUSTOM_INIT_STATE_SW1, 0, .id=120, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

// ---------------------------

void relay_write(bool on, int gpio) {
    disable_time = xTaskGetTickCountFromISR();
    gpio_write(gpio, on ? 1 : 0);
}

void identify_task() {
    relay_write(true, RELAY_GPIO);
    for (uint8_t i = 0; i < 3; i++) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        relay_write(true, RELAY_GPIO);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        relay_write(false, RELAY_GPIO);
    }
    
    vTaskDelete(NULL);
}

void save_settings() {
    printf("KT >>> Saving settings\n");
    
    sysparam_status_t status;
    bool bool_value;
    int8_t int8_value;
    
    status = sysparam_get_bool("show_setup", &bool_value);
    if (status == SYSPARAM_OK && bool_value != show_setup.value.bool_value) {
        status = sysparam_set_bool("show_setup", show_setup.value.bool_value);
    }
    
    status = sysparam_get_int8("init_state_sw1", &int8_value);
    if (status == SYSPARAM_OK && int8_value != custom_init_state_sw1.value.int_value) {
        status = sysparam_set_int8("init_state_sw1", custom_init_state_sw1.value.int_value);
    }
}

void device_restart_task() {
    vTaskDelay(5500 / portTICK_PERIOD_MS);
    
    if (ota_firmware.value.bool_value) {
        rboot_set_temp_rom(1);
    }
    
    sdk_system_restart();
    
    vTaskDelete(NULL);
}

void device_restart() {
    printf("KT >>> Restarting device\n");
    xTaskCreate(device_restart_task, "device_restart_task", 256, NULL, 1, NULL);
}

void reset_call_task() {
    printf("KT >>> Resetting device to factory defaults\n");
    sysparam_status_t status;
    
    vTaskDelay(3100 / portTICK_PERIOD_MS);
    
    status = sysparam_set_bool("show_setup", false);
    status = sysparam_set_int8("init_state_sw1", 0);
    
    if (status != SYSPARAM_OK) {
        printf("KT !!! ERROR Flash problem\n");
    }
    
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    homekit_server_reset();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    wifi_config_reset();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    sdk_system_restart();
    
    vTaskDelete(NULL);
}

void save_states() {
    printf("KT >>> Saving last states\n");
    
    sysparam_status_t status;
    bool bool_value;
    
    if (custom_init_state_sw1.value.int_value > 1) {
        status = sysparam_get_bool("last_state_sw1", &bool_value);
        if (status == SYSPARAM_OK && bool_value != switch1_on.value.bool_value) {
            status = sysparam_set_bool("last_state_sw1", switch1_on.value.bool_value);
        }
    }
}

void factory_default() {
    printf("KT >>> Checking factory default call\n");
    
    if (reset_toggle_counter > 10) {
        if (xTaskGetTickCountFromISR() < ALLOWED_FACTORY_RESET_TIME / portTICK_PERIOD_MS) {
            gpio_disable(TOGGLE_GPIO);
            xTaskCreate(identify_task, "identify_task", 256, NULL, 1, NULL);
            xTaskCreate(reset_call_task, "reset_call_task", 256, NULL, 1, NULL);
        } else {
            printf("KT !!! Reset to factory defaults not allowed after %i msecs since boot. Repower device and try again\n", ALLOWED_FACTORY_RESET_TIME);
        }
    }
    
    reset_toggle_counter = 0;
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
    printf("KT >>> Toggle Switch\n");
    switch1_on.value = value;
    relay_write(switch1_on.value.bool_value, RELAY_GPIO);
    printf("KT >>> Relay -> %i\n", switch1_on.value.bool_value);
    save_states_callback();
}

homekit_value_t read_switch1_on_callback() {
    return switch1_on.value;
}

IRAM void toggle_switch_intr_callback(const uint8_t gpio) {
    gpio_state = gpio_read(TOGGLE_GPIO);
    sdk_os_timer_arm(&toggle_switch_timer, DEBOUNCE_TIME, 0);
}

IRAM void toggle_switch_callback() {
    sdk_os_timer_disarm(&toggle_switch_timer);
    
    const uint32_t now = xTaskGetTickCountFromISR();
    
    if (now - disable_time > DISABLE_TIME / portTICK_PERIOD_MS) {
        if (gpio_read(TOGGLE_GPIO) == gpio_state) {
            printf("KT >>> Toggle Switch manual\n");
            switch1_on.value.bool_value = !switch1_on.value.bool_value;
            relay_write(switch1_on.value.bool_value, RELAY_GPIO);
            printf("KT >>> Relay -> %i\n", switch1_on.value.bool_value);
            homekit_characteristic_notify(&switch1_on, switch1_on.value);
            save_states_callback();
            
            reset_toggle_counter++;
            sdk_os_timer_arm(&factory_default_timer, 3200, 0);
        }
    }
}

void identify(homekit_value_t _value) {
    printf("KT >>> Identify\n");
    xTaskCreate(identify_task, "identify_task", 256, NULL, 1, NULL);
}

void hardware_init() {
    sysparam_status_t status;
    bool bool_value;
    
    status = sysparam_get_bool("show_setup", &bool_value);
    if (status == SYSPARAM_OK) {
        show_setup.value.bool_value = bool_value;
        printf("KT >>> Loading show_setup -> %i\n", show_setup.value.bool_value);
    } else {
        sysparam_set_bool("show_setup", false);
        printf("KT >>> Setting show_setup to default -> false\n");
    }
    
    printf("KT >>> Loading device gpio settings\n");
    
    gpio_disable(0);
    
    gpio_enable(RELAY_GPIO, GPIO_OUTPUT);
    relay_write(switch1_on.value.bool_value, RELAY_GPIO);
    
    gpio_enable(TOGGLE_GPIO, GPIO_INPUT);
    gpio_set_pullup(TOGGLE_GPIO, false, false);
    gpio_set_interrupt(TOGGLE_GPIO, GPIO_INTTYPE_EDGE_ANY, toggle_switch_intr_callback);
    
    sdk_os_timer_setfn(&toggle_switch_timer, toggle_switch_callback, NULL);
    sdk_os_timer_setfn(&change_settings_timer, save_settings, NULL);
    sdk_os_timer_setfn(&save_states_timer, save_states, NULL);
    sdk_os_timer_setfn(&factory_default_timer, factory_default, NULL);
    
    wifi_config_init("KellyTron", NULL, on_wifi_ready);
}

void settings_init() {
    // Initial Setup
    sysparam_status_t status;
    bool bool_value;
    int8_t int8_value;
    
    // Load Saved Settings and set factory values for missing settings
    printf("KT >>> Loading settings\n");
    
    sysparam_set_string("ota_repo", "RavenSystem/ravencore");
    status = sysparam_set_int8("device_type", 12);
    
    status = sysparam_get_bool("show_setup", &bool_value);
    if (status == SYSPARAM_OK) {
        show_setup.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool("show_setup", false);
    }
    
    status = sysparam_get_int8("init_state_sw1", &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw1.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8("init_state_sw1", 0);
    }
    
    // Load Saved States
    printf("KT >>> Loading saved states\n");
    
    if (custom_init_state_sw1.value.int_value > 1) {
        status = sysparam_get_bool("last_state_sw1", &bool_value);
        if (status == SYSPARAM_OK) {
            if (custom_init_state_sw1.value.int_value == 2) {
                switch1_on.value.bool_value = bool_value;
            } else {
                switch1_on.value.bool_value = !bool_value;
            }
        } else {
            status = sysparam_set_bool("last_state_sw1", false);
        }
    } else if (custom_init_state_sw1.value.int_value == 1) {
        switch1_on.value.bool_value = true;
    }
    
    if (status == SYSPARAM_OK) {
        hardware_init();
    } else {
        printf("KT !!! ERROR in settings_init loading settings. Flash problem\n");
    }
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, NULL);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER, "RavenSystem");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, NULL);
homekit_characteristic_t model = HOMEKIT_CHARACTERISTIC_(MODEL, "KellyTron");
homekit_characteristic_t identify_function = HOMEKIT_CHARACTERISTIC_(IDENTIFY, identify);

homekit_characteristic_t switch1_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Switch");

homekit_characteristic_t setup_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Setup", .id=100);
homekit_characteristic_t device_type_name = HOMEKIT_CHARACTERISTIC_(CUSTOM_DEVICE_TYPE_NAME, "Switch", .id=101);

homekit_characteristic_t firmware = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, "0.0.3");

homekit_accessory_category_t accessory_category = homekit_accessory_category_switch;

void create_accessory_name() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    char *name_value = malloc(17);
    snprintf(name_value, 17, "KellyTron %02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    name.value = HOMEKIT_STRING(name_value);
    
    char *serial_value = malloc(13);
    snprintf(serial_value, 13, "%02X%02X%02X%02X%02X%02X", macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);
    serial.value = HOMEKIT_STRING(serial_value);
}

homekit_server_config_t config;

void create_accessory() {
    uint8_t service_count = 3;
    
    // Total service count
    if (show_setup.value.bool_value) {
        service_count++;
    }
    
    homekit_accessory_t **accessories = calloc(2, sizeof(homekit_accessory_t*));
    
    homekit_accessory_t *sonoff = accessories[0] = calloc(1, sizeof(homekit_accessory_t));
    sonoff->id = 1;
    sonoff->category = accessory_category;
    sonoff->config_number = 000003;   // Matches as example: firmware_revision 2.3.8 = 02.03.10 (octal) = config_number 020310
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
    
    printf("KT >>> Creating accessory\n");

    homekit_service_t *sonoff_switch1 = sonoff->services[1] = calloc(1, sizeof(homekit_service_t));
    sonoff_switch1->id = 8;
    sonoff_switch1->type = HOMEKIT_SERVICE_SWITCH;
    sonoff_switch1->primary = true;
    sonoff_switch1->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
    sonoff_switch1->characteristics[0] = &switch1_service_name;
    sonoff_switch1->characteristics[1] = &switch1_on;
    sonoff_switch1->characteristics[2] = &show_setup;
    
    // Setup Accessory, visible only in 3party Apps
    if (show_setup.value.bool_value) {
        homekit_service_t *sonoff_setup = sonoff->services[2] = calloc(1, sizeof(homekit_service_t));
        sonoff_setup->id = 99;
        sonoff_setup->type = HOMEKIT_SERVICE_CUSTOM_SETUP;
        sonoff_setup->primary = false;
        
        uint8_t setting_count = 5;
        
        sysparam_status_t status;
        char *char_value;
        
        status = sysparam_get_string("ota_repo", &char_value);
        if (status == SYSPARAM_OK) {
            setting_count++;
        }
        
        sonoff_setup->characteristics = calloc(setting_count, sizeof(homekit_characteristic_t*));
        sonoff_setup->characteristics[0] = &setup_service_name;
        sonoff_setup->characteristics[1] = &device_type_name;
        sonoff_setup->characteristics[2] = &reboot_device;
        sonoff_setup->characteristics[3] = &custom_init_state_sw1;
        if (status == SYSPARAM_OK) {
            sonoff_setup->characteristics[4] = &ota_firmware;
        }
    }
    
    config.accessories = accessories;
    config.password = "021-82-017";
    
    homekit_server_init(&config);
}

void on_wifi_ready() {
    create_accessory_name();
    create_accessory();
}

void user_init(void) {
    uart_set_baud(0, 115200);
    
    printf("KT >>> KellyTron firmware loaded\n");
    printf("KT >>> Developed by RavenSystem - José Antonio Jiménez\n");
    printf("KT >>> Firmware revision: %s\n\n", firmware.value.string_value);
    
    settings_init();
}
