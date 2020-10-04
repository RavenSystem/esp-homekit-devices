/*
 * Home Accessory Architect
 *
 * Copyright 2019-2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#include <unistd.h>
#include <string.h>
#include <esp/uart.h>
#include <FreeRTOS.h>
#include <timers.h>
#include <task.h>
#include <espressif/esp_common.h>
#include <rboot-api.h>
#include <sysparam.h>
#include <math.h>
#include <spiflash.h>

#include <esplibs/libmain.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/etharp.h"

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <led_codes.h>
#include <adv_button.h>
#include <adv_hlw.h>
#include <ping.h>
#include <adv_logger.h>

#include <multipwm/multipwm.h>

#include <dht.h>
#include <ds18b20/ds18b20.h>

#include <cJSON.h>

#include "setup_mode/include/wifi_config.h"
#include "../common/ir_code.h"

#include "extra_characteristics.h"
#include "header.h"
#include "haa_lightbulb_driver.h"
#include "types.h"
#include "my92xx.h"

main_config_t main_config = {
    .wifi_status = WIFI_STATUS_CONNECTED,
    .wifi_channel = 0,
    .wifi_ping_max_errors = 255,
    .wifi_ping_is_running = false,
    .wifi_error_count = 0,
    
    .setup_mode_toggle_counter = INT8_MIN,
    .setup_mode_toggle_counter_max = SETUP_MODE_DEFAULT_ACTIVATE_COUNT,
    .led_gpio = GPIO_OVERFLOW,
    .setup_mode_time = 0,
    
    .ping_is_running = false,
    .pwm_freq = 0,
    .setpwm_is_running = false,
    .setpwm_bool_semaphore = true,
    .enable_homekit_server = true,

    .ir_tx_freq = 13,
    .ir_tx_gpio = GPIO_OVERFLOW,
    .ir_tx_inv = false,
    
    .ping_poll_period = PING_POLL_PERIOD_DEFAULT,
    
    .used_gpio_0 = false,
    .used_gpio_1 = false,
    .used_gpio_2 = false,
    .used_gpio_3 = false,
    .used_gpio_4 = false,
    .used_gpio_5 = false,
    .used_gpio_9 = false,
    .used_gpio_10 = false,
    .used_gpio_11 = false,
    .used_gpio_12 = false,
    .used_gpio_13 = false,
    .used_gpio_14 = false,
    .used_gpio_15 = false,
    .used_gpio_16 = false,
    .used_gpio_17 = false,
    
    .save_states_timer = NULL,
    .setup_mode_toggle_timer = NULL,
    
    .ch_groups = NULL,
    .lightbulb_groups = NULL,
    .ping_inputs = NULL,
    .last_states = NULL
};

void hkc_autooff_setter_task(TimerHandle_t xTimer);
void do_actions(ch_group_t* ch_group, uint8_t int_action);
void do_wildcard_actions(ch_group_t* ch_group, uint8_t index, const float action_value);

#ifdef HAA_DEBUG
int32_t free_heap = 0;
void free_heap_watchdog() {
    int32_t size = xPortGetFreeHeapSize();
    if (size != free_heap) {
        free_heap = size;
        INFO("* Free Heap = %d", free_heap);
        
        char* space = NULL;
        while (!space && size > 0) {
            space = malloc(size);
            size -= 4;
        }
        
        free(space);
        INFO("* Max chunk = %i", size + 4);
        INFO("* CPU Speed = %i", sdk_system_get_cpu_freq());
    }
}
#endif  // HAA_DEBUG

void disable_emergency_setup(TimerHandle_t xTimer) {
    INFO("Disarming Emergency Setup Mode");
    sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
    xTimerDelete(xTimer, XTIMER_BLOCK_TIME);
}

void change_uart_gpio(const uint8_t gpio) {
    if (gpio == 1) {
        gpio_set_iomux_function(1, IOMUX_GPIO1_FUNC_GPIO);
    } else if (gpio == 3) {
        gpio_set_iomux_function(3, IOMUX_GPIO3_FUNC_GPIO);
    }
}

uint16_t process_hexstr(const char* string, char** output_hex_string) {
    const uint16_t len = strlen(string) >> 1;
    char* hex_string = malloc(len + 1);
    memset(hex_string, 0, len + 1);
    
    char buffer[3];
    buffer[2] = 0;
    
    for (uint16_t i = 0; i < len; i++) {
        buffer[0] = string[i << 1];
        buffer[1] = string[(i << 1) + 1];
                           
        hex_string[i] = (uint8_t) strtol(buffer, NULL, 16);
    }
    
    *output_hex_string = hex_string;
    
    return len;
}

action_task_t* new_action_task() {
    action_task_t* action_task = malloc(sizeof(action_task_t));
    memset(action_task, 0, sizeof(*action_task));
    
    return action_task;
}

ch_group_t* new_ch_group() {
    ch_group_t* ch_group = malloc(sizeof(ch_group_t));
    memset(ch_group, 0, sizeof(*ch_group));
    ch_group->main_enabled = true;
    ch_group->child_enabled = true;
    
    ch_group->next = main_config.ch_groups;
    main_config.ch_groups = ch_group;
    
    return ch_group;
}

ch_group_t* ch_group_find(homekit_characteristic_t* ch) {
    ch_group_t* ch_group = main_config.ch_groups;
    while (ch_group &&
           ch_group->ch0 != ch &&
           ch_group->ch1 != ch &&
           ch_group->ch2 != ch &&
           ch_group->ch3 != ch &&
           ch_group->ch4 != ch &&
           ch_group->ch5 != ch &&
           ch_group->ch6 != ch &&
           ch_group->ch7 != ch) {
        ch_group = ch_group->next;
    }

    return ch_group;
}

ch_group_t* ch_group_find_by_acc(uint8_t accessory) {
    ch_group_t* ch_group = main_config.ch_groups;
    while (ch_group &&
           ch_group->accessory != accessory) {
        ch_group = ch_group->next;
    }

    return ch_group;
}

lightbulb_group_t* lightbulb_group_find(homekit_characteristic_t* ch) {
    lightbulb_group_t* lightbulb_group = main_config.lightbulb_groups;
    while (lightbulb_group &&
           lightbulb_group->ch0 != ch) {
        lightbulb_group = lightbulb_group->next;
    }

    return lightbulb_group;
}

bool ping_host(char* host) {
    bool ping_result = false;
    
    ip_addr_t target_ip;
    const struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_RAW
    };
    struct addrinfo* res;

    int err = getaddrinfo(host, NULL, &hints, &res);

    if (err == 0 && res != NULL) {
        struct sockaddr* sa = res->ai_addr;
        if (sa->sa_family == AF_INET) {
            struct in_addr ipv4_inaddr = ((struct sockaddr_in*) sa)->sin_addr;
            memcpy(&target_ip, &ipv4_inaddr, sizeof(target_ip));
        }
#if LWIP_IPV6
        if (sa->sa_family == AF_INET6) {
            struct in_addr ipv6_inaddr = ((struct sockaddr_in6 *)sa)->sin6_addr;
            memcpy(&target_ip, &ipv6_inaddr, sizeof(target_ip));
        }
#endif
        ping_result = ping(target_ip);
    }
    
    if (res) {
        freeaddrinfo(res);
    }
    
    return ping_result;
}

inline void led_blink(const int blinks) {
    led_code(main_config.led_gpio, (blinking_params_t) { blinks, 0 });
}

void reboot_callback() {
    sdk_system_restart();
}

void reboot_haa() {
    led_blink(5);
    INFO("\nRebooting...\n");
    xTimerStart(xTimerCreate(0, pdMS_TO_TICKS(2900), pdFALSE, NULL, reboot_callback), XTIMER_BLOCK_TIME);
}

void resend_arp() {
    struct netif *netif = sdk_system_get_netif(STATION_IF);
    if (netif) {
        LOCK_TCPIP_CORE();
        etharp_gratuitous(netif);
        UNLOCK_TCPIP_CORE();
    }
}

void wifi_ping_task() {
    main_config.wifi_ping_is_running = true;
    
    struct ip_info info;
    bool ping_result = true;
    
    if (sdk_wifi_get_ip_info(STATION_IF, &info)) {
        char gw_host[16];
        snprintf(gw_host, 16, IPSTR, IP2STR(&info.gw));
        
        while (main_config.ping_is_running) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        ping_result = ping_host(gw_host);

        if (!ping_result) {
            main_config.wifi_error_count++;
            ERROR("GW %s ping (%i)", gw_host, main_config.wifi_error_count);
        }
    }

    if (ping_result) {
        main_config.wifi_error_count = 0;
    }
    
    main_config.wifi_ping_is_running = false;
    
    vTaskDelete(NULL);
}

void wifi_watchdog() {
    if (sdk_wifi_station_get_connect_status() == STATION_GOT_IP && main_config.wifi_error_count <= main_config.wifi_ping_max_errors) {
        uint8_t current_channel = sdk_wifi_get_channel();
        if (main_config.wifi_channel != current_channel) {
            xTimerStop(WIFI_WATCHDOG_TIMER, XTIMER_BLOCK_TIME);
            
            main_config.wifi_status = WIFI_STATUS_PRECONNECTED;
            INFO("Wifi new Ch%i", current_channel);
            main_config.wifi_channel = current_channel;
            resend_arp();
            homekit_mdns_announce();
            
            xTimerStart(WIFI_RECONNECTION_TIMER, XTIMER_BLOCK_TIME);
        }
        
        if (main_config.wifi_ping_max_errors != 255 && !main_config.wifi_ping_is_running && !homekit_is_pairing()) {
            if (xTaskCreate(wifi_ping_task, "wifi_ping_task", PING_TASK_SIZE, NULL, PING_TASK_PRIORITY, NULL) != pdPASS) {
                ERROR("Creating wifi_ping_task");
            }
        }
        
    } else {
        ERROR("Wifi error");
        xTimerStop(WIFI_WATCHDOG_TIMER, XTIMER_BLOCK_TIME);
        xTimerStart(WIFI_RECONNECTION_TIMER, XTIMER_BLOCK_TIME);
        main_config.wifi_error_count = 0;
        main_config.wifi_status = WIFI_STATUS_DISCONNECTED;
        sdk_wifi_station_disconnect();
    }
}

void wifi_reconnection() {
    if (main_config.wifi_status == WIFI_STATUS_DISCONNECTED) {
        main_config.wifi_status = WIFI_STATUS_CONNECTING_1;
        INFO("Wifi reconnecting...");
        wifi_config_connect();

    } else if (sdk_wifi_station_get_connect_status() == STATION_GOT_IP) {
        if (main_config.wifi_status == WIFI_STATUS_PRECONNECTED) {
            main_config.wifi_status = WIFI_STATUS_CONNECTED;
            main_config.wifi_channel = sdk_wifi_get_channel();
            INFO("mDNS reannounced");
            homekit_mdns_announce();
            
            xTimerStop(WIFI_RECONNECTION_TIMER, XTIMER_BLOCK_TIME);
            xTimerStart(WIFI_WATCHDOG_TIMER, XTIMER_BLOCK_TIME);
            
        } else {
            main_config.wifi_status = WIFI_STATUS_PRECONNECTED;
            INFO("Wifi reconnected");
            resend_arp();
            homekit_mdns_announce();
            
            main_config.wifi_error_count = 0;
            
            do_actions(ch_group_find_by_acc(ACC_TYPE_ROOT_DEVICE), 3);
        }
    
    } else if (main_config.wifi_status == WIFI_STATUS_CONNECTING_1) {
        main_config.wifi_status = WIFI_STATUS_CONNECTING_2;
        
    } else {
        sdk_wifi_station_disconnect();
        led_blink(8);
        ERROR("Wifi disconnected");

        main_config.wifi_status = WIFI_STATUS_DISCONNECTED;
        wifi_config_reset();
        
        main_config.wifi_error_count++;
        if (main_config.wifi_error_count < WIFI_ERROR_COUNT_REBOOT) {
            do_actions(ch_group_find_by_acc(ACC_TYPE_ROOT_DEVICE), 4);
        } else {
            main_config.wifi_error_count = 0;
            do_actions(ch_group_find_by_acc(ACC_TYPE_ROOT_DEVICE), 5);
        }
    }
}

ping_input_t* ping_input_find_by_host(char* host) {
    ping_input_t* ping_input = main_config.ping_inputs;
    while (ping_input &&
           strcmp(ping_input->host, host) != 0) {
        ping_input = ping_input->next;
    }

    return ping_input;
}

void ping_task() {
    INFO("Ping...");
    
    main_config.ping_is_running = true;

    void ping_input_run_callback_fn(ping_input_callback_fn_t* callbacks) {
        ping_input_callback_fn_t* ping_input_callback_fn = callbacks;
        
        while (ping_input_callback_fn) {
            if (!ping_input_callback_fn->disable_without_wifi ||
                (ping_input_callback_fn->disable_without_wifi && main_config.wifi_status >= WIFI_STATUS_PRECONNECTED)) {
                ping_input_callback_fn->callback(0, ping_input_callback_fn->ch_group, ping_input_callback_fn->param);
            }
            ping_input_callback_fn = ping_input_callback_fn->next;
        }
    }
    
    ping_input_t* ping_input = main_config.ping_inputs;
    while (ping_input) {
        bool ping_result = false;
        
        uint8_t i = 0;
        do {
            i++;
            
            ping_result = ping_host(ping_input->host);
    
            vTaskDelay(pdMS_TO_TICKS(10));
        } while (i < PING_RETRIES && !ping_result);
        
        if (ping_result && (!ping_input->last_response || ping_input->ignore_last_response)) {
            ping_input->last_response = true;
            INFO("Ping %s", ping_input->host);
            ping_input_run_callback_fn(ping_input->callback_1);

        } else if (!ping_result && (ping_input->last_response || ping_input->ignore_last_response)) {
            ping_input->last_response = false;
            ERROR("Ping %s", ping_input->host);
            ping_input_run_callback_fn(ping_input->callback_0);
        }
        
        ping_input = ping_input->next;
    }
    
    main_config.ping_is_running = false;
    
    vTaskDelete(NULL);
}

void ping_task_timer_worker() {
    if (!main_config.ping_is_running && !homekit_is_pairing()) {
        if (xTaskCreate(ping_task, "ping_task", PING_TASK_SIZE, NULL, PING_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("Creating ping_task");
        }
    }
}

// -----

void setup_mode_call(const uint8_t gpio, void* args, const uint8_t param) {
    INFO("Setup mode call");
    
    if (main_config.setup_mode_time == 0 || xTaskGetTickCountFromISR() < main_config.setup_mode_time * 1000 / portTICK_PERIOD_MS) {
        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 1);
        reboot_haa();
    } else {
        ERROR("Not allowed after %i secs since boot", main_config.setup_mode_time);
    }
}

void setup_mode_toggle_upcount() {
    if (main_config.setup_mode_toggle_counter_max > 0) {
        main_config.setup_mode_toggle_counter++;
        xTimerStart(main_config.setup_mode_toggle_timer, XTIMER_BLOCK_TIME);
    }
}

void setup_mode_toggle() {
    if (main_config.setup_mode_toggle_counter >= main_config.setup_mode_toggle_counter_max) {
        setup_mode_call(0, NULL, 0);
    }
    
    main_config.setup_mode_toggle_counter = 0;
}

// -----
void save_states() {
    INFO("Saving states");
    last_state_t* last_state = main_config.last_states;
    sysparam_status_t status;
    
    while (last_state) {
        switch (last_state->ch_type) {
            case CH_TYPE_INT8:
                status = sysparam_set_int8(last_state->id, last_state->ch->value.int_value);
                break;
                
            case CH_TYPE_INT32:
                status = sysparam_set_int32(last_state->id, last_state->ch->value.int_value);
                break;
                
            case CH_TYPE_FLOAT:
                status = sysparam_set_int32(last_state->id, last_state->ch->value.float_value * 100);
                break;
                
            case CH_TYPE_STRING:
                status = sysparam_set_string(last_state->id, last_state->ch->value.string_value);
                break;
                
            default:    // case CH_TYPE_BOOL
                status = sysparam_set_bool(last_state->id, last_state->ch->value.bool_value);
                break;
        }
        
        if (status != SYSPARAM_OK) {
            ERROR("Flash saving states");
        }
        
        last_state = last_state->next;
    }
}

inline void save_states_callback() {
    xTimerStart(main_config.save_states_timer, XTIMER_BLOCK_TIME);
}

void hkc_group_notify(ch_group_t* ch_group) {
    if (!ch_group->homekit_enabled) {
        return;
    }
    
    if (ch_group->ch0) {
        homekit_characteristic_notify(ch_group->ch0, ch_group->ch0->value);
    }
    
    if (ch_group->ch1) {
        homekit_characteristic_notify(ch_group->ch1, ch_group->ch1->value);
    }
    
    if (ch_group->ch2) {
        homekit_characteristic_notify(ch_group->ch2, ch_group->ch2->value);
        if (ch_group->ch3) {
            homekit_characteristic_notify(ch_group->ch3, ch_group->ch3->value);
            if (ch_group->ch4) {
                homekit_characteristic_notify(ch_group->ch4, ch_group->ch4->value);
                if (ch_group->ch5) {
                    homekit_characteristic_notify(ch_group->ch5, ch_group->ch5->value);
                    if (ch_group->ch6) {
                        homekit_characteristic_notify(ch_group->ch6, ch_group->ch6->value);
                        if (ch_group->ch7) {
                            homekit_characteristic_notify(ch_group->ch7, ch_group->ch7->value);
                        }
                    }
                }
            }
        }
    }
}

void hkc_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    INFO("Setter");
    ch->value = value;
    hkc_group_notify(ch_group_find(ch));
    
    save_states_callback();
}

void hkc_setter_with_setup(homekit_characteristic_t* ch, const homekit_value_t value) {
    hkc_setter(ch, value);
    
    setup_mode_toggle_upcount();
}

// --- ON
void hkc_on_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        if (ch->value.bool_value != value.bool_value) {
            led_blink(1);
            INFO("Setter ON");
            
            ch->value = value;
            
            do_actions(ch_group, (uint8_t) ch->value.bool_value);
            
            if (ch->value.bool_value && ch_group->num[0] > 0) {
                autooff_setter_params_t* autooff_setter_params = malloc(sizeof(autooff_setter_params_t));
                autooff_setter_params->ch = ch;
                autooff_setter_params->type = TYPE_ON;
                xTimerStart(xTimerCreate(0, pdMS_TO_TICKS(ch_group->num[0] * 1000), pdFALSE, (void*) autooff_setter_params, hkc_autooff_setter_task), XTIMER_BLOCK_TIME);
            }
            
            setup_mode_toggle_upcount();
            save_states_callback();
            
            if (ch_group->ch2) {
                if (value.bool_value) {
                    ch_group->ch2->value = ch_group->ch1->value;
                    xTimerStart(ch_group->timer, XTIMER_BLOCK_TIME);
                } else {
                    ch_group->ch2->value.int_value = 0;
                    xTimerStop(ch_group->timer, XTIMER_BLOCK_TIME);
                }
            }
        }
    }
    
    hkc_group_notify(ch_group);
}

void hkc_on_status_setter(homekit_characteristic_t* ch0, const homekit_value_t value) {
    if (ch0->value.bool_value != value.bool_value) {
        led_blink(1);
        INFO("Setter Status ON");
        ch0->value = value;
        
        hkc_group_notify(ch_group_find(ch0));
    }
}

void on_timer_worker(TimerHandle_t xTimer) {
    homekit_characteristic_t* ch = (homekit_characteristic_t*) pvTimerGetTimerID(xTimer);
    ch_group_t* ch_group = ch_group_find(ch);
    
    ch_group->ch2->value.int_value--;
    
    if (ch_group->ch2->value.int_value == 0) {
        xTimerStop(ch_group->timer, XTIMER_BLOCK_TIME);
        
        hkc_on_setter(ch, HOMEKIT_BOOL(false));
    }
}

// --- LOCK MECHANISM
void hkc_lock_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        if (ch->value.int_value != value.int_value) {
            led_blink(1);
            INFO("Setter LOCK");
            
            ch->value = value;
            
            uint8_t lock_index = 0;
            if (ch == ch_group->ch1) {
                ch_group->ch0->value = value;
                do_actions(ch_group, (uint8_t) ch->value.int_value);
            } else {
                ch_group->ch2->value = value;
                do_actions(ch_group, ((uint8_t) ch->value.int_value) + 2);
                lock_index = 1;
            }

            if (ch->value.int_value == 0 && ch_group->num[lock_index] > 0) {
                autooff_setter_params_t* autooff_setter_params = malloc(sizeof(autooff_setter_params_t));
                autooff_setter_params->ch = ch;
                autooff_setter_params->type = TYPE_LOCK;
                xTimerStart(xTimerCreate(0, pdMS_TO_TICKS(ch_group->num[lock_index] * 1000), pdFALSE, (void*) autooff_setter_params, hkc_autooff_setter_task), XTIMER_BLOCK_TIME);
            }
            
            setup_mode_toggle_upcount();
            save_states_callback();
        }
    }
    
    hkc_group_notify(ch_group);
}

void hkc_lock_status_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    if (ch->value.int_value != value.int_value) {
        led_blink(1);
        INFO("Setter Status LOCK");
        ch->value = value;
        
        ch_group_t* ch_group = ch_group_find(ch);
        
        if (ch == ch_group->ch1) {
            ch_group->ch0->value = value;
        } else {
            ch_group->ch2->value = value;
        }
        
        hkc_group_notify(ch_group);
    }
}

// --- BUTTON EVENT
void button_event(const uint8_t gpio, void* args, const uint8_t event_type) {
    ch_group_t* ch_group = args;
    
    if (ch_group->child_enabled) {
        led_blink(event_type + 1);
        INFO("Setter EVENT %i", event_type);
        
        homekit_characteristic_notify(ch_group->ch0, HOMEKIT_UINT8(event_type));
        
        do_actions(ch_group, event_type);
        
        setup_mode_toggle_upcount();
    }
}

// --- SENSORS
void sensor_1(const uint8_t gpio, void* args, const uint8_t type) {
    INFO("DigI Sensor GPIO %i", gpio);
    
    ch_group_t* ch_group = args;

    if (ch_group->main_enabled) {
        if ((type == TYPE_SENSOR &&
            ch_group->ch0->value.int_value == 0) ||
            (type == TYPE_SENSOR_BOOL &&
            ch_group->ch0->value.bool_value == false)) {
            led_blink(1);
            INFO("Sensor ON");
            
            if (type == TYPE_SENSOR) {
                ch_group->ch0->value = HOMEKIT_UINT8(1);
            } else {
                ch_group->ch0->value = HOMEKIT_BOOL(true);
            }

            do_actions(ch_group, 1);
            
            if (ch_group->num[0] > 0) {
                autooff_setter_params_t* autooff_setter_params = malloc(sizeof(autooff_setter_params_t));
                autooff_setter_params->ch = ch_group->ch0;
                autooff_setter_params->type = type;
                xTimerStart(xTimerCreate(0, pdMS_TO_TICKS(ch_group->num[0] * 1000), pdFALSE, (void*) autooff_setter_params, hkc_autooff_setter_task), XTIMER_BLOCK_TIME);
            }
        }
    }
    
    hkc_group_notify(ch_group);
}

void sensor_status_1(const uint8_t gpio, void* args, const uint8_t type) {
    INFO("DigI Sensor Status GPIO %i", gpio);
    
    ch_group_t* ch_group = args;

    if ((type == TYPE_SENSOR &&
        ch_group->ch0->value.int_value == 0) ||
        (type == TYPE_SENSOR_BOOL &&
        ch_group->ch0->value.bool_value == false)) {
        led_blink(1);
        INFO("Sensor Status ON");
        
        if (type == TYPE_SENSOR) {
            ch_group->ch0->value = HOMEKIT_UINT8(1);
        } else {
            ch_group->ch0->value = HOMEKIT_BOOL(true);
        }

        hkc_group_notify(ch_group);
    }
}

void sensor_0(const uint8_t gpio, void* args, const uint8_t type) {
    INFO("DigI Sensor GPIO %i", gpio);
    
    ch_group_t* ch_group = args;
    
    if (ch_group->main_enabled) {
        if ((type == TYPE_SENSOR &&
            ch_group->ch0->value.int_value == 1) ||
            (type == TYPE_SENSOR_BOOL &&
            ch_group->ch0->value.bool_value == true) ||
            ch_group->acc_type == ACC_TYPE_DOORBELL) {
            led_blink(1);
            INFO("Sensor OFF");
            
            if (type == TYPE_SENSOR) {
                ch_group->ch0->value = HOMEKIT_UINT8(0);
            } else {
                ch_group->ch0->value = HOMEKIT_BOOL(false);
            }
            
            do_actions(ch_group, 0);
        }
    }
    
    hkc_group_notify(ch_group);
}

void sensor_status_0(const uint8_t gpio, void* args, const uint8_t type) {
    INFO("DigI Sensor Status GPIO %i", gpio);
    
    ch_group_t* ch_group = args;

    if ((type == TYPE_SENSOR &&
        ch_group->ch0->value.int_value == 1) ||
        (type == TYPE_SENSOR_BOOL &&
        ch_group->ch0->value.bool_value == true) ||
        ch_group->acc_type == ACC_TYPE_DOORBELL) {
        led_blink(1);
        INFO("Sensor Status OFF");
        
        if (type == TYPE_SENSOR) {
            ch_group->ch0->value = HOMEKIT_UINT8(0);
        } else {
            ch_group->ch0->value = HOMEKIT_BOOL(false);
        }
        
        hkc_group_notify(ch_group);
    }
}

void power_monitor_task(void* args) {
    ch_group_t* ch_group = args;
    
    float voltage = 0;
    float current = 0;
    float power = 0;
    
    if (PM_SENSOR_TYPE == 2) {
        // TODO
        
    } else {
        voltage = adv_hlw_get_voltage_freq((uint8_t) PM_SENSOR_HLW_GPIO);
        current = adv_hlw_get_current_freq((uint8_t) PM_SENSOR_HLW_GPIO);
        power = adv_hlw_get_power_freq((uint8_t) PM_SENSOR_HLW_GPIO);
    }
    
    voltage = (PM_VOLTAGE_FACTOR * voltage) + PM_VOLTAGE_OFFSET;
    current = (PM_CURRENT_FACTOR * current) + PM_CURRENT_OFFSET;
    
    if (voltage < 0) {
        voltage = 0;
    }
    
    if (PM_SENSOR_TYPE < 2.f && PM_SENSOR_HLW_GPIO_CF == PM_SENSOR_HLW_GPIO_DEFAULT) {
        power = voltage * current;
    }
    
    power = (PM_POWER_FACTOR * power) + PM_POWER_OFFSET;
    
    INFO("PM Acc %i: V = %g, C = %g, P = %g", ch_group->accessory, voltage, current, power);
    
    if (PM_SENSOR_TYPE < 2.f) {
        if (current < 0) {
            current = 0;
        }
        
        if (power < 0) {
            power = 0;
        }
    }
    
    if (voltage < 750.f) {
        do_wildcard_actions(ch_group, 0, voltage);
        do_wildcard_actions(ch_group, 1, current);
        do_wildcard_actions(ch_group, 2, power);
        
        if (voltage != ch_group->ch1->value.float_value ||
            current != ch_group->ch2->value.float_value ||
            power != ch_group->ch3->value.float_value) {
            
            ch_group->ch1->value = HOMEKIT_FLOAT(voltage);
            ch_group->ch2->value = HOMEKIT_FLOAT(current);
            ch_group->ch3->value = HOMEKIT_FLOAT(power);
            
            hkc_group_notify(ch_group);
        }
    } else {
        ERROR("PM Read");
    }
    
    vTaskDelete(NULL);
}

void power_monitor_timer_worker(TimerHandle_t xTimer) {
    if (!homekit_is_pairing()) {
        void* args = (void*) pvTimerGetTimerID(xTimer);
        if (xTaskCreate(power_monitor_task, "power_monitor", POWER_MONITOR_TASK_SIZE, args, POWER_MONITOR_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("Creating power_monitor_task");
        }
    }
}

// --- WATER VALVE
void hkc_valve_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        if (ch->value.int_value != value.int_value) {
            led_blink(1);
            INFO("Setter VALVE");
            
            ch->value = value;
            ch_group->ch1->value = value;
            
            do_actions(ch_group, (uint8_t) ch->value.int_value);
            
            if (ch->value.int_value == 1 && ch_group->num[0] > 0) {
                autooff_setter_params_t* autooff_setter_params = malloc(sizeof(autooff_setter_params_t));
                autooff_setter_params->ch = ch;
                autooff_setter_params->type = TYPE_VALVE;
                xTimerStart(xTimerCreate(0, pdMS_TO_TICKS(ch_group->num[0] * 1000), pdFALSE, (void*) autooff_setter_params, hkc_autooff_setter_task), XTIMER_BLOCK_TIME);
            }
            
            setup_mode_toggle_upcount();
            save_states_callback();
            
            if (ch_group->ch3) {
                if (value.int_value == 0) {
                    ch_group->ch3->value.int_value = 0;
                    xTimerStop(ch_group->timer, XTIMER_BLOCK_TIME);
                } else {
                    ch_group->ch3->value = ch_group->ch2->value;
                    xTimerStart(ch_group->timer, XTIMER_BLOCK_TIME);
                }
            }
        }
    }
    
    hkc_group_notify(ch_group);
}

void hkc_valve_status_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    if (ch->value.int_value != value.int_value) {
        led_blink(1);
        INFO("Setter Status VALVE");
        
        ch->value = value;
        ch_group_t* ch_group = ch_group_find(ch);
        ch_group->ch1->value = value;
        
        hkc_group_notify(ch_group);
    }
}

void valve_timer_worker(TimerHandle_t xTimer) {
    homekit_characteristic_t* ch = (homekit_characteristic_t*) pvTimerGetTimerID(xTimer);
    ch_group_t* ch_group = ch_group_find(ch);
    
    ch_group->ch3->value.int_value--;
    
    if (ch_group->ch3->value.int_value == 0) {
        xTimerStop(ch_group->timer, XTIMER_BLOCK_TIME);
        
        hkc_valve_setter(ch, HOMEKIT_UINT8(0));
    }
}

// --- THERMOSTAT
void process_th(void* args) {
    ch_group_t* ch_group = args;
    
    void heating(const float deadband, const float deadband_soft_on, const float deadband_force_idle) {
        INFO("TH Heater");
        if (SENSOR_TEMPERATURE_FLOAT < (TH_HEATER_TARGET_TEMP_FLOAT - deadband - deadband_soft_on)) {
            THERMOSTAT_MODE_INT = THERMOSTAT_MODE_HEATER;
            if (ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_HEATER_ON) {
                ch_group->last_wildcard_action[2] = THERMOSTAT_ACTION_HEATER_ON;
                do_actions(ch_group, THERMOSTAT_ACTION_HEATER_ON);
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT < (TH_HEATER_TARGET_TEMP_FLOAT - deadband)) {
            THERMOSTAT_MODE_INT = THERMOSTAT_MODE_HEATER;
            if (ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_HEATER_SOFT_ON) {
                ch_group->last_wildcard_action[2] = THERMOSTAT_ACTION_HEATER_SOFT_ON;
                do_actions(ch_group, THERMOSTAT_ACTION_HEATER_SOFT_ON);
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT < (TH_HEATER_TARGET_TEMP_FLOAT + deadband)) {
            if (THERMOSTAT_MODE_INT == THERMOSTAT_MODE_HEATER) {
                if (TH_DEADBAND_SOFT_ON > 0.000f) {
                    if (ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_HEATER_SOFT_ON) {
                        ch_group->last_wildcard_action[2] = THERMOSTAT_ACTION_HEATER_SOFT_ON;
                        do_actions(ch_group, THERMOSTAT_ACTION_HEATER_SOFT_ON);
                    }
                } else {
                    if (ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_HEATER_ON) {
                        ch_group->last_wildcard_action[2] = THERMOSTAT_ACTION_HEATER_ON;
                        do_actions(ch_group, THERMOSTAT_ACTION_HEATER_ON);
                    }
                }
                
            } else {
                THERMOSTAT_MODE_INT = THERMOSTAT_MODE_IDLE;
                if (ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_HEATER_IDLE) {
                    ch_group->last_wildcard_action[2] = THERMOSTAT_ACTION_HEATER_IDLE;
                    do_actions(ch_group, THERMOSTAT_ACTION_HEATER_IDLE);
                }
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT >= (TH_HEATER_TARGET_TEMP_FLOAT + deadband + deadband_force_idle) &&
                   TH_DEADBAND_FORCE_IDLE > 0.000f) {
            THERMOSTAT_MODE_INT = THERMOSTAT_MODE_IDLE;
            if (ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_HEATER_FORCE_IDLE) {
                ch_group->last_wildcard_action[2] = THERMOSTAT_ACTION_HEATER_FORCE_IDLE;
                do_actions(ch_group, THERMOSTAT_ACTION_HEATER_FORCE_IDLE);
            }
            
        } else {
            THERMOSTAT_MODE_INT = THERMOSTAT_MODE_IDLE;
            if (TH_DEADBAND_FORCE_IDLE == 0.000f ||
                ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_HEATER_FORCE_IDLE) {
                if (ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_HEATER_IDLE) {
                    ch_group->last_wildcard_action[2] = THERMOSTAT_ACTION_HEATER_IDLE;
                    do_actions(ch_group, THERMOSTAT_ACTION_HEATER_IDLE);
                }
            }
        }
    }
    
    void cooling(const float deadband, const float deadband_soft_on, const float deadband_force_idle) {
        INFO("TH Cooler");
        if (SENSOR_TEMPERATURE_FLOAT > (TH_COOLER_TARGET_TEMP_FLOAT + deadband + deadband_soft_on)) {
            THERMOSTAT_MODE_INT = THERMOSTAT_MODE_COOLER;
            if (ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_COOLER_ON) {
                ch_group->last_wildcard_action[2] = THERMOSTAT_ACTION_COOLER_ON;
                do_actions(ch_group, THERMOSTAT_ACTION_COOLER_ON);
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT > (TH_COOLER_TARGET_TEMP_FLOAT + deadband)) {
            THERMOSTAT_MODE_INT = THERMOSTAT_MODE_COOLER;
            if (ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_COOLER_SOFT_ON) {
                ch_group->last_wildcard_action[2] = THERMOSTAT_ACTION_COOLER_SOFT_ON;
                do_actions(ch_group, THERMOSTAT_ACTION_COOLER_SOFT_ON);
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT > (TH_COOLER_TARGET_TEMP_FLOAT - deadband)) {
            if (THERMOSTAT_MODE_INT == THERMOSTAT_MODE_COOLER) {
                if (TH_DEADBAND_SOFT_ON > 0.000f) {
                    if (ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_COOLER_SOFT_ON) {
                        ch_group->last_wildcard_action[2] = THERMOSTAT_ACTION_COOLER_SOFT_ON;
                        do_actions(ch_group, THERMOSTAT_ACTION_COOLER_SOFT_ON);
                    }
                } else {
                    if (ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_COOLER_ON) {
                        ch_group->last_wildcard_action[2] = THERMOSTAT_ACTION_COOLER_ON;
                        do_actions(ch_group, THERMOSTAT_ACTION_COOLER_ON);
                    }
                }
                
            } else {
                THERMOSTAT_MODE_INT = THERMOSTAT_MODE_IDLE;
                if (ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_COOLER_IDLE) {
                    ch_group->last_wildcard_action[2] = THERMOSTAT_ACTION_COOLER_IDLE;
                    do_actions(ch_group, THERMOSTAT_ACTION_COOLER_IDLE);
                }
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT <= (TH_COOLER_TARGET_TEMP_FLOAT - deadband - deadband_force_idle) &&
                   TH_DEADBAND_FORCE_IDLE > 0.000f) {
            THERMOSTAT_MODE_INT = THERMOSTAT_MODE_IDLE;
            if (ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_COOLER_FORCE_IDLE) {
                ch_group->last_wildcard_action[2] = THERMOSTAT_ACTION_COOLER_FORCE_IDLE;
                do_actions(ch_group, THERMOSTAT_ACTION_COOLER_FORCE_IDLE);
            }
            
        } else {
            THERMOSTAT_MODE_INT = THERMOSTAT_MODE_IDLE;
            if (TH_DEADBAND_FORCE_IDLE == 0.000f ||
                ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_COOLER_FORCE_IDLE) {
                if (ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_COOLER_IDLE) {
                    ch_group->last_wildcard_action[2] = THERMOSTAT_ACTION_COOLER_IDLE;
                    do_actions(ch_group, THERMOSTAT_ACTION_COOLER_IDLE);
                }
            }
        }
    }
    
    if (ch_group->ch2->value.int_value) {
        if (ch_group->ch5->value.int_value == THERMOSTAT_TARGET_MODE_HEATER) {
            heating(TH_DEADBAND, TH_DEADBAND_SOFT_ON, TH_DEADBAND_FORCE_IDLE);
                    
        } else if (ch_group->ch5->value.int_value == THERMOSTAT_TARGET_MODE_COOLER) {
            cooling(TH_DEADBAND, TH_DEADBAND_SOFT_ON, TH_DEADBAND_FORCE_IDLE);
            
        } else {    // THERMOSTAT_TARGET_MODE_AUTO
            const float mid_target_temp = (TH_HEATER_TARGET_TEMP_FLOAT + TH_COOLER_TARGET_TEMP_FLOAT) / 2.000f;
            
            bool is_heater = false;
            if (THERMOSTAT_MODE_INT == THERMOSTAT_MODE_OFF) {
                if (SENSOR_TEMPERATURE_FLOAT < mid_target_temp) {
                    is_heater = true;
                }
            } else if (SENSOR_TEMPERATURE_FLOAT <= TH_HEATER_TARGET_TEMP_FLOAT) {
                is_heater = true;
            } else if (SENSOR_TEMPERATURE_FLOAT < TH_COOLER_TARGET_TEMP_FLOAT &&
                       (ch_group->last_wildcard_action[2] == THERMOSTAT_ACTION_HEATER_IDLE ||
                       ch_group->last_wildcard_action[2] == THERMOSTAT_ACTION_HEATER_ON ||
                       ch_group->last_wildcard_action[2] == THERMOSTAT_ACTION_HEATER_FORCE_IDLE ||
                       ch_group->last_wildcard_action[2] == THERMOSTAT_ACTION_HEATER_SOFT_ON)) {
                is_heater = true;
            }
            
            const float th_deadband_force_idle = TH_COOLER_TARGET_TEMP_FLOAT - mid_target_temp;
            const float th_deadband = th_deadband_force_idle / 1.500f;
            
            if (is_heater) {
                heating(th_deadband, th_deadband_force_idle - th_deadband, th_deadband_force_idle);
            } else {
                cooling(th_deadband, th_deadband_force_idle - th_deadband, th_deadband_force_idle);
            }
        }
        
    } else {
        INFO("TH Off");
        THERMOSTAT_MODE_INT = THERMOSTAT_MODE_OFF;
        if (ch_group->last_wildcard_action[2] != THERMOSTAT_ACTION_TOTAL_OFF) {
            ch_group->last_wildcard_action[2] = THERMOSTAT_ACTION_TOTAL_OFF;
            do_actions(ch_group, THERMOSTAT_ACTION_TOTAL_OFF);
        }
    }
    
    hkc_group_notify(ch_group);
    
    save_states_callback();
}

void process_th_timer(TimerHandle_t xTimer) {
    ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
    process_th(ch_group);
}

void update_th(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("Setter TH");
        
        ch->value = value;
        
        xTimerStart(ch_group->timer2, XTIMER_BLOCK_TIME);
        
    } else {
        hkc_group_notify(ch_group);
    }
}

void hkc_th_target_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    setup_mode_toggle_upcount();
    
    update_th(ch, value);
}

void th_input(const uint8_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;

    if (ch_group->child_enabled) {
        switch (type) {
            case 0:
                ch_group->ch2->value.int_value = 0;
                break;
                
            case 1:
                ch_group->ch2->value.int_value = 1;
                break;
                
            case 5:
                ch_group->ch2->value.int_value = 1;
                ch_group->ch5->value.int_value = 2;
                break;
                
            case 6:
                ch_group->ch2->value.int_value = 1;
                ch_group->ch5->value.int_value = 1;
                break;
                
            case 7:
                ch_group->ch2->value.int_value = 1;
                ch_group->ch5->value.int_value = 0;
                break;
                
            default:    // case 9:  // Cyclic
                if (ch_group->ch2->value.int_value) {
                    if (TH_TYPE == THERMOSTAT_TYPE_HEATERCOOLER) {
                        if (ch_group->ch5->value.int_value > 0) {
                            ch_group->ch5->value.int_value--;
                        } else {
                            ch_group->ch2->value.int_value = 0;
                        }
                    } else {
                        ch_group->ch2->value.int_value = 0;
                    }
                } else {
                    ch_group->ch2->value.int_value = 1;
                    if (TH_TYPE == THERMOSTAT_TYPE_HEATERCOOLER) {
                        ch_group->ch5->value.int_value = THERMOSTAT_TARGET_MODE_COOLER;
                    }
                }
                break;
        }
        
        hkc_th_target_setter(ch_group->ch0, ch_group->ch0->value);
    }
}

void th_input_temp(const uint8_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;

    if (ch_group->child_enabled) {
        float set_h_temp = ch_group->ch6->value.float_value;
        float set_c_temp = ch_group->ch7->value.float_value;
        
        if (type == THERMOSTAT_TEMP_UP) {
            set_h_temp += 0.5;
            set_c_temp += 0.5;
            if (set_h_temp > TH_MAX_TEMP) {
                set_h_temp = TH_MAX_TEMP;
            }
            if (set_c_temp > TH_MAX_TEMP) {
                set_c_temp = TH_MAX_TEMP;
            }
        } else {    // type == THERMOSTAT_TEMP_DOWN
            set_h_temp -= 0.5;
            set_c_temp -= 0.5;
            if (set_h_temp < TH_MIN_TEMP) {
                set_h_temp = TH_MIN_TEMP;
            }
            if (set_c_temp < TH_MIN_TEMP) {
                set_c_temp = TH_MIN_TEMP;
            }
        }
        
        ch_group->ch6->value.float_value = set_h_temp;
        ch_group->ch7->value.float_value = set_c_temp;
        
        update_th(ch_group->ch0, ch_group->ch0->value);
        
        // Extra actions
        if (type == THERMOSTAT_TEMP_UP) {
            do_actions(ch_group, THERMOSTAT_ACTION_TEMP_UP);
        } else {    // type == THERMOSTAT_TEMP_DOWN
            do_actions(ch_group, THERMOSTAT_ACTION_TEMP_DOWN);
        }
    }
}

// --- TEMPERATURE
void temperature_task(void* args) {
    INFO("Read TH sensor");
    
    float taylor_log(float x) {
        // https://stackoverflow.com/questions/46879166/finding-the-natural-logarithm-of-a-number-using-taylor-series-in-c
        if (x <= 0.0) {
            return x;
        }
        
        float z = (x + 1) / (x - 1);
        const float step = ((x - 1) * (x - 1)) / ((x + 1) * (x + 1));
        float totalValue = 0;
        float powe = 1;
        for (uint8_t i = 0; i < 10; i++) {
            z *= step;
            const float y = (1 / powe) * z;
            totalValue = totalValue + y;
            powe = powe + 2;
        }
        
        totalValue *= 2;
        
        return totalValue;
    }
    
    ch_group_t* ch_group = args;
    
    float humidity_value, temperature_value;
    bool get_temp = false;
    
    if (TH_SENSOR_TYPE != 3 && TH_SENSOR_TYPE < 5) {
        dht_sensor_type_t current_sensor_type = DHT_TYPE_DHT22; // TH_SENSOR_TYPE == 2
        
        if (TH_SENSOR_TYPE == 1) {
            current_sensor_type = DHT_TYPE_DHT11;
        } else if (TH_SENSOR_TYPE == 4) {
            current_sensor_type = DHT_TYPE_SI7021;
        }
        
        get_temp = dht_read_float_data(current_sensor_type, TH_SENSOR_GPIO, &humidity_value, &temperature_value);
        
    } else if (TH_SENSOR_TYPE == 3) {
        ds18b20_addr_t ds18b20_addr[(uint8_t) TH_SENSOR_INDEX];
        
        if (ds18b20_scan_devices(TH_SENSOR_GPIO, ds18b20_addr, (uint8_t) TH_SENSOR_INDEX) >= (uint8_t) TH_SENSOR_INDEX) {
            float temps[1];
            ds18b20_addr_t ds18b20_addr_single[1];
            ds18b20_addr_single[0] = ds18b20_addr[((uint8_t) TH_SENSOR_INDEX) - 1];
            ds18b20_measure_and_read_multi(TH_SENSOR_GPIO, ds18b20_addr_single, 1, temps);
            temperature_value = temps[0];
            humidity_value = 0.0;
            get_temp = true;
        }
        
    } else {
        float adc = sdk_system_adc_read();
        if (TH_SENSOR_TYPE == 5) {
            // https://github.com/arendst/Tasmota/blob/7177c7d8e003bb420d8cae39f544c2b8a9af09fe/tasmota/xsns_02_analog.ino#L201
            temperature_value = KELVIN_TO_CELSIUS(3350 / (3350 / 298.15 + taylor_log(((32000 * adc) / ((1024 * 3.3) - adc)) / 10000))) - 15;
            
        } else if (TH_SENSOR_TYPE == 6) {
            temperature_value = KELVIN_TO_CELSIUS(3350 / (3350 / 298.15 - taylor_log(((32000 * adc) / ((1024 * 3.3) - adc)) / 10000))) + 15;
            
        } else if (TH_SENSOR_TYPE == 7) {
            temperature_value = 1024 - adc;
            
        } else {    // TH_SENSOR_TYPE == 8
            temperature_value = adc;
        }
        
        if (TH_SENSOR_HUM_OFFSET != 0.000000f) {
            temperature_value *= TH_SENSOR_HUM_OFFSET;
        }
        
        get_temp = true;
    }
    
    /*
     * Only for tests. Keep comment for releases
     */
    //get_temp = true; temperature_value = 23;
    
    if (get_temp) {
        TH_SENSOR_ERROR_COUNT = 0;
        
        if (ch_group->ch0) {
            temperature_value += TH_SENSOR_TEMP_OFFSET;
            if (temperature_value < -100) {
                temperature_value = -100;
            } else if (temperature_value > 200) {
                temperature_value = 200;
            }
            
            INFO("TEMP %g", temperature_value);
            
            if (temperature_value != ch_group->ch0->value.float_value) {
                ch_group->ch0->value = HOMEKIT_FLOAT(temperature_value);
                
                if (ch_group->ch5) {
                    update_th(ch_group->ch0, ch_group->ch0->value);
                }
                
                do_wildcard_actions(ch_group, 0, temperature_value);
            }
        }
        
        if (ch_group->ch1) {
            humidity_value += TH_SENSOR_HUM_OFFSET;
            if (humidity_value < 0) {
                humidity_value = 0;
            } else if (humidity_value > 100) {
                humidity_value = 100;
            }

            INFO("HUM %g", humidity_value);
            
            if (humidity_value != ch_group->ch1->value.float_value) {
                ch_group->ch1->value = HOMEKIT_FLOAT(humidity_value);
                
                do_wildcard_actions(ch_group, 1, humidity_value);
            }
        }
        
    } else {
        led_blink(5);
        ERROR("Sensor");
        
        TH_SENSOR_ERROR_COUNT++;

        if ((uint8_t) TH_SENSOR_ERROR_COUNT > TH_SENSOR_MAX_ALLOWED_ERRORS) {
            TH_SENSOR_ERROR_COUNT = 0;
            
            ch_group->ch0->value.float_value = 0;
            if (ch_group->ch1) {
                ch_group->ch1->value.float_value = 0;
            }

            do_actions(ch_group, THERMOSTAT_ACTION_SENSOR_ERROR);
        }

    }
    
    hkc_group_notify(ch_group);
    
    vTaskDelete(NULL);
}

void temperature_timer_worker(TimerHandle_t xTimer) {
    if (!homekit_is_pairing()) {
        void* args = (void*) pvTimerGetTimerID(xTimer);
        if (xTaskCreate(temperature_task, "temperature", TEMPERATURE_TASK_SIZE, args, TEMPERATURE_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("Creating temperature_task");
        }
    }
}

// --- LIGHTBULBS
// https://gist.github.com/rasod/42eab9206e28ca91c8d9f926fa71a938
float white_factor(const float r, const float g, const uint16_t b, const float w_r, const float w_g, const float w_b, const float rgb_min, const float rgb_max) {
    float r_f = 1;
    float g_f = 1;
    float b_f = 1;
    float w_f = 0;
    
    if (rgb_max >= 1) {
        const float w_min = MIN(MIN(w_r, w_g), w_b);

        if (w_r > w_min) {
            r_f = (r - rgb_min) / (w_g - w_min);
        }

        if (w_g > w_min) {
            g_f = (g - rgb_min) / (w_g - w_min);
        }
        
        if (w_b > w_min) {
            b_f = (b - rgb_min) / (w_b - w_min);
        }

        w_f = MIN(MAX(0, MIN(MIN(r_f, g_f), b_f)), 1);
    }

    if (w_r > 0) {
        r_f = r / w_r;
    }
    
    if (w_g > 0) {
        g_f = g / w_g;
    }
    
    if (w_b > 0) {
        b_f = b / w_b;
    }

    return MIN(MAX(0, MIN(MIN(MIN(r_f, g_f), b_f), w_f)), 1);
}

//https://github.com/espressif/esp-idf/examples/peripherals/rmt/led_strip/main/led_strip_main.c
void hsi2rgbw(uint16_t h, uint16_t s, uint16_t v, lightbulb_group_t* lightbulb_group) {
    h %= 360; // h -> [0,360]
    const uint32_t rgb_max = v * (PWM_SCALE / 100.00f);
    const uint32_t rgb_min = rgb_max * (100 - s) / 100.f;

    const uint32_t i = h / 60;
    const uint32_t diff = h % 60;

    const uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    uint32_t r, g, b;
    
    INFO("light switch %i", i);
    switch (i) {
        case 0:
            r = rgb_max;
            g = rgb_min + rgb_adj;
            b = rgb_min;
            break;
        case 1:
            r = rgb_max - rgb_adj;
            g = rgb_max;
            b = rgb_min;
            break;
        case 2:
            r = rgb_min;
            g = rgb_max;
            b = rgb_min + rgb_adj;
            break;
        case 3:
            r = rgb_min;
            g = rgb_max - rgb_adj;
            b = rgb_max;
            break;
        case 4:
            r = rgb_min + rgb_adj;
            g = rgb_min;
            b = rgb_max;
            break;
        default:    // case 5:
            r = rgb_max;
            g = rgb_min;
            b = rgb_max - rgb_adj;
            break;
    }

    float cw_f = white_factor(r, g, b, CW_RED, CW_GREEN, CW_BLUE, rgb_min, rgb_max);
    float ww_f = white_factor(r, g, b, WW_RED, WW_GREEN, WW_BLUE, rgb_min, rgb_max);

    const float white_f = ww_f - cw_f;
    if (white_f > 1) {
        cw_f /= white_f;
        ww_f /= white_f;
    }
    
    const uint32_t cw = cw_f * PWM_SCALE;
    const uint32_t ww = ww_f * PWM_SCALE;
    
    lightbulb_group->target_r  = lightbulb_group->factor_r  * ((r  > PWM_SCALE) ? PWM_SCALE : r);
    lightbulb_group->target_g  = lightbulb_group->factor_g  * ((g  > PWM_SCALE) ? PWM_SCALE : g);
    lightbulb_group->target_b  = lightbulb_group->factor_b  * ((b  > PWM_SCALE) ? PWM_SCALE : b);
    lightbulb_group->target_w  = lightbulb_group->factor_w  * ((rgb_min > PWM_SCALE) ? PWM_SCALE : rgb_min);
    lightbulb_group->target_cw = lightbulb_group->factor_cw * ((cw > PWM_SCALE) ? PWM_SCALE : cw);
    lightbulb_group->target_ww = lightbulb_group->factor_ww * ((ww > PWM_SCALE) ? PWM_SCALE : ww);
}

void multipwm_set_all() {
    multipwm_stop(main_config.pwm_info);
    for (uint8_t i = 0; i < main_config.pwm_info->channels; i++) {
        multipwm_set_duty(main_config.pwm_info, i, main_config.multipwm_duty[i]);
    }
    multipwm_start(main_config.pwm_info);
}

void rgbw_set_timer_worker() {
    if (!main_config.setpwm_bool_semaphore) {
        main_config.setpwm_bool_semaphore = true;
        
        uint8_t channels_to_set = 0; 
        
        if(main_config.pwm_info != NULL)
        {
            channels_to_set = main_config.pwm_info->channels;
        }
        else 
        {
            channels_to_set = main_config.driver_interface->descriptor->numChannelsPerChip;
        }
        uint8_t num_channels = channels_to_set; 

        lightbulb_group_t* lightbulb_group = main_config.lightbulb_groups;
        
        while (main_config.setpwm_is_running && lightbulb_group) {
            if (lightbulb_group->pwm_r != 255) {
                if (lightbulb_group->target_r - main_config.multipwm_duty[lightbulb_group->pwm_r] >= lightbulb_group->step) {
                    main_config.multipwm_duty[lightbulb_group->pwm_r] += lightbulb_group->step;
                } else if (main_config.multipwm_duty[lightbulb_group->pwm_r] - lightbulb_group->target_r >= lightbulb_group->step) {
                    main_config.multipwm_duty[lightbulb_group->pwm_r] -= lightbulb_group->step;
                } else {
                    main_config.multipwm_duty[lightbulb_group->pwm_r] = lightbulb_group->target_r;
                    channels_to_set--;
                }
            }
            
            if (lightbulb_group->pwm_g != 255) {
                if (lightbulb_group->target_g - main_config.multipwm_duty[lightbulb_group->pwm_g] >= lightbulb_group->step) {
                    main_config.multipwm_duty[lightbulb_group->pwm_g] += lightbulb_group->step;
                } else if (main_config.multipwm_duty[lightbulb_group->pwm_g] - lightbulb_group->target_g >= lightbulb_group->step) {
                    main_config.multipwm_duty[lightbulb_group->pwm_g] -= lightbulb_group->step;
                } else {
                    main_config.multipwm_duty[lightbulb_group->pwm_g] = lightbulb_group->target_g;
                    channels_to_set--;
                }
            }
            
            if (lightbulb_group->pwm_b != 255) {
                if (lightbulb_group->target_b - main_config.multipwm_duty[lightbulb_group->pwm_b] >= lightbulb_group->step) {
                    main_config.multipwm_duty[lightbulb_group->pwm_b] += lightbulb_group->step;
                } else if (main_config.multipwm_duty[lightbulb_group->pwm_b] - lightbulb_group->target_b >= lightbulb_group->step) {
                    main_config.multipwm_duty[lightbulb_group->pwm_b] -= lightbulb_group->step;
                } else {
                    main_config.multipwm_duty[lightbulb_group->pwm_b] = lightbulb_group->target_b;
                    channels_to_set--;
                }
            }
            
            if (lightbulb_group->pwm_w != 255) {
                if (lightbulb_group->target_w - main_config.multipwm_duty[lightbulb_group->pwm_w] >= lightbulb_group->step) {
                    main_config.multipwm_duty[lightbulb_group->pwm_w] += lightbulb_group->step;
                } else if (main_config.multipwm_duty[lightbulb_group->pwm_w] - lightbulb_group->target_w >= lightbulb_group->step) {
                    main_config.multipwm_duty[lightbulb_group->pwm_w] -= lightbulb_group->step;
                } else {
                    main_config.multipwm_duty[lightbulb_group->pwm_w] = lightbulb_group->target_w;
                    channels_to_set--;
                }
            }
            
            if (lightbulb_group->pwm_cw != 255) {
                if (lightbulb_group->target_cw - main_config.multipwm_duty[lightbulb_group->pwm_cw] >= lightbulb_group->step) {
                    main_config.multipwm_duty[lightbulb_group->pwm_cw] += lightbulb_group->step;
                } else if (main_config.multipwm_duty[lightbulb_group->pwm_cw] - lightbulb_group->target_cw >= lightbulb_group->step) {
                    main_config.multipwm_duty[lightbulb_group->pwm_cw] -= lightbulb_group->step;
                } else {
                    main_config.multipwm_duty[lightbulb_group->pwm_cw] = lightbulb_group->target_cw;
                    channels_to_set--;
                }
            }
            
            if (lightbulb_group->pwm_ww != 255) {
                if (lightbulb_group->target_ww - main_config.multipwm_duty[lightbulb_group->pwm_ww] >= lightbulb_group->step) {
                    main_config.multipwm_duty[lightbulb_group->pwm_ww] += lightbulb_group->step;
                } else if (main_config.multipwm_duty[lightbulb_group->pwm_ww] - lightbulb_group->target_ww >= lightbulb_group->step) {
                    main_config.multipwm_duty[lightbulb_group->pwm_ww] -= lightbulb_group->step;
                } else {
                    main_config.multipwm_duty[lightbulb_group->pwm_ww] = lightbulb_group->target_ww;
                    channels_to_set--;
                }
            }
            
            //INFO("RGBW-CW-WW -> %i, %i, %i, %i, %i, %i", main_config.multipwm_duty[lightbulb_group->pwm_r], main_config.multipwm_duty[lightbulb_group->pwm_g], main_config.multipwm_duty[lightbulb_group->pwm_b], main_config.multipwm_duty[lightbulb_group->pwm_w], main_config.multipwm_duty[lightbulb_group->pwm_cw], main_config.multipwm_duty[lightbulb_group->pwm_ww]);

            if (channels_to_set == 0) {
                main_config.setpwm_is_running = false;
                xTimerStop(main_config.pwm_timer, XTIMER_BLOCK_TIME);
                
                INFO("Color fixed");
                for (uint8_t i = 0; i < num_channels; i++) {
                    INFO("PWM Ch %i = %i", i, main_config.multipwm_duty[i]);
                }
            }
            
            lightbulb_group = lightbulb_group->next;
        }
        if(main_config.pwm_info != NULL)
        {
            INFO("WWWW 1\n");
            multipwm_set_all();
        } 
        else
        {            
            INFO("WWWW 2\n");
            haa_lighbulb_driver_set_all(main_config.driver_interface, main_config.multipwm_duty);
        }
        

        main_config.setpwm_bool_semaphore = false;
    } else {
        ERROR("MISSED Color set");
    }
}

void hkc_rgbw_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (!ch_group->main_enabled) {
        hkc_group_notify(ch_group);
        
    } else if (ch != ch_group->ch0 || value.bool_value != ch_group->ch0->value.bool_value) {
        lightbulb_group_t* lightbulb_group = lightbulb_group_find(ch_group->ch0);
        
        ch->value = value;
        
        if (ch_group->ch0->value.bool_value) {
            if (lightbulb_group->target_r == 0 &&
                lightbulb_group->target_g == 0 &&
                lightbulb_group->target_b == 0 &&
                lightbulb_group->target_w == 0 &&
                lightbulb_group->target_cw == 0 &&
                lightbulb_group->target_ww == 0) {
                setup_mode_toggle_upcount();
            }
            
            if (lightbulb_group->pwm_r != 255) {            // RGB, RGB-W, RGB-CW-WW, RGB-W-CW-WW
                hsi2rgbw(ch_group->ch2->value.float_value, ch_group->ch3->value.float_value, ch_group->ch1->value.int_value, lightbulb_group);
                
            } else if (lightbulb_group->pwm_b != 255) {     // Custom Color Temperature
                uint16_t target_color = 0;
                
                if (ch_group->ch2->value.int_value >= COLOR_TEMP_MAX - 5) {
                    target_color = PWM_SCALE;
                    
                } else if (ch_group->ch2->value.int_value > COLOR_TEMP_MIN + 1) { // Conversion based on @seritos curve
                    target_color = PWM_SCALE * (((0.09 + sqrt(0.18 + (0.1352 * (ch_group->ch2->value.int_value - COLOR_TEMP_MIN - 1)))) / 0.0676) - 1) / 100;
                }
                
                const uint32_t w = lightbulb_group->factor_w * target_color * ch_group->ch1->value.int_value / 100;
                const uint32_t b = lightbulb_group->factor_b * (PWM_SCALE - target_color) * ch_group->ch1->value.int_value / 100;
                lightbulb_group->target_w = ((w > PWM_SCALE) ? PWM_SCALE : w);
                lightbulb_group->target_b = ((b > PWM_SCALE) ? PWM_SCALE : b);
                
            } else {                                        // One Color Dimmer
                const uint32_t w = lightbulb_group->factor_w * PWM_SCALE * ch_group->ch1->value.int_value / 100;
                lightbulb_group->target_w = ((w > PWM_SCALE) ? PWM_SCALE : w);
            }
        } else {
            lightbulb_group->autodimmer = 0;
            lightbulb_group->target_r = 0;
            lightbulb_group->target_g = 0;
            lightbulb_group->target_b = 0;
            lightbulb_group->target_w = 0;
            lightbulb_group->target_cw = 0;
            lightbulb_group->target_ww = 0;
            
            setup_mode_toggle_upcount();
        }
        
        led_blink(1);
        INFO("Target RGBW = %i, %i, %i, %i, %i, %i", lightbulb_group->target_r, lightbulb_group->target_g, lightbulb_group->target_b, lightbulb_group->target_w, lightbulb_group->target_cw, lightbulb_group->target_ww);
        
        if (lightbulb_group->is_pwm && !main_config.setpwm_is_running) {
            main_config.setpwm_is_running = true;
            xTimerStart(main_config.pwm_timer, XTIMER_BLOCK_TIME);
        }
        /*
        else if(lightbulb_group->driver_interface != NULL)
        {
            if(lightbulb_group->pwm_r != 255)
            {
                for(uint8_t chips = 0; chips < lightbulb_group->driver_interface->descriptor->numChips; chips++)
                {
                    my92xx_setChannel(lightbulb_group->driver_interface->descriptor,chips*lightbulb_group->driver_interface->descriptor->numChannels + lightbulb_group->pwm_r, lightbulb_group->target_r);
                }
                main_config.multipwm_duty[lightbulb_group->pwm_r] = lightbulb_group->target_r;
            }
            if(lightbulb_group->pwm_g != 255)
            {
                for(uint8_t chips = 0; chips < lightbulb_group->driver_interface->descriptor->numChips; chips++)
                {
                    my92xx_setChannel(lightbulb_group->driver_interface->descriptor,chips*lightbulb_group->driver_interface->descriptor->numChannels + lightbulb_group->pwm_g, lightbulb_group->target_g);
                }
                main_config.multipwm_duty[lightbulb_group->pwm_g] = lightbulb_group->target_g;
            }
            if(lightbulb_group->pwm_b != 255)
            {
                for(uint8_t chips = 0; chips < lightbulb_group->driver_interface->descriptor->numChips; chips++)
                {
                    my92xx_setChannel(lightbulb_group->driver_interface->descriptor,chips*lightbulb_group->driver_interface->descriptor->numChannels + lightbulb_group->pwm_b, lightbulb_group->target_b);
                }
                main_config.multipwm_duty[lightbulb_group->pwm_b] = lightbulb_group->target_b;
            }
            if(lightbulb_group->pwm_w != 255)
            {
                for(uint8_t chips = 0; chips < lightbulb_group->driver_interface->descriptor->numChips; chips++)
                {
                    my92xx_setChannel(lightbulb_group->driver_interface->descriptor,chips*lightbulb_group->driver_interface->descriptor->numChannels + lightbulb_group->pwm_w, lightbulb_group->target_w);
                }
                main_config.multipwm_duty[lightbulb_group->pwm_w] = lightbulb_group->target_w;
            }
            if(lightbulb_group->pwm_cw != 255)
            {
                for(uint8_t chips = 0; chips < lightbulb_group->driver_interface->descriptor->numChips; chips++)
                {
                    my92xx_setChannel(lightbulb_group->driver_interface->descriptor,chips*lightbulb_group->driver_interface->descriptor->numChannels + lightbulb_group->pwm_cw, lightbulb_group->target_cw);
                }
                main_config.multipwm_duty[lightbulb_group->pwm_cw] = lightbulb_group->target_cw;
            }
            if(lightbulb_group->pwm_ww != 255)
            {
                for(uint8_t chips = 0; chips < lightbulb_group->driver_interface->descriptor->numChips; chips++)
                {
                    my92xx_setChannel(lightbulb_group->driver_interface->descriptor,chips*lightbulb_group->driver_interface->descriptor->numChannels + lightbulb_group->pwm_ww, lightbulb_group->target_ww);
                }
                main_config.multipwm_duty[lightbulb_group->pwm_ww] = lightbulb_group->target_ww;
            }
            my92xx_send(lightbulb_group->driver_interface->descriptor);
        }
        */
        do_actions(ch_group, (uint8_t) ch_group->ch0->value.bool_value);
        
        if (ch_group->ch0->value.bool_value) {
            do_wildcard_actions(ch_group, 0, ch_group->ch1->value.int_value);
        } else {
            ch_group->last_wildcard_action[0] = NO_LAST_WILDCARD_ACTION;
        }
        
        hkc_group_notify(ch_group);
        
        save_states_callback();

    } else {
        homekit_characteristic_notify(ch_group->ch0, ch_group->ch0->value);
    }
}

void rgbw_brightness(const uint8_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;

    if (ch_group->child_enabled) {
        if (type == LIGHTBULB_BRIGHTNESS_UP) {
            if (ch_group->ch1->value.int_value + 10 < 100) {
                ch_group->ch1->value.int_value += 10;
            } else {
                ch_group->ch1->value.int_value = 100;
            }
        } else {    // type == LIGHTBULB_BRIGHTNESS_DOWN
            if (ch_group->ch1->value.int_value - 10 > 0) {
                ch_group->ch1->value.int_value -= 10;
            } else {
                ch_group->ch1->value.int_value = 0;
            }
        }
        
        hkc_rgbw_setter(ch_group->ch1, ch_group->ch1->value);
    }
}

void autodimmer_task(void* args) {
    INFO("AUTODimmer started");
    
    homekit_characteristic_t* ch = args;
    ch_group_t* ch_group = ch_group_find(ch);
    lightbulb_group_t* lightbulb_group = lightbulb_group_find(ch_group->ch0);
    
    lightbulb_group->autodimmer = 4 * 100 / lightbulb_group->autodimmer_task_step;
    while(lightbulb_group->autodimmer > 0) {
        lightbulb_group->autodimmer--;
        if (ch_group->ch1->value.int_value < 100) {
            if (ch_group->ch1->value.int_value + lightbulb_group->autodimmer_task_step < 100) {
                ch_group->ch1->value.int_value += lightbulb_group->autodimmer_task_step;
            } else {
                ch_group->ch1->value.int_value = 100;
            }
        } else {
            ch_group->ch1->value.int_value = lightbulb_group->autodimmer_task_step;
        }
        hkc_rgbw_setter(ch_group->ch1, ch_group->ch1->value);
        
        vTaskDelay(pdMS_TO_TICKS(lightbulb_group->autodimmer_task_delay));
        
        if (ch_group->ch1->value.int_value == 100) {    // Double wait when brightness is 100%
            vTaskDelay(pdMS_TO_TICKS(lightbulb_group->autodimmer_task_delay));
        }
    }
    
    INFO("AUTODimmer stopped");
    
    vTaskDelete(NULL);
}

void no_autodimmer_called(TimerHandle_t xTimer) {
    homekit_characteristic_t* ch0 = (homekit_characteristic_t*) pvTimerGetTimerID(xTimer);
    lightbulb_group_t* lightbulb_group = lightbulb_group_find(ch0);
    lightbulb_group->armed_autodimmer = false;
    hkc_rgbw_setter(ch0, HOMEKIT_BOOL(false));
}

void autodimmer_call(homekit_characteristic_t* ch0, const homekit_value_t value) {
    lightbulb_group_t* lightbulb_group = lightbulb_group_find(ch0);
    if (lightbulb_group->autodimmer_task_step == 0 || (value.bool_value && lightbulb_group->autodimmer == 0)) {
        hkc_rgbw_setter(ch0, value);
    } else if (lightbulb_group->autodimmer > 0) {
        lightbulb_group->autodimmer = 0;
    } else {
        lightbulb_group->autodimmer = 0;
        ch_group_t* ch_group = ch_group_find(ch0);
        if (lightbulb_group->armed_autodimmer) {
            lightbulb_group->armed_autodimmer = false;
            xTimerStop(ch_group->timer, XTIMER_BLOCK_TIME);
            if (xTaskCreate(autodimmer_task, "autodimmer", AUTODIMMER_TASK_SIZE, (void*) ch0, AUTODIMMER_TASK_PRIORITY, NULL) != pdPASS) {
                ERROR("Creating autodimmer_task");
            }
        } else {
            xTimerStart(ch_group->timer, XTIMER_BLOCK_TIME);
            lightbulb_group->armed_autodimmer = true;
        }
    }
}

// --- GARAGE DOOR
void garage_door_stop(const uint8_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    if (ch_group->ch0->value.int_value == GARAGE_DOOR_OPENING || ch_group->ch0->value.int_value == GARAGE_DOOR_CLOSING) {
        led_blink(1);
        INFO("GD stop");
        
        ch_group->ch0->value.int_value = GARAGE_DOOR_STOPPED;
        
        xTimerStop(ch_group->timer, XTIMER_BLOCK_TIME);

        do_actions(ch_group, 10);
        
        hkc_group_notify(ch_group);
    }
}

void garage_door_obstruction(const uint8_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    led_blink(1);
    INFO("GD obstr: %i", type);
    
    ch_group->ch2->value.bool_value = (bool) type;
    
    hkc_group_notify(ch_group);
    
    do_actions(ch_group, type + 8);
}

void garage_door_sensor(const uint8_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    led_blink(1);
    INFO("GD sensor: %i", type);
    
    ch_group->ch0->value.int_value = type;
    
    if (type > 1) {
        ch_group->ch1->value.int_value = type - 2;
        xTimerStart(ch_group->timer, XTIMER_BLOCK_TIME);
    } else {
        ch_group->ch1->value.int_value = type;
        xTimerStop(ch_group->timer, XTIMER_BLOCK_TIME);
        
        if (type == 0) {
            GARAGE_DOOR_CURRENT_TIME = GARAGE_DOOR_WORKING_TIME - GARAGE_DOOR_TIME_MARGIN;
        } else {
            GARAGE_DOOR_CURRENT_TIME = GARAGE_DOOR_TIME_MARGIN;
        }
        
        if (ch_group->ch2->value.bool_value) {
            garage_door_obstruction(0, ch_group, 0);
        }
    }
    
    hkc_group_notify(ch_group);
    
    do_actions(ch_group, type + 4);
}

void hkc_garage_door_setter(homekit_characteristic_t* ch1, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch1);
    if ((ch_group->main_enabled) && !ch_group->ch2->value.bool_value) {
        uint8_t current_door_state = ch_group->ch0->value.int_value;
        if (current_door_state == GARAGE_DOOR_STOPPED) {
            current_door_state = ch_group->ch1->value.int_value;
        } else if (current_door_state > 1) {
            current_door_state -= 2;
        }

        if (value.int_value != current_door_state) {
            led_blink(1);
            INFO("Setter GD");
            
            ch1->value = value;

            do_actions(ch_group, (uint8_t) ch_group->ch0->value.int_value);
   
            if ((value.int_value == GARAGE_DOOR_OPENED && GARAGE_DOOR_HAS_F4 == 0) ||
                       ch_group->ch0->value.int_value == GARAGE_DOOR_CLOSING) {
                garage_door_sensor(0, ch_group, GARAGE_DOOR_OPENING);
            } else if ((value.int_value == GARAGE_DOOR_CLOSED && GARAGE_DOOR_HAS_F5 == 0) ||
                       ch_group->ch0->value.int_value == GARAGE_DOOR_OPENING) {
                garage_door_sensor(0, ch_group, GARAGE_DOOR_CLOSING);
            }
            
            setup_mode_toggle_upcount();
        }

    }
    
    hkc_group_notify(ch_group);
}

void garage_door_timer_worker(TimerHandle_t xTimer) {
    homekit_characteristic_t* ch0 = (homekit_characteristic_t*) pvTimerGetTimerID(xTimer);
    ch_group_t* ch_group = ch_group_find(ch0);

    void halt_timer() {
        xTimerStop(ch_group->timer, XTIMER_BLOCK_TIME);
        if (GARAGE_DOOR_TIME_MARGIN > 0) {
            garage_door_obstruction(0, ch_group, 1);
        }
    }
    
    if (ch0->value.int_value == GARAGE_DOOR_OPENING) {
        GARAGE_DOOR_CURRENT_TIME++;

        if (GARAGE_DOOR_CURRENT_TIME >= GARAGE_DOOR_WORKING_TIME - GARAGE_DOOR_TIME_MARGIN && GARAGE_DOOR_HAS_F2 == 0) {
            xTimerStop(ch_group->timer, XTIMER_BLOCK_TIME);
            garage_door_sensor(0, ch_group, GARAGE_DOOR_OPENED);
            
        } else if (GARAGE_DOOR_CURRENT_TIME >= GARAGE_DOOR_WORKING_TIME && GARAGE_DOOR_HAS_F2 == 1) {
            halt_timer();
        }
        
    } else {    // GARAGE_DOOR_CLOSING
        GARAGE_DOOR_CURRENT_TIME -= GARAGE_DOOR_CLOSE_TIME_FACTOR;
        
        if (GARAGE_DOOR_CURRENT_TIME <= GARAGE_DOOR_TIME_MARGIN && GARAGE_DOOR_HAS_F3 == 0) {
            xTimerStop(ch_group->timer, XTIMER_BLOCK_TIME);
            garage_door_sensor(0, ch_group, GARAGE_DOOR_CLOSED);
            
        } else if (GARAGE_DOOR_CURRENT_TIME <= 0 && GARAGE_DOOR_HAS_F3 == 1) {
            halt_timer();
        }
    }
}

// --- WINDOW COVER
void normalize_position(ch_group_t* ch_group) {
    if (WINDOW_COVER_MOTOR_POSITION < 0) {
        WINDOW_COVER_MOTOR_POSITION = 0;
    } else if (WINDOW_COVER_MOTOR_POSITION > 100) {
        WINDOW_COVER_MOTOR_POSITION = 100;
    }
    
    if (WINDOW_COVER_HOMEKIT_POSITION < 0) {
        WINDOW_COVER_HOMEKIT_POSITION = 0;
    } else if (WINDOW_COVER_HOMEKIT_POSITION > 100) {
        WINDOW_COVER_HOMEKIT_POSITION = 100;
    }
}

float window_cover_step(ch_group_t* ch_group, const float cover_time) {
    const float actual_time = SYSTEM_UPTIME_MS;
    const float time = actual_time - WINDOW_COVER_LAST_TIME;
    WINDOW_COVER_LAST_TIME = actual_time;

    return (100.00000000f / cover_time) * (time / 1000.00000000f);
}

void window_cover_stop(ch_group_t* ch_group) {
    xTimerStop(ch_group->timer, XTIMER_BLOCK_TIME);
    
    led_blink(1);

    switch (WINDOW_COVER_CH_STATE->value.int_value) {
        case WINDOW_COVER_CLOSING:
            WINDOW_COVER_MOTOR_POSITION -= window_cover_step(ch_group, WINDOW_COVER_TIME_CLOSE);
            break;
            
        case WINDOW_COVER_OPENING:
            WINDOW_COVER_MOTOR_POSITION += window_cover_step(ch_group, WINDOW_COVER_TIME_OPEN);
            break;
            
        default:
            break;
    }

    INFO("WC Stopped Motor %f, HomeKit %f", WINDOW_COVER_MOTOR_POSITION, WINDOW_COVER_HOMEKIT_POSITION);
    
    normalize_position(ch_group);
    
    WINDOW_COVER_CH_CURRENT_POSITION->value.int_value = WINDOW_COVER_HOMEKIT_POSITION;
    WINDOW_COVER_CH_TARGET_POSITION->value.int_value = WINDOW_COVER_HOMEKIT_POSITION;
    
    if (WINDOW_COVER_CH_STATE->value.int_value == WINDOW_COVER_CLOSING) {
        do_actions(ch_group, WINDOW_COVER_STOP_FROM_CLOSING);
    } else if (WINDOW_COVER_CH_STATE->value.int_value == WINDOW_COVER_OPENING) {
        do_actions(ch_group, WINDOW_COVER_STOP_FROM_OPENING);
    }
    
    do_actions(ch_group, WINDOW_COVER_STOP);
    
    WINDOW_COVER_CH_STATE->value.int_value = WINDOW_COVER_STOP;
    
    hkc_group_notify(ch_group);
    
    setup_mode_toggle_upcount();
    
    save_states_callback();
}

void window_cover_obstruction(const uint8_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    led_blink(1);
    INFO("WC obstr: %i", type);
    
    ch_group->ch3->value.bool_value = (bool) type;
    
    hkc_group_notify(ch_group);
    
    do_actions(ch_group, type + WINDOW_COVER_OBSTRUCTION);
}

void window_cover_timer_rearm_stop(TimerHandle_t xTimer) {
    ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
    
    WINDOW_COVER_STOP_ENABLE = 1;
}

void hkc_window_cover_setter(homekit_characteristic_t* ch1, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch1);
    
    void disable_stop() {
        WINDOW_COVER_STOP_ENABLE = 0;
        xTimerStart(ch_group->timer2, XTIMER_BLOCK_TIME);
    }
    
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("Setter WC: Current: %i, Target: %i", WINDOW_COVER_CH_CURRENT_POSITION->value.int_value, value.int_value);
        
        ch1->value = value;

        normalize_position(ch_group);
        
        WINDOW_COVER_LAST_TIME = SYSTEM_UPTIME_MS;
        
        if (value.int_value < WINDOW_COVER_CH_CURRENT_POSITION->value.int_value) {
            disable_stop();
            
            if (WINDOW_COVER_CH_STATE->value.int_value == WINDOW_COVER_OPENING) {
                do_actions(ch_group, WINDOW_COVER_CLOSING_FROM_MOVING);
                setup_mode_toggle_upcount();
            } else {
                do_actions(ch_group, WINDOW_COVER_CLOSING);
            }
            
            if (WINDOW_COVER_CH_STATE->value.int_value == WINDOW_COVER_STOP) {
                xTimerStart(ch_group->timer, XTIMER_BLOCK_TIME);
            }
            
            WINDOW_COVER_CH_STATE->value.int_value = WINDOW_COVER_CLOSING;

        } else if (value.int_value > WINDOW_COVER_CH_CURRENT_POSITION->value.int_value) {
            disable_stop();
            
            if (WINDOW_COVER_CH_STATE->value.int_value == WINDOW_COVER_CLOSING) {
                do_actions(ch_group, WINDOW_COVER_OPENING_FROM_MOVING);
                setup_mode_toggle_upcount();
            } else {
                do_actions(ch_group, WINDOW_COVER_OPENING);
            }
            
            if (WINDOW_COVER_CH_STATE->value.int_value == WINDOW_COVER_STOP) {
                xTimerStart(ch_group->timer, XTIMER_BLOCK_TIME);
            }
            
            WINDOW_COVER_CH_STATE->value.int_value = WINDOW_COVER_OPENING;

        } else {
            window_cover_stop(ch_group);
        }
        
        do_wildcard_actions(ch_group, 0, value.int_value);
    
    }
    
    hkc_group_notify(ch_group);
}

void window_cover_timer_worker(TimerHandle_t xTimer) {
    ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
    
    uint8_t margin = 0;     // Used as covering offset to add extra time when target position completely closed or opened
    if (WINDOW_COVER_CH_TARGET_POSITION->value.int_value == 0 || WINDOW_COVER_CH_TARGET_POSITION->value.int_value == 100) {
        margin = WINDOW_COVER_MARGIN_SYNC;
    }
    
    void normalize_current_position() {
        if (WINDOW_COVER_MOTOR_POSITION < 0) {
            WINDOW_COVER_CH_CURRENT_POSITION->value.int_value = 0;
        } else if (WINDOW_COVER_MOTOR_POSITION > 100) {
            WINDOW_COVER_CH_CURRENT_POSITION->value.int_value = 100;
        } else {
            if ((WINDOW_COVER_CH_CURRENT_POSITION->value.int_value >> 2) != (uint8_t) WINDOW_COVER_MOTOR_POSITION >> 2) {
                INFO("WC Moving Motor %f, HomeKit %f", WINDOW_COVER_MOTOR_POSITION, WINDOW_COVER_HOMEKIT_POSITION);
                WINDOW_COVER_CH_CURRENT_POSITION->value.int_value = WINDOW_COVER_HOMEKIT_POSITION;
                homekit_characteristic_notify(WINDOW_COVER_CH_CURRENT_POSITION, WINDOW_COVER_CH_CURRENT_POSITION->value);
            } else {
                WINDOW_COVER_CH_CURRENT_POSITION->value.int_value = WINDOW_COVER_MOTOR_POSITION;
            }
        }
    }

    float window_cover_motor_position(const float position, const float correction) {
        return position / (1 + ((100 - position) * correction * 0.00020000f));
    }
    
    switch (WINDOW_COVER_CH_STATE->value.int_value) {
        case WINDOW_COVER_CLOSING:
            WINDOW_COVER_MOTOR_POSITION -= window_cover_step(ch_group, WINDOW_COVER_TIME_CLOSE);
            if (WINDOW_COVER_MOTOR_POSITION > 0) {
                WINDOW_COVER_HOMEKIT_POSITION = window_cover_motor_position(WINDOW_COVER_MOTOR_POSITION, WINDOW_COVER_CORRECTION);
            } else {
                WINDOW_COVER_HOMEKIT_POSITION = WINDOW_COVER_MOTOR_POSITION;
            }
            
            normalize_current_position();

            if ((WINDOW_COVER_CH_TARGET_POSITION->value.int_value - margin) >= WINDOW_COVER_HOMEKIT_POSITION) {
                window_cover_stop(ch_group);
            }
            break;
            
        case WINDOW_COVER_OPENING:
            WINDOW_COVER_MOTOR_POSITION += window_cover_step(ch_group, WINDOW_COVER_TIME_OPEN);
            if (WINDOW_COVER_MOTOR_POSITION < 100) {
                WINDOW_COVER_HOMEKIT_POSITION = window_cover_motor_position(WINDOW_COVER_MOTOR_POSITION, WINDOW_COVER_CORRECTION);
            } else {
                WINDOW_COVER_HOMEKIT_POSITION = WINDOW_COVER_MOTOR_POSITION;
            }
        
            normalize_current_position();
            
            if ((WINDOW_COVER_CH_TARGET_POSITION->value.int_value + margin) <= WINDOW_COVER_HOMEKIT_POSITION) {
                window_cover_stop(ch_group);
            }
            break;
            
        default:    // case WINDOW_COVER_STOP:
            window_cover_stop(ch_group);
            break;
    }
}

// --- FAN
void hkc_fan_setter(homekit_characteristic_t* ch0, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch0);
    if (ch_group->main_enabled) {
        if (ch0->value.bool_value != value.bool_value) {
            led_blink(1);
            INFO("Setter FAN");
            
            ch0->value = value;
            
            do_actions(ch_group, (uint8_t) value.int_value);
            
            if (value.bool_value) {
                do_wildcard_actions(ch_group, 0, ch_group->ch1->value.float_value);
                
                if (ch0->value.bool_value && ch_group->num[0] > 0) {
                    autooff_setter_params_t* autooff_setter_params = malloc(sizeof(autooff_setter_params_t));
                    autooff_setter_params->ch = ch0;
                    autooff_setter_params->type = TYPE_FAN;
                    xTimerStart(xTimerCreate(0, pdMS_TO_TICKS(ch_group->num[0] * 1000), pdFALSE, (void*) autooff_setter_params, hkc_autooff_setter_task), XTIMER_BLOCK_TIME);
                }
            } else {
                ch_group->last_wildcard_action[0] = NO_LAST_WILDCARD_ACTION;
            }

            setup_mode_toggle_upcount();
            
            save_states_callback();
        }
    }
    
    hkc_group_notify(ch_group);
}

void hkc_fan_speed_setter(homekit_characteristic_t* ch1, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch1);
    if (ch_group->main_enabled) {
        if (ch1->value.float_value != value.float_value) {
            led_blink(1);
            INFO("Setter Speed FAN");
            
            ch1->value = value;
            
            if (ch_group->ch0->value.bool_value) {
                do_wildcard_actions(ch_group, 0, value.float_value);
            }
            
            save_states_callback();
        }
    }
    
    hkc_group_notify(ch_group);
}

void hkc_fan_status_setter(homekit_characteristic_t* ch0, const homekit_value_t value) {
    if (ch0->value.int_value != value.int_value) {
        led_blink(1);
        INFO("Setter Status FAN");
        
        ch0->value = value;
        
        hkc_group_notify(ch_group_find(ch0));
        
        save_states_callback();
    }
}

// --- TV
void hkc_tv_active(homekit_characteristic_t* ch0, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch0);
    if (ch_group->main_enabled) {
        if (ch0->value.int_value != value.int_value) {
            led_blink(1);
            INFO("Setter TV ON %i", value.int_value);
            
            ch0->value = value;
            
            do_actions(ch_group, value.int_value);
            
            setup_mode_toggle_upcount();
            save_states_callback();
        }
    }
    
    hkc_group_notify(ch_group);
}

void hkc_tv_status_active(homekit_characteristic_t* ch0, const homekit_value_t value) {
    if (ch0->value.int_value != value.int_value) {
        led_blink(1);
        INFO("Setter Status TV ON %i", value.int_value);
        
        ch0->value = value;
        
        hkc_group_notify(ch_group_find(ch0));
        
        save_states_callback();
    }
}

void hkc_tv_active_identifier(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        if (ch->value.int_value != value.int_value) {
            led_blink(1);
            INFO("Setter TV Input %i", value.int_value);
            
            ch->value = value;

            do_actions(ch_group, (MAX_ACTIONS - 1) + value.int_value);
        }
    }
    
    hkc_group_notify(ch_group);
}

void hkc_tv_key(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("Setter TV Key %i", value.int_value + 2);
        
        ch->value = value;
        
        do_actions(ch_group, value.int_value + 2);
    }
    
    hkc_group_notify(ch_group);
}

void hkc_tv_power_mode(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("Setter TV Settings %i", value.int_value + 30);
        
        ch->value = value;
        
        do_actions(ch_group, value.int_value + 30);
    }
    
    hkc_group_notify(ch_group);
}

void hkc_tv_mute(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("Setter TV Mute %i", value.int_value + 20);
        
        ch->value = value;
        
        do_actions(ch_group, value.int_value + 20);
    }
    
    hkc_group_notify(ch_group);
}

void hkc_tv_volume(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("Setter TV Volume %i", value.int_value + 22);
        
        ch->value = value;
        
        do_actions(ch_group, value.int_value + 22);
    }
    
    hkc_group_notify(ch_group);
}

void hkc_tv_configured_name(homekit_characteristic_t* ch1, const homekit_value_t value) {
    INFO("Setter TV Name %s", value.string_value);
    
    char* new_name = strdup(value.string_value);
    
    homekit_value_destruct(&ch1->value);
    ch1->value = HOMEKIT_STRING(new_name);

    hkc_group_notify(ch_group_find(ch1));

    save_states_callback();
}

void hkc_tv_input_configured_name(homekit_characteristic_t* ch, const homekit_value_t value) {
    homekit_characteristic_notify(ch, ch->value);
}

// --- DIGITAL INPUTS
void window_cover_diginput(const uint8_t gpio, void* args, const uint8_t type) {
    INFO("WC DigI GPIO %i", gpio);
    
    ch_group_t* ch_group = args;

    if (ch_group->child_enabled) {
        switch (type) {
            case WINDOW_COVER_CLOSING:
                hkc_window_cover_setter(ch_group->ch1, HOMEKIT_UINT8(0));
                break;
                
            case WINDOW_COVER_OPENING:
                hkc_window_cover_setter(ch_group->ch1, HOMEKIT_UINT8(100));
                break;
                
            case (WINDOW_COVER_CLOSING + 3):
                if (WINDOW_COVER_CH_STATE->value.int_value == WINDOW_COVER_CLOSING) {
                    hkc_window_cover_setter(ch_group->ch1, WINDOW_COVER_CH_CURRENT_POSITION->value);
                } else {
                    hkc_window_cover_setter(ch_group->ch1, HOMEKIT_UINT8(0));
                }
                break;
                
            case (WINDOW_COVER_OPENING + 3):
                if (WINDOW_COVER_CH_STATE->value.int_value == WINDOW_COVER_OPENING) {
                    hkc_window_cover_setter(ch_group->ch1, WINDOW_COVER_CH_CURRENT_POSITION->value);
                } else {
                    hkc_window_cover_setter(ch_group->ch1, HOMEKIT_UINT8(100));
                }
                break;
                
            default:    // case WINDOW_COVER_STOP:
                if ((int8_t) WINDOW_COVER_STOP_ENABLE == 1) {
                    hkc_window_cover_setter(ch_group->ch1, WINDOW_COVER_CH_CURRENT_POSITION->value);
                } else {
                    INFO("WC DigI Stop ignored");
                }
                break;
        }
    }
}

void diginput(const uint8_t gpio, void* args, const uint8_t type) {
    INFO("DigI GPIO %i", gpio);
    
    ch_group_t* ch_group = args;
    
    if (ch_group->child_enabled) {
        switch (type) {
            case TYPE_LOCK:
                if (ch_group->ch1->value.int_value == 1) {
                    hkc_lock_setter(ch_group->ch1, HOMEKIT_UINT8(0));
                } else {
                    hkc_lock_setter(ch_group->ch1, HOMEKIT_UINT8(1));
                }
                break;
                
            case TYPE_DOUBLE_LOCK:
                if (ch_group->ch3->value.int_value == 1) {
                    hkc_lock_setter(ch_group->ch3, HOMEKIT_UINT8(0));
                } else {
                    hkc_lock_setter(ch_group->ch3, HOMEKIT_UINT8(1));
                }
                break;
                
            case TYPE_VALVE:
                if (ch_group->ch0->value.int_value == 1) {
                    hkc_valve_setter(ch_group->ch0, HOMEKIT_UINT8(0));
                } else {
                    hkc_valve_setter(ch_group->ch0, HOMEKIT_UINT8(1));
                }
                break;
                
            case TYPE_LIGHTBULB:
                autodimmer_call(ch_group->ch0, HOMEKIT_BOOL(!ch_group->ch0->value.bool_value));
                break;
                
            case TYPE_GARAGE_DOOR:
                if (ch_group->ch1->value.int_value == 1) {
                    hkc_garage_door_setter(ch_group->ch1, HOMEKIT_UINT8(0));
                } else {
                    hkc_garage_door_setter(ch_group->ch1, HOMEKIT_UINT8(1));
                }
                break;
                
            case TYPE_WINDOW_COVER:
                if (ch_group->ch1->value.int_value == 0) {
                    hkc_window_cover_setter(ch_group->ch1, HOMEKIT_UINT8(100));
                } else {
                    hkc_window_cover_setter(ch_group->ch1, HOMEKIT_UINT8(0));
                }
                break;
                
            case TYPE_FAN:
                if (ch_group->ch0->value.bool_value == true) {
                    hkc_fan_setter(ch_group->ch0, HOMEKIT_BOOL(false));
                } else {
                    hkc_fan_setter(ch_group->ch0, HOMEKIT_BOOL(true));
                }
                break;
                
            case TYPE_TV:
                if (ch_group->ch0->value.int_value == 1) {
                    hkc_tv_active(ch_group->ch0, HOMEKIT_UINT8(0));
                } else {
                    hkc_tv_active(ch_group->ch0, HOMEKIT_UINT8(1));
                }
                break;
                
            default:    // case TYPE_ON:
                hkc_on_setter(ch_group->ch0, HOMEKIT_BOOL(!ch_group->ch0->value.bool_value));
                break;
        }
    }
}

void diginput_1(const uint8_t gpio, void* args, const uint8_t type) {
    INFO("DigI GPIO %i", gpio);
    
    ch_group_t* ch_group = args;

    if (ch_group->child_enabled) {
        switch (type) {
            case TYPE_LOCK:
                if (ch_group->ch1->value.int_value == 0) {
                    hkc_lock_setter(ch_group->ch1, HOMEKIT_UINT8(1));
                }
                break;
                
            case TYPE_DOUBLE_LOCK:
                if (ch_group->ch3->value.int_value == 0) {
                    hkc_lock_setter(ch_group->ch3, HOMEKIT_UINT8(1));
                }
                break;
                
            case TYPE_VALVE:
                if (ch_group->ch0->value.int_value == 0) {
                    hkc_valve_setter(ch_group->ch0, HOMEKIT_UINT8(1));
                }
                break;
                
            case TYPE_LIGHTBULB:
                if (ch_group->ch0->value.bool_value == false) {
                    autodimmer_call(ch_group->ch0, HOMEKIT_BOOL(true));
                }
                break;
                
            case TYPE_GARAGE_DOOR:
                if (ch_group->ch1->value.int_value == 0) {
                    hkc_garage_door_setter(ch_group->ch1, HOMEKIT_UINT8(1));
                }
                break;
                
            case TYPE_FAN:
                if (ch_group->ch0->value.bool_value == false) {
                    hkc_fan_setter(ch_group->ch0, HOMEKIT_BOOL(true));
                }
                break;
                
            case TYPE_TV:
                if (ch_group->ch0->value.int_value == 0) {
                    hkc_tv_active(ch_group->ch0, HOMEKIT_UINT8(1));
                }
                break;
                
            default:    // case TYPE_ON:
                if (ch_group->ch0->value.bool_value == false) {
                    hkc_on_setter(ch_group->ch0, HOMEKIT_BOOL(true));
                }
                break;
        }
    }
}

void digstate_1(const uint8_t gpio, void* args, const uint8_t type) {
    INFO("DigI GPIO %i", gpio);
    
    ch_group_t* ch_group = args;

    if (ch_group->child_enabled) {
        switch (type) {
            case TYPE_LOCK:
                if (ch_group->ch1->value.int_value == 0) {
                    hkc_lock_status_setter(ch_group->ch1, HOMEKIT_UINT8(1));
                }
                break;
                
            case TYPE_DOUBLE_LOCK:
                if (ch_group->ch3->value.int_value == 0) {
                    hkc_lock_status_setter(ch_group->ch3, HOMEKIT_UINT8(1));
                }
                break;
                
            case TYPE_VALVE:
                if (ch_group->ch0->value.int_value == 0) {
                    hkc_valve_status_setter(ch_group->ch0, HOMEKIT_UINT8(1));
                }
                break;
                
            case TYPE_FAN:
                if (ch_group->ch0->value.bool_value == false) {
                    hkc_fan_status_setter(ch_group->ch0, HOMEKIT_BOOL(true));
                }
                break;
                
            case TYPE_TV:
                if (ch_group->ch0->value.int_value == 0) {
                    hkc_tv_status_active(ch_group->ch0, HOMEKIT_UINT8(1));
                }
                break;
                
            default:    // case TYPE_ON:
                if (ch_group->ch0->value.bool_value == false) {
                    hkc_on_status_setter(ch_group->ch0, HOMEKIT_BOOL(true));
                }
                break;
        }
    }
}

void diginput_0(const uint8_t gpio, void* args, const uint8_t type) {
    INFO("DigI GPIO %i", gpio);
    
    ch_group_t* ch_group = args;

    if (ch_group->child_enabled) {
        switch (type) {
            case TYPE_LOCK:
                if (ch_group->ch1->value.int_value == 1) {
                    hkc_lock_setter(ch_group->ch1, HOMEKIT_UINT8(0));
                }
                break;
                
            case TYPE_DOUBLE_LOCK:
                if (ch_group->ch3->value.int_value == 1) {
                    hkc_lock_setter(ch_group->ch3, HOMEKIT_UINT8(0));
                }
                break;
                
            case TYPE_VALVE:
                if (ch_group->ch0->value.int_value == 1) {
                    hkc_valve_setter(ch_group->ch0, HOMEKIT_UINT8(0));
                }
                break;
                
            case TYPE_LIGHTBULB:
                if (ch_group->ch0->value.bool_value == true) {
                    autodimmer_call(ch_group->ch0, HOMEKIT_BOOL(false));
                }
                break;
                
            case TYPE_GARAGE_DOOR:
                if (ch_group->ch1->value.int_value == 1) {
                    hkc_garage_door_setter(ch_group->ch1, HOMEKIT_UINT8(0));
                }
                break;
                
            case TYPE_FAN:
                if (ch_group->ch0->value.bool_value == true) {
                    hkc_fan_setter(ch_group->ch0, HOMEKIT_BOOL(false));
                }
                break;
                
            case TYPE_TV:
                if (ch_group->ch0->value.int_value == 1) {
                    hkc_tv_active(ch_group->ch0, HOMEKIT_UINT8(0));
                }
                break;
                
            default:    // case TYPE_ON:
                if (ch_group->ch0->value.bool_value == true) {
                    hkc_on_setter(ch_group->ch0, HOMEKIT_BOOL(false));
                }
                break;
        }
    }
}

void digstate_0(const uint8_t gpio, void* args, const uint8_t type) {
    INFO("DigI GPIO %i", gpio);
    
    ch_group_t* ch_group = args;

    if (ch_group->child_enabled) {
        switch (type) {
            case TYPE_LOCK:
                if (ch_group->ch1->value.int_value == 1) {
                    hkc_lock_status_setter(ch_group->ch1, HOMEKIT_UINT8(0));
                }
                break;
                
            case TYPE_DOUBLE_LOCK:
                if (ch_group->ch3->value.int_value == 1) {
                    hkc_lock_status_setter(ch_group->ch3, HOMEKIT_UINT8(0));
                }
                break;
                
            case TYPE_VALVE:
                if (ch_group->ch0->value.int_value == 1) {
                    hkc_valve_status_setter(ch_group->ch0, HOMEKIT_UINT8(0));
                }
                break;
                
            case TYPE_FAN:
                if (ch_group->ch0->value.bool_value == true) {
                    hkc_fan_status_setter(ch_group->ch0, HOMEKIT_BOOL(false));
                }
                break;
                
            case TYPE_TV:
                if (ch_group->ch0->value.int_value == 1) {
                    hkc_tv_status_active(ch_group->ch0, HOMEKIT_UINT8(0));
                }
                break;
                
            default:    // case TYPE_ON:
                if (ch_group->ch0->value.bool_value == true) {
                    hkc_on_status_setter(ch_group->ch0, HOMEKIT_BOOL(false));
                }
                break;
        }
    }
}

// --- AUTO-OFF
void hkc_autooff_setter_task(TimerHandle_t xTimer) {
    autooff_setter_params_t* autooff_setter_params = (autooff_setter_params_t*) pvTimerGetTimerID(xTimer);

    switch (autooff_setter_params->type) {
        case TYPE_LOCK:
        case TYPE_DOUBLE_LOCK:
            hkc_lock_setter(autooff_setter_params->ch, HOMEKIT_UINT8(1));
            break;
            
        case TYPE_SENSOR:
        case TYPE_SENSOR_BOOL:
            sensor_0(0, ch_group_find(autooff_setter_params->ch), autooff_setter_params->type);
            break;
            
        case TYPE_VALVE:
            hkc_valve_setter(autooff_setter_params->ch, HOMEKIT_UINT8(0));
            break;
            
        case TYPE_FAN:
            hkc_fan_setter(autooff_setter_params->ch, HOMEKIT_BOOL(false));
            break;
            
        default:    // case TYPE_ON:
            hkc_on_setter(autooff_setter_params->ch, HOMEKIT_BOOL(false));
            break;
    }
    
    free(autooff_setter_params);
    xTimerDelete(xTimer, XTIMER_BLOCK_TIME);
}

// --- HTTP/TCP task
void http_get_task(void* pvParameters) {
    action_task_t* action_task = pvParameters;
    action_http_t* action_http = action_task->ch_group->action_http;
    
    while(action_http) {
        if (action_http->action == action_task->action) {
            INFO("HTTP/TCP Action %s:%i", action_http->host, action_http->port_n);
            
            const struct addrinfo hints = {
                .ai_family = AF_UNSPEC,
                .ai_socktype = SOCK_STREAM,
            };
            struct addrinfo* res;
            
            char port[6];
            memset(port, 0, 6);
            itoa(action_http->port_n, port, 10);
            
            if (getaddrinfo(action_http->host, port, &hints, &res) == 0) {
                int s = socket(res->ai_family, res->ai_socktype, 0);
                if (s >= 0) {
                    if (connect(s, res->ai_addr, res->ai_addrlen) == 0) {
                        uint16_t content_len_n = 0;
                        str_ch_value_t* str_ch_value_first = NULL;
                        
                        char* method = "GET";
                        char* method_req = NULL;
                        if (action_http->method_n == 1 || action_http->method_n == 2) {
                            content_len_n = strlen(action_http->content);
                            
                            char* content_search = action_http->content;
                            str_ch_value_t* str_ch_value_last = NULL;
                            
                            do {
                                content_search = strstr(content_search, HTTP_ACTION_WILDCARD_VALUE);
                                if (content_search) {
                                    char buffer[10];
                                    buffer[2] = 0;
                                    
                                    buffer[0] = content_search[5];
                                    buffer[1] = content_search[6];
                                    
                                    uint8_t acc_number = (uint8_t) strtol(buffer, NULL, 10);
                                    
                                    ch_group_t* ch_group_found = action_task->ch_group;
                                    
                                    if (acc_number > 0) {
                                        ch_group_found = ch_group_find_by_acc(acc_number);
                                    }
                                    
                                    buffer[0] = content_search[7];
                                    buffer[1] = content_search[8];

                                    homekit_value_t* value;
                                    
                                    switch ((uint8_t) strtol(buffer, NULL, 10)) {
                                        case 1:
                                            value = &ch_group_found->ch1->value;
                                            break;
                                            
                                        case 2:
                                            value = &ch_group_found->ch2->value;
                                            break;
                                            
                                        case 3:
                                            value = &ch_group_found->ch3->value;
                                            break;
                                            
                                        case 4:
                                            value = &ch_group_found->ch4->value;
                                            break;
                                            
                                        case 5:
                                            value = &ch_group_found->ch5->value;
                                            break;
                                            
                                        case 6:
                                            value = &ch_group_found->ch6->value;
                                            break;
                                            
                                        case 7:
                                            value = &ch_group_found->ch7->value;
                                            break;
                                            
                                        default:    // case 0:
                                            value = &ch_group_found->ch0->value;
                                            break;
                                    }
                                    
                                    switch (value->format) {
                                        case homekit_format_bool:
                                            snprintf(buffer, 10, "%i", value->bool_value);
                                            break;
                                            
                                        case homekit_format_uint8:
                                        case homekit_format_uint16:
                                        case homekit_format_uint32:
                                        case homekit_format_uint64:
                                        case homekit_format_int:
                                            snprintf(buffer, 10, "%i", value->int_value);
                                            break;

                                        case homekit_format_float:
                                            snprintf(buffer, 10, "%.3f", value->float_value);
                                            break;
                                            
                                        default:
                                            buffer[0] = 0;
                                            break;
                                    }
                                    
                                    content_len_n += strlen(buffer) - 9;
                                    
                                    str_ch_value_t* str_ch_value = malloc(sizeof(str_ch_value_t));
                                    memset(str_ch_value, 0, sizeof(*str_ch_value));
                                    
                                    str_ch_value->string = strdup(buffer);
                                    str_ch_value->next = NULL;
                                    INFO("Wildcard value: %s", str_ch_value->string);
                                    
                                    if (str_ch_value_first == NULL) {
                                        str_ch_value_first = str_ch_value;
                                        str_ch_value_last = str_ch_value;
                                    } else {
                                        str_ch_value_last->next = str_ch_value;
                                        str_ch_value_last = str_ch_value;
                                    }

                                    content_search += 9;
                                }
                                
                            } while (content_search);

                            char content_len[4];
                            itoa(content_len_n, content_len, 10);
                            method_req = malloc(48);
                            snprintf(method_req, 48, "Content-type: text/html\r\nContent-length: %s\r\n", content_len);
                            
                            if (action_http->method_n == 1) {
                                method = "PUT";
                            } else {
                                method = "POST";
                            }

                        }
                        
                        char* req = NULL;
                        
                        if (action_http->method_n == 3) {
                            req = action_http->content;
                            
                        } else {
                            action_http->len = 69 + strlen(method) + ((method_req != NULL) ? strlen(method_req) : 0) + strlen(FIRMWARE_VERSION) + strlen(action_http->host) +  strlen(action_http->url) + content_len_n;
                            
                            req = malloc(action_http->len);
                            snprintf(req, action_http->len, "%s /%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: HAA/"FIRMWARE_VERSION" esp8266\r\nConnection: close\r\n%s\r\n",
                                     method,
                                     action_http->url,
                                     action_http->host,
                                     (method_req != NULL) ? method_req : "");

                            if (str_ch_value_first) {
                                str_ch_value_t* str_ch_value = str_ch_value_first;
                                char* content_search = action_http->content;
                                char* last_pos = action_http->content;
                                
                                do {
                                    content_search = strstr(last_pos, HTTP_ACTION_WILDCARD_VALUE);
                                    
                                    if (content_search - last_pos > 0) {
                                        strncat(req, last_pos, content_search - last_pos);
                                    }
                                    
                                    strcat(req, str_ch_value->string);
                                    
                                    free(str_ch_value->string);

                                    str_ch_value_t* str_ch_value_old = str_ch_value;
                                    str_ch_value = str_ch_value->next;
                                    
                                    free(str_ch_value_old);

                                    last_pos = content_search + 9;
                                    
                                } while (str_ch_value);
                                
                                strcat(req, last_pos);
                                
                            } else {
                                strcat(req, action_http->content);
                            }
                            
                        }
                        
                        if (write(s, req, action_http->len) >= 0) {
                            INFO("Payload:\n%s", req);
                            
                        } else {
                            ERROR("HTTP");
                        }
                        
                        if (method_req) {
                            free(method_req);
                        }
                        
                        if (req && action_http->method_n != 3) {
                            free(req);
                        }
                    } else {
                        ERROR("Connection");
                    }
                } else {
                    ERROR("Socket");
                }
                
                close(s);
            } else {
                ERROR("DNS");
            }
            
            freeaddrinfo(res);
            
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        action_http = action_http->next;
    }
    
    free(pvParameters);
    vTaskDelete(NULL);
}

// --- IR Send task
void ir_tx_task(void* pvParameters) {
    action_task_t* action_task = pvParameters;
    action_ir_tx_t* action_ir_tx = action_task->ch_group->action_ir_tx;
    
    while(action_ir_tx) {
        if (action_ir_tx->action == action_task->action) {
            uint16_t* ir_code = NULL;
            uint16_t ir_code_len = 0;
            
            uint8_t freq = main_config.ir_tx_freq;
            if (action_ir_tx->freq > 0) {
                freq = action_ir_tx->freq;
            }
            
            // Decoding protocol based IR code
            if (action_ir_tx->prot_code) {
                char* prot = NULL;
                
                if (action_ir_tx->prot) {
                    prot = action_ir_tx->prot;
                } else if (action_task->ch_group->ir_protocol) {
                    prot = action_task->ch_group->ir_protocol;
                } else {
                    prot = ch_group_find_by_acc(ACC_TYPE_ROOT_DEVICE)->ir_protocol;
                }
                
                // Decoding protocol based IR code length
                const uint8_t ir_action_protocol_len = strlen(prot);
                const uint16_t json_ir_code_len = strlen(action_ir_tx->prot_code);
                ir_code_len = 3;
                
                printf("IR Protocol bits: ");
                
                switch (ir_action_protocol_len) {
                    case IR_ACTION_PROTOCOL_LEN_4BITS:
                        INFO("4");
                        for (uint16_t i = 0; i < json_ir_code_len; i++) {
                            char* found = strchr(baseUC_dic, action_ir_tx->prot_code[i]);
                            if (found) {
                                if (found - baseUC_dic < 13) {
                                    ir_code_len += (1 + found - baseUC_dic) << 1;
                                } else {
                                    ir_code_len += (1 - 13 + found - baseUC_dic) << 1;
                                }
                            } else {
                                found = strchr(baseLC_dic, action_ir_tx->prot_code[i]);
                                if (found - baseLC_dic < 13) {
                                    ir_code_len += (1 + found - baseLC_dic) << 1;
                                } else {
                                    ir_code_len += (1 - 13 + found - baseLC_dic) << 1;
                                }
                            }
                        }
                        break;
                        
                    case IR_ACTION_PROTOCOL_LEN_6BITS:
                        INFO("6");
                        for (uint16_t i = 0; i < json_ir_code_len; i++) {
                            char* found = strchr(baseUC_dic, action_ir_tx->prot_code[i]);
                            if (found) {
                                if (found - baseUC_dic < 9) {
                                    ir_code_len += (1 + found - baseUC_dic) << 1;
                                } else if (found - baseUC_dic < 18) {
                                    ir_code_len += (1 - 9 + found - baseUC_dic) << 1;
                                } else {
                                    ir_code_len += (1 - 18 + found - baseUC_dic) << 1;
                                }
                            } else {
                                found = strchr(baseLC_dic, action_ir_tx->prot_code[i]);
                                if (found - baseLC_dic < 9) {
                                    ir_code_len += (1 + found - baseLC_dic) << 1;
                                } else if (found - baseLC_dic < 18) {
                                    ir_code_len += (1 - 9 + found - baseLC_dic) << 1;
                                } else {
                                    ir_code_len += (1 - 18 + found - baseLC_dic) << 1;
                                }
                            }
                        }
                        break;
                        
                    default:    // case IR_ACTION_PROTOCOL_LEN_2BITS:
                        INFO("2");
                        for (uint16_t i = 0; i < json_ir_code_len; i++) {
                            char* found = strchr(baseUC_dic, action_ir_tx->prot_code[i]);
                            if (found) {
                                ir_code_len += (1 + found - baseUC_dic) << 1;
                            } else {
                                found = strchr(baseLC_dic, action_ir_tx->prot_code[i]);
                                ir_code_len += (1 + found - baseLC_dic) << 1;
                            }
                        }
                        break;
                }
                
                ir_code = malloc(sizeof(uint16_t) * ir_code_len);
                
                INFO("IR Code Len: %i\nIR Protocol: %s", ir_code_len, prot);
                
                uint16_t bit0_mark = 0, bit0_space = 0, bit1_mark = 0, bit1_space = 0;
                uint16_t bit2_mark = 0, bit2_space = 0, bit3_mark = 0, bit3_space = 0;
                uint16_t bit4_mark = 0, bit4_space = 0, bit5_mark = 0, bit5_space = 0;
                uint16_t packet;
                uint8_t index;
                
                for (uint8_t i = 0; i < (ir_action_protocol_len >> 1); i++) {
                    index = i << 1;     // i * 2
                    char* found = strchr(baseRaw_dic, prot[index]);
                    packet = (found - baseRaw_dic) * IR_CODE_LEN * IR_CODE_SCALE;
                    
                    found = strchr(baseRaw_dic, prot[index + 1]);
                    packet += (found - baseRaw_dic) * IR_CODE_SCALE;

                    printf("%s%5d ", i & 1 ? "-" : "+", packet);
                    
                    switch (i) {
                        case IR_CODE_HEADER_MARK_POS:
                            ir_code[0] = packet;
                            break;
                            
                        case IR_CODE_HEADER_SPACE_POS:
                            ir_code[1] = packet;
                            break;
                            
                        case IR_CODE_BIT0_MARK_POS:
                            bit0_mark = packet;
                            break;
                            
                        case IR_CODE_BIT0_SPACE_POS:
                            bit0_space = packet;
                            break;
                            
                        case IR_CODE_BIT1_MARK_POS:
                            bit1_mark = packet;
                            break;
                            
                        case IR_CODE_BIT1_SPACE_POS:
                            bit1_space = packet;
                            break;
                            
                        case IR_CODE_BIT2_MARK_POS:
                            if (ir_action_protocol_len == IR_ACTION_PROTOCOL_LEN_2BITS) {
                                ir_code[ir_code_len - 1] = packet;
                            } else {
                                bit2_mark = packet;
                            }
                            break;
                                
                        case IR_CODE_BIT2_SPACE_POS:
                            bit2_space = packet;
                            break;
                                
                        case IR_CODE_BIT3_MARK_POS:
                            bit3_mark = packet;
                            break;
                                
                        case IR_CODE_BIT3_SPACE_POS:
                            bit3_space = packet;
                            break;
                            
                        case IR_CODE_BIT4_MARK_POS:
                            if (ir_action_protocol_len == IR_ACTION_PROTOCOL_LEN_4BITS) {
                                ir_code[ir_code_len - 1] = packet;
                            } else {
                                bit4_mark = packet;
                            }
                            break;
                                    
                        case IR_CODE_BIT4_SPACE_POS:
                            bit4_space = packet;
                            break;
                                
                        case IR_CODE_BIT5_MARK_POS:
                            bit5_mark = packet;
                            break;
                                
                        case IR_CODE_BIT5_SPACE_POS:
                            bit5_space = packet;
                            break;
                            
                        case IR_CODE_FOOTER_MARK_POS_6BITS:
                            ir_code[ir_code_len - 1] = packet;
                            break;
                            
                        default:
                            // Do nothing
                            break;
                    }
                }
                
                // Decoding BIT code part
                uint16_t ir_code_index = 2;
                
                void fill_code(const uint16_t count, const uint16_t bit_mark, const uint16_t bit_space) {
                    for (uint16_t j = 0; j < count; j++) {
                        ir_code[ir_code_index] = bit_mark;
                        ir_code_index++;
                        ir_code[ir_code_index] = bit_space;
                        ir_code_index++;
                    }
                }
                
                for (uint16_t i = 0; i < json_ir_code_len; i++) {
                    char* found = strchr(baseUC_dic, action_ir_tx->prot_code[i]);
                    if (found) {
                        switch (ir_action_protocol_len) {
                            case IR_ACTION_PROTOCOL_LEN_4BITS:
                                if (found - baseUC_dic < 13) {
                                    fill_code(1 + found - baseUC_dic, bit1_mark, bit1_space);
                                } else {
                                    fill_code(found - baseUC_dic - 12, bit3_mark, bit3_space);
                                }
                                break;
                                
                            case IR_ACTION_PROTOCOL_LEN_6BITS:
                                if (found - baseUC_dic < 9) {
                                    fill_code(1 + found - baseUC_dic, bit1_mark, bit1_space);
                                } else if (found - baseUC_dic < 18) {
                                    fill_code(found - baseUC_dic - 8, bit3_mark, bit3_space);
                                } else {
                                    fill_code(found - baseUC_dic - 17, bit5_mark, bit5_space);
                                }
                                break;
                                
                            default:    // case IR_ACTION_PROTOCOL_LEN_2BITS:
                                fill_code(1 + found - baseUC_dic, bit1_mark, bit1_space);
                                break;
                        }
                        
                    } else {
                        found = strchr(baseLC_dic, action_ir_tx->prot_code[i]);
                        switch (ir_action_protocol_len) {
                            case IR_ACTION_PROTOCOL_LEN_4BITS:
                                if (found - baseLC_dic < 13) {
                                    fill_code(1 + found - baseLC_dic, bit0_mark, bit0_space);
                                } else {
                                    fill_code(found - baseLC_dic - 12, bit2_mark, bit2_space);
                                }
                                break;
                                
                            case IR_ACTION_PROTOCOL_LEN_6BITS:
                                if (found - baseLC_dic < 9) {
                                    fill_code(1 + found - baseLC_dic, bit0_mark, bit0_space);
                                } else if (found - baseLC_dic < 18) {
                                    fill_code(found - baseLC_dic - 8, bit2_mark, bit2_space);
                                } else {
                                    fill_code(found - baseLC_dic - 17, bit4_mark, bit4_space);
                                }
                                break;
                                
                            default:    // case IR_ACTION_PROTOCOL_LEN_2BITS:
                                fill_code(1 + found - baseLC_dic, bit0_mark, bit0_space);
                                break;
                        }
                    }
                }
                
                INFO("\nIR code: %s", action_ir_tx->prot_code);
                for (uint16_t i = 0; i < ir_code_len; i++) {
                    printf("%s%5d ", i & 1 ? "-" : "+", ir_code[i]);
                    if (i % 16 == 15) {
                        INFO("");
                    }

                }
                INFO("");
                
            } else {    // IR_ACTION_RAW_CODE
                const uint16_t json_ir_code_len = strlen(action_ir_tx->raw_code);
                ir_code_len = json_ir_code_len >> 1;
                
                ir_code = malloc(sizeof(uint16_t) * ir_code_len);
                
                INFO("IR packet (%i)", ir_code_len);

                uint16_t index, packet;
                for (uint16_t i = 0; i < ir_code_len; i++) {
                    index = i << 1;
                    char* found = strchr(baseRaw_dic, action_ir_tx->raw_code[index]);
                    packet = (found - baseRaw_dic) * IR_CODE_LEN * IR_CODE_SCALE;
                    
                    found = strchr(baseRaw_dic, action_ir_tx->raw_code[index + 1]);
                    packet += (found - baseRaw_dic) * IR_CODE_SCALE;

                    ir_code[i] = packet;

                    printf("%s%5d ", i & 1 ? "-" : "+", packet);
                    if (i % 16 == 15) {
                        INFO("");
                    }
                }
                
                INFO("");
            }
            
            // IR TRANSMITTER
            uint32_t start;
            const bool ir_true = true ^ main_config.ir_tx_inv;
            const bool ir_false = false ^ main_config.ir_tx_inv;

            for (uint8_t r = 0; r < action_ir_tx->repeats; r++) {
                
                taskENTER_CRITICAL();
                
                for (uint16_t i = 0; i < ir_code_len; i++) {
                    if (ir_code[i] > 0) {
                        if (i & 1) {    // Space
                            gpio_write(main_config.ir_tx_gpio, ir_false);
                            sdk_os_delay_us(ir_code[i]);
                        } else {        // Mark
                            start = sdk_system_get_time();
                            while ((sdk_system_get_time() - start) < ir_code[i]) {
                                gpio_write(main_config.ir_tx_gpio, ir_true);
                                sdk_os_delay_us(freq);
                                gpio_write(main_config.ir_tx_gpio, ir_false);
                                sdk_os_delay_us(freq);
                            }
                        }
                    }
                }
                
                gpio_write(main_config.ir_tx_gpio, ir_false);

                taskEXIT_CRITICAL();
                
                INFO("IR %i sent", r);
                
                vTaskDelay(pdMS_TO_TICKS(action_ir_tx->pause));
            }
            
            if (ir_code) {
                free(ir_code);
            }
        }
        
        action_ir_tx = action_ir_tx->next;
    }
    
    free(pvParameters);
    vTaskDelete(NULL);
}

// --- UART action task
void uart_action_task(void* pvParameters) {
    action_task_t* action_task = pvParameters;
    action_uart_t* action_uart = action_task->ch_group->action_uart;

    while (action_uart) {
        if (action_uart->action == action_task->action) {
            INFO("UART Action");
            
            taskENTER_CRITICAL();
            
            for (uint8_t i = 0; i < action_uart->len; i++) {
                uart_putc(action_uart->uart, action_uart->command[i]);
            }

            uart_flush_txfifo(action_uart->uart);
            
            taskEXIT_CRITICAL();
        }
        
        if (action_uart->pause > 0) {
            vTaskDelay(pdMS_TO_TICKS(action_uart->pause));
        }
        
        action_uart = action_uart->next;
    }
    
    free(pvParameters);
    vTaskDelete(NULL);
}

// --- ACTIONS
void autoswitch_timer(TimerHandle_t xTimer) {
    action_relay_t* action_relay = (action_relay_t*) pvTimerGetTimerID(xTimer);

    gpio_write(action_relay->gpio, !action_relay->value);
    INFO("AutoSw digO GPIO %i -> %i", action_relay->gpio, !action_relay->value);
    
    xTimerDelete(xTimer, XTIMER_BLOCK_TIME);
}

void do_actions(ch_group_t* ch_group, uint8_t action) {
    INFO("Exec Action: acc %i, action %i", ch_group->accessory, action);
    
    // Copy actions
    action_copy_t* action_copy = ch_group->action_copy;
    while(action_copy) {
        if (action_copy->action == action) {
            action = action_copy->new_action;
            action_copy = NULL;
        } else {
            action_copy = action_copy->next;
        }
    }
    
    // Digital outputs
    action_relay_t* action_relay = ch_group->action_relay;
    while(action_relay) {
        if (action_relay->action == action) {
            gpio_write(action_relay->gpio, action_relay->value);
            INFO("Digital Output: gpio %i, val %i, inch %g", action_relay->gpio, action_relay->value, action_relay->inching);
            
            if (action_relay->inching > 0) {
                xTimerStart(xTimerCreate(0, pdMS_TO_TICKS(action_relay->inching * 1000), pdFALSE, (void*) action_relay, autoswitch_timer), XTIMER_BLOCK_TIME);
            }
        }

        action_relay = action_relay->next;
    }
    
    // Accessory Manager
    action_acc_manager_t* action_acc_manager = ch_group->action_acc_manager;
    while(action_acc_manager) {
        if (action_acc_manager->action == action) {
            ch_group_t* ch_group = ch_group_find_by_acc(action_acc_manager->accessory);
            if (ch_group) {
                INFO("Acc Manager: target %i, val %g", action_acc_manager->accessory, action_acc_manager->value);
                
                if (action_acc_manager->value == -10000.f) {
                    ch_group->main_enabled = false;
                } else if (action_acc_manager->value == -10001.f) {
                    ch_group->main_enabled = true;
                } else if (action_acc_manager->value == -20000.f) {
                    ch_group->child_enabled = false;
                } else if (action_acc_manager->value == -20001.f) {
                    ch_group->child_enabled = true;
                } else {
                    switch (ch_group->acc_type) {
                        case ACC_TYPE_BUTTON:
                            button_event(0, ch_group, (uint8_t) action_acc_manager->value);
                            break;
                            
                        case ACC_TYPE_LOCK:
                            if ((int8_t) action_acc_manager->value < 2) {
                                hkc_lock_setter(ch_group->ch1, HOMEKIT_UINT8((uint8_t) action_acc_manager->value));
                            } else {
                                hkc_lock_setter(ch_group->ch3, HOMEKIT_UINT8(((uint8_t) action_acc_manager->value) - 2));
                            }
                            break;
                            
                        case ACC_TYPE_CONTACT_SENSOR:
                        case ACC_TYPE_DOORBELL:
                            if ((bool) action_acc_manager->value) {
                                sensor_1(0, ch_group, TYPE_SENSOR);
                            } else {
                                sensor_0(0, ch_group, TYPE_SENSOR);
                            }
                            break;
                            
                        case ACC_TYPE_MOTION_SENSOR:
                            if ((bool) action_acc_manager->value) {
                                sensor_1(0, ch_group, TYPE_SENSOR_BOOL);
                            } else {
                                sensor_0(0, ch_group, TYPE_SENSOR_BOOL);
                            }
                            break;
                            
                        case ACC_TYPE_WATER_VALVE:
                            if ((int8_t) action_acc_manager->value == -1) {
                                if (ch_group->ch3) {
                                    ch_group->ch3->value = ch_group->ch2->value;
                                }
                            } else {
                                hkc_valve_setter(ch_group->ch0, HOMEKIT_UINT8((uint8_t) action_acc_manager->value));
                            }
                            break;
                            
                        case ACC_TYPE_THERMOSTAT:
                            if (action_acc_manager->value == 0.02f) {
                                update_th(ch_group->ch2, HOMEKIT_UINT8(0));
                            } else if (action_acc_manager->value == 0.03f) {
                                update_th(ch_group->ch2, HOMEKIT_UINT8(1));
                            } else if (action_acc_manager->value == 0.04f) {
                                update_th(ch_group->ch5, HOMEKIT_UINT8(2));
                            } else if (action_acc_manager->value == 0.05f) {
                                update_th(ch_group->ch5, HOMEKIT_UINT8(1));
                            } else if (action_acc_manager->value == 0.06f) {
                                update_th(ch_group->ch5, HOMEKIT_UINT8(0));
                            } else {
                                if (((uint16_t) (action_acc_manager->value * 100) % 2) == 0) {
                                    update_th(ch_group->ch6, HOMEKIT_FLOAT(action_acc_manager->value));
                                } else {
                                    update_th(ch_group->ch7, HOMEKIT_FLOAT(action_acc_manager->value - 0.01f));
                                }
                            }
                            break;
                            
                        case ACC_TYPE_GARAGE_DOOR:
                            if ((uint8_t) action_acc_manager->value < 2) {
                                hkc_garage_door_setter(ch_group->ch1, HOMEKIT_UINT8((uint8_t) action_acc_manager->value));
                            } else if ((uint8_t) action_acc_manager->value == 2) {
                                garage_door_stop(0, ch_group, 0);
                            } else {
                                garage_door_obstruction(0, ch_group, (uint8_t) action_acc_manager->value - 3);
                            }
                            break;
                            
                        case ACC_TYPE_LIGHTBULB:
                            hkc_rgbw_setter(ch_group->ch0, HOMEKIT_BOOL((bool) action_acc_manager->value));
                            break;
                            
                        case ACC_TYPE_WINDOW_COVER:
                            if ((int8_t) action_acc_manager->value < 0) {
                                window_cover_obstruction(0, ch_group, (uint8_t) action_acc_manager->value + 2);
                            } else if ((uint8_t) action_acc_manager->value > 100) {
                                hkc_window_cover_setter(WINDOW_COVER_CH_TARGET_POSITION, WINDOW_COVER_CH_CURRENT_POSITION->value);
                            } else {
                                hkc_window_cover_setter(WINDOW_COVER_CH_TARGET_POSITION, HOMEKIT_UINT8(action_acc_manager->value));
                            }
                            break;
                            
                        case ACC_TYPE_FAN:
                            if ((uint8_t) action_acc_manager->value == 0) {
                                hkc_fan_setter(ch_group->ch0, HOMEKIT_BOOL(false));
                            } else if ((uint8_t) action_acc_manager->value > 100) {
                                hkc_fan_setter(ch_group->ch0, HOMEKIT_BOOL(true));
                            } else {
                                hkc_fan_speed_setter(ch_group->ch1, HOMEKIT_FLOAT(action_acc_manager->value));
                            }
                            break;
                            
                        case ACC_TYPE_TV:
                            if ((uint8_t) action_acc_manager->value < 2 && (uint8_t) action_acc_manager->value >= 0) {
                                hkc_tv_active(ch_group->ch0, HOMEKIT_UINT8(action_acc_manager->value));
                            } else if ((uint8_t) action_acc_manager->value < 20) {
                                hkc_tv_key(ch_group->ch3, HOMEKIT_UINT8((uint8_t) action_acc_manager->value - 2));
                            } else if ((uint8_t) action_acc_manager->value < 22) {
                                hkc_tv_mute(ch_group->ch5, HOMEKIT_BOOL((bool) (action_acc_manager->value - 20)));
                            } else if ((uint8_t) action_acc_manager->value < 24) {
                                hkc_tv_volume(ch_group->ch7, HOMEKIT_UINT8((uint8_t) action_acc_manager->value - 22));
                            } else if ((uint8_t) action_acc_manager->value < 32) {
                                hkc_tv_power_mode(ch_group->ch4, HOMEKIT_UINT8((uint8_t) action_acc_manager->value - 30));
                            } else if ((uint8_t) action_acc_manager->value > 100) {
                                hkc_tv_active_identifier(ch_group->ch2, HOMEKIT_UINT8((uint8_t) action_acc_manager->value - 100));
                            }
                            break;
                            
                        default:    // ON Type ch
                            if ((int8_t) action_acc_manager->value == -1) {
                                if (ch_group->ch2) {
                                    ch_group->ch2->value = ch_group->ch1->value;
                                }
                            } else {
                                hkc_on_setter(ch_group->ch0, HOMEKIT_BOOL((bool) action_acc_manager->value));
                            }
                            break;
                    }
                }
            } else {
                ERROR("Target not found");
            }
        }

        action_acc_manager = action_acc_manager->next;
    }
    
    // System actions
    action_system_t* action_system = ch_group->action_system;
    while(action_system) {
        if (action_system->action == action) {
            INFO("System Action: val %i", action_system->value);
            
            char* ota = NULL;
            
            switch (action_system->value) {
                case SYSTEM_ACTION_SETUP_MODE:
                    setup_mode_call(0, NULL, 0);
                    break;
                    
                case SYSTEM_ACTION_OTA_UPDATE:
                    if (sysparam_get_string("ota_repo", &ota) == SYSPARAM_OK) {
                        rboot_set_temp_rom(1);
                        reboot_haa();
                    }
                    break;
                    
                default:    // case SYSTEM_ACTION_REBOOT:
                    reboot_haa();
                    break;
            }
        }
        
        action_system = action_system->next;
    }
    
    // UART actions
    if (ch_group->action_uart) {
        action_task_t* action_task = new_action_task();
        action_task->action = action;
        action_task->ch_group = ch_group;
        
        if (xTaskCreate(uart_action_task, "uart_action", UART_ACTION_TASK_SIZE, action_task, UART_ACTION_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("Creating uart_action_task");
        }
    }
    
    // HTTP GET actions
    if (ch_group->action_http) {
        action_task_t* action_task = new_action_task();
        action_task->action = action;
        action_task->ch_group = ch_group;
        
        if (xTaskCreate(http_get_task, "http_get", HTTP_GET_TASK_SIZE, action_task, HTTP_GET_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("Creating http_get_task");
        }
    }
    
    // IR TX actions
    if (ch_group->action_ir_tx) {
        action_task_t* action_task = new_action_task();
        action_task->action = action;
        action_task->ch_group = ch_group;
        
        if (xTaskCreate(ir_tx_task, "ir_tx", IR_TX_TASK_SIZE, action_task, IR_TX_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("Creating ir_tx_task");
        }
    }
}

void do_wildcard_actions(ch_group_t* ch_group, uint8_t index, const float action_value) {
    INFO("Wildcard %i %.2f", index, action_value);
    float last_value, last_diff = 1000000;
    wildcard_action_t* wildcard_action = ch_group->wildcard_action;
    wildcard_action_t* last_wildcard_action = NULL;
    
    while (wildcard_action) {
        if (wildcard_action->index == index) {
            const float diff = action_value - wildcard_action->value;

            if (wildcard_action->value <= action_value && diff < last_diff) {
                last_value = wildcard_action->value;
                last_diff = diff;
                last_wildcard_action = wildcard_action;
            }
        }
        
        wildcard_action = wildcard_action->next;
    }
    
    if (last_wildcard_action != NULL) {
        if (ch_group->last_wildcard_action[index] != last_value || last_wildcard_action->repeat) {
            ch_group->last_wildcard_action[index] = last_value;
            INFO("Wilcard Action %i %.2f", index, last_value);
            do_actions(ch_group, last_wildcard_action->target_action);
        }
    }
}

// --- IDENTIFY
void identify(homekit_value_t _value) {
    led_blink(6);
    INFO("ID");
}

// ---------

void delayed_sensor_starter_task() {
    ch_group_t* ch_group = main_config.ch_groups;
    
    bool is_first = true;
    
    while (ch_group) {
        if ((ch_group->acc_type == ACC_TYPE_THERMOSTAT ||
            ch_group->acc_type == ACC_TYPE_TEMP_SENSOR ||
            ch_group->acc_type == ACC_TYPE_HUM_SENSOR ||
            ch_group->acc_type == ACC_TYPE_TH_SENSOR) &&
            ch_group->timer) {
            
            if (!is_first) {
                vTaskDelay(pdMS_TO_TICKS(3000));
            }
            is_first = false;
            
            INFO("Starting delayed sensor for acc %i", ch_group->accessory);
            
            temperature_timer_worker(ch_group->timer);
            xTimerStart(ch_group->timer, XTIMER_BLOCK_TIME);
        }

        ch_group = ch_group->next;
    }
    
    vTaskDelete(NULL);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, NULL);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER, "José A. Jiménez Campos");
homekit_characteristic_t model = HOMEKIT_CHARACTERISTIC_(MODEL, "RavenSystem HAA");
homekit_characteristic_t identify_function = HOMEKIT_CHARACTERISTIC_(IDENTIFY, identify);
homekit_characteristic_t firmware = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, FIRMWARE_VERSION);
homekit_characteristic_t hap_version = HOMEKIT_CHARACTERISTIC_(VERSION, "1.1.0");

homekit_server_config_t config;

void run_homekit_server() {
    main_config.wifi_channel = sdk_wifi_get_channel();

    FREEHEAP();
    
    if (main_config.enable_homekit_server) {
        INFO("Start HK Server");

        homekit_server_init(&config);
        FREEHEAP();
    }
    
    WIFI_RECONNECTION_TIMER = xTimerCreate(0, pdMS_TO_TICKS(WIFI_RECONNECTION_POLL_PERIOD_MS), pdTRUE, NULL, wifi_reconnection);
    WIFI_WATCHDOG_TIMER = xTimerCreate(0, pdMS_TO_TICKS(WIFI_WATCHDOG_POLL_PERIOD_MS), pdTRUE, NULL, wifi_watchdog);
    xTimerStart(WIFI_WATCHDOG_TIMER, XTIMER_BLOCK_TIME);
    
    do_actions(ch_group_find_by_acc(ACC_TYPE_ROOT_DEVICE), 2);

    if (main_config.ping_inputs) {
        xTimerStart(xTimerCreate(0, pdMS_TO_TICKS(main_config.ping_poll_period * 1000.000f), pdTRUE, NULL, ping_task_timer_worker), XTIMER_BLOCK_TIME);
    }
    
    led_blink(6);
}

void printf_header() {
    INFO("\n\n");
    INFO("Home Accessory Architect v%s", FIRMWARE_VERSION);
    INFO("(c) 2019-2020 José Antonio Jiménez Campos\n");
    
#ifdef HAA_DEBUG
    INFO("HAA DEBUG ENABLED\n");
#endif  // HAA_DEBUG
}

void normal_mode_init() {
    char* txt_config = NULL;
    sysparam_get_string(HAA_JSON_SYSPARAM, &txt_config);

    cJSON* json_haa = cJSON_Parse(txt_config);

    cJSON* json_config = cJSON_GetObjectItemCaseSensitive(json_haa, GENERAL_CONFIG);
    cJSON* json_accessories = cJSON_GetObjectItemCaseSensitive(json_haa, ACCESSORIES);
    
    const uint8_t total_accessories = cJSON_GetArraySize(json_accessories);
    
    if (total_accessories == 0) {
        uart_set_baud(0, 115200);
        printf_header();
        INFO("JSON:\n %s\n", txt_config ? txt_config : "none");
        ERROR("Invalid JSON\n");
        sysparam_set_int8(TOTAL_ACC_SYSPARAM, 0);
        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 2);
        
        INFO("Rebooting...\n\n");
        vTaskDelay(pdMS_TO_TICKS(200));
        sdk_system_restart();
        
        vTaskDelete(NULL);
    }
    
    bool get_used_gpio(const uint8_t gpio) {
        switch (gpio) {
            case 1:
                return main_config.used_gpio_1;
                
            case 2:
                return main_config.used_gpio_2;
                
            case 3:
                return main_config.used_gpio_3;
                
            case 4:
                return main_config.used_gpio_4;
                
            case 5:
                return main_config.used_gpio_5;
                
            case 9:
                return main_config.used_gpio_9;
                
            case 10:
                return main_config.used_gpio_10;
                
            case 11:
                return main_config.used_gpio_11;
                
            case 12:
                return main_config.used_gpio_12;
                
            case 13:
                return main_config.used_gpio_13;
                
            case 14:
                return main_config.used_gpio_14;
                
            case 15:
                return main_config.used_gpio_15;
                
            case 16:
                return main_config.used_gpio_16;
                
            case 17:
                return main_config.used_gpio_17;
                
            default:    // case 0:
                return main_config.used_gpio_0;
        }
    }
    
    void set_used_gpio(const uint8_t gpio) {
        switch (gpio) {
            case 1:
                main_config.used_gpio_1 = true;
                break;
                
            case 2:
                main_config.used_gpio_2 = true;
                break;
                
            case 3:
                main_config.used_gpio_3 = true;
                break;
                
            case 4:
                main_config.used_gpio_4 = true;
                break;
                
            case 5:
                main_config.used_gpio_5 = true;
                break;
                
            case 9:
                main_config.used_gpio_9 = true;
                break;
                
            case 10:
                main_config.used_gpio_10 = true;
                break;
                
            case 11:
                main_config.used_gpio_11 = true;
                break;
                
            case 12:
                main_config.used_gpio_12 = true;
                break;
                
            case 13:
                main_config.used_gpio_13 = true;
                break;
                
            case 14:
                main_config.used_gpio_14 = true;
                break;
                
            case 15:
                main_config.used_gpio_15 = true;
                break;
                
            case 16:
                main_config.used_gpio_16 = true;
                break;
                
            case 17:
                main_config.used_gpio_17 = true;
                break;
                
            default:    // case 0:
                main_config.used_gpio_0 = true;
                break;
        }
    }
    
    // Buttons GPIO Setup function
    bool diginput_register(cJSON* json_buttons, void* callback, ch_group_t* ch_group, const uint8_t param) {
        bool run_at_launch = false;
        
        for (uint8_t j = 0; j < cJSON_GetArraySize(json_buttons); j++) {
            const uint8_t gpio = (uint8_t) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), PIN_GPIO)->valuedouble;
            bool pullup_resistor = true;
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), PULLUP_RESISTOR) != NULL &&
                cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), PULLUP_RESISTOR)->valuedouble == 0) {
                pullup_resistor = false;
            }
            
            bool inverted = false;
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), INVERTED) != NULL &&
                cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), INVERTED)->valuedouble == 1) {
                inverted = true;
            }
            
            uint8_t button_filter = 0;
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), BUTTON_FILTER) != NULL) {
                button_filter = (uint8_t) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), BUTTON_FILTER)->valuedouble;
            }
            
            uint8_t button_type = 1;
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), BUTTON_PRESS_TYPE) != NULL) {
                button_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), BUTTON_PRESS_TYPE)->valuedouble;
            }
            
            if (!get_used_gpio(gpio)) {
                change_uart_gpio(gpio);
                adv_button_create(gpio, pullup_resistor, inverted);
                
                if (button_filter > 0) {
                    adv_button_set_gpio_probes(gpio, button_filter);
                }
                
                set_used_gpio(gpio);
            }
            adv_button_register_callback_fn(gpio, callback, button_type, (void*) ch_group, param);
            
            INFO("New Digital Input: gpio %i, type %i, inv %i, filter %i", gpio, button_type, inverted, button_filter);
             
            if (gpio_read(gpio) == button_type) {
                run_at_launch = true;
            }
        }
        
        return run_at_launch;
    }
    
    // Ping Setup function
    void ping_register(cJSON* json_pings, void* callback, ch_group_t* ch_group, const uint8_t param) {
        for(uint8_t j = 0; j < cJSON_GetArraySize(json_pings); j++) {
            ping_input_t* ping_input = ping_input_find_by_host(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_pings, j), PING_HOST)->valuestring);
            
            if (!ping_input) {
                ping_input = malloc(sizeof(ping_input_t));
                memset(ping_input, 0, sizeof(*ping_input));
                
                ping_input->host = strdup(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_pings, j), PING_HOST)->valuestring);
                ping_input->last_response = false;
                
                ping_input->next = main_config.ping_inputs;
                main_config.ping_inputs = ping_input;
            }
            
            bool response_type = true;
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_pings, j), PING_RESPONSE_TYPE) != NULL) {
                response_type = (bool) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_pings, j), PING_RESPONSE_TYPE)->valuedouble;
            }
            
            ping_input->ignore_last_response = false;
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_pings, j), PING_IGNORE_LAST_RESPONSE) != NULL) {
                ping_input->ignore_last_response = (bool) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_pings, j), PING_IGNORE_LAST_RESPONSE)->valuedouble;
            }
            
            ping_input_callback_fn_t* ping_input_callback_fn;
            ping_input_callback_fn = malloc(sizeof(ping_input_callback_fn_t));
            memset(ping_input_callback_fn, 0, sizeof(*ping_input_callback_fn));
            
            ping_input_callback_fn->disable_without_wifi = false;
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_pings, j), PING_DISABLE_WITHOUT_WIFI) != NULL) {
                ping_input_callback_fn->disable_without_wifi = (bool) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_pings, j), PING_DISABLE_WITHOUT_WIFI)->valuedouble;
            }
            
            ping_input_callback_fn->callback = callback;
            ping_input_callback_fn->ch_group = ch_group;
            ping_input_callback_fn->param = param;
            
            if (response_type) {
                ping_input_callback_fn->next = ping_input->callback_1;
                ping_input->callback_1 = ping_input_callback_fn;
            } else {
                ping_input_callback_fn->next = ping_input->callback_0;
                ping_input->callback_0 = ping_input_callback_fn;
            }
            
            INFO("New Ping Input: host %s, res %i", ping_input->host, response_type);
        }
    }
    
    // Initial state function
    double set_initial_state(const uint8_t accessory, const uint8_t ch_number, cJSON* json_context, homekit_characteristic_t* ch, const uint8_t ch_type, const double default_value) {
        double state = default_value;
        INFO("Set init state");
        if (cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE) != NULL) {
            const uint8_t initial_state = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE)->valuedouble;
            if (initial_state < INIT_STATE_LAST) {
                    state = initial_state;
            } else {
                char* saved_state_id = malloc(3);
                uint16_t int_saved_state_id = ((accessory + 10) * 10) + ch_number;
                
                itoa(int_saved_state_id, saved_state_id, 10);
                last_state_t* last_state = malloc(sizeof(last_state_t));
                memset(last_state, 0, sizeof(*last_state));
                last_state->id = saved_state_id;
                last_state->ch = ch;
                last_state->ch_type = ch_type;
                last_state->next = main_config.last_states;
                main_config.last_states = last_state;
                
                sysparam_status_t status;
                bool saved_state_bool = false;
                int8_t saved_state_int8;
                int32_t saved_state_int32;
                char* saved_state_string = NULL;
                
                switch (ch_type) {
                    case CH_TYPE_INT8:
                        status = sysparam_get_int8(saved_state_id, &saved_state_int8);
                        
                        if (status == SYSPARAM_OK) {
                            state = saved_state_int8;
                        }
                        break;
                        
                    case CH_TYPE_INT32:
                        status = sysparam_get_int32(saved_state_id, &saved_state_int32);
                        
                        if (status == SYSPARAM_OK) {
                            state = saved_state_int32;
                        }
                        break;
                        
                    case CH_TYPE_FLOAT:
                        status = sysparam_get_int32(saved_state_id, &saved_state_int32);
                        
                        if (status == SYSPARAM_OK) {
                            state = saved_state_int32 / 100.00f;
                        }
                        break;
                        
                    case CH_TYPE_STRING:
                        status = sysparam_get_string(saved_state_id, &saved_state_string);
                        
                        if (status == SYSPARAM_OK) {
                            state = (uint32_t) saved_state_string;
                        }
                        break;
                        
                    default:    // case CH_TYPE_BOOL
                        status = sysparam_get_bool(saved_state_id, &saved_state_bool);
                        
                        if (status == SYSPARAM_OK) {
                            if (initial_state == INIT_STATE_LAST) {
                                state = saved_state_bool;
                            } else if (ch_type == CH_TYPE_BOOL) {    // initial_state == INIT_STATE_INV_LAST
                                state = !saved_state_bool;
                            }
                        }
                        break;
                }
                
                if (status != SYSPARAM_OK) {
                    ERROR("Init state: not saved, using default");
                }
                
                if (ch_type == CH_TYPE_STRING && state > 0) {
                    INFO("Init state: %s", (char*) (uint32_t) state);
                } else {
                    INFO("Init state: %g", state);
                }
                
            }
        }
        
        return state;
    }
    
    // REGISTER ACTIONS
    // Copy actions
    inline void new_action_copy(ch_group_t* ch_group, cJSON* json_context, uint8_t fixed_action) {
        action_copy_t* last_action = ch_group->action_copy;
        
        void register_action(cJSON* json_accessory, uint8_t new_int_action) {
            char action[3];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                cJSON* json_action = cJSON_GetObjectItemCaseSensitive(json_accessory, action);
                if (cJSON_GetObjectItemCaseSensitive(json_action, COPY_ACTIONS) != NULL) {
                    action_copy_t* action_copy = malloc(sizeof(action_copy_t));
                    memset(action_copy, 0, sizeof(*action_copy));
                    
                    action_copy->action = new_int_action;
                    action_copy->new_action = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_action, COPY_ACTIONS)->valuedouble;
                    
                    action_copy->next = last_action;
                    last_action = action_copy;
                    
                    INFO("New Copy Action %i: val %i", new_int_action, action_copy->new_action);
                }
            }
        }
        
        if (fixed_action < MAX_ACTIONS) {
            for (uint8_t int_action = 0; int_action < MAX_ACTIONS; int_action++) {
                register_action(json_context, int_action);
            }
        } else {
            register_action(json_context, fixed_action);
        }
        
        ch_group->action_copy = last_action;
    }
    
    // Digital outputs
    inline void new_action_relay(ch_group_t* ch_group, cJSON* json_context, uint8_t fixed_action) {
        action_relay_t* last_action = ch_group->action_relay;
        
        void register_action(cJSON* json_accessory, uint8_t new_int_action) {
            char action[3];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), DIGITAL_OUTPUTS_ARRAY) != NULL) {
                    cJSON* json_relays = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), DIGITAL_OUTPUTS_ARRAY);
                    for (int16_t i = cJSON_GetArraySize(json_relays) - 1; i >= 0; i--) {
                        action_relay_t* action_relay = malloc(sizeof(action_relay_t));
                        memset(action_relay, 0, sizeof(*action_relay));
                        
                        cJSON* json_relay = cJSON_GetArrayItem(json_relays, i);
                        
                        action_relay->action = new_int_action;
                        
                        action_relay->value = false;
                        if (cJSON_GetObjectItemCaseSensitive(json_relay, VALUE) != NULL) {
                            action_relay->value = (bool) cJSON_GetObjectItemCaseSensitive(json_relay, VALUE)->valuedouble;
                        }
                        
                        action_relay->gpio = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_relay, PIN_GPIO)->valuedouble;
                        if (!get_used_gpio(action_relay->gpio)) {
                            change_uart_gpio(action_relay->gpio);
                            gpio_enable(action_relay->gpio, GPIO_OUTPUT);
                            gpio_write(action_relay->gpio, action_relay->value);
                            
                            set_used_gpio(action_relay->gpio);
                            
                            INFO("New GPIO Output %i set to %i", action_relay->gpio, action_relay->value);
                        }

                        action_relay->inching = 0;
                        if (cJSON_GetObjectItemCaseSensitive(json_relay, AUTOSWITCH_TIME) != NULL) {
                            action_relay->inching = (float) cJSON_GetObjectItemCaseSensitive(json_relay, AUTOSWITCH_TIME)->valuedouble;
                        }
                        
                        action_relay->next = last_action;
                        last_action = action_relay;
                        
                        INFO("New Digital Output Action %i: gpio %i, val %i, inch %g", new_int_action, action_relay->gpio, action_relay->value, action_relay->inching);
                    }
                }
            }
        }
        
        if (fixed_action < MAX_ACTIONS) {
            for (uint8_t int_action = 0; int_action < MAX_ACTIONS; int_action++) {
                register_action(json_context, int_action);
            }
        } else {
            register_action(json_context, fixed_action);
        }
        
        ch_group->action_relay = last_action;
    }
    
    // Accessory Manager
    inline void new_action_acc_manager(ch_group_t* ch_group, cJSON* json_context, uint8_t fixed_action) {
        action_acc_manager_t* last_action = ch_group->action_acc_manager;
        
        void register_action(cJSON* json_accessory, uint8_t new_int_action) {
            char action[3];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), MANAGE_OTHERS_ACC_ARRAY) != NULL) {
                    cJSON* json_acc_managers = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), MANAGE_OTHERS_ACC_ARRAY);
                    
                    for (int16_t i = cJSON_GetArraySize(json_acc_managers) - 1; i >= 0; i--) {
                        action_acc_manager_t* action_acc_manager = malloc(sizeof(action_acc_manager_t));
                        memset(action_acc_manager, 0, sizeof(*action_acc_manager));
                        
                        cJSON* json_acc_manager = cJSON_GetArrayItem(json_acc_managers, i);
                        
                        action_acc_manager->action = new_int_action;
                        
                        action_acc_manager->accessory = 1;
                        if (cJSON_GetObjectItemCaseSensitive(json_acc_manager, ACCESSORY_INDEX) != NULL) {
                            action_acc_manager->accessory = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_acc_manager, ACCESSORY_INDEX)->valuedouble;
                        }

                        action_acc_manager->value = 0;
                        if (cJSON_GetObjectItemCaseSensitive(json_acc_manager, VALUE) != NULL) {
                            action_acc_manager->value = (float) cJSON_GetObjectItemCaseSensitive(json_acc_manager, VALUE)->valuedouble;
                        }
                        
                        action_acc_manager->next = last_action;
                        last_action = action_acc_manager;
                        
                        INFO("New Acc Manager Action %i: acc %i, val %g", new_int_action, action_acc_manager->accessory, action_acc_manager->value);
                    }
                }
            }
        }
        
        if (fixed_action < MAX_ACTIONS) {
            for (uint8_t int_action = 0; int_action < MAX_ACTIONS; int_action++) {
                register_action(json_context, int_action);
            }
        } else {
            register_action(json_context, fixed_action);
        }
        
        ch_group->action_acc_manager = last_action;
    }
    
    // System Actions
    inline void new_action_system(ch_group_t* ch_group, cJSON* json_context, uint8_t fixed_action) {
        action_system_t* last_action = ch_group->action_system;
        
        void register_action(cJSON* json_accessory, uint8_t new_int_action) {
            char action[3];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), SYSTEM_ACTIONS_ARRAY) != NULL) {
                    cJSON* json_action_systems = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), SYSTEM_ACTIONS_ARRAY);
                    for (int16_t i = cJSON_GetArraySize(json_action_systems) - 1; i >= 0; i--) {
                        action_system_t* action_system = malloc(sizeof(action_system_t));
                        memset(action_system, 0, sizeof(*action_system));
                        
                        cJSON* json_action_system = cJSON_GetArrayItem(json_action_systems, i);
                        
                        action_system->action = new_int_action;
                        
                        action_system->value = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_action_system, SYSTEM_ACTION)->valuedouble;
                        
                        action_system->next = last_action;
                        last_action = action_system;
                        
                        INFO("New System Action %i: val %i", new_int_action, action_system->value);
                    }
                }
            }
        }
        
        if (fixed_action < MAX_ACTIONS) {
            for (uint8_t int_action = 0; int_action < MAX_ACTIONS; int_action++) {
                register_action(json_context, int_action);
            }
        } else {
            register_action(json_context, fixed_action);
        }
        
        ch_group->action_system = last_action;
    }
    
    // HTTP/TCP Actions
    inline void new_action_http(ch_group_t* ch_group, cJSON* json_context, uint8_t fixed_action) {
        action_http_t* last_action = ch_group->action_http;
        
        void register_action(cJSON* json_accessory, uint8_t new_int_action) {
            char action[3];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), HTTP_ACTIONS_ARRAY) != NULL) {
                    cJSON* json_action_https = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), HTTP_ACTIONS_ARRAY);
                    for (int16_t i = cJSON_GetArraySize(json_action_https) - 1; i >= 0; i--) {
                        action_http_t* action_http = malloc(sizeof(action_http_t));
                        memset(action_http, 0, sizeof(*action_http));
                        
                        cJSON* json_action_http = cJSON_GetArrayItem(json_action_https, i);
                        
                        action_http->action = new_int_action;
                        
                        action_http->host = strdup(cJSON_GetObjectItemCaseSensitive(json_action_http, HTTP_ACTION_HOST)->valuestring);
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_http, HTTP_ACTION_URL) != NULL) {
                            action_http->url = strdup(cJSON_GetObjectItemCaseSensitive(json_action_http, HTTP_ACTION_URL)->valuestring);
                        } else {
                            action_http->url = strdup("");
                        }
                        
                        action_http->port_n = 80;
                        if (cJSON_GetObjectItemCaseSensitive(json_action_http, HTTP_ACTION_PORT) != NULL) {
                            action_http->port_n = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_action_http, HTTP_ACTION_PORT)->valuedouble;
                        }
                        
                        action_http->method_n = 0;
                        if (cJSON_GetObjectItemCaseSensitive(json_action_http, HTTP_ACTION_METHOD) != NULL) {
                            action_http->method_n = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_action_http, HTTP_ACTION_METHOD)->valuedouble;
                        }
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_http, HTTP_ACTION_CONTENT) != NULL) {
                            action_http->content = strdup(cJSON_GetObjectItemCaseSensitive(json_action_http, HTTP_ACTION_CONTENT)->valuestring);
                        } else {
                            action_http->content = strdup("");
                        }
                        
                        INFO("New HTTP/TCP Action %i: host %s:%i, method %i, content %s", new_int_action, action_http->url, action_http->port_n, action_http->method_n, action_http->content);
                        
                        if (action_http->method_n == 3 ) {
                            action_http->len = strlen(action_http->content);
                        } else if (action_http->method_n == 4) {
                            action_http->method_n = 3;
                            free(action_http->content);
                            action_http->len = process_hexstr(cJSON_GetObjectItemCaseSensitive(json_action_http, HTTP_ACTION_CONTENT)->valuestring, &action_http->content);
                        }
                        
                        action_http->next = last_action;
                        last_action = action_http;
                    }
                }
            }
        }
        
        if (fixed_action < MAX_ACTIONS) {
            for (uint8_t int_action = 0; int_action < MAX_ACTIONS; int_action++) {
                register_action(json_context, int_action);
            }
        } else {
            register_action(json_context, fixed_action);
        }
        
        ch_group->action_http = last_action;
    }
    
    // IR TX Actions
    inline void new_action_ir_tx(ch_group_t* ch_group, cJSON* json_context, uint8_t fixed_action) {
        action_ir_tx_t* last_action = ch_group->action_ir_tx;
        
        void register_action(cJSON* json_accessory, uint8_t new_int_action) {
            char action[3];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), IR_ACTIONS_ARRAY) != NULL) {
                    cJSON* json_action_ir_txs = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), IR_ACTIONS_ARRAY);
                    for (int16_t i = cJSON_GetArraySize(json_action_ir_txs) - 1; i >= 0; i--) {
                        action_ir_tx_t* action_ir_tx = malloc(sizeof(action_ir_tx_t));
                        memset(action_ir_tx, 0, sizeof(*action_ir_tx));
                        
                        cJSON* json_action_ir_tx = cJSON_GetArrayItem(json_action_ir_txs, i);
                        
                        action_ir_tx->action = new_int_action;
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_ir_tx, IR_ACTION_PROTOCOL) != NULL) {
                            action_ir_tx->prot = strdup(cJSON_GetObjectItemCaseSensitive(json_action_ir_tx, IR_ACTION_PROTOCOL)->valuestring);
                        }
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_ir_tx, IR_ACTION_PROTOCOL_CODE) != NULL) {
                            action_ir_tx->prot_code = strdup(cJSON_GetObjectItemCaseSensitive(json_action_ir_tx, IR_ACTION_PROTOCOL_CODE)->valuestring);
                        }

                        if (cJSON_GetObjectItemCaseSensitive(json_action_ir_tx, IR_ACTION_RAW_CODE) != NULL) {
                            action_ir_tx->raw_code = strdup(cJSON_GetObjectItemCaseSensitive(json_action_ir_tx, IR_ACTION_RAW_CODE)->valuestring);
                        }
                        
                        action_ir_tx->freq = 0;
                        if (cJSON_GetObjectItemCaseSensitive(json_action_ir_tx, IR_ACTION_FREQ) != NULL) {
                            action_ir_tx->freq = 1000 / cJSON_GetObjectItemCaseSensitive(json_action_ir_tx, IR_ACTION_FREQ)->valuedouble / 2;
                        }
                        
                        action_ir_tx->repeats = 1;
                        if (cJSON_GetObjectItemCaseSensitive(json_action_ir_tx, IR_ACTION_REPEATS) != NULL) {
                            action_ir_tx->repeats = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_action_ir_tx, IR_ACTION_REPEATS)->valuedouble;
                        }
                        
                        action_ir_tx->pause = 100;
                        if (cJSON_GetObjectItemCaseSensitive(json_action_ir_tx, IR_ACTION_REPEATS_PAUSE) != NULL) {
                            action_ir_tx->pause = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_action_ir_tx, IR_ACTION_REPEATS_PAUSE)->valuedouble;
                        }
                        
                        action_ir_tx->next = last_action;
                        last_action = action_ir_tx;
                        
                        INFO("New IR TX Action %i: repeats %i, pause %i", new_int_action, action_ir_tx->repeats, action_ir_tx->pause);
                    }
                }
            }
        }
        
        if (fixed_action < MAX_ACTIONS) {
            for (uint8_t int_action = 0; int_action < MAX_ACTIONS; int_action++) {
                register_action(json_context, int_action);
            }
        } else {
            register_action(json_context, fixed_action);
        }
        
        ch_group->action_ir_tx = last_action;
    }
    
    // UART Actions
    inline void new_action_uart(ch_group_t* ch_group, cJSON* json_context, uint8_t fixed_action) {
        action_uart_t* last_action = ch_group->action_uart;
        
        void register_action(cJSON* json_accessory, uint8_t new_int_action) {
            char action[3];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), UART_ACTIONS_ARRAY) != NULL) {
                    cJSON* json_action_uarts = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), UART_ACTIONS_ARRAY);
                    for (int16_t i = cJSON_GetArraySize(json_action_uarts) - 1; i >= 0; i--) {
                        action_uart_t* action_uart = malloc(sizeof(action_uart_t));
                        memset(action_uart, 0, sizeof(*action_uart));
                        
                        cJSON* json_action_uart = cJSON_GetArrayItem(json_action_uarts, i);
                        
                        action_uart->action = new_int_action;
                        
                        action_uart->uart = 0;
                        if (cJSON_GetObjectItemCaseSensitive(json_action_uart, UART_ACTION_UART) != NULL) {
                            action_uart->uart = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_action_uart, UART_ACTION_UART)->valuedouble;
                        }
                        
                        action_uart->pause = 0;
                        if (cJSON_GetObjectItemCaseSensitive(json_action_uart, UART_ACTION_PAUSE) != NULL) {
                            action_uart->uart = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_action_uart, UART_ACTION_PAUSE)->valuedouble;
                        }
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_uart, VALUE) != NULL) {
                            action_uart->len = process_hexstr(cJSON_GetObjectItemCaseSensitive(json_action_uart, VALUE)->valuestring, &action_uart->command);
                        }
                        
                        action_uart->next = last_action;
                        last_action = action_uart;
                        
                        INFO("New UART Action %i: uart %i, pause %i, len %i", new_int_action, action_uart->uart, action_uart->pause, action_uart->len);
                    }
                }
            }
        }
        
        if (fixed_action < MAX_ACTIONS) {
            for (uint8_t int_action = 0; int_action < MAX_ACTIONS; int_action++) {
                register_action(json_context, int_action);
            }
        } else {
            register_action(json_context, fixed_action);
        }
        
        ch_group->action_uart = last_action;
    }
    
    void register_actions(ch_group_t* ch_group, cJSON* json_accessory, uint8_t fixed_action) {
        new_action_copy(ch_group, json_accessory, fixed_action);
        new_action_relay(ch_group, json_accessory, fixed_action);
        new_action_acc_manager(ch_group, json_accessory, fixed_action);
        new_action_system(ch_group, json_accessory, fixed_action);
        new_action_http(ch_group, json_accessory, fixed_action);
        new_action_ir_tx(ch_group, json_accessory, fixed_action);
        new_action_uart(ch_group, json_accessory, fixed_action);
    }
    
    void register_wildcard_actions(ch_group_t* ch_group, cJSON* json_accessory) {
        uint8_t global_index = MAX_ACTIONS;     // First wirldcard action must have a higher index than highest possible normal action
        wildcard_action_t* last_action = ch_group->wildcard_action;
        
        for (uint8_t int_index = 0; int_index < MAX_WILDCARD_ACTIONS; int_index++) {
            char number[2];
            itoa(int_index, number, 10);
            
            char index[3];
            snprintf(index, 3, "%s%s", WILDCARD_ACTIONS_ARRAY_HEADER, number);
            
            cJSON* json_wilcard_actions = cJSON_GetObjectItemCaseSensitive(json_accessory, index);
            for (uint8_t i = 0; i < cJSON_GetArraySize(json_wilcard_actions); i++) {
                wildcard_action_t* wildcard_action = malloc(sizeof(wildcard_action_t));
                memset(wildcard_action, 0, sizeof(*wildcard_action));
                
                cJSON* json_wilcard_action = cJSON_GetArrayItem(json_wilcard_actions, i);
                
                wildcard_action->index = int_index;
                wildcard_action->value = (float) cJSON_GetObjectItemCaseSensitive(json_wilcard_action, VALUE)->valuedouble;
                
                wildcard_action->repeat = false;
                if (cJSON_GetObjectItemCaseSensitive(json_wilcard_action, WILDCARD_ACTION_REPEAT) != NULL) {
                    wildcard_action->repeat = (bool) cJSON_GetObjectItemCaseSensitive(json_wilcard_action, WILDCARD_ACTION_REPEAT)->valuedouble;
                }
                
                if (cJSON_GetObjectItemCaseSensitive(json_wilcard_action, "0") != NULL) {
                    char action[3];
                    itoa(global_index, action, 10);
                    cJSON* json_new_input_action = cJSON_CreateObject();
                    cJSON_AddItemReferenceToObject(json_new_input_action, action, cJSON_GetObjectItemCaseSensitive(json_wilcard_action, "0"));
                    register_actions(ch_group, json_new_input_action, global_index);
                    cJSON_Delete(json_new_input_action);
                }
                
                wildcard_action->target_action = global_index;
                global_index++;
                
                wildcard_action->next = last_action;
                last_action = wildcard_action;
            }
        }
        
        ch_group->wildcard_action = last_action;
    }
    
    // REGISTER ACCESSORY CONFIGURATION
    bool acc_homekit_enabled(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, ENABLE_HOMEKIT_SERVER) != NULL) {
            return (bool) cJSON_GetObjectItemCaseSensitive(json_accessory, ENABLE_HOMEKIT_SERVER)->valuedouble;
        }
        return true;
    }
    
    float autoswitch_time(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, AUTOSWITCH_TIME) != NULL) {
            return (float) cJSON_GetObjectItemCaseSensitive(json_accessory, AUTOSWITCH_TIME)->valuedouble;
        }
        return 0;
    }
    
    float autoswitch_time_1(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, AUTOSWITCH_TIME_1) != NULL) {
            return (float) cJSON_GetObjectItemCaseSensitive(json_accessory, AUTOSWITCH_TIME_1)->valuedouble;
        }
        return 0;
    }
    
    int8_t th_sensor_gpio(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_GPIO) != NULL) {
            return (uint8_t) cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_GPIO)->valuedouble;
        }
        return -1;
    }
    
    uint8_t th_sensor_type(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_TYPE) != NULL) {
            return (uint8_t) cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_TYPE)->valuedouble;
        }
        return 2;
    }
    
    float th_sensor_temp_offset(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_OFFSET) != NULL) {
            return (float) cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_OFFSET)->valuedouble;
        }
        return 0;
    }
    
    float th_sensor_hum_offset(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, HUMIDITY_OFFSET) != NULL) {
            return (float) cJSON_GetObjectItemCaseSensitive(json_accessory, HUMIDITY_OFFSET)->valuedouble;
        }
        return 0;
    }
    
    uint8_t th_sensor_index(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_INDEX) != NULL) {
            return (uint8_t) cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_INDEX)->valuedouble;
        }
        return 1;
    }
    
    float sensor_poll_period(cJSON* json_accessory, float poll_period) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_POLL_PERIOD) != NULL) {
            poll_period = (float) cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_POLL_PERIOD)->valuedouble;
            
            if (poll_period < TH_SENSOR_POLL_PERIOD_MIN) {
                poll_period = TH_SENSOR_POLL_PERIOD_MIN;
            }
        }
        
        return poll_period;
    }
    
    void th_sensor(ch_group_t* ch_group, cJSON* json_accessory) {
        TH_SENSOR_GPIO = th_sensor_gpio(json_accessory);
        TH_SENSOR_TYPE = th_sensor_type(json_accessory);
        TH_SENSOR_TEMP_OFFSET = th_sensor_temp_offset(json_accessory);
        TH_SENSOR_HUM_OFFSET = th_sensor_hum_offset(json_accessory);
        if ((uint8_t) TH_SENSOR_TYPE == 3) {
            TH_SENSOR_INDEX = th_sensor_index(json_accessory);
        }
        TH_SENSOR_POLL_PERIOD = sensor_poll_period(json_accessory, TH_SENSOR_POLL_PERIOD_DEFAULT);
        TH_SENSOR_ERROR_COUNT = 0;
    }
    
    void th_sensor_starter(ch_group_t* ch_group) {
        ch_group->timer = xTimerCreate(0, pdMS_TO_TICKS(TH_SENSOR_POLL_PERIOD * 1000), pdTRUE, (void*) ch_group, temperature_timer_worker);
    }
    
    // ----- CONFIG SECTION
    
    // UART configuration
    bool used_uart[2];
    used_uart[0] = false;
    used_uart[1] = false;

    if (cJSON_GetObjectItemCaseSensitive(json_config, UART_CONFIG_ARRAY) != NULL) {
        cJSON* json_uarts = cJSON_GetObjectItemCaseSensitive(json_config, UART_CONFIG_ARRAY);
        for (uint8_t i = 0; i < cJSON_GetArraySize(json_uarts); i++) {
            cJSON* json_uart = cJSON_GetArrayItem(json_uarts, i);
            
            if (cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_ENABLE) != NULL) {
                uint8_t uart_config = (int8_t) cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_ENABLE)->valuedouble;
                
                if (uart_config == 2) {
                    sdk_system_uart_swap();
                    uart_config = 0;
                }
                
                uint32_t speed = 115200;
                if (cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_SPEED) != NULL) {
                    speed = (uint32_t) cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_SPEED)->valuedouble;
                }
                
                uint8_t stopbits = 0;
                if (cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_STOPBITS) != NULL) {
                    stopbits = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_STOPBITS)->valuedouble;
                }
                
                uint8_t parity = 0;
                if (cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_PARITY) != NULL) {
                    parity = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_PARITY)->valuedouble;
                }
                
                if (uart_config == 1) {
                    gpio_set_iomux_function(2, IOMUX_GPIO2_FUNC_UART1_TXD);
                }
                
                uart_set_baud(uart_config, speed);
                
                switch (stopbits) {
                    case 1:
                        uart_set_stopbits(uart_config, UART_STOPBITS_1);
                        break;
                        
                    case 2:
                        uart_set_stopbits(uart_config, UART_STOPBITS_1_5);
                        break;
                        
                    case 3:
                        uart_set_stopbits(uart_config, UART_STOPBITS_2);
                        break;
                        
                    default:    // case 0:
                        uart_set_stopbits(uart_config, UART_STOPBITS_0);
                        break;
                }

                switch (parity) {
                    case 1:
                        uart_set_parity_enabled(uart_config, true);
                        uart_set_parity(uart_config, UART_PARITY_ODD);
                        break;
                        
                    case 2:
                        uart_set_parity_enabled(uart_config, true);
                        uart_set_parity(uart_config, UART_PARITY_EVEN);
                        break;
                        
                    default:    // case 0:
                        uart_set_parity_enabled(uart_config, false);
                        break;
                }
                
                if (uart_config == 0) {
                    used_uart[0] = true;
                } else {    // uart_config == 1
                    used_uart[1] = true;
                }
            }
        
        }
    }
    
    // Log output type
    uint8_t log_output_type = 0;
    if (cJSON_GetObjectItemCaseSensitive(json_config, LOG_OUTPUT) != NULL) {
        log_output_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, LOG_OUTPUT)->valuedouble;
        
        if ((log_output_type == 1 || log_output_type == 4) && !used_uart[0]) {
            uart_set_baud(0, 115200);
        } else if ((log_output_type == 2 || log_output_type == 5) && !used_uart[1]) {
            uart_set_baud(1, 115200);
        }
    }
    
    adv_logger_init(log_output_type);

    printf_header();
    INFO("NORMAL MODE\n\nJSON:\n %s\n", txt_config);
    
    free(txt_config);

    // Custom Hostname
    char* custom_hostname = name.value.string_value;
    if (cJSON_GetObjectItemCaseSensitive(json_config, CUSTOM_HOSTNAME) != NULL) {
        custom_hostname = strdup(cJSON_GetObjectItemCaseSensitive(json_config, CUSTOM_HOSTNAME)->valuestring);
    }
    INFO("Hostname: %s", custom_hostname);
    
    // Status LED
    if (cJSON_GetObjectItemCaseSensitive(json_config, STATUS_LED_GPIO) != NULL) {
        main_config.led_gpio = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, STATUS_LED_GPIO)->valuedouble;

        bool led_inverted = true;
        if (cJSON_GetObjectItemCaseSensitive(json_config, INVERTED) != NULL) {
            led_inverted = (bool) cJSON_GetObjectItemCaseSensitive(json_config, INVERTED)->valuedouble;
        }
        
        change_uart_gpio(main_config.led_gpio);
        led_create(main_config.led_gpio, led_inverted);
        INFO("Status LED GPIO: %i, inv: %i", main_config.led_gpio, led_inverted);
    }
    
    // IR TX LED Frequency
    if (cJSON_GetObjectItemCaseSensitive(json_config, IR_ACTION_FREQ) != NULL) {
        main_config.ir_tx_freq = 1000 / cJSON_GetObjectItemCaseSensitive(json_config, IR_ACTION_FREQ)->valuedouble / 2;
        INFO("IR TX Freq: %i", main_config.ir_tx_freq);
    }
    
    // IR TX LED Inverted
    if (cJSON_GetObjectItemCaseSensitive(json_config, IR_ACTION_TX_GPIO_INVERTED) != NULL) {
        main_config.ir_tx_inv = (bool) cJSON_GetObjectItemCaseSensitive(json_config, IR_ACTION_TX_GPIO_INVERTED)->valuedouble;
        INFO("IR TX Inv: %i", main_config.ir_tx_inv);
    }
    
    // IR TX LED GPIO
    if (cJSON_GetObjectItemCaseSensitive(json_config, IR_ACTION_TX_GPIO) != NULL) {
        main_config.ir_tx_gpio = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, IR_ACTION_TX_GPIO)->valuedouble;
        change_uart_gpio(main_config.ir_tx_gpio);
        gpio_enable(main_config.ir_tx_gpio, GPIO_OUTPUT);
        set_used_gpio(main_config.ir_tx_gpio);
        gpio_write(main_config.ir_tx_gpio, false ^ main_config.ir_tx_inv);
        INFO("IR TX GPIO: %i", main_config.ir_tx_gpio);
    }
    
    // Button filter
    if (cJSON_GetObjectItemCaseSensitive(json_config, BUTTON_FILTER) != NULL) {
        uint8_t button_filter_value = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, BUTTON_FILTER)->valuedouble;
        adv_button_set_evaluate_delay(button_filter_value);
        INFO("Button filter: %i", button_filter_value);
    }
    
    // PWM frequency
    if (cJSON_GetObjectItemCaseSensitive(json_config, PWM_FREQ) != NULL) {
        main_config.pwm_freq = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_config, PWM_FREQ)->valuedouble;
    }
    INFO("PWM Freq: %i", main_config.pwm_freq);
    
    // Ping poll period
    if (cJSON_GetObjectItemCaseSensitive(json_config, PING_POLL_PERIOD) != NULL) {
        main_config.ping_poll_period = (float) cJSON_GetObjectItemCaseSensitive(json_config, PING_POLL_PERIOD)->valuedouble;
    }
    INFO("Ping period: %g secs", main_config.ping_poll_period);
    
    // Allowed Setup Mode Time
    if (cJSON_GetObjectItemCaseSensitive(json_config, ALLOWED_SETUP_MODE_TIME) != NULL) {
        main_config.setup_mode_time = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_config, ALLOWED_SETUP_MODE_TIME)->valuedouble;
    }
    INFO("Setup mode time: %i secs", main_config.setup_mode_time);
    
    // Run HomeKit Server
    if (cJSON_GetObjectItemCaseSensitive(json_config, ENABLE_HOMEKIT_SERVER) != NULL) {
        main_config.enable_homekit_server = (bool) cJSON_GetObjectItemCaseSensitive(json_config, ENABLE_HOMEKIT_SERVER)->valuedouble;
    }
    INFO("Run HomeKit Server: %i", main_config.enable_homekit_server);
    
    // Allow unsecure connections
    if (cJSON_GetObjectItemCaseSensitive(json_config, ALLOW_INSECURE_CONNECTIONS) != NULL) {
        config.insecure = (bool) cJSON_GetObjectItemCaseSensitive(json_config, ALLOW_INSECURE_CONNECTIONS)->valuedouble;
    }
    INFO("Unsecure connections: %i", config.insecure);
    
    // mDNS TTL
    config.mdns_ttl = MDNS_TTL_DEFAULT;
    if (cJSON_GetObjectItemCaseSensitive(json_config, MDNS_TTL) != NULL) {
        config.mdns_ttl = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_config, MDNS_TTL)->valuedouble;
    }
    INFO("mDNS TTL: %i secs", config.mdns_ttl);
    
    // Gateway Ping
    if (cJSON_GetObjectItemCaseSensitive(json_config, WIFI_PING_ERRORS) != NULL) {
        main_config.wifi_ping_max_errors = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, WIFI_PING_ERRORS)->valuedouble;
    }
    INFO("Gateway Ping: %i", main_config.wifi_ping_max_errors);
    
    // Common serial prefix
    char* common_serial_prefix = NULL;
    if (cJSON_GetObjectItemCaseSensitive(json_config, SERIAL_STRING) != NULL) {
        common_serial_prefix = cJSON_GetObjectItemCaseSensitive(json_config, SERIAL_STRING)->valuestring;
        INFO("Common serial prefix: %s", common_serial_prefix);
    }
    
    // Times to toggle quickly an accessory status to enter setup mode
    if (cJSON_GetObjectItemCaseSensitive(json_config, SETUP_MODE_ACTIVATE_COUNT) != NULL) {
        main_config.setup_mode_toggle_counter_max = (int8_t) cJSON_GetObjectItemCaseSensitive(json_config, SETUP_MODE_ACTIVATE_COUNT)->valuedouble;
    }
    INFO("Toggles to enter setup mode: %i", main_config.setup_mode_toggle_counter_max);
    
    if (main_config.setup_mode_toggle_counter_max > 0) {
        main_config.setup_mode_toggle_timer = xTimerCreate(0, pdMS_TO_TICKS(SETUP_MODE_TOGGLE_TIME_MS), pdFALSE, NULL, setup_mode_toggle);
    }
    
    // Buttons to enter setup mode
    diginput_register(cJSON_GetObjectItemCaseSensitive(json_config, BUTTONS_ARRAY), setup_mode_call, NULL, 0);
    
    // ----- END CONFIG SECTION
    
    uint8_t hk_total_ac = 1;
    bool bridge_needed = false;

    for (uint8_t i = 0; i < total_accessories; i++) {
        if (acc_homekit_enabled(cJSON_GetArrayItem(json_accessories, i))) {
            hk_total_ac += 1;
        }
    }
    
    if (total_accessories > ACCESSORIES_WITHOUT_BRIDGE || bridge_needed) {
        // Bridge needed
        bridge_needed = true;
        hk_total_ac += 1;
    }
    
    // Saved States Timer Function
    main_config.save_states_timer = xTimerCreate(0, pdMS_TO_TICKS(SAVE_STATES_DELAY_MS), pdFALSE, NULL, save_states);
    
    homekit_accessory_t** accessories = calloc(hk_total_ac, sizeof(homekit_accessory_t*));
    
    // Define services and characteristics
    uint8_t accessory_numerator = 1;
    uint8_t acc_count = 0;
    
    // HomeKit last config number
    int last_config_number = 1;
    sysparam_get_int32(LAST_CONFIG_NUMBER, &last_config_number);
    
    void new_accessory(const uint8_t accessory, uint8_t services, const bool homekit_enabled, cJSON* json_context) {
        if (!homekit_enabled) {
            accessory_numerator++;
            return;
        }
        
        uint8_t serial_acc = accessory_numerator;
        accessory_numerator++;
        
        if (acc_count == 0) {
            services++;
            
            if (bridge_needed) {
                serial_acc = 0;
            }
        }

        char* serial_prefix = NULL;
        if (json_context && cJSON_GetObjectItemCaseSensitive(json_context, SERIAL_STRING) != NULL) {
            serial_prefix = cJSON_GetObjectItemCaseSensitive(json_context, SERIAL_STRING)->valuestring;
        } else {
            serial_prefix = common_serial_prefix;
        }
        
        uint8_t serial_prefix_len = SERIAL_STRING_LEN;
        bool use_config_number = false;
        if (serial_prefix) {
            serial_prefix_len += strlen(serial_prefix) + 1;
            
            if (strcmp(serial_prefix, "cn") == 0) {
                serial_prefix_len += 4;
                use_config_number = true;
            }
        }

        char* serial_str = malloc(serial_prefix_len);
        uint8_t macaddr[6];
        sdk_wifi_get_macaddr(STATION_IF, macaddr);
        
        if (use_config_number) {
            snprintf(serial_str, serial_prefix_len, "%i-%02X%02X%02X-%i", last_config_number, macaddr[3], macaddr[4], macaddr[5], serial_acc);
        } else {
            snprintf(serial_str, serial_prefix_len, "%s%s%02X%02X%02X-%i", serial_prefix ? serial_prefix : "", serial_prefix ? "-" : "", macaddr[3], macaddr[4], macaddr[5], serial_acc);
        }
        
        INFO("Serial: %s", serial_str);
        
        homekit_characteristic_t* serial = NEW_HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, serial_str);
        //serial.value = HOMEKIT_STRING(serial_str);
        
        accessories[accessory] = calloc(1, sizeof(homekit_accessory_t));
        accessories[accessory]->id = accessory + 1;
        accessories[accessory]->services = calloc(services, sizeof(homekit_service_t*));
        
        accessories[accessory]->services[0] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[0]->id = 1;
        accessories[accessory]->services[0]->type = HOMEKIT_SERVICE_ACCESSORY_INFORMATION;
        accessories[accessory]->services[0]->characteristics = calloc(7, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[0]->characteristics[0] = &name;
        accessories[accessory]->services[0]->characteristics[1] = &manufacturer;
        accessories[accessory]->services[0]->characteristics[2] = serial;
        accessories[accessory]->services[0]->characteristics[3] = &model;
        accessories[accessory]->services[0]->characteristics[4] = &firmware;
        accessories[accessory]->services[0]->characteristics[5] = &identify_function;
        
        if (acc_count == 0) {
            INFO("HAP v1.1.0");
            services -= 2;
            accessories[accessory]->services[services] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[services]->id = 100;
            accessories[accessory]->services[services]->type = HOMEKIT_SERVICE_HAP_INFORMATION;
            accessories[accessory]->services[services]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[services]->characteristics[0] = &hap_version;
        }
    }

    void set_accessory_ir_protocol(ch_group_t* ch_group, cJSON* json_context) {
        if (cJSON_GetObjectItemCaseSensitive(json_context, IR_ACTION_PROTOCOL) != NULL) {
            ch_group->ir_protocol = strdup(cJSON_GetObjectItemCaseSensitive(json_context, IR_ACTION_PROTOCOL)->valuestring);
        }
    }
    
    bool get_exec_actions_on_boot(cJSON* json_context) {
        bool exec_actions_on_boot = true;
        if (cJSON_GetObjectItemCaseSensitive(json_context, EXEC_ACTIONS_ON_BOOT) != NULL) {
            exec_actions_on_boot = (bool) cJSON_GetObjectItemCaseSensitive(json_context, EXEC_ACTIONS_ON_BOOT)->valuedouble;
        }
        
        INFO("Exec Ac on Boot: %i", exec_actions_on_boot);
        
        return exec_actions_on_boot;
    }
    
    uint8_t get_initial_state(cJSON* json_context) {
        uint8_t initial_state = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE) != NULL) {
            initial_state = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE)->valuedouble;
        }
        
        INFO("Init State: %i", initial_state);
        
        return initial_state;
    }


    uint8_t new_switch(uint8_t accessory, cJSON* json_context, const uint8_t acc_type) {
        ch_group_t* ch_group = new_ch_group();
        ch_group->accessory = accessory_numerator;
        ch_group->homekit_enabled = acc_homekit_enabled(json_context);
        
        new_accessory(accessory, 3, ch_group->homekit_enabled, json_context);
        
        homekit_characteristic_t* ch0 = NEW_HOMEKIT_CHARACTERISTIC(ON, false, .setter_ex=hkc_on_setter);
        
        uint32_t max_duration = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, VALVE_MAX_DURATION) != NULL) {
            max_duration = (uint32_t) cJSON_GetObjectItemCaseSensitive(json_context, VALVE_MAX_DURATION)->valuedouble;
        }
        
        ch_group->acc_type = acc_type;
        ch_group->ch0 = ch0;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        ch_group->num[0] = autoswitch_time(json_context);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[1]->id = 8;
            accessories[accessory]->services[1]->primary = true;
        }
        
        uint8_t ch_calloc = 2;
        if (max_duration > 0) {
            ch_calloc += 2;
        }
        
        if (acc_type == ACC_TYPE_SWITCH) {
            if (ch_group->homekit_enabled) {
                accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_SWITCH;
                accessories[accessory]->services[1]->characteristics = calloc(ch_calloc, sizeof(homekit_characteristic_t*));
            }
            
        } else {    // acc_type == ACC_TYPE_OUTLET
            ch_calloc++;
            
            homekit_characteristic_t* ch3 = NEW_HOMEKIT_CHARACTERISTIC(OUTLET_IN_USE, true);
            
            if (ch_group->homekit_enabled) {
                accessories[accessory]->services[1]->characteristics = calloc(ch_calloc, sizeof(homekit_characteristic_t*));
                accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_OUTLET;
                accessories[accessory]->services[1]->characteristics[1] = ch3;
            }
            
            if (max_duration > 0) {
                ch_group->ch3 = ch3;
            } else {
                ch_group->ch1 = ch3;
            }
        }
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1]->characteristics[0] = ch0;
        }
        
        if (max_duration > 0) {
            homekit_characteristic_t* ch1 = NEW_HOMEKIT_CHARACTERISTIC(SET_DURATION, max_duration, .max_value=(float[]) {max_duration}, .setter_ex=hkc_setter);
            homekit_characteristic_t* ch2 = NEW_HOMEKIT_CHARACTERISTIC(REMAINING_DURATION, 0, .max_value=(float[]) {max_duration});
            
            ch_group->ch1 = ch1;
            ch_group->ch2 = ch2;
            
            if (ch_group->homekit_enabled) {
                accessories[accessory]->services[1]->characteristics[ch_calloc - 3] = ch1;
                accessories[accessory]->services[1]->characteristics[ch_calloc - 2] = ch2;
            }
            
            const uint32_t initial_time = (uint32_t) set_initial_state(ch_group->accessory, 1, cJSON_Parse(INIT_STATE_LAST_STR), ch1, CH_TYPE_INT32, max_duration);
            if (initial_time > max_duration) {
                ch1->value.int_value = max_duration;
            } else {
                ch1->value.int_value = initial_time;
            }
            
            ch_group->timer = xTimerCreate(0, pdMS_TO_TICKS(1000), pdTRUE, (void*) ch0, on_timer_worker);
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, TYPE_ON);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, TYPE_ON);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput_1, ch_group, TYPE_ON);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput_0, ch_group, TYPE_ON);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_1), digstate_1, ch_group, TYPE_ON);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_0), digstate_0, ch_group, TYPE_ON);
        
        const bool exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch_group, TYPE_ON);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch_group, TYPE_ON);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate_1, ch_group, TYPE_ON);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate_0, ch_group, TYPE_ON);
            
            ch0->value.bool_value = !((bool) set_initial_state(ch_group->accessory, 0, json_context, ch0, CH_TYPE_BOOL, 0));
            if (exec_actions_on_boot) {
                hkc_on_setter(ch0, HOMEKIT_BOOL(!ch0->value.bool_value));
            } else {
                hkc_on_status_setter(ch0, HOMEKIT_BOOL(!ch0->value.bool_value));
            }
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch_group, TYPE_ON)) {
                if (exec_actions_on_boot) {
                    diginput_1(0, ch_group, TYPE_ON);
                } else {
                    digstate_1(0, ch_group, TYPE_ON);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch_group, TYPE_ON)) {
                ch0->value = HOMEKIT_BOOL(true);
                if (exec_actions_on_boot) {
                    diginput_0(0, ch_group, TYPE_ON);
                } else {
                    digstate_0(0, ch_group, TYPE_ON);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate_1, ch_group, TYPE_ON)) {
                digstate_1(0, ch_group, TYPE_ON);
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate_0, ch_group, TYPE_ON)) {
                ch0->value = HOMEKIT_BOOL(true);
                digstate_0(0, ch_group, TYPE_ON);
            }
        }
        
        if (ch_group->homekit_enabled) {
            return accessory + 1;
        }
        return accessory;
    }
    
    uint8_t new_button_event(uint8_t accessory, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group();
        ch_group->accessory = accessory_numerator;
        ch_group->homekit_enabled = acc_homekit_enabled(json_context);
        
        new_accessory(accessory, 3, ch_group->homekit_enabled, json_context);

        homekit_characteristic_t* ch0 = NEW_HOMEKIT_CHARACTERISTIC(PROGRAMMABLE_SWITCH_EVENT, 0);

        ch_group->acc_type = ACC_TYPE_BUTTON;
        ch_group->ch0 = ch0;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[1]->id = 8;
            accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_STATELESS_PROGRAMMABLE_SWITCH;
            accessories[accessory]->services[1]->primary = true;
            accessories[accessory]->services[1]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[1]->characteristics[0] = ch0;
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), button_event, ch_group, SINGLEPRESS_EVENT);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), button_event, ch_group, DOUBLEPRESS_EVENT);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2), button_event, ch_group, LONGPRESS_EVENT);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), button_event, ch_group, SINGLEPRESS_EVENT);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), button_event, ch_group, DOUBLEPRESS_EVENT);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_2), button_event, ch_group, LONGPRESS_EVENT);
        
        if (ch_group->homekit_enabled) {
            return accessory + 1;
        }
        return accessory;
    }
    
    uint8_t new_lock(const uint8_t accessory, cJSON* json_context, const uint8_t acc_type) {
        ch_group_t* ch_group = new_ch_group();
        ch_group->accessory = accessory_numerator;
        ch_group->homekit_enabled = acc_homekit_enabled(json_context);
 
        if (acc_type == ACC_TYPE_DOUBLE_LOCK) {
            new_accessory(accessory, 4, ch_group->homekit_enabled, json_context);
        } else {
            new_accessory(accessory, 3, ch_group->homekit_enabled, json_context);
        }
        
        homekit_characteristic_t* ch0 = NEW_HOMEKIT_CHARACTERISTIC(LOCK_CURRENT_STATE, 1);
        homekit_characteristic_t* ch1 = NEW_HOMEKIT_CHARACTERISTIC(LOCK_TARGET_STATE, 1, .setter_ex=hkc_lock_setter);
        homekit_characteristic_t* ch2 = NULL;
        homekit_characteristic_t* ch3 = NULL;
        
        if (acc_type == ACC_TYPE_DOUBLE_LOCK) {
            ch2 = NEW_HOMEKIT_CHARACTERISTIC(LOCK_CURRENT_STATE, 1);
            ch3 = NEW_HOMEKIT_CHARACTERISTIC(LOCK_TARGET_STATE, 1, .setter_ex=hkc_lock_setter);
        }
        
        ch_group->acc_type = ACC_TYPE_LOCK;
        ch_group->ch0 = ch0;
        ch_group->ch1 = ch1;
        ch_group->ch2 = ch2;
        ch_group->ch3 = ch3;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        ch_group->num[0] = autoswitch_time(json_context);
        ch_group->num[1] = autoswitch_time_1(json_context);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[1]->id = 8;
            accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_LOCK_MECHANISM;
            accessories[accessory]->services[1]->primary = true;
            accessories[accessory]->services[1]->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[1]->characteristics[0] = ch0;
            accessories[accessory]->services[1]->characteristics[1] = ch1;
            
            if (acc_type == ACC_TYPE_DOUBLE_LOCK) {
                accessories[accessory]->services[2] = calloc(1, sizeof(homekit_service_t));
                accessories[accessory]->services[2]->id = 12;
                accessories[accessory]->services[2]->type = HOMEKIT_SERVICE_LOCK_MECHANISM;
                accessories[accessory]->services[2]->primary = false;
                accessories[accessory]->services[2]->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                accessories[accessory]->services[2]->characteristics[0] = ch2;
                accessories[accessory]->services[2]->characteristics[1] = ch3;
            }
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, TYPE_LOCK);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, TYPE_LOCK);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput_1, ch_group, TYPE_LOCK);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput_0, ch_group, TYPE_LOCK);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_1), digstate_1, ch_group, TYPE_LOCK);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_0), digstate_0, ch_group, TYPE_LOCK);
        
        if (acc_type == ACC_TYPE_DOUBLE_LOCK) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY_1), diginput, ch_group, TYPE_DOUBLE_LOCK);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY_1), diginput, ch_group, TYPE_DOUBLE_LOCK);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_3), diginput_1, ch_group, TYPE_DOUBLE_LOCK);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_2), diginput_0, ch_group, TYPE_DOUBLE_LOCK);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_3), digstate_1, ch_group, TYPE_DOUBLE_LOCK);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_2), digstate_0, ch_group, TYPE_DOUBLE_LOCK);
        }
        
        const bool exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch_group, TYPE_LOCK);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch_group, TYPE_LOCK);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate_1, ch_group, TYPE_LOCK);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate_0, ch_group, TYPE_LOCK);
            
            ch1->value.int_value = !((uint8_t) set_initial_state(ch_group->accessory, 0, json_context, ch1, CH_TYPE_INT8, 1));
            if (exec_actions_on_boot) {
                hkc_lock_setter(ch1, HOMEKIT_UINT8(!ch1->value.int_value));
            } else {
                hkc_lock_status_setter(ch1, HOMEKIT_UINT8(!ch1->value.int_value));
            }
            
            if (acc_type == ACC_TYPE_DOUBLE_LOCK) {
                diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3), diginput_1, ch_group, TYPE_DOUBLE_LOCK);
                diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2), diginput_0, ch_group, TYPE_DOUBLE_LOCK);
                diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_3), digstate_1, ch_group, TYPE_DOUBLE_LOCK);
                diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_2), digstate_0, ch_group, TYPE_DOUBLE_LOCK);
                
                ch3->value.int_value = !((uint8_t) set_initial_state(ch_group->accessory, 1, json_context, ch3, CH_TYPE_INT8, 1));
                if (exec_actions_on_boot) {
                    hkc_lock_setter(ch3, HOMEKIT_UINT8(!ch3->value.int_value));
                } else {
                    hkc_lock_status_setter(ch3, HOMEKIT_UINT8(!ch3->value.int_value));
                }
            }
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch_group, TYPE_LOCK)) {
                if (exec_actions_on_boot) {
                    diginput_1(0, ch_group, TYPE_LOCK);
                } else {
                    digstate_1(0, ch_group, TYPE_LOCK);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch_group, TYPE_LOCK)) {
                ch1->value = HOMEKIT_UINT8(0);
                if (exec_actions_on_boot) {
                    diginput_0(0, ch_group, TYPE_LOCK);
                } else {
                    digstate_0(0, ch_group, TYPE_LOCK);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate_1, ch_group, TYPE_LOCK)) {
                digstate_1(0, ch_group, TYPE_LOCK);
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate_0, ch_group, TYPE_LOCK)) {
                digstate_0(0, ch_group, TYPE_LOCK);
            }
            
            if (acc_type == ACC_TYPE_DOUBLE_LOCK) {
                if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3), diginput_1, ch_group, TYPE_DOUBLE_LOCK)) {
                    if (exec_actions_on_boot) {
                        diginput_1(0, ch_group, TYPE_DOUBLE_LOCK);
                    } else {
                        digstate_1(0, ch_group, TYPE_DOUBLE_LOCK);
                    }
                }
                
                if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2), diginput_0, ch_group, TYPE_DOUBLE_LOCK)) {
                    ch3->value = HOMEKIT_UINT8(0);
                    if (exec_actions_on_boot) {
                        diginput_0(0, ch_group, TYPE_DOUBLE_LOCK);
                    } else {
                        digstate_0(0, ch_group, TYPE_DOUBLE_LOCK);
                    }
                }
                
                if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_3), digstate_1, ch_group, TYPE_DOUBLE_LOCK)) {
                    digstate_1(0, ch_group, TYPE_DOUBLE_LOCK);
                }
                
                if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_2), digstate_0, ch_group, TYPE_DOUBLE_LOCK)) {
                    digstate_0(0, ch_group, TYPE_DOUBLE_LOCK);
                }
            }
        }
        
        if (ch_group->homekit_enabled) {
            return accessory + 1;
        }
        return accessory;
    }
    
    uint8_t new_sensor(const uint8_t accessory, cJSON* json_context, uint8_t acc_type) {
        ch_group_t* ch_group = new_ch_group();
        ch_group->acc_type = ACC_TYPE_CONTACT_SENSOR;
        ch_group->accessory = accessory_numerator;
        ch_group->homekit_enabled = acc_homekit_enabled(json_context);
        
        new_accessory(accessory, 3, ch_group->homekit_enabled, json_context);
        
        homekit_characteristic_t* ch0;
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[1]->id = 8;
            accessories[accessory]->services[1]->primary = true;
        }
        
        bool is_power_meter = false;
        
        if (acc_type > 20) {
            acc_type -= 70;
            is_power_meter = true;
        }
        
        switch (acc_type) {
            case ACC_TYPE_OCCUPANCY_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_OCCUPANCY_SENSOR;
                }
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(OCCUPANCY_DETECTED, 0);
                break;
                
            case ACC_TYPE_LEAK_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_LEAK_SENSOR;
                }
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(LEAK_DETECTED, 0);
                break;
                
            case ACC_TYPE_SMOKE_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_SMOKE_SENSOR;
                }
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(SMOKE_DETECTED, 0);
                break;
                
            case ACC_TYPE_CARBON_MONOXIDE_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_CARBON_MONOXIDE_SENSOR;
                }
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(CARBON_MONOXIDE_DETECTED, 0);
                break;
                
            case ACC_TYPE_CARBON_DIOXIDE_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_CARBON_DIOXIDE_SENSOR;
                }
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(CARBON_DIOXIDE_DETECTED, 0);
                break;
                
            case ACC_TYPE_FILTER_CHANGE_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_FILTER_MAINTENANCE;
                }
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(FILTER_CHANGE_INDICATION, 0);
                break;
                
            case ACC_TYPE_MOTION_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_MOTION_SENSOR;
                }
                ch_group->acc_type = ACC_TYPE_MOTION_SENSOR;
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(MOTION_DETECTED, 0);
                break;
                
            case ACC_TYPE_DOORBELL:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_DOORBELL;
                }
                ch_group->acc_type = ACC_TYPE_DOORBELL;
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(PROGRAMMABLE_SWITCH_EVENT, 0);
                break;
                
            default:    // case ACC_TYPE_CONTACT_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_CONTACT_SENSOR;
                }
                ch0 = NEW_HOMEKIT_CHARACTERISTIC(CONTACT_SENSOR_STATE, 0);
                break;
        }
        
        ch_group->ch0 = ch0;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        ch_group->num[0] = autoswitch_time(json_context);
        
        uint8_t calloc_count = 2;
        
        if (is_power_meter) {
            calloc_count += 3;
        }
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1]->characteristics = calloc(calloc_count, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[1]->characteristics[0] = ch0;
        }

        if (is_power_meter) {
            register_wildcard_actions(ch_group, json_context);
            ch_group->last_wildcard_action[0] = NO_LAST_WILDCARD_ACTION;
            ch_group->last_wildcard_action[1] = NO_LAST_WILDCARD_ACTION;
            ch_group->last_wildcard_action[2] = NO_LAST_WILDCARD_ACTION;
            
            homekit_characteristic_t* ch1 = NEW_HOMEKIT_CHARACTERISTIC(MIAU_VOLT, 0);
            homekit_characteristic_t* ch2 = NEW_HOMEKIT_CHARACTERISTIC(MIAU_AMPERE, 0);
            homekit_characteristic_t* ch3 = NEW_HOMEKIT_CHARACTERISTIC(MIAU_WATT, 0);
            
            if (ch_group->homekit_enabled) {
                accessories[accessory]->services[1]->characteristics[1] = ch1;
                accessories[accessory]->services[1]->characteristics[2] = ch2;
                accessories[accessory]->services[1]->characteristics[3] = ch3;
            }
         
            ch_group->ch1 = ch1;
            ch_group->ch2 = ch2;
            ch_group->ch3 = ch3;

            PM_VOLTAGE_FACTOR = PM_VOLTAGE_FACTOR_DEFAULT;
            if (cJSON_GetObjectItemCaseSensitive(json_context, PM_VOLTAGE_FACTOR_SET) != NULL) {
                PM_VOLTAGE_FACTOR = (float) cJSON_GetObjectItemCaseSensitive(json_context, PM_VOLTAGE_FACTOR_SET)->valuedouble;
            }
            
            PM_VOLTAGE_OFFSET = PM_VOLTAGE_OFFSET_DEFAULT;
            if (cJSON_GetObjectItemCaseSensitive(json_context, PM_VOLTAGE_OFFSET_SET) != NULL) {
                PM_VOLTAGE_OFFSET = (float) cJSON_GetObjectItemCaseSensitive(json_context, PM_VOLTAGE_OFFSET_SET)->valuedouble;
            }
            
            PM_CURRENT_FACTOR = PM_CURRENT_FACTOR_DEFAULT;
            if (cJSON_GetObjectItemCaseSensitive(json_context, PM_CURRENT_FACTOR_SET) != NULL) {
                PM_CURRENT_FACTOR = (float) cJSON_GetObjectItemCaseSensitive(json_context, PM_CURRENT_FACTOR_SET)->valuedouble;
            }
            
            PM_CURRENT_OFFSET = PM_CURRENT_OFFSET_DEFAULT;
            if (cJSON_GetObjectItemCaseSensitive(json_context, PM_CURRENT_OFFSET_SET) != NULL) {
                PM_CURRENT_OFFSET = (float) cJSON_GetObjectItemCaseSensitive(json_context, PM_CURRENT_OFFSET_SET)->valuedouble;
            }
            
            PM_POWER_FACTOR = PM_POWER_FACTOR_DEFAULT;
            if (cJSON_GetObjectItemCaseSensitive(json_context, PM_POWER_FACTOR_SET) != NULL) {
                PM_POWER_FACTOR = (float) cJSON_GetObjectItemCaseSensitive(json_context, PM_POWER_FACTOR_SET)->valuedouble;
            }
            
            PM_POWER_OFFSET = PM_POWER_OFFSET_DEFAULT;
            if (cJSON_GetObjectItemCaseSensitive(json_context, PM_POWER_OFFSET_SET) != NULL) {
                PM_POWER_OFFSET = (float) cJSON_GetObjectItemCaseSensitive(json_context, PM_POWER_OFFSET_SET)->valuedouble;
            }
            
            PM_SENSOR_TYPE = PM_SENSOR_TYPE_DEFAULT;
            if (cJSON_GetObjectItemCaseSensitive(json_context, PM_SENSOR_TYPE_SET) != NULL) {
                PM_SENSOR_TYPE = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, PM_SENSOR_TYPE_SET)->valuedouble;
            }
            
            if (PM_SENSOR_TYPE == 2) {
                // TODO
                
            } else {
                int8_t gpio_cf = PM_SENSOR_HLW_GPIO_DEFAULT;
                if (cJSON_GetObjectItemCaseSensitive(json_context, PM_SENSOR_HLW_GPIO_CF_SET) != NULL) {
                    gpio_cf = (int8_t) cJSON_GetObjectItemCaseSensitive(json_context, PM_SENSOR_HLW_GPIO_CF_SET)->valuedouble;
                }
                
                PM_SENSOR_HLW_GPIO_CF = gpio_cf;
                
                int8_t gpio_cf1 = PM_SENSOR_HLW_GPIO_DEFAULT;
                if (cJSON_GetObjectItemCaseSensitive(json_context, PM_SENSOR_HLW_GPIO_CF1_SET) != NULL) {
                    gpio_cf1 = (int8_t) cJSON_GetObjectItemCaseSensitive(json_context, PM_SENSOR_HLW_GPIO_CF1_SET)->valuedouble;
                }
                
                int8_t gpio_sel = PM_SENSOR_HLW_GPIO_DEFAULT;
                if (cJSON_GetObjectItemCaseSensitive(json_context, PM_SENSOR_HLW_GPIO_SEL_SET) != NULL) {
                    gpio_sel = (int8_t) cJSON_GetObjectItemCaseSensitive(json_context, PM_SENSOR_HLW_GPIO_SEL_SET)->valuedouble;
                }
                
                if (gpio_cf != PM_SENSOR_HLW_GPIO_DEFAULT) {
                    PM_SENSOR_HLW_GPIO = gpio_cf;
                } else {
                    PM_SENSOR_HLW_GPIO = gpio_cf1;
                }
                
                adv_hlw_unit_create(gpio_cf, gpio_cf1, gpio_sel, (uint8_t) PM_SENSOR_TYPE);
            }

            const float poll_period = sensor_poll_period(json_context, PM_POLL_PERIOD_DEFAULT);
            ch_group->timer = xTimerCreate(0, pdMS_TO_TICKS(poll_period * 1000), pdTRUE, (void*) ch_group, power_monitor_timer_worker);
            xTimerStart(ch_group->timer, XTIMER_BLOCK_TIME);
        }

        const bool exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (acc_type == ACC_TYPE_MOTION_SENSOR) {
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), sensor_0, ch_group, TYPE_SENSOR_BOOL);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), sensor_1, ch_group, TYPE_SENSOR_BOOL);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_1), sensor_status_1, ch_group, TYPE_SENSOR_BOOL);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_0), sensor_status_0, ch_group, TYPE_SENSOR_BOOL);
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), sensor_0, ch_group, TYPE_SENSOR_BOOL)) {
                if (exec_actions_on_boot) {
                    sensor_0(0, ch_group, TYPE_SENSOR_BOOL);
                } else {
                    sensor_status_0(0, ch_group, TYPE_SENSOR_BOOL);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), sensor_1, ch_group, TYPE_SENSOR_BOOL)) {
                if (exec_actions_on_boot) {
                    sensor_1(0, ch_group, TYPE_SENSOR_BOOL);
                } else {
                    sensor_status_1(0, ch_group, TYPE_SENSOR_BOOL);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), sensor_status_1, ch_group, TYPE_SENSOR_BOOL)) {
                sensor_status_0(0, ch_group, TYPE_SENSOR_BOOL);
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), sensor_status_0, ch_group, TYPE_SENSOR_BOOL)) {
                sensor_status_1(0, ch_group, TYPE_SENSOR_BOOL);
            }
        } else {
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), sensor_0, ch_group, TYPE_SENSOR);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), sensor_1, ch_group, TYPE_SENSOR);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_1), sensor_status_1, ch_group, TYPE_SENSOR);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_0), sensor_status_0, ch_group, TYPE_SENSOR);
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), sensor_0, ch_group, TYPE_SENSOR)) {
                if (exec_actions_on_boot) {
                    sensor_0(0, ch_group, TYPE_SENSOR);
                } else {
                    sensor_status_0(0, ch_group, TYPE_SENSOR);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), sensor_1, ch_group, TYPE_SENSOR)) {
                if (exec_actions_on_boot) {
                    sensor_1(0, ch_group, TYPE_SENSOR);
                } else {
                    sensor_status_1(0, ch_group, TYPE_SENSOR);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), sensor_status_1, ch_group, TYPE_SENSOR)) {
                sensor_status_0(0, ch_group, TYPE_SENSOR);
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), sensor_status_0, ch_group, TYPE_SENSOR)) {
                sensor_status_1(0, ch_group, TYPE_SENSOR);
            }
        }
        
        if (ch_group->homekit_enabled) {
            return accessory + 1;
        }
        return accessory;
    }
    
    uint8_t new_water_valve(uint8_t accessory, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group();
        ch_group->accessory = accessory_numerator;
        ch_group->homekit_enabled = acc_homekit_enabled(json_context);
        
        new_accessory(accessory, 3, ch_group->homekit_enabled, json_context);
        
        uint8_t valve_type = VALVE_SYSTEM_TYPE_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, VALVE_SYSTEM_TYPE) != NULL) {
            valve_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, VALVE_SYSTEM_TYPE)->valuedouble;
        }
        
        uint32_t valve_max_duration = VALVE_MAX_DURATION_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, VALVE_MAX_DURATION) != NULL) {
            valve_max_duration = (uint32_t) cJSON_GetObjectItemCaseSensitive(json_context, VALVE_MAX_DURATION)->valuedouble;
        }
        
        homekit_characteristic_t* ch0 = NEW_HOMEKIT_CHARACTERISTIC(ACTIVE, 0, .setter_ex=hkc_valve_setter);
        homekit_characteristic_t* ch1 = NEW_HOMEKIT_CHARACTERISTIC(IN_USE, 0);
        homekit_characteristic_t* ch2, *ch3;

        ch_group->acc_type = ACC_TYPE_WATER_VALVE;
        ch_group->ch0 = ch0;
        ch_group->ch1 = ch1;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        ch_group->num[0] = autoswitch_time(json_context);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[1]->id = 8;
            accessories[accessory]->services[1]->primary = true;
            accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_VALVE;
        }
        
        if (valve_max_duration == 0) {
            accessories[accessory]->services[1]->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
        } else {
            ch2 = NEW_HOMEKIT_CHARACTERISTIC(SET_DURATION, valve_max_duration, .max_value=(float[]) {valve_max_duration}, .setter_ex=hkc_setter);
            ch3 = NEW_HOMEKIT_CHARACTERISTIC(REMAINING_DURATION, 0, .max_value=(float[]) {valve_max_duration});
            
            ch_group->ch2 = ch2;
            ch_group->ch3 = ch3;
            
            if (ch_group->homekit_enabled) {
                accessories[accessory]->services[1]->characteristics = calloc(6, sizeof(homekit_characteristic_t*));
                accessories[accessory]->services[1]->characteristics[3] = ch2;
                accessories[accessory]->services[1]->characteristics[4] = ch3;
            }
            
            const uint32_t initial_time = (uint32_t) set_initial_state(ch_group->accessory, 2, cJSON_Parse(INIT_STATE_LAST_STR), ch2, CH_TYPE_INT32, 900);
            if (initial_time > valve_max_duration) {
                ch2->value.int_value = valve_max_duration;
            } else {
                ch2->value.int_value = initial_time;
            }
            
            ch_group->timer = xTimerCreate(0, pdMS_TO_TICKS(1000), pdTRUE, (void*) ch0, valve_timer_worker);
        }
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1]->characteristics[0] = ch0;
            accessories[accessory]->services[1]->characteristics[1] = ch1;
            accessories[accessory]->services[1]->characteristics[2] = NEW_HOMEKIT_CHARACTERISTIC(VALVE_TYPE, valve_type);
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, TYPE_VALVE);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, TYPE_VALVE);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput_1, ch_group, TYPE_VALVE);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput_0, ch_group, TYPE_VALVE);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_1), digstate_1, ch_group, TYPE_VALVE);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_0), digstate_0, ch_group, TYPE_VALVE);
        
        const bool exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch_group, TYPE_VALVE);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch_group, TYPE_VALVE);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate_1, ch_group, TYPE_VALVE);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate_0, ch_group, TYPE_VALVE);
            
            ch0->value.int_value = !((uint8_t) set_initial_state(ch_group->accessory, 0, json_context, ch0, CH_TYPE_INT8, 0));
            if (exec_actions_on_boot) {
                hkc_valve_setter(ch0, HOMEKIT_UINT8(!ch0->value.int_value));
            } else {
                hkc_valve_status_setter(ch0, HOMEKIT_UINT8(!ch0->value.int_value));
            }
            
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch_group, TYPE_VALVE)) {
                if (exec_actions_on_boot) {
                    diginput_1(0, ch_group, TYPE_VALVE);
                } else {
                    digstate_1(0, ch_group, TYPE_VALVE);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch_group, TYPE_VALVE)) {
                ch0->value = HOMEKIT_UINT8(1);
                if (exec_actions_on_boot) {
                    diginput_0(0, ch_group, TYPE_VALVE);
                } else {
                    digstate_0(0, ch_group, TYPE_VALVE);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate_1, ch_group, TYPE_VALVE)) {
                digstate_1(0, ch_group, TYPE_VALVE);
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate_0, ch_group, TYPE_VALVE)) {
                ch0->value = HOMEKIT_UINT8(1);
                digstate_0(0, ch_group, TYPE_VALVE);
            }
        }
        
        if (ch_group->homekit_enabled) {
            return accessory + 1;
        }
        return accessory;
    }
    
    uint8_t new_thermostat(uint8_t accessory, cJSON* json_context, const uint8_t acc_type) {
        ch_group_t* ch_group = new_ch_group();
        ch_group->accessory = accessory_numerator;
        ch_group->homekit_enabled = acc_homekit_enabled(json_context);
        
        uint8_t th_accessories = 3;
        if (acc_type == ACC_TYPE_THERMOSTAT_WITH_HUM) {
            th_accessories++;
        }
        
        new_accessory(accessory, th_accessories, ch_group->homekit_enabled, json_context);

        ch_group->acc_type = ACC_TYPE_THERMOSTAT;
        
        // Custom ranges of Target Temperatures
        TH_MIN_TEMP = THERMOSTAT_DEFAULT_MIN_TEMP;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MIN_TEMP) != NULL) {
            TH_MIN_TEMP = (float) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MIN_TEMP)->valuedouble;
        }
        
        if (TH_MIN_TEMP < -100) {
            TH_MIN_TEMP = -100;
        }
        
        TH_MAX_TEMP = THERMOSTAT_DEFAULT_MAX_TEMP;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MAX_TEMP) != NULL) {
            TH_MAX_TEMP = (float) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MAX_TEMP)->valuedouble;
        }
        
        if (TH_MAX_TEMP > 200) {
            TH_MAX_TEMP = 200;
        }
        
        const float default_target_temp = (TH_MIN_TEMP + TH_MAX_TEMP) / 2;
        
        // Temperature Deadbands
        TH_DEADBAND = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND) != NULL) {
            TH_DEADBAND = (float) (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND)->valuedouble / 2.000f);
        }
        
        TH_DEADBAND_FORCE_IDLE = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND_FORCE_IDLE) != NULL) {
            TH_DEADBAND_FORCE_IDLE = (float) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND_FORCE_IDLE)->valuedouble;
        }
        
        TH_DEADBAND_SOFT_ON = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND_SOFT_ON) != NULL) {
            TH_DEADBAND_SOFT_ON = (float) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND_SOFT_ON)->valuedouble;
        }
        
        // Thermostat Type
        TH_TYPE = THERMOSTAT_TYPE_HEATER;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_TYPE) != NULL) {
            TH_TYPE = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_TYPE)->valuedouble;
        }
        
        // HomeKit Characteristics
        homekit_characteristic_t* ch0 = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_TEMPERATURE, 0, .min_value=(float[]) {-100}, .max_value=(float[]) {200});
        homekit_characteristic_t* ch2 = NEW_HOMEKIT_CHARACTERISTIC(ACTIVE, 0, .setter_ex=hkc_th_target_setter);
        homekit_characteristic_t* ch3 = NEW_HOMEKIT_CHARACTERISTIC(TEMPERATURE_DISPLAY_UNITS, 0, .setter_ex=hkc_setter);
        homekit_characteristic_t* ch4 = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_HEATER_COOLER_STATE, 0);
        homekit_characteristic_t* ch6 = NEW_HOMEKIT_CHARACTERISTIC(HEATING_THRESHOLD_TEMPERATURE, default_target_temp -1, .min_value=(float[]) {TH_MIN_TEMP}, .max_value=(float[]) {TH_MAX_TEMP}, .setter_ex=update_th);
        homekit_characteristic_t* ch7 = NEW_HOMEKIT_CHARACTERISTIC(COOLING_THRESHOLD_TEMPERATURE, default_target_temp +1, .min_value=(float[]) {TH_MIN_TEMP}, .max_value=(float[]) {TH_MAX_TEMP}, .setter_ex=update_th);
        
        if (ch_group->homekit_enabled) {
            uint8_t ch_calloc = 7;
            if (TH_TYPE == THERMOSTAT_TYPE_HEATERCOOLER) {
                ch_calloc += 1;
            }
        
            accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[1]->id = 8;
            accessories[accessory]->services[1]->primary = true;
            accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_HEATER_COOLER;
            accessories[accessory]->services[1]->characteristics = calloc(ch_calloc, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[1]->characteristics[0] = ch2;
            accessories[accessory]->services[1]->characteristics[1] = ch0;
            accessories[accessory]->services[1]->characteristics[2] = ch3;
            accessories[accessory]->services[1]->characteristics[3] = ch4;
        }
        
        homekit_characteristic_t* ch5;
        
        const float initial_h_target_temp = set_initial_state(ch_group->accessory, 6, cJSON_Parse(INIT_STATE_LAST_STR), ch6, CH_TYPE_FLOAT, default_target_temp -1);
        if (initial_h_target_temp > TH_MAX_TEMP || initial_h_target_temp < TH_MIN_TEMP) {
            ch6->value.float_value = default_target_temp -1;
        } else {
            ch6->value.float_value = initial_h_target_temp;
        }
        
        const float initial_c_target_temp = set_initial_state(ch_group->accessory, 7, cJSON_Parse(INIT_STATE_LAST_STR), ch7, CH_TYPE_FLOAT, default_target_temp +1);
        if (initial_c_target_temp > TH_MAX_TEMP || initial_c_target_temp < TH_MIN_TEMP) {
            ch7->value.float_value = default_target_temp +1;
        } else {
            ch7->value.float_value = initial_c_target_temp;
        }
        
        switch ((uint8_t) TH_TYPE) {
            case THERMOSTAT_TYPE_COOLER:
                ch5 = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HEATER_COOLER_STATE, THERMOSTAT_TARGET_MODE_COOLER, .min_value=(float[]) {THERMOSTAT_TARGET_MODE_COOLER}, .max_value=(float[]) {THERMOSTAT_TARGET_MODE_COOLER}, .valid_values={.count=1, .values=(uint8_t[]) {THERMOSTAT_TARGET_MODE_COOLER}});
                
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[1]->characteristics[5] = ch7;
                }
                break;
                
            case THERMOSTAT_TYPE_HEATERCOOLER:
                ch5 = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HEATER_COOLER_STATE, THERMOSTAT_TARGET_MODE_AUTO, .setter_ex=update_th);
                
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[1]->characteristics[5] = ch6;
                    accessories[accessory]->services[1]->characteristics[6] = ch7;
                }
                break;
                
            default:        // case THERMOSTAT_TYPE_HEATER:
                ch5 = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HEATER_COOLER_STATE, THERMOSTAT_TARGET_MODE_HEATER, .min_value=(float[]) {THERMOSTAT_TARGET_MODE_HEATER}, .max_value=(float[]) {THERMOSTAT_TARGET_MODE_HEATER}, .valid_values={.count=1, .values=(uint8_t[]) {THERMOSTAT_TARGET_MODE_HEATER}});

                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[1]->characteristics[5] = ch6;
                }
                break;
        }
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1]->characteristics[4] = ch5;
        }
        
        if (acc_type == ACC_TYPE_THERMOSTAT_WITH_HUM) {
            homekit_characteristic_t* ch1 = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_RELATIVE_HUMIDITY, 0);
            ch_group->ch1 = ch1;
            
            if (ch_group->homekit_enabled) {
                accessories[accessory]->services[2] = calloc(1, sizeof(homekit_service_t));
                accessories[accessory]->services[2]->id = 18;
                accessories[accessory]->services[2]->primary = false;
                accessories[accessory]->services[2]->type = HOMEKIT_SERVICE_HUMIDITY_SENSOR;
                accessories[accessory]->services[2]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
                accessories[accessory]->services[2]->characteristics[0] = ch1;
            }
        }
        
        ch_group->ch0 = ch0;
        ch_group->ch2 = ch2;
        ch_group->ch3 = ch3;
        ch_group->ch4 = ch4;
        ch_group->ch5 = ch5;
        ch_group->ch6 = ch6;
        ch_group->ch7 = ch7;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        th_sensor(ch_group, json_context);
        ch_group->last_wildcard_action[0] = NO_LAST_WILDCARD_ACTION;
        ch_group->last_wildcard_action[1] = NO_LAST_WILDCARD_ACTION;
        ch_group->last_wildcard_action[2] = NO_LAST_WILDCARD_ACTION;
        
        ch_group->timer2 = xTimerCreate(0, pdMS_TO_TICKS(TH_UPDATE_DELAY_MS), pdFALSE, (void*) ch_group, process_th_timer);
        
        if (TH_SENSOR_GPIO != -1 || TH_SENSOR_TYPE > 4) {
            th_sensor_starter(ch_group);
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), th_input, ch_group, 9);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3), th_input_temp, ch_group, THERMOSTAT_TEMP_UP);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_4), th_input_temp, ch_group, THERMOSTAT_TEMP_DOWN);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), th_input, ch_group, 9);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_3), th_input_temp, ch_group, THERMOSTAT_TEMP_UP);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_4), th_input_temp, ch_group, THERMOSTAT_TEMP_DOWN);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), th_input, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), th_input, ch_group, 0);
        
        if (TH_TYPE == THERMOSTAT_TYPE_HEATERCOOLER) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_5), th_input, ch_group, 5);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_6), th_input, ch_group, 6);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_7), th_input, ch_group, 7);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_5), th_input, ch_group, 5);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_6), th_input, ch_group, 6);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_7), th_input, ch_group, 7);
            
            ch5->value.int_value = set_initial_state(ch_group->accessory, 5, cJSON_Parse(INIT_STATE_LAST_STR), ch5, CH_TYPE_INT8, 0);
        }
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), th_input, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), th_input, ch_group, 0);
            
            ch2->value.int_value = !((uint8_t) set_initial_state(ch_group->accessory, 2, json_context, ch2, CH_TYPE_INT8, 0));
            update_th(ch2, HOMEKIT_UINT8(!ch2->value.int_value));
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), th_input, ch_group, 1)) {
                th_input(0, ch_group, 1);
            }
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), th_input, ch_group, 0)) {
                ch2->value = HOMEKIT_UINT8(1);
                th_input(0, ch_group, 0);
            }
        }
        
        if (ch_group->homekit_enabled) {
            return accessory + 1;
        }
        return accessory;
    }
    
    uint8_t new_temp_sensor(uint8_t accessory, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group();
        ch_group->accessory = accessory_numerator;
        ch_group->homekit_enabled = acc_homekit_enabled(json_context);
        
        new_accessory(accessory, 3, ch_group->homekit_enabled, json_context);
        
        homekit_characteristic_t* ch0 = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_TEMPERATURE, 0, .min_value=(float[]) {-100}, .max_value=(float[]) {200});
  
        ch_group->acc_type = ACC_TYPE_TEMP_SENSOR;
        ch_group->ch0 = ch0;
        th_sensor(ch_group, json_context);
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        ch_group->last_wildcard_action[0] = NO_LAST_WILDCARD_ACTION;
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[1]->id = 8;
            accessories[accessory]->services[1]->primary = true;
            accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_TEMPERATURE_SENSOR;
            accessories[accessory]->services[1]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[1]->characteristics[0] = ch0;
        }
            
        if (TH_SENSOR_GPIO != -1 || TH_SENSOR_TYPE > 4) {
            th_sensor_starter(ch_group);
        }
        
        if (ch_group->homekit_enabled) {
            return accessory + 1;
        }
        return accessory;
    }
    
    uint8_t new_hum_sensor(uint8_t accessory, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group();
        ch_group->accessory = accessory_numerator;
        ch_group->homekit_enabled = acc_homekit_enabled(json_context);
        
        new_accessory(accessory, 3, ch_group->homekit_enabled, json_context);
        
        homekit_characteristic_t* ch1 = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_RELATIVE_HUMIDITY, 0);
        
        ch_group->acc_type = ACC_TYPE_HUM_SENSOR;
        ch_group->ch1 = ch1;
        th_sensor(ch_group, json_context);
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        ch_group->last_wildcard_action[1] = NO_LAST_WILDCARD_ACTION;
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[1]->id = 8;
            accessories[accessory]->services[1]->primary = true;
            accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_HUMIDITY_SENSOR;
            accessories[accessory]->services[1]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[1]->characteristics[0] = ch1;
        }
        
        if (TH_SENSOR_GPIO != -1) {
            th_sensor_starter(ch_group);
        }
        
        if (ch_group->homekit_enabled) {
            return accessory + 1;
        }
        return accessory;
    }
    
    uint8_t new_th_sensor(uint8_t accessory, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group();
        ch_group->accessory = accessory_numerator;
        ch_group->homekit_enabled = acc_homekit_enabled(json_context);
        
        new_accessory(accessory, 4, ch_group->homekit_enabled, json_context);
        
        homekit_characteristic_t* ch0 = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_TEMPERATURE, 0, .min_value=(float[]) {-100}, .max_value=(float[]) {200});
        homekit_characteristic_t* ch1 = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_RELATIVE_HUMIDITY, 0);
        
        ch_group->acc_type = ACC_TYPE_TH_SENSOR;
        ch_group->ch0 = ch0;
        ch_group->ch1 = ch1;
        th_sensor(ch_group, json_context);
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        ch_group->last_wildcard_action[0] = NO_LAST_WILDCARD_ACTION;
        ch_group->last_wildcard_action[1] = NO_LAST_WILDCARD_ACTION;
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[1]->id = 8;
            accessories[accessory]->services[1]->primary = true;
            accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_TEMPERATURE_SENSOR;
            accessories[accessory]->services[1]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[1]->characteristics[0] = ch0;
            
            accessories[accessory]->services[2] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[2]->id = 11;
            accessories[accessory]->services[2]->primary = false;
            accessories[accessory]->services[2]->type = HOMEKIT_SERVICE_HUMIDITY_SENSOR;
            accessories[accessory]->services[2]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[2]->characteristics[0] = ch1;
        }
        
        if (TH_SENSOR_GPIO != -1) {
            th_sensor_starter(ch_group);
        }
        
        if (ch_group->homekit_enabled) {
            return accessory + 1;
        }
        return accessory;
    }
    
    uint8_t new_lightbulb(uint8_t accessory, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group();
        ch_group->accessory = accessory_numerator;
        ch_group->homekit_enabled = acc_homekit_enabled(json_context);
        
        new_accessory(accessory, 3, ch_group->homekit_enabled, json_context);
        
        bool is_pwm = true;
        uint8_t driver = 0; 
        uint8_t di_pin = 0; 
        uint8_t dcki_pin = 0; 

        if(cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_DRIVER) != NULL)
        {
            if ((cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_DRIVER_GPIO_DCKI) != NULL) && 
                (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_DRIVER_GPIO_DCKI) != NULL))
            {
                di_pin = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_DRIVER_GPIO_DI)->valuedouble;
                dcki_pin = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_DRIVER_GPIO_DCKI)->valuedouble;
                driver = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_DRIVER)->valuedouble;
            }
        }
 
        if ((cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_R) == NULL) &&
            (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_W) == NULL)) {
            is_pwm = false;
        }

        if (is_pwm && !main_config.lightbulb_groups) {
            INFO("PWM Init");
            main_config.pwm_timer = xTimerCreate(0, pdMS_TO_TICKS(RGBW_PERIOD), pdTRUE, NULL, rgbw_set_timer_worker);
            
            if(driver == 0)
            {
                main_config.pwm_info = malloc(sizeof(pwm_info_t));
                memset(main_config.pwm_info, 0, sizeof(*main_config.pwm_info));
                
                multipwm_init(main_config.pwm_info);
                if (main_config.pwm_freq > 0) {
                    multipwm_set_freq(main_config.pwm_info, main_config.pwm_freq);
                }
                main_config.pwm_info->channels = 0;
            }
            else
            {
                switch(driver)
                {
                    case LIGHTBULB_DRIVER_MY9291:
                    {
                        my92xx_cmd_t my92xx_command = MY92XX_COMMAND_DEFAULT;
                        my92xx_init(&(main_config.driver_interface), MY92XX_MODEL_MY9291, 1, di_pin, dcki_pin, &my92xx_command);
                        break;
                    }
                    case LIGHTBULB_DRIVER_MY9231:
                    {
                        my92xx_cmd_t my92xx_command = MY92XX_COMMAND_DEFAULT;
                        my92xx_init(&(main_config.driver_interface), MY92XX_MODEL_MY9231, 1, di_pin, dcki_pin, &my92xx_command);
                        break;
                    }
                    default:
                    {
                        my92xx_cmd_t my92xx_command = MY92XX_COMMAND_DEFAULT;
                        my92xx_init(&(main_config.driver_interface), MY92XX_MODEL_MY9291, 1, di_pin, dcki_pin, &my92xx_command);
                        break;
                    }
                }
            }
        }
        
        homekit_characteristic_t* ch0 = NEW_HOMEKIT_CHARACTERISTIC(ON, false, .setter_ex=hkc_rgbw_setter);
        homekit_characteristic_t* ch1 = NEW_HOMEKIT_CHARACTERISTIC(BRIGHTNESS, 100, .setter_ex=hkc_rgbw_setter);
 
        ch_group->acc_type = ACC_TYPE_LIGHTBULB;
        ch_group->ch0 = ch0;
        ch_group->ch1 = ch1;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        ch_group->last_wildcard_action[0] = NO_LAST_WILDCARD_ACTION;
        
        lightbulb_group_t* lightbulb_group = malloc(sizeof(lightbulb_group_t));
        memset(lightbulb_group, 0, sizeof(*lightbulb_group));
        lightbulb_group->ch0 = ch0;
        lightbulb_group->is_pwm = is_pwm;
        lightbulb_group->pwm_r = 255;
        lightbulb_group->pwm_g = 255;
        lightbulb_group->pwm_b = 255;
        lightbulb_group->pwm_w = 255;
        lightbulb_group->pwm_cw = 255;
        lightbulb_group->pwm_ww = 255;
        lightbulb_group->target_r = 255;
        lightbulb_group->target_g = 255;
        lightbulb_group->target_b = 255;
        lightbulb_group->target_w = 255;
        lightbulb_group->target_cw = 0;
        lightbulb_group->target_ww = 0;
        lightbulb_group->factor_r = 1;
        lightbulb_group->factor_g = 1;
        lightbulb_group->factor_b = 1;
        lightbulb_group->factor_w = 1;
        lightbulb_group->factor_cw = 1;
        lightbulb_group->factor_ww = 1;
        lightbulb_group->step = RGBW_STEP_DEFAULT;
        lightbulb_group->autodimmer = 0;
        lightbulb_group->armed_autodimmer = false;
        lightbulb_group->autodimmer_task_delay = AUTODIMMER_TASK_DELAY_DEFAULT;
        lightbulb_group->autodimmer_task_step = AUTODIMMER_TASK_STEP_DEFAULT;
        lightbulb_group->next = main_config.lightbulb_groups;
        main_config.lightbulb_groups = lightbulb_group;

        if (is_pwm) {
            if(driver == 0)
            {
                if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_R) != NULL && main_config.pwm_info->channels < MULTIPWM_MAX_CHANNELS) {
                    lightbulb_group->pwm_r = main_config.pwm_info->channels;
                    main_config.pwm_info->channels++;
                    multipwm_set_pin(main_config.pwm_info, lightbulb_group->pwm_r, (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_R)->valuedouble);
                }

                if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_G) != NULL && main_config.pwm_info->channels < MULTIPWM_MAX_CHANNELS) {
                    lightbulb_group->pwm_g = main_config.pwm_info->channels;
                    main_config.pwm_info->channels++;
                    multipwm_set_pin(main_config.pwm_info, lightbulb_group->pwm_g, (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_G)->valuedouble);
                }

                if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_B) != NULL && main_config.pwm_info->channels < MULTIPWM_MAX_CHANNELS) {
                    lightbulb_group->pwm_b = main_config.pwm_info->channels;
                    main_config.pwm_info->channels++;
                    multipwm_set_pin(main_config.pwm_info, lightbulb_group->pwm_b, (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_B)->valuedouble);
                }

                if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_W) != NULL && main_config.pwm_info->channels < MULTIPWM_MAX_CHANNELS) {
                    lightbulb_group->pwm_w = main_config.pwm_info->channels;
                    main_config.pwm_info->channels++;
                    multipwm_set_pin(main_config.pwm_info, lightbulb_group->pwm_w, (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_W)->valuedouble);
                }

                if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_CW) != NULL && main_config.pwm_info->channels < MULTIPWM_MAX_CHANNELS) {
                    lightbulb_group->pwm_cw = main_config.pwm_info->channels;
                    main_config.pwm_info->channels++;
                    multipwm_set_pin(main_config.pwm_info, lightbulb_group->pwm_cw, (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_CW)->valuedouble);
                }

                if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_WW) != NULL && main_config.pwm_info->channels < MULTIPWM_MAX_CHANNELS) {
                    lightbulb_group->pwm_ww = main_config.pwm_info->channels;
                    main_config.pwm_info->channels++;
                    multipwm_set_pin(main_config.pwm_info, lightbulb_group->pwm_ww, (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_WW)->valuedouble);
                }

            }
            else
            {
                uint8_t numChannels = main_config.driver_interface->descriptor->numChannels;
                if ((numChannels > 0) && (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_R) != NULL)) 
                {
                    lightbulb_group->pwm_r = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_R)->valuedouble;
                    numChannels--;
                }
                
                if ((numChannels > 0) && (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_G) != NULL)) 
                {
                    lightbulb_group->pwm_g = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_G)->valuedouble;
                    numChannels--;
                }
                
                if ((numChannels > 0) && (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_B) != NULL))
                {
                    lightbulb_group->pwm_b = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_B)->valuedouble;
                    numChannels--;
                }
                
                if ((numChannels > 0) && (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_W) != NULL))
                {
                    lightbulb_group->pwm_w = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_W)->valuedouble;
                    numChannels--;
                }
                
                if ((numChannels > 0) &&  (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_CW) != NULL))
                {
                    lightbulb_group->pwm_cw = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_CW)->valuedouble;
                    numChannels--;
                }
                
                if ((numChannels > 0) && (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_WW) != NULL))
                {
                    lightbulb_group->pwm_ww = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_PWM_GPIO_WW)->valuedouble;
                    numChannels--;
                }
            }
            
            
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_R) != NULL) {
                lightbulb_group->factor_r = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_R)->valuedouble;
            }

            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_G) != NULL) {
                lightbulb_group->factor_g = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_G)->valuedouble;
            }

            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_B) != NULL) {
                lightbulb_group->factor_b = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_B)->valuedouble;
            }

            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_W) != NULL) {
                lightbulb_group->factor_w = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_W)->valuedouble;
            }
            
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_CW) != NULL) {
                lightbulb_group->factor_cw = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_CW)->valuedouble;
            }
            
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_WW) != NULL) {
                lightbulb_group->factor_ww = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FACTOR_WW)->valuedouble;
            }
        }
        
        INFO("\nInit BULB\n pwm_r: %i\n  pwm_g: %i\n  pwm_b: %i\n pwm_w: %i\n  pwm_cw: %i\n  pwm_ww: %i\n  is_pwm: %i\n",
                        lightbulb_group->pwm_r,
                        lightbulb_group->pwm_g,
                        lightbulb_group->pwm_b,
                        lightbulb_group->pwm_w,
                        lightbulb_group->pwm_cw,
                        lightbulb_group->pwm_ww,
                        lightbulb_group->is_pwm);
        if(main_config.driver_interface != NULL)
        {
            INFO("\nInit Driver\n family: %i\n  model: %i\n  numChips: %i\n numChannels: %i\n  di_pin: %i\n  dcki_pin: %i\n  bit_width: %i\n",
            main_config.driver_interface->descriptor->family,
            main_config.driver_interface->descriptor->model,
            main_config.driver_interface->descriptor->numChips,
            main_config.driver_interface->descriptor->numChannels,
            main_config.driver_interface->descriptor->di_pin,
            main_config.driver_interface->descriptor->dcki_pin,
            main_config.driver_interface->descriptor->bit_width);
        }


        if (cJSON_GetObjectItemCaseSensitive(json_context, RGBW_STEP) != NULL) {
            lightbulb_group->step = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_context, RGBW_STEP)->valuedouble;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, AUTODIMMER_TASK_DELAY) != NULL) {
            lightbulb_group->autodimmer_task_delay = cJSON_GetObjectItemCaseSensitive(json_context, AUTODIMMER_TASK_DELAY)->valuedouble * (1000 / portTICK_PERIOD_MS);
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, AUTODIMMER_TASK_STEP) != NULL) {
            lightbulb_group->autodimmer_task_step = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, AUTODIMMER_TASK_STEP)->valuedouble;
        }

        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[1]->id = 8;
            accessories[accessory]->services[1]->primary = true;
            accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_LIGHTBULB;
        }
        
        if (lightbulb_group->pwm_r != 255) {
            homekit_characteristic_t* ch2 = NEW_HOMEKIT_CHARACTERISTIC(HUE, 0, .setter_ex=hkc_rgbw_setter);
            homekit_characteristic_t* ch3 = NEW_HOMEKIT_CHARACTERISTIC(SATURATION, 0, .setter_ex=hkc_rgbw_setter);
            
            ch_group->ch2 = ch2;
            ch_group->ch3 = ch3;
            
            if (ch_group->homekit_enabled) {
                accessories[accessory]->services[1]->characteristics = calloc(5, sizeof(homekit_characteristic_t*));
                accessories[accessory]->services[1]->characteristics[0] = ch0;
                accessories[accessory]->services[1]->characteristics[1] = ch1;
                accessories[accessory]->services[1]->characteristics[2] = ch2;
                accessories[accessory]->services[1]->characteristics[3] = ch3;
            }
            
            ch2->value.float_value = set_initial_state(ch_group->accessory, 2, cJSON_Parse(INIT_STATE_LAST_STR), ch2, CH_TYPE_FLOAT, 0);
            ch3->value.float_value = set_initial_state(ch_group->accessory, 3, cJSON_Parse(INIT_STATE_LAST_STR), ch3, CH_TYPE_FLOAT, 0);
        } else if (lightbulb_group->pwm_b != 255) {
            homekit_characteristic_t* ch2 = NEW_HOMEKIT_CHARACTERISTIC(COLOR_TEMPERATURE, 152, .setter_ex=hkc_rgbw_setter);
            
            ch_group->ch2 = ch2;
            
            if (ch_group->homekit_enabled) {
                accessories[accessory]->services[1]->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
                accessories[accessory]->services[1]->characteristics[0] = ch0;
                accessories[accessory]->services[1]->characteristics[1] = ch1;
                accessories[accessory]->services[1]->characteristics[2] = ch2;
            }
            
            ch2->value.int_value = set_initial_state(ch_group->accessory, 2, cJSON_Parse(INIT_STATE_LAST_STR), ch2, CH_TYPE_INT32, 152);
        } else {
            if (ch_group->homekit_enabled) {
                accessories[accessory]->services[1]->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                accessories[accessory]->services[1]->characteristics[0] = ch0;
                accessories[accessory]->services[1]->characteristics[1] = ch1;
            }
        }

        ch1->value.int_value = set_initial_state(ch_group->accessory, 1, cJSON_Parse(INIT_STATE_LAST_STR), ch1, CH_TYPE_INT8, 100);

        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, TYPE_LIGHTBULB);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2), rgbw_brightness, ch_group, LIGHTBULB_BRIGHTNESS_UP);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3), rgbw_brightness, ch_group, LIGHTBULB_BRIGHTNESS_DOWN);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, TYPE_LIGHTBULB);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_2), rgbw_brightness, ch_group, LIGHTBULB_BRIGHTNESS_UP);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_3), rgbw_brightness, ch_group, LIGHTBULB_BRIGHTNESS_DOWN);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput_1, ch_group, TYPE_LIGHTBULB);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput_0, ch_group, TYPE_LIGHTBULB);
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY) != NULL ||
            cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0) != NULL ||
            cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1) != NULL) {
            if (lightbulb_group->autodimmer_task_step > 0) {
                ch_group->timer = xTimerCreate(0, pdMS_TO_TICKS(AUTODIMMER_DELAY), pdFALSE, (void*) ch0, no_autodimmer_called);
            }
        } else {
            lightbulb_group->autodimmer_task_step = 0;
        }
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch_group, TYPE_LIGHTBULB);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch_group, TYPE_LIGHTBULB);
            
            ch0->value.bool_value = !((bool) set_initial_state(ch_group->accessory, 0, json_context, ch0, CH_TYPE_BOOL, 0));
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch_group, TYPE_LIGHTBULB)) {
                ch0->value = HOMEKIT_BOOL(false);
            }
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch_group, TYPE_LIGHTBULB)) {
                ch0->value = HOMEKIT_BOOL(true);
            }
        }
        
        if (ch_group->homekit_enabled) {
            return accessory + 1;
        }
        return accessory;
    }
    
    uint8_t new_garage_door(uint8_t accessory, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group();
        ch_group->accessory = accessory_numerator;
        ch_group->homekit_enabled = acc_homekit_enabled(json_context);
        
        new_accessory(accessory, 3, ch_group->homekit_enabled, json_context);
        
        homekit_characteristic_t* ch0 = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_DOOR_STATE, 1);
        homekit_characteristic_t* ch1 = NEW_HOMEKIT_CHARACTERISTIC(TARGET_DOOR_STATE, 1, .setter_ex=hkc_garage_door_setter);
        homekit_characteristic_t* ch2 = NEW_HOMEKIT_CHARACTERISTIC(OBSTRUCTION_DETECTED, false);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[1]->id = 8;
            accessories[accessory]->services[1]->primary = true;
            accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_GARAGE_DOOR_OPENER;
            accessories[accessory]->services[1]->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[1]->characteristics[0] = ch0;
            accessories[accessory]->services[1]->characteristics[1] = ch1;
            accessories[accessory]->services[1]->characteristics[2] = ch2;
        }

        ch_group->acc_type = ACC_TYPE_GARAGE_DOOR;
        ch_group->ch0 = ch0;
        ch_group->ch1 = ch1;
        ch_group->ch2 = ch2;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        GARAGE_DOOR_CURRENT_TIME = GARAGE_DOOR_TIME_MARGIN_DEFAULT;
        GARAGE_DOOR_WORKING_TIME = GARAGE_DOOR_TIME_OPEN_DEFAULT;
        GARAGE_DOOR_TIME_MARGIN = GARAGE_DOOR_TIME_MARGIN_DEFAULT;
        GARAGE_DOOR_CLOSE_TIME_FACTOR = 1;
        
        ch_group->timer = xTimerCreate(0, pdMS_TO_TICKS(1000), pdTRUE, (void*) ch0, garage_door_timer_worker);
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_MARGIN_SET) != NULL) {
            GARAGE_DOOR_TIME_MARGIN = cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_MARGIN_SET)->valuedouble;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_OPEN_SET) != NULL) {
            GARAGE_DOOR_WORKING_TIME = cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_OPEN_SET)->valuedouble;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_CLOSE_SET) != NULL) {
            GARAGE_DOOR_CLOSE_TIME_FACTOR = GARAGE_DOOR_WORKING_TIME / cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_CLOSE_SET)->valuedouble;
        }
        
        GARAGE_DOOR_WORKING_TIME += GARAGE_DOOR_TIME_MARGIN * 2;
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, TYPE_GARAGE_DOOR);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch_group, TYPE_GARAGE_DOOR);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch_group, TYPE_GARAGE_DOOR);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_8), garage_door_stop, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, TYPE_GARAGE_DOOR);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput_0, ch_group, TYPE_GARAGE_DOOR);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput_1, ch_group, TYPE_GARAGE_DOOR);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_8), garage_door_stop, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_5), garage_door_sensor, ch_group, GARAGE_DOOR_CLOSING);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_4), garage_door_sensor, ch_group, GARAGE_DOOR_OPENING);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_3), garage_door_sensor, ch_group, GARAGE_DOOR_CLOSED);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_2), garage_door_sensor, ch_group, GARAGE_DOOR_OPENED);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_6), garage_door_obstruction, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_7), garage_door_obstruction, ch_group, 1);
        
        ch0->value.int_value = (uint8_t) set_initial_state(ch_group->accessory, 0, json_context, ch0, CH_TYPE_INT8, 1);
        if (ch0->value.int_value > 1) {
            ch1->value.int_value = ch0->value.int_value - 2;
        } else {
            ch1->value.int_value = ch0->value.int_value;
        }
        
        if (ch0->value.int_value == 0) {
            GARAGE_DOOR_CURRENT_TIME = GARAGE_DOOR_WORKING_TIME - GARAGE_DOOR_TIME_MARGIN;
        }

        GARAGE_DOOR_HAS_F5 = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_5) != NULL) {
            GARAGE_DOOR_HAS_F5 = 1;
        }
        
        GARAGE_DOOR_HAS_F4 = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_4) != NULL) {
            GARAGE_DOOR_HAS_F4 = 1;
        }
        
        GARAGE_DOOR_HAS_F3 = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3) != NULL) {
            GARAGE_DOOR_HAS_F3 = 1;
        }
        
        GARAGE_DOOR_HAS_F2 = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2) != NULL) {
            GARAGE_DOOR_HAS_F2 = 1;
        }
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_5), garage_door_sensor, ch_group, GARAGE_DOOR_CLOSING)) {
            garage_door_sensor(0, ch_group, GARAGE_DOOR_OPENING);
        }
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_4), garage_door_sensor, ch_group, GARAGE_DOOR_OPENING)) {
            garage_door_sensor(0, ch_group, GARAGE_DOOR_OPENING);
        }
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3), garage_door_sensor, ch_group, GARAGE_DOOR_CLOSED)) {
            garage_door_sensor(0, ch_group, GARAGE_DOOR_CLOSED);
        }
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2), garage_door_sensor, ch_group, GARAGE_DOOR_OPENED)) {
            garage_door_sensor(0, ch_group, GARAGE_DOOR_OPENED);
        }

        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_6), garage_door_obstruction, ch_group, 0)) {
            garage_door_obstruction(0, ch_group, 0);
        }
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_7), garage_door_obstruction, ch_group, 1)) {
            garage_door_obstruction(0, ch_group, 1);
        }
        
        if (ch_group->homekit_enabled) {
            return accessory + 1;
        }
        return accessory;
    }
    
    uint8_t new_window_cover(uint8_t accessory, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group();
        ch_group->accessory = accessory_numerator;
        ch_group->homekit_enabled = acc_homekit_enabled(json_context);
        
        new_accessory(accessory, 3, ch_group->homekit_enabled, json_context);
        
        uint8_t cover_type = WINDOW_COVER_TYPE_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_TYPE) != NULL) {
            cover_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_TYPE)->valuedouble;
        }
        
        homekit_characteristic_t* ch0 = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_POSITION, 0);
        homekit_characteristic_t* ch1 = NEW_HOMEKIT_CHARACTERISTIC(TARGET_POSITION, 0, .setter_ex=hkc_window_cover_setter);
        homekit_characteristic_t* ch2 = NEW_HOMEKIT_CHARACTERISTIC(POSITION_STATE, WINDOW_COVER_STOP);
        homekit_characteristic_t* ch3 = NEW_HOMEKIT_CHARACTERISTIC(OBSTRUCTION_DETECTED, false);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[1]->id = 8;
            accessories[accessory]->services[1]->primary = true;
        
            switch (cover_type) {
                case 1:
                    accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_WINDOW;
                    break;
                    
                case 2:
                    accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_DOOR;
                    break;
                    
                default:    // case 0:
                    accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_WINDOW_COVERING;
                    break;
            }

            accessories[accessory]->services[1]->characteristics = calloc(5, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[1]->characteristics[0] = ch0;
            accessories[accessory]->services[1]->characteristics[1] = ch1;
            accessories[accessory]->services[1]->characteristics[2] = ch2;
            accessories[accessory]->services[1]->characteristics[3] = ch3;
        }
        
        ch_group->acc_type = ACC_TYPE_WINDOW_COVER;
        WINDOW_COVER_CH_CURRENT_POSITION = ch0;
        WINDOW_COVER_CH_TARGET_POSITION = ch1;
        WINDOW_COVER_CH_STATE = ch2;
        WINDOW_COVER_CH_OBSTRUCTION = ch3;
        WINDOW_COVER_TIME_OPEN = WINDOW_COVER_TIME_DEFAULT;
        WINDOW_COVER_TIME_CLOSE = WINDOW_COVER_TIME_DEFAULT;
        WINDOW_COVER_CORRECTION = WINDOW_COVER_CORRECTION_DEFAULT;
        WINDOW_COVER_MARGIN_SYNC = WINDOW_COVER_MARGIN_SYNC_DEFAULT;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        ch_group->last_wildcard_action[0] = NO_LAST_WILDCARD_ACTION;
        
        ch_group->timer = xTimerCreate(0, pdMS_TO_TICKS(WINDOW_COVER_POLL_PERIOD_MS), pdTRUE, (void*) ch_group, window_cover_timer_worker);
        
        ch_group->timer2 = xTimerCreate(0, pdMS_TO_TICKS(WINDOW_COVER_STOP_ENABLE_DELAY_MS), pdFALSE, (void*) ch_group, window_cover_timer_rearm_stop);
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_TIME_OPEN_SET) != NULL) {
            WINDOW_COVER_TIME_OPEN = cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_TIME_OPEN_SET)->valuedouble;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_TIME_CLOSE_SET) != NULL) {
            WINDOW_COVER_TIME_CLOSE = cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_TIME_CLOSE_SET)->valuedouble;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_CORRECTION_SET) != NULL) {
            WINDOW_COVER_CORRECTION = cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_CORRECTION_SET)->valuedouble;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_MARGIN_SYNC_SET) != NULL) {
            WINDOW_COVER_MARGIN_SYNC = cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_MARGIN_SYNC_SET)->valuedouble;
        }
        
        WINDOW_COVER_HOMEKIT_POSITION = set_initial_state(ch_group->accessory, 0, cJSON_Parse(INIT_STATE_LAST_STR), ch0, CH_TYPE_INT8, 0);
        WINDOW_COVER_MOTOR_POSITION = (WINDOW_COVER_HOMEKIT_POSITION * (1 + (0.02000000f * WINDOW_COVER_CORRECTION))) / (1 + (0.00020000f * WINDOW_COVER_CORRECTION * WINDOW_COVER_HOMEKIT_POSITION));
        ch0->value.int_value = (uint8_t) WINDOW_COVER_HOMEKIT_POSITION;
        ch1->value.int_value = ch0->value.int_value;
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, TYPE_WINDOW_COVER);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), window_cover_diginput, ch_group, WINDOW_COVER_CLOSING);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), window_cover_diginput, ch_group, WINDOW_COVER_OPENING);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2), window_cover_diginput, ch_group, WINDOW_COVER_STOP);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3), window_cover_diginput, ch_group, WINDOW_COVER_CLOSING + 3);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_4), window_cover_diginput, ch_group, WINDOW_COVER_OPENING + 3);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, TYPE_WINDOW_COVER);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), window_cover_diginput, ch_group, WINDOW_COVER_CLOSING);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), window_cover_diginput, ch_group, WINDOW_COVER_OPENING);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_2), window_cover_diginput, ch_group, WINDOW_COVER_STOP);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_3), window_cover_diginput, ch_group, WINDOW_COVER_CLOSING + 3);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_4), window_cover_diginput, ch_group, WINDOW_COVER_OPENING + 3);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_5), window_cover_obstruction, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_6), window_cover_obstruction, ch_group, 1);
        
        if (get_exec_actions_on_boot(json_context)) {
            window_cover_stop(ch_group);
        }
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_5), window_cover_obstruction, ch_group, 0)) {
            window_cover_obstruction(0, ch_group, 0);
        }
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_6), window_cover_obstruction, ch_group, 1)) {
            window_cover_obstruction(0, ch_group, 1);
        }

        if (ch_group->homekit_enabled) {
            return accessory + 1;
        }
        return accessory;
    }
    
    uint8_t new_fan(uint8_t accessory, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group();
        ch_group->accessory = accessory_numerator;
        ch_group->homekit_enabled = acc_homekit_enabled(json_context);
        
        new_accessory(accessory, 3, ch_group->homekit_enabled, json_context);
        
        uint8_t max_speed = 100;
        if (cJSON_GetObjectItemCaseSensitive(json_context, FAN_SPEED_STEPS) != NULL) {
            max_speed = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, FAN_SPEED_STEPS)->valuedouble;
        }
        
        homekit_characteristic_t* ch0 = NEW_HOMEKIT_CHARACTERISTIC(ON, false, .setter_ex=hkc_fan_setter);
        homekit_characteristic_t* ch1 = NEW_HOMEKIT_CHARACTERISTIC(ROTATION_SPEED, max_speed, .max_value=(float[]) {max_speed}, .setter_ex=hkc_fan_speed_setter);
        
        ch_group->acc_type = ACC_TYPE_FAN;
        ch_group->ch0 = ch0;
        ch_group->ch1 = ch1;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        ch_group->last_wildcard_action[0] = NO_LAST_WILDCARD_ACTION;
        ch_group->num[0] = autoswitch_time(json_context);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[1]->id = 8;
            accessories[accessory]->services[1]->primary = true;
            accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_FAN;
            accessories[accessory]->services[1]->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[1]->characteristics[0] = ch0;
            accessories[accessory]->services[1]->characteristics[1] = ch1;
        }
        
        float saved_max_speed = set_initial_state(ch_group->accessory, 1, cJSON_Parse(INIT_STATE_LAST_STR), ch1, CH_TYPE_FLOAT, max_speed);
        if (saved_max_speed > max_speed) {
            saved_max_speed = max_speed;
        }
        ch1->value.float_value = saved_max_speed;
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, TYPE_FAN);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, TYPE_FAN);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput_1, ch_group, TYPE_FAN);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput_0, ch_group, TYPE_FAN);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_1), digstate_1, ch_group, TYPE_FAN);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_0), digstate_0, ch_group, TYPE_FAN);
        
        const bool exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch_group, TYPE_FAN);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch_group, TYPE_FAN);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate_1, ch_group, TYPE_FAN);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate_0, ch_group, TYPE_FAN);
            
            ch0->value.bool_value = !((uint8_t) set_initial_state(ch_group->accessory, 0, json_context, ch0, CH_TYPE_BOOL, false));
            if (exec_actions_on_boot) {
                 hkc_fan_setter(ch0, HOMEKIT_UINT8(!ch0->value.bool_value));
            } else {
                hkc_fan_status_setter(ch0, HOMEKIT_UINT8(!ch0->value.bool_value));
            }
           
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch_group, TYPE_FAN)) {
                if (exec_actions_on_boot) {
                    diginput_1(0, ch_group, TYPE_FAN);
                } else {
                    digstate_1(0, ch_group, TYPE_FAN);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch_group, TYPE_FAN)) {
                ch0->value = HOMEKIT_UINT8(1);
                if (exec_actions_on_boot) {
                    diginput_0(0, ch_group, TYPE_FAN);
                } else {
                    digstate_0(0, ch_group, TYPE_FAN);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate_1, ch_group, TYPE_FAN)) {
                digstate_1(0, ch_group, TYPE_FAN);
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate_0, ch_group, TYPE_FAN)) {
                ch0->value = HOMEKIT_UINT8(1);
                digstate_0(0, ch_group, TYPE_FAN);
            }
        }
        
        if (ch_group->homekit_enabled) {
            return accessory + 1;
        }
        return accessory;
    }
    
    uint8_t new_tv(uint8_t accessory, cJSON* json_context) {
        cJSON* json_inputs = cJSON_GetObjectItemCaseSensitive(json_context, TV_INPUTS_ARRAY);
        uint8_t inputs = cJSON_GetArraySize(json_inputs);
        
        if (inputs == 0) {
            inputs = 1;
        }
        
        ch_group_t* ch_group = new_ch_group();
        ch_group->accessory = accessory_numerator;
        ch_group->homekit_enabled = acc_homekit_enabled(json_context);
        
        new_accessory(accessory, 4 + inputs, ch_group->homekit_enabled, json_context);

        homekit_characteristic_t* ch0 = NEW_HOMEKIT_CHARACTERISTIC(ACTIVE, 0, .setter_ex=hkc_tv_active);
        homekit_characteristic_t* ch1 = NEW_HOMEKIT_CHARACTERISTIC(CONFIGURED_NAME, "HAA TV", .setter_ex=hkc_tv_configured_name);
        homekit_characteristic_t* ch2 = NEW_HOMEKIT_CHARACTERISTIC(ACTIVE_IDENTIFIER, 1, .setter_ex=hkc_tv_active_identifier);
        homekit_characteristic_t* ch3 = NEW_HOMEKIT_CHARACTERISTIC(REMOTE_KEY, .setter_ex=hkc_tv_key);
        homekit_characteristic_t* ch4 = NEW_HOMEKIT_CHARACTERISTIC(POWER_MODE_SELECTION, 0, .setter_ex=hkc_tv_power_mode);
        
        homekit_characteristic_t* ch5 = NEW_HOMEKIT_CHARACTERISTIC(MUTE, false, .setter_ex=hkc_tv_mute);
        homekit_characteristic_t* ch6 = NEW_HOMEKIT_CHARACTERISTIC(ACTIVE, true);
        homekit_characteristic_t* ch7 = NEW_HOMEKIT_CHARACTERISTIC(VOLUME_SELECTOR, .setter_ex=hkc_tv_volume);

        ch_group->acc_type = ACC_TYPE_TV;
        ch_group->ch0 = ch0;
        ch_group->ch1 = ch1;
        ch_group->ch2 = ch2;
        ch_group->ch3 = ch3;
        ch_group->ch4 = ch4;
        ch_group->ch5 = ch5;
        ch_group->ch6 = ch6;
        ch_group->ch7 = ch7;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        
        homekit_service_t* new_tv_input_service(const uint8_t service_number, char* name) {
            INFO("TV Input: %s", name);
            
            homekit_service_t* *service = calloc(1, sizeof(homekit_service_t*));
            
            service[0] = calloc(1, sizeof(homekit_service_t));
            service[0]->id = 26 + (service_number * 8);
            service[0]->primary = false;
            service[0]->type = HOMEKIT_SERVICE_INPUT_SOURCE;
            service[0]->characteristics = calloc(7, sizeof(homekit_characteristic_t*));
            service[0]->characteristics[0] = NEW_HOMEKIT_CHARACTERISTIC(NAME, "I");
            service[0]->characteristics[1] = NEW_HOMEKIT_CHARACTERISTIC(IDENTIFIER, service_number);
            service[0]->characteristics[2] = NEW_HOMEKIT_CHARACTERISTIC(CONFIGURED_NAME, name, .setter_ex=hkc_tv_input_configured_name);
            service[0]->characteristics[3] = NEW_HOMEKIT_CHARACTERISTIC(INPUT_SOURCE_TYPE, HOMEKIT_INPUT_SOURCE_TYPE_HDMI);
            service[0]->characteristics[4] = NEW_HOMEKIT_CHARACTERISTIC(IS_CONFIGURED, true);
            service[0]->characteristics[5] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_VISIBILITY_STATE, HOMEKIT_CURRENT_VISIBILITY_STATE_SHOWN);
            
            return *service;
        }
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[1] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[1]->id = 18;
            accessories[accessory]->services[1]->primary = true;
            accessories[accessory]->services[1]->type = HOMEKIT_SERVICE_TELEVISION;
            accessories[accessory]->services[1]->characteristics = calloc(9, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[1]->characteristics[0] = ch0;
            accessories[accessory]->services[1]->characteristics[1] = ch1;
            accessories[accessory]->services[1]->characteristics[2] = ch2;
            accessories[accessory]->services[1]->characteristics[3] = NEW_HOMEKIT_CHARACTERISTIC(NAME, "TV");
            accessories[accessory]->services[1]->characteristics[4] = NEW_HOMEKIT_CHARACTERISTIC(SLEEP_DISCOVERY_MODE, HOMEKIT_SLEEP_DISCOVERY_MODE_ALWAYS_DISCOVERABLE);
            accessories[accessory]->services[1]->characteristics[5] = ch3;
            accessories[accessory]->services[1]->characteristics[6] = NEW_HOMEKIT_CHARACTERISTIC(PICTURE_MODE, HOMEKIT_PICTURE_MODE_STANDARD);
            accessories[accessory]->services[1]->characteristics[7] = ch4;
            
            accessories[accessory]->services[1]->linked = calloc(inputs + 1, sizeof(homekit_service_t*));
            
            for (uint8_t i = 0; i < inputs; i++) {
                cJSON* json_input = cJSON_GetArrayItem(json_inputs, i);
                
                char* name = strdup("TV");
                if (cJSON_GetObjectItemCaseSensitive(json_input, TV_INPUT_NAME) != NULL) {
                    free(name);
                    name = strdup(cJSON_GetObjectItemCaseSensitive(json_input, TV_INPUT_NAME)->valuestring);
                    if (cJSON_GetObjectItemCaseSensitive(json_input, "0") != NULL) {
                        uint8_t int_action = MAX_ACTIONS + i;
                        char action[3];
                        itoa(int_action, action, 10);
                        cJSON* json_new_input_action = cJSON_CreateObject();
                        cJSON_AddItemReferenceToObject(json_new_input_action, action, cJSON_GetObjectItemCaseSensitive(json_input, "0"));
                        register_actions(ch_group, json_new_input_action, int_action);
                        cJSON_Delete(json_new_input_action);
                    }
                }
                
                accessories[accessory]->services[1]->linked[i] = new_tv_input_service(i + 1, name);
                accessories[accessory]->services[i + 3] = accessories[accessory]->services[1]->linked[i];
            }
            
            accessories[accessory]->services[2] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[2]->id = 28;
            accessories[accessory]->services[2]->primary = false;
            accessories[accessory]->services[2]->type = HOMEKIT_SERVICE_TELEVISION_SPEAKER;
            accessories[accessory]->services[2]->characteristics = calloc(5, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[2]->characteristics[0] = ch5;
            accessories[accessory]->services[2]->characteristics[1] = ch6;
            accessories[accessory]->services[2]->characteristics[2] = NEW_HOMEKIT_CHARACTERISTIC(VOLUME_CONTROL_TYPE, HOMEKIT_VOLUME_CONTROL_TYPE_RELATIVE);
            accessories[accessory]->services[2]->characteristics[3] = ch7;
        }
        
        uint32_t configured_name = set_initial_state(ch_group->accessory, 1, cJSON_Parse(INIT_STATE_LAST_STR), ch1, CH_TYPE_STRING, 0);
        if (configured_name > 0) {
            homekit_value_destruct(&ch1->value);
            ch1->value = HOMEKIT_STRING((char*) configured_name);
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, TYPE_TV);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, TYPE_TV);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput_1, ch_group, TYPE_TV);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput_0, ch_group, TYPE_TV);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_1), digstate_1, ch_group, TYPE_TV);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_0), digstate_0, ch_group, TYPE_TV);
        
        const bool exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch_group, TYPE_TV);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch_group, TYPE_TV);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate_1, ch_group, TYPE_TV);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate_0, ch_group, TYPE_TV);
            
            ch0->value.int_value = !((uint8_t) set_initial_state(ch_group->accessory, 0, json_context, ch0, CH_TYPE_INT8, 0));
            if (exec_actions_on_boot) {
                hkc_tv_active(ch0, HOMEKIT_UINT8(!ch0->value.int_value));
            } else {
                hkc_tv_status_active(ch0, HOMEKIT_UINT8(!ch0->value.int_value));
            }
            
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput_1, ch_group, TYPE_TV)) {
                if (exec_actions_on_boot) {
                    diginput_1(0, ch_group, TYPE_TV);
                } else {
                    digstate_1(0, ch_group, TYPE_TV);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput_0, ch_group, TYPE_TV)) {
                ch0->value = HOMEKIT_UINT8(1);
                if (exec_actions_on_boot) {
                    diginput_0(0, ch_group, TYPE_TV);
                } else {
                    digstate_0(0, ch_group, TYPE_TV);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate_1, ch_group, TYPE_TV)) {
                digstate_1(0, ch_group, TYPE_TV);
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate_0, ch_group, TYPE_TV)) {
                ch0->value = HOMEKIT_UINT8(1);
                digstate_0(0, ch_group, TYPE_TV);
            }
        }
        
        if (ch_group->homekit_enabled) {
            return accessory + 1;
        }
        return accessory;
    }
    
    // *** Accessory Builder
    // Root device
    ch_group_t* root_device_ch_group = new_ch_group();
    register_actions(root_device_ch_group, json_config, 0);
    set_accessory_ir_protocol(root_device_ch_group, json_config);
    
    // Exec action 0 from root device
    do_actions(root_device_ch_group, 0);
    
    // Initial delay
    if (cJSON_GetObjectItemCaseSensitive(json_config, ACC_CREATION_DELAY) != NULL) {
        vTaskDelay(pdMS_TO_TICKS(cJSON_GetObjectItemCaseSensitive(json_config, ACC_CREATION_DELAY)->valuedouble * 1000));
    }
    
    // Bridge
    if (bridge_needed) {
        INFO("BRIDGE CREATED");
        new_accessory(0, 2, true, NULL);
        acc_count++;
    }
    
    for (uint8_t i = 0; i < total_accessories; i++) {
        INFO("\nACCESSORY %i", accessory_numerator);
        uint8_t acc_type = ACC_TYPE_SWITCH;
        if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_accessories, i), ACCESSORY_TYPE) != NULL) {
            acc_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_accessories, i), ACCESSORY_TYPE)->valuedouble;
        }
        
        cJSON* json_accessory = cJSON_GetArrayItem(json_accessories, i);
        
        // Creating HomeKit Accessory
        INFO("Type %i", acc_type);
        if (acc_type == ACC_TYPE_BUTTON) {
            acc_count = new_button_event(acc_count, json_accessory);
            
        } else if (acc_type == ACC_TYPE_LOCK || acc_type == ACC_TYPE_DOUBLE_LOCK) {
            acc_count = new_lock(acc_count, json_accessory, acc_type);
            
        } else if ((acc_type >= ACC_TYPE_CONTACT_SENSOR && acc_type <= ACC_TYPE_DOORBELL) ||
                   (acc_type >= ACC_POWER_MONITOR_INIT && acc_type <= ACC_POWER_MONITOR_END)) {
            acc_count = new_sensor(acc_count, json_accessory, acc_type);
            
        } else if (acc_type == ACC_TYPE_WATER_VALVE) {
            acc_count = new_water_valve(acc_count, json_accessory);
        
        } else if (acc_type == ACC_TYPE_THERMOSTAT ||
                   acc_type == ACC_TYPE_THERMOSTAT_WITH_HUM) {
            acc_count = new_thermostat(acc_count, json_accessory, acc_type);
            
        } else if (acc_type == ACC_TYPE_TEMP_SENSOR) {
            acc_count = new_temp_sensor(acc_count, json_accessory);
            
        } else if (acc_type == ACC_TYPE_HUM_SENSOR) {
            acc_count = new_hum_sensor(acc_count, json_accessory);
            
        } else if (acc_type == ACC_TYPE_TH_SENSOR) {
            acc_count = new_th_sensor(acc_count, json_accessory);
            
        } else if (acc_type == ACC_TYPE_LIGHTBULB) {
            acc_count = new_lightbulb(acc_count, json_accessory);
            
        } else if (acc_type == ACC_TYPE_GARAGE_DOOR) {
            acc_count = new_garage_door(acc_count, json_accessory);
            
        } else if (acc_type == ACC_TYPE_WINDOW_COVER) {
            acc_count = new_window_cover(acc_count, json_accessory);

        } else if (acc_type == ACC_TYPE_LIGHT_SENSOR) {
            //acc_count = new_light_sensor(acc_count, json_accessory);
            
        } else if (acc_type == ACC_TYPE_TV) {
            acc_count = new_tv(acc_count, json_accessory);
            
        } else if (acc_type == ACC_TYPE_FAN) {
            acc_count = new_fan(acc_count, json_accessory);
        
        } else {    // acc_type == ACC_TYPE_SWITCH || acc_type == ACC_TYPE_OUTLET
            acc_count = new_switch(acc_count, json_accessory, acc_type);
        }
        
        main_config.setup_mode_toggle_counter = INT8_MIN;
        
        FREEHEAP();
        
        // Accessory creation delay
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, ACC_CREATION_DELAY) != NULL) {
            vTaskDelay(pdMS_TO_TICKS(cJSON_GetObjectItemCaseSensitive(json_accessory, ACC_CREATION_DELAY)->valuedouble * 1000));
        }
        
        taskYIELD();
    }
    
    sysparam_set_int8(TOTAL_ACC_SYSPARAM, hk_total_ac);
    
    INFO("");
    
    // --- LIGHTBULBS INIT
    if (main_config.lightbulb_groups) {
        INFO("Init Lights");
        
        main_config.setpwm_bool_semaphore = false;
        
        lightbulb_group_t* lightbulb_group = main_config.lightbulb_groups;
        while (lightbulb_group) {
            ch_group_t* ch_group = ch_group_find(lightbulb_group->ch0);
            bool kill_switch = false;
            
            if (!ch_group->main_enabled) {
                ch_group->main_enabled = true;
                kill_switch = true;
            }
            
            hkc_rgbw_setter(lightbulb_group->ch0, HOMEKIT_BOOL(!lightbulb_group->ch0->value.bool_value));
            
            if (kill_switch) {
                ch_group->main_enabled = false;
            }
            
            lightbulb_group = lightbulb_group->next;
        }
        
        INFO("");
    }
    
    // --- HOMEKIT SET CONFIG
    // HomeKit Device Category
    if (cJSON_GetObjectItemCaseSensitive(json_config, HOMEKIT_DEVICE_CATEGORY_SET) != NULL) {
        config.category = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_config, HOMEKIT_DEVICE_CATEGORY_SET)->valuedouble;
        
    } else if (bridge_needed) {
        config.category = homekit_accessory_category_bridge;
        
    } else {
        switch (ch_group_find_by_acc(1)->acc_type) {
            case ACC_TYPE_SWITCH:
                config.category = homekit_accessory_category_switch;
                break;
                
            case ACC_TYPE_OUTLET:
                config.category = homekit_accessory_category_outlet;
                break;
                
            case ACC_TYPE_BUTTON:
                config.category = homekit_accessory_category_programmable_switch;
                break;
                
            case ACC_TYPE_LOCK:
                config.category = homekit_accessory_category_door_lock;
                break;
                
            case ACC_TYPE_CONTACT_SENSOR:
            case ACC_TYPE_MOTION_SENSOR:
            case ACC_TYPE_TEMP_SENSOR:
            case ACC_TYPE_HUM_SENSOR:
            case ACC_TYPE_TH_SENSOR:
            case ACC_TYPE_LIGHT_SENSOR:
                config.category = homekit_accessory_category_sensor;
                break;
                
            case ACC_TYPE_DOORBELL:
                config.category = homekit_accessory_category_video_door_bell;
                break;
                
            case ACC_TYPE_WATER_VALVE:
                config.category = homekit_accessory_category_faucet;
                break;
                
            case ACC_TYPE_THERMOSTAT:
            case ACC_TYPE_THERMOSTAT_WITH_HUM:
                config.category = homekit_accessory_category_air_conditioner;
                break;
                
            case ACC_TYPE_LIGHTBULB:
                config.category = homekit_accessory_category_lightbulb;
                break;
                
            case ACC_TYPE_GARAGE_DOOR:
                config.category = homekit_accessory_category_garage;
                break;
                
            case ACC_TYPE_WINDOW_COVER:
                config.category = homekit_accessory_category_window_covering;
                break;
                
            case ACC_TYPE_TV:
                config.category = homekit_accessory_category_television;
                break;
                
            case ACC_TYPE_FAN:
                config.category = homekit_accessory_category_fan;
                break;
                
            default:
                config.category = homekit_accessory_category_other;
                break;
        }
    }
    
    INFO("Device Category: %i\n", config.category);
    
    config.accessories = accessories;
    config.setupId = "JOSE";
    config.config_number = last_config_number;
    
    main_config.setup_mode_toggle_counter = 0;
    
    cJSON_Delete(json_haa);
    
    if (xTaskCreate(delayed_sensor_starter_task, "delayed_sensor", DELAYED_SENSOR_START_TASK_SIZE, NULL, DELAYED_SENSOR_START_TASK_PRIORITY, NULL) != pdPASS) {
        ERROR("Creating delayed_sensor_starter_task");
    }

    wifi_config_init("HAA", NULL, run_homekit_server, custom_hostname);
    
    led_blink(3);
    
    do_actions(root_device_ch_group, 1);

    //vTaskDelay(pdMS_TO_TICKS(8000)); sdk_wifi_station_disconnect(); sdk_wifi_station_connect();      // Emulates a Wifi disconnection. Keep comment for releases
    
    vTaskDelete(NULL);
}

void user_init(void) {
    sdk_wifi_station_set_auto_connect(false);
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_disconnect();
    sdk_wifi_set_sleep_type(WIFI_SLEEP_NONE);

    INFO("\n");
    
    // Sysparam starter
    sysparam_status_t status;
    status = sysparam_init(SYSPARAMSECTOR, 0);
    if (status == SYSPARAM_OK) {
        INFO("HAA Sysparam ready");
    } else {
        ERROR("HAA Sysparam. Creating...");

        for (uint8_t i = 0; i < SYSPARAMSIZE; i++) {
            spiflash_erase_sector(SYSPARAMSECTOR + (SECTORSIZE * i));
        }

        sysparam_create_area(SYSPARAMSECTOR, SYSPARAMSIZE, true);
        sysparam_init(SYSPARAMSECTOR, 0);
    }

    INFO("\n");
    INFO("size %i", sizeof(main_config.save_states_timer));
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    snprintf(main_config.name_value, 11, "HAA-%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    
    int8_t haa_setup = 1;
    
    //sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 2);    // Force to enter always in setup mode. Only for tests. Keep comment for releases
    
    //sysparam_set_string("ota_repo", "1");             // Simulates Installation with OTA. Only for tests. Keep comment for releases
    
    sysparam_get_int8(HAA_SETUP_MODE_SYSPARAM, &haa_setup);
    
    if (haa_setup > 0) {
        uart_set_baud(0, 115200);
        printf_header();
        INFO("SETUP MODE");
        wifi_config_init("HAA", NULL, NULL, name.value.string_value);
        
    } else {
#ifdef HAA_DEBUG
        free_heap_watchdog();
        xTimerStart(xTimerCreate(0, pdMS_TO_TICKS(1000), pdTRUE, NULL, free_heap_watchdog), XTIMER_BLOCK_TIME);
#endif // HAA_DEBUG
        
        // Arming emergency Setup Mode
        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 1);
        xTimerStart(xTimerCreate(0, pdMS_TO_TICKS(EXIT_EMERGENCY_SETUP_MODE_TIME), pdFALSE, NULL, disable_emergency_setup), XTIMER_BLOCK_TIME);
        
        name.value = HOMEKIT_STRING(main_config.name_value);
        
        if (xTaskCreate(normal_mode_init, "normal_init", INITIAL_SETUP_TASK_SIZE, NULL, INITIAL_SETUP_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("Creating normal_mode_init");
        }
    }
}

