/*
 * Sonoff TH with Power Outage Warning
 * 
 * v0.2.2
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

#include <dht/dht.h>

#define BUTTON_GPIO         0
#define LED_GPIO            13
#define RELAY_GPIO          12
#define SENSOR_GPIO         14

#define DEBOUNCE_TIME       300     / portTICK_PERIOD_MS
#define RESET_TIME          10000   / portTICK_PERIOD_MS

#define delay_ms(ms)        vTaskDelay((ms) / portTICK_PERIOD_MS)

#define POLL_PERIOD         30000

#define POW_DELAY           20000
#define POW_DURATION        3000

uint32_t last_button_event_time, last_reset_event_time;

void relay_write(bool on) {
    gpio_write(RELAY_GPIO, on ? 1 : 0);
}

void led_write(bool on) {
    gpio_write(LED_GPIO, on ? 0 : 1);
}

void update_state();

void on_update(homekit_characteristic_t *ch, homekit_value_t value, void *context) {
    update_state();
}

void on_target(homekit_characteristic_t *ch, homekit_value_t value, void *context);

void identify_task() {
    led_code(LED_GPIO, IDENTIFY_ACCESSORY);
    vTaskDelete(NULL);
}

void wifi_connected_task() {
    led_code(LED_GPIO, WIFI_CONNECTED);
    vTaskDelete(NULL);
}

void function_off_task() {
    led_code(LED_GPIO, FUNCTION_A);
    vTaskDelete(NULL);
}

void function_heat_task() {
    led_code(LED_GPIO, FUNCTION_B);
    vTaskDelete(NULL);
}

void function_cool_task() {
    led_code(LED_GPIO, FUNCTION_C);
    vTaskDelete(NULL);
}

void reset_task() {
    homekit_server_reset();
    wifi_config_reset();
    
    led_code(LED_GPIO, RESTART_DEVICE);
    
    sdk_system_restart();
    vTaskDelete(NULL);
}

void identify(homekit_value_t _value) {
    xTaskCreate(identify_task, "Identify", 96, NULL, 3, NULL);
}

homekit_characteristic_t current_temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t target_temperature  = HOMEKIT_CHARACTERISTIC_(TARGET_TEMPERATURE, 23, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update));
homekit_characteristic_t units = HOMEKIT_CHARACTERISTIC_(TEMPERATURE_DISPLAY_UNITS, 0);
homekit_characteristic_t current_state = HOMEKIT_CHARACTERISTIC_(CURRENT_HEATING_COOLING_STATE, 0);
homekit_characteristic_t target_state = HOMEKIT_CHARACTERISTIC_(TARGET_HEATING_COOLING_STATE, 0, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_target));
homekit_characteristic_t current_humidity = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);

homekit_characteristic_t power_cut_alarm = HOMEKIT_CHARACTERISTIC_(MOTION_DETECTED, false);
homekit_characteristic_t power_cut_switch = HOMEKIT_CHARACTERISTIC_(ON, true);

void power_outage_warning_task() {
    uint8_t i;
    for (i=0; i<150; i++) {
        delay_ms(POW_DELAY - POW_DURATION);
        
        if (!power_cut_switch.value.bool_value) {
            break;
        }
        
        printf(">>> Power Outage Warning: ON event sent\n");
        power_cut_alarm.value = HOMEKIT_BOOL(true);
        homekit_characteristic_notify(&power_cut_alarm, HOMEKIT_BOOL(true));
        
        delay_ms(POW_DURATION);
        
        printf(">>> Power Outage Warning: OFF event sent\n");
        power_cut_alarm.value = HOMEKIT_BOOL(false);
        homekit_characteristic_notify(&power_cut_alarm, HOMEKIT_BOOL(false));
    }
    
    power_cut_switch.value = HOMEKIT_BOOL(false);
    homekit_characteristic_notify(&power_cut_switch, HOMEKIT_BOOL(false));
    
    vTaskDelete(NULL);
}

void on_target(homekit_characteristic_t *ch, homekit_value_t value, void *context) {
    switch (target_state.value.int_value) {
        case 1:
            xTaskCreate(function_heat_task, "Function heat", 96, NULL, 3, NULL);
            break;
            
        case 2:
            xTaskCreate(function_cool_task, "Function cool", 96, NULL, 3, NULL);
            break;
            
        default:
            xTaskCreate(function_off_task, "Function off", 96, NULL, 3, NULL);
            break;
    }
    
    update_state();
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
            
            relay_write(true);
        }
    } else if (state == 2 && current_temperature.value.float_value > target_temperature.value.float_value) {
        if (current_state.value.int_value != 2) {
            current_state.value = HOMEKIT_UINT8(2);
            homekit_characteristic_notify(&current_state, current_state.value);
            
            relay_write(true);
        }
    } else if (current_state.value.int_value != 0) {
        current_state.value = HOMEKIT_UINT8(0);
        homekit_characteristic_notify(&current_state, current_state.value);
            
        relay_write(false);
    }
}

void button_intr_callback(uint8_t gpio) {
    uint32_t now = xTaskGetTickCountFromISR();
    
    if (((now - last_button_event_time) > DEBOUNCE_TIME) && (gpio_read(BUTTON_GPIO) == 1)) {
        if ((now - last_reset_event_time) > RESET_TIME) {
            xTaskCreate(reset_task, "Reset", 128, NULL, 1, NULL);
        } else {
            last_button_event_time = now;
            
            uint8_t state = target_state.value.int_value + 1;
            switch (state) {
                case 1:
                    xTaskCreate(function_heat_task, "Function heat", 96, NULL, 3, NULL);
                    break;
                    
                case 2:
                    xTaskCreate(function_cool_task, "Function cool", 96, NULL, 3, NULL);
                    break;
                    
                default:
                    state = 0;
                    xTaskCreate(function_off_task, "Function off", 96, NULL, 3, NULL);
                    break;
            }
            
            target_state.value = HOMEKIT_UINT8(state);
            homekit_characteristic_notify(&target_state, target_state.value);
            
            update_state();
        }
    } else if (gpio_read(BUTTON_GPIO) == 0) {
        last_reset_event_time = now;
    }
}

void temperature_sensor_task() {
    gpio_enable(LED_GPIO, GPIO_OUTPUT);
    led_write(false);
    
    gpio_set_pullup(BUTTON_GPIO, true, true);
    gpio_set_interrupt(BUTTON_GPIO, GPIO_INTTYPE_EDGE_ANY, button_intr_callback);
    
    gpio_set_pullup(SENSOR_GPIO, false, false);
    
    gpio_enable(RELAY_GPIO, GPIO_OUTPUT);
    relay_write(false);
    
    last_button_event_time = xTaskGetTickCountFromISR();
    
    float humidity_value, temperature_value;
    float old_humidity_value = 0.0, old_temperature_value = 0.0;
    while (1) {
        delay_ms(POLL_PERIOD);
        
        if (dht_read_float_data(DHT_TYPE_DHT22, SENSOR_GPIO, &humidity_value, &temperature_value)) {
            printf(">>> Sensor: temperature %g, humidity %g\n", temperature_value, humidity_value);
            
            if (temperature_value != old_temperature_value) {
                old_temperature_value = temperature_value;
                current_temperature.value = HOMEKIT_FLOAT(temperature_value);
                homekit_characteristic_notify(&current_temperature, current_temperature.value);
                
                update_state();
                
                if (humidity_value != old_humidity_value) {
                    delay_ms(1000);
                    old_humidity_value = humidity_value;
                    current_humidity.value = HOMEKIT_FLOAT(humidity_value);
                    homekit_characteristic_notify(&current_humidity, current_humidity.value);
                }
            }

        } else {
            printf(">>> Sensor: ERROR\n");
            led_code(LED_GPIO, SENSOR_ERROR);
            
            if (current_state.value.int_value != 0) {
                current_state.value = HOMEKIT_UINT8(0);
                homekit_characteristic_notify(&current_state, current_state.value);
                
                relay_write(false);
            }
            
        }
    }
    
    vTaskDelete(NULL);
}

void thermostat_init() {
    xTaskCreate(temperature_sensor_task, "Thermostat", 256, NULL, 2, NULL);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Sonoff Thermostat");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "SonoffTH N/A");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_thermostat, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "iTEAD"),
            &serial,
            HOMEKIT_CHARACTERISTIC(MODEL, "Sonoff TH wPOW"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.2.2"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(THERMOSTAT, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Thermostat"),
            &current_temperature,
            &target_temperature,
            &current_state,
            &target_state,
            &units,
            &current_humidity,
            NULL
        }),
        HOMEKIT_SERVICE(MOTION_SENSOR, .primary=false, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Power Outage Warning"),
            &power_cut_alarm,
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=false, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Power Outage System"),
            &power_cut_switch,
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
    
    char *name_value = malloc(16);
    snprintf(name_value, 16, "SonoffTH %02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    
    name.value = HOMEKIT_STRING(name_value);
    serial.value = HOMEKIT_STRING(name_value);
}

void on_wifi_ready() {
    xTaskCreate(wifi_connected_task, "Wifi connected", 96, NULL, 3, NULL);
    
    create_accessory_name();
        
    homekit_server_init(&config);
    
    xTaskCreate(power_outage_warning_task, "Power Outage Warning", 192, NULL, 3, NULL);
}

void user_init(void) {
    uart_set_baud(0, 115200);
    
    wifi_config_init("SonoffTH", NULL, on_wifi_ready);
    
    thermostat_init();
}
