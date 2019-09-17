/*
 * Home Accessory Architect
 *
 * v0.0.6
 * 
 * Copyright 2019 José Antonio Jiménez (@RavenSystem)
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
#define FIRMWARE_VERSION                "0.0.7"
#define FIRMWARE_VERSION_OCTAL          000007      // Matches as example: firmware_revision 2.3.8 = 02.03.10 (octal) = config_number 020310

// JSON
#define GENERAL_CONFIG                  "c"
#define LOG_OUTPUT                      "o"
#define STATUS_LED_GPIO                 "l"
#define INVERTED                        "i"
#define BUTTON_FILTER                   "f"
#define ENABLE_HOMEKIT_SERVER           "h"
#define ACCESSORIES                     "a"
#define BUTTONS_ARRAY                   "b"
#define BUTTON_PRESS_TYPE               "t"
#define PULLUP_RESISTOR                 "p"
#define VALUE                           "v"
#define DIGITAL_OUTPUTS_ARRAY           "r"
#define AUTOSWITCH_TIME                 "i"
#define PIN_GPIO                        "g"
#define MAX_ACTIONS                     2

#define ACCESSORY_TYPE                  "t"
#define ACC_TYPE_SWITCH                 1
#define ACC_TYPE_OUTLET                 2
#define ACC_TYPE_BUTTON                 3

#ifndef HAA_MAX_ACCESSORIES
#define HAA_MAX_ACCESSORIES             4           // Max number of accessories before use a bridge
#endif

#define ALLOWED_SETUP_MODE_TIME         30000

#define FREEHEAP()                      printf("HAA > Free Heap: %d\n", xPortGetFreeHeapSize())

typedef struct _autoswitch_params {
    uint8_t gpio;
    bool value;
    double time;
} autoswitch_params_t;

typedef struct _autooff_setter_params {
    homekit_characteristic_t *ch;
    double time;
} autooff_setter_params_t;

uint8_t setup_mode_toggle_counter = 0, led_gpio = 255;
ETSTimer setup_mode_toggle_timer;
bool used_gpio[17];
bool led_inverted = false;
bool enable_homekit_server = true;

void led_task(void *pvParameters) {
    const uint8_t times = (int) pvParameters;
    
    for (uint8_t i=0; i<times; i++) {
        gpio_write(led_gpio, true ^ led_inverted);
        vTaskDelay(30 / portTICK_PERIOD_MS);
        gpio_write(led_gpio, false ^ led_inverted);
        vTaskDelay(130 / portTICK_PERIOD_MS);
    }
    
    vTaskDelete(NULL);
}

void led_blink(const int blinks) {
    if (led_gpio != 255) {
        xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) blinks, 1, NULL);
    }
}

// -----

void setup_mode_task() {
    sysparam_set_bool("setup", true);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    sdk_system_restart();
}

void setup_mode_call(const uint8_t gpio, void *args) {
    printf("HAA > Checking setup mode call\n");
    
    if (xTaskGetTickCountFromISR() < ALLOWED_SETUP_MODE_TIME / portTICK_PERIOD_MS) {
        led_blink(4);
        xTaskCreate(setup_mode_task, "setup_mode_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    } else {
        printf("HAA ! Setup mode not allowed after %i msecs since boot. Repower device and try again\n", ALLOWED_SETUP_MODE_TIME);
    }
}

void setup_mode_toggle_upcount() {
    setup_mode_toggle_counter++;
    sdk_os_timer_arm(&setup_mode_toggle_timer, 1000, 0);
}

void setup_mode_toggle() {
    if (setup_mode_toggle_counter > 10) {
        setup_mode_call(0, NULL);
    }
    
    setup_mode_toggle_counter = 0;
}

// -----
void autoswitch_task(void *pvParameters) {
    autoswitch_params_t *autoswitch_params = pvParameters;

    vTaskDelay(autoswitch_params->time * 1000 / portTICK_PERIOD_MS);
    
    gpio_write(autoswitch_params->gpio, autoswitch_params->value);
    printf("HAA > Autoswitch digital output GPIO %i -> %i\n", autoswitch_params->gpio, autoswitch_params->value);
    
    free(autoswitch_params);
    vTaskDelete(NULL);
}

void do_actions(cJSON *json_context, const uint8_t int_action) {
    char *action = malloc(2);
    itoa(int_action, action, 10);
    
    if (cJSON_GetObjectItem(json_context, action) != NULL) {
        cJSON *actions = cJSON_GetObjectItem(json_context, action);
        
        // Digital outputs
        cJSON *json_relays = cJSON_GetObjectItem(actions, DIGITAL_OUTPUTS_ARRAY);
        for(uint8_t i=0; i<cJSON_GetArraySize(json_relays); i++) {
            cJSON *json_relay = cJSON_GetArrayItem(json_relays, i);
            
            const uint8_t gpio = (uint8_t) cJSON_GetObjectItem(json_relay, PIN_GPIO)->valuedouble;
            const bool output_value = (bool) cJSON_GetObjectItem(json_relay, VALUE)->valuedouble;

            gpio_write(gpio, output_value);
            printf("HAA > Digital output GPIO %i -> %i\n", gpio, output_value);
            
            if (cJSON_GetObjectItem(json_relay, AUTOSWITCH_TIME) != NULL) {
                const double autoswitch_time = cJSON_GetObjectItem(json_relay, AUTOSWITCH_TIME)->valuedouble;
                if (autoswitch_time > 0) {
                    autoswitch_params_t *autoswitch_params = malloc(sizeof(autoswitch_params_t));
                    autoswitch_params->gpio = gpio;
                    autoswitch_params->value = output_value ^ 1;
                    autoswitch_params->time = autoswitch_time;
                    xTaskCreate(autoswitch_task, "autoswitch_task", configMINIMAL_STACK_SIZE, autoswitch_params, 1, NULL);
                }
            }
        }
    }
    
    free(action);
}

homekit_value_t hkc_getter(const homekit_characteristic_t *ch) {
    printf("HAA > Getter\n");
    return ch->value;
}

void hkc_on_setter(homekit_characteristic_t *ch, const homekit_value_t value);

void hkc_autooff_setter_task(void *pvParameters) {
    autooff_setter_params_t *autooff_setter_params = pvParameters;
    
    vTaskDelay(autooff_setter_params->time * 1000 / portTICK_PERIOD_MS);
    
    hkc_on_setter(autooff_setter_params->ch, HOMEKIT_BOOL(false));
    
    free(autooff_setter_params);
    vTaskDelete(NULL);
}

void hkc_on_setter(homekit_characteristic_t *ch, const homekit_value_t value) {
    led_blink(1);
    printf("HAA > Setter ON\n");
    
    ch->value = value;
    homekit_characteristic_notify(ch, ch->value);
    
    cJSON *json_context = ch->context;
    
    do_actions(json_context, (uint8_t) ch->value.bool_value);
    
    if (ch->value.bool_value && cJSON_GetObjectItem(json_context, AUTOSWITCH_TIME) != NULL) {
        const double autoswitch_time = cJSON_GetObjectItem(json_context, AUTOSWITCH_TIME)->valuedouble;
        if (autoswitch_time > 0) {
            autooff_setter_params_t *autooff_setter_params = malloc(sizeof(autooff_setter_params_t));
            autooff_setter_params->ch = ch;
            autooff_setter_params->time = autoswitch_time;
            xTaskCreate(hkc_autooff_setter_task, "hkc_autooff_setter_task", configMINIMAL_STACK_SIZE, autooff_setter_params, 1, NULL);
        }
    }
    
    setup_mode_toggle_upcount();
}

void button_on(const uint8_t gpio, void *args) {
    homekit_characteristic_t *ch = args;
    ch->value.bool_value = !ch->value.bool_value;
    hkc_on_setter(ch, ch->value);
}

void button_event1(const uint8_t gpio, void *args) {
    homekit_characteristic_t *ch = args;
    homekit_characteristic_notify(ch, HOMEKIT_UINT8(0));
    led_blink(1);
    printf("HAA > Single press event\n");
    
    setup_mode_toggle_upcount();
}

void button_event2(const uint8_t gpio, void *args) {
    homekit_characteristic_t *ch = args;
    homekit_characteristic_notify(ch, HOMEKIT_UINT8(1));
    led_blink(2);
    printf("HAA > Double press event\n");
    
    setup_mode_toggle_upcount();
}

void button_event3(const uint8_t gpio, void *args) {
    homekit_characteristic_t *ch = args;
    homekit_characteristic_notify(ch, HOMEKIT_UINT8(2));
    led_blink(3);
    printf("HAA > Long press event\n");
}

void identify(homekit_value_t _value) {
    led_blink(6);
    printf("HAA > Identifying\n");
}

// ---------

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, NULL);
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, NULL);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER, "RavenSystem");
homekit_characteristic_t model = HOMEKIT_CHARACTERISTIC_(MODEL, "HAA");
homekit_characteristic_t identify_function = HOMEKIT_CHARACTERISTIC_(IDENTIFY, identify);
homekit_characteristic_t firmware = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, FIRMWARE_VERSION);

homekit_server_config_t config;

void run_homekit_server() {
    if (enable_homekit_server) {
        printf("HAA > Starting HK Server\n");
        homekit_server_init(&config);
    }
    
    led_blink(6);
}

// Buttons GPIO Setup
void buttons_setup(cJSON *json_buttons, void *callback, void *hk_ch) {
    for(uint8_t j=0; j<cJSON_GetArraySize(json_buttons); j++) {
        const uint8_t gpio = (uint8_t) cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), PIN_GPIO)->valuedouble;
        bool pullup_resistor = true;
        if (cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), PULLUP_RESISTOR) != NULL &&
            cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), PULLUP_RESISTOR)->valuedouble == 0) {
            pullup_resistor = false;
        }
        
        bool inverted = false;
        if (cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), INVERTED) != NULL &&
            cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), INVERTED)->valuedouble == 1) {
            inverted = true;
        }
        
        uint8_t button_type = 1;
        if (cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), BUTTON_PRESS_TYPE) != NULL) {
            button_type = (uint8_t) cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), BUTTON_PRESS_TYPE)->valuedouble;
        }
        
        if (!used_gpio[gpio]) {
            adv_button_create(gpio, pullup_resistor, inverted);
            used_gpio[gpio] = true;
        }
        adv_button_register_callback_fn(gpio, callback, button_type, hk_ch);
        printf("HAA > Enable button GPIO %i, type=%i, inverted=%i\n", gpio, button_type, inverted);
    }
}

void normal_mode_init() {
    for (uint8_t g=0; g<17; g++) {
        used_gpio[g] = false;
    }
    
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
    
    const uint8_t total_accessories = cJSON_GetArraySize(json_accessories);
    
    if (total_accessories == 0) {
        uart_set_baud(0, 115200);
        printf("HAA ! ERROR: Invalid JSON\n");
        xTaskCreate(setup_mode_task, "setup_mode_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
        return;
    }
    
    // ----- CONFIG SECTION
    
    // Log output type
    if (cJSON_GetObjectItem(json_config, LOG_OUTPUT) != NULL &&
        cJSON_GetObjectItem(json_config, LOG_OUTPUT)->valuedouble == 1) {
        uart_set_baud(0, 115200);
        printf("\n\nHAA > Home Accessory Architect\nHAA > Developed by @RavenSystem - José Antonio Jiménez\nHAA > Version: %s\n\n", FIRMWARE_VERSION);
        printf("HAA > Running in NORMAL mode...\n");
        printf("HAA > ----- PROCESSING JSON -----\n");
        printf("HAA > Enable UART log output\n");
    }

    // Status LED
    if (cJSON_GetObjectItem(json_config, STATUS_LED_GPIO) != NULL) {
        led_gpio = (uint8_t) cJSON_GetObjectItem(json_config, STATUS_LED_GPIO)->valuedouble;

        if (cJSON_GetObjectItem(json_config, INVERTED) != NULL) {
                led_inverted = (bool) cJSON_GetObjectItem(json_config, INVERTED)->valuedouble;
        }
        
        gpio_enable(led_gpio, GPIO_OUTPUT);
        used_gpio[led_gpio] = true;
        gpio_write(led_gpio, false ^ led_inverted);
        printf("HAA > Enable LED GPIO %i, inverted=%i\n", led_gpio, led_inverted);
    }
    
    // Button filter
    if (cJSON_GetObjectItem(json_config, BUTTON_FILTER) != NULL) {
        uint8_t button_filter_value = (uint8_t) cJSON_GetObjectItem(json_config, BUTTON_FILTER)->valuedouble;
        adv_button_set_evaluate_delay(button_filter_value);
        printf("HAA > Button filter set to %i\n", button_filter_value);
    }
    
    // Run HomeKit Server
    if (cJSON_GetObjectItem(json_config, ENABLE_HOMEKIT_SERVER) != NULL) {
        enable_homekit_server = (bool) cJSON_GetObjectItem(json_config, ENABLE_HOMEKIT_SERVER)->valuedouble;
        printf("HAA > Run HomeKit Server set to %i\n", enable_homekit_server);
    }
    
    // Buttons to enter setup mode
    buttons_setup(cJSON_GetObjectItem(json_config, BUTTONS_ARRAY), setup_mode_call, NULL);
    
    // ----- END CONFIG SECTION
    
    uint8_t hk_total_ac = 1;
    bool bridge_needed = false;

    for(uint8_t i=0; i<total_accessories; i++) {
        uint8_t acc_type = ACC_TYPE_SWITCH;
        if (cJSON_GetObjectItem(cJSON_GetArrayItem(json_accessories, i), ACCESSORY_TYPE) != NULL) {
            acc_type = (uint8_t) cJSON_GetObjectItem(cJSON_GetArrayItem(json_accessories, i), ACCESSORY_TYPE)->valuedouble;
        }
        
        switch (acc_type) {
            case ACC_TYPE_OUTLET:
                hk_total_ac += 1;
                break;

            case ACC_TYPE_BUTTON:
                hk_total_ac += 1;
                break;

            default:    // case ACC_TYPE_SWITCH:
                hk_total_ac += 1;
                break;
        }
    }
    
    if (total_accessories > HAA_MAX_ACCESSORIES || bridge_needed) {
        // Bridge needed
        bridge_needed = true;
        hk_total_ac += 1;
    }
    
    homekit_accessory_t **accessories = calloc(hk_total_ac, sizeof(homekit_accessory_t*));
    
    
    void buttons_setup_programable(cJSON *json_buttons, void *hk_ch) {
        for(int j=0; j<cJSON_GetArraySize(json_buttons); j++) {
            const uint8_t gpio = (uint8_t) cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), PIN_GPIO)->valuedouble;
            bool pullup_resistor = true;
            if (cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), PULLUP_RESISTOR) != NULL &&
                cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), PULLUP_RESISTOR)->valuedouble == 0) {
                pullup_resistor = false;
            }
            
            bool inverted = false;
            if (cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), INVERTED) != NULL &&
                cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), INVERTED)->valuedouble == 1) {
                inverted = true;
            }
            
            uint8_t button_type = 1;
            if (cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), BUTTON_PRESS_TYPE) != NULL) {
                button_type = (uint8_t) cJSON_GetObjectItem(cJSON_GetArrayItem(json_buttons, j), BUTTON_PRESS_TYPE)->valuedouble;
            }
            
            if (!used_gpio[gpio]) {
                adv_button_create(gpio, pullup_resistor, inverted);
                used_gpio[gpio] = true;
            }
            
            switch (j) {
                case 0:
                    adv_button_register_callback_fn(gpio, button_event1, button_type, hk_ch);
                    printf("HAA > Enable button GPIO %i, type=%i, inverted=%i\n", gpio, button_type, inverted);
                    break;
                    
                case 1:
                    adv_button_register_callback_fn(gpio, button_event2, button_type, hk_ch);
                    printf("HAA > Enable button GPIO %i, type=%i, inverted=%i\n", gpio, button_type, inverted);
                    break;
                    
                case 2:
                    adv_button_register_callback_fn(gpio, button_event3, button_type, hk_ch);
                    printf("HAA > Enable button GPIO %i, type=%i, inverted=%i\n", gpio, button_type, inverted);
                    break;
                    
                default:
                    printf("HAA ! Error with button GPIO %i, type=%i. Only 3 buttons are allowed\n", gpio, button_type);
                    break;
            }
        }
    }
    
    // Define services and characteristics
    void new_accessory(const uint8_t accessory, const uint8_t services) {
        accessories[accessory] = calloc(1, sizeof(homekit_accessory_t));
        accessories[accessory]->id = accessory + 1;
        accessories[accessory]->services = calloc(services, sizeof(homekit_service_t*));
        
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

    void new_switch(const uint8_t accessory, cJSON *json_context) {
        new_accessory(accessory, 3);
        
        homekit_characteristic_t *ch = NEW_HOMEKIT_CHARACTERISTIC(ON, false, .getter_ex=hkc_getter, .setter_ex=hkc_on_setter, .context=json_context);
        
        accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[1]->id = 8;
        accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_SWITCH;
        accessories[accessory]->services[1]->primary = true;
        accessories[accessory]->services[1]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[1]->characteristics[0] = ch;
        
        buttons_setup(cJSON_GetObjectItem(json_context, BUTTONS_ARRAY), button_on, (void*) ch);
    }
    
    void new_outlet(const uint8_t accessory, cJSON *json_context) {
        new_accessory(accessory, 3);
        
        homekit_characteristic_t *ch = NEW_HOMEKIT_CHARACTERISTIC(ON, false, .getter_ex=hkc_getter, .setter_ex=hkc_on_setter, .context=json_context);
        
        accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[1]->id = 8;
        accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_OUTLET;
        accessories[accessory]->services[1]->primary = true;
        accessories[accessory]->services[1]->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[1]->characteristics[0] = ch;
        accessories[accessory]->services[1]->characteristics[1] = NEW_HOMEKIT_CHARACTERISTIC(OUTLET_IN_USE, true, .getter_ex=hkc_getter, .context=json_context);;
        
        buttons_setup(cJSON_GetObjectItem(json_context, BUTTONS_ARRAY), button_on, (void*) ch);
    }
    
    void new_button(const uint8_t accessory, cJSON *json_context) {
        new_accessory(accessory, 3);
        
        homekit_characteristic_t *ch = NEW_HOMEKIT_CHARACTERISTIC(PROGRAMMABLE_SWITCH_EVENT, 0);
        
        accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[1]->id = 8;
        accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_STATELESS_PROGRAMMABLE_SWITCH;
        accessories[accessory]->services[1]->primary = true;
        accessories[accessory]->services[1]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[1]->characteristics[0] = ch;
        
        buttons_setup_programable(cJSON_GetObjectItem(json_context, BUTTONS_ARRAY), (void*) ch);
    }
    
    // Accessory Builder
    uint8_t acc_count = 0;
    
    if (bridge_needed) {
        printf("HAA > Creating Acc 0, type=bridge\n");
        new_accessory(0, 2);
        acc_count += 1;
    }
    
    for(uint8_t i=0; i<total_accessories; i++) {
        uint8_t acc_type = ACC_TYPE_SWITCH;
        if (cJSON_GetObjectItem(cJSON_GetArrayItem(json_accessories, i), ACCESSORY_TYPE) != NULL) {
            acc_type = (uint8_t) cJSON_GetObjectItem(cJSON_GetArrayItem(json_accessories, i), ACCESSORY_TYPE)->valuedouble;
        }
        
        cJSON *json_accessory = cJSON_GetArrayItem(json_accessories, i);

        // Digital outputs GPIO Setup
        for (uint8_t int_action=0; int_action<MAX_ACTIONS; int_action++) {
            char *action = malloc(2);
            itoa(int_action, action, 10);
            
            if (cJSON_GetObjectItem(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItem(cJSON_GetObjectItem(json_accessory, action), DIGITAL_OUTPUTS_ARRAY) != NULL) {
                    cJSON *json_relays = cJSON_GetObjectItem(cJSON_GetObjectItem(json_accessory, action), DIGITAL_OUTPUTS_ARRAY);
                    free(action);
                    for(uint8_t j=0; j<cJSON_GetArraySize(json_relays); j++) {
                        const uint8_t gpio = (uint8_t) cJSON_GetObjectItem(cJSON_GetArrayItem(json_relays, j), PIN_GPIO)->valuedouble;
                        if (!used_gpio[gpio]) {
                            gpio_enable(gpio, GPIO_OUTPUT);
                            used_gpio[gpio] = true;
                            printf("HAA > Enable digital output GPIO %i\n", gpio);
                        }
                    }
                }
            }
        }
        
        // Creating HomeKit Accessory
        printf("HAA > Creating Acc %i, type=%i\n", acc_count, acc_type);
        if (acc_type == ACC_TYPE_OUTLET) {
            new_outlet(acc_count, json_accessory);
            acc_count += 1;
        } else if (acc_type == ACC_TYPE_BUTTON) {
            new_button(acc_count, json_accessory);
            acc_count += 1;
        } else {    // case ACC_TYPE_SWITCH:
            new_switch(acc_count, json_accessory);
            acc_count += 1;
        }
    }
    
    // -----
    
    cJSON_Delete(json_config);

    config.accessories = accessories;
    config.password = "021-82-017";
    config.setupId = "JOSE";
    config.category = homekit_accessory_category_other;
    config.config_number = FIRMWARE_VERSION_OCTAL;
    
    FREEHEAP();
    printf("HAA > ---------------------------\n");
    
    wifi_config_init("HAA", NULL, run_homekit_server);
}

void user_init(void) {
    sysparam_status_t status;
    bool haa_setup = false;
    status = sysparam_get_bool("setup", &haa_setup);
    if (status == SYSPARAM_OK && haa_setup == true) {
        uart_set_baud(0, 115200);
        printf("\n\nHAA > Home Accessory Architect\nHAA > Developed by @RavenSystem - José Antonio Jiménez\nHAA > Version: %s\n\n", FIRMWARE_VERSION);
        printf("HAA > Running in SETUP mode...\n");
        wifi_config_init("HAA", NULL, NULL);
    } else {
        normal_mode_init();
    }
}
