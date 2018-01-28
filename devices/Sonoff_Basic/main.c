/* Sonoff Basic
 *
 * An extra function has been added to GPIO14:
 * A switch (a mount wall switch, for example), can be
 * attached to GPIO14 and ground pin.
 *
 * To reset HomeKit config, you can push quickly button until
 * LED turns on.
 *
 * In order to flash the sonoff basic you will have to
 * have a 3,3v (logic level) FTDI adapter.
 *
 * To flash this example connect 3,3v, TX, RX, GND
 * in this order, beginning in the (square) pin header
 * next to the button.
 * Next hold down the button and connect the FTDI adapter
 * to your computer. The sonoff is now in flash mode and
 * you can flash the custom firmware.
 *
 * WARNING: Do not connect the sonoff to AC while it's
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
#include <espressif/esp_system.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>

// GPIO
#define BUTTON_GPIO     0
#define LED_GPIO        13
#define RELAY_GPIO      12
#define SWITCH_GPIO     14

// The minimum amount that has to occur between each button press.
const uint16_t button_event_debounce_time = 350 / portTICK_PERIOD_MS;
// The last time the button was pressed, in ticks.
uint32_t last_button_event_time;
// Times button has been pushed quickly
uint8_t times = 0;

void relay_write(bool on) {
    gpio_write(RELAY_GPIO, on ? 1 : 0);
}

void led_write(bool on) {
    gpio_write(LED_GPIO, on ? 0 : 1);
}

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);

void button_intr_callback(uint8_t gpio);

homekit_characteristic_t switch_on = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback));

void gpio_init() {
    gpio_enable(LED_GPIO, GPIO_OUTPUT);
    led_write(false);
    
    gpio_enable(RELAY_GPIO, GPIO_OUTPUT);
    relay_write(switch_on.value.bool_value);
    
    gpio_set_pullup(BUTTON_GPIO, true, true);
    gpio_set_interrupt(BUTTON_GPIO, GPIO_INTTYPE_EDGE_NEG, button_intr_callback);
    
    gpio_enable(SWITCH_GPIO, GPIO_INPUT);
    gpio_set_pullup(SWITCH_GPIO, true, true);
    gpio_set_interrupt(SWITCH_GPIO, GPIO_INTTYPE_EDGE_ANY, button_intr_callback);
}

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    printf(">>> Switching\n");
    relay_write(switch_on.value.bool_value);
}

void identify_task(void *_args) {
    // Identify Sonoff by turning on builtin LED for 3 seconds
    led_write(true);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    led_write(false);
    vTaskDelete(NULL);
}

void switch_reset_task(void *_args) {
    // Remove HomeKit config and reset device
    printf(">>> Resetting HomeKit setup\n");
    homekit_server_reset();
    
    // Turn on LED for 2 seconds
    led_write(true);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    led_write(false);
    
    printf(">>> Restarting device\n");
    sdk_system_restart();
    vTaskDelete(NULL);
}

void button_intr_callback(uint8_t gpio) {
    uint32_t now = xTaskGetTickCountFromISR();
    
    if ((now - last_button_event_time) < button_event_debounce_time) {
        // Debounce time, ignore switching events, but cheking to perform a HomeKit config reset
        if (gpio == BUTTON_GPIO) {
            times++;
            if (times == 5) {
                xTaskCreate(switch_reset_task, "Switch reset", 128, NULL, 1, NULL);
            }
        }
    } else {
        times = 0;
        last_button_event_time = now;
        switch_on.value.bool_value = !switch_on.value.bool_value;
        relay_write(switch_on.value.bool_value);
        homekit_characteristic_notify(&switch_on, switch_on.value);
    }
}

void identify(homekit_value_t _value) {
    printf(">>> Identifying\n");
    xTaskCreate(identify_task, "Switch identify", 128, NULL, 2, NULL);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Sonoff Switch");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "SonoffB N/A");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "iTEAD"),
            &serial,
            HOMEKIT_CHARACTERISTIC(MODEL, "Sonoff Basic"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Sonoff Switch"),
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
    // Get MAC address
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    // Use MAC address as part of accesory name and serial number
    uint8_t name_len = snprintf(NULL, 0, "SonoffB %02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "SonoffB %02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    
    name.value = HOMEKIT_STRING(name_value);
    serial.value = name.value;
}

void on_wifi_ready() {
    xTaskCreate(identify_task, "Identify", 128, NULL, 1, NULL);
    
    create_accessory_name();
        
    // Start HomeKit
    homekit_server_init(&config);
}

void user_init(void) {
    uart_set_baud(0, 115200);
    
    wifi_config_init("SonoffB", NULL, on_wifi_ready);
    
    gpio_init();
}
