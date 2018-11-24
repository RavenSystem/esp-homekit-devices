/*
 * KellyTron
 *
 * v0.0.1
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
#include <task.h>

#include <etstimer.h>
#include <esplibs/libmain.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>

#include "../common/custom_characteristics.h"

#define TOGGLE1_GPIO        5

#define RELAY1_GPIO         5

#define DISABLED_TIME       50

static volatile uint32_t last_press_time;
static ETSTimer device_restart_timer, change_settings_timer, extra_func_timer;

void switch1_on_callback(homekit_value_t value);
homekit_value_t read_switch1_on_callback();
void switch2_on_callback(homekit_value_t value);
homekit_value_t read_switch2_on_callback();
void switch3_on_callback(homekit_value_t value);
homekit_value_t read_switch3_on_callback();
void switch4_on_callback(homekit_value_t value);
homekit_value_t read_switch4_on_callback();

void switch_worker();

void change_settings_callback();
void show_setup_callback();
void ota_firmware_callback();

void on_wifi_ready();

homekit_characteristic_t switch1_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter=read_switch1_on_callback, .setter=switch1_on_callback);

homekit_characteristic_t show_setup = HOMEKIT_CHARACTERISTIC_(CUSTOM_SHOW_SETUP, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(show_setup_callback));

homekit_characteristic_t reboot_device = HOMEKIT_CHARACTERISTIC_(CUSTOM_REBOOT_DEVICE, false, .id=103, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(ota_firmware_callback));
homekit_characteristic_t ota_firmware = HOMEKIT_CHARACTERISTIC_(CUSTOM_OTA_UPDATE, false, .id=110, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(ota_firmware_callback));

void relay_write(bool on, int gpio) {
    last_press_time = xTaskGetTickCountFromISR() + (DISABLED_TIME / portTICK_PERIOD_MS);
    gpio_write(gpio, on ? 1 : 0);
}


void save_settings() {
    sysparam_status_t status;
    bool bool_value;
    
    status = sysparam_get_bool("show_setup", &bool_value);
    if (status == SYSPARAM_OK && bool_value != show_setup.value.bool_value) {
        sysparam_set_bool("show_setup", show_setup.value.bool_value);
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
    
    homekit_server_reset();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    wifi_config_reset();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    sdk_system_restart();
    
    vTaskDelete(NULL);
}

void reset_call(const uint8_t gpio) {
    relay_write(false, RELAY1_GPIO);
    gpio_disable(RELAY1_GPIO);
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

void toggle_switch(const uint8_t gpio) {
    printf(">>> Toggle Switch manual\n");
    switch1_on.value.bool_value = !switch1_on.value.bool_value;
    relay_write(switch1_on.value.bool_value, relay1_gpio);
    printf(">>> Relay 1 -> %i\n", switch1_on.value.bool_value);
    homekit_characteristic_notify(&switch1_on, switch1_on.value);
}

void button_simple1_intr_callback(const uint8_t gpio) {
    if (device_type_static == 7) {
        toggle_valve();
    } else {
        toggle_switch(gpio);
    }
}

void identify(homekit_value_t _value) {
    printf(">>> Identify\n");
}

void gpio_init() {
    sysparam_status_t status;
    bool bool_value;
    
    status = sysparam_get_bool("show_setup", &bool_value);
    if (status == SYSPARAM_OK) {
        show_setup.value.bool_value = bool_value;
        printf(">>> Loading show_setup -> %i\n", show_setup.value.bool_value);
    } else {
        sysparam_set_bool("show_setup", false);
        printf(">>> Setting show_setup to default -> false\n");
    }
    
    printf(">>> Loading device gpio settings\n");
    
    
    
    gpio_enable(relay1_gpio, GPIO_OUTPUT);
    relay_write(switch1_on.value.bool_value, relay1_gpio);
    
    sdk_os_timer_setfn(&change_settings_timer, save_settings, NULL);
    
    wifi_config_init("KellyTron", NULL, on_wifi_ready);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, NULL);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER, "RavenSystem");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, NULL);
homekit_characteristic_t model = HOMEKIT_CHARACTERISTIC_(MODEL, "KellyTron");
homekit_characteristic_t identify_function = HOMEKIT_CHARACTERISTIC_(IDENTIFY, identify);

homekit_characteristic_t switch1_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Switch 1");

homekit_characteristic_t setup_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Setup", .id=100);
homekit_characteristic_t device_type_name = HOMEKIT_CHARACTERISTIC_(CUSTOM_DEVICE_TYPE_NAME, "Switch 1", .id=101);

homekit_characteristic_t firmware = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, "0.0.1");

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
    sonoff->config_number = 000001;   // Matches as example: firmware_revision 2.3.8 = 02.03.10 (octal) = config_number 020310
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
    
    printf(">>> Creating accessory\n");
    
    char *device_type_name_value = malloc(9);
    snprintf(device_type_name_value, 9, "Switch 1");
    device_type_name.value = HOMEKIT_STRING(device_type_name_value);
    
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
    create_accessory_name();
    create_accessory();
}

void user_init(void) {
    uart_set_baud(0, 115200);
    
    printf(">>> KellyTron firmware loaded\n");
    printf(">>> Developed by RavenSystem - José Antonio Jiménez\n");
    printf(">>> Firmware revision: %s\n\n", firmware.value.string_value);
    
    gpio_init();
}

