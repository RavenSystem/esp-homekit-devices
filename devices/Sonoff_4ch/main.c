/* 
 * Sonoff 4CH
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
#include <espressif/esp_wifi.h>
#include <espressif/esp_common.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <led_codes.h>

#define BUTTON1_GPIO        0
#define BUTTON2_GPIO        9
#define BUTTON3_GPIO        10
#define BUTTON4_GPIO        14
#define LED_GPIO            13
#define RELAY1_GPIO         12
#define RELAY2_GPIO         5
#define RELAY3_GPIO         4
#define RELAY4_GPIO         15

#define DEBOUNCE_TIME       300     / portTICK_PERIOD_MS
#define RESET_TIME          10000   / portTICK_PERIOD_MS

uint32_t last_button_event_time, last_reset_event_time;
uint8_t pushed_gpio;

void relay_write(bool on, int gpio) {
    gpio_write(gpio, on ? 1 : 0);
}

void led_write(bool on) {
    gpio_write(LED_GPIO, on ? 0 : 1);
}

void switch_on_callback1(homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void switch_on_callback2(homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void switch_on_callback3(homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void switch_on_callback4(homekit_characteristic_t *_ch, homekit_value_t on, void *context);

void button_intr_callback(uint8_t gpio);

homekit_characteristic_t switch1_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback1));
homekit_characteristic_t switch2_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback2));
homekit_characteristic_t switch3_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback3));
homekit_characteristic_t switch4_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback4));

void gpio_init() {
    gpio_enable(LED_GPIO, GPIO_OUTPUT);
    led_write(false);
    
    gpio_enable(RELAY1_GPIO, GPIO_OUTPUT);
    relay_write(switch1_on.value.bool_value, RELAY1_GPIO);
    
    gpio_enable(RELAY2_GPIO, GPIO_OUTPUT);
    relay_write(switch2_on.value.bool_value, RELAY2_GPIO);
    
    gpio_enable(RELAY3_GPIO, GPIO_OUTPUT);
    relay_write(switch3_on.value.bool_value, RELAY3_GPIO);
    
    gpio_enable(RELAY4_GPIO, GPIO_OUTPUT);
    relay_write(switch4_on.value.bool_value, RELAY4_GPIO);
    
    gpio_set_pullup(BUTTON1_GPIO, true, true);
    gpio_set_interrupt(BUTTON1_GPIO, GPIO_INTTYPE_EDGE_ANY, button_intr_callback);
    
    gpio_set_pullup(BUTTON2_GPIO, true, true);
    gpio_set_interrupt(BUTTON2_GPIO, GPIO_INTTYPE_EDGE_ANY, button_intr_callback);
    
    gpio_set_pullup(BUTTON3_GPIO, true, true);
    gpio_set_interrupt(BUTTON3_GPIO, GPIO_INTTYPE_EDGE_ANY, button_intr_callback);
    
    gpio_set_pullup(BUTTON4_GPIO, true, true);
    gpio_set_interrupt(BUTTON4_GPIO, GPIO_INTTYPE_EDGE_ANY, button_intr_callback);
    
    last_button_event_time = xTaskGetTickCountFromISR();
}

void switch_on_callback1(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    relay_write(switch1_on.value.bool_value, RELAY1_GPIO);
}

void switch_on_callback2(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    relay_write(switch2_on.value.bool_value, RELAY2_GPIO);
}

void switch_on_callback3(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    relay_write(switch3_on.value.bool_value, RELAY3_GPIO);
}

void switch_on_callback4(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    relay_write(switch4_on.value.bool_value, RELAY4_GPIO);
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

void button_intr_callback(uint8_t gpio) {
    uint32_t now = xTaskGetTickCountFromISR();
    
    if (((now - last_button_event_time) > DEBOUNCE_TIME) && (gpio_read(gpio) == 1) && (pushed_gpio == gpio)) {
        if ((now - last_reset_event_time) > RESET_TIME && (gpio == BUTTON1_GPIO)) {
            xTaskCreate(reset_task, "Reset", 256, NULL, 1, NULL);
        } else {
            last_button_event_time = now;
            xTaskCreate(function_task, "Function", 256, NULL, 3, NULL);
            switch (gpio) {
                case BUTTON1_GPIO:
                    switch1_on.value.bool_value = !switch1_on.value.bool_value;
                    relay_write(switch1_on.value.bool_value, RELAY1_GPIO);
                    homekit_characteristic_notify(&switch1_on, switch1_on.value);
                    break;
                    
                case BUTTON2_GPIO:
                    switch2_on.value.bool_value = !switch2_on.value.bool_value;
                    relay_write(switch2_on.value.bool_value, RELAY2_GPIO);
                    homekit_characteristic_notify(&switch2_on, switch2_on.value);
                    break;
                    
                case BUTTON3_GPIO:
                    switch3_on.value.bool_value = !switch3_on.value.bool_value;
                    relay_write(switch3_on.value.bool_value, RELAY3_GPIO);
                    homekit_characteristic_notify(&switch3_on, switch3_on.value);
                    break;
                    
                case BUTTON4_GPIO:
                    switch4_on.value.bool_value = !switch4_on.value.bool_value;
                    relay_write(switch4_on.value.bool_value, RELAY4_GPIO);
                    homekit_characteristic_notify(&switch4_on, switch4_on.value);
                    break;
                    
                default:
                    break;
            }
        }
    } else if (gpio_read(gpio) == 0) {
        last_reset_event_time = now;
        pushed_gpio = gpio;
    }
}

void identify(homekit_value_t _value) {
    xTaskCreate(identify_task, "Identify", 256, NULL, 3, NULL);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Sonoff Switch");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "Sonoff4 N/A");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "iTEAD"),
            &serial,
            HOMEKIT_CHARACTERISTIC(MODEL, "Sonoff 4CH"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.2"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Sonoff Switch 1"),
            &switch1_on,
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Sonoff Switch 2"),
            &switch2_on,
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Sonoff Switch 3"),
            &switch3_on,
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Sonoff Switch 4"),
            &switch4_on,
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
    snprintf(name_value, 15, "Sonoff4 %02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    
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
    
    wifi_config_init("Sonoff4", NULL, on_wifi_ready);
    
    gpio_init();
}
