/*
 * Sonoff S20 Button
 * 
 * v0.2
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
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <etstimer.h>
#include <esplibs/libmain.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_common.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <led_codes.h>

#define BUTTON_GPIO         0
#define LED_GPIO            13
#define RELAY_GPIO          12

#define DEBOUNCE_TIME       50      / portTICK_PERIOD_MS
#define DOUBLE_PRESS_TIME   400
#define LONGPRESS_TIME      750     / portTICK_PERIOD_MS
#define OUTLET_TIME         2500    / portTICK_PERIOD_MS
#define RESET_TIME          10000   / portTICK_PERIOD_MS

uint32_t last_button_event_time, last_reset_event_time;
ETSTimer press_timer;
uint8_t press_count = 0;

void relay_write(bool on) {
    gpio_write(RELAY_GPIO, on ? 1 : 0);
}

void led_write(bool on) {
    gpio_write(LED_GPIO, on ? 0 : 1);
}

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void button_timer_callback();
void button_intr_callback(uint8_t gpio);

homekit_characteristic_t switch_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback));
homekit_characteristic_t switch_outlet_in_use = HOMEKIT_CHARACTERISTIC_(OUTLET_IN_USE, false);
homekit_characteristic_t button_event = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);

void gpio_init() {
    gpio_enable(LED_GPIO, GPIO_OUTPUT);
    led_write(false);
    
    gpio_enable(RELAY_GPIO, GPIO_OUTPUT);
    relay_write(switch_on.value.bool_value);
    
    gpio_set_pullup(BUTTON_GPIO, true, true);
    gpio_set_interrupt(BUTTON_GPIO, GPIO_INTTYPE_EDGE_ANY, button_intr_callback);
    
    last_button_event_time = xTaskGetTickCountFromISR();
    
    sdk_os_timer_disarm(&press_timer);
    sdk_os_timer_setfn(&press_timer, button_timer_callback, NULL);
}

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    relay_write(switch_on.value.bool_value);
    homekit_characteristic_notify(&switch_outlet_in_use, switch_on.value);
}

void functionA_task(void *_args) {
    led_code(LED_GPIO, FUNCTION_A);
    vTaskDelete(NULL);
}

void functionB_task(void *_args) {
    led_code(LED_GPIO, FUNCTION_B);
    vTaskDelete(NULL);
}

void functionC_task(void *_args) {
    led_code(LED_GPIO, FUNCTION_C);
    vTaskDelete(NULL);
}

void functionD_task(void *_args) {
    led_code(LED_GPIO, FUNCTION_D);
    vTaskDelete(NULL);
}

void identify_task(void *_args) {
    led_code(LED_GPIO, IDENTIFY_ACCESSORY);
    vTaskDelete(NULL);
}

void wifi_connected_task(void *_args) {
    led_code(LED_GPIO, WIFI_CONNECTED);
    vTaskDelete(NULL);
}

void reset_task(void *_args) {
    homekit_server_reset();
    wifi_config_reset();
    
    led_code(LED_GPIO, RESTART_DEVICE);
    
    sdk_system_restart();
    vTaskDelete(NULL);
}

void button_intr_callback(uint8_t gpio) {
    uint32_t now = xTaskGetTickCountFromISR();
    
    if (((now - last_button_event_time) > DEBOUNCE_TIME) && (gpio_read(BUTTON_GPIO) == 1)) {
        last_button_event_time = now;
        
        if ((now - last_reset_event_time) > RESET_TIME) {
            xTaskCreate(reset_task, "Reset", 256, NULL, 1, NULL);
        } else if ((now - last_reset_event_time) > OUTLET_TIME) {
            press_count = 0;
            xTaskCreate(functionD_task, "Function D", 128, NULL, 3, NULL);
            switch_on.value.bool_value = !switch_on.value.bool_value;
            relay_write(switch_on.value.bool_value);
            homekit_characteristic_notify(&switch_on, switch_on.value);
            homekit_characteristic_notify(&switch_outlet_in_use, switch_on.value);
        } else if ((now - last_reset_event_time) > LONGPRESS_TIME) {
            press_count = 0;
            xTaskCreate(functionC_task, "Function C", 128, NULL, 3, NULL);
            homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(2));
        } else {
            press_count++;
            if (press_count > 1) {
                press_count = 0;
                sdk_os_timer_disarm(&press_timer);
                xTaskCreate(functionB_task, "Function B", 128, NULL, 3, NULL);
                homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(1));
            } else {
                sdk_os_timer_arm(&press_timer, DOUBLE_PRESS_TIME, 1);
            }
        }
    } else {
        last_reset_event_time = now;
    }
}

void button_timer_callback() {
    press_count = 0;
    sdk_os_timer_disarm(&press_timer);
    
    xTaskCreate(functionA_task, "Function A", 128, NULL, 3, NULL);
    homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(0));
}

void identify(homekit_value_t _value) {
    xTaskCreate(identify_task, "Identify", 128, NULL, 3, NULL);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Sonoff S20 Button");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "SonoffS20 N/A");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_programmable_switch, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "iTEAD"),
            &serial,
            HOMEKIT_CHARACTERISTIC(MODEL, "Sonoff S20 Button"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.2"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Sonoff Button"),
            &button_event,
            NULL
        }),
        HOMEKIT_SERVICE(OUTLET, .primary=false, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Sonoff S20"),
            &switch_on,
            &switch_outlet_in_use,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "021-82-017"
};

void create_accessory_name() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    char *name_value = malloc(17);
    snprintf(name_value, 17, "SonoffS20 %02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    
    name.value = HOMEKIT_STRING(name_value);
    serial.value = name.value;
}

void on_wifi_ready() {
    xTaskCreate(wifi_connected_task, "Wifi connected", 256, NULL, 3, NULL);
    
    create_accessory_name();
        
    homekit_server_init(&config);
}

void user_init(void) {
    uart_set_baud(0, 115200);
    
    wifi_config_init("SonoffS20", NULL, on_wifi_ready);
    
    gpio_init();
}
