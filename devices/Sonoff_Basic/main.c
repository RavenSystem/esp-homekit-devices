/*
 * Sonoff Basic
 * 
 * v0.5
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
#include <espressif/esp_wifi.h>
#include <espressif/esp_common.h>

#include <etstimer.h>
#include <esplibs/libmain.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <led_codes.h>

#define BUTTON_GPIO         0
#define LED_GPIO            13
#define RELAY_GPIO          12
#define SWITCH_GPIO         14

#define DEBOUNCE_TIME       500     / portTICK_PERIOD_MS
#define RESET_TIME          10000

uint32_t last_button_event_time;
ETSTimer switch_timer, reset_timer, device_restart_timer;

uint8_t switch_old_state = 0, switch_state = 0;
uint16_t switch_value = 0;

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void button_intr_callback(uint8_t gpio);
void switch_worker();

homekit_characteristic_t switch_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback));

void relay_write(bool on) {
    gpio_write(RELAY_GPIO, on ? 1 : 0);
}

void led_write(bool on) {
    gpio_write(LED_GPIO, on ? 0 : 1);
}

void device_restart() {
    sdk_system_restart();
}

void reset_call() {
    homekit_server_reset();
    wifi_config_reset();
    
    led_code(LED_GPIO, RESTART_DEVICE);
    
    sdk_os_timer_setfn(&device_restart_timer, device_restart, NULL);
    sdk_os_timer_arm(&device_restart_timer, 5000, 0);
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
    sdk_os_timer_setfn(&switch_timer, switch_worker, NULL);
    sdk_os_timer_arm(&switch_timer, 40, 1);
    
    sdk_os_timer_setfn(&reset_timer, reset_call, NULL);
    
    last_button_event_time = xTaskGetTickCountFromISR();
}

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    led_code(LED_GPIO, FUNCTION_A);
    relay_write(switch_on.value.bool_value);
}

void toggle_switch() {
    led_code(LED_GPIO, FUNCTION_A);
    switch_on.value.bool_value = !switch_on.value.bool_value;
    relay_write(switch_on.value.bool_value);
    homekit_characteristic_notify(&switch_on, switch_on.value);
}

void button_intr_callback(uint8_t gpio) {
    uint32_t now = xTaskGetTickCountFromISR();
    
    if (((now - last_button_event_time) > DEBOUNCE_TIME) && (gpio_read(BUTTON_GPIO) == 1)) {
        sdk_os_timer_disarm(&reset_timer);
        last_button_event_time = now;
        toggle_switch();
    } else if (gpio_read(BUTTON_GPIO) == 0) {
        sdk_os_timer_arm(&reset_timer, RESET_TIME, 0);
    }
}

#define maxvalue_unsigned(x) ((1 << (8 * sizeof(x))) - 1)
void switch_worker() {      // Based on https://github.com/pcsaito/esp-homekit-demo/tree/LPFToggle
    switch_value += ((gpio_read(SWITCH_GPIO) * maxvalue_unsigned(switch_value)) - switch_value) >> 3;
    switch_state = (switch_value > (maxvalue_unsigned(switch_value) >> 1));
    
    if (switch_state != switch_old_state) {
        switch_old_state = switch_state;
        toggle_switch();
    }
}

void identify(homekit_value_t _value) {
    led_code(LED_GPIO, IDENTIFY_ACCESSORY);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Sonoff Switch");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "Sonoff N/A");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "iTEAD"),
            &serial,
            HOMEKIT_CHARACTERISTIC(MODEL, "Sonoff Basic"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.5"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Switch"),
            &switch_on,
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
    
    char *name_value = malloc(14);
    snprintf(name_value, 14, "Sonoff %02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    
    name.value = HOMEKIT_STRING(name_value);
    serial.value = name.value;
}

void on_wifi_ready() {
    led_code(LED_GPIO, WIFI_CONNECTED);
    
    create_accessory_name();
        
    homekit_server_init(&config);
}

void user_init(void) {
    uart_set_baud(0, 115200);
    
    wifi_config_init("Sonoff", NULL, on_wifi_ready);
    
    gpio_init();
}
