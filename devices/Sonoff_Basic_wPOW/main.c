/*
 * Sonoff Basic with Power Outage Warning
 * 
 * v0.2.1
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
#include <espressif/esp_wifi.h>
#include <espressif/esp_common.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <led_codes.h>

#define BUTTON_GPIO         0
#define LED_GPIO            13
#define RELAY_GPIO          12
#define SWITCH_GPIO         14

#define DEBOUNCE_TIME       300     / portTICK_PERIOD_MS
#define RESET_TIME          10000   / portTICK_PERIOD_MS

#define delay_ms(ms)        vTaskDelay((ms) / portTICK_PERIOD_MS)

#define POW_DELAY           15000
#define POW_WORKING_TIME    600000

uint32_t last_button_event_time, last_reset_event_time;

void relay_write(bool on) {
    gpio_write(RELAY_GPIO, on ? 1 : 0);
}

void led_write(bool on) {
    gpio_write(LED_GPIO, on ? 0 : 1);
}

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);

void button_intr_callback(uint8_t gpio);
void switch_intr_callback(uint8_t gpio);

homekit_characteristic_t switch_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback));

homekit_characteristic_t power_cut_alarm = HOMEKIT_CHARACTERISTIC_(MOTION_DETECTED, false);

void power_outage_warning_task(void *_args) {
    delay_ms(POW_DELAY);

    printf(">>> Power Outage Warning: ON event sent\n");
    power_cut_alarm.value = HOMEKIT_BOOL(true);
    homekit_characteristic_notify(&power_cut_alarm, HOMEKIT_BOOL(true));
    homekit_characteristic_set_delayed_notify1(&power_cut_alarm, HOMEKIT_BOOL(true));
    homekit_characteristic_set_delayed_notify2(&power_cut_alarm, HOMEKIT_BOOL(false));
    
     delay_ms(2000);
    
    printf(">>> Power Outage Warning: OFF event sent\n");
    power_cut_alarm.value = HOMEKIT_BOOL(false);
    homekit_characteristic_notify(&power_cut_alarm, HOMEKIT_BOOL(false));
    
    delay_ms(POW_WORKING_TIME);
    
    homekit_characteristic_remove_delayed_notify1();
    homekit_characteristic_remove_delayed_notify2();
    
    vTaskDelete(NULL);
}

void gpio_init() {
    gpio_enable(LED_GPIO, GPIO_OUTPUT);
    led_write(false);
    
    gpio_enable(RELAY_GPIO, GPIO_OUTPUT);
    relay_write(switch_on.value.bool_value);
    
    gpio_set_pullup(BUTTON_GPIO, true, true);
    gpio_set_interrupt(BUTTON_GPIO, GPIO_INTTYPE_EDGE_ANY, button_intr_callback);
    
    gpio_enable(SWITCH_GPIO, GPIO_INPUT);
    gpio_set_pullup(SWITCH_GPIO, true, true);
    gpio_set_interrupt(SWITCH_GPIO, GPIO_INTTYPE_EDGE_ANY, switch_intr_callback);
    
    last_button_event_time = xTaskGetTickCountFromISR();
}

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    relay_write(switch_on.value.bool_value);
}

void function_task(void *_args) {
    led_code(LED_GPIO, FUNCTION_A);
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

void toggle_switch() {
    xTaskCreate(function_task, "Function", 128, NULL, 3, NULL);
    switch_on.value.bool_value = !switch_on.value.bool_value;
    relay_write(switch_on.value.bool_value);
    homekit_characteristic_notify(&switch_on, switch_on.value);
}

void switch_intr_callback(uint8_t gpio) {
    uint32_t now = xTaskGetTickCountFromISR();
    
    if ((now - last_button_event_time) > DEBOUNCE_TIME) {
        last_button_event_time = now;
        toggle_switch();
    }
}

void button_intr_callback(uint8_t gpio) {
    uint32_t now = xTaskGetTickCountFromISR();
    
    if (((now - last_button_event_time) > DEBOUNCE_TIME) && (gpio_read(BUTTON_GPIO) == 1)) {
        if ((now - last_reset_event_time) > RESET_TIME) {
            xTaskCreate(reset_task, "Reset", 256, NULL, 1, NULL);
        } else {
            last_button_event_time = now;
            toggle_switch();
        }
    } else if (gpio_read(BUTTON_GPIO) == 0) {
        last_reset_event_time = now;
    }
}

void identify(homekit_value_t _value) {
    xTaskCreate(identify_task, "Identify", 128, NULL, 3, NULL);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Sonoff Switch");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "SonoffB N/A");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "iTEAD"),
            &serial,
            HOMEKIT_CHARACTERISTIC(MODEL, "Sonoff Basic wPOW"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.2.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Sonoff Switch"),
            &switch_on,
            NULL
        }),
        HOMEKIT_SERVICE(MOTION_SENSOR, .primary=false, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Power Outage"),
            &power_cut_alarm,
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
    
    char *name_value = malloc(15);
    snprintf(name_value, 15, "SonoffB %02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    
    name.value = HOMEKIT_STRING(name_value);
    serial.value = name.value;
}

void on_wifi_ready() {
    xTaskCreate(wifi_connected_task, "Wifi connected", 256, NULL, 3, NULL);
    
    create_accessory_name();
        
    homekit_server_init(&config);
    
    xTaskCreate(power_outage_warning_task, "Power Outage Warning", 256, NULL, 4, NULL);
}

void user_init(void) {
    uart_set_baud(0, 115200);
    
    wifi_config_init("SonoffB", NULL, on_wifi_ready);
    
    gpio_init();
}
