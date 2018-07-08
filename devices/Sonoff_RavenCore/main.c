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

#define LED_GPIO            13
#define SWITCH_GPIO         14

#define BUTTON1_GPIO        0
#define BUTTON2_GPIO        9
#define BUTTON3_GPIO        10
#define BUTTON4_GPIO        14

#define RELAY1_GPIO         12
#define RELAY2_GPIO         5
#define RELAY3_GPIO         4
#define RELAY4_GPIO         15

#define DEBOUNCE_TIME       50      / portTICK_PERIOD_MS
#define DOUBLE_PRESS_TIME   350
#define LONGPRESS_TIME      500     / portTICK_PERIOD_MS
#define OUTLET_TIME         1200    / portTICK_PERIOD_MS
#define RESET_TIME          10000

#define POLL_PERIOD         30000

uint8_t switch_old_state, switch_state, press_count = 0;
uint16_t switch_value = 65535;
uint32_t last_button_event_time, last_reset_event_time;
float old_humidity_value = 0.0, old_temperature_value = 0.0;
static ETSTimer switch_timer, reset_timer, device_restart_timer, change_settings_timer, thermostat_timer, press_timer;

void switch1_on_callback();
void switch2_on_callback();
void switch3_on_callback();
void switch4_on_callback();

void button_intr_callback(uint8_t gpio);
void switch_worker();

void change_settings_callback();
void show_setup_callback();
void ota_firmware_callback();
void gpio14_toggle_callback();

homekit_characteristic_t switch1_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch1_on_callback));
homekit_characteristic_t switch2_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch2_on_callback));
homekit_characteristic_t switch3_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch3_on_callback));
homekit_characteristic_t switch4_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch4_on_callback));

homekit_characteristic_t switch_outlet_in_use = HOMEKIT_CHARACTERISTIC_(OUTLET_IN_USE, false);
homekit_characteristic_t button_event = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);

homekit_characteristic_t show_setup = HOMEKIT_CHARACTERISTIC_(CUSTOM_SHOW_SETUP, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(show_setup_callback));
homekit_characteristic_t device_type = HOMEKIT_CHARACTERISTIC_(CUSTOM_DEVICE_TYPE, 1, .id=102, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t reboot_device = HOMEKIT_CHARACTERISTIC_(CUSTOM_REBOOT_DEVICE, false, .id=103, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(ota_firmware_callback));
homekit_characteristic_t ota_firmware = HOMEKIT_CHARACTERISTIC_(CUSTOM_OTA_UPDATE, false, .id=110, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(ota_firmware_callback));
homekit_characteristic_t gpio14_toggle = HOMEKIT_CHARACTERISTIC_(CUSTOM_GPIO14_TOGGLE, true, .id=111, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(gpio14_toggle_callback));

void relay_write(bool on, int gpio) {
    gpio_write(gpio, on ? 1 : 0);
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

void show_setup_callback() {
    if (show_setup.value.bool_value) {
        sdk_os_timer_setfn(&device_restart_timer, set_restart_countdown, NULL);
        sdk_os_timer_arm(&device_restart_timer, 10000, 0);
    } else {
        sdk_os_timer_disarm(&device_restart_timer);
    }
    
    save_settings();
}

void reset_call() {
    homekit_server_reset();
    wifi_config_reset();

    set_restart_countdown();
}

void ota_firmware_callback() {
    if (ota_firmware.value.bool_value || reboot_device.value.bool_value) {
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
    
    gpio_set_pullup(BUTTON1_GPIO, true, true);
    gpio_set_interrupt(BUTTON1_GPIO, GPIO_INTTYPE_EDGE_ANY, button_intr_callback);
    
    gpio_enable(RELAY1_GPIO, GPIO_OUTPUT);
    relay_write(switch1_on.value.bool_value, RELAY1_GPIO);

    sdk_os_timer_setfn(&switch_timer, switch_worker, NULL);
    sdk_os_timer_setfn(&reset_timer, reset_call, NULL);
    sdk_os_timer_setfn(&change_settings_timer, save_settings, NULL);
    
    last_button_event_time = xTaskGetTickCountFromISR();
}

void switch1_on_callback() {
    led_code(LED_GPIO, FUNCTION_A);
    relay_write(switch1_on.value.bool_value, RELAY1_GPIO);
    
    if (device_type.value.int_value == 3) {
        homekit_characteristic_notify(&switch_outlet_in_use, switch1_on.value);
    }
}

void switch2_on_callback() {
    led_code(LED_GPIO, FUNCTION_A);
    relay_write(switch2_on.value.bool_value, RELAY2_GPIO);
}

void switch3_on_callback() {
    led_code(LED_GPIO, FUNCTION_A);
    relay_write(switch3_on.value.bool_value, RELAY3_GPIO);
}

void switch4_on_callback() {
    led_code(LED_GPIO, FUNCTION_A);
    relay_write(switch4_on.value.bool_value, RELAY4_GPIO);
}

void toggle_switch() {
    switch1_on.value.bool_value = !switch1_on.value.bool_value;
    switch1_on_callback();
    homekit_characteristic_notify(&switch1_on, switch1_on.value);
}

void button_intr_callback(uint8_t gpio) {
    uint32_t now = xTaskGetTickCountFromISR();
    
    if (gpio_read(BUTTON1_GPIO) == 0) {
        sdk_os_timer_arm(&reset_timer, RESET_TIME, 0);
    } else {
        sdk_os_timer_disarm(&reset_timer);
        
        if ((now - last_button_event_time) > DEBOUNCE_TIME) {
            printf(">>> Button pressed\n");
            last_button_event_time = now;
            toggle_switch();
        }
    }
}

void button_dual_intr_callback(uint8_t gpio) {
    uint32_t now = xTaskGetTickCountFromISR();
    
    if (((now - last_button_event_time) > DEBOUNCE_TIME) && (gpio_read(BUTTON3_GPIO) == 1)) {
        last_button_event_time = now;
        
        sdk_os_timer_disarm(&reset_timer);

        press_count++;
        if (press_count > 1) {
            printf(">>> Button: Double press\n");
            press_count = 0;
            sdk_os_timer_disarm(&press_timer);
            led_code(LED_GPIO, FUNCTION_B);
            switch2_on.value.bool_value = !switch2_on.value.bool_value;
            switch2_on_callback();
            homekit_characteristic_notify(&switch2_on, switch2_on.value);
        } else {
            sdk_os_timer_arm(&press_timer, DOUBLE_PRESS_TIME, 0);
        }
    } else {
        sdk_os_timer_arm(&reset_timer, RESET_TIME, 0);
        
        last_reset_event_time = now;
    }
}

void button_dual_timer_callback() {
    // Single press
    printf(">>> Button: Single press\n");
    press_count = 0;
    
    led_code(LED_GPIO, FUNCTION_A);
    
    toggle_switch();
}

void button_complex_intr_callback(uint8_t gpio) {
    uint32_t now = xTaskGetTickCountFromISR();
    
    if (((now - last_button_event_time) > DEBOUNCE_TIME) && (gpio_read(BUTTON1_GPIO) == 1)) {
        last_button_event_time = now;
        
        sdk_os_timer_disarm(&reset_timer);
        
        if ((now - last_reset_event_time) > OUTLET_TIME) {
            printf(">>> Button: Very Long press\n");
            press_count = 0;
            led_code(LED_GPIO, FUNCTION_D);
            switch1_on.value.bool_value = !switch1_on.value.bool_value;
            relay_write(switch1_on.value.bool_value, RELAY1_GPIO);
            homekit_characteristic_notify(&switch1_on, switch1_on.value);
            homekit_characteristic_notify(&switch_outlet_in_use, switch1_on.value);
        } else if ((now - last_reset_event_time) > LONGPRESS_TIME) {
            printf(">>> Button: Long press\n");
            press_count = 0;
            led_code(LED_GPIO, FUNCTION_C);
            homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(2));
        } else {
            press_count++;
            if (press_count > 1) {
                printf(">>> Button: Double press\n");
                press_count = 0;
                sdk_os_timer_disarm(&press_timer);
                led_code(LED_GPIO, FUNCTION_B);
                homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(1));
            } else {
                sdk_os_timer_arm(&press_timer, DOUBLE_PRESS_TIME, 0);
            }
        }
    } else {
        sdk_os_timer_arm(&reset_timer, RESET_TIME, 0);
        
        last_reset_event_time = now;
    }
}

void button_timer_callback() {
    // Single press
    printf(">>> Button: Single press\n");
    press_count = 0;
    
    led_code(LED_GPIO, FUNCTION_A);
    
    homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(0));
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

homekit_characteristic_t button_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Button");

homekit_characteristic_t outlet_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Outlet");

homekit_characteristic_t setup_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Setup", .id=100);
homekit_characteristic_t device_type_name = HOMEKIT_CHARACTERISTIC_(CUSTOM_DEVICE_TYPE_NAME, "Switch Basic", .id=101);

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
    ////// Load saved settings section
    sysparam_status_t status;
    bool bool_value;
    int8_t int8_value;
    char *char_value;
    
    // Load common settings
    status = sysparam_get_bool("show_setup", &bool_value);
    if (status == SYSPARAM_OK) {
        show_setup.value.bool_value = bool_value;
        printf(">>> Loading show_setup -> %i\n", show_setup.value.bool_value);
    } else {
        status = sysparam_set_bool("show_setup", false);
        printf(">>> Setting show_setup to default -> false\n");
    }

    status = sysparam_get_int8("device_type", &int8_value);
    if (status == SYSPARAM_OK) {
        device_type.value.int_value = int8_value;
        //device_type.value.int_value = 1;
        printf(">>> Loading device_type -> %i\n", device_type.value.int_value);
    } else {
        status = sysparam_set_int8("device_type", 1);
        printf(">>> Setting device_type to default -> 1\n");
    }
    
    // Load device type settings
    /* Device Types
     1. Switch Basic
     2. Switch Dual
     3. Button + Socket
     4. Switch 4ch
     5. Thermostat
     6. Switch Basic + TH Sensor
     */
    switch (device_type.value.int_value) {
        case 1:
            printf(">>> Loading device type settings for 1\n");
            status = sysparam_get_bool("gpio14_toggle", &bool_value);
            if (status == SYSPARAM_OK) {
                gpio14_toggle.value.bool_value = bool_value;
            } else {
                status = sysparam_set_bool("gpio14_toggle", true);
            }
            break;
            
        case 5:
        case 6:
            printf(">>> Loading device type settings for 5 and 6\n");
            break;
            
        default:
            break;
    }
    ////// End load saved settings section

    uint8_t service_count = 3, service_number = 2;
    if (show_setup.value.bool_value) {
        service_count++;
    }
    
    if (device_type.value.int_value == 2 || device_type.value.int_value == 3) {
        service_count++;
    } else if (device_type.value.int_value == 4) {
        service_count += 3;
    }
    
    homekit_accessory_t **accessories = calloc(2, sizeof(homekit_accessory_t*));

    homekit_accessory_t *sonoff = accessories[0] = calloc(1, sizeof(homekit_accessory_t));
        sonoff->id = 1;
        sonoff->category = homekit_accessory_category_switch;
        sonoff->config_number = 000100;   // Matches as example: firmware_revision 2.3.7 = 02.03.07 = config_number 020307
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

            /* Device Types
             1. Switch Basic
             2. Switch Dual
             3. Button + Socket
             4. Switch 4ch
             5. Thermostat
             6. Switch Basic + TH Sensor
             */
            if (device_type.value.int_value == 2) {
                printf(">>> Creating accessory for type 2\n");
                
                char *device_type_name_value = malloc(12);
                snprintf(device_type_name_value, 12, "Switch Dual");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                gpio_enable(RELAY2_GPIO, GPIO_OUTPUT);
                relay_write(switch2_on.value.bool_value, RELAY2_GPIO);

                gpio_enable(BUTTON3_GPIO, GPIO_INPUT);
                gpio_set_pullup(BUTTON3_GPIO, true, true);
                gpio_set_interrupt(BUTTON3_GPIO, GPIO_INTTYPE_EDGE_ANY, button_dual_intr_callback);
                
                sdk_os_timer_setfn(&press_timer, button_dual_timer_callback, NULL);
                
                
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
            } else if (device_type.value.int_value == 3) {
                printf(">>> Creating accessory for type 3\n");
                
                char *device_type_name_value = malloc(14);
                snprintf(device_type_name_value, 14, "Button Socket");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                gpio_set_interrupt(BUTTON1_GPIO, GPIO_INTTYPE_EDGE_ANY, button_complex_intr_callback);
                
                sdk_os_timer_setfn(&press_timer, button_timer_callback, NULL);
                
                homekit_service_t *sonoff_button = sonoff->services[1] = calloc(1, sizeof(homekit_service_t));
                sonoff_button->id = 21;
                sonoff_button->type = HOMEKIT_SERVICE_STATELESS_PROGRAMMABLE_SWITCH;
                sonoff_button->primary = true;
                sonoff_button->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
                    sonoff_button->characteristics[0] = &button_service_name;
                    sonoff_button->characteristics[1] = &button_event;
                    sonoff_button->characteristics[2] = &show_setup;
                
                homekit_service_t *sonoff_outlet = sonoff->services[2] = calloc(1, sizeof(homekit_service_t));
                sonoff_outlet->id = 25;
                sonoff_outlet->type = HOMEKIT_SERVICE_OUTLET;
                sonoff_outlet->primary = false;
                sonoff_outlet->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
                    sonoff_outlet->characteristics[0] = &outlet_service_name;
                    sonoff_outlet->characteristics[1] = &switch1_on;
                    sonoff_outlet->characteristics[2] = &switch_outlet_in_use;
                
                service_number = 3;
            } else if (device_type.value.int_value == 4) {
                printf(">>> Creating accessory for type 4\n");
                
                char *device_type_name_value = malloc(11);
                snprintf(device_type_name_value, 11, "Switch 4ch");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                gpio_enable(BUTTON2_GPIO, GPIO_INPUT);
                gpio_set_pullup(BUTTON2_GPIO, true, true);
                gpio_set_interrupt(BUTTON2_GPIO, GPIO_INTTYPE_EDGE_ANY, button_intr_callback);
                
                gpio_enable(BUTTON3_GPIO, GPIO_INPUT);
                gpio_set_pullup(BUTTON3_GPIO, true, true);
                gpio_set_interrupt(BUTTON3_GPIO, GPIO_INTTYPE_EDGE_ANY, button_intr_callback);
                
                gpio_enable(BUTTON4_GPIO, GPIO_INPUT);
                gpio_set_pullup(BUTTON4_GPIO, true, true);
                gpio_set_interrupt(BUTTON4_GPIO, GPIO_INTTYPE_EDGE_ANY, button_intr_callback);
                
                gpio_enable(RELAY2_GPIO, GPIO_OUTPUT);
                relay_write(switch2_on.value.bool_value, RELAY2_GPIO);
                
                gpio_enable(RELAY3_GPIO, GPIO_OUTPUT);
                relay_write(switch3_on.value.bool_value, RELAY3_GPIO);
                
                gpio_enable(RELAY4_GPIO, GPIO_OUTPUT);
                relay_write(switch4_on.value.bool_value, RELAY4_GPIO);

                
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
            } else if (device_type.value.int_value == 5) {
                printf(">>> Creating accessory for type 5\n");
                
                char *device_type_name_value = malloc(11);
                snprintf(device_type_name_value, 11, "Thermostat");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
            } else if (device_type.value.int_value == 6) {
                printf(">>> Creating accessory for type 6\n");
                
                char *device_type_name_value = malloc(17);
                snprintf(device_type_name_value, 17, "Switch TH Sensor");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
            } else { // device_type.value.int_value == 1
                printf(">>> Creating accessory for type 1\n");
                
                char *device_type_name_value = malloc(13);
                snprintf(device_type_name_value, 13, "Switch Basic");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                if (gpio14_toggle.value.bool_value) {
                    gpio_enable(SWITCH_GPIO, GPIO_INPUT);
                    gpio_set_pullup(SWITCH_GPIO, true, true);
                    sdk_os_timer_arm(&switch_timer, 40, 1);
                    
                    switch_evaluate();
                    switch_old_state = switch_state;
                }
                
                
                homekit_service_t *sonoff_switch = sonoff->services[1] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch->id = 8;
                sonoff_switch->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch->primary = true;
                sonoff_switch->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
                    sonoff_switch->characteristics[0] = &switch1_service_name;
                    sonoff_switch->characteristics[1] = &switch1_on;
                    sonoff_switch->characteristics[2] = &show_setup;
            }

            // Setup Accessory
            if (show_setup.value.bool_value) {
                homekit_service_t *sonoff_setup = sonoff->services[service_number] = calloc(1, sizeof(homekit_service_t));
                sonoff_setup->id = 99;
                sonoff_setup->type = HOMEKIT_SERVICE_CUSTOM_SETUP;
                sonoff_setup->primary = false;
        
                uint8_t setting_count = 5, setting_number = 4;
                
                status = sysparam_get_string("ota_repo", &char_value);
                if (status == SYSPARAM_OK) {
                    setting_count++;
                }
                
                if (device_type.value.int_value == 1) {
                    setting_count++;
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
                    if (device_type.value.int_value == 1) {
                        sonoff_setup->characteristics[setting_number] = &gpio14_toggle;
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
    
    wifi_config_init("Sonoff", NULL, on_wifi_ready);
    
    gpio_init();
}
