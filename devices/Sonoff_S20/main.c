/* Sonoff S20
 *
 * To reset config, you can hold down the button
 * for at least 10 seconds and then release it.
 *
 * In order to flash the sonoff basic you will have to
 * have a 3,3v (logic level) FTDI adapter.
 *
 * To flash this example connect 3,3v, TX, RX, GND
 * in this order.
 * 
 * Next hold down the button and connect the FTDI adapter
 * to your computer. The sonoff is now in flash mode and
 * you can flash the custom firmware.
 *
 * WARNING: Do not connect the Sonoff to AC while it's
 * connected to the FTDI adapter! This may fry your
 * computer and sonoff.
 *
 */

#include <stdio.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <espressif/esp_wifi.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>

#include <led_codes.h>

#define BUTTON_GPIO         0
#define LED_GPIO            13
#define RELAY_GPIO          12

#define DEBOUNCE_TIME       300     / portTICK_PERIOD_MS
#define RESET_TIME          10000   / portTICK_PERIOD_MS

uint32_t last_button_event_time;
uint32_t last_reset_event_time;

void relay_write(bool on) {
    gpio_write(RELAY_GPIO, on ? 1 : 0);
}

void led_write(bool on) {
    gpio_write(LED_GPIO, on ? 0 : 1);
}

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);

void button_intr_callback(uint8_t gpio);

homekit_characteristic_t switch_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback));
homekit_characteristic_t switch_outlet_in_use = HOMEKIT_CHARACTERISTIC_(OUTLET_IN_USE, false);

void gpio_init() {
    gpio_enable(LED_GPIO, GPIO_OUTPUT);
    led_write(false);
    
    gpio_enable(RELAY_GPIO, GPIO_OUTPUT);
    relay_write(switch_on.value.bool_value);
    
    gpio_set_pullup(BUTTON_GPIO, true, true);
    gpio_set_interrupt(BUTTON_GPIO, GPIO_INTTYPE_EDGE_ANY, button_intr_callback);
    
    last_button_event_time = xTaskGetTickCountFromISR();
}

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    relay_write(switch_on.value.bool_value);
    homekit_characteristic_notify(&switch_outlet_in_use, switch_on.value);
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
    
    abort();
    vTaskDelete(NULL);
}

void button_intr_callback(uint8_t gpio) {
    uint32_t now = xTaskGetTickCountFromISR();
    
    if (((now - last_button_event_time) > DEBOUNCE_TIME) && (gpio_read(BUTTON_GPIO) == 1)) {
        if ((now - last_reset_event_time) > RESET_TIME) {
            xTaskCreate(reset_task, "Reset", 128, NULL, 1, NULL);
        } else {
            last_button_event_time = now;
            
            xTaskCreate(function_task, "Function", 128, NULL, 2, NULL);
            switch_on.value.bool_value = !switch_on.value.bool_value;
            relay_write(switch_on.value.bool_value);
            homekit_characteristic_notify(&switch_on, switch_on.value);
            homekit_characteristic_notify(&switch_outlet_in_use, switch_on.value);
        }
    } else if (gpio_read(BUTTON_GPIO) == 0) {
        last_reset_event_time = now;
    }
}

void identify(homekit_value_t _value) {
    xTaskCreate(identify_task, "Identify", 128, NULL, 2, NULL);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Sonoff S20");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "SonoffS20 N/A");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_outlet, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "iTEAD"),
            &serial,
            HOMEKIT_CHARACTERISTIC(MODEL, "Sonoff S20"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1.0"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(OUTLET, .primary=true, .characteristics=(homekit_characteristic_t*[]){
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
    
    uint8_t name_len = snprintf(NULL, 0, "SonoffS20 %02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "SonoffS20 %02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    
    name.value = HOMEKIT_STRING(name_value);
    serial.value = name.value;
}

void on_wifi_ready() {
    xTaskCreate(wifi_connected_task, "Wifi connected", 128, NULL, 1, NULL);
    
    create_accessory_name();
        
    homekit_server_init(&config);
}

void user_init(void) {
    uart_set_baud(0, 115200);
    
    wifi_config_init("SonoffS20", NULL, on_wifi_ready);
    
    gpio_init();
}
