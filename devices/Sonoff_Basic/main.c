/*
 * Example of using esp-homekit library to control
 * a simple $5 Sonoff Basic using HomeKit.
 * The esp-wifi-config library is also used in this
 * example. This means you don't have to specify
 * your network's SSID and password before building.
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

// The GPIO pin that is connected to the relay on the Sonoff Basic.
const int relay_gpio = 12;
// The GPIO pin that is connected to the LED on the Sonoff Basic.
const int led_gpio = 13;
// The GPIO pin that is connected to an optional external switch with ground.
const int switch_gpio = 14;
// The GPIO pin that is oconnected to the button on the Sonoff Basic.
const int button_gpio = 0;
// The minimum amount that has to occur between each button press.
const uint16_t button_event_debounce_time = 350 / portTICK_PERIOD_MS;
// The last time the button was pressed, in ticks.
uint32_t last_button_event_time;
// Times button has been pushed quickly
uint8_t times = 0;

void relay_write(bool on) {
    gpio_write(relay_gpio, on ? 1 : 0);
}

void led_write(bool on) {
    gpio_write(led_gpio, on ? 0 : 1);
}

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);

void button_intr_callback(uint8_t gpio);

homekit_characteristic_t switch_on = HOMEKIT_CHARACTERISTIC_(
                                                             ON,
                                                             false,
                                                             .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback)
                                                             );

void gpio_init() {
    gpio_enable(led_gpio, GPIO_OUTPUT);
    led_write(false);
    gpio_enable(relay_gpio, GPIO_OUTPUT);
    relay_write(switch_on.value.bool_value);
    
    gpio_set_pullup(button_gpio, true, true);
    gpio_set_interrupt(button_gpio, GPIO_INTTYPE_EDGE_NEG, button_intr_callback);
    
    gpio_enable(switch_gpio, GPIO_INPUT);
    gpio_set_pullup(switch_gpio, true, true);
    gpio_set_interrupt(switch_gpio, GPIO_INTTYPE_EDGE_ANY, button_intr_callback);
}

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    printf(">>> Switching\n");
    relay_write(switch_on.value.bool_value);
}

void switch_identify_task(void *_args) {
    // Identify Sonoff by turning on builtin LED for 2 seconds
    led_write(true);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    led_write(false);
    vTaskDelete(NULL);
}

void switch_reset_task(void *_args) {
    // Remove HomeKit config and reset device
    printf(">>> Resetting HomeKit setup\n");
    homekit_server_reset();
    led_write(true);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    led_write(false);
    printf(">>> Resetting device\n");
    sdk_system_restart();
    vTaskDelete(NULL);
}

void button_intr_callback(uint8_t gpio) {
    uint32_t now = xTaskGetTickCountFromISR();
    
    if ((now - last_button_event_time) < button_event_debounce_time) {
        // Debounce time, ignore switching events, but cheking to perform a HomeKit config reset
        if (gpio == button_gpio) {
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

void switch_identify(homekit_value_t _value) {
    printf(">>> Identifying\n");
    xTaskCreate(switch_identify_task, "Switch identify", 128, NULL, 2, NULL);
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
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0.0"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, switch_identify),
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
    uint8_t name_len = snprintf(NULL, 0, "Sonoff %02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "Sonoff %02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    
    name.value = HOMEKIT_STRING(name_value);
    serial.value = HOMEKIT_STRING(name_value);
}

void on_wifi_ready() {
    xTaskCreate(switch_identify_task, "Switch identify", 128, NULL, 1, NULL);
    
    create_accessory_name();
        
    // Start HomeKit
    homekit_server_init(&config);
}

void user_init(void) {
    uart_set_baud(0, 115200);
    
    wifi_config_init("Sonoff", NULL, on_wifi_ready);
    
    gpio_init();
}
