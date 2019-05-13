/*
 * HAA
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

//#include <stdio.h>
#include <string.h>
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
#include <adv_button.h>

#include <dht/dht.h>
#include <ds18b20/ds18b20.h>

#include <cJSON.h>

// Version
#define FIRMWARE_VERSION                "0.0.1"
#define FIRMWARE_VERSION_OCTAL          000001      // Matches as example: firmware_revision 2.3.8 = 02.03.10 (octal) = config_number 020310

// JSON
#define GENERAL_CONFIG                  "c"
#define LOG_OUTPUT                      "o"
#define STATUS_LED_GPIO                 "l"
#define ACCESSORIES                     "a"
#define BUTTONS_ARRAY                   "b"
#define BUTTON_PRESS_TYPE               "t"
#define PULLUP_RESISTOR                 "p"
#define RELAYS_ARRAY                    "r"
#define PIN_GPIO                        "g"
#define INVERTED                        "i"
#define ACCESSORY_TYPE                  "t"
#define ACC_TYPE_SWITCH                 1
#define ACC_TYPE_OUTLET                 2
#define ACC_TYPE_BUTTON                 3

#ifndef HAA_MAX_ACCESSORIES
#define HAA_MAX_ACCESSORIES             4           // Max number of accessories before use a bridge
#endif

#define ALLOWED_SETUP_MODE_TIME         30000

#define LED_BLINK(x)                    xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) x, 1, NULL)
#define FREEHEAP()                      printf("HAA > Free Heap: %d\n", xPortGetFreeHeapSize())

uint8_t setup_mode_toggle_counter = 0, led_gpio = 255;
ETSTimer setup_mode_toggle_timer;
bool led_inverted = false;

void led_task(void *pvParameters) {
    if (led_gpio != 255) {
        const uint8_t times = (int) pvParameters;
        
        for (uint8_t i=0; i<times; i++) {
            gpio_write(led_gpio, true ^ led_inverted);
            vTaskDelay(20 / portTICK_PERIOD_MS);
            gpio_write(led_gpio, false ^ led_inverted);
            vTaskDelay(150 / portTICK_PERIOD_MS);
        }
    }
    
    vTaskDelete(NULL);
}

// -----

void setup_mode_task() {
    sysparam_set_bool("setup", true);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    sdk_system_restart();
}

void setup_mode_call(const uint8_t gpio) {
    printf("HAA > Checking setup mode call\n");
    
    if (xTaskGetTickCountFromISR() < ALLOWED_SETUP_MODE_TIME / portTICK_PERIOD_MS) {
        LED_BLINK(4);
        xTaskCreate(setup_mode_task, "setup_mode_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    } else {
        printf("HAA ! Setup mode not allowed after %i msecs since boot. Repower device and try again\n", ALLOWED_SETUP_MODE_TIME);
    }
}

void setup_mode_toggle_upcount() {
    setup_mode_toggle_counter++;
    sdk_os_timer_arm(&setup_mode_toggle_timer, 3100, 0);
}

void setup_mode_toggle() {
    if (setup_mode_toggle_counter > 10) {
        setup_mode_call(0);
    }
    
    setup_mode_toggle_counter = 0;
}

// -----

homekit_value_t hkc_getter(const homekit_characteristic_t *ch) {
    return ch->value;
}

void hkc_on_setter(homekit_characteristic_t *ch, const homekit_value_t value) {
    ch->value = value;
    homekit_characteristic_notify(ch, ch->value);
    
    cJSON *json_context = ch->context;

    printf("HAA > Setter ON\n");
    LED_BLINK(1);

    cJSON *json_relays = cJSON_GetObjectItem(json_context, RELAYS_ARRAY);
    
    for(int i=0; i<cJSON_GetArraySize(json_relays); i++) {
        const int gpio = (int) cJSON_GetObjectItem(cJSON_GetArrayItem(json_relays, i), PIN_GPIO)->valuedouble;
        bool inverted = false;
        if (cJSON_GetObjectItem(cJSON_GetArrayItem(json_relays, i), INVERTED) != NULL) {
            inverted = (bool) cJSON_GetObjectItem(cJSON_GetArrayItem(json_relays, i), INVERTED)->valuedouble;
        }
        const bool relay_value = value.bool_value ^ inverted;
        gpio_write(gpio, relay_value);
        printf("HAA > Relay GPIO %i -> %i\n", gpio, relay_value);
    }
    
    setup_mode_toggle_upcount();
}

void button_on(const uint8_t gpio, void *args) {
    homekit_characteristic_t *ch = args;
    ch->value.bool_value = !ch->value.bool_value;
    hkc_on_setter(ch, ch->value);
}

void identify(homekit_value_t _value) {
    printf("HAA > Identifying\n");
    LED_BLINK(6);
}

// ---------

void *memdup(const void *src, size_t size) {
    void *dst = malloc(size);
    return dst ? memcpy(dst, src, size) : NULL;
}

#define NEW_HOMEKIT_CHARACTERISTIC(name, ...)   (homekit_characteristic_t*)memdup((homekit_characteristic_t[]){HOMEKIT_CHARACTERISTIC_(name, ##__VA_ARGS__)}, sizeof(homekit_characteristic_t))

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, NULL);
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, NULL);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER, "RavenSystem");
homekit_characteristic_t model = HOMEKIT_CHARACTERISTIC_(MODEL, "HAA");
homekit_characteristic_t identify_function = HOMEKIT_CHARACTERISTIC_(IDENTIFY, identify);
homekit_characteristic_t firmware = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, FIRMWARE_VERSION);

homekit_server_config_t config;

void run_homekit_server() {
    printf("HAA > Starting HK Server\n");
    homekit_server_init(&config);
    LED_BLINK(6);
}

void normal_mode_init() {
    sdk_os_timer_setfn(&setup_mode_toggle_timer, setup_mode_toggle, NULL);
    
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    char *name_value = malloc(11);
    snprintf(name_value, 11, "HAA-%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    name.value = HOMEKIT_STRING(name_value);
    
    char *serial_value = malloc(13);
    snprintf(serial_value, 13, "%02X%02X%02X%02X%02X%02X", macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);
    serial.value = HOMEKIT_STRING(serial_value);
    
    char *txt_config = NULL;
    sysparam_get_string("haa_conf", &txt_config);
    
    cJSON *json_config = cJSON_GetObjectItem(cJSON_Parse(txt_config), GENERAL_CONFIG);
    cJSON *json_accessories = cJSON_GetObjectItem(cJSON_Parse(txt_config), ACCESSORIES);
    
    free(txt_config);
    
    const int total_accessories = cJSON_GetArraySize(json_accessories);
    
    if (total_accessories == 0) {
        uart_set_baud(0, 115200);
        printf("HAA ! ERROR: Invalid JSON\n");
        xTaskCreate(setup_mode_task, "setup_mode_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
        return;
    }
    
    // Log output type
    if (cJSON_GetObjectItem(json_config, LOG_OUTPUT) != NULL &&
        cJSON_GetObjectItem(json_config, LOG_OUTPUT)->valuedouble == 1) {
        uart_set_baud(0, 115200);
        printf("HAA > Running in NORMAL mode...\n");
        printf("HAA > Enable UART log output\n");
    }

    // Status LED
    if (cJSON_GetObjectItem(json_config, STATUS_LED_GPIO) != NULL) {
        led_gpio = (uint8_t) cJSON_GetObjectItem(json_config, STATUS_LED_GPIO)->valuedouble;

        if (cJSON_GetObjectItem(json_config, INVERTED) != NULL) {
                led_inverted = (bool) cJSON_GetObjectItem(json_config, INVERTED)->valuedouble;
        }
        
        gpio_enable(led_gpio, GPIO_OUTPUT);
        gpio_write(led_gpio, false ^ led_inverted);
        printf("HAA > Enable LED GPIO %i\n", led_gpio);
    }
    
    int hk_total_ac = 1;
    int hk_total_ch = 0;

    for(int i=0; i<total_accessories; i++) {
        int acc_type = ACC_TYPE_SWITCH;
        if (cJSON_GetObjectItem(cJSON_GetArrayItem(json_accessories, i), ACCESSORY_TYPE) != NULL) {
            acc_type = (int) cJSON_GetObjectItem(cJSON_GetArrayItem(json_accessories, i), ACCESSORY_TYPE)->valuedouble;
        }
        
        switch (acc_type) {
            case ACC_TYPE_OUTLET:
                // TODO
                break;
                
            default:    // case ACC_TYPE_SWITCH:
                hk_total_ac += 1;
                hk_total_ch += 1;
                break;
        }
    }
    
    if (total_accessories > HAA_MAX_ACCESSORIES) {
        // Bridge needed
        hk_total_ac += 1;
    }
    
    homekit_accessory_t **accessories = calloc(hk_total_ac, sizeof(homekit_accessory_t*));
    homekit_characteristic_t *hk_ch[hk_total_ch];
    
    // Define services and characteristics
    void new_accessory(const uint8_t accessory) {
        accessories[accessory] = calloc(1, sizeof(homekit_accessory_t));
        accessories[accessory]->id = accessory + 1;
        accessories[accessory]->services = calloc(3, sizeof(homekit_service_t*));
        
        accessories[accessory]->services[0] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[0]->id = 1;
        accessories[accessory]->services[0]->type = HOMEKIT_SERVICE_ACCESSORY_INFORMATION;
        accessories[accessory]->services[0]->characteristics = calloc(7, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[0]->characteristics[0] = &name;
        accessories[accessory]->services[0]->characteristics[1] = &manufacturer;
        accessories[accessory]->services[0]->characteristics[2] = &serial;
        accessories[accessory]->services[0]->characteristics[3] = &model;
        accessories[accessory]->services[0]->characteristics[4] = &firmware;
        accessories[accessory]->services[0]->characteristics[5] = &identify_function;
    }

    void new_switch(const int accessory, const int ch_number, cJSON *json_context) {
        new_accessory(accessory);
        
        hk_ch[ch_number] = NEW_HOMEKIT_CHARACTERISTIC(ON, false, .getter_ex=hkc_getter, .setter_ex=hkc_on_setter, .context=json_context);
        
        accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[1]->id = 8;
        accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_SWITCH;
        accessories[accessory]->services[1]->primary = true;
        accessories[accessory]->services[1]->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[1]->characteristics[0] = hk_ch[ch_number];
        
        // Buttons GPIO Setup
        cJSON *json_buttons = cJSON_GetObjectItem(json_context, BUTTONS_ARRAY);
        for(int j=0; j<cJSON_GetArraySize(json_buttons); j++) {
            const uint8_t gpio = (uint8_t) cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), PIN_GPIO)->valuedouble;
            bool pullup_resistor = false;
            if (cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), PULLUP_RESISTOR) != NULL &&
                cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), PULLUP_RESISTOR)->valuedouble == 1) {
                pullup_resistor = true;
            }
            
            bool inverted = false;
            if (cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), INVERTED) != NULL &&
                cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), INVERTED)->valuedouble == 1) {
                inverted = true;
            }
            
            uint8_t button_type = 1;
            if (cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), BUTTON_PRESS_TYPE) != NULL) {
                button_type = (uint8_t) cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), BUTTON_PRESS_TYPE)->valuedouble == 1;
            }
            
            adv_button_create(gpio, pullup_resistor, inverted);
            adv_button_register_callback_fn(gpio, button_on, button_type, (void*) hk_ch[ch_number]);
            printf("HAA > Enable button GPIO %i, type=%i, inverted=%i\n", gpio, button_type, inverted);
        }
    }
    
    // Accessory Builder
    int acc_count = 0;
    int ch_count = 0;
    
    if (total_accessories > HAA_MAX_ACCESSORIES) {
        new_accessory(0);
        acc_count += 1;
    }
    
    for(int i=0; i<total_accessories; i++) {
        int acc_type = ACC_TYPE_SWITCH;
        if (cJSON_GetObjectItem(cJSON_GetArrayItem(json_accessories, i), ACCESSORY_TYPE) != NULL) {
            acc_type = (int) cJSON_GetObjectItem(cJSON_GetArrayItem(json_accessories, i), ACCESSORY_TYPE)->valuedouble;
        }

        printf("HAA > Creating acc_type=%i\n", acc_type);
        
        // Relays GPIO Setup
        cJSON *json_relays = cJSON_GetObjectItem(cJSON_GetArrayItem(json_accessories, i), RELAYS_ARRAY);
        for(int j=0; j<cJSON_GetArraySize(json_relays); j++) {
            const int gpio = (int) cJSON_GetObjectItem(cJSON_GetArrayItem(json_relays, j), PIN_GPIO)->valuedouble;
            gpio_enable(gpio, GPIO_OUTPUT);
            printf("HAA > Enable relay GPIO %i\n", gpio);
        }
        
        if (acc_type == ACC_TYPE_OUTLET) {
            // TODO
        } else {    // case ACC_TYPE_SWITCH:
            new_switch(acc_count, ch_count, cJSON_GetArrayItem(json_accessories, i));
            ch_count += 1;
            acc_count += 1;
        }
    }
    
    cJSON_Delete(json_config);

    config.accessories = accessories;
    config.password = "021-82-017";
    config.setupId = "JOSE";
    config.category = homekit_accessory_category_bridge;
    config.config_number = FIRMWARE_VERSION_OCTAL;
    
    FREEHEAP();
    
    wifi_config_init("HAA", NULL, run_homekit_server);
}

void user_init(void) {
    bool haa_setup;
    sysparam_get_bool("setup", &haa_setup);
    if (haa_setup) {
        uart_set_baud(0, 115200);
        printf("HAA > Running in SETUP mode...\n");
        wifi_config_init("HAA", NULL, NULL);
    } else {
        normal_mode_init();
    }
}
