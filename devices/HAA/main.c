/*
 * Home Accessory Architect
 *
 * Copyright 2019-2021 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#if defined(ESP_OPEN_RTOS)

#include <unistd.h>
#include <string.h>
#include <esp/uart.h>
#include <FreeRTOS.h>
#include <task.h>
#include <espressif/esp_common.h>
#include <rboot-api.h>
#include <sysparam.h>
#include <math.h>
#include <esplibs/libmain.h>

#elif defined(ESP_IDF)

#define sdk_system_restart()                esp_restart()

#else
#error "!!! UNKNOWN PLATFORM: ESP_OPEN_RTOS or ESP_IDF"
#endif

#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>
#ifdef HAA_DEBUG
#include <lwip/stats.h>
#endif // HAA_DEBUG

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <adv_button.h>
#include <adv_hlw.h>
#include <raven_ntp.h>
#include <adv_pwm.h>
#include <adv_nrzled.h>
#include <ping.h>
#include <adv_logger_ntp.h>
#include <adv_i2c.h>
#include <timers_helper.h>

#include <dht.h>
#include <ds18b20/ds18b20.h>

#include <cJSON.h>

#include "setup_mode/include/wifi_config.h"
#include "../common/ir_code.h"

#include "extra_characteristics.h"
#include "header.h"
#include "types.h"

main_config_t main_config = {
    .wifi_status = WIFI_STATUS_DISCONNECTED,
    .wifi_channel = 0,
    .wifi_ip = 0,
    .wifi_ping_max_errors = 255,
    .wifi_error_count = 0,
    .wifi_arp_count = 0,
    .wifi_roaming_count = 1,
    
    .setup_mode_toggle_counter = INT8_MIN,
    .setup_mode_toggle_counter_max = SETUP_MODE_DEFAULT_ACTIVATE_COUNT,
    .setup_mode_time = 0,
    
    .network_is_busy = false,
    .enable_homekit_server = false,

    .ir_tx_freq = 13,
    .ir_tx_gpio = MAX_GPIOS,
    .ir_tx_inv = false,
    
    .ping_poll_period = PING_POLL_PERIOD_DEFAULT,
    
    .setpwm_bool_semaphore = false,
    .setpwm_is_running = false,
    
    .clock_ready = false,
    .timetable_ready = false,
    
    .used_gpio = 0,
    
    .setup_mode_toggle_timer = NULL,
    
    .ch_groups = NULL,
    .lightbulb_groups = NULL,
    .ping_inputs = NULL,
    .last_states = NULL,
    
    .status_led = NULL,
    
    .ntp_host = NULL,
    .timetable_actions = NULL
};

// https://martin.ankerl.com/2012/01/25/optimized-approximative-pow-in-c-and-cpp/
double fast_precise_pow(double a, double b) {
    int e = abs((int)b);
    
    union {
        double d;
        int x[2];
    } u = { a };
    
    u.x[1] = (int) ((b - e) * (u.x[1] - 1072632447) + 1072632447);
    u.x[0] = 0;
    
    double r = 1.0;
    
    while (e) {
        if (e & 1) {
            r *= a;
        }
        a *= a;
        e >>= 1;
    }
    
    return r * u.d;
}

bool get_used_gpio(const uint16_t gpio) {
    if (gpio < MAX_GPIOS) {
        const uint64_t bit = 1 << gpio;
        return main_config.used_gpio & bit;
    }
    
    return false;
}

void set_used_gpio(const uint16_t gpio) {
    if (gpio < MAX_GPIOS) {
        if (gpio == 1) {
            gpio_set_iomux_function(1, IOMUX_GPIO1_FUNC_GPIO);
        } else if (gpio == 3) {
            gpio_set_iomux_function(3, IOMUX_GPIO3_FUNC_GPIO);
        }
        
        const uint64_t bit = 1 << gpio;
        main_config.used_gpio |= bit;
    }
}

mcp23017_t* IRAM mcp_find_by_index(const uint8_t index) {
    mcp23017_t* mcp23017 = main_config.mcp23017s;
    while (mcp23017 && mcp23017->index != index) {
        mcp23017 = mcp23017->next;
    }
    
    return mcp23017;
}

void IRAM extended_gpio_write(const uint16_t extended_gpio, bool value) {
    if (extended_gpio < 17) {
        gpio_write(extended_gpio, value);
        
    } else {    // MCP23017
        const uint8_t gpio_type = extended_gpio / 100;
        mcp23017_t* mcp23017 = mcp_find_by_index(gpio_type);
        
        if (mcp23017) {
            uint8_t gpio = extended_gpio % 100;
            uint8_t mcp_outs = mcp23017->a_outs;
            uint8_t mcp_reg = 0x14;
            if (gpio > 7) {
                gpio -= 8;
                mcp_outs = mcp23017->b_outs;
                mcp_reg = 0x15;
            }
            
            const uint8_t bit = 1 << gpio;
            
            if (value) {
                mcp_outs |= bit;
            } else if ((mcp_outs & bit) != 0) {
                mcp_outs ^= bit;
            }
            
            if (mcp_reg == 0x14) {
                mcp23017->a_outs = mcp_outs;
            } else {
                mcp23017->b_outs = mcp_outs;
            }
            
            i2c_slave_write(mcp23017->bus, mcp23017->addr, &mcp_reg, 1, &mcp_outs, 1);
        }
    }
}

void led_code_run(TimerHandle_t xTimer) {
    uint16_t delay = STATUS_LED_DURATION_OFF;
    
    main_config.status_led->status = !main_config.status_led->status;
    extended_gpio_write(main_config.status_led->gpio, main_config.status_led->status);
    
    if (main_config.status_led->status == main_config.status_led->inverted) {
        main_config.status_led->count++;
    } else {
        delay = STATUS_LED_DURATION_ON;
    }
    
    if (main_config.status_led->count < main_config.status_led->times) {
        esp_timer_change_period(xTimer, delay);
    }
}

void led_blink(const uint8_t times) {
    if (main_config.status_led) {
        esp_timer_stop(main_config.status_led->timer);
        
        main_config.status_led->times = times;
        main_config.status_led->status = main_config.status_led->inverted;
        main_config.status_led->count = 0;
        
        led_code_run(main_config.status_led->timer);
    }
}

void led_create(const uint8_t gpio, const bool inverted) {
    main_config.status_led = malloc(sizeof(led_t));
    memset(main_config.status_led, 0, sizeof(*main_config.status_led));
    
    main_config.status_led->timer = esp_timer_create(10, false, NULL, led_code_run);
    
    main_config.status_led->gpio = gpio;
    main_config.status_led->inverted = inverted;
    main_config.status_led->status = inverted;
    
    if (gpio < 17) {
        gpio_write(gpio, inverted);
        gpio_enable(gpio, GPIO_OUTPUT);
    }
    
    extended_gpio_write(gpio, inverted);
}

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
        stats_display();
    }
}
#endif  // HAA_DEBUG

void disable_emergency_setup(TimerHandle_t xTimer) {
    INFO("Disarming Emergency Setup Mode");
    sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
    esp_timer_delete(xTimer);
}

uint16_t process_hexstr(const char* string, uint8_t** output_hex_string) {
    const uint16_t len = strlen(string) >> 1;
    uint8_t* hex_string = malloc(len);
    memset(hex_string, 0, len);
    
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

ch_group_t* new_ch_group(const uint8_t chs, const uint8_t nums, const uint8_t last_wildcard_actions) {
    ch_group_t* ch_group = malloc(sizeof(ch_group_t));
    memset(ch_group, 0, sizeof(*ch_group));
    
    if (chs > 0) {
        size_t size = chs * sizeof(homekit_characteristic_t*);
        ch_group->chs = chs;
        ch_group->ch = (homekit_characteristic_t**) malloc(size);
        memset(ch_group->ch, 0, size);
    }
    
    if (nums > 0) {
        size_t size = nums * sizeof(float);
        ch_group->num = (float*) malloc(size);
        memset(ch_group->num, 0, size);
    }
    
    if (last_wildcard_actions > 0) {
        ch_group->last_wildcard_action = (float*) malloc(last_wildcard_actions * sizeof(float));
        
        for (uint8_t i = 0; i < last_wildcard_actions; i++) {
            ch_group->last_wildcard_action[i] = NO_LAST_WILDCARD_ACTION;
        }
    }
    
    ch_group->next = main_config.ch_groups;
    main_config.ch_groups = ch_group;
    
    return ch_group;
}

ch_group_t* IRAM ch_group_find(homekit_characteristic_t* ch) {
    ch_group_t* ch_group = main_config.ch_groups;
    while (ch_group) {
        for (uint8_t i = 0; i < ch_group->chs; i++) {
            if (ch_group->ch[i] == ch) {
                return ch_group;
            }
        }
        
        ch_group = ch_group->next;
    }

    return NULL;
}

ch_group_t* IRAM ch_group_find_by_acc(uint16_t accessory) {
    ch_group_t* ch_group = main_config.ch_groups;
    while (ch_group &&
           ch_group->accessory != accessory) {
        ch_group = ch_group->next;
    }

    return ch_group;
}

lightbulb_group_t* IRAM lightbulb_group_find(homekit_characteristic_t* ch) {
    lightbulb_group_t* lightbulb_group = main_config.lightbulb_groups;
    while (lightbulb_group &&
           lightbulb_group->ch0 != ch) {
        lightbulb_group = lightbulb_group->next;
    }

    return lightbulb_group;
}

addressled_t* addressled_find(const uint8_t gpio) {
    addressled_t* addressled = main_config.addressleds;
    while (addressled &&
           addressled->gpio != gpio) {
        addressled = addressled->next;
    }

    return addressled;
}

addressled_t* new_addressled(const uint8_t gpio, const uint16_t max_range) {
    addressled_t* addressled = addressled_find(gpio);
    
    if (addressled) {
        if (max_range > addressled->max_range) {
            addressled->max_range = max_range;
        }
    } else {
        gpio_enable(gpio, GPIO_OUTPUT);
        gpio_write(gpio, false);
        
        addressled = malloc(sizeof(addressled_t));
        memset(addressled, 0, sizeof(*addressled));
        
        addressled->gpio = gpio;
        addressled->max_range = max_range;
        
        addressled->map[0] = 1;
        addressled->map[1] = 0;
        addressled->map[2] = 2;
        addressled->map[3] = 3;
        addressled->map[4] = 4;
        
        // WS2812B 800MHz
        addressled->time_0 = nrz_ticks(0.4);
        addressled->time_1 = nrz_ticks(0.8);
        addressled->period = nrz_ticks(0.4 + 0.85);
        
        addressled->next = main_config.addressleds;
        main_config.addressleds = addressled;
    }
    
    return addressled;
}

void save_historical_data(homekit_characteristic_t* ch_hist) {
    if (!main_config.clock_ready) {
        return;
    }
    
    time_t time = raven_ntp_get_time_t();
    
    ch_group_t* ch_group = main_config.ch_groups;
    while (ch_group) {
        if (ch_group->ch_hist == ch_hist && ch_group->main_enabled) {
            float value = 0;
            switch (ch_hist->value.format) {
                case HOMETKIT_FORMAT_BOOL:
                    value = (uint8_t) ch_hist->value.bool_value;
                    break;
                    
                case HOMETKIT_FORMAT_UINT8:
                case HOMETKIT_FORMAT_UINT16:
                case HOMETKIT_FORMAT_UINT32:
                case HOMETKIT_FORMAT_UINT64:
                case HOMETKIT_FORMAT_INT:
                    value = ch_hist->value.int_value;
                    break;
                    
                case HOMETKIT_FORMAT_FLOAT:
                    value = ch_hist->value.float_value;
                    break;
                    
                default:
                    return;
            }
            
            uint32_t final_time = time;
            int32_t final_data = value * FLOAT_FACTOR_SAVE_AS_INT;
            
            //INFO("Saved %i, %i (%0.5f)", final_time, final_data, value);
            
            uint32_t last_register = HIST_LAST_REGISTER;
            last_register += HIST_REGISTER_SIZE;
            uint32_t current_ch = last_register / HIST_BLOCK_SIZE;
            uint32_t current_pos = last_register % HIST_BLOCK_SIZE;
            
            if (current_ch >= ch_group->chs) {
                current_ch = 0;
                current_pos = 0;
            }
            
            //INFO("Current ch & pos: %i, %i", current_ch, current_pos);
            
            if (ch_group->ch[current_ch]->value.data_size < HIST_BLOCK_SIZE) {
                ch_group->ch[current_ch]->value.data_size = HIST_BLOCK_SIZE;
            }
            
            memcpy(ch_group->ch[current_ch]->value.data_value + current_pos, &final_time, HIST_TIME_SIZE);
            memcpy(ch_group->ch[current_ch]->value.data_value + current_pos + HIST_TIME_SIZE, &final_data, HIST_DATA_SIZE);
            
            HIST_LAST_REGISTER = (current_ch * HIST_BLOCK_SIZE) + current_pos;
        }
        
        ch_group = ch_group->next;
    }
}

void historical_timer_worker(TimerHandle_t xTimer) {
    homekit_characteristic_t* ch = (homekit_characteristic_t*) pvTimerGetTimerID(xTimer);
    save_historical_data(ch);
}

int ping_host(char* host) {
    if (main_config.wifi_status != WIFI_STATUS_CONNECTED) {
        return false;
    }
    
    int ping_result = -1;
    
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

void reboot_task() {
    led_blink(5);
    
    INFO("\nRebooting\n");
    esp_timer_stop(WIFI_WATCHDOG_TIMER);
    
    vTaskDelay(MS_TO_TICKS((hwrand() % RANDOM_DELAY_MS) + 1000));
    
    sdk_system_restart();
}

void reboot_haa() {
    if (xTaskCreate(reboot_task, "reboot", REBOOT_TASK_SIZE, NULL, REBOOT_TASK_PRIORITY, NULL) != pdPASS) {
        ERROR("Creating reboot");
        homekit_remove_oldest_client();
    }
}

void ntp_task() {
    int result = -10;
    uint8_t tries = 0;
    for (;;) {
        tries++;
        
        if (main_config.network_is_busy) {
            vTaskDelay(MS_TO_TICKS(234));
        }
        main_config.network_is_busy = true;
        
        result = raven_ntp_update(main_config.ntp_host);
        
        main_config.network_is_busy = false;
        
        INFO("NTP upd (%i)", result);
        
        if (result != 0 && tries < 4) {
            vTaskDelay(MS_TO_TICKS(10000));
        } else {
            if (!main_config.clock_ready && result == 0) {
                main_config.clock_ready = true;
                
                ch_group_t* ch_group = main_config.ch_groups;
                while (ch_group) {
                    if (ch_group->acc_type == ACC_TYPE_HISTORICAL && ch_group->child_enabled) {
                        save_historical_data(ch_group->ch_hist);
                    }
                    
                    ch_group = ch_group->next;
                }
            }
            
            break;
        }
    }
    
    vTaskDelete(NULL);
}

void ntp_timer_worker(TimerHandle_t xTimer) {
    if (!homekit_is_pairing()) {
        if (main_config.wifi_status != WIFI_STATUS_CONNECTED) {
            raven_ntp_get_time_t();
        } else if (xTaskCreate(ntp_task, "ntp", NTP_TASK_SIZE, NULL, NTP_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("Creating NTP");
            homekit_remove_oldest_client();
            raven_ntp_get_time_t();
        }
    } else {
        ERROR("ntp_task: HK pairing");
        raven_ntp_get_time_t();
    }
}

void wifi_ping_gw_task() {
    main_config.network_is_busy = true;
    
    struct ip_info info;
    if (sdk_wifi_get_ip_info(STATION_IF, &info)) {
        char gw_host[16];
        snprintf(gw_host, 16, IPSTR, IP2STR(&info.gw));

        int ping_result = ping_host(gw_host);

        if (ping_result == 0) {
            main_config.wifi_error_count++;
            ERROR("GW %s ping (%i/%i)", gw_host, main_config.wifi_error_count - 1, main_config.wifi_ping_max_errors - 1);
        } else if (ping_result == 1) {
            main_config.wifi_error_count = 0;
        }
    }
    
    main_config.network_is_busy = false;
    vTaskDelete(NULL);
}

void wifi_reconnection_task(void* args) {
    main_config.wifi_status = WIFI_STATUS_DISCONNECTED;
    esp_timer_stop(WIFI_WATCHDOG_TIMER);
    homekit_mdns_announce_pause();
    
    INFO("Wifi reconnection process started");
    
    do_actions(ch_group_find_by_acc(ACC_TYPE_ROOT_DEVICE), 4);

    led_blink(3);
    
    if ((uint32_t) args == 1) {
        vTaskDelay(MS_TO_TICKS(500));
        wifi_config_reset();
    }
    
    for (;;) {
        vTaskDelay(MS_TO_TICKS(WIFI_RECONNECTION_POLL_PERIOD_MS));
        
        const int new_ip = wifi_config_get_ip();
        if (new_ip >= 0) {
            vTaskDelay(MS_TO_TICKS(2000));
            
            main_config.wifi_status = WIFI_STATUS_CONNECTED;
            main_config.wifi_error_count = 0;
            main_config.wifi_arp_count = 0;
            main_config.wifi_channel = sdk_wifi_get_channel();
            main_config.wifi_ip = new_ip;
            
            homekit_mdns_announce();

            do_actions(ch_group_find_by_acc(ACC_TYPE_ROOT_DEVICE), 3);
            
            esp_timer_start(WIFI_WATCHDOG_TIMER);
            
            INFO("Wifi reconnection OK");
            
            break;
            
        } else if (main_config.wifi_status == WIFI_STATUS_DISCONNECTED) {
            INFO("Wifi reconnecting...");
            main_config.wifi_status = WIFI_STATUS_CONNECTING;
            int8_t phy_mode = 3;
            sysparam_get_int8(WIFI_LAST_WORKING_PHY_SYSPARAM, &phy_mode);
            wifi_config_connect(1, phy_mode, true);
            
        } else {
            main_config.wifi_error_count++;
            if (main_config.wifi_error_count > WIFI_DISCONNECTED_LONG_TIME) {
                ERROR("Wifi disconnected for a long time");
                main_config.wifi_error_count = 0;
                main_config.wifi_status = WIFI_STATUS_DISCONNECTED;
                
                led_blink(3);
                
                vTaskDelay(MS_TO_TICKS(600));
                
                do_actions(ch_group_find_by_acc(ACC_TYPE_ROOT_DEVICE), 5);
            }
        }
    }
    
    vTaskDelete(NULL);
}

void wifi_watchdog() {
    //INFO("Wifi status = %i", main_config.wifi_status);
    const int current_ip = wifi_config_get_ip();
    if (current_ip >= 0 && main_config.wifi_error_count <= main_config.wifi_ping_max_errors) {
        uint8_t current_channel = sdk_wifi_get_channel();
        
        if (main_config.wifi_mode == 3) {
            if (main_config.wifi_roaming_count == 0) {
                esp_timer_change_period(WIFI_WATCHDOG_TIMER, WIFI_WATCHDOG_POLL_PERIOD_MS);
            }
            
            main_config.wifi_roaming_count++;
            
            if (main_config.wifi_roaming_count >= WIFI_WATCHDOG_ROAMING_PERIOD) {
                esp_timer_change_period(WIFI_WATCHDOG_TIMER, 5000);
                main_config.wifi_roaming_count = 0;
                wifi_config_smart_connect();
            }
        }
        
        if (main_config.wifi_channel != current_channel) {
            main_config.wifi_channel = current_channel;
            INFO("Wifi new Ch: %i", current_channel);
            homekit_mdns_announce();
            do_actions(ch_group_find_by_acc(ACC_TYPE_ROOT_DEVICE), 6);
        }
        
        if (main_config.wifi_ip != current_ip) {
            main_config.wifi_ip = current_ip;
            homekit_mdns_announce();
            do_actions(ch_group_find_by_acc(ACC_TYPE_ROOT_DEVICE), 7);
        }
        
        main_config.wifi_arp_count++;
        if (main_config.wifi_arp_count >= WIFI_WATCHDOG_ARP_RESEND_PERIOD) {
            main_config.wifi_arp_count = 0;
            wifi_config_resend_arp();
        }
        
        if (main_config.wifi_ping_max_errors != 255 && !homekit_is_pairing() && !main_config.network_is_busy) {
            if (xTaskCreate(wifi_ping_gw_task, "gwping", WIFI_PING_GW_TASK_SIZE, NULL, WIFI_PING_GW_TASK_PRIORITY, NULL) != pdPASS) {
                ERROR("Creating wifi_ping_gw");
                homekit_remove_oldest_client();
            }
        }
        
    } else {
        ERROR("Wifi error");
        
        uint32_t force_disconnect = 0;
        if (main_config.wifi_error_count > main_config.wifi_ping_max_errors) {
            force_disconnect = 1;
        }
        
        main_config.wifi_error_count = 0;
        
        if (xTaskCreate(wifi_reconnection_task, "recon", WIFI_RECONNECTION_TASK_SIZE, (void*) force_disconnect, WIFI_RECONNECTION_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("Creating wifi_reconnection");
            homekit_remove_oldest_client();
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
    main_config.network_is_busy = true;

    void ping_input_run_callback_fn(ping_input_callback_fn_t* callbacks) {
        ping_input_callback_fn_t* ping_input_callback_fn = callbacks;
        
        while (ping_input_callback_fn) {
            if (!ping_input_callback_fn->disable_without_wifi ||
                (ping_input_callback_fn->disable_without_wifi && wifi_config_get_ip() >= 0)) {
                ping_input_callback_fn->callback(88, ping_input_callback_fn->ch_group, ping_input_callback_fn->param);
            }
            ping_input_callback_fn = ping_input_callback_fn->next;
        }
    }
    
    ping_input_t* ping_input = main_config.ping_inputs;
    while (ping_input) {
        int ping_result;
        
        uint8_t i = 0;
        while (((ping_result = ping_host(ping_input->host)) != 1) && i < PING_RETRIES) {
            i++;
            vTaskDelay(MS_TO_TICKS(250));
        }
        
        if ((ping_result == 1) && (!ping_input->last_response || ping_input->ignore_last_response)) {
            ping_input->last_response = true;
            INFO("Ping %s OK", ping_input->host);
            ping_input_run_callback_fn(ping_input->callback_1);

        } else if ((ping_result == 0) && (ping_input->last_response || ping_input->ignore_last_response)) {
            ping_input->last_response = false;
            INFO("Ping %s FAIL", ping_input->host);
            ping_input_run_callback_fn(ping_input->callback_0);
        }
        
        ping_input = ping_input->next;
    }
    
    main_config.network_is_busy = false;
    vTaskDelete(NULL);
}

void ping_task_timer_worker() {
    if (!main_config.network_is_busy && !homekit_is_pairing()) {
        if (xTaskCreate(ping_task, "ping", PING_TASK_SIZE, NULL, PING_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("Creating ping");
            homekit_remove_oldest_client();
        }
    } else {
        ERROR("ping_task: network busy %i, HK pairing %i", main_config.network_is_busy, homekit_is_pairing());
    }
}

// -----

void setup_mode_call(const uint16_t gpio, void* args, const uint8_t param) {
    INFO("Setup mode call");
    
    if (main_config.setup_mode_time == 0 || xTaskGetTickCountFromISR() < main_config.setup_mode_time * (1000 / portTICK_PERIOD_MS)) {
        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 1);
        reboot_haa();
    } else {
        ERROR("Not allowed after %i secs since boot", main_config.setup_mode_time);
    }
}

void setup_mode_toggle_upcount() {
    if (main_config.setup_mode_toggle_counter_max > 0) {
        main_config.setup_mode_toggle_counter++;
        INFO("Setup mode trigger %i/%i", main_config.setup_mode_toggle_counter, main_config.setup_mode_toggle_counter_max);
        
        if (main_config.setup_mode_toggle_counter == main_config.setup_mode_toggle_counter_max) {
            setup_mode_call(99, NULL, 0);
        } else {
            esp_timer_start(main_config.setup_mode_toggle_timer);
        }
    }
}

void setup_mode_toggle() {
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
                
            case CH_TYPE_INT:
                status = sysparam_set_int32(last_state->id, last_state->ch->value.int_value);
                break;
                
            case CH_TYPE_FLOAT:
                status = sysparam_set_int32(last_state->id, (int) (last_state->ch->value.float_value * FLOAT_FACTOR_SAVE_AS_INT));
                break;
                
            case CH_TYPE_STRING:
                status = sysparam_set_string(last_state->id, last_state->ch->value.string_value);
                break;
                
            default:    // case CH_TYPE_BOOL
                status = sysparam_set_bool(last_state->id, last_state->ch->value.bool_value);
                break;
        }
        
        if (status != SYSPARAM_OK) {
            ERROR("Flash saving for Ch%s", last_state->id);
        }
        
        last_state = last_state->next;
    }
}

inline void save_states_callback() {
    esp_timer_start(SAVE_STATES_TIMER);
}

void IRAM homekit_characteristic_notify_safe(homekit_characteristic_t *ch) {
    if (ch_group_find(ch)->homekit_enabled && main_config.wifi_status == WIFI_STATUS_CONNECTED && main_config.enable_homekit_server) {
        homekit_characteristic_notify(ch);
    }
}

void hkc_custom_ota_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    if (!strcmp(value.string_value, CUSTOM_TRIGGER_COMMAND)) {
        INFO("<0> OTA update");
        rboot_set_temp_rom(1);
        homekit_characteristic_notify(ch);
        reboot_haa();
    }
}

void hkc_custom_setup_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    if (!strcmp(value.string_value, CUSTOM_TRIGGER_COMMAND)) {
        INFO("<0> Setup mode");
        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 1);
        homekit_characteristic_notify(ch);
        reboot_haa();
    }
}

void hkc_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    INFO("<%i> Generic setter", ch_group->accessory);
    ch->value = value;
    homekit_characteristic_notify_safe(ch);
    
    save_historical_data(ch);
    
    save_states_callback();
}

void pm_custom_consumption_reset(ch_group_t* ch_group) {
    if (!main_config.clock_ready) {
        ERROR("<%i> Clock not ready", ch_group->accessory);
        return;
    }
    
    led_blink(1);
    INFO("<%i> Setter Consumption Reset", ch_group->accessory);
    
    time_t time = raven_ntp_get_time_t();
    
    ch_group->ch[4]->value.float_value = ch_group->ch[3]->value.float_value;
    ch_group->ch[3]->value.float_value = 0;
    ch_group->ch[5]->value.int_value = time;
    
    save_states_callback();
    
    save_historical_data(ch_group->ch[4]);
    
    homekit_characteristic_notify_safe(ch_group->ch[3]);
    homekit_characteristic_notify_safe(ch_group->ch[4]);
    homekit_characteristic_notify_safe(ch_group->ch[5]);
    homekit_characteristic_notify_safe(ch_group->ch[6]);
}

void hkc_custom_consumption_reset_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    if (!strcmp(value.string_value, CUSTOM_TRIGGER_COMMAND)) {
        ch_group_t* ch_group = ch_group_find(ch);
        pm_custom_consumption_reset(ch_group);
    }
}

// --- ON
void hkc_on_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        if (ch->value.bool_value != value.bool_value) {
            led_blink(1);
            INFO("<%i> Setter ON %i", ch_group->accessory, value.bool_value);
            
            ch->value.bool_value = value.bool_value;
            
            if (ch->value.bool_value) {
                esp_timer_start(ch_group->timer2);
            } else {
                esp_timer_stop(ch_group->timer2);
            }
            
            setup_mode_toggle_upcount();
            save_states_callback();
            
            if (ch_group->chs > 1) {
                if (value.bool_value) {
                    ch_group->ch[2]->value.int_value = ch_group->ch[1]->value.int_value;
                    esp_timer_start(ch_group->timer);
                } else {
                    ch_group->ch[2]->value.int_value = 0;
                    esp_timer_stop(ch_group->timer);
                }
                
                homekit_characteristic_notify_safe(ch_group->ch[2]);
            }
            
            save_historical_data(ch);
            
            do_actions(ch_group, (uint8_t) ch->value.bool_value);
        }
    }
    
    homekit_characteristic_notify_safe(ch);
}

void hkc_on_status_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    if (ch->value.bool_value != value.bool_value) {
        led_blink(1);
        ch_group_t* ch_group = ch_group_find(ch);
        INFO("<%i> Setter Status ON %i", ch_group->accessory, value.bool_value);
        ch->value.bool_value = value.bool_value;
        
        homekit_characteristic_notify_safe(ch);
    }
}

void on_timer_worker(TimerHandle_t xTimer) {
    ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
    
    ch_group->ch[2]->value.int_value--;
    
    if (ch_group->ch[2]->value.int_value == 0) {
        esp_timer_stop(ch_group->timer);
        
        hkc_on_setter(ch_group->ch[0], HOMEKIT_BOOL(false));
    }
}

// --- LOCK MECHANISM
void hkc_lock_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        if (ch->value.int_value != value.int_value) {
            led_blink(1);
            INFO("<%i> Setter LOCK %i", ch_group->accessory, value.int_value);
            
            ch->value.int_value = value.int_value;
            
            ch_group->ch[0]->value.int_value = value.int_value;
            
            if (ch->value.int_value == 0) {
                esp_timer_start(ch_group->timer2);
            } else {
                esp_timer_stop(ch_group->timer2);
            }
            
            setup_mode_toggle_upcount();
            save_states_callback();
            
            save_historical_data(ch);
            
            do_actions(ch_group, (uint8_t) ch->value.int_value);
        }
    }
    
    homekit_characteristic_notify_safe(ch_group->ch[0]);
    homekit_characteristic_notify_safe(ch);
}

void hkc_lock_status_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    if (ch->value.int_value != value.int_value) {
        led_blink(1);
        ch_group_t* ch_group = ch_group_find(ch);
        
        INFO("<%i> Setter Status LOCK %i", ch_group->accessory, value.int_value);
        
        ch->value.int_value = value.int_value;
        
        ch_group->ch[0]->value.int_value = value.int_value;

        homekit_characteristic_notify_safe(ch_group->ch[0]);
        homekit_characteristic_notify_safe(ch);
    }
}

// --- BUTTON EVENT / DOORBELL
void button_event(const uint8_t gpio, void* args, const uint8_t event_type) {
    ch_group_t* ch_group = args;
    
    if (ch_group->child_enabled) {
        led_blink(1);
        INFO("<%i> Setter EVENT %i", ch_group->accessory, event_type);
        
        ch_group->ch[0]->value.int_value = event_type;
        homekit_characteristic_notify_safe(ch_group->ch[0]);
        
        save_historical_data(ch_group->ch[0]);
        
        do_actions(ch_group, event_type);
        
        if (ch_group->chs > 1 && event_type == 0) {
            INFO("<%i> Ding-dong", ch_group->accessory);
            if ((uint8_t) DOORBELL_LAST_STATE == 1) {
                DOORBELL_LAST_STATE = 0;
            } else {
                DOORBELL_LAST_STATE = 1;
            }
            ch_group->ch[1]->value.int_value = (uint8_t) DOORBELL_LAST_STATE;
            homekit_characteristic_notify_safe(ch_group->ch[1]);
            
            save_historical_data(ch_group->ch[1]);
        }
        
        setup_mode_toggle_upcount();
    }
}

// --- SENSORS
void binary_sensor(const uint16_t gpio, void* args, uint8_t type) {
    ch_group_t* ch_group = args;

    if (ch_group->main_enabled) {
        bool new_value = type;
        if (type >= 4) {
            if (ch_group->acc_type == ACC_TYPE_CONTACT_SENSOR) {
                new_value = !ch_group->ch[0]->value.int_value;
            } else {
                new_value = !ch_group->ch[0]->value.bool_value;
            }
            
            if (type == 4) {
                type = new_value;
            } else {    // type == 5
                type = 2 + new_value;
            }
        } else if (type == 2) {
            new_value = false;
        }
        
        if ((ch_group->acc_type == ACC_TYPE_CONTACT_SENSOR &&
            ch_group->ch[0]->value.int_value != new_value) ||
            (ch_group->acc_type == ACC_TYPE_MOTION_SENSOR &&
            ch_group->ch[0]->value.bool_value != new_value)) {
            led_blink(1);
            
            INFO("<%i> DigI Sensor %i GPIO %i", ch_group->accessory, type, gpio);
            
            if (ch_group->acc_type == ACC_TYPE_CONTACT_SENSOR) {
                ch_group->ch[0]->value.int_value = new_value;
            } else {
                ch_group->ch[0]->value.bool_value = new_value;
            }
            
            if (type <= 1) {
                if ((ch_group->acc_type == ACC_TYPE_CONTACT_SENSOR && ch_group->ch[0]->value.int_value == 1) ||
                    (ch_group->acc_type == ACC_TYPE_MOTION_SENSOR && ch_group->ch[0]->value.bool_value == true)) {
                    esp_timer_start(ch_group->timer2);
                } else {
                    esp_timer_stop(ch_group->timer2);
                }
                
                save_historical_data(ch_group->ch[0]);
                
                do_actions(ch_group, type);
            }
        }
    }
    
    homekit_characteristic_notify_safe(ch_group->ch[0]);
}

void power_monitor_task(void* args) {
    ch_group_t* ch_group = args;
    
    float voltage = 0;
    float current = 0;
    float power = 0;
    
    bool get_data = true;
    
    const uint8_t pm_sensor_type = PM_SENSOR_TYPE;
    if (pm_sensor_type >= 2) {
        int convert_ade_read(uint8_t data[]) {
            int value = 0;
            
            for (uint8_t i = 0; i < 4; i++) {
                value = (value << 8) | data[i];
            }
            
            uint32_t sign_mask = sign_mask = (1 << 31);
            
            if ((value & sign_mask) != 0) {
                value &= ~sign_mask;
                value |= (1 << 31);
            }
            
            return value;
        }
        
        const uint8_t bus = PM_SENSOR_ADE_BUS;
        const uint8_t addr = PM_SENSOR_ADE_ADDR;
        
        uint8_t value[4] = { 0, 0, 0, 0 };
        uint8_t reg1[2] = { 0x03, 0x1C };
        uint8_t reg2[2] = { 0x03, 0x12 };
        i2c_slave_read(bus, addr, reg1, 2, value, 4);
        vTaskDelay(1);
        voltage = convert_ade_read(value);
        
        reg1[1] = 0x1A;
        
        if (pm_sensor_type == 3) {
            // Channel B
            reg1[1]++;  // 0x1B
            reg2[1]++;  // 0x13
        }
        
        i2c_slave_read(bus, addr, reg1, 2, value, 4);
        vTaskDelay(1);
        current = convert_ade_read(value);
        
        if (current >= 2000) {
            i2c_slave_read(bus, addr, reg2, 2, value, 4);
            vTaskDelay(1);
            power = convert_ade_read(value);
        } else {
            get_data = false;
            current = 0;
        }
        
    } else {
        voltage = adv_hlw_get_voltage_freq((uint8_t) PM_SENSOR_HLW_GPIO);
        current = adv_hlw_get_current_freq((uint8_t) PM_SENSOR_HLW_GPIO);
        power = adv_hlw_get_power_freq((uint8_t) PM_SENSOR_HLW_GPIO);
    }
    
    voltage = (PM_VOLTAGE_FACTOR * voltage) + PM_VOLTAGE_OFFSET;
    
    if (voltage < 0) {
        voltage = 0;
    }
    
    if (get_data) {
        current = (PM_CURRENT_FACTOR * current) + PM_CURRENT_OFFSET;
    }
    
    if (pm_sensor_type < 2 && PM_SENSOR_HLW_GPIO_CF == PM_SENSOR_DATA_DEFAULT) {
        power = voltage * current;
    }
    
    if (get_data) {
        power = (PM_POWER_FACTOR * power) + PM_POWER_OFFSET;
    }
    
#ifdef POWER_MONITOR_DEBUG
    voltage = (hwrand() % 40) + 200;
    current = (hwrand() % 100) / 10.0f;
    power = hwrand() % 70;
#endif //POWER_MONITOR_DEBUG
    
    INFO("<%i> PM: V = %g, C = %g, P = %g", ch_group->accessory, voltage, current, power);
    
    if (pm_sensor_type < 2) {
        if (current < 0) {
            current = 0;
        }
        
        if (power < 0) {
            power = 0;
        }
    }
    
    PM_LAST_SAVED_CONSUPTION += PM_POLL_PERIOD;
    
    if (voltage < 600.f &&
        current < 200.f &&
        power < 50000.f) {
        const float consumption = ch_group->ch[3]->value.float_value + ((power * PM_POLL_PERIOD) / 3600000.f);
        INFO("<%i> PM: KWh = %1.7g", ch_group->accessory, consumption);
        
        do_wildcard_actions(ch_group, 0, voltage);
        
        if (voltage != ch_group->ch[0]->value.float_value) {
            const int old_voltage = ch_group->ch[0]->value.float_value * 10;
            ch_group->ch[0]->value.float_value = voltage;
            
            if (old_voltage != (int) (voltage * 10)) {
                homekit_characteristic_notify_safe(ch_group->ch[0]);
            }
        }
        
        do_wildcard_actions(ch_group, 1, current);
        
        if (current != ch_group->ch[1]->value.float_value) {
            const int old_current = ch_group->ch[1]->value.float_value * 1000;
            ch_group->ch[1]->value.float_value = current;
            
            if (old_current != (int) (current * 1000)) {
                homekit_characteristic_notify_safe(ch_group->ch[1]);
            }
        }
        
        do_wildcard_actions(ch_group, 2, power);
            
        if (power != ch_group->ch[2]->value.float_value) {
            const int old_power = ch_group->ch[2]->value.float_value * 10;
            ch_group->ch[2]->value.float_value = power;
            
            if (old_power != (int) (power * 10)) {
                homekit_characteristic_notify_safe(ch_group->ch[2]);
            }
        }
        
        do_wildcard_actions(ch_group, 3, consumption);
        
        if (consumption != ch_group->ch[3]->value.float_value) {
            const int old_consumption = ch_group->ch[3]->value.float_value * 1000;
            ch_group->ch[3]->value.float_value = consumption;
            
            if (old_consumption != (int) (consumption * 1000)) {
                homekit_characteristic_notify_safe(ch_group->ch[3]);
            }
        }
        
        if (PM_LAST_SAVED_CONSUPTION > 3600) {
            PM_LAST_SAVED_CONSUPTION = 0;
            save_states_callback();
        }
    } else {
        ERROR("<%i> PM Read", ch_group->accessory);
    }
    
    vTaskDelete(NULL);
}

void power_monitor_timer_worker(TimerHandle_t xTimer) {
    if (!homekit_is_pairing()) {
        if (xTaskCreate(power_monitor_task, "pm", POWER_MONITOR_TASK_SIZE, (void*) pvTimerGetTimerID(xTimer), POWER_MONITOR_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("Creating power_monitor");
            homekit_remove_oldest_client();
        }
    } else {
        ERROR("pm_task: HK pairing");
    }
}

// --- WATER VALVE
void hkc_valve_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        if (ch->value.int_value != value.int_value) {
            led_blink(1);
            INFO("<%i> Setter VALVE", ch_group->accessory);
            
            ch->value.int_value = value.int_value;
            ch_group->ch[1]->value.int_value = value.int_value;
            
            homekit_characteristic_notify_safe(ch_group->ch[1]);
            
            if (ch->value.int_value == 1) {
                esp_timer_start(ch_group->timer2);
            } else {
                esp_timer_stop(ch_group->timer2);
            }
            
            setup_mode_toggle_upcount();
            save_states_callback();
            
            if (ch_group->chs > 2) {
                if (value.int_value == 0) {
                    ch_group->ch[3]->value.int_value = 0;
                    esp_timer_stop(ch_group->timer);
                } else {
                    ch_group->ch[3]->value = ch_group->ch[2]->value;
                    esp_timer_start(ch_group->timer);
                }
                
                homekit_characteristic_notify_safe(ch_group->ch[3]);
            }
            
            save_historical_data(ch);
            
            do_actions(ch_group, (uint8_t) ch->value.int_value);
        }
    }
    
    homekit_characteristic_notify_safe(ch);
}

void hkc_valve_status_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    if (ch->value.int_value != value.int_value) {
        led_blink(1);
        
        ch->value.int_value = value.int_value;
        ch_group_t* ch_group = ch_group_find(ch);
        ch_group->ch[1]->value.int_value = value.int_value;
        
        INFO("<%i> Setter Status VALVE", ch_group->accessory);
        
        homekit_characteristic_notify_safe(ch_group->ch[1]);
        homekit_characteristic_notify_safe(ch);
    }
}

void valve_timer_worker(TimerHandle_t xTimer) {
    ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
    
    ch_group->ch[3]->value.int_value--;
    
    if (ch_group->ch[3]->value.int_value == 0) {
        esp_timer_stop(ch_group->timer);
        
        hkc_valve_setter(ch_group->ch[0], HOMEKIT_UINT8(0));
    }
}

// --- IAIRZONING
void set_zones_task(void* args) {
    ch_group_t* iairzoning_group = args;
    
    INFO("<%i> iAirZoning eval", iairzoning_group->accessory);

    uint8_t iairzoning_final_main_mode = IAIRZONING_LAST_ACTION;
    
    // Fix impossible cases
    ch_group_t* ch_group = main_config.ch_groups;
    while (ch_group) {
        if (ch_group->acc_type == ACC_TYPE_THERMOSTAT && iairzoning_group->accessory == (uint16_t) TH_IAIRZONING_CONTROLLER) {
            switch ((uint8_t) THERMOSTAT_CURRENT_ACTION) {
                case THERMOSTAT_ACTION_HEATER_ON:
                case THERMOSTAT_ACTION_HEATER_IDLE:
                case THERMOSTAT_ACTION_HEATER_FORCE_IDLE:
                case THERMOSTAT_ACTION_HEATER_SOFT_ON:
                    if (IAIRZONING_MAIN_MODE == THERMOSTAT_MODE_COOLER) {
                        THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_TOTAL_OFF;
                        hkc_setter(ch_group->ch[2], HOMEKIT_UINT8(THERMOSTAT_MODE_OFF));
                    } else {
                        IAIRZONING_MAIN_MODE = THERMOSTAT_MODE_HEATER;
                    }
                    break;
                    
                case THERMOSTAT_ACTION_COOLER_ON:
                case THERMOSTAT_ACTION_COOLER_IDLE:
                case THERMOSTAT_ACTION_COOLER_FORCE_IDLE:
                case THERMOSTAT_ACTION_COOLER_SOFT_ON:
                    if (IAIRZONING_MAIN_MODE == THERMOSTAT_MODE_HEATER) {
                        THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_TOTAL_OFF;
                        hkc_setter(ch_group->ch[2], HOMEKIT_UINT8(THERMOSTAT_MODE_OFF));
                    } else {
                        IAIRZONING_MAIN_MODE = THERMOSTAT_MODE_COOLER;
                    }
                    break;
                    
                default:    // THERMOSTAT_OFF
                    // Do nothing
                    break;
            }
        }

        ch_group = ch_group->next;
    }
    
    bool thermostat_all_off = true;
    bool thermostat_all_idle = true;
    bool thermostat_all_soft_on = true;
    bool thermostat_force_idle = false;
    
    ch_group = main_config.ch_groups;
    while (ch_group && thermostat_all_idle) {
        if (ch_group->acc_type == ACC_TYPE_THERMOSTAT && iairzoning_group->accessory == (uint16_t) TH_IAIRZONING_CONTROLLER) {
            if (THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_TOTAL_OFF) {
                thermostat_all_off = false;
                
                if (THERMOSTAT_CURRENT_ACTION == THERMOSTAT_ACTION_HEATER_ON ||
                    THERMOSTAT_CURRENT_ACTION == THERMOSTAT_ACTION_COOLER_ON) {
                    thermostat_all_idle = false;
                    thermostat_all_soft_on = false;
                    
                } else if (THERMOSTAT_CURRENT_ACTION == THERMOSTAT_ACTION_HEATER_SOFT_ON ||
                           THERMOSTAT_CURRENT_ACTION == THERMOSTAT_ACTION_COOLER_SOFT_ON) {
                    thermostat_all_idle = false;

                } else if (THERMOSTAT_CURRENT_ACTION == THERMOSTAT_ACTION_HEATER_FORCE_IDLE ||
                           THERMOSTAT_CURRENT_ACTION == THERMOSTAT_ACTION_COOLER_FORCE_IDLE) {
                    thermostat_force_idle = true;
                    
                }
            }
        }
        
        ch_group = ch_group->next;
    }
    
    INFO("<%i> iAirZoning All OFF: %i, all IDLE: %i, all soft ON: %i, force IDLE: %i",
         iairzoning_group->accessory,
         thermostat_all_off,
         thermostat_all_idle,
         thermostat_all_soft_on,
         thermostat_force_idle);
    
    if (thermostat_all_off) {
        if (IAIRZONING_MAIN_MODE != THERMOSTAT_MODE_OFF) {
            IAIRZONING_MAIN_MODE = THERMOSTAT_MODE_OFF;
            iairzoning_final_main_mode = THERMOSTAT_ACTION_TOTAL_OFF;
            
            // Open all gates
            ch_group = main_config.ch_groups;
            while (ch_group) {
                if (ch_group->acc_type == ACC_TYPE_THERMOSTAT && iairzoning_group->accessory == (uint16_t) TH_IAIRZONING_CONTROLLER) {
                    if ((uint8_t) TH_IAIRZONING_GATE_CURRENT_STATE != TH_IAIRZONING_GATE_OPEN) {
                        TH_IAIRZONING_GATE_CURRENT_STATE = TH_IAIRZONING_GATE_OPEN;
                        do_actions(ch_group, THERMOSTAT_ACTION_GATE_OPEN);
                        vTaskDelay(IAIRZONING_DELAY_ACTION);
                    }
                }

                ch_group = ch_group->next;
            }
        }
        
    } else {
        if (thermostat_all_idle) {
            if (IAIRZONING_MAIN_MODE == THERMOSTAT_MODE_HEATER) {
                if (thermostat_force_idle) {
                    iairzoning_final_main_mode = THERMOSTAT_ACTION_HEATER_FORCE_IDLE;
                } else {
                    iairzoning_final_main_mode = THERMOSTAT_ACTION_HEATER_IDLE;
                }
            } else {
                if (thermostat_force_idle) {
                    iairzoning_final_main_mode = THERMOSTAT_ACTION_COOLER_FORCE_IDLE;
                } else {
                    iairzoning_final_main_mode = THERMOSTAT_ACTION_COOLER_IDLE;
                }
            }
        } else if (thermostat_all_soft_on) {
            if (IAIRZONING_MAIN_MODE == THERMOSTAT_MODE_HEATER) {
                iairzoning_final_main_mode = THERMOSTAT_ACTION_HEATER_SOFT_ON;
            } else {
                iairzoning_final_main_mode = THERMOSTAT_ACTION_COOLER_SOFT_ON;
            }
        } else {
            if (IAIRZONING_MAIN_MODE == THERMOSTAT_MODE_HEATER) {
                iairzoning_final_main_mode = THERMOSTAT_ACTION_HEATER_ON;
            } else {
                iairzoning_final_main_mode = THERMOSTAT_ACTION_COOLER_ON;
            }
        }
        
        ch_group = main_config.ch_groups;
        while (ch_group) {
            if (ch_group->acc_type == ACC_TYPE_THERMOSTAT && iairzoning_group->accessory == (uint16_t) TH_IAIRZONING_CONTROLLER) {
                switch ((uint8_t) THERMOSTAT_CURRENT_ACTION) {
                    case THERMOSTAT_ACTION_HEATER_ON:
                    case THERMOSTAT_ACTION_COOLER_ON:
                    case THERMOSTAT_ACTION_HEATER_SOFT_ON:
                    case THERMOSTAT_ACTION_COOLER_SOFT_ON:
                        if ((uint8_t) TH_IAIRZONING_GATE_CURRENT_STATE != TH_IAIRZONING_GATE_OPEN) {
                            TH_IAIRZONING_GATE_CURRENT_STATE = TH_IAIRZONING_GATE_OPEN;
                            do_actions(ch_group, THERMOSTAT_ACTION_GATE_OPEN);
                            vTaskDelay(IAIRZONING_DELAY_ACTION);
                        }
                        break;
                        
                    case THERMOSTAT_ACTION_HEATER_IDLE:
                    case THERMOSTAT_ACTION_COOLER_IDLE:
                    case THERMOSTAT_ACTION_HEATER_FORCE_IDLE:
                    case THERMOSTAT_ACTION_COOLER_FORCE_IDLE:
                        if (thermostat_all_idle) {
                            if ((uint8_t) TH_IAIRZONING_GATE_CURRENT_STATE != TH_IAIRZONING_GATE_OPEN) {
                                TH_IAIRZONING_GATE_CURRENT_STATE = TH_IAIRZONING_GATE_OPEN;
                                do_actions(ch_group, THERMOSTAT_ACTION_GATE_OPEN);
                                vTaskDelay(IAIRZONING_DELAY_ACTION);
                            }
                        } else {
                            if ((uint8_t) TH_IAIRZONING_GATE_CURRENT_STATE != TH_IAIRZONING_GATE_CLOSE) {
                                TH_IAIRZONING_GATE_CURRENT_STATE = TH_IAIRZONING_GATE_CLOSE;
                                do_actions(ch_group, THERMOSTAT_ACTION_GATE_CLOSE);
                                vTaskDelay(IAIRZONING_DELAY_ACTION);
                            }
                        }
                        break;
                        
                    default:    // THERMOSTAT_OFF
                        if ((uint8_t) TH_IAIRZONING_GATE_CURRENT_STATE != TH_IAIRZONING_GATE_CLOSE) {
                            TH_IAIRZONING_GATE_CURRENT_STATE = TH_IAIRZONING_GATE_CLOSE;
                            do_actions(ch_group, THERMOSTAT_ACTION_GATE_CLOSE);
                            vTaskDelay(IAIRZONING_DELAY_ACTION);
                        }
                        break;
                }
            }
            
            ch_group = ch_group->next;
        }
    }
    
    if (iairzoning_final_main_mode != IAIRZONING_LAST_ACTION) {
        INFO("<%i> iAirZoning set mode %i", iairzoning_group->accessory, iairzoning_final_main_mode);
        IAIRZONING_LAST_ACTION = iairzoning_final_main_mode;
        do_actions(iairzoning_group, iairzoning_final_main_mode);
    }
    
    vTaskDelete(NULL);
}

void set_zones_timer_worker(TimerHandle_t xTimer) {
    if (xTaskCreate(set_zones_task, "zones", SET_ZONES_TASK_SIZE, (void*) pvTimerGetTimerID(xTimer), SET_ZONES_TASK_PRIORITY, NULL) != pdPASS) {
        ERROR("Creating set_zones");
        homekit_remove_oldest_client();
        esp_timer_start(xTimer);
    }
}

// --- THERMOSTAT
void process_th_task(void* args) {
    ch_group_t* ch_group = args;
    
    INFO("<%i> TH Process", ch_group->accessory);
    
    bool mode_has_changed = false;
    
    void heating(const float deadband, const float deadband_soft_on, const float deadband_force_idle) {
        INFO("<%i> Heating", ch_group->accessory);
        if (SENSOR_TEMPERATURE_FLOAT < (TH_HEATER_TARGET_TEMP_FLOAT - deadband - deadband_soft_on)) {
            TH_MODE_INT = THERMOSTAT_MODE_HEATER;
            if (THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_HEATER_ON) {
                THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_HEATER_ON;
                do_actions(ch_group, THERMOSTAT_ACTION_HEATER_ON);
                mode_has_changed = true;
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT < (TH_HEATER_TARGET_TEMP_FLOAT - deadband)) {
            TH_MODE_INT = THERMOSTAT_MODE_HEATER;
            if (THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_HEATER_SOFT_ON) {
                THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_HEATER_SOFT_ON;
                do_actions(ch_group, THERMOSTAT_ACTION_HEATER_SOFT_ON);
                mode_has_changed = true;
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT < (TH_HEATER_TARGET_TEMP_FLOAT + deadband)) {
            if (TH_MODE_INT == THERMOSTAT_MODE_HEATER) {
                if (TH_DEADBAND_SOFT_ON > 0.000f) {
                    if (THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_HEATER_SOFT_ON) {
                        THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_HEATER_SOFT_ON;
                        do_actions(ch_group, THERMOSTAT_ACTION_HEATER_SOFT_ON);
                        mode_has_changed = true;
                    }
                } else {
                    if (THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_HEATER_ON) {
                        THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_HEATER_ON;
                        do_actions(ch_group, THERMOSTAT_ACTION_HEATER_ON);
                        mode_has_changed = true;
                    }
                }
                
            } else {
                TH_MODE_INT = THERMOSTAT_MODE_IDLE;
                if (THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_HEATER_IDLE) {
                    THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_HEATER_IDLE;
                    do_actions(ch_group, THERMOSTAT_ACTION_HEATER_IDLE);
                    mode_has_changed = true;
                }
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT >= (TH_HEATER_TARGET_TEMP_FLOAT + deadband + deadband_force_idle) &&
                   TH_DEADBAND_FORCE_IDLE > 0.000f) {
            TH_MODE_INT = THERMOSTAT_MODE_IDLE;
            if (THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_HEATER_FORCE_IDLE) {
                THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_HEATER_FORCE_IDLE;
                do_actions(ch_group, THERMOSTAT_ACTION_HEATER_FORCE_IDLE);
                mode_has_changed = true;
            }
            
        } else {
            TH_MODE_INT = THERMOSTAT_MODE_IDLE;
            if (TH_DEADBAND_FORCE_IDLE == 0.000f ||
                THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_HEATER_FORCE_IDLE) {
                if (THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_HEATER_IDLE) {
                    THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_HEATER_IDLE;
                    do_actions(ch_group, THERMOSTAT_ACTION_HEATER_IDLE);
                    mode_has_changed = true;
                }
            }
        }
    }
    
    void cooling(const float deadband, const float deadband_soft_on, const float deadband_force_idle) {
        INFO("<%i> Cooling", ch_group->accessory);
        if (SENSOR_TEMPERATURE_FLOAT > (TH_COOLER_TARGET_TEMP_FLOAT + deadband + deadband_soft_on)) {
            TH_MODE_INT = THERMOSTAT_MODE_COOLER;
            if (THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_COOLER_ON) {
                THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_COOLER_ON;
                do_actions(ch_group, THERMOSTAT_ACTION_COOLER_ON);
                mode_has_changed = true;
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT > (TH_COOLER_TARGET_TEMP_FLOAT + deadband)) {
            TH_MODE_INT = THERMOSTAT_MODE_COOLER;
            if (THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_COOLER_SOFT_ON) {
                THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_COOLER_SOFT_ON;
                do_actions(ch_group, THERMOSTAT_ACTION_COOLER_SOFT_ON);
                mode_has_changed = true;
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT > (TH_COOLER_TARGET_TEMP_FLOAT - deadband)) {
            if (TH_MODE_INT == THERMOSTAT_MODE_COOLER) {
                if (TH_DEADBAND_SOFT_ON > 0.000f) {
                    if (THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_COOLER_SOFT_ON) {
                        THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_COOLER_SOFT_ON;
                        do_actions(ch_group, THERMOSTAT_ACTION_COOLER_SOFT_ON);
                        mode_has_changed = true;
                    }
                } else {
                    if (THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_COOLER_ON) {
                        THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_COOLER_ON;
                        do_actions(ch_group, THERMOSTAT_ACTION_COOLER_ON);
                        mode_has_changed = true;
                    }
                }
                
            } else {
                TH_MODE_INT = THERMOSTAT_MODE_IDLE;
                if (THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_COOLER_IDLE) {
                    THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_COOLER_IDLE;
                    do_actions(ch_group, THERMOSTAT_ACTION_COOLER_IDLE);
                    mode_has_changed = true;
                }
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT <= (TH_COOLER_TARGET_TEMP_FLOAT - deadband - deadband_force_idle) &&
                   TH_DEADBAND_FORCE_IDLE > 0.000f) {
            TH_MODE_INT = THERMOSTAT_MODE_IDLE;
            if (THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_COOLER_FORCE_IDLE) {
                THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_COOLER_FORCE_IDLE;
                do_actions(ch_group, THERMOSTAT_ACTION_COOLER_FORCE_IDLE);
                mode_has_changed = true;
            }
            
        } else {
            TH_MODE_INT = THERMOSTAT_MODE_IDLE;
            if (TH_DEADBAND_FORCE_IDLE == 0.000f ||
                THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_COOLER_FORCE_IDLE) {
                if (THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_COOLER_IDLE) {
                    THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_COOLER_IDLE;
                    do_actions(ch_group, THERMOSTAT_ACTION_COOLER_IDLE);
                    mode_has_changed = true;
                }
            }
        }
    }
    
    if (TH_ACTIVE_INT) {
        if (TH_TARGET_MODE_INT == THERMOSTAT_TARGET_MODE_HEATER) {
            heating(TH_DEADBAND, TH_DEADBAND_SOFT_ON, TH_DEADBAND_FORCE_IDLE);
            homekit_characteristic_notify_safe(ch_group->ch[5]);
                    
        } else if (TH_TARGET_MODE_INT == THERMOSTAT_TARGET_MODE_COOLER) {
            cooling(TH_DEADBAND, TH_DEADBAND_SOFT_ON, TH_DEADBAND_FORCE_IDLE);
            homekit_characteristic_notify_safe(ch_group->ch[6]);
            
        } else {    // THERMOSTAT_TARGET_MODE_AUTO
            const float mid_target = (TH_HEATER_TARGET_TEMP_FLOAT + TH_COOLER_TARGET_TEMP_FLOAT) / 2.000f;
            
            bool is_heater = false;
            if (TH_MODE_INT == THERMOSTAT_MODE_OFF) {
                if (SENSOR_TEMPERATURE_FLOAT < mid_target) {
                    is_heater = true;
                }
            } else if (SENSOR_TEMPERATURE_FLOAT <= TH_HEATER_TARGET_TEMP_FLOAT) {
                is_heater = true;
            } else if (SENSOR_TEMPERATURE_FLOAT < (TH_COOLER_TARGET_TEMP_FLOAT + 1.5) &&
                       (THERMOSTAT_CURRENT_ACTION == THERMOSTAT_ACTION_HEATER_IDLE ||
                        THERMOSTAT_CURRENT_ACTION == THERMOSTAT_ACTION_HEATER_ON ||
                        THERMOSTAT_CURRENT_ACTION == THERMOSTAT_ACTION_HEATER_FORCE_IDLE ||
                        THERMOSTAT_CURRENT_ACTION == THERMOSTAT_ACTION_HEATER_SOFT_ON)) {
                is_heater = true;
            }
            
            const float deadband_force_idle = TH_COOLER_TARGET_TEMP_FLOAT - mid_target;
            const float deadband = deadband_force_idle / 1.500f;
            
            if (is_heater) {
                heating(deadband, deadband_force_idle - deadband, deadband_force_idle);
            } else {
                cooling(deadband, deadband_force_idle - deadband, deadband_force_idle);
            }
            
            homekit_characteristic_notify_safe(ch_group->ch[5]);
            homekit_characteristic_notify_safe(ch_group->ch[6]);
        }
        
    } else {
        INFO("<%i> Off", ch_group->accessory);
        TH_MODE_INT = THERMOSTAT_MODE_OFF;
        if (THERMOSTAT_CURRENT_ACTION != THERMOSTAT_ACTION_TOTAL_OFF) {
            THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_TOTAL_OFF;
            do_actions(ch_group, THERMOSTAT_ACTION_TOTAL_OFF);
            mode_has_changed = true;
        }
    }
    
    if (TH_ACTIVE_INT) {
        if (TH_TARGET_MODE_INT == THERMOSTAT_TARGET_MODE_HEATER ||
            TH_TARGET_MODE_INT == THERMOSTAT_TARGET_MODE_AUTO) {
            do_wildcard_actions(ch_group, 2, TH_HEATER_TARGET_TEMP_FLOAT);
        }
        if (TH_TARGET_MODE_INT == THERMOSTAT_TARGET_MODE_COOLER ||
            TH_TARGET_MODE_INT == THERMOSTAT_TARGET_MODE_AUTO) {
            do_wildcard_actions(ch_group, 3, TH_COOLER_TARGET_TEMP_FLOAT);
        }
    } else {
        for (uint8_t i = 2; i <= 3; i++) {
            ch_group->last_wildcard_action[i] = NO_LAST_WILDCARD_ACTION;
        }
    }
    
    homekit_characteristic_notify_safe(ch_group->ch[2]);
    homekit_characteristic_notify_safe(ch_group->ch[3]);
    homekit_characteristic_notify_safe(ch_group->ch[4]);
    
    if (TH_IAIRZONING_CONTROLLER > 0 && mode_has_changed) {
        esp_timer_start(ch_group_find_by_acc(TH_IAIRZONING_CONTROLLER)->timer2);
    }
    
    save_states_callback();
    
    save_historical_data(ch_group->ch[3]);
    
    vTaskDelete(NULL);
}

void process_th_timer(TimerHandle_t xTimer) {
    if (xTaskCreate(process_th_task, "th", PROCESS_TH_TASK_SIZE, (void*) pvTimerGetTimerID(xTimer), PROCESS_TH_TASK_PRIORITY, NULL) != pdPASS) {
        ERROR("Creating process_th");
        homekit_remove_oldest_client();
        esp_timer_start(xTimer);
    }
}

void update_th(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("<%i> Setter TH", ch_group->accessory);
        
        ch->value = value;
        
        if (ch == ch_group->ch[2]) {
            homekit_characteristic_notify_safe(ch);
        }
        
        esp_timer_start(ch_group->timer2);
        
        if (ch != ch_group->ch[0]) {
            save_historical_data(ch);
        }

    } else {
        homekit_characteristic_notify_safe(ch);
    }
}

void hkc_th_target_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    setup_mode_toggle_upcount();
    
    update_th(ch, value);
}

void th_input(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;

    if (ch_group->child_enabled) {
        switch (type) {
            case 0:
                TH_ACTIVE_INT = 0;
                break;
                
            case 1:
                TH_ACTIVE_INT = 1;
                break;
                
            case 5:
                TH_ACTIVE_INT = 1;
                TH_TARGET_MODE_INT = THERMOSTAT_TARGET_MODE_COOLER;
                break;
                
            case 6:
                TH_ACTIVE_INT = 1;
                TH_TARGET_MODE_INT = THERMOSTAT_TARGET_MODE_HEATER;
                break;
                
            case 7:
                TH_ACTIVE_INT = 1;
                TH_TARGET_MODE_INT = THERMOSTAT_TARGET_MODE_AUTO;
                break;
                
            default:    // case 9:  // Cyclic
                if (TH_ACTIVE_INT) {
                    if (TH_TYPE >= THERMOSTAT_TYPE_HEATERCOOLER) {
                        if ((TH_TARGET_MODE_INT > THERMOSTAT_TARGET_MODE_AUTO && TH_TYPE == THERMOSTAT_TYPE_HEATERCOOLER) ||
                            (TH_TARGET_MODE_INT > THERMOSTAT_TARGET_MODE_COOLER && TH_TYPE == THERMOSTAT_TYPE_HEATERCOOLER_NOAUTO)) {
                            TH_TARGET_MODE_INT--;
                        } else {
                            TH_ACTIVE_INT = 0;
                        }
                    } else {
                        TH_ACTIVE_INT = 0;
                    }
                } else {
                    TH_ACTIVE_INT = 1;
                    if (TH_TYPE == THERMOSTAT_TYPE_HEATERCOOLER) {
                        TH_TARGET_MODE_INT = THERMOSTAT_TARGET_MODE_AUTO;
                    } else if (TH_TYPE == THERMOSTAT_TYPE_HEATERCOOLER_NOAUTO) {
                        TH_TARGET_MODE_INT = THERMOSTAT_TARGET_MODE_COOLER;
                    }
                }
                break;
        }
        
        hkc_th_target_setter(ch_group->ch[0], ch_group->ch[0]->value);
    }
}

void th_input_temp(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;

    if (ch_group->child_enabled) {
        if (TH_TYPE != THERMOSTAT_TYPE_COOLER) {
            float set_h_temp = TH_HEATER_TARGET_TEMP_FLOAT;
            
            if (type == THERMOSTAT_TEMP_UP) {
                set_h_temp += 0.5;
                if (set_h_temp > TH_HEATER_MAX_TEMP) {
                    set_h_temp = TH_HEATER_MAX_TEMP;
                }
            } else {    // type == THERMOSTAT_TEMP_DOWN
                set_h_temp -= 0.5;
                if (set_h_temp < TH_HEATER_MIN_TEMP) {
                    set_h_temp = TH_HEATER_MIN_TEMP;
                }
            }
            
            TH_HEATER_TARGET_TEMP_FLOAT = set_h_temp;
            homekit_characteristic_notify_safe(ch_group->ch[5]);
        }
        
        if (TH_TYPE != THERMOSTAT_TYPE_HEATER) {
            float set_c_temp = TH_COOLER_TARGET_TEMP_FLOAT;
            
            if (type == THERMOSTAT_TEMP_UP) {
                set_c_temp += 0.5;
                if (set_c_temp > TH_COOLER_MAX_TEMP) {
                    set_c_temp = TH_COOLER_MAX_TEMP;
                }
            } else {    // type == THERMOSTAT_TEMP_DOWN
                set_c_temp -= 0.5;
                if (set_c_temp < TH_COOLER_MIN_TEMP) {
                    set_c_temp = TH_COOLER_MIN_TEMP;
                }
            }
            
            TH_COOLER_TARGET_TEMP_FLOAT = set_c_temp;
            homekit_characteristic_notify_safe(ch_group->ch[6]);
        }
        
        update_th(ch_group->ch[0], ch_group->ch[0]->value);
        
        // Extra actions
        if (type == THERMOSTAT_TEMP_UP) {
            do_actions(ch_group, THERMOSTAT_ACTION_TEMP_UP);
        } else {    // type == THERMOSTAT_TEMP_DOWN
            do_actions(ch_group, THERMOSTAT_ACTION_TEMP_DOWN);
        }
    }
}

// --- HUMIDIFIER
void process_hum_task(void* args) {
    ch_group_t* ch_group = args;
    
    INFO("<%i> Hum Process", ch_group->accessory);
    
    void hum(const float deadband, const float deadband_soft_on, const float deadband_force_idle) {
        INFO("<%i> Hum", ch_group->accessory);
        if (SENSOR_HUMIDITY_FLOAT < (HM_HUM_TARGET_FLOAT - deadband - deadband_soft_on)) {
            HM_MODE_INT = HUMIDIF_MODE_HUM;
            if (HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_HUM_ON) {
                HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_HUM_ON;
                do_actions(ch_group, HUMIDIF_ACTION_HUM_ON);
            }
            
        } else if (SENSOR_HUMIDITY_FLOAT < (HM_HUM_TARGET_FLOAT - deadband)) {
            HM_MODE_INT = HUMIDIF_MODE_HUM;
            if (HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_HUM_SOFT_ON) {
                HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_HUM_SOFT_ON;
                do_actions(ch_group, HUMIDIF_ACTION_HUM_SOFT_ON);
            }
            
        } else if (SENSOR_HUMIDITY_FLOAT < (HM_HUM_TARGET_FLOAT + deadband)) {
            if (HM_MODE_INT == HUMIDIF_MODE_HUM) {
                if (HM_DEADBAND_SOFT_ON > 0.000f) {
                    if (HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_HUM_SOFT_ON) {
                        HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_HUM_SOFT_ON;
                        do_actions(ch_group, HUMIDIF_ACTION_HUM_SOFT_ON);
                    }
                } else {
                    if (HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_HUM_ON) {
                        HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_HUM_ON;
                        do_actions(ch_group, HUMIDIF_ACTION_HUM_ON);
                    }
                }
                
            } else {
                HM_MODE_INT = HUMIDIF_MODE_IDLE;
                if (HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_HUM_IDLE) {
                    HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_HUM_IDLE;
                    do_actions(ch_group, HUMIDIF_ACTION_HUM_IDLE);
                }
            }
            
        } else if ((SENSOR_HUMIDITY_FLOAT >= (HM_HUM_TARGET_FLOAT + deadband + deadband_force_idle) ||
                    SENSOR_HUMIDITY_FLOAT == 100.f ) &&
                   HM_DEADBAND_FORCE_IDLE > 0.000f) {
            HM_MODE_INT = HUMIDIF_MODE_IDLE;
            if (HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_HUM_FORCE_IDLE) {
                HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_HUM_FORCE_IDLE;
                do_actions(ch_group, HUMIDIF_ACTION_HUM_FORCE_IDLE);
            }
            
        } else {
            HM_MODE_INT = HUMIDIF_MODE_IDLE;
            if (HM_DEADBAND_FORCE_IDLE == 0.000f ||
                HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_HUM_FORCE_IDLE) {
                if (HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_HUM_IDLE) {
                    HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_HUM_IDLE;
                    do_actions(ch_group, HUMIDIF_ACTION_HUM_IDLE);
                }
            }
        }
    }
    
    void dehum(const float deadband, const float deadband_soft_on, const float deadband_force_idle) {
        INFO("<%i> Dehum", ch_group->accessory);
        if (SENSOR_HUMIDITY_FLOAT > (HM_DEHUM_TARGET_FLOAT + deadband + deadband_soft_on)) {
            HM_MODE_INT = HUMIDIF_MODE_DEHUM;
            if (HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_DEHUM_ON) {
                HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_DEHUM_ON;
                do_actions(ch_group, HUMIDIF_ACTION_DEHUM_ON);
            }
            
        } else if (SENSOR_HUMIDITY_FLOAT > (HM_DEHUM_TARGET_FLOAT + deadband)) {
            HM_MODE_INT = HUMIDIF_MODE_DEHUM;
            if (HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_DEHUM_SOFT_ON) {
                HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_DEHUM_SOFT_ON;
                do_actions(ch_group, HUMIDIF_ACTION_DEHUM_SOFT_ON);
            }
            
        } else if (SENSOR_HUMIDITY_FLOAT > (HM_DEHUM_TARGET_FLOAT - deadband)) {
            if (HM_MODE_INT == HUMIDIF_MODE_DEHUM) {
                if (HM_DEADBAND_SOFT_ON > 0.000f) {
                    if (HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_DEHUM_SOFT_ON) {
                        HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_DEHUM_SOFT_ON;
                        do_actions(ch_group, HUMIDIF_ACTION_DEHUM_SOFT_ON);
                    }
                } else {
                    if (HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_DEHUM_ON) {
                        HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_DEHUM_ON;
                        do_actions(ch_group, HUMIDIF_ACTION_DEHUM_ON);
                    }
                }
                
            } else {
                HM_MODE_INT = HUMIDIF_MODE_IDLE;
                if (HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_DEHUM_IDLE) {
                    HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_DEHUM_IDLE;
                    do_actions(ch_group, HUMIDIF_ACTION_DEHUM_IDLE);
                }
            }
            
        } else if ((SENSOR_HUMIDITY_FLOAT <= (HM_DEHUM_TARGET_FLOAT - deadband - deadband_force_idle) ||
                    SENSOR_HUMIDITY_FLOAT == 0.f) &&
                   HM_DEADBAND_FORCE_IDLE > 0.000f) {
            HM_MODE_INT = HUMIDIF_MODE_IDLE;
            if (HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_DEHUM_FORCE_IDLE) {
                HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_DEHUM_FORCE_IDLE;
                do_actions(ch_group, HUMIDIF_ACTION_DEHUM_FORCE_IDLE);
            }
            
        } else {
            HM_MODE_INT = HUMIDIF_MODE_IDLE;
            if (HM_DEADBAND_FORCE_IDLE == 0.000f ||
                HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_DEHUM_FORCE_IDLE) {
                if (HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_DEHUM_IDLE) {
                    HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_DEHUM_IDLE;
                    do_actions(ch_group, HUMIDIF_ACTION_DEHUM_IDLE);
                }
            }
        }
    }
    
    if (HM_ACTIVE_INT) {
        if (HM_TARGET_MODE_INT == HUMIDIF_TARGET_MODE_HUM) {
            hum(HM_DEADBAND, HM_DEADBAND_SOFT_ON, HM_DEADBAND_FORCE_IDLE);
            homekit_characteristic_notify_safe(ch_group->ch[5]);
                    
        } else if (HM_TARGET_MODE_INT == HUMIDIF_TARGET_MODE_DEHUM) {
            dehum(HM_DEADBAND, HM_DEADBAND_SOFT_ON, HM_DEADBAND_FORCE_IDLE);
            homekit_characteristic_notify_safe(ch_group->ch[6]);
            
        } else {    // HUMIDIF_TARGET_MODE_AUTO
            const float mid_target = (HM_HUM_TARGET_FLOAT + HM_DEHUM_TARGET_FLOAT) / 2.000f;
            
            bool is_hum = false;
            if (HM_MODE_INT == HUMIDIF_MODE_OFF) {
                if (SENSOR_HUMIDITY_FLOAT < mid_target) {
                    is_hum = true;
                }
            } else if (SENSOR_HUMIDITY_FLOAT <= HM_HUM_TARGET_FLOAT) {
                is_hum = true;
            } else if (SENSOR_HUMIDITY_FLOAT < (HM_DEHUM_TARGET_FLOAT + 8) &&
                       (HUMIDIF_CURRENT_ACTION == HUMIDIF_ACTION_HUM_IDLE ||
                        HUMIDIF_CURRENT_ACTION == HUMIDIF_ACTION_HUM_ON ||
                        HUMIDIF_CURRENT_ACTION == HUMIDIF_ACTION_HUM_FORCE_IDLE ||
                        HUMIDIF_CURRENT_ACTION == HUMIDIF_ACTION_HUM_SOFT_ON)) {
                is_hum = true;
            }
            
            const float deadband_force_idle = HUMIDIF_TARGET_MODE_HUM - mid_target;
            const float deadband = deadband_force_idle / 1.500f;
            
            if (is_hum) {
                hum(deadband, deadband_force_idle - deadband, deadband_force_idle);
            } else {
                dehum(deadband, deadband_force_idle - deadband, deadband_force_idle);
            }
            
            homekit_characteristic_notify_safe(ch_group->ch[5]);
            homekit_characteristic_notify_safe(ch_group->ch[6]);
        }
        
    } else {
        INFO("<%i> Off", ch_group->accessory);
        HM_MODE_INT = HUMIDIF_MODE_OFF;
        if (HUMIDIF_CURRENT_ACTION != HUMIDIF_ACTION_TOTAL_OFF) {
            HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_TOTAL_OFF;
            do_actions(ch_group, HUMIDIF_ACTION_TOTAL_OFF);
        }
    }
    
    if (HM_ACTIVE_INT) {
        if (HM_TARGET_MODE_INT == HUMIDIF_TARGET_MODE_HUM ||
            HM_TARGET_MODE_INT == HUMIDIF_TARGET_MODE_AUTO) {
            do_wildcard_actions(ch_group, 2, HM_HUM_TARGET_FLOAT);
        }
        if (HM_TARGET_MODE_INT == HUMIDIF_TARGET_MODE_DEHUM ||
            HM_TARGET_MODE_INT == HUMIDIF_TARGET_MODE_AUTO) {
            do_wildcard_actions(ch_group, 3, HM_DEHUM_TARGET_FLOAT);
        }
    } else {
        for (uint8_t i = 2; i <= 3; i++) {
            ch_group->last_wildcard_action[i] = NO_LAST_WILDCARD_ACTION;
        }
    }
    
    homekit_characteristic_notify_safe(ch_group->ch[2]);
    homekit_characteristic_notify_safe(ch_group->ch[3]);
    homekit_characteristic_notify_safe(ch_group->ch[4]);
    
    save_states_callback();
    
    save_historical_data(ch_group->ch[3]);
    
    vTaskDelete(NULL);
}

void process_humidif_timer(TimerHandle_t xTimer) {
    if (xTaskCreate(process_hum_task, "hum", PROCESS_HUMIDIF_TASK_SIZE, (void*) pvTimerGetTimerID(xTimer), PROCESS_HUMIDIF_TASK_PRIORITY, NULL) != pdPASS) {
        ERROR("Creating process_hum");
        homekit_remove_oldest_client();
        esp_timer_start(xTimer);
    }
}

void update_humidif(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("<%i> Setter Humidif", ch_group->accessory);
        
        ch->value = value;
        
        if (ch == ch_group->ch[2]) {
            homekit_characteristic_notify_safe(ch);
        }
        
        esp_timer_start(ch_group->timer2);

        if (ch != ch_group->ch[1]) {
            save_historical_data(ch);
        }
        
    } else {
        homekit_characteristic_notify_safe(ch);
    }
}

void hkc_humidif_target_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    setup_mode_toggle_upcount();
    
    update_humidif(ch, value);
}

void humidif_input(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;

    if (ch_group->child_enabled) {
        switch (type) {
            case 0:
                HM_ACTIVE_INT = 0;
                break;
                
            case 1:
                HM_ACTIVE_INT = 1;
                break;
                
            case 5:
                HM_ACTIVE_INT = 1;
                HM_TARGET_MODE_INT = HUMIDIF_TARGET_MODE_DEHUM;
                break;
                
            case 6:
                HM_ACTIVE_INT = 1;
                HM_TARGET_MODE_INT = HUMIDIF_TARGET_MODE_HUM;
                break;
                
            case 7:
                HM_ACTIVE_INT = 1;
                HM_TARGET_MODE_INT = HUMIDIF_TARGET_MODE_AUTO;
                break;
                
            default:    // case 9:  // Cyclic
                if (HM_ACTIVE_INT) {
                    if (HM_TYPE >= HUMIDIF_TYPE_HUMDEHUM) {
                        if ((HM_TARGET_MODE_INT > HUMIDIF_TARGET_MODE_AUTO && HM_TYPE == HUMIDIF_TYPE_HUMDEHUM) ||
                            (HM_TARGET_MODE_INT > HUMIDIF_TARGET_MODE_DEHUM && HM_TYPE == HUMIDIF_TYPE_HUMDEHUM_NOAUTO)) {
                            HM_TARGET_MODE_INT--;
                        } else {
                            HM_ACTIVE_INT = 0;
                        }
                    } else {
                        HM_ACTIVE_INT = 0;
                    }
                } else {
                    HM_ACTIVE_INT = 1;
                    if (HM_TYPE == HUMIDIF_TYPE_HUMDEHUM) {
                        HM_TARGET_MODE_INT = HUMIDIF_TARGET_MODE_DEHUM;
                    } else if (HM_TYPE == HUMIDIF_TYPE_HUMDEHUM_NOAUTO) {
                        HM_TARGET_MODE_INT = HUMIDIF_TARGET_MODE_DEHUM;
                    }
                }
                break;
        }
        
        hkc_humidif_target_setter(ch_group->ch[0], ch_group->ch[0]->value);
    }
}

void humidif_input_temp(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;

    if (ch_group->child_enabled) {
        if (HM_TYPE != HUMIDIF_TYPE_DEHUM) {
            int8_t set_h_temp = HM_HUM_TARGET_FLOAT;

            if (type == HUMIDIF_UP) {
                set_h_temp += 5;
                if (set_h_temp > 100) {
                    set_h_temp = 100;
                }
            } else {    // type == HUMIDIF_DOWN
                set_h_temp -= 5;
                if (set_h_temp < 0) {
                    set_h_temp = 0;
                }
            }
            
            HM_HUM_TARGET_FLOAT = set_h_temp;
            homekit_characteristic_notify_safe(ch_group->ch[5]);
        }
        
        if (HM_TYPE != HUMIDIF_TYPE_HUM) {
            int8_t set_c_temp = HM_DEHUM_TARGET_FLOAT;

            if (type == HUMIDIF_UP) {
                set_c_temp += 5;
                if (set_c_temp > 100) {
                    set_c_temp = 100;
                }
            } else {    // type == HUMIDIF_DOWN
                set_c_temp -= 5;
                if (set_c_temp < 0) {
                    set_c_temp = 0;
                }
            }
            
            HM_DEHUM_TARGET_FLOAT = set_c_temp;
            homekit_characteristic_notify_safe(ch_group->ch[6]);
        }
        
        update_humidif(ch_group->ch[1], ch_group->ch[1]->value);
        
        // Extra actions
        if (type == HUMIDIF_UP) {
            do_actions(ch_group, HUMIDIF_ACTION_UP);
        } else {    // type == HUMIDIF_DOWN
            do_actions(ch_group, HUMIDIF_ACTION_DOWN);
        }
    }
}

// --- TEMPERATURE
void temperature_task(void* args) {
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
    
    float temperature_value = 0.0;
    float humidity_value = 0.0;
    bool get_temp = false;
    uint8_t iairzoning = 0;
    
    if (ch_group->acc_type == ACC_TYPE_IAIRZONING) {
        INFO("<%i> iAirZoning sensors", ch_group->accessory);
        iairzoning = ch_group->accessory;
        ch_group = main_config.ch_groups;
    }
    
    while (ch_group) {
        get_temp = false;
        
        if (iairzoning == 0 || (ch_group->acc_type == ACC_TYPE_THERMOSTAT && iairzoning == (uint8_t) TH_IAIRZONING_CONTROLLER)) {
            INFO("<%i> TH sensor", ch_group->accessory);
            
            if (TH_SENSOR_TYPE != 3 && TH_SENSOR_TYPE < 5) {
                dht_sensor_type_t current_sensor_type = DHT_TYPE_DHT22; // TH_SENSOR_TYPE == 2
                
                if (TH_SENSOR_TYPE == 1) {
                    current_sensor_type = DHT_TYPE_DHT11;
                } else if (TH_SENSOR_TYPE == 4) {
                    current_sensor_type = DHT_TYPE_SI7021;
                }
                
                get_temp = dht_read_float_data(current_sensor_type, TH_SENSOR_GPIO, &humidity_value, &temperature_value);
                
            } else if (TH_SENSOR_TYPE == 3) {
                const uint8_t sensor_index = TH_SENSOR_INDEX;
                ds18b20_addr_t ds18b20_addrs[sensor_index];
                
                if (ds18b20_scan_devices(TH_SENSOR_GPIO, ds18b20_addrs, sensor_index) >= sensor_index) {
                    ds18b20_addr_t ds18b20_addr;
                    ds18b20_addr = ds18b20_addrs[sensor_index - 1];
                    ds18b20_measure_and_read_multi(TH_SENSOR_GPIO, &ds18b20_addr, 1, &temperature_value);
                    
                    if (temperature_value < 130.f && temperature_value > -60.f) {
                        get_temp = true;
                    }
                }
                
            } else {
                const float adc = sdk_system_adc_read();
                if (TH_SENSOR_TYPE == 5) {
                    // https://github.com/arendst/Tasmota/blob/7177c7d8e003bb420d8cae39f544c2b8a9af09fe/tasmota/xsns_02_analog.ino#L201
                    temperature_value = KELVIN_TO_CELSIUS(3350 / (3350 / 298.15 + taylor_log(((32000 * adc) / ((1024 * 3.3) - adc)) / 10000))) - 15;
                    
                } else if (TH_SENSOR_TYPE == 6) {
                    temperature_value = KELVIN_TO_CELSIUS(3350 / (3350 / 298.15 - taylor_log(((32000 * adc) / ((1024 * 3.3) - adc)) / 10000))) + 15;
                    
                } else if (TH_SENSOR_TYPE == 7) {
                    temperature_value = 1024 - adc;
                    
                } else if (TH_SENSOR_TYPE == 8) {
                    temperature_value = adc;
                    
                } else if (TH_SENSOR_TYPE == 9){
                    humidity_value = 1024 - adc;
                    
                } else {    // TH_SENSOR_TYPE == 10
                    humidity_value = adc;
                }
                
                if (TH_SENSOR_HUM_OFFSET != 0.000000f && TH_SENSOR_TYPE < 9) {
                    temperature_value *= TH_SENSOR_HUM_OFFSET;
                    
                } else if (TH_SENSOR_TEMP_OFFSET != 0.000000f && TH_SENSOR_TYPE >= 9) {
                    humidity_value *= TH_SENSOR_TEMP_OFFSET;
                }
                
                get_temp = true;
            }
            
            /*
             * Only for tests. Keep comment for releases
             */
            //get_temp = true; temperature_value = 23; humidity_value = 68;
            
            if (get_temp) {
                TH_SENSOR_ERROR_COUNT = 0;
                
                if (ch_group->chs > 0 && ch_group->ch[0]) {
                    temperature_value += TH_SENSOR_TEMP_OFFSET;
                    if (temperature_value < -100.f) {
                        temperature_value = -100.f;
                    } else if (temperature_value > 200.f) {
                        temperature_value = 200.f;
                    }
                    
                    temperature_value *= 10.f;
                    temperature_value = ((int) temperature_value) / 10.0f;
                    
                    INFO("<%i> TEMP %gC", ch_group->accessory, temperature_value);
                    
                    if (temperature_value != ch_group->ch[0]->value.float_value) {
                        ch_group->ch[0]->value.float_value = temperature_value;
                        homekit_characteristic_notify_safe(ch_group->ch[0]);
                        
                        if (ch_group->chs > 4) {
                            update_th(ch_group->ch[0], ch_group->ch[0]->value);
                        }
                        
                        do_wildcard_actions(ch_group, 0, temperature_value);
                    }
                }
                
                if (ch_group->chs > 1 && ch_group->ch[1]) {
                    humidity_value += TH_SENSOR_HUM_OFFSET;
                    if (humidity_value < 0) {
                        humidity_value = 0;
                    } else if (humidity_value > 100) {
                        humidity_value = 100;
                    }

                    INFO("<%i> HUM %i", ch_group->accessory, (uint8_t) humidity_value);
                    
                    if ((uint8_t) humidity_value != (uint8_t) ch_group->ch[1]->value.float_value) {
                        ch_group->ch[1]->value.float_value = (uint8_t) humidity_value;
                        homekit_characteristic_notify_safe(ch_group->ch[1]);
                        
                        if (ch_group->chs > 4) {
                            update_humidif(ch_group->ch[1], ch_group->ch[1]->value);
                        }
                        
                        do_wildcard_actions(ch_group, 1, (uint8_t) humidity_value);
                    }
                }
                
            } else {
                led_blink(5);
                ERROR("<%i> Sensor", ch_group->accessory);
                
                TH_SENSOR_ERROR_COUNT++;

                if ((uint8_t) TH_SENSOR_ERROR_COUNT > TH_SENSOR_MAX_ALLOWED_ERRORS) {
                    TH_SENSOR_ERROR_COUNT = 0;
                    
                    if (ch_group->chs > 0 && ch_group->ch[0]) {
                        ch_group->ch[0]->value.float_value = 0;
                        homekit_characteristic_notify_safe(ch_group->ch[0]);
                    }
                    if (ch_group->chs > 1 && ch_group->ch[1]) {
                        ch_group->ch[1]->value.float_value = 0;
                        homekit_characteristic_notify_safe(ch_group->ch[1]);
                    }

                    do_actions(ch_group, THERMOSTAT_ACTION_SENSOR_ERROR);
                }

            }
        }
        
        if (iairzoning > 0) {
            if (ch_group->acc_type == ACC_TYPE_THERMOSTAT && iairzoning == (uint8_t) TH_IAIRZONING_CONTROLLER) {
                vTaskDelay(MS_TO_TICKS(100));
            }

            ch_group = ch_group->next;
            
        } else {
            ch_group = NULL;
        }
    }
    
    vTaskDelete(NULL);
}

void temperature_timer_worker(TimerHandle_t xTimer) {
    if (!homekit_is_pairing()) {
        if (xTaskCreate(temperature_task, "temp", TEMPERATURE_TASK_SIZE, (void*) pvTimerGetTimerID(xTimer), TEMPERATURE_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("Creating temperature");
            homekit_remove_oldest_client();
        }
    } else {
        ERROR("temperature_task: HK pairing");
    }
}

// --- LIGHTBULBS
void hsi2rgbw(uint16_t h, float s, uint8_t v, ch_group_t* ch_group) {
    // **************************************************
    // * All credits and thanks to Kevin John Cutler    *
    // * https://github.com/kevinjohncutler/colormixing *
    // **************************************************
    
    // https://gist.github.com/rasod/42eab9206e28ca91c8d9f926fa71a938
    // https://gist.github.com/unteins/6ecb69883d55ad8424b70be405bf4115
    // Ths is the main color mixing function
    // Takes in HSV, assigns PWM duty cycle to the lightbulb_group struct
    // https://github.com/espressif/esp-idf/examples/peripherals/rmt/led_strip/main/led_strip_main.c
    // ref5: https://github.com/patdie421/mea-edomus/blob/0eb0f9a8630ce610e3d1f6dd3c3a8d29d2dffea6/src/interfaces/type_004/philipshue_color.c
    // Bruce Lindbloom's website http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html

    // Helper function to sum arrays
    inline float array_sum(float arr[]) {
        float sum = arr[0];
        for (uint8_t i = 1; i < 4; i++) {
            sum = sum + arr[i];
        }
        
        return sum;
    }

    // Helper function to find max of an array
    float array_max(float arr[], const int num_elements) {
        float max = arr[0];
        for (uint8_t i = 1; i < num_elements; i++) {
            if (arr[i] > max) {
                max = arr[i];
            }
        }
        
        return max;
    }

    // Helper function to find min of an array
    inline float array_min(float arr[]) {
        float min = arr[0];
        for (uint8_t i = 1; i < 3; i++) {
            if (arr[i] < min) {
                min = arr[i];
            }
        }
        
        return min;
    }

    // Helper function to perform dot product
    inline float array_dot(float arr1[], float arr2[]) {
        float sum = arr1[0] * arr2[0];
        for (uint8_t i = 1; i < 5; i++) {
            sum += arr1[i] * arr2[i];
        }
        
        return sum;
    }

    // Helper function to multiply array by a constant
    void array_multiply(float arr[], const float scalar, const int num_elements) {
        for (uint8_t i = 0; i < num_elements; i++) {
            arr[i] *= scalar;
        }
    }

    // Helper function to asign array componenets
    void array_equals(float arr[], float vals[], const int num_elements) {
        for (uint8_t i = 0; i < num_elements; i++) {
            arr[i] = vals[i];
        }
    }

    // Helper function to rescale array so that max is 1
    void array_rescale(float arr[], const int num_elements) {
        const float amax = array_max(arr, num_elements);
        if (amax != 0) {
            array_multiply(arr, 1.0f / amax, num_elements);
        } else {
            array_multiply(arr, 0, num_elements);
        }
    }

    // Helper function to calculate barycentric coordinates; now returns coordinates with the max=1 always
    void bary(float L[], float p0[], float p1[], float p2[], float p3[]) {
        const float denom = (p2[1] - p3[1]) * (p1[0] - p3[0]) + (p3[0] - p2[0]) * (p1[1] - p3[1]); // float vs const float?
        L[0] = ((p2[1] - p3[1]) * (p0[0] - p3[0]) + (p3[0] - p2[0]) * (p0[1] - p3[1])) / denom;
        L[1] = ((p3[1] - p1[1]) * (p0[0] - p3[0]) + (p1[0] - p3[0]) * (p0[1] - p3[1])) / denom;
        L[2] = 1 - (L[0] + L[1]);
        // Deal with numerical imprecision; may want to switch to doubles but let's try this instread
        // Snaps to 0 and 1 when close. Kinda makes sense to di this anyway considering that the PWM is finite
        for (uint8_t i = 0; i < 3; i++) {
            if (fabsf(L[i]) < 1E-4) {
                L[i] = 0;
            } else if (fabsf(L[i]-1) < 1E-4) {
                L[i] = 1;
            }
        }
    }

    // Get the color p0's barycentric coordinates based on where it is. Assumes p4 is within the triangle formed by p1,p2,p3.
    void getWeights(float coeffs[], float p0[], float p1[], float p2[], float p3[], float p4[]) {
        float L[3];
        bary(L, p0, p1, p2, p4); // Try red-green-W
        if ((L[0] >= 0)&&(L[1] >= 0)&&(L[2] >= 0)) {
            float vals[] = {L[0], L[1], 0, L[2]};
            array_equals(coeffs, vals, 4);
        } else {
            bary(L, p0, p2, p3, p4); // Try green-blue-W
            if ((L[0] >= 0)&&(L[1] >= 0)&&(L[2] >= 0)) {
                float vals[] = {0, L[0], L[1], L[2]};
                array_equals(coeffs, vals, 4);
            } else { // must be red-blue-W
                bary(L, p0, p1, p3, p4);
                float vals[] = {L[0], 0, L[1], L[2]};
                array_equals(coeffs, vals, 4);
            }
        }
        array_rescale(coeffs, 4);
    }

    // Multiply 3x3 square matrices together
    inline void matrix_matrix_multiply(float mat1[3][3], float mat2[3][3], float res[3][3]) {
        for (uint8_t i = 0; i < 3; i++) {
            for (uint8_t j = 0; j < 3; j++) {
                res[i][j] = 0;
                for (uint8_t k = 0; k < 3; k++)
                    res[i][j] += mat1[i][k] * mat2[k][j];
            }
        }
    }

    // Helper function to find intersection of lines defined by two pairs of points p1,p2 and p3,p4. Stores value in pointer p.
    void intersect(float p[], float p1[], float p2[], float p3[], float p4[]) {
        const float denom = (p1[0] - p2[0]) * (p3[1] - p4[1]) - (p1[1] - p2[1]) * (p3[0] - p4[0]);
        p[0] = ((p1[0] * p2[1] - p1[1] * p2[0]) * (p3[0] - p4[0]) - (p1[0] - p2[0]) * (p3[0] * p4[1] - p3[1] * p4[0])) / denom;
        p[1] = ((p1[0] * p2[1] - p1[1] * p2[0]) * (p3[1] - p4[1]) - (p1[1] - p2[1]) * (p3[0] * p4[1] - p3[1] * p4[0])) / denom;
    }

    // Helper function to compute the 2x2 matrix for sending 2 vectors to 2 other vectors.
    void pair_transform(float m[2][2], float p1[2], float p2[2], float p3[2], float p4[2]) {
        const float denom = p2[1] * p4[0] - p2[0] * p4[1];
        m[0][0] =  (p2[1] * p3[0] - p1[0] * p4[1]) / denom;
        m[0][1] = -(p2[0] * p3[0] - p1[0] * p4[0]) / denom;
        m[1][0] =  (p2[1] * p3[1] - p1[1] * p4[1]) / denom;
        m[1][1] = -(p2[0] * p3[1] - p1[1] * p4[0]) / denom;
    }

    void gamut_transform(float p[2], float m[2][2], float shift[2]) {
        // Shift the point so that whitepoint is at origin
        float pp[2];
        pp[0] = p[0] - shift[0];
        pp[1] = p[1] - shift[1];
        
        // Apply the matrix
        p[0] = m[0][0]*pp[0]+m[0][1]*pp[1];
        p[1] = m[1][0]*pp[0]+m[1][1]*pp[1];
        
        // shift back
        p[0] = p[0] + shift[0];
        p[1] = p[1] + shift[1];
    }

    // helper dunction to do p = a + b
    void array_subtract(float p[2], float a[2], float b[2]) {
        for (uint8_t i = 0; i < 2; i++) {
            p[i] = a[i] - b[i];
        }
    }
    
    // *** HSI TO RGBW FUNCTION ***
#ifdef LIGHT_DEBUG
    uint32_t run_time = sdk_system_get_time();
#endif
    
    lightbulb_group_t* lightbulb_group = lightbulb_group_find(ch_group->ch[0]);

    h %= 360; // shorthand modulo arithmetic, h = h%360, so that h is rescaled to the range [0,360) (angle around hue circle)
    
    const uint32_t rgb_max = 1; // Ignore brightness for initial conversion
    const float rgb_min = rgb_max * (100 - s) / 100.f; // Again rescaling 100->1, backing off from max

    const uint32_t i = h / 60; // which 1/6th of the hue circle you are in
    const uint32_t diff = h % 60; // remainder (how far counterclockwise into that 60 degree sector)

    const float rgb_adj = (rgb_max - rgb_min) * diff / 60; // radius*angle = arc length

    float wheel_rgb[3]; // declare variables
    
    // Different rules depending on the sector
    // I think it is something like approximating the RGB cube for each sector
    // Indeed, six sectors for six faces of the cube.
    // INFO("light switch %i", i);
    switch (i) {
        case 0:     // Red to yellow
            wheel_rgb[0] = rgb_max;
            wheel_rgb[1] = rgb_min + rgb_adj;
            wheel_rgb[2] = rgb_min;
            break;
            
        case 1:     // Yellow to green
            wheel_rgb[0] = rgb_max - rgb_adj;
            wheel_rgb[1] = rgb_max;
            wheel_rgb[2] = rgb_min;
            break;
            
        case 2:     // Green to cyan
            wheel_rgb[0] = rgb_min;
            wheel_rgb[1] = rgb_max;
            wheel_rgb[2] = rgb_min + rgb_adj;
            break;
            
        case 3:     // Cyan to blue
            wheel_rgb[0] = rgb_min;
            wheel_rgb[1] = rgb_max - rgb_adj;
            wheel_rgb[2] = rgb_max;
            break;
            
        case 4:     // Blue to magenta
            wheel_rgb[0] = rgb_min + rgb_adj;
            wheel_rgb[1] = rgb_min;
            wheel_rgb[2] = rgb_max;
            break;
            
        default:    // Magenta to red
            wheel_rgb[0] = rgb_max;
            wheel_rgb[1] = rgb_min;
            wheel_rgb[2] = rgb_max - rgb_adj;
            break;
    }
    // I SHOULD float-check that this is the correct conversion, but wanting logs will do as well
    
    L_DEBUG("RGB: [%g, %g, %g]", wheel_rgb[0], wheel_rgb[1], wheel_rgb[2]);
    
    // (2) convert to XYZ then to xy(ignore Y). Also now apply gamma correction.
    float gc[3];
    for (uint8_t i = 0; i < 3; i++) {
        gc[i] = (wheel_rgb[i] > 0.04045f) ? fast_precise_pow((wheel_rgb[i] + 0.055f) / (1.0f + 0.055f), 2.4f) : (wheel_rgb[i] / 12.92f);
    }
    
    // Get the xy coordinates using sRGB Primaries. This appears to be the space that HomeKit gives HSV commands in, however, we need to do some gamut streching.
    // This matrix should later be computed fromms scratch using the functions above
    const float denom = gc[2] * (1.0849816845413038 - 1.013932032965188 * WP[0] - 1.1309562993446372 * WP[1]) + gc[0] * (-0.07763613020327381 + 0.6290345979071447 * WP[0] + 0.28254416233165086 * WP[1]) + gc[1] * (-0.007345554338030566 + 0.38489743505804436 * WP[0] + 0.8484121370129867 * WP[1]);
    float p[2] = {
        (gc[2] * (0.14346747729293707 - 0.10973602163740542 * WP[0] - 0.15130242398432014 * WP[1]) + gc[1] * (-0.07488078606033685 + 0.6384263445494799 * WP[0] - 0.021643836986853505 * WP[1]) + gc[0] * (-0.06858669123260035 + 0.47130967708792587 * WP[0] + 0.17294626097117374 * WP[1])) / denom,
        (gc[2] * (0.044068179152577595 - 0.039763003918887874 * WP[0] - 0.023967881257084177 * WP[1]) + gc[0] * (-0.029315872239491187 + 0.18228806745026777 * WP[0] + 0.12851324243365222 * WP[1]) + gc[1] * (-0.014752306913086446 - 0.14252506353137964 * WP[0] + 0.8954546388234319 * WP[1])) / denom
    };
    L_DEBUG("Chromaticity: [%g, %g]", p[0], p[1]);
    
    // Instead of testing the barycentric coordinates, I can just test the slope against the trinagles that cut out the planckian locus
    // This slope is of the line connecting the point to the sRGB blue vertex
//    float sr[2] = {0.648438930511,0.330873042345}; // these ones from HAA
//    float sg[2] = {0.321112006903,0.597964227200};
//    float sb[2] = {0.155906423926,0.066072970629};
//    float slope = (p[1]-sb[1]) / (p[0]-sb[0]);
    float L[3];
    float planckR[2] = { 0.549644, 0.411449 };
    float planckG[2] = { 0.241671, 0.232935 };
    float planckB[2] = { 0.389596, 0.46404 };
    bary(L, p, planckR, planckG, planckB);
    
    L_DEBUG("Bary test: [%g, %g, %g]", L[0], L[1], L[2]);
    if ((L[0] < 0) || (L[1] < 0) || (L[2] < 0)) { // Outside the white triangle
        
        // Instead of the algo above to simply use different primaries (which stretches intermediate CMY way off), I came up with a new one. It checks for being in one of six triangles, and applies a unique transform for each to fill the gamut without disrupting CMY. By fixing one intermediate point, I suspect that the colors will be much more 'as expected'.
//        float cyan[2]    = {0.249488, 0.367277};
//        float magenta[2] = {0.364161, 0.178021};
//        float yellow[2]  = {0.438746, 0.501925};
//        float *CMY[3] = {cyan, magneta, yellow}; // array of pointers
        float CMY[3][2] = { { 0.249488, 0.367277 }, { 0.364161, 0.178021 }, { 0.438746, 0.501925 } };     // actual sCMY
        float RGB[3][2] = { { 0.648428, 0.330855 }, { 0.321142, 0.597873 }, { 0.155883, 0.0660408 } };    // actual sRGB
        float LED_CMY[3][2];
        float* LED_RGB[3] = { R, G, B };
        L_DEBUG("LED RGB: { {%g, %g}, {%g, %g}, {%g, %g} }", LED_RGB[0][0], LED_RGB[0][1], LED_RGB[1][0], LED_RGB[1][1], LED_RGB[2][0], LED_RGB[2][1]);

        for (uint8_t i = 0; i < 3; i++) {
            uint8_t j = ((i + 1) % 3);
            uint8_t k = ((i + 2) % 3); //had to adapt a bit for 0-based indexing
            float pp[2];
            intersect(pp, WP, *(myCMY + i), *(LED_RGB + j), *(LED_RGB + k)); // compute point long the line from white-C(MY) to RG(B) line.
            LED_CMY[i][0] = pp[0];
            LED_CMY[i][1] = pp[1]; // assign coordinates to the LED_CMY array
        }
        L_DEBUG("LED CMY: { {%g, %g}, {%g, %g}, {%g, %g} }", LED_CMY[0][0], LED_CMY[0][1], LED_CMY[1][0], LED_CMY[1][1], LED_CMY[2][0], LED_CMY[2][1]);

        // Go though each region. Most robust is barycentric check, albeing susecpible to numerical imprecision - at least there will be no issue with vertical lines. Ordered to have better time for red and magenta triangles (warm whites and reds).
        
        uint8_t i, j, k;
        float mat1[2][2];
        bary(L, p, *(CMY + 0), *(CMY + 1), *(CMY + 2)); // Reduce to inner CMY or outer RGB trinagle set
        if ((L[0] >= 0) && (L[1] >= 0) && (L[2] >= 0)) {
            bary(L, p, WP, *(CMY + 1), *(CMY + 2)); // Test magenta triangle
            if ((L[0] >= 0) && (L[1] >= 0) && (L[2] >= 0)) {
                i = 1;
                j = 2;
            } else {
                bary(L, p, WP, *(CMY + 0), *(CMY + 2)); // Test yellow triangle
                if ((L[0] >= 0) && (L[1] >= 0) && (L[2] >= 0)) {
                    i = 0;
                    j = 2;
                } else { // Must be in cyan
                    i = 0;
                    j = 1;
                }
            }
            // i and j correspond to which CMY primaries govern the transformation
            float p1[2], p2[2], p3[2], p4[2];
            array_subtract(p1, *(LED_CMY + i), WP); // must shift so that the white point is at the origin
            array_subtract(p2, *(CMY + i),     WP);
            array_subtract(p3, *(LED_CMY + j), WP);
            array_subtract(p4, *(CMY + j),     WP);
            pair_transform(mat1, p1, p2, p3, p4);
            gamut_transform(p,mat1,WP); // Still want to perfom inner transform on p
            
        } else { // must be in outer RGB triangles
            bary(L, p, *(CMY + 1), *(CMY + 2), *(RGB + 0)); //Test red triangle (magenta-yellow-red)
            L_DEBUG("Red test bary: [%g, %g, %g]", L[0], L[1], L[2]);
            if ((L[0] >= 0) && (L[1] >= 0) && (L[2] >= 0)) {
                L_DEBUG("In magenta-red-yellow");
                i = 1;
                j = 2;
                k = 0;
            } else {
                bary(L, p, *(CMY + 0), *(CMY + 2), *(RGB + 1)); //Test green triangle (yellow-green-cyan)
                L_DEBUG("Green test bary: [%g,%g,%g]", L[0], L[1], L[2]);
                if ((L[0] >= 0) && (L[1] >= 0) && (L[2] >= 0)) {
                    L_DEBUG("In yellow-green-cyan");
                    i = 0;
                    j = 2;
                    k = 1;
                } else { // must be in blue triangle
                    L_DEBUG("In cyan-blue-magneta");
                    i = 0;
                    j = 1;
                    k = 2;
                }
            }
            L_DEBUG("Indices: [%i, %i, %i]", i, j, k);
            L_DEBUG("Pointer test: [%g, %g]", LED_CMY[i][0], LED_CMY[i][1]);
            // i and j correspond to which CMY primaries govern the inner transformation; k governs outer vertex transformation
                    
            // Need to put this into function to avoid repeating code, or use a test with k to apply second transform if necessary
            float p1[2], p2[2], p3[2], p4[2];
            array_subtract(p1, *(LED_CMY + i), WP); // must shift so that the white point is at the origin
            array_subtract(p2, *(CMY + i),     WP);
            array_subtract(p3, *(LED_CMY + j), WP);
            array_subtract(p4, *(CMY + j),     WP);
            pair_transform(mat1, p1, p2, p3, p4);
            gamut_transform(p, mat1, WP); // Still want to perfom inner transform on p
            L_DEBUG("p1 = [%g, %g], p2 = [%g, %g], p3 = [%g, %g], p4 = [%g, %g]", p1[0], p1[1], p2[0], p2[1], p3[0], p3[1], p4[0], p4[1]);
            L_DEBUG("After gamut transform p = [%g, %g]", p[0], p[1]);
            L_DEBUG("mat1 = { {%g, %g}, {%g, %g} }", mat1[0][0], mat1[0][1], mat1[1][0], mat1[1][1]);
            
            float vertex[2] = { RGB[k][0], RGB[k][1] }; // need to check that this properly copying
            L_DEBUG("vertex(sRGB): [%g, %g]", vertex[0], vertex[1]);
            gamut_transform(vertex, mat1, WP); // in particular want to track where the desired vertex went
            L_DEBUG("vertex(sRGB) after transform: [%g, %g]", vertex[0], vertex[1]);
            
            // Now apply additional trnasformation to map RGB vertexes
            float d[2] = { LED_CMY[i][0], LED_CMY[i][1] };
            float edge[2] = { LED_CMY[j][0] - LED_CMY[i][0], LED_CMY[j][1] - LED_CMY[i][1] };
            L_DEBUG("Edge: [%g, %g]", edge[0], edge[1]);
            L_DEBUG("d: [%g, %g]", d[0], d[1]);

            float mat2[2][2], p5[2], p6[2];
//            array_subtract(p5, (LED_RGB + k), d); THIS DOES NOT WORK for this matrix, idk why
            float new_vertex[2] = { myRGB[k][0], myRGB[k][1] };
            array_subtract(p5, new_vertex, d);
            array_subtract(p6, vertex, d);
            pair_transform(mat2, edge, edge, p5, p6); // this defines mat2
            gamut_transform(p, mat2, d); // this performs the shift by d, applies mat2, then shifts back

            L_DEBUG("LED_RGB+k: [%g, %g]", (LED_RGB+k)[0], (LED_RGB+k)[1]);
            L_DEBUG("new_vertex: [%g, %g]", new_vertex[0], new_vertex[1]);
            L_DEBUG("p5: [%g, %g]", p5[0], p5[1]);
            L_DEBUG("p6: [%g, %g]", p6[0], p6[1]);
            L_DEBUG("mat2 = { {%g, %g}, {%g, %g} }", mat2[0][0], mat2[0][1], mat2[1][0], mat2[1][1]);
            
        }
        INFO("New chromaticity: [%g, %g]", p[0], p[1]);
    }
  
    float targetRGB[3];
    bary(targetRGB, p, R, G, B);
    
    array_multiply(targetRGB, 1 / array_max(targetRGB, 3), 3); // Never can be all zeros, ok to divide; just to max out to do extraRGB
    
    //  NEW IDEA: start with RGBW and if there are 5 channels then add on the RGBWW. This might max out thw whites better, as it guarantees both whites are always used.
    float coeffs[5] = { 0, 0, 0, 0, 0 };
    if ((targetRGB[0] >= 0) && (targetRGB[1] >= 0) && (targetRGB[2] >= 0)) { // within gamut
        
        // RGBW assumes W is in CW position
        float coeffs1[4];
        getWeights(coeffs1, p, R, G, B, CW);
        for (uint8_t i = 0; i < 4; i++) {
            coeffs[i] += coeffs1[i];
        }
        // If WW, then compute RGBW again with WW, add to the main coeff list. Can easlily add support for any LEDs inside the gamut. The only thing I am worried about is that the RGB is always pulling float-duty with two vertexes instead of so
        if (LIGHTBULB_CHANNELS == 5) {
            float coeffs2[4];
            getWeights(coeffs2, p, R, G, B, WW);
            for (uint8_t i = 0; i < 3; i++) {
                coeffs[i] += coeffs2[i];
            }
            coeffs[4] += coeffs2[3];
        }

        L_DEBUG("Coeffs before flux: %g, %g, %g, %g, %g",coeffs[0],coeffs[1],coeffs[2],coeffs[3],coeffs[4]);
        
        // (3.a.0) Correct for differences in intrinsic flux; needed before extraRGB step because we must balance RGB to whites first to see what headroom is left
        for (uint8_t i = 0; i < 5; i++) {
            if (lightbulb_group->flux[i] != 0) {
                coeffs[i] /= lightbulb_group->flux[i];
            } else {
                coeffs[i] = 0;
            }
        }

        // (3.a.1) Renormalize the coeffieients so that no LED is over-driven
        array_rescale(coeffs, 5);
        
        // (3.a.2) apply a correction to to scale down the whites according to saturation. rgb_min is (100-s)/100, so at full saturation there should be no whites at all. This is non-physical and should be avoided. Flux corrections are better.
        if (LIGHTBULB_CURVE_FACTOR != 0) {
            array_multiply(coeffs, 1 - (expf(LIGHTBULB_CURVE_FACTOR * s / 100.f) - 1) / (expf(LIGHTBULB_CURVE_FACTOR) - 1), 5);
        }
        
        // (3.a.3) Calculate any extra RGB that we have headroom for given the target color
        float extraRGB[3] = { targetRGB[0], targetRGB[1], targetRGB[2] }; // initialize the target for convenienece
        // Adjust the extra RGB according to relative flux; need to rescale those fluxes to >=1 in order to only shrink components, not go over calculated allotment
        float rflux[3] =  { lightbulb_group->flux[0], lightbulb_group->flux[1], lightbulb_group->flux[2] };
        array_multiply(rflux, 1.0f / array_min(rflux), 3); // assumes nonzero fluxes
        for (uint8_t i = 0; i < 3; i++) {
            if (rflux[i] != 0) {
                extraRGB[i] /= rflux[i];
            } else {
                extraRGB[i] = 0;
            }
        }

        float loRGB[3] = {1 - coeffs[0], 1 - coeffs[1], 1 - coeffs[2]}; // 'leftover' RGB
        if ((loRGB[0] >= 0) && (loRGB[1] >= 0) && (loRGB[2] >= 0)) { // this test seems totally unecessary
            float diff[3] = {extraRGB[0] - loRGB[0], extraRGB[1] - loRGB[1], extraRGB[2] - loRGB[2]};
            const float maxdiff = array_max(diff, 3);
            if ((maxdiff == diff[0]) && (extraRGB[0] != 0)) {
                array_multiply(extraRGB, loRGB[0] / extraRGB[0], 3);
            } else if ((maxdiff == diff[1]) && (extraRGB[1] != 0)) {
                array_multiply(extraRGB, loRGB[1] / extraRGB[1], 3);
            } else if ((maxdiff == diff[2]) && (extraRGB[2] != 0)) {
                array_multiply(extraRGB, loRGB[2] / extraRGB[2], 3);
            }
        } else {
            array_multiply(extraRGB, 0, 3);
        }
        
        L_DEBUG("extraRGB: %g, %g, %g", extraRGB[0],extraRGB[1],extraRGB[2]);

        // (3.a.4) Add the extra RGB to the final tally
        coeffs[0] += extraRGB[0];
        coeffs[1] += extraRGB[1];
        coeffs[2] += extraRGB[2];
    
    } else { // (3.b.1) Outside of gamut; easiest thing to do is to clamp the barycentric coordinates and renormalize... this might explain some bluish purples?
        for (uint8_t i = 0; i < 3; i++) {
            if (targetRGB[i] < 0) {
                targetRGB[i] = 0;
            }
            if (targetRGB[i] > 1) { // Used to be redundant, now with color factors is useful
                targetRGB[i] = 1;
            }
        }
        
        float vals[5] = { targetRGB[0] / lightbulb_group->flux[0], targetRGB[1] / lightbulb_group->flux[1], targetRGB[2] / lightbulb_group->flux[2], 0, 0 };
        array_equals(coeffs, vals, 5);
        L_DEBUG("Out of gamut: %g, %g, %g", targetRGB[0], targetRGB[1], targetRGB[2]);
    }
  
    // Rescale to make sure none are overdriven
    array_rescale(coeffs, 5);
    
    // (5) Brightness defined by the normalized value argument. We also divide by the scale found earlier to amp the brightness to maximum when the value is 1 (v/100). We also introduce the PWM_SCALE as the final 'units'.
    // Max power cutoff: want to limit the total scaled flux. Should do sum of flux times coeff, but what should the cutoff be? Based on everything being on, i.e. sum of fluxes.

    float brightness = (v / 100.f) * PWM_SCALE;

    if (LIGHTBULB_MAX_POWER != 1) {
        const float flux_ratio = array_dot(lightbulb_group->flux, coeffs) / array_sum(lightbulb_group->flux); // Actual brightness compared to theoretrical max brightness
        brightness *= MIN(LIGHTBULB_MAX_POWER, flux_ratio) / flux_ratio; // Hard cap at 1 as to not accidentally overdrive
    }

    // (6) Assign the target colors to lightbulb group struct, now in fraction of PWM_SCALE. This min function is just a final check, it should not ever go over PWM_SCALE.
    for (uint8_t n = 0; n < 5; n++) {
        lightbulb_group->target[n]  = MIN(floorf(coeffs[n] * brightness), PWM_SCALE);
    }

#ifdef LIGHT_DEBUG
    INFO("hsi2rgbw runtime: %0.3f ms", ((float) (sdk_system_get_time() - run_time)) * 1e-3);
#endif
}
/*
void hsi2white(uint16_t h, float s, uint8_t v, ch_group_t* ch_group) {
    lightbulb_group_t* lightbulb_group = lightbulb_group_find(ch_group->ch[0]);
    
    
}

void white2hsi(uint16_t t, ch_group_t* ch_group) {
    lightbulb_group_t* lightbulb_group = lightbulb_group_find(ch_group->ch[0]);
    
    
}
*/
void IRAM rgbw_set_timer_worker() {
    if (!main_config.setpwm_bool_semaphore) {
        main_config.setpwm_bool_semaphore = true;
        
        bool all_channels_ready = true;
        
        lightbulb_group_t* lightbulb_group = main_config.lightbulb_groups;
        
        while (main_config.setpwm_is_running && lightbulb_group) {
            if (LIGHTBULB_TYPE != LIGHTBULB_TYPE_VIRTUAL) {
                lightbulb_group->has_changed = false;
                for (uint8_t i = 0; i < LIGHTBULB_CHANNELS; i++) {
                    if (abs(lightbulb_group->target[i] - lightbulb_group->current[i]) > abs(LIGHTBULB_STEP_VALUE[i])) {
                        all_channels_ready = false;
                        lightbulb_group->current[i] += LIGHTBULB_STEP_VALUE[i];
                        lightbulb_group->has_changed = true;
                        
                        if (LIGHTBULB_TYPE == LIGHTBULB_TYPE_PWM) {
                            adv_pwm_set_duty(lightbulb_group->gpio[i], lightbulb_group->current[i], LIGHTBULB_PWM_DITHER);
                        }
                    } else if (lightbulb_group->current[i] != lightbulb_group->target[i]) {
                        lightbulb_group->current[i] = lightbulb_group->target[i];
                        lightbulb_group->has_changed = true;
                        
                        if (LIGHTBULB_TYPE == LIGHTBULB_TYPE_PWM) {
                            adv_pwm_set_duty(lightbulb_group->gpio[i], lightbulb_group->current[i], LIGHTBULB_PWM_DITHER);
                        }
                    }
                }
                
                if (lightbulb_group->has_changed && LIGHTBULB_TYPE == LIGHTBULB_TYPE_MY92X1) {
                    //
                    // TO-DO
                    //
                }
            }
            
            lightbulb_group = lightbulb_group->next;
        }
        
        addressled_t* addressled = main_config.addressleds;
        while (addressled) {
            uint8_t* colors = malloc(addressled->max_range);
            memset(colors, 0, addressled->max_range);
            if (colors) {
                bool has_changed = false;
                
                lightbulb_group = main_config.lightbulb_groups;
                while (lightbulb_group) {
                    if (LIGHTBULB_TYPE == LIGHTBULB_TYPE_NRZ && lightbulb_group->gpio[0] == addressled->gpio) {
                        if (lightbulb_group->has_changed) {
                            has_changed = true;
                        }
                        
                        for (uint16_t p = LIGHTBULB_RANGE_START; p < LIGHTBULB_RANGE_END; p = p + LIGHTBULB_CHANNELS) {
                            for (uint8_t i = 0; i < LIGHTBULB_CHANNELS; i++) {
                                uint8_t color = lightbulb_group->current[addressled->map[i]] >> 8;
                                memcpy(colors + p + i, &color, 1);
                            }
                        }
                    }
                    
                    lightbulb_group = lightbulb_group->next;
                }
                
                if (has_changed) {
                    taskENTER_CRITICAL();
                    nrzled_set(addressled->gpio, addressled->time_0, addressled->time_1, addressled->period, colors, addressled->max_range);
                    taskEXIT_CRITICAL();
                }
                
                free(colors);
            } else {
                homekit_remove_oldest_client();
                break;
            }
            
            addressled = addressled->next;
        }
        
        if (all_channels_ready) {
            main_config.setpwm_is_running = false;
            esp_timer_stop(main_config.set_lightbulb_timer);
            
            INFO("Color fixed");
        }

        main_config.setpwm_bool_semaphore = false;
        
    } else {
        ERROR("Color set");
    }
}

void lightbulb_task(void* args) {
    ch_group_t* ch_group = args;
    lightbulb_group_t* lightbulb_group = lightbulb_group_find(ch_group->ch[0]);
    
    led_blink(1);
    INFO("<%i> Lightbulb setter ON %i, BRI %i", ch_group->accessory, ch_group->ch[0]->value.bool_value, ch_group->ch[1]->value.int_value);
    if (LIGHTBULB_CHANNELS == 2) {
        INFO("<%i> Lightbulb setter TEMP %i", ch_group->accessory, ch_group->ch[2]->value.int_value);
    } else if (LIGHTBULB_CHANNELS >= 3) {
        /*
        if (lightbulb_group->temp_changed) {
            INFO("<%i> Lightbulb setter TEMP %i", ch_group->accessory, ch_group->ch[4]->value.int_value);
        } else {
        */
            INFO("<%i> Lightbulb setter HUE %g, SAT %g", ch_group->accessory, ch_group->ch[2]->value.float_value, ch_group->ch[3]->value.float_value);
        //}
    }
    
    if (ch_group->ch[0]->value.bool_value) {
        // Turn ON
        if (lightbulb_group->target[0] == 0 &&
            lightbulb_group->target[1] == 0 &&
            lightbulb_group->target[2] == 0 &&
            lightbulb_group->target[3] == 0 &&
            lightbulb_group->target[4] == 0) {
            setup_mode_toggle_upcount();
        }

        if (LIGHTBULB_CHANNELS >= 3) {
            // Channels 3+
            hsi2rgbw(ch_group->ch[2]->value.float_value, ch_group->ch[3]->value.float_value, ch_group->ch[1]->value.int_value, ch_group);
            
        } else if (LIGHTBULB_CHANNELS == 2) {
            // Channels 2
            uint16_t target_color = 0;
            if (ch_group->ch[2]->value.int_value >= COLOR_TEMP_MAX - 5) {
                target_color = PWM_SCALE;
                
            } else if (ch_group->ch[2]->value.int_value > COLOR_TEMP_MIN + 1) { // Conversion based on @seritos curve
                target_color = PWM_SCALE * (((0.09 + sqrt(0.18 + (0.1352 * (ch_group->ch[2]->value.int_value - COLOR_TEMP_MIN - 1)))) / 0.0676) - 1) / 100;
            }
            
            if (LIGHTBULB_TYPE == LIGHTBULB_TYPE_PWM_CWWW) {
                const uint32_t w = PWM_SCALE * ch_group->ch[1]->value.int_value / 100;
                lightbulb_group->target[0] = MIN(LIGHTBULB_MAX_POWER * w, PWM_SCALE);
                lightbulb_group->target[1] = target_color;
                
            } else {
                const uint32_t cw = lightbulb_group->flux[0] * target_color * ch_group->ch[1]->value.int_value / 100;
                const uint32_t ww = lightbulb_group->flux[1] * (PWM_SCALE - target_color) * ch_group->ch[1]->value.int_value / 100;
                lightbulb_group->target[0] = MIN(LIGHTBULB_MAX_POWER * cw, PWM_SCALE);
                lightbulb_group->target[1] = MIN(LIGHTBULB_MAX_POWER * ww, PWM_SCALE);
            }
            
        } else {
            // Channels 1
            const uint32_t w = PWM_SCALE * ch_group->ch[1]->value.int_value / 100;
            lightbulb_group->target[0] = MIN(LIGHTBULB_MAX_POWER * w, PWM_SCALE);
        }
    } else {
        // Turn OFF
        lightbulb_group->autodimmer = 0;
        
        for (uint8_t i = 0; i < 5; i++) {
            lightbulb_group->target[i] = 0;
        }
        
        setup_mode_toggle_upcount();
    }
    
    homekit_characteristic_notify_safe(ch_group->ch[0]);
    
    INFO("<%i> Target Color = %i, %i, %i, %i, %i", ch_group->accessory, lightbulb_group->target[0],
                                                                        lightbulb_group->target[1],
                                                                        lightbulb_group->target[2],
                                                                        lightbulb_group->target[3],
                                                                        lightbulb_group->target[4]);
    
    // Turn ON
    if (lightbulb_group->target[0] != lightbulb_group->current[0] ||
        lightbulb_group->target[1] != lightbulb_group->current[1] ||
        lightbulb_group->target[2] != lightbulb_group->current[2] ||
        lightbulb_group->target[3] != lightbulb_group->current[3] ||
        lightbulb_group->target[4] != lightbulb_group->current[4]
        ) {
        if (LIGHTBULB_TYPE != LIGHTBULB_TYPE_VIRTUAL) {
            uint16_t max_diff = abs(lightbulb_group->current[0] - lightbulb_group->target[0]);
            for (uint8_t i = 1; i < LIGHTBULB_CHANNELS; i++) {
                const uint16_t diff = abs(lightbulb_group->current[i] - lightbulb_group->target[i]);
                if (diff > max_diff) {
                    max_diff = diff;
                }
            }
            
            const uint16_t steps = (max_diff / lightbulb_group->step) + 1;
            
            for (uint8_t i = 0; i < LIGHTBULB_CHANNELS; i++) {
                if (lightbulb_group->target[i] == lightbulb_group->current[i]) {
                    LIGHTBULB_STEP_VALUE[i] = 0;
                } else {
                    LIGHTBULB_STEP_VALUE[i] = ((lightbulb_group->target[i] - lightbulb_group->current[i]) / steps) + (lightbulb_group->target[i] > lightbulb_group->current[i] ? 1 : -1);
                }
            }
        }
        
        if (LIGHTBULB_TYPE != LIGHTBULB_TYPE_VIRTUAL && !main_config.setpwm_is_running) {
            main_config.setpwm_is_running = true;
            rgbw_set_timer_worker(main_config.set_lightbulb_timer);
            esp_timer_start(main_config.set_lightbulb_timer);
        }
        
        homekit_characteristic_notify_safe(ch_group->ch[1]);
        
        if (ch_group->chs > 2) {
            homekit_characteristic_notify_safe(ch_group->ch[2]);
            if (ch_group->chs > 3) {
                homekit_characteristic_notify_safe(ch_group->ch[3]);
            }
        }
    }
    
    do_actions(ch_group, (uint8_t) ch_group->ch[0]->value.bool_value);
    
    if (ch_group->ch[0]->value.bool_value) {
        do_wildcard_actions(ch_group, 0, ch_group->ch[1]->value.int_value);
    } else {
        ch_group->last_wildcard_action[0] = NO_LAST_WILDCARD_ACTION;
    }
    
    save_states_callback();
    
    lightbulb_group->lightbulb_task_running = false;
    
    vTaskDelete(NULL);
}

void lightbulb_task_timer(TimerHandle_t xTimer) {
    if (xTaskCreate(lightbulb_task, "light", LIGHTBULB_TASK_SIZE, (void*) pvTimerGetTimerID(xTimer), LIGHTBULB_TASK_PRIORITY, NULL) != pdPASS) {
        ch_group_t* ch_group = (void*) pvTimerGetTimerID(xTimer);
        lightbulb_group_t* lightbulb_group = lightbulb_group_find(ch_group->ch[0]);
        lightbulb_group->lightbulb_task_running = false;
        ERROR("Creating lightbulb");
        homekit_remove_oldest_client();
        esp_timer_start(xTimer);
    }
}

void hkc_rgbw_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (!ch_group->main_enabled) {
        homekit_characteristic_notify_safe(ch);
        
    } else if (ch != ch_group->ch[0] || value.bool_value != ch_group->ch[0]->value.bool_value) {
        bool old_on_value = ch_group->ch[0]->value.bool_value;
        ch->value = value;
        
        lightbulb_group_t* lightbulb_group = lightbulb_group_find(ch_group->ch[0]);
        
         /*
        if (ch == ch_group->ch[4]) {
            lightbulb_group->temp_changed = true;
        } else {
            lightbulb_group->temp_changed = false;
        }
        */
        
        if (ch == ch_group->ch[0]) {
            if (ch->value.bool_value) {
                esp_timer_start(ch_group->timer2);
            } else {
                esp_timer_stop(ch_group->timer2);
            }
        }
        
        if (old_on_value == ch_group->ch[0]->value.bool_value && lightbulb_group->autodimmer == 0) {
            esp_timer_start(LIGHTBULB_SET_DELAY_TIMER);
        } else if (!lightbulb_group->lightbulb_task_running) {
            lightbulb_group->lightbulb_task_running = true;
            esp_timer_stop(LIGHTBULB_SET_DELAY_TIMER);
            lightbulb_task_timer(LIGHTBULB_SET_DELAY_TIMER);
        }
        
        save_historical_data(ch);

    } else {
        homekit_characteristic_notify_safe(ch_group->ch[0]);
    }
}

void rgbw_brightness(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;

    if (ch_group->child_enabled) {
        if (type == LIGHTBULB_BRIGHTNESS_UP) {
            if (ch_group->ch[1]->value.int_value + 10 < 100) {
                ch_group->ch[1]->value.int_value += 10;
            } else {
                ch_group->ch[1]->value.int_value = 100;
            }
        } else {    // type == LIGHTBULB_BRIGHTNESS_DOWN
            if (ch_group->ch[1]->value.int_value - 10 > 0) {
                ch_group->ch[1]->value.int_value -= 10;
            } else {
                ch_group->ch[1]->value.int_value = 1;
            }
        }
        
        hkc_rgbw_setter(ch_group->ch[1], ch_group->ch[1]->value);
    }
}

void autodimmer_task(void* args) {
    homekit_characteristic_t* ch = args;
    ch_group_t* ch_group = ch_group_find(ch);
    lightbulb_group_t* lightbulb_group = lightbulb_group_find(ch_group->ch[0]);
    
    lightbulb_group->autodimmer = ((8 * 100) / lightbulb_group->autodimmer_task_step) + 1;
    
    vTaskDelay(MS_TO_TICKS(AUTODIMMER_DELAY));
    
    if (lightbulb_group->autodimmer == 0) {
        hkc_rgbw_setter(ch_group->ch[0], HOMEKIT_BOOL(false));
        vTaskDelete(NULL);
    }
    
    INFO("<%i> AUTODim ON", ch_group->accessory);
    
    lightbulb_group->autodimmer_reverse = !lightbulb_group->autodimmer_reverse;
    
    while (lightbulb_group->autodimmer > 0) {
        lightbulb_group->autodimmer--;
        
        if (!lightbulb_group->autodimmer_reverse) {
            int16_t new_result = ch_group->ch[1]->value.int_value + lightbulb_group->autodimmer_task_step;
            if (new_result < 100) {
                if (ch_group->ch[1]->value.int_value == 1 && new_result > 2) {
                    new_result -= 1;
                }
                ch_group->ch[1]->value.int_value = new_result;
            } else {
                ch_group->ch[1]->value.int_value = 100;
                lightbulb_group->autodimmer_reverse = true;
            }
        } else {
            int16_t new_result = ch_group->ch[1]->value.int_value - lightbulb_group->autodimmer_task_step;
            if (new_result > 1) {
                ch_group->ch[1]->value.int_value = new_result;
            } else {
                ch_group->ch[1]->value.int_value = 1;
                lightbulb_group->autodimmer_reverse = false;
            }
        }
        
        hkc_rgbw_setter(ch_group->ch[1], ch_group->ch[1]->value);
        
        vTaskDelay(lightbulb_group->autodimmer_task_delay);
        
        if (ch_group->ch[1]->value.int_value == 1 ||
            ch_group->ch[1]->value.int_value == 100) {    // Double wait when brightness is 1% or 100%
            vTaskDelay(lightbulb_group->autodimmer_task_delay);
        }
    }
    
    INFO("<%i> AUTODim OFF", ch_group->accessory);
    
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
        ch_group_t* ch_group = ch_group_find(ch0);
        if (lightbulb_group->armed_autodimmer) {
            lightbulb_group->armed_autodimmer = false;
            esp_timer_stop(LIGHTBULB_AUTODIMMER_TIMER);
            
            if (xTaskCreate(autodimmer_task, "autodim", AUTODIMMER_TASK_SIZE, (void*) ch0, AUTODIMMER_TASK_PRIORITY, NULL) != pdPASS) {
                ERROR("<%i> Creating AUTODim", ch_group->accessory);
                homekit_remove_oldest_client();
            }
        } else {
            esp_timer_start(LIGHTBULB_AUTODIMMER_TIMER);
            lightbulb_group->armed_autodimmer = true;
        }
    }
}

// --- GARAGE DOOR
void garage_door_stop(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    if (GD_CURRENT_DOOR_STATE_INT == GARAGE_DOOR_OPENING || GD_CURRENT_DOOR_STATE_INT == GARAGE_DOOR_CLOSING) {
        led_blink(1);
        INFO("<%i> GD stop", ch_group->accessory);
        
        GD_CURRENT_DOOR_STATE_INT = GARAGE_DOOR_STOPPED;
        
        esp_timer_stop(LIGHTBULB_AUTODIMMER_TIMER);

        save_historical_data(ch_group->ch[0]);
        
        do_actions(ch_group, 10);
        
        homekit_characteristic_notify_safe(GD_CURRENT_DOOR_STATE);
    }
}

void garage_door_obstruction(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    led_blink(1);
    INFO("<%i> GD obstr %i", ch_group->accessory, type);
    
    GD_OBSTRUCTION_DETECTED_BOOL = (bool) type;
    
    homekit_characteristic_notify_safe(GD_OBSTRUCTION_DETECTED);
    
    save_historical_data(ch_group->ch[2]);
    
    do_actions(ch_group, type + 8);
}

void garage_door_sensor(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    led_blink(1);
    INFO("<%i> GD sensor %i", ch_group->accessory, type);
    
    GD_CURRENT_DOOR_STATE_INT = type;
    
    if (type > 1) {
        GD_TARGET_DOOR_STATE_INT = type - 2;
        esp_timer_start(ch_group->timer);
    } else {
        esp_timer_stop(ch_group->timer);
        GD_TARGET_DOOR_STATE_INT = type;
        
        if (type == 0) {
            GARAGE_DOOR_CURRENT_TIME = GARAGE_DOOR_WORKING_TIME - GARAGE_DOOR_TIME_MARGIN;
        } else {    // type == 1
            GARAGE_DOOR_CURRENT_TIME = GARAGE_DOOR_TIME_MARGIN;
        }
        
        if (GD_OBSTRUCTION_DETECTED_BOOL) {
            garage_door_obstruction(99, ch_group, 0);
        }
    }
    
    homekit_characteristic_notify_safe(GD_CURRENT_DOOR_STATE);
    homekit_characteristic_notify_safe(GD_TARGET_DOOR_STATE);
    
    save_historical_data(ch_group->ch[0]);
    
    do_actions(ch_group, type + 4);
}

void hkc_garage_door_setter(homekit_characteristic_t* ch1, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch1);
    if (ch_group->main_enabled) {
        uint8_t current_door_state = GD_CURRENT_DOOR_STATE_INT;
        if (current_door_state == GARAGE_DOOR_STOPPED) {
            current_door_state = GD_TARGET_DOOR_STATE_INT;
            GD_CURRENT_DOOR_STATE_INT = current_door_state + 2;
        } else if (current_door_state > GARAGE_DOOR_CLOSED) {
            if (GARAGE_DOOR_VIRTUAL_STOP == 1) {
                garage_door_stop(99, ch_group, 0);
            } else {
                current_door_state -= 2;
            }
        }

        if (value.int_value != current_door_state && GD_CURRENT_DOOR_STATE_INT != GARAGE_DOOR_STOPPED) {
            led_blink(1);
            INFO("<%i> Setter GD %i", ch_group->accessory, value.int_value);
            
            ch1->value.int_value = value.int_value;

            do_actions(ch_group, (uint8_t) GD_CURRENT_DOOR_STATE_INT);
   
            if ((value.int_value == GARAGE_DOOR_OPENED && GARAGE_DOOR_HAS_F4 == 0) ||
                GD_CURRENT_DOOR_STATE_INT == GARAGE_DOOR_CLOSING) {
                garage_door_sensor(99, ch_group, GARAGE_DOOR_OPENING);
                
            } else if ((value.int_value == GARAGE_DOOR_CLOSED && GARAGE_DOOR_HAS_F5 == 0) ||
                       GD_CURRENT_DOOR_STATE_INT == GARAGE_DOOR_OPENING) {
                garage_door_sensor(99, ch_group, GARAGE_DOOR_CLOSING);
            }
            
            setup_mode_toggle_upcount();
        }
        
        save_historical_data(ch1);
    }
    
    homekit_characteristic_notify_safe(ch1);
}

void garage_door_timer_worker(TimerHandle_t xTimer) {
    ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
    
    void halt_timer() {
        esp_timer_stop(ch_group->timer);
        if (GARAGE_DOOR_TIME_MARGIN > 0) {
            garage_door_obstruction(99, ch_group, 3);
        }
    }
    
    if (GD_CURRENT_DOOR_STATE_INT == GARAGE_DOOR_OPENING) {
        GARAGE_DOOR_CURRENT_TIME++;

        if (GARAGE_DOOR_CURRENT_TIME >= GARAGE_DOOR_WORKING_TIME - GARAGE_DOOR_TIME_MARGIN && GARAGE_DOOR_HAS_F2 == 0) {
            esp_timer_stop(ch_group->timer);
            garage_door_sensor(99, ch_group, GARAGE_DOOR_OPENED);
            
        } else if (GARAGE_DOOR_CURRENT_TIME >= GARAGE_DOOR_WORKING_TIME && GARAGE_DOOR_HAS_F2 == 1) {
            halt_timer();
        }
        
    } else {    // GARAGE_DOOR_CLOSING
        GARAGE_DOOR_CURRENT_TIME -= GARAGE_DOOR_CLOSE_TIME_FACTOR;
        
        if (GARAGE_DOOR_CURRENT_TIME <= GARAGE_DOOR_TIME_MARGIN && GARAGE_DOOR_HAS_F3 == 0) {
            esp_timer_stop(ch_group->timer);
            garage_door_sensor(99, ch_group, GARAGE_DOOR_CLOSED);
            
        } else if (GARAGE_DOOR_CURRENT_TIME <= 0 && GARAGE_DOOR_HAS_F3 == 1) {
            halt_timer();
        }
    }
    
    INFO("<%i> GD Pos %g/%g", ch_group->accessory, GARAGE_DOOR_CURRENT_TIME, GARAGE_DOOR_WORKING_TIME);
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
    esp_timer_stop(ch_group->timer);
    
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
    
    INFO("<%i> WC Stopped Motor %f, HomeKit %f", ch_group->accessory, WINDOW_COVER_MOTOR_POSITION, WINDOW_COVER_HOMEKIT_POSITION);
    
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
    
    if (WINDOW_COVER_VIRTUAL_STOP == 2) {
        WINDOW_COVER_VIRTUAL_STOP = 1;
    }
    
    homekit_characteristic_notify_safe(WINDOW_COVER_CH_STATE);
    homekit_characteristic_notify_safe(WINDOW_COVER_CH_CURRENT_POSITION);
    homekit_characteristic_notify_safe(WINDOW_COVER_CH_TARGET_POSITION);
    
    setup_mode_toggle_upcount();
    
    save_historical_data(ch_group->ch[0]);
    save_historical_data(ch_group->ch[1]);
    save_historical_data(ch_group->ch[2]);
    
    save_states_callback();
}

void window_cover_obstruction(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    led_blink(1);
    INFO("<%i> WC obstr %i", ch_group->accessory, type);
    
    WINDOW_COVER_CH_OBSTRUCTION->value.bool_value = (bool) type;
    
    do_actions(ch_group, type + WINDOW_COVER_OBSTRUCTION);
    
    if (WINDOW_COVER_VIRTUAL_STOP == 2) {
        WINDOW_COVER_VIRTUAL_STOP = 1;
    }
    
    save_historical_data(ch_group->ch[3]);
    
    homekit_characteristic_notify_safe(ch_group->ch[3]);
}

void window_cover_timer_rearm_stop(TimerHandle_t xTimer) {
    ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
    
    WINDOW_COVER_STOP_ENABLE = 1;
}

void hkc_window_cover_setter(homekit_characteristic_t* ch1, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch1);
    
    void disable_stop() {
        WINDOW_COVER_STOP_ENABLE = 0;
        esp_timer_start(ch_group->timer2);
    }
    
    void start_wc_timer() {
        if (WINDOW_COVER_CH_STATE->value.int_value == WINDOW_COVER_STOP) {
            esp_timer_start(ch_group->timer);
        }
    }
    
    void check_obstruction() {
        if (WINDOW_COVER_CH_OBSTRUCTION->value.bool_value) {
            WINDOW_COVER_CH_OBSTRUCTION->value.bool_value = false;
            homekit_characteristic_notify_safe(WINDOW_COVER_CH_OBSTRUCTION);
        }
    }
    
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("<%i> Setter WC: Current: %i, Target: %i", ch_group->accessory, WINDOW_COVER_CH_CURRENT_POSITION->value.int_value, value.int_value);
        
        ch1->value.int_value = value.int_value;

        normalize_position(ch_group);
        
        WINDOW_COVER_LAST_TIME = SYSTEM_UPTIME_MS;
        
        // CLOSE
        if (value.int_value < WINDOW_COVER_CH_CURRENT_POSITION->value.int_value) {
            disable_stop();
            
            if (WINDOW_COVER_CH_STATE->value.int_value == WINDOW_COVER_OPENING) {
                if (WINDOW_COVER_VIRTUAL_STOP == 1 && value.int_value == 0) {
                    window_cover_stop(ch_group);
                    vTaskDelay(5);
                } else {
                    do_actions(ch_group, WINDOW_COVER_CLOSING_FROM_MOVING);
                    
                    start_wc_timer();
                    
                    WINDOW_COVER_CH_STATE->value.int_value = WINDOW_COVER_CLOSING;
                }
                setup_mode_toggle_upcount();
                
            } else {
                do_actions(ch_group, WINDOW_COVER_CLOSING);
                
                start_wc_timer();
                
                WINDOW_COVER_CH_STATE->value.int_value = WINDOW_COVER_CLOSING;
            }
            
            check_obstruction();

        // OPEN
        } else if (value.int_value > WINDOW_COVER_CH_CURRENT_POSITION->value.int_value) {
            disable_stop();
            
            if (WINDOW_COVER_CH_STATE->value.int_value == WINDOW_COVER_CLOSING) {
                if (WINDOW_COVER_VIRTUAL_STOP == 1 && value.int_value == 100) {
                    window_cover_stop(ch_group);
                    vTaskDelay(5);
                } else {
                    do_actions(ch_group, WINDOW_COVER_OPENING_FROM_MOVING);
                    
                    start_wc_timer();
                    
                    WINDOW_COVER_CH_STATE->value.int_value = WINDOW_COVER_OPENING;
                }
                setup_mode_toggle_upcount();
                
            } else {
                do_actions(ch_group, WINDOW_COVER_OPENING);
                
                start_wc_timer();
                
                WINDOW_COVER_CH_STATE->value.int_value = WINDOW_COVER_OPENING;
            }
            
            check_obstruction();
            
        // STOP
        } else {
            window_cover_stop(ch_group);
        }
        
        do_wildcard_actions(ch_group, 0, value.int_value);
        
        save_historical_data(ch1);
    }
    
    if (WINDOW_COVER_VIRTUAL_STOP == 2) {
        WINDOW_COVER_VIRTUAL_STOP = 1;
    }
    
    homekit_characteristic_notify_safe(WINDOW_COVER_CH_STATE);
    homekit_characteristic_notify_safe(ch1);
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
            if (((int16_t) WINDOW_COVER_MOTOR_POSITION % 15) == 0) {
                INFO("<%i> WC Moving Motor %f, HomeKit %f", ch_group->accessory, WINDOW_COVER_MOTOR_POSITION, WINDOW_COVER_HOMEKIT_POSITION);
                WINDOW_COVER_CH_CURRENT_POSITION->value.int_value = WINDOW_COVER_HOMEKIT_POSITION;
                homekit_characteristic_notify_safe(WINDOW_COVER_CH_CURRENT_POSITION);
            } else {
                WINDOW_COVER_CH_CURRENT_POSITION->value.int_value = WINDOW_COVER_MOTOR_POSITION;
            }
        }
    }
    
    float window_cover_homekit_position(const float motor_position, const float correction) {
        return motor_position / (1 + ((100 - motor_position) * correction * 0.00020000f));
    }
    
    switch (WINDOW_COVER_CH_STATE->value.int_value) {
        case WINDOW_COVER_CLOSING:
            WINDOW_COVER_MOTOR_POSITION -= window_cover_step(ch_group, WINDOW_COVER_TIME_CLOSE);
            if (WINDOW_COVER_MOTOR_POSITION > 0) {
                WINDOW_COVER_HOMEKIT_POSITION = window_cover_homekit_position(WINDOW_COVER_MOTOR_POSITION, WINDOW_COVER_CORRECTION);
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
                WINDOW_COVER_HOMEKIT_POSITION = window_cover_homekit_position(WINDOW_COVER_MOTOR_POSITION, WINDOW_COVER_CORRECTION);
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
        if (ch0->value.int_value != value.int_value) {
            led_blink(1);
            INFO("<%i> Setter FAN %i", ch_group->accessory, value.int_value);
            
            ch0->value.int_value = value.int_value;
            
            if (value.int_value) {
                if (ch_group->ch[1]->value.float_value == 0) {
                    ch_group->ch[1]->value.float_value = *ch_group->ch[1]->max_value;
                    homekit_characteristic_notify_safe(ch_group->ch[1]);
                }
                do_wildcard_actions(ch_group, 0, ch_group->ch[1]->value.float_value);
                
                if (ch0->value.int_value == 1) {
                    esp_timer_start(ch_group->timer2);
                } else {
                    esp_timer_stop(ch_group->timer2);
                }
                
            } else {
                ch_group->last_wildcard_action[0] = NO_LAST_WILDCARD_ACTION;
            }

            setup_mode_toggle_upcount();
            save_states_callback();
            
            do_actions(ch_group, (uint8_t) value.int_value);
            
            save_historical_data(ch0);
        }
    }
    
    homekit_characteristic_notify_safe(ch0);
}

void hkc_fan_speed_setter(homekit_characteristic_t* ch1, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch1);
    if (ch_group->main_enabled) {
        if (ch1->value.float_value != value.float_value) {
            led_blink(1);
            INFO("<%i> Setter Speed FAN %g", ch_group->accessory, value.float_value);
            
            ch1->value.float_value = value.float_value;
            
            if (ch_group->ch[0]->value.bool_value) {
                do_wildcard_actions(ch_group, 0, value.float_value);
            }
            
            save_states_callback();
            
            save_historical_data(ch1);
        }
    }
    
    homekit_characteristic_notify_safe(ch1);
}

void hkc_fan_status_setter(homekit_characteristic_t* ch0, const homekit_value_t value) {
    if (ch0->value.int_value != value.int_value) {
        led_blink(1);
        ch_group_t* ch_group = ch_group_find(ch0);
        
        INFO("<%i> Setter Status FAN %i", ch_group->accessory, value.int_value);
        
        ch0->value.int_value = value.int_value;
        
        homekit_characteristic_notify_safe(ch0);
        
        save_states_callback();
    }
}

// --- LIGHT SENSOR
void light_sensor_task(void* args) {
    ch_group_t* ch_group = (ch_group_t*) args;
    
    float luxes = 0.0001f;
    
    const uint8_t light_sersor_type = LIGHT_SENSOR_TYPE;
    if (light_sersor_type < 2) {
        // https://www.allaboutcircuits.com/projects/design-a-luxmeter-using-a-light-dependent-resistor/
        const float adc_raw_read = sdk_system_adc_read();
        
        float ldr_resistor;
        if (light_sersor_type == 0) {
            ldr_resistor = LIGHT_SENSOR_RESISTOR * ((1023.1 - adc_raw_read) / adc_raw_read);
        } else {
            ldr_resistor = (LIGHT_SENSOR_RESISTOR  * adc_raw_read) / (1023.1 - adc_raw_read);
        }
        
        luxes = 1 / fast_precise_pow(ldr_resistor, LIGHT_SENSOR_POW);
        
    } else if (light_sersor_type == 2) {  // BH1750
        uint8_t value[2] = { 0, 0 };
        i2c_slave_read(LIGHT_SENSOR_I2C_BUS, LIGHT_SENSOR_I2C_ADDR, NULL, 0, value, 2);
        
        uint16_t final_value = value[0] << 8 | value[1];
        
        luxes = final_value / 1.2f;
    }
    
    luxes = (luxes * LIGHT_SENSOR_FACTOR) + LIGHT_SENSOR_OFFSET;
    INFO("<%i> Luxes %g", ch_group->accessory, luxes);
    
    if (luxes < 0.0001f) {
        luxes = 0.0001f;
    } else if (luxes > 100000.f) {
        luxes = 100000.f;
    }
    
    if ((uint32_t) ch_group->ch[0]->value.float_value != (uint32_t) luxes) {
        do_wildcard_actions(ch_group, 0, luxes);
        ch_group->ch[0]->value.float_value = luxes;
        homekit_characteristic_notify_safe(ch_group->ch[0]);
    }
    
    vTaskDelete(NULL);
}

void light_sensor_timer_worker(TimerHandle_t xTimer) {
    if (!homekit_is_pairing()) {
        if (xTaskCreate(light_sensor_task, "lux", LIGHT_SENSOR_TASK_SIZE, (void*) pvTimerGetTimerID(xTimer), LIGHT_SENSOR_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("Creating light_sensor");
            homekit_remove_oldest_client();
        }
    } else {
        ERROR("light_sensor_task: HK pairing");
    }
}

// --- SECURITY SYSTEM
void hkc_sec_system(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        if (ch->value.int_value != value.int_value) {
            led_blink(1);
            INFO("<%i> Setter SEC SYSTEM %i", ch_group->accessory, value.int_value);
            
            esp_timer_stop(SEC_SYSTEM_REC_ALARM_TIMER);
            
            ch->value.int_value = value.int_value;
            SEC_SYSTEM_CH_CURRENT_STATE->value.int_value = value.int_value;
            
            do_actions(ch_group, value.int_value);
            
            setup_mode_toggle_upcount();
            save_states_callback();
            
            save_historical_data(ch);
            save_historical_data(SEC_SYSTEM_CH_CURRENT_STATE);
        }
    }
    
    homekit_characteristic_notify_safe(ch);
    homekit_characteristic_notify_safe(SEC_SYSTEM_CH_CURRENT_STATE);
}

void hkc_sec_system_status(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch->value.int_value != value.int_value) {
        led_blink(1);
        INFO("<%i> Setter Status SEC SYSTEM %i", ch_group->accessory, value.int_value);
        
        esp_timer_stop(SEC_SYSTEM_REC_ALARM_TIMER);
        
        ch->value.int_value = value.int_value;
        SEC_SYSTEM_CH_CURRENT_STATE->value.int_value = value.int_value;
        
        save_states_callback();
        
        save_historical_data(ch);
        save_historical_data(SEC_SYSTEM_CH_CURRENT_STATE);
    }
    
    homekit_characteristic_notify_safe(ch);
    homekit_characteristic_notify_safe(SEC_SYSTEM_CH_CURRENT_STATE);
}

void sec_system_recurrent_alarm(TimerHandle_t xTimer) {
    ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
    
    if (SEC_SYSTEM_CH_CURRENT_STATE->value.int_value == 4) {
        SEC_SYSTEM_CH_CURRENT_STATE->value.int_value = SEC_SYSTEM_CH_TARGET_STATE->value.int_value;
    } else {
        SEC_SYSTEM_CH_CURRENT_STATE->value.int_value = 4;
    }
    
    homekit_characteristic_notify_safe(SEC_SYSTEM_CH_CURRENT_STATE);
}

// --- TV
void hkc_tv_active(homekit_characteristic_t* ch0, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch0);
    if (ch_group->main_enabled) {
        if (ch0->value.int_value != value.int_value) {
            led_blink(1);
            INFO("<%i> Setter TV ON %i", ch_group->accessory, value.int_value);
            
            ch0->value.int_value = value.int_value;
            
            do_actions(ch_group, value.int_value);
            
            setup_mode_toggle_upcount();
            save_states_callback();
            
            save_historical_data(ch0);
        }
    }
    
    homekit_characteristic_notify_safe(ch0);
}

void hkc_tv_status_active(homekit_characteristic_t* ch0, const homekit_value_t value) {
    if (ch0->value.int_value != value.int_value) {
        led_blink(1);
        ch_group_t* ch_group = ch_group_find(ch0);
        
        INFO("<%i> Setter Status TV ON %i", ch_group->accessory, value.int_value);
        
        ch0->value.int_value = value.int_value;
        
        homekit_characteristic_notify_safe(ch0);
        
        save_states_callback();
        
        save_historical_data(ch0);
    }
}

void hkc_tv_active_identifier(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        if (ch->value.int_value != value.int_value) {
            led_blink(1);
            INFO("<%i> Setter TV Input %i", ch_group->accessory, value.int_value);
            
            ch->value.int_value = value.int_value;

            do_actions(ch_group, (MAX_ACTIONS - 1) + value.int_value);
            
            save_historical_data(ch);
        }
    }
    
    homekit_characteristic_notify_safe(ch);
}

void hkc_tv_key(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("<%i> Setter TV Key %i", ch_group->accessory, value.int_value + 2);
        
        ch->value.int_value = value.int_value;
        
        do_actions(ch_group, value.int_value + 2);
        
        save_historical_data(ch);
    }
    
    homekit_characteristic_notify_safe(ch);
}

void hkc_tv_power_mode(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("<%i> Setter TV Settings %i", ch_group->accessory, value.int_value + 30);
        
        ch->value.int_value = value.int_value;
        
        do_actions(ch_group, value.int_value + 30);
        
        save_historical_data(ch);
    }
    
    homekit_characteristic_notify_safe(ch);
}

void hkc_tv_mute(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("<%i> Setter TV Mute %i", ch_group->accessory, value.int_value + 20);
        
        ch->value.int_value = value.int_value;
        
        do_actions(ch_group, value.int_value + 20);
        
        save_historical_data(ch);
    }
    
    homekit_characteristic_notify_safe(ch);
}

void hkc_tv_volume(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("<%i> Setter TV Volume %i", ch_group->accessory, value.int_value + 22);
        
        ch->value.int_value = value.int_value;
        
        do_actions(ch_group, value.int_value + 22);
        
        save_historical_data(ch);
    }
    
    homekit_characteristic_notify_safe(ch);
}

void hkc_tv_configured_name(homekit_characteristic_t* ch1, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch1);
    
    INFO("<%i> Setter TV Name %s", ch_group->accessory, value.string_value);
    
    char* new_name = strdup(value.string_value);
    
    homekit_value_destruct(&ch1->value);
    ch1->value = HOMEKIT_STRING(new_name);

    homekit_characteristic_notify_safe(ch1);

    save_states_callback();
}

void hkc_tv_input_configured_name(homekit_characteristic_t* ch, const homekit_value_t value) {
    homekit_characteristic_notify_safe(ch);
}

// --- BINARY INPUTS
void window_cover_diginput(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    INFO("<%i> WC DigI GPIO %i", ch_group->accessory, gpio);

    if (ch_group->child_enabled) {
        switch (type) {
            case WINDOW_COVER_CLOSING:
                if (WINDOW_COVER_VIRTUAL_STOP > 0) {
                    WINDOW_COVER_VIRTUAL_STOP = 2;
                }
                hkc_window_cover_setter(ch_group->ch[1], HOMEKIT_UINT8(0));
                break;
                
            case WINDOW_COVER_OPENING:
                if (WINDOW_COVER_VIRTUAL_STOP > 0) {
                    WINDOW_COVER_VIRTUAL_STOP = 2;
                }
                hkc_window_cover_setter(ch_group->ch[1], HOMEKIT_UINT8(100));
                break;
                
            case (WINDOW_COVER_CLOSING + 3):
                if (WINDOW_COVER_CH_STATE->value.int_value == WINDOW_COVER_CLOSING) {
                    hkc_window_cover_setter(ch_group->ch[1], WINDOW_COVER_CH_CURRENT_POSITION->value);
                } else {
                    if (WINDOW_COVER_VIRTUAL_STOP > 0) {
                        WINDOW_COVER_VIRTUAL_STOP = 2;
                    }
                    hkc_window_cover_setter(ch_group->ch[1], HOMEKIT_UINT8(0));
                }
                break;
                
            case (WINDOW_COVER_OPENING + 3):
                if (WINDOW_COVER_CH_STATE->value.int_value == WINDOW_COVER_OPENING) {
                    hkc_window_cover_setter(ch_group->ch[1], WINDOW_COVER_CH_CURRENT_POSITION->value);
                } else {
                    if (WINDOW_COVER_VIRTUAL_STOP > 0) {
                        WINDOW_COVER_VIRTUAL_STOP = 2;
                    }
                    hkc_window_cover_setter(ch_group->ch[1], HOMEKIT_UINT8(100));
                }
                break;
                
            default:    // case WINDOW_COVER_STOP:
                if ((int8_t) WINDOW_COVER_STOP_ENABLE == 1) {
                    hkc_window_cover_setter(ch_group->ch[1], WINDOW_COVER_CH_CURRENT_POSITION->value);
                } else {
                    INFO("<%i> WC DigI Stop ignored", ch_group->accessory);
                }
                break;
        }
    }
}

void diginput(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    INFO("<%i> DigI %i GPIO %i", ch_group->accessory, type, gpio);

    if (ch_group->child_enabled) {
        switch (ch_group->acc_type) {
            case ACC_TYPE_LOCK:
                if (type == 2) {
                    hkc_lock_setter(ch_group->ch[1], HOMEKIT_UINT8(!ch_group->ch[1]->value.int_value));
                } else {
                    hkc_lock_setter(ch_group->ch[1], HOMEKIT_UINT8(type));
                }
                break;
                
            case ACC_TYPE_WATER_VALVE:
                if (ch_group->ch[0]->value.int_value == 1) {
                    hkc_valve_setter(ch_group->ch[0], HOMEKIT_UINT8(0));
                } else {
                    hkc_valve_setter(ch_group->ch[0], HOMEKIT_UINT8(1));
                }
                break;
                
            case ACC_TYPE_LIGHTBULB:
                if (type == 2) {
                    if (ch_group->ch[1]->value.int_value == 0) {
                        ch_group->ch[1]->value.int_value = 100;
                    }
                    autodimmer_call(ch_group->ch[0], HOMEKIT_BOOL(!ch_group->ch[0]->value.bool_value));
                } else {
                    autodimmer_call(ch_group->ch[0], HOMEKIT_BOOL(type));
                }
                break;
                
            case ACC_TYPE_GARAGE_DOOR:
                if (type == 2) {
                    hkc_garage_door_setter(GD_TARGET_DOOR_STATE, HOMEKIT_UINT8(!GD_TARGET_DOOR_STATE_INT));
                } else {
                    hkc_garage_door_setter(GD_TARGET_DOOR_STATE, HOMEKIT_UINT8(type));
                }
                break;
                
            case ACC_TYPE_WINDOW_COVER:
                if (type == 2) {
                    if (ch_group->ch[1]->value.int_value == 0) {
                        hkc_window_cover_setter(ch_group->ch[1], HOMEKIT_UINT8(100));
                    } else {
                        hkc_window_cover_setter(ch_group->ch[1], HOMEKIT_UINT8(0));
                    }
                } else if (type == 1) {
                    hkc_window_cover_setter(ch_group->ch[1], HOMEKIT_UINT8(100));
                } else {
                    hkc_window_cover_setter(ch_group->ch[1], HOMEKIT_UINT8(0));
                }
                break;
                
            case ACC_TYPE_FAN:
                if (type == 2) {
                    hkc_fan_setter(ch_group->ch[0], HOMEKIT_BOOL(!ch_group->ch[0]->value.bool_value));
                } else {
                    hkc_fan_setter(ch_group->ch[0], HOMEKIT_BOOL(type));
                }
                break;
                
            case ACC_TYPE_TV:
                if (type == 2) {
                    hkc_tv_active(ch_group->ch[0], HOMEKIT_UINT8(!ch_group->ch[0]->value.int_value));
                } else {
                    hkc_tv_active(ch_group->ch[0], HOMEKIT_UINT8(type));
                }
                break;
                
            default:
                if (type == 2) {
                    hkc_on_setter(ch_group->ch[0], HOMEKIT_BOOL(!ch_group->ch[0]->value.bool_value));
                } else {
                    hkc_on_setter(ch_group->ch[0], HOMEKIT_BOOL(type));
                }
                break;
        }
    }
}

void digstate(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    INFO("<%i> DigI S%i GPIO %i", ch_group->accessory, type, gpio);

    if (ch_group->child_enabled) {
        switch (ch_group->acc_type) {
            case ACC_TYPE_LOCK:
                hkc_lock_status_setter(ch_group->ch[1], HOMEKIT_UINT8(type));
                break;
                
            case ACC_TYPE_WATER_VALVE:
                hkc_valve_status_setter(ch_group->ch[0], HOMEKIT_UINT8(type));
                break;
                
            case ACC_TYPE_FAN:
                hkc_fan_status_setter(ch_group->ch[0], HOMEKIT_BOOL(type));
                break;
                
            case ACC_TYPE_TV:
                hkc_tv_status_active(ch_group->ch[0], HOMEKIT_UINT8(type));
                break;
                
            default:
                hkc_on_status_setter(ch_group->ch[0], HOMEKIT_BOOL(type));
                break;
        }
    }
}

// --- AUTO-OFF
void hkc_autooff_setter_task(TimerHandle_t xTimer) {
    ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
    INFO("<%i> AutoOff", ch_group->accessory);
    
    switch (ch_group->acc_type) {
        case ACC_TYPE_LOCK:
            hkc_lock_setter(ch_group->ch[1], HOMEKIT_UINT8(1));
            break;
            
        case ACC_TYPE_CONTACT_SENSOR:
        case ACC_TYPE_MOTION_SENSOR:
            binary_sensor(99, ch_group, 0);
            break;
            
        case ACC_TYPE_WATER_VALVE:
            hkc_valve_setter(ch_group->ch[0], HOMEKIT_UINT8(0));
            break;
            
        case ACC_TYPE_LIGHTBULB:
            hkc_rgbw_setter(ch_group->ch[0], HOMEKIT_BOOL(false));
            break;
            
        case ACC_TYPE_FAN:
            hkc_fan_setter(ch_group->ch[0], HOMEKIT_BOOL(false));
            break;
            
        default:    // ACC_TYPE_SWITCH  ACC_TYPE_OUTLET
            hkc_on_setter(ch_group->ch[0], HOMEKIT_BOOL(false));
            break;
    }
}

// --- Network Action task
void net_action_task(void* pvParameters) {
    action_task_t* action_task = pvParameters;
    action_network_t* action_network = action_task->ch_group->action_network;
    
    uint8_t errors = 0;
    
    while (action_network) {
        if (action_network->action == action_task->action && !action_network->is_running) {
            action_network->is_running = true;
            
            while (main_config.network_is_busy) {
                vTaskDelay(MS_TO_TICKS(50));
            }
            
            main_config.network_is_busy = true;

            INFO("<%i> Network Action %s:%i", action_task->ch_group->accessory, action_network->host, action_network->port_n);
            
            struct addrinfo* res;
            
            char port[6];
            memset(port, 0, 6);
            itoa(action_network->port_n, port, 10);
            
            if (action_network->method_n < 10) {
                const struct addrinfo hints = {
                    .ai_family = AF_UNSPEC,
                    .ai_socktype = SOCK_STREAM,
                };
                
                int getaddr_result = getaddrinfo(action_network->host, port, &hints, &res);
                if (getaddr_result == 0) {
                    int s = socket(res->ai_family, res->ai_socktype, 0);
                    if (s >= 0) {
                        const struct timeval sndtimeout = { 3, 0 };
                        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &sndtimeout, sizeof(sndtimeout));
                        const struct timeval rcvtimeout = { 1, 0 };
                        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &rcvtimeout, sizeof(rcvtimeout));

                        int connect_result = connect(s, res->ai_addr, res->ai_addrlen);
                        if (connect_result == 0) {
                            uint16_t content_len_n = 0;
                            str_ch_value_t* str_ch_value_first = NULL;
                            
                            char* method = strdup("GET");
                            char* method_req = NULL;
                            if (action_network->method_n == 1 ||
                                action_network->method_n == 2 ||
                                action_network->method_n == 3) {
                                content_len_n = strlen(action_network->content);
                                
                                char* content_search = action_network->content;
                                str_ch_value_t* str_ch_value_last = NULL;
                                
                                do {
                                    content_search = strstr(content_search, NETWORK_ACTION_WILDCARD_VALUE);
                                    if (content_search) {
                                        char buffer[15];
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
                                                value = &ch_group_found->ch[1]->value;
                                                break;
                                                
                                            case 2:
                                                value = &ch_group_found->ch[2]->value;
                                                break;
                                                
                                            case 3:
                                                value = &ch_group_found->ch[3]->value;
                                                break;
                                                
                                            case 4:
                                                value = &ch_group_found->ch[4]->value;
                                                break;
                                                
                                            case 5:
                                                value = &ch_group_found->ch[5]->value;
                                                break;
                                                
                                            case 6:
                                                value = &ch_group_found->ch[6]->value;
                                                break;
                                                
                                            default:    // case 0:
                                                value = &ch_group_found->ch[0]->value;
                                                break;
                                        }
                                        
                                        switch (value->format) {
                                            case HOMETKIT_FORMAT_BOOL:
                                                snprintf(buffer, 15, "%s", value->bool_value ? "true" : "false");
                                                break;
                                                
                                            case HOMETKIT_FORMAT_UINT8:
                                            case HOMETKIT_FORMAT_UINT16:
                                            case HOMETKIT_FORMAT_UINT32:
                                            case HOMETKIT_FORMAT_UINT64:
                                            case HOMETKIT_FORMAT_INT:
                                                snprintf(buffer, 15, "%i", value->int_value);
                                                break;

                                            case HOMETKIT_FORMAT_FLOAT:
                                                snprintf(buffer, 15, "%1.7g", value->float_value);
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
                                        INFO("Wildcard val %s", str_ch_value->string);
                                        
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
                                method_req = malloc(23);
                                snprintf(method_req, 23, "Content-length: %s\r\n", content_len);
                                
                                free(method);
                                if (action_network->method_n == 1) {
                                    method = strdup("PUT");
                                } else {
                                    method = strdup("POST");
                                }
                            }
                            
                            char* req = NULL;
                            
                            if (action_network->method_n == 3) {    // TCP RAW
                                req = action_network->content;
                                
                            } else if (action_network->method_n != 4) {     // HTTP
                                action_network->len = 69 + strlen(method) + ((method_req != NULL) ? strlen(method_req) : 0) + strlen(FIRMWARE_VERSION) + strlen(action_network->host) +  strlen(action_network->url) + strlen(action_network->header) + content_len_n;
                                
                                req = malloc(action_network->len);
                                if (!req) {
                                    lwip_close(s);
                                    action_network->is_running = false;
                                    main_config.network_is_busy = false;
                                    
                                    if (method_req) {
                                        free(method_req);
                                    }
                                    
                                    free(method);
                                    freeaddrinfo(res);
                                    
                                    homekit_remove_oldest_client();
                                    errors++;
                                    
                                    if (errors < 5) {
                                        vTaskDelay(MS_TO_TICKS(200));
                                        continue;
                                    } else {
                                        break;
                                    }
                                }
                                
                                snprintf(req, action_network->len, "%s /%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: HAA/"FIRMWARE_VERSION" esp8266\r\nConnection: close\r\n%s%s\r\n",
                                         method,
                                         action_network->url,
                                         action_network->host,
                                         action_network->header,
                                         (method_req != NULL) ? method_req : "");
                                free(method);
                                
                                if (str_ch_value_first) {
                                    str_ch_value_t* str_ch_value = str_ch_value_first;
                                    char* content_search = action_network->content;
                                    char* last_pos = action_network->content;
                                    
                                    do {
                                        content_search = strstr(last_pos, NETWORK_ACTION_WILDCARD_VALUE);
                                        
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
                                    strcat(req, action_network->content);
                                }
                                
                            }
                            
                            int result = -1;
                            if (action_network->method_n == 4) {
                                result = write(s, action_network->raw, action_network->len);
                            } else {
                                result = write(s, req, action_network->len);
                            }

                            if (result >= 0) {
                                if (action_network->method_n == 4) {
                                    INFO("<%i> Payload RAW", action_task->ch_group->accessory);
                                } else {
                                    INFO("<%i> Payload:\n%s", action_task->ch_group->accessory, req);
                                }
                                
                                if (action_network->wait_response) {
                                    INFO("<%i> TCP Response:", action_task->ch_group->accessory);
                                    int read_byte;
                                    uint16_t total_recv = 0;
                                    do {
                                        uint8_t recv_buffer[64];
                                        memset(recv_buffer, 0, 64);
                                        read_byte = lwip_read(s, recv_buffer, 64);
                                        printf("%s", recv_buffer);
                                        recv_buffer[0] = 0;
                                        total_recv += 64;
                                    } while (read_byte > 0 && total_recv < 2048);
                                    
                                    INFO("Error: %i", read_byte);
                                }
                                
                            } else {
                                ERROR("<%i> TCP (%i)", action_task->ch_group->accessory, result);
                            }
                            
                            if (method_req) {
                                free(method_req);
                            }
                            
                            if (req && action_network->method_n != 3) {
                                free(req);
                            }

                        } else {
                            ERROR("<%i> Connection (%i)", action_task->ch_group->accessory, connect_result);
                        }
                    } else {
                        ERROR("<%i> Socket (%i)", action_task->ch_group->accessory, s);
                    }
                    
                    lwip_close(s);
                } else {
                    ERROR("<%i> DNS (%i)", action_task->ch_group->accessory, getaddr_result);
                }

            } else {
                const struct addrinfo hints = {
                    .ai_family = AF_UNSPEC,
                    .ai_socktype = SOCK_DGRAM,
                };
                
                int getaddr_result = getaddrinfo(action_network->host, port, &hints, &res);
                if (getaddr_result == 0) {
                    int s = socket(res->ai_family, res->ai_socktype, 0);
                    if (s >= 0) {
                        const struct timeval sndtimeout = { 3, 0 };
                        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &sndtimeout, sizeof(sndtimeout));
                        
                        uint8_t* wol = NULL;
                        if (action_network->method_n == 12) {
                            wol = malloc(102);
                            for (uint8_t i = 0; i < 6; i++) {
                                wol[i] = 255;
                            }
                            
                            for (uint8_t i = 6; i < 102; i += 6) {
                                for (uint8_t j = 0; j < 6; j++) {
                                    wol[i + j] = action_network->raw[j];
                                }
                            }
                        }
                        
                        int result = -1;
                        if (action_network->method_n == 13) {
                            result = lwip_sendto(s, action_network->content, action_network->len, 0, res->ai_addr, res->ai_addrlen);
                        } else {
                            uint8_t wol_attemps = 1;
                            if (wol) {
                                wol_attemps = 5;
                            }
                            
                            for (uint8_t udp_sent = 0; udp_sent < wol_attemps; udp_sent++) {
                                result = lwip_sendto(s, wol ? wol : action_network->raw, wol ? 102 : action_network->len, 0, res->ai_addr, res->ai_addrlen);
                                if (wol) {
                                    vTaskDelay(MS_TO_TICKS(10));
                                }
                            }
                        }
                        
                        if (result > 0) {
                            if (action_network->method_n == 13) {
                                INFO("<%i> Payload:\n%s", action_task->ch_group->accessory, action_network->content);
                            } else {
                                INFO("<%i> Payload RAW", action_task->ch_group->accessory);
                            }
                            
                        } else {
                            ERROR("<%i> UDP", action_task->ch_group->accessory);
                        }
                        
                        if (wol) {
                            free(wol);
                        }
                    } else {
                        ERROR("<%i> Socket (%i)", action_task->ch_group->accessory, s);
                    }
                
                    lwip_close(s);
                
                } else {
                    ERROR("<%i> DNS (%i)", action_task->ch_group->accessory, getaddr_result);
                }
            }
            
            freeaddrinfo(res);
            
            action_network->is_running = false;
            main_config.network_is_busy = false;
            
            INFO("<%i> Network Action %s:%i done", action_task->ch_group->accessory, action_network->host, action_network->port_n);
            
            vTaskDelay(MS_TO_TICKS(20));
        }
        
        action_network = action_network->next;
    }

    free(pvParameters);
    vTaskDelete(NULL);
}

// --- IR Send task
void ir_tx_task(void* pvParameters) {
    action_task_t* action_task = pvParameters;
    action_ir_tx_t* action_ir_tx = action_task->ch_group->action_ir_tx;
    
    uint8_t errors = 0;
    
    while (action_ir_tx) {
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
                
                printf("<%i> IR Protocol bits: ", action_task->ch_group->accessory);
                
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
                if (!ir_code) {
                    homekit_remove_oldest_client();
                    errors++;
                    
                    if (errors < 5) {
                        vTaskDelay(MS_TO_TICKS(200));
                        continue;
                    } else {
                        break;
                    }
                }
                
                INFO("<%i> IR Code Len: %i\nIR Protocol: %s", action_task->ch_group->accessory, ir_code_len, prot);
                
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
                
                INFO("\n<%i> IR code: %s", action_task->ch_group->accessory, action_ir_tx->prot_code);
                for (uint16_t i = 0; i < ir_code_len; i++) {
                    printf("%s%5d ", i & 1 ? "-" : "+", ir_code[i]);
                    if (i % 16 == 15) {
                        printf("\n");
                    }

                }
                printf("\n");
                
            } else {    // IR_ACTION_RAW_CODE
                const uint16_t json_ir_code_len = strlen(action_ir_tx->raw_code);
                ir_code_len = json_ir_code_len >> 1;
                
                ir_code = malloc(sizeof(uint16_t) * ir_code_len);
                
                INFO("<%i> IR packet (%i)", action_task->ch_group->accessory, ir_code_len);

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
                        printf("\n");
                    }
                }
                
                printf("\n");
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
                
                INFO("<%i> IR %i sent", action_task->ch_group->accessory, r);
                
                vTaskDelay(action_ir_tx->pause);
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
    vTaskDelay(1);
    
    action_task_t* action_task = pvParameters;
    action_uart_t* action_uart = action_task->ch_group->action_uart;

    while (action_uart) {
        if (action_uart->action == action_task->action) {
            INFO("<%i> UART Action", action_task->ch_group->accessory);
            
            taskENTER_CRITICAL();
            
            for (uint8_t i = 0; i < action_uart->len; i++) {
                uart_putc(action_uart->uart, action_uart->command[i]);
            }

            uart_flush_txfifo(action_uart->uart);
            
            taskEXIT_CRITICAL();
        }
        
        if (action_uart->pause > 0) {
            vTaskDelay(action_uart->pause);
        }
        
        action_uart = action_uart->next;
    }
    
    free(pvParameters);
    vTaskDelete(NULL);
}

// --- ACTIONS
void autoswitch_timer(TimerHandle_t xTimer) {
    action_binary_output_t* action_binary_output = (action_binary_output_t*) pvTimerGetTimerID(xTimer);

    extended_gpio_write(action_binary_output->gpio, !action_binary_output->value);
    INFO("AutoSw digO GPIO %i -> %i", action_binary_output->gpio, !action_binary_output->value);
    
    esp_timer_delete(xTimer);
}

void do_actions(ch_group_t* ch_group, uint8_t action) {
    INFO("<%i> Exec Action %i", ch_group->accessory, action);
    
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
    
    // Binary outputs
    action_binary_output_t* action_binary_output = ch_group->action_binary_output;
    while(action_binary_output) {
        if (action_binary_output->action == action) {
            extended_gpio_write(action_binary_output->gpio, action_binary_output->value);
            INFO("<%i> Binary Output: gpio %i, val %i, inch %g", ch_group->accessory, action_binary_output->gpio, action_binary_output->value, action_binary_output->inching);
            
            if (action_binary_output->inching > 0) {
                esp_timer_start(esp_timer_create(action_binary_output->inching * 1000, false, (void*) action_binary_output, autoswitch_timer));
            }
        }

        action_binary_output = action_binary_output->next;
    }
    
    // Service Notification Manager
    action_acc_manager_t* action_acc_manager = ch_group->action_acc_manager;
    while(action_acc_manager) {
        if (action_acc_manager->action == action) {
            ch_group_t* ch_group = ch_group_find_by_acc(action_acc_manager->accessory);
            if (ch_group) {
                INFO("Serv Notif: targ %i, val %g", action_acc_manager->accessory, action_acc_manager->value);
                
                int value_int = action_acc_manager->value;
                
                if (value_int == -10000) {
                    ch_group->main_enabled = false;
                } else if (value_int == -10001) {
                    ch_group->main_enabled = true;
                } else if (value_int == -10002) {
                    ch_group->main_enabled = !ch_group->main_enabled;
                } else if (value_int == -20000) {
                    ch_group->child_enabled = false;
                } else if (value_int == -20001) {
                    ch_group->child_enabled = true;
                } else if (value_int == -20002) {
                    ch_group->child_enabled = !ch_group->child_enabled;
                } else {
                    bool alarm_recurrent = false;
                    
                    switch (ch_group->acc_type) {
                        case ACC_TYPE_BUTTON:
                        case ACC_TYPE_DOORBELL:
                            button_event(0, ch_group, value_int);
                            break;
                            
                        case ACC_TYPE_LOCK:
                            if (value_int == -1) {
                                esp_timer_start(ch_group->timer2);
                            } else if (value_int == 4) {
                                hkc_lock_setter(ch_group->ch[1], HOMEKIT_UINT8(!ch_group->ch[1]->value.int_value));
                            } else if (value_int == 5) {
                                hkc_lock_status_setter(ch_group->ch[1], HOMEKIT_UINT8(!ch_group->ch[1]->value.int_value));
                            } else if (value_int > 1) {
                                hkc_lock_status_setter(ch_group->ch[1], HOMEKIT_UINT8((value_int - 2)));
                            } else {
                                hkc_lock_setter(ch_group->ch[1], HOMEKIT_UINT8(value_int));
                            }
                            break;
                            
                        case ACC_TYPE_CONTACT_SENSOR:
                        case ACC_TYPE_MOTION_SENSOR:
                            if (value_int == -1) {
                                esp_timer_start(ch_group->timer2);
                            } else {
                                binary_sensor(99, ch_group, value_int);
                            }
                            break;
                            
                        case ACC_TYPE_WATER_VALVE:
                            if (value_int < 0) {
                                if (value_int == -1) {
                                    esp_timer_start(ch_group->timer2);
                                }
                                
                                if (ch_group->chs > 2) {
                                    if (value_int < -1) {
                                        hkc_setter(ch_group->ch[2], HOMEKIT_UINT32(- value_int - 1));
                                    }
                                    if (value_int == -1 || ch_group->ch[3]->value.int_value > ch_group->ch[2]->value.int_value) {
                                        ch_group->ch[3]->value.int_value = ch_group->ch[2]->value.int_value;
                                    }
                            
                                }
                            } else {
                                hkc_valve_setter(ch_group->ch[0], HOMEKIT_UINT8(value_int));
                            }
                            break;
                            
                        case ACC_TYPE_THERMOSTAT:
                            value_int = action_acc_manager->value * 100.f;
                            if (value_int == 2) {
                                update_th(ch_group->ch[2], HOMEKIT_UINT8(0));
                            } else if (value_int == 3) {
                                update_th(ch_group->ch[2], HOMEKIT_UINT8(1));
                            } else if (value_int == 4) {
                                update_th(ch_group->ch[4], HOMEKIT_UINT8(2));
                            } else if (value_int == 5) {
                                update_th(ch_group->ch[4], HOMEKIT_UINT8(1));
                            } else if (value_int == 6) {
                                update_th(ch_group->ch[4], HOMEKIT_UINT8(0));
                            } else {
                                if (value_int % 2 == 0) {
                                    update_th(ch_group->ch[5], HOMEKIT_FLOAT(action_acc_manager->value));
                                } else {
                                    update_th(ch_group->ch[6], HOMEKIT_FLOAT(action_acc_manager->value - 0.01f));
                                }
                            }
                            break;
                            
                        case ACC_TYPE_HUMIDIFIER:
                            if (value_int < 0) {
                                update_humidif(ch_group->ch[4], HOMEKIT_UINT8(value_int + 3));
                            } else if (value_int <= 1) {
                                update_humidif(ch_group->ch[2], HOMEKIT_UINT8(value_int));
                            } else if (value_int <= 1000) {
                                update_humidif(ch_group->ch[5], HOMEKIT_FLOAT(value_int - 1000));
                            } else {    // if (value_int <= 2000)
                                update_humidif(ch_group->ch[6], HOMEKIT_FLOAT(value_int - 2000));
                            }
                            break;
                            
                        case ACC_TYPE_GARAGE_DOOR:
                            if (value_int < 2) {
                                hkc_garage_door_setter(GD_TARGET_DOOR_STATE, HOMEKIT_UINT8(value_int));
                            } else if (value_int == 2) {
                                garage_door_stop(99, ch_group, 0);
                            } else if (value_int == 5) {
                                hkc_garage_door_setter(GD_TARGET_DOOR_STATE, HOMEKIT_UINT8(!GD_TARGET_DOOR_STATE_INT));
                            } else if (value_int >= 10) {
                                garage_door_sensor(99, ch_group, value_int - 10);
                            } else {
                                garage_door_obstruction(99, ch_group, value_int - 3);
                            }
                            break;
                            
                        case ACC_TYPE_LIGHTBULB:
                            if (value_int > 1) {
                                if (value_int < 103) {              // BRI
                                    hkc_rgbw_setter(ch_group->ch[1], HOMEKIT_INT(value_int - 2));
                                    
                                } else if (value_int >= 3000) {     // TEMP
                                    hkc_rgbw_setter(ch_group->ch[2], HOMEKIT_UINT32(value_int - 3000));
                                    
                                } else if (value_int >= 2000) {     // SAT
                                    hkc_rgbw_setter(ch_group->ch[3], HOMEKIT_FLOAT(value_int - 2000));
                                    
                                } else if (value_int >= 1000) {     // HUE
                                    hkc_rgbw_setter(ch_group->ch[2], HOMEKIT_FLOAT(value_int - 1000));
                                } else if (value_int >= 600) {      // BRI+
                                    int new_bri = ch_group->ch[1]->value.int_value + (value_int - 600);
                                    if (new_bri > 100) {
                                        new_bri = 100;
                                    }
                                    hkc_rgbw_setter(ch_group->ch[1], HOMEKIT_INT(new_bri));
                                } else if (value_int >= 300) {      // BRI-
                                    int new_bri = ch_group->ch[1]->value.int_value - (value_int - 300);
                                    if (new_bri < 1) {
                                        new_bri = 1;
                                    }
                                    hkc_rgbw_setter(ch_group->ch[1], HOMEKIT_INT(new_bri));
                                }
                            } else if (value_int < 0) {
                                if (ch_group->ch[0]->value.bool_value) {
                                    lightbulb_group_t* lightbulb_group = lightbulb_group_find(ch_group->ch[0]);
                                    if (value_int == -1) {
                                        lightbulb_group->armed_autodimmer = true;
                                        autodimmer_call(ch_group->ch[0], HOMEKIT_BOOL(false));
                                    } else {    // action_acc_manager->value == -2
                                        lightbulb_group->autodimmer = 0;
                                    }
                                }
                            } else {
                                hkc_rgbw_setter(ch_group->ch[0], HOMEKIT_BOOL((bool) value_int));
                            }
                            break;
                            
                        case ACC_TYPE_WINDOW_COVER:
                            if (value_int < 0) {
                                window_cover_obstruction(0, ch_group, value_int + 2);
                                
                            } else if (value_int == 101) {
                                hkc_window_cover_setter(WINDOW_COVER_CH_TARGET_POSITION, WINDOW_COVER_CH_CURRENT_POSITION->value);
                                
                            } else if (value_int >= 200) {
                                WINDOW_COVER_HOMEKIT_POSITION = value_int - 200;
                                
                                WINDOW_COVER_CH_CURRENT_POSITION->value.int_value = WINDOW_COVER_HOMEKIT_POSITION;
                                WINDOW_COVER_MOTOR_POSITION = ((100 * WINDOW_COVER_CORRECTION * WINDOW_COVER_HOMEKIT_POSITION) + (5000 * WINDOW_COVER_HOMEKIT_POSITION)) / ((WINDOW_COVER_CORRECTION * WINDOW_COVER_HOMEKIT_POSITION) + 5000);
                                homekit_characteristic_notify_safe(WINDOW_COVER_CH_CURRENT_POSITION);
                                
                                if (WINDOW_COVER_CH_STATE->value.int_value == WINDOW_COVER_STOP) {
                                    WINDOW_COVER_CH_TARGET_POSITION->value.int_value = WINDOW_COVER_CH_CURRENT_POSITION->value.int_value;
                                    homekit_characteristic_notify_safe(WINDOW_COVER_CH_TARGET_POSITION);
                                }
                                
                            } else {
                                hkc_window_cover_setter(WINDOW_COVER_CH_TARGET_POSITION, HOMEKIT_UINT8(value_int));
                            }
                            break;
                            
                        case ACC_TYPE_FAN:
                            if (value_int == 0) {
                                hkc_fan_setter(ch_group->ch[0], HOMEKIT_BOOL(false));
                            } else if (value_int == -201) {
                                esp_timer_start(ch_group->timer2);
                            } else if (value_int < -100) {
                                const float new_value = ch_group->ch[1]->value.float_value + action_acc_manager->value + 100;
                                if (new_value < *ch_group->ch[1]->min_value) {
                                    hkc_fan_speed_setter(ch_group->ch[1], HOMEKIT_FLOAT(*ch_group->ch[1]->min_value));
                                } else {
                                    hkc_fan_speed_setter(ch_group->ch[1], HOMEKIT_FLOAT(new_value));
                                }
                            } else if (value_int < 0) {
                                const float new_value = ch_group->ch[1]->value.float_value - action_acc_manager->value;
                                if (new_value > *ch_group->ch[1]->max_value) {
                                    hkc_fan_speed_setter(ch_group->ch[1], HOMEKIT_FLOAT(*ch_group->ch[1]->max_value));
                                } else {
                                    hkc_fan_speed_setter(ch_group->ch[1], HOMEKIT_FLOAT(new_value));
                                }
                            } else if (value_int > 100) {
                                hkc_fan_setter(ch_group->ch[0], HOMEKIT_BOOL(true));
                            } else {
                                hkc_fan_speed_setter(ch_group->ch[1], HOMEKIT_FLOAT(action_acc_manager->value));
                            }
                            break;
                            
                        case ACC_TYPE_SECURITY_SYSTEM:
                            if (value_int >= 14) {
                                alarm_recurrent = true;
                                value_int -= 10;
                            }
                            
                            if (value_int == 8) {
                                SEC_SYSTEM_CH_CURRENT_STATE->value.int_value = SEC_SYSTEM_CH_TARGET_STATE->value.int_value;
                                do_actions(ch_group, 8);
                                homekit_characteristic_notify_safe(SEC_SYSTEM_CH_CURRENT_STATE);
                                save_historical_data(SEC_SYSTEM_CH_CURRENT_STATE);
                                
                            } else if ((value_int == 4 && SEC_SYSTEM_CH_TARGET_STATE->value.int_value != SEC_SYSTEM_OFF) ||
                                       SEC_SYSTEM_CH_TARGET_STATE->value.int_value == value_int - 5) {
                                SEC_SYSTEM_CH_CURRENT_STATE->value.int_value = 4;
                                do_actions(ch_group, value_int);
                                homekit_characteristic_notify_safe(SEC_SYSTEM_CH_CURRENT_STATE);
                                save_historical_data(SEC_SYSTEM_CH_CURRENT_STATE);
                                if (alarm_recurrent) {
                                    INFO("<%i> Recurrent alarm", ch_group->accessory);
                                    esp_timer_start(SEC_SYSTEM_REC_ALARM_TIMER);
                                }
                                
                            } else if (value_int >= 10) {
                                hkc_sec_system_status(SEC_SYSTEM_CH_TARGET_STATE, HOMEKIT_UINT8(value_int - 10));
                                
                            } else if (value_int <= 3) {
                                hkc_sec_system(SEC_SYSTEM_CH_TARGET_STATE, HOMEKIT_UINT8(value_int));
                            }
                            break;
                            
                        case ACC_TYPE_TV:
                            if (value_int == -1 || value_int == -2) {
                                hkc_tv_status_active(ch_group->ch[0], HOMEKIT_UINT8(value_int + 2));
                            } else if (value_int == 0 || value_int == 1) {
                                hkc_tv_active(ch_group->ch[0], HOMEKIT_UINT8(value_int));
                            } else if (value_int < 20) {
                                hkc_tv_key(ch_group->ch[3], HOMEKIT_UINT8(value_int - 2));
                            } else if (value_int < 22) {
                                hkc_tv_mute(ch_group->ch[5], HOMEKIT_BOOL((bool) (value_int - 20)));
                            } else if (value_int < 24) {
                                hkc_tv_volume(ch_group->ch[6], HOMEKIT_UINT8(value_int - 22));
                            } else if (value_int < 32) {
                                hkc_tv_power_mode(ch_group->ch[4], HOMEKIT_UINT8(value_int - 30));
                            } else if (value_int > 100) {
                                hkc_tv_active_identifier(ch_group->ch[2], HOMEKIT_UINT8(value_int - 100));
                            }
                            break;
                            
                        case ACC_TYPE_POWER_MONITOR:
                            if (value_int == 0) {
                                pm_custom_consumption_reset(ch_group);
                            }
                            break;
                            
                        case ACC_TYPE_HISTORICAL:
                            if (value_int == 0) {
                                save_historical_data(ch_group->ch_hist);
                            }
                            break;
                            
                        default:    // ON Type ch
                            if (value_int < 0) {
                                if (value_int == -1) {
                                    esp_timer_start(ch_group->timer2);
                                }
                                
                                if (ch_group->chs > 1) {
                                    if (value_int < -1) {
                                        hkc_setter(ch_group->ch[1], HOMEKIT_UINT32(- value_int - 1));
                                    }
                                    if (value_int == -1 || ch_group->ch[2]->value.int_value > ch_group->ch[1]->value.int_value) {
                                        ch_group->ch[2]->value.int_value = ch_group->ch[1]->value.int_value;
                                    }
                                }
                            } else if (value_int == 4) {
                                hkc_on_setter(ch_group->ch[0], HOMEKIT_BOOL(!ch_group->ch[0]->value.bool_value));
                            } else if (value_int == 5) {
                                hkc_on_status_setter(ch_group->ch[0], HOMEKIT_BOOL(!ch_group->ch[0]->value.bool_value));
                            } else if (value_int > 1) {
                                hkc_on_status_setter(ch_group->ch[0], HOMEKIT_BOOL((bool) (value_int - 2)));
                            } else {
                                hkc_on_setter(ch_group->ch[0], HOMEKIT_BOOL((bool) value_int));
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
            INFO("<%i> System Action %i", ch_group->accessory, action_system->value);
            switch (action_system->value) {
                case SYSTEM_ACTION_SETUP_MODE:
                    setup_mode_call(99, NULL, 0);
                    break;
                    
                case SYSTEM_ACTION_OTA_UPDATE:
                    rboot_set_temp_rom(1);
                    reboot_haa();
                    break;
                    
                case SYSTEM_ACTION_WIFI_RECONNECTION:
                    sdk_wifi_station_disconnect();
                    break;
                    
                case SYSTEM_ACTION_WIFI_RECONNECTION_2:
                    wifi_config_reset();
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
        
        if (xTaskCreate(uart_action_task, "uart", UART_ACTION_TASK_SIZE, action_task, UART_ACTION_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("<%i> Creating uart", ch_group->accessory);
            free(action_task);
            homekit_remove_oldest_client();
        }
    }
    
    // Network actions
    if (ch_group->action_network && main_config.wifi_status == WIFI_STATUS_CONNECTED) {
        action_task_t* action_task = new_action_task();
        action_task->action = action;
        action_task->ch_group = ch_group;
        
        if (xTaskCreate(net_action_task, "net", NETWORK_ACTION_TASK_SIZE, action_task, NETWORK_ACTION_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("<%i> Creating net", ch_group->accessory);
            free(action_task);
            homekit_remove_oldest_client();
        }
    }
    
    // IR TX actions
    if (ch_group->action_ir_tx) {
        action_task_t* action_task = new_action_task();
        action_task->action = action;
        action_task->ch_group = ch_group;
        
        if (xTaskCreate(ir_tx_task, "ir", IR_TX_TASK_SIZE, action_task, IR_TX_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("<%i> Creating ir", ch_group->accessory);
            free(action_task);
            homekit_remove_oldest_client();
        }
    }
}

void do_wildcard_actions(ch_group_t* ch_group, uint8_t index, const float action_value) {
    INFO("<%i> Wildcard %i %1.7g", ch_group->accessory, index, action_value);
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
            INFO("<%i> Wilcard Action %i %.2f", ch_group->accessory, index, last_value);
            do_actions(ch_group, last_wildcard_action->target_action);
        }
    }
}

void timetable_actions_timer_worker(TimerHandle_t xTimer) {
    if (!main_config.clock_ready) {
        return;
    }
    
    struct tm* timeinfo;
    time_t time = raven_ntp_get_time_t();
    timeinfo = localtime(&time);
    
    if (!main_config.timetable_ready) {
        if (timeinfo->tm_sec == 0) {
            esp_timer_change_period(xTimer, 60000);
            main_config.timetable_ready = true;
        } else {
            return;
        }
    }
    
    timetable_action_t* timetable_action = main_config.timetable_actions;
    while (timetable_action) {
        if (
            (timetable_action->mon  == ALL_MONS  || timetable_action->mon  == timeinfo->tm_mon ) &&
            (timetable_action->mday == ALL_MDAYS || timetable_action->mday == timeinfo->tm_mday) &&
            (timetable_action->hour == ALL_HOURS || timetable_action->hour == timeinfo->tm_hour) &&
            (timetable_action->min  == ALL_MINS  || timetable_action->min  == timeinfo->tm_min ) &&
            (timetable_action->wday == ALL_WDAYS || timetable_action->wday == timeinfo->tm_wday)
            ) {
            
            do_actions(ch_group_find_by_acc(ACC_TYPE_ROOT_DEVICE), timetable_action->action);
        }
        
        timetable_action = timetable_action->next;
    }
    
    if (timeinfo->tm_hour == 23 &&
        timeinfo->tm_min  == 59 &&
        timeinfo->tm_sec  != 0) {
        esp_timer_change_period(xTimer, 1000);
        main_config.timetable_ready = false;
    }
}

// --- IDENTIFY
void identify(homekit_characteristic_t* ch, const homekit_value_t value) {
    led_blink(6);
    INFO("ID");
}

// ---------

void delayed_sensor_task() {
    vTaskDelay(MS_TO_TICKS(3000));
    
    ch_group_t* ch_group = main_config.ch_groups;
    while (ch_group) {
        if (ch_group->timer &&
            ch_group->acc_type >= ACC_TYPE_THERMOSTAT &&
            ch_group->acc_type <= ACC_TYPE_HUMIDIFIER_WITH_TEMP) {
            
            INFO("<%i> Starting TH sensor", ch_group->accessory);
            
            temperature_timer_worker(ch_group->timer);
            esp_timer_start(ch_group->timer);
            
            vTaskDelay(MS_TO_TICKS(4000));
        }
        
        ch_group = ch_group->next;
    }
    
    ch_group = main_config.ch_groups;
    while (ch_group) {
        if (ch_group->acc_type == ACC_TYPE_IAIRZONING &&
            ch_group->timer) {
            
            INFO("<%i> Starting iAirZoning", ch_group->accessory);
            
            temperature_timer_worker(ch_group->timer);
            esp_timer_start(ch_group->timer);
            
            vTaskDelay(MS_TO_TICKS(9500));
        }
        
        ch_group = ch_group->next;
    }
    
    vTaskDelete(NULL);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, NULL);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER, "José A. Jiménez Campos");
homekit_characteristic_t model = HOMEKIT_CHARACTERISTIC_(MODEL, "RavenSystem HAA Griffon");
homekit_characteristic_t identify_function = HOMEKIT_CHARACTERISTIC_(IDENTIFY, identify);
homekit_characteristic_t firmware = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, FIRMWARE_VERSION);
homekit_characteristic_t hap_version = HOMEKIT_CHARACTERISTIC_(VERSION, "1.1.0");
homekit_characteristic_t haa_ota_update = HOMEKIT_CHARACTERISTIC_(CUSTOM_OTA_UPDATE, "", .setter_ex=hkc_custom_ota_setter);
homekit_characteristic_t haa_enable_setup = HOMEKIT_CHARACTERISTIC_(CUSTOM_ENABLE_SETUP, "", .setter_ex=hkc_custom_setup_setter);

homekit_server_config_t config;

void run_homekit_server() {
    main_config.wifi_channel = sdk_wifi_get_channel();
    main_config.wifi_status = WIFI_STATUS_CONNECTED;
    main_config.wifi_ip = wifi_config_get_ip();
    
    FREEHEAP();
    
    if (main_config.enable_homekit_server) {
        homekit_server_init(&config);
        FREEHEAP();
    }
    
    if (!main_config.ntp_host) {
        struct ip_info info;
        if (sdk_wifi_get_ip_info(STATION_IF, &info)) {
            char gw_host[16];
            snprintf(gw_host, 16, IPSTR, IP2STR(&info.gw));
            main_config.ntp_host = strdup(gw_host);
        }
    }
    
    if (main_config.ntp_host) {
        ntp_timer_worker(NULL);
        esp_timer_start(esp_timer_create(NTP_POLL_PERIOD_MS, true, NULL, ntp_timer_worker));
        
        if (main_config.timetable_actions) {
            esp_timer_start(esp_timer_create(1000, true, NULL, timetable_actions_timer_worker));
        }
    }
    
    vTaskDelay(MS_TO_TICKS(500));
    
    do_actions(ch_group_find_by_acc(ACC_TYPE_ROOT_DEVICE), 2);
    
    led_blink(4);
    
    vTaskDelay(MS_TO_TICKS(500));
    
    WIFI_WATCHDOG_TIMER = esp_timer_create(WIFI_WATCHDOG_POLL_PERIOD_MS, true, NULL, wifi_watchdog);
    
    while (esp_timer_start(WIFI_WATCHDOG_TIMER) == TIMER_HELPER_ERR_NO_PROCESS) {
        vTaskDelay(MS_TO_TICKS(1000));
    }
    
    if (main_config.ping_inputs) {
        esp_timer_start(esp_timer_create(main_config.ping_poll_period * 1000.00f, true, NULL, ping_task_timer_worker));
    }
}

void printf_header() {
    INFO("\n\n");
    INFO("Home Accessory Architect v"FIRMWARE_VERSION);
    INFO("(c) 2018-2021 José Antonio Jiménez Campos\n");
    
#ifdef HAA_DEBUG
    INFO("HAA DEBUG ENABLED\n");
#endif  // HAA_DEBUG
}

void reset_uart() {
    gpio_disable(1);
    iomux_set_pullup_flags(5, 0);
    iomux_set_function(5, IOMUX_GPIO1_FUNC_UART0_TXD);
    uart_set_baud(0, 115200);
}

void normal_mode_init() {
    char* txt_config = NULL;
    sysparam_get_string(HAA_JSON_SYSPARAM, &txt_config);

    cJSON* json_haa = cJSON_Parse(txt_config);

    cJSON* json_config = cJSON_GetObjectItemCaseSensitive(json_haa, GENERAL_CONFIG);
    cJSON* json_accessories = cJSON_GetObjectItemCaseSensitive(json_haa, ACCESSORIES_ARRAY);
    
    const uint8_t total_accessories = cJSON_GetArraySize(json_accessories);
    
    if (total_accessories == 0) {
        sysparam_set_int32(TOTAL_SERV_SYSPARAM, 0);
        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 2);
        
        vTaskDelay(MS_TO_TICKS(200));
        sdk_system_restart();
    }
    
    // Buttons GPIO Setup function
    bool diginput_register(cJSON* json_buttons, void* callback, ch_group_t* ch_group, const uint8_t param) {
        bool active = false;
        
        for (uint8_t j = 0; j < cJSON_GetArraySize(json_buttons); j++) {
            const uint16_t gpio = (uint16_t) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), PIN_GPIO)->valuedouble;
            
            bool inverted = false;
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), INVERTED) != NULL &&
                cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), INVERTED)->valuedouble == 1) {
                inverted = true;
            }
            
            uint8_t button_filter = 0;
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), BUTTON_FILTER) != NULL) {
                button_filter = (uint8_t) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), BUTTON_FILTER)->valuedouble;
            }
            
            if (gpio < MAX_GPIOS) {
                if (!get_used_gpio(gpio)) {
                    set_used_gpio(gpio);
                    
                    bool pullup_resistor = true;
                    if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), PULLUP_RESISTOR) != NULL &&
                        cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), PULLUP_RESISTOR)->valuedouble == 0) {
                        pullup_resistor = false;
                    }
                    
                    uint8_t button_mode = 0;
                    if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), BUTTON_MODE) != NULL) {
                        button_mode = (uint8_t) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), BUTTON_MODE)->valuedouble;
                    }
                    
                    adv_button_create(gpio, pullup_resistor, inverted, button_mode);
                    
                    INFO("New Input GPIO %i: inv %i, filter %i, mode %i", gpio, inverted, button_filter, button_mode);
                }
                
            } else {    // MCP23017
                mcp23017_t* mcp = mcp_find_by_index(gpio / 100);
                adv_button_create(gpio, mcp->bus, inverted, mcp->addr);
                
                INFO("New Input MCP GPIO %i: inv %i, filter %i, bus %i, addr %i", gpio, inverted, button_filter, mcp->bus, mcp->addr);
            }
            
            if (button_filter > 0) {
                adv_button_set_gpio_probes(gpio, button_filter);
            }
            
            uint8_t button_type = 1;
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), BUTTON_PRESS_TYPE) != NULL) {
                button_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), BUTTON_PRESS_TYPE)->valuedouble;
            }
            
            adv_button_register_callback_fn(gpio, callback, button_type, (void*) ch_group, param);
            
            if (adv_button_read_by_gpio(gpio) == button_type) {
                active = true;
            }
            
            INFO("New DigI GPIO %i: type %i, status %i", gpio, button_type, active);
        }
        
        return active;
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
                int saved_state_int;
                char* saved_state_string = NULL;
                
                switch (ch_type) {
                    case CH_TYPE_INT8:
                        status = sysparam_get_int8(saved_state_id, &saved_state_int8);
                        
                        if (status == SYSPARAM_OK) {
                            state = saved_state_int8;
                        }
                        break;
                        
                    case CH_TYPE_INT:
                        status = sysparam_get_int32(saved_state_id, &saved_state_int);
                        
                        if (status == SYSPARAM_OK) {
                            state = saved_state_int;
                        }
                        break;
                        
                    case CH_TYPE_FLOAT:
                        status = sysparam_get_int32(saved_state_id, &saved_state_int);
                        
                        if (status == SYSPARAM_OK) {
                            state = saved_state_int / FLOAT_FACTOR_SAVE_AS_INT;
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
    
    // Binary outputs
    inline void new_action_binary_output(ch_group_t* ch_group, cJSON* json_context, uint8_t fixed_action) {
        action_binary_output_t* last_action = ch_group->action_binary_output;
        
        void register_action(cJSON* json_accessory, uint8_t new_int_action) {
            char action[3];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), BINARY_OUTPUTS_ARRAY) != NULL) {
                    cJSON* json_relays = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), BINARY_OUTPUTS_ARRAY);
                    for (int16_t i = cJSON_GetArraySize(json_relays) - 1; i >= 0; i--) {
                        action_binary_output_t* action_binary_output = malloc(sizeof(action_binary_output_t));
                        memset(action_binary_output, 0, sizeof(*action_binary_output));
                        
                        cJSON* json_relay = cJSON_GetArrayItem(json_relays, i);
                        
                        action_binary_output->action = new_int_action;
                        
                        action_binary_output->value = false;
                        if (cJSON_GetObjectItemCaseSensitive(json_relay, VALUE) != NULL) {
                            action_binary_output->value = (bool) cJSON_GetObjectItemCaseSensitive(json_relay, VALUE)->valuedouble;
                        }
                        
                        action_binary_output->gpio = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_relay, PIN_GPIO)->valuedouble;
                        if (!get_used_gpio(action_binary_output->gpio) && action_binary_output->gpio < MAX_GPIOS) {
                            bool initial_value = false;
                            if (cJSON_GetObjectItemCaseSensitive(json_relay, INITIAL_VALUE) != NULL) {
                                initial_value = (bool) cJSON_GetObjectItemCaseSensitive(json_relay, INITIAL_VALUE)->valuedouble;
                            }
                            
                            set_used_gpio(action_binary_output->gpio);
                            gpio_write(action_binary_output->gpio, initial_value);
                            gpio_enable(action_binary_output->gpio, GPIO_OUTPUT);
                            gpio_write(action_binary_output->gpio, initial_value);
                            
                            INFO("New Binary Output GPIO %i", action_binary_output->gpio);
                        }

                        action_binary_output->inching = 0;
                        if (cJSON_GetObjectItemCaseSensitive(json_relay, AUTOSWITCH_TIME) != NULL) {
                            action_binary_output->inching = (float) cJSON_GetObjectItemCaseSensitive(json_relay, AUTOSWITCH_TIME)->valuedouble;
                        }
                        
                        action_binary_output->next = last_action;
                        last_action = action_binary_output;
                        
                        INFO("New Binary Output Action %i: gpio %i, val %i, inch %g", new_int_action, action_binary_output->gpio, action_binary_output->value, action_binary_output->inching);
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
        
        ch_group->action_binary_output = last_action;
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

                        action_acc_manager->action = new_int_action;
                        action_acc_manager->value = 0;
                        
                        action_acc_manager->next = last_action;
                        last_action = action_acc_manager;
                        
                        cJSON* json_acc_manager = cJSON_GetArrayItem(json_acc_managers, i);
                        for (uint8_t j = 0; j < cJSON_GetArraySize(json_acc_manager); j++) {
                            const float value = (float) cJSON_GetArrayItem(json_acc_manager, j)->valuedouble;
                            int16_t acc_index;
                            
                            switch (j) {
                                case 0:
                                    acc_index = value;
                                    if (acc_index <= 0) {
                                        action_acc_manager->accessory = ch_group->accessory + acc_index;
                                    } else if (acc_index > 7000) {
                                        action_acc_manager->accessory = ch_group->accessory + (acc_index - 7000);
                                    } else {
                                        action_acc_manager->accessory = acc_index;
                                    }
                                    break;
                                    
                                case 1:
                                    action_acc_manager->value = value;
                                    break;
                            }
                        }
                        
                        INFO("New Serv Manager Act %i: ser %i, val %g", new_int_action, action_acc_manager->accessory, action_acc_manager->value);
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
    
    // Network Actions
    inline void new_action_network(ch_group_t* ch_group, cJSON* json_context, uint8_t fixed_action) {
        action_network_t* last_action = ch_group->action_network;
        
        void register_action(cJSON* json_accessory, uint8_t new_int_action) {
            char action[3];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), NETWORK_ACTIONS_ARRAY) != NULL) {
                    cJSON* json_action_networks = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), NETWORK_ACTIONS_ARRAY);
                    for (int16_t i = cJSON_GetArraySize(json_action_networks) - 1; i >= 0; i--) {
                        action_network_t* action_network = malloc(sizeof(action_network_t));
                        memset(action_network, 0, sizeof(*action_network));
                        
                        cJSON* json_action_network = cJSON_GetArrayItem(json_action_networks, i);
                        
                        action_network->action = new_int_action;
                        
                        action_network->host = strdup(cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_HOST)->valuestring);
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_URL) != NULL) {
                            action_network->url = strdup(cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_URL)->valuestring);
                        } else {
                            action_network->url = strdup("");
                        }
                        
                        action_network->port_n = 80;
                        if (cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_PORT) != NULL) {
                            action_network->port_n = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_PORT)->valuedouble;
                        }
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_WAIT_RESPONSE_SET) != NULL) {
                            action_network->wait_response = (bool) cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_WAIT_RESPONSE_SET)->valuedouble;
                        }
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_METHOD) != NULL) {
                            action_network->method_n = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_METHOD)->valuedouble;
                        }
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_HEADER) != NULL) {
                            action_network->header = strdup(cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_HEADER)->valuestring);
                        } else {
                            action_network->header = strdup("Content-type: text/html\r\n");
                        }
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_CONTENT) != NULL) {
                            action_network->content = strdup(cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_CONTENT)->valuestring);
                        } else {
                            action_network->content = strdup("");
                        }
                        
                        INFO("New Network Action %i: %s:%i", new_int_action, action_network->host, action_network->port_n);
                        
                        if (action_network->method_n ==  3 ||
                            action_network->method_n == 13) {
                            action_network->len = strlen(action_network->content);
                        } else if (action_network->method_n ==  4 ||
                                   action_network->method_n == 12 ||
                                   action_network->method_n == 14) {
                            
                            free(action_network->content);
                            action_network->len = process_hexstr(cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_CONTENT)->valuestring, &action_network->raw);
                        }
                        
                        action_network->next = last_action;
                        last_action = action_network;
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
        
        ch_group->action_network = last_action;
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
                        
                        action_ir_tx->pause = MS_TO_TICKS(100);
                        if (cJSON_GetObjectItemCaseSensitive(json_action_ir_tx, IR_ACTION_REPEATS_PAUSE) != NULL) {
                            action_ir_tx->pause = MS_TO_TICKS(cJSON_GetObjectItemCaseSensitive(json_action_ir_tx, IR_ACTION_REPEATS_PAUSE)->valuedouble);
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
                            action_uart->pause = MS_TO_TICKS(cJSON_GetObjectItemCaseSensitive(json_action_uart, UART_ACTION_PAUSE)->valuedouble);
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
        new_action_binary_output(ch_group, json_accessory, fixed_action);
        new_action_acc_manager(ch_group, json_accessory, fixed_action);
        new_action_system(ch_group, json_accessory, fixed_action);
        new_action_network(ch_group, json_accessory, fixed_action);
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
    
    // REGISTER SERVICE CONFIGURATION
    uint8_t acc_homekit_enabled(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, ENABLE_HOMEKIT) != NULL) {
            return (uint8_t) cJSON_GetObjectItemCaseSensitive(json_accessory, ENABLE_HOMEKIT)->valuedouble;
        }
        return 1;
    }
    
    TimerHandle_t autoswitch_time(cJSON* json_accessory, ch_group_t* ch_group) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, AUTOSWITCH_TIME) != NULL) {
            const uint32_t time = cJSON_GetObjectItemCaseSensitive(json_accessory, AUTOSWITCH_TIME)->valuedouble * 1000.f;
            if (time > 0) {
                return esp_timer_create(time, false, (void*) ch_group, hkc_autooff_setter_task);
            }
        }
        return NULL;
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
    
    float th_update_delay(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, THERMOSTAT_UPDATE_DELAY) != NULL) {
            float th_delay = (float) cJSON_GetObjectItemCaseSensitive(json_accessory, THERMOSTAT_UPDATE_DELAY)->valuedouble;
            if (th_delay < THERMOSTAT_UPDATE_DELAY_MIN) {
                return THERMOSTAT_UPDATE_DELAY_MIN;
            }
            return th_delay;
        }
        return THERMOSTAT_UPDATE_DELAY_DEFAULT;
    }
    
    float th_sensor(ch_group_t* ch_group, cJSON* json_accessory) {
        TH_SENSOR_GPIO = th_sensor_gpio(json_accessory);
        TH_SENSOR_TYPE = th_sensor_type(json_accessory);
        TH_SENSOR_TEMP_OFFSET = th_sensor_temp_offset(json_accessory);
        TH_SENSOR_HUM_OFFSET = th_sensor_hum_offset(json_accessory);
        TH_SENSOR_ERROR_COUNT = 0;
        if ((uint8_t) TH_SENSOR_TYPE == 3) {
            TH_SENSOR_INDEX = th_sensor_index(json_accessory);
        }
        
        return sensor_poll_period(json_accessory, TH_SENSOR_POLL_PERIOD_DEFAULT);
    }
    
    void th_sensor_starter(ch_group_t* ch_group, float poll_period) {
        ch_group->timer = esp_timer_create(poll_period * 1000, true, (void*) ch_group, temperature_timer_worker);
    }
    
    uint8_t virtual_stop(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, VIRTUAL_STOP_SET) != NULL) {
            return (uint8_t) cJSON_GetObjectItemCaseSensitive(json_accessory, VIRTUAL_STOP_SET)->valuedouble;
        }
        return VIRTUAL_STOP_DEFAULT;
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
                
                switch (uart_config) {
                    case 1:
                        set_used_gpio(2);
                        gpio_disable(2);
                        gpio_set_iomux_function(2, IOMUX_GPIO2_FUNC_UART1_TXD);
                        break;
                        
                    case 2:
                        set_used_gpio(15);
                        gpio_disable(15);
                        sdk_system_uart_swap();
                        uart_config = 0;
                        break;
                        
                    default:    // case 0:
                        set_used_gpio(1);
                        reset_uart();
                        break;
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
        
        if ((log_output_type == 1 || log_output_type == 4 || log_output_type == 7) && !used_uart[0]) {
            set_used_gpio(1);
            reset_uart();
        } else if ((log_output_type == 2 || log_output_type == 5 || log_output_type == 8) && !used_uart[1]) {
            set_used_gpio(2);
            gpio_disable(2);
            gpio_set_iomux_function(2, IOMUX_GPIO2_FUNC_UART1_TXD);
            uart_set_baud(1, 115200);
        }
    }
    
    char* log_output_target = NULL;
    if (cJSON_GetObjectItemCaseSensitive(json_config, LOG_OUTPUT_TARGET) != NULL) {
        log_output_target = strdup(cJSON_GetObjectItemCaseSensitive(json_config, LOG_OUTPUT_TARGET)->valuestring);
    }
    
    adv_logger_init(log_output_type, log_output_target);
    free(log_output_target);

    printf_header();
    INFO("NORMAL MODE\n\nJSON:\n %s\n", txt_config);
    
    free(txt_config);

    // Custom Hostname
    char* custom_hostname = name.value.string_value;
    if (cJSON_GetObjectItemCaseSensitive(json_config, CUSTOM_HOSTNAME) != NULL) {
        custom_hostname = strdup(cJSON_GetObjectItemCaseSensitive(json_config, CUSTOM_HOSTNAME)->valuestring);
    }
    INFO("Hostname: %s", custom_hostname);
    
    // Custom NTP Host
    if (cJSON_GetObjectItemCaseSensitive(json_config, CUSTOM_NTP_HOST) != NULL) {
        main_config.ntp_host = strdup(cJSON_GetObjectItemCaseSensitive(json_config, CUSTOM_NTP_HOST)->valuestring);
        INFO("NTP Host %s", main_config.ntp_host);
    }
    
    // Timezone
    if (cJSON_GetObjectItemCaseSensitive(json_config, TIMEZONE) != NULL) {
        char* timezone = strdup(cJSON_GetObjectItemCaseSensitive(json_config, TIMEZONE)->valuestring);
        INFO("Timezone %s", timezone);
        setenv("TZ", timezone, 1);
        tzset();
        free(timezone);
    }
    
    // I2C Bus
    if (cJSON_GetObjectItemCaseSensitive(json_config, I2C_CONFIG_ARRAY) != NULL) {
        cJSON* json_i2cs = cJSON_GetObjectItemCaseSensitive(json_config, I2C_CONFIG_ARRAY);
        for (uint8_t i = 0; i < cJSON_GetArraySize(json_i2cs); i++) {
            cJSON* json_i2c = cJSON_GetArrayItem(json_i2cs, i);

            uint8_t i2c_scl_gpio = (uint8_t) cJSON_GetArrayItem(json_i2c, 0)->valuedouble;
            if (get_used_gpio(i2c_scl_gpio)) {
                ERROR("I2C bus %i: scl %i is used", i, i2c_scl_gpio);
                i2c_scl_gpio = MAX_GPIOS;
                break;
            }
            set_used_gpio(i2c_scl_gpio);

            uint8_t i2c_sda_gpio = (uint8_t) cJSON_GetArrayItem(json_i2c, 1)->valuedouble;
            if (get_used_gpio(i2c_sda_gpio)) {
                ERROR("I2C bus %i: sda %i is used", i, i2c_sda_gpio);
                i2c_sda_gpio = MAX_GPIOS;
                break;
            }
            set_used_gpio(i2c_sda_gpio);

            uint32_t i2c_freq_hz = (uint32_t) (cJSON_GetArrayItem(json_i2c, 2)->valuedouble * 1000.f);
            
            if (i2c_scl_gpio < MAX_GPIOS &&
                i2c_sda_gpio < MAX_GPIOS &&
                i2c_freq_hz > 0) {
                const uint32_t i2c_res = i2c_init_hz(i, i2c_scl_gpio, i2c_sda_gpio, i2c_freq_hz);
                INFO("I2C bus %i: scl %i, sda %i, freq %i, res %i", i, i2c_scl_gpio, i2c_sda_gpio, i2c_freq_hz, i2c_res);
            } else {
                ERROR("I2C JSON");
            }
        }
    }
    
    // MCP23017 Interfaces
    if (cJSON_GetObjectItemCaseSensitive(json_config, MCP23017_ARRAY) != NULL) {
        cJSON* json_mcp23017s = cJSON_GetObjectItemCaseSensitive(json_config, MCP23017_ARRAY);
        for (uint8_t i = 0; i < cJSON_GetArraySize(json_mcp23017s); i++) {
            cJSON* json_mcp23017 = cJSON_GetArrayItem(json_mcp23017s, i);
            
            mcp23017_t* mcp23017 = malloc(sizeof(mcp23017_t));
            memset(mcp23017, 0, sizeof(*mcp23017));

            mcp23017->next = main_config.mcp23017s;
            main_config.mcp23017s = mcp23017;
            
            mcp23017->index = i + 1;

            mcp23017->bus = (uint8_t) cJSON_GetArrayItem(json_mcp23017, 0)->valuedouble;
            mcp23017->addr = (uint8_t) cJSON_GetArrayItem(json_mcp23017, 1)->valuedouble;
            
            INFO("MCP23017 %i: bus %i, addr %i", mcp23017->index, mcp23017->bus, mcp23017->addr);
            
            const uint8_t byte_zeros = 0x00;
            const uint8_t byte_ones = 0xFF;
            
            // Full reset
            uint8_t mcp_reg;
            for (mcp_reg = 0x00; mcp_reg < 0x02; mcp_reg++) {
                i2c_slave_write(mcp23017->bus, mcp23017->addr, &mcp_reg, 1, &byte_ones, 1);
            }
            for (mcp_reg = 0x02; mcp_reg < 0x16; mcp_reg++) {
                i2c_slave_write(mcp23017->bus, mcp23017->addr, &mcp_reg, 1, &byte_zeros, 1);
            }
            
            for (uint8_t channel = 0; channel < 2; channel++) {
                uint16_t mcp_mode = 258;    // Default set to INPUT and not pullup
                if (channel == 0) {
                    if (cJSON_GetArrayItem(json_mcp23017, 2) != NULL) {
                        mcp_mode = (uint16_t) cJSON_GetArrayItem(json_mcp23017, 2)->valuedouble;
                    }
                    
                    INFO("MCP23017 %i ChA: %i", mcp23017->index, mcp_mode);
                    
                } else {
                    if (cJSON_GetArrayItem(json_mcp23017, 3) != NULL) {
                        mcp_mode = (uint16_t) cJSON_GetArrayItem(json_mcp23017, 3)->valuedouble;
                    }
                    
                    INFO("MCP23017 %i ChB: %i", mcp23017->index, mcp_mode);
                }
                
                uint8_t reg = channel;
                if (mcp_mode > 255) {   // Mode INPUT
                    switch (mcp_mode) {
                        case 256:
                            // Pull-up HIGH
                            reg += 0x0C;
                            i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &byte_ones, 1);
                            // Polarity NORMAL
                            //reg = channel + 0x02;
                            //i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &byte_zeros, 1);
                            break;
                            
                        case 257:
                            // Pull-up HIGH
                            reg += 0x0C;
                            i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &byte_ones, 1);
                            // Polarity INVERTED
                            reg = channel + 0x02;
                            i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &byte_ones, 1);
                            break;
                            
                        case 259:
                            // Pull-up LOW
                            //reg += 0x0C;
                            //i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &byte_zeros, 1);
                            // Polarity INVERTED
                            reg = channel + 0x02;
                            i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &byte_ones, 1);
                            break;
                            
                        default:    // 258
                            // Pull-up LOW
                            //reg += 0x0C;
                            //i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &byte_zeros, 1);
                            // Polarity NORMAL
                            //reg = channel + 0x02;
                            //i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &byte_zeros, 1);
                            break;
                    }
                    
                } else {    // Mode OUTPUT
                    reg += 0x14;
                    i2c_slave_write(mcp23017->bus, mcp23017->addr, &channel, 1, &byte_zeros, 1);
                    
                    if (channel == 0) {
                        mcp23017->a_outs = mcp_mode;
                        i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &mcp23017->a_outs, 1);
                    } else {
                        mcp23017->b_outs = mcp_mode;
                        i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &mcp23017->b_outs, 1);
                    }
                }
            }
        }
    }
    
    // Timetable Actions
    if (cJSON_GetObjectItemCaseSensitive(json_config, TIMETABLE_ACTION_ARRAY) != NULL) {
        cJSON* json_timetable_actions = cJSON_GetObjectItemCaseSensitive(json_config, TIMETABLE_ACTION_ARRAY);
        for (uint8_t i = 0; i < cJSON_GetArraySize(json_timetable_actions); i++) {
            timetable_action_t* timetable_action = malloc(sizeof(timetable_action_t));
            memset(timetable_action, 0, sizeof(*timetable_action));
            timetable_action->mon  = ALL_MONS;
            timetable_action->mday = ALL_MDAYS;
            timetable_action->hour = ALL_HOURS;
            timetable_action->min  = ALL_MINS;
            timetable_action->wday = ALL_WDAYS;
            
            timetable_action->next = main_config.timetable_actions;
            main_config.timetable_actions = timetable_action;
            
            cJSON* json_timetable_action = cJSON_GetArrayItem(json_timetable_actions, i);
            for (uint8_t j = 0; j < cJSON_GetArraySize(json_timetable_action); j++) {
                const int8_t value = (int8_t) cJSON_GetArrayItem(json_timetable_action, j)->valuedouble;
                
                if (value >= 0) {
                    switch (j) {
                        case 0:
                            timetable_action->action = value;
                            break;
                            
                        case 1:
                            timetable_action->hour = value;
                            break;
                            
                        case 2:
                            timetable_action->min = value;
                            break;
                            
                        case 3:
                            timetable_action->wday = value;
                            break;
                            
                        case 4:
                            timetable_action->mday = value;
                            break;

                        case 5:
                            timetable_action->mon = value;
                            break;
                    }
                }
            }
            
            INFO("New Timetable Action %i: %ih %im, %i wday, %i mday, %i month", timetable_action->action, timetable_action->hour, timetable_action->min, timetable_action->wday, timetable_action->mday, timetable_action->mon);
        }
    }
    
    // Status LED
    if (cJSON_GetObjectItemCaseSensitive(json_config, STATUS_LED_GPIO) != NULL) {
        const uint8_t led_gpio = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_config, STATUS_LED_GPIO)->valuedouble;
        
        bool led_inverted = true;
        if (cJSON_GetObjectItemCaseSensitive(json_config, INVERTED) != NULL) {
            led_inverted = (bool) cJSON_GetObjectItemCaseSensitive(json_config, INVERTED)->valuedouble;
        }
        
        set_used_gpio(led_gpio);
        led_create(led_gpio, led_inverted);
        INFO("Status LED GPIO %i, inv %i", led_gpio, led_inverted);
    }
    
    // IR TX LED Frequency
    if (cJSON_GetObjectItemCaseSensitive(json_config, IR_ACTION_FREQ) != NULL) {
        main_config.ir_tx_freq = 1000 / cJSON_GetObjectItemCaseSensitive(json_config, IR_ACTION_FREQ)->valuedouble / 2;
        INFO("IR TX Freq %i", main_config.ir_tx_freq);
    }
    
    // IR TX LED Inverted
    if (cJSON_GetObjectItemCaseSensitive(json_config, IR_ACTION_TX_GPIO_INVERTED) != NULL) {
        main_config.ir_tx_inv = (bool) cJSON_GetObjectItemCaseSensitive(json_config, IR_ACTION_TX_GPIO_INVERTED)->valuedouble;
        INFO("IR TX Inv %i", main_config.ir_tx_inv);
    }
    
    // IR TX LED GPIO
    if (cJSON_GetObjectItemCaseSensitive(json_config, IR_ACTION_TX_GPIO) != NULL) {
        main_config.ir_tx_gpio = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, IR_ACTION_TX_GPIO)->valuedouble;
        set_used_gpio(main_config.ir_tx_gpio);
        gpio_enable(main_config.ir_tx_gpio, GPIO_OUTPUT);
        gpio_write(main_config.ir_tx_gpio, false ^ main_config.ir_tx_inv);
        INFO("IR TX GPIO %i", main_config.ir_tx_gpio);
    }
    
    // Button filter
    if (cJSON_GetObjectItemCaseSensitive(json_config, BUTTON_FILTER) != NULL) {
        uint8_t button_filter_value = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, BUTTON_FILTER)->valuedouble;
        adv_button_set_evaluate_delay(button_filter_value);
        INFO("Button filter %i", button_filter_value);
    }
    
    // PWM frequency
    if (cJSON_GetObjectItemCaseSensitive(json_config, PWM_FREQ) != NULL) {
        adv_pwm_set_freq((uint16_t) cJSON_GetObjectItemCaseSensitive(json_config, PWM_FREQ)->valuedouble);
    }
    
    // Ping poll period
    if (cJSON_GetObjectItemCaseSensitive(json_config, PING_POLL_PERIOD) != NULL) {
        main_config.ping_poll_period = (float) cJSON_GetObjectItemCaseSensitive(json_config, PING_POLL_PERIOD)->valuedouble;
    }
    INFO("Ping period %g secs", main_config.ping_poll_period);
    
    // Allowed Setup Mode Time
    if (cJSON_GetObjectItemCaseSensitive(json_config, ALLOWED_SETUP_MODE_TIME) != NULL) {
        main_config.setup_mode_time = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_config, ALLOWED_SETUP_MODE_TIME)->valuedouble;
    }
    INFO("Setup mode time %i secs", main_config.setup_mode_time);
    
    // HomeKit Server Clients
    config.max_clients = HOMEKIT_SERVER_MAX_CLIENTS_DEFAULT;
    if (cJSON_GetObjectItemCaseSensitive(json_config, HOMEKIT_SERVER_MAX_CLIENTS) != NULL) {
        config.max_clients = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, HOMEKIT_SERVER_MAX_CLIENTS)->valuedouble;
        if (config.max_clients > HOMEKIT_SERVER_MAX_CLIENTS_MAX) {
            config.max_clients = HOMEKIT_SERVER_MAX_CLIENTS_MAX;
        }
    }
    if (config.max_clients > 0) {
        main_config.enable_homekit_server = true;
    }
    INFO("HK Server Clients %i", config.max_clients);
    
    // Allow unsecure connections
    if (cJSON_GetObjectItemCaseSensitive(json_config, ALLOW_INSECURE_CONNECTIONS) != NULL) {
        config.insecure = (bool) cJSON_GetObjectItemCaseSensitive(json_config, ALLOW_INSECURE_CONNECTIONS)->valuedouble;
    }
    INFO("Unsecure connections %i", config.insecure);
    
    // mDNS TTL
    config.mdns_ttl = MDNS_TTL_DEFAULT;
    if (cJSON_GetObjectItemCaseSensitive(json_config, MDNS_TTL) != NULL) {
        config.mdns_ttl = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_config, MDNS_TTL)->valuedouble;
    }
    INFO("mDNS TTL %i", config.mdns_ttl);
    
    // Gateway Ping
    if (cJSON_GetObjectItemCaseSensitive(json_config, WIFI_PING_ERRORS) != NULL) {
        main_config.wifi_ping_max_errors = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, WIFI_PING_ERRORS)->valuedouble;
        INFO("Gateway Ping %i", main_config.wifi_ping_max_errors);
        main_config.wifi_ping_max_errors++;
    }
    
    // Common serial prefix
    char* common_serial_prefix = NULL;
    if (cJSON_GetObjectItemCaseSensitive(json_config, SERIAL_STRING) != NULL) {
        common_serial_prefix = cJSON_GetObjectItemCaseSensitive(json_config, SERIAL_STRING)->valuestring;
        INFO("Common serial prefix %s", common_serial_prefix);
    }
    
    // Times to toggle quickly an accessory status to enter setup mode
    if (cJSON_GetObjectItemCaseSensitive(json_config, SETUP_MODE_ACTIVATE_COUNT) != NULL) {
        main_config.setup_mode_toggle_counter_max = (int8_t) cJSON_GetObjectItemCaseSensitive(json_config, SETUP_MODE_ACTIVATE_COUNT)->valuedouble;
    }
    INFO("Toggles to enter setup %i", main_config.setup_mode_toggle_counter_max);
    
    if (main_config.setup_mode_toggle_counter_max > 0) {
        main_config.setup_mode_toggle_timer = esp_timer_create(SETUP_MODE_TOGGLE_TIME_MS, false, NULL, setup_mode_toggle);
    }
    
    // Buttons to enter setup mode
    diginput_register(cJSON_GetObjectItemCaseSensitive(json_config, BUTTONS_ARRAY), setup_mode_call, NULL, 0);
    
    // ----- END CONFIG SECTION
    
    uint8_t get_acc_type(cJSON* json_accessory) {
        uint8_t acc_type = ACC_TYPE_SWITCH;
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, ACCESSORY_TYPE) != NULL) {
            acc_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_accessory, ACCESSORY_TYPE)->valuedouble;
        }
        
        return acc_type;
    }
    
    uint16_t get_service_recount(const uint8_t acc_type, cJSON* json_context) {
        uint16_t service_recount = 1;
        
        switch (acc_type) {
            case ACC_TYPE_DOORBELL:
            case ACC_TYPE_THERMOSTAT_WITH_HUM:
            case ACC_TYPE_TH_SENSOR:
            case ACC_TYPE_HUMIDIFIER_WITH_TEMP:
                service_recount = 2;
                break;
                
            case ACC_TYPE_TV:
                service_recount = 2;
                cJSON* json_inputs = cJSON_GetObjectItemCaseSensitive(json_context, TV_INPUTS_ARRAY);
                uint8_t inputs = cJSON_GetArraySize(json_inputs);
                
                if (inputs == 0) {
                    inputs = 1;
                }
                
                service_recount += inputs;
                break;
                
            default:
                break;
        }
        
        return service_recount;
    }
    
    uint16_t get_total_services(const uint16_t acc_type, cJSON* json_accessory) {
        uint16_t total_services = 2 + get_service_recount(acc_type, json_accessory);
        
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, EXTRA_SERVICES_ARRAY) != NULL) {
            cJSON* json_extra_services = cJSON_GetObjectItemCaseSensitive(json_accessory, EXTRA_SERVICES_ARRAY);
            for (uint16_t i = 0; i < cJSON_GetArraySize(json_extra_services); i++) {
                cJSON* json_extra_service = cJSON_GetArrayItem(json_extra_services, i);
                total_services += get_service_recount(get_acc_type(json_extra_service), json_extra_service);
            }
        }
        
        return total_services;
    }
    
    uint16_t hk_total_ac = 1;
    bool bridge_needed = false;

    for (uint16_t i = 0; i < total_accessories; i++) {
        cJSON* json_accessory = cJSON_GetArrayItem(json_accessories, i);
        if (acc_homekit_enabled(json_accessory) && get_acc_type(json_accessory) != ACC_TYPE_IAIRZONING) {
            hk_total_ac += 1;
        }
    }
    
    if (hk_total_ac > (ACCESSORIES_WITHOUT_BRIDGE + 1)) {
        // Bridge needed
        bridge_needed = true;
        hk_total_ac += 1;
    }
    
    homekit_accessory_t** accessories = calloc(hk_total_ac, sizeof(homekit_accessory_t*));
    
    // Define services and characteristics
    uint16_t service_numerator = 0;
    uint16_t acc_count = 0;
    
    // HomeKit last config number
    int last_config_number = 1;
    sysparam_get_int32(LAST_CONFIG_NUMBER_SYSPARAM, &last_config_number);
    
    //uint16_t service_iid = 100;
    
    void new_accessory(const uint16_t accessory, uint16_t services, const bool homekit_enabled, cJSON* json_context) {
        if (!homekit_enabled) {
            return;
        }

        if (acc_count == 0) {
            services += 2;
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
        
        uint16_t serial_index = accessory + 1;
        if (bridge_needed) {
            serial_index--;
        }
        if (use_config_number) {
            snprintf(serial_str, serial_prefix_len, "%i-%02X%02X%02X-%i", last_config_number, macaddr[3], macaddr[4], macaddr[5], serial_index);
        } else {
            snprintf(serial_str, serial_prefix_len, "%s%s%02X%02X%02X-%i", serial_prefix ? serial_prefix : "", serial_prefix ? "-" : "", macaddr[3], macaddr[4], macaddr[5], serial_index);
        }
        
        INFO("Serial %s", serial_str);

        accessories[accessory] = calloc(1, sizeof(homekit_accessory_t));
        accessories[accessory]->id = accessory + 1;
        accessories[accessory]->services = calloc(services, sizeof(homekit_service_t*));
        
        accessories[accessory]->services[0] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[0]->id = 1;
        accessories[accessory]->services[0]->type = HOMEKIT_SERVICE_ACCESSORY_INFORMATION;
        accessories[accessory]->services[0]->characteristics = calloc(7, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[0]->characteristics[0] = &name;
        accessories[accessory]->services[0]->characteristics[1] = &manufacturer;
        accessories[accessory]->services[0]->characteristics[2] = NEW_HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, serial_str);
        accessories[accessory]->services[0]->characteristics[3] = &model;
        accessories[accessory]->services[0]->characteristics[4] = &firmware;
        accessories[accessory]->services[0]->characteristics[5] = &identify_function;
        
        if (acc_count == 0) {
            services -= 3;
            accessories[accessory]->services[services] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[services]->id = 65000;
            accessories[accessory]->services[services]->hidden = true;
            accessories[accessory]->services[services]->type = HOMEKIT_SERVICE_HAP_INFORMATION;
            accessories[accessory]->services[services]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[services]->characteristics[0] = &hap_version;
            
            services++;
            accessories[accessory]->services[services] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[services]->id = 65010;
            accessories[accessory]->services[services]->hidden = true;
            accessories[accessory]->services[services]->type = HOMEKIT_SERVICE_CUSTOM_SETUP;
            accessories[accessory]->services[services]->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[services]->characteristics[0] = &haa_ota_update;
            accessories[accessory]->services[services]->characteristics[1] = &haa_enable_setup;
        }
        
        //service_iid = 100;
    }
    
    void set_killswitch(ch_group_t* ch_group, cJSON* json_context) {
        uint8_t killswitch = KILLSWITCH_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, KILLSWITCH) != NULL) {
            killswitch = cJSON_GetObjectItemCaseSensitive(json_context, KILLSWITCH)->valuedouble;
        }
        
        ch_group->main_enabled = killswitch & 0b01;
        ch_group->child_enabled = killswitch & 0b10;
        
        INFO("Killswitch main %i child %i", ch_group->main_enabled, ch_group->child_enabled);
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
        
        INFO("Exec Ac on Boot %i", exec_actions_on_boot);
        
        return exec_actions_on_boot;
    }
    
    uint8_t get_initial_state(cJSON* json_context) {
        uint8_t initial_state = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE) != NULL) {
            initial_state = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE)->valuedouble;
        }
        
        INFO("Init State %i", initial_state);
        
        return initial_state;
    }
    
    cJSON* init_last_state_json = cJSON_Parse(INIT_STATE_LAST_STR);
    
    // *** NEW SWITCH / OUTLET
    void new_switch(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context, const uint8_t acc_type) {
        uint32_t max_duration = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, VALVE_MAX_DURATION) != NULL) {
            max_duration = (uint32_t) cJSON_GetObjectItemCaseSensitive(json_context, VALVE_MAX_DURATION)->valuedouble;
        }
        
        uint8_t ch_calloc = 2;
        if (max_duration > 0) {
            ch_calloc += 2;
        }
        
        ch_group_t* ch_group = new_ch_group(ch_calloc - 1, 0, 0);
        ch_group->accessory = service_numerator;
        set_killswitch(ch_group, json_context);
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(ON, false, .setter_ex=hkc_on_setter);
        
        ch_group->acc_type = acc_type;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        ch_group->timer2 = autoswitch_time(json_context, ch_group);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            accessories[accessory]->services[service]->characteristics = calloc(ch_calloc, sizeof(homekit_characteristic_t*));
            
            if (acc_type == ACC_TYPE_SWITCH) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_SWITCH;
            } else {    // acc_type == ACC_TYPE_OUTLET
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_OUTLET;
            }
            
            //service_iid += 2;
        }
        
        if (max_duration > 0) {
            ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(SET_DURATION, max_duration, .max_value=(float[]) {max_duration}, .setter_ex=hkc_setter);
            ch_group->ch[2] = NEW_HOMEKIT_CHARACTERISTIC(REMAINING_DURATION, 0, .max_value=(float[]) {max_duration});
            
            if (ch_group->homekit_enabled) {
                accessories[accessory]->services[service]->characteristics[1] = ch_group->ch[1];
                accessories[accessory]->services[service]->characteristics[2] = ch_group->ch[2];
                
                //service_iid += 2;
            }
            
            const uint32_t initial_time = (uint32_t) set_initial_state(ch_group->accessory, 1, init_last_state_json, ch_group->ch[1], CH_TYPE_INT, max_duration);
            if (initial_time > max_duration) {
                ch_group->ch[1]->value.int_value = max_duration;
            } else {
                ch_group->ch[1]->value.int_value = initial_time;
            }
            
            ch_group->timer = esp_timer_create(1000, true, (void*) ch_group, on_timer_worker);
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_1), digstate, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_0), digstate, ch_group, 0);
        
        const bool exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0);
            
            ch_group->ch[0]->value.bool_value = !((bool) set_initial_state(ch_group->accessory, 0, json_context, ch_group->ch[0], CH_TYPE_BOOL, 0));
            if (exec_actions_on_boot) {
                hkc_on_setter(ch_group->ch[0], HOMEKIT_BOOL(!ch_group->ch[0]->value.bool_value));
            } else {
                hkc_on_status_setter(ch_group->ch[0], HOMEKIT_BOOL(!ch_group->ch[0]->value.bool_value));
            }
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1)) {
                if (exec_actions_on_boot) {
                    diginput(99, ch_group, 1);
                } else {
                    digstate(99, ch_group, 1);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0)) {
                ch_group->ch[0]->value.bool_value = true;
                if (exec_actions_on_boot) {
                    diginput(99, ch_group, 0);
                } else {
                    digstate(99, ch_group, 0);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1)) {
                digstate(99, ch_group, 1);
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0)) {
                ch_group->ch[0]->value.bool_value = true;
                digstate(99, ch_group, 0);
            }
        }
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_SWITCH;
            }
        }
    }
    
    // *** NEW BUTTON EVENT / DOORBELL
    void new_button_event(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context, const uint8_t acc_type) {
        ch_group_t* ch_group = new_ch_group(1 + (acc_type == ACC_TYPE_DOORBELL), 1, 0);
        ch_group->accessory = service_numerator;
        set_killswitch(ch_group, json_context);
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;

        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;

        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(PROGRAMMABLE_SWITCH_EVENT, 0);
        
        ch_group->acc_type = acc_type;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        DOORBELL_LAST_STATE = 1;
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_PROGRAMMABLE_SWITCH;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_STATELESS_PROGRAMMABLE_SWITCH;
            }
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
            
            //service_iid += 2;
            
            if (acc_type == ACC_TYPE_DOORBELL) {
                service++;
                
                ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(PROGRAMMABLE_SWITCH_EVENT, 0);
                
                accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
                accessories[accessory]->services[service]->id = (service - 1) * 50;
                //accessories[accessory]->services[service]->id = service_iid;
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_DOORBELL;
                accessories[accessory]->services[service]->primary = false;
                accessories[accessory]->services[service]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
                accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[1];
                
                //service_iid += 2;
            }
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), button_event, ch_group, SINGLEPRESS_EVENT);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), button_event, ch_group, DOUBLEPRESS_EVENT);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2), button_event, ch_group, LONGPRESS_EVENT);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), button_event, ch_group, SINGLEPRESS_EVENT);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), button_event, ch_group, DOUBLEPRESS_EVENT);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_2), button_event, ch_group, LONGPRESS_EVENT);
    }
    
    // *** NEW LOCK
    void new_lock(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(2, 0, 0);
        ch_group->accessory = service_numerator;
        set_killswitch(ch_group, json_context);
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(LOCK_CURRENT_STATE, 1);
        ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(LOCK_TARGET_STATE, 1, .setter_ex=hkc_lock_setter);
        
        ch_group->acc_type = ACC_TYPE_LOCK;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        ch_group->timer2 = autoswitch_time(json_context, ch_group);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_LOCK;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_LOCK_MECHANISM;
            }
            
            accessories[accessory]->services[service]->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
            accessories[accessory]->services[service]->characteristics[1] = ch_group->ch[1];
            
            //service_iid += 3;
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_1), digstate, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_0), digstate, ch_group, 0);
        
        const bool exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0);
            
            ch_group->ch[1]->value.int_value = !((uint8_t) set_initial_state(ch_group->accessory, 0, json_context, ch_group->ch[1], CH_TYPE_INT8, 1));
            if (exec_actions_on_boot) {
                hkc_lock_setter(ch_group->ch[1], HOMEKIT_UINT8(!ch_group->ch[1]->value.int_value));
            } else {
                hkc_lock_status_setter(ch_group->ch[1], HOMEKIT_UINT8(!ch_group->ch[1]->value.int_value));
            }
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1)) {
                if (exec_actions_on_boot) {
                    diginput(99, ch_group, 1);
                } else {
                    digstate(99, ch_group, 1);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0)) {
                ch_group->ch[1]->value.int_value = 0;
                if (exec_actions_on_boot) {
                    diginput(99, ch_group, 0);
                } else {
                    digstate(99, ch_group, 0);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1)) {
                digstate(99, ch_group, 1);
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0)) {
                ch_group->ch[1]->value.int_value = 0;
                digstate(99, ch_group, 0);
            }
        }
    }
    
    // *** NEW BINARY SENSOR
    void new_binary_sensor(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context, uint8_t acc_type) {
        ch_group_t* ch_group = new_ch_group(1, 0, 0);
        ch_group->acc_type = ACC_TYPE_CONTACT_SENSOR;
        ch_group->accessory = service_numerator;
        set_killswitch(ch_group, json_context);
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
        }
        
        switch (acc_type) {
            case ACC_TYPE_OCCUPANCY_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_OCCUPANCY_SENSOR;
                }
                ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(OCCUPANCY_DETECTED, 0);
                break;
                
            case ACC_TYPE_LEAK_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_LEAK_SENSOR;
                }
                ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(LEAK_DETECTED, 0);
                break;
                
            case ACC_TYPE_SMOKE_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_SMOKE_SENSOR;
                }
                ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(SMOKE_DETECTED, 0);
                break;
                
            case ACC_TYPE_CARBON_MONOXIDE_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CARBON_MONOXIDE_SENSOR;
                }
                ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CARBON_MONOXIDE_DETECTED, 0);
                break;
                
            case ACC_TYPE_CARBON_DIOXIDE_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CARBON_DIOXIDE_SENSOR;
                }
                ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CARBON_DIOXIDE_DETECTED, 0);
                break;
                
            case ACC_TYPE_FILTER_CHANGE_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_FILTER_MAINTENANCE;
                }
                ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(FILTER_CHANGE_INDICATION, 0);
                break;
                
            case ACC_TYPE_MOTION_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_MOTION_SENSOR;
                }
                ch_group->acc_type = ACC_TYPE_MOTION_SENSOR;
                ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(MOTION_DETECTED, 0);
                break;
                
            default:    // case ACC_TYPE_CONTACT_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CONTACT_SENSOR;
                }
                ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CONTACT_SENSOR_STATE, 0);
                break;
        }
        
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        ch_group->timer2 = autoswitch_time(json_context, ch_group);

        if (ch_group->homekit_enabled) {
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_BINARY_SENSOR;
            }
            
            accessories[accessory]->services[service]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
            
            //service_iid += 2;
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), binary_sensor, ch_group, 4);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), binary_sensor, ch_group, 4);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), binary_sensor, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), binary_sensor, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_0), binary_sensor, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_1), binary_sensor, ch_group, 3);
        
        const bool exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), binary_sensor, ch_group, 0)) {
            if (acc_type == ACC_TYPE_MOTION_SENSOR) {
                ch_group->ch[0]->value.int_value = 1;
            } else {
                ch_group->ch[0]->value.bool_value = true;
            }
            
            if (exec_actions_on_boot) {
                binary_sensor(99, ch_group, 0);
            } else {
                binary_sensor(99, ch_group, 2);
            }
        }
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), binary_sensor, ch_group, 1)) {
            if (exec_actions_on_boot) {
                binary_sensor(99, ch_group, 1);
            } else {
                binary_sensor(99, ch_group, 3);
            }
        }
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), binary_sensor, ch_group, 2)) {
            if (acc_type == ACC_TYPE_MOTION_SENSOR) {
                ch_group->ch[0]->value.int_value = 1;
            } else {
                ch_group->ch[0]->value.bool_value = true;
            }
            
            binary_sensor(99, ch_group, 2);
        }
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), binary_sensor, ch_group, 3)) {
            binary_sensor(99, ch_group, 3);
        }
    }
    
    // *** NEW WATER VALVE
    void new_valve(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        uint32_t valve_max_duration = VALVE_MAX_DURATION_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, VALVE_MAX_DURATION) != NULL) {
            valve_max_duration = (uint32_t) cJSON_GetObjectItemCaseSensitive(json_context, VALVE_MAX_DURATION)->valuedouble;
        }
        
        uint8_t calloc_count = 4;
        if (valve_max_duration > 0) {
            calloc_count += 2;
        }
        
        ch_group_t* ch_group = new_ch_group(calloc_count - 2, 0, 0);
        ch_group->acc_type = ACC_TYPE_WATER_VALVE;
        ch_group->accessory = service_numerator;
        set_killswitch(ch_group, json_context);
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        uint8_t valve_type = VALVE_SYSTEM_TYPE_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, VALVE_SYSTEM_TYPE) != NULL) {
            valve_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, VALVE_SYSTEM_TYPE)->valuedouble;
        }
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(ACTIVE, 0, .setter_ex=hkc_valve_setter);
        ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(IN_USE, 0);

        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        ch_group->timer2 = autoswitch_time(json_context, ch_group);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            accessories[accessory]->services[service]->characteristics = calloc(calloc_count, sizeof(homekit_characteristic_t*));
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_VALVE;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_VALVE;
            }
            
            //service_iid += 2;
        }
        
        if (valve_max_duration > 0) {
            ch_group->ch[2] = NEW_HOMEKIT_CHARACTERISTIC(SET_DURATION, valve_max_duration, .max_value=(float[]) {valve_max_duration}, .setter_ex=hkc_setter);
            ch_group->ch[3] = NEW_HOMEKIT_CHARACTERISTIC(REMAINING_DURATION, 0, .max_value=(float[]) {valve_max_duration});
            
            if (ch_group->homekit_enabled) {
                accessories[accessory]->services[service]->characteristics[3] = ch_group->ch[2];
                accessories[accessory]->services[service]->characteristics[4] = ch_group->ch[3];
                
                //service_iid += 2;
            }
            
            const uint32_t initial_time = (uint32_t) set_initial_state(ch_group->accessory, 2, init_last_state_json, ch_group->ch[2], CH_TYPE_INT, 900);
            if (initial_time > valve_max_duration) {
                ch_group->ch[2]->value.int_value = valve_max_duration;
            } else {
                ch_group->ch[2]->value.int_value = initial_time;
            }
            
            ch_group->timer = esp_timer_create(1000, true, (void*) ch_group, valve_timer_worker);
        }
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
            accessories[accessory]->services[service]->characteristics[1] = ch_group->ch[1];
            accessories[accessory]->services[service]->characteristics[2] = NEW_HOMEKIT_CHARACTERISTIC(VALVE_TYPE, valve_type);
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_1), digstate, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_0), digstate, ch_group, 0);
        
        const bool exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0);
            
            ch_group->ch[0]->value.int_value = !((uint8_t) set_initial_state(ch_group->accessory, 0, json_context, ch_group->ch[0], CH_TYPE_INT8, 0));
            if (exec_actions_on_boot) {
                hkc_valve_setter(ch_group->ch[0], HOMEKIT_UINT8(!ch_group->ch[0]->value.int_value));
            } else {
                hkc_valve_status_setter(ch_group->ch[0], HOMEKIT_UINT8(!ch_group->ch[0]->value.int_value));
            }
            
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1)) {
                if (exec_actions_on_boot) {
                    diginput(99, ch_group, 1);
                } else {
                    digstate(99, ch_group, 1);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0)) {
                ch_group->ch[0]->value.int_value = 1;
                if (exec_actions_on_boot) {
                    diginput(99, ch_group, 0);
                } else {
                    digstate(99, ch_group, 0);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1)) {
                digstate(99, ch_group, 1);
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0)) {
                ch_group->ch[0]->value.int_value = 1;
                digstate(99, ch_group, 0);
            }
        }
    }
    
    // *** NEW THERMOSTAT
    void new_thermostat(const uint8_t accessory,  uint8_t service, const uint8_t total_services, cJSON* json_context, const uint8_t acc_type) {
        ch_group_t* ch_group = new_ch_group(7, 10, 6);
        ch_group->acc_type = ACC_TYPE_THERMOSTAT;
        ch_group->accessory = service_numerator;
        set_killswitch(ch_group, json_context);
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        // Custom ranges of Target Temperatures
        float min_temp = THERMOSTAT_DEFAULT_MIN_TEMP;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MIN_TEMP) != NULL) {
            min_temp = (float) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MIN_TEMP)->valuedouble;
        }
        
        float max_temp = THERMOSTAT_DEFAULT_MAX_TEMP;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MAX_TEMP) != NULL) {
            max_temp = (float) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MAX_TEMP)->valuedouble;
        }
        
        const float default_target_temp = (min_temp + max_temp) / 2;
        
        // Temperature Deadbands
        TH_DEADBAND = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND) != NULL) {
            TH_DEADBAND = cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND)->valuedouble / 2.000f;
        }
        
        TH_DEADBAND_FORCE_IDLE = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND_FORCE_IDLE) != NULL) {
            TH_DEADBAND_FORCE_IDLE = cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND_FORCE_IDLE)->valuedouble;
        }
        
        TH_DEADBAND_SOFT_ON = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND_SOFT_ON) != NULL) {
            TH_DEADBAND_SOFT_ON = cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND_SOFT_ON)->valuedouble;
        }
        
        // Thermostat Type
        TH_TYPE = THERMOSTAT_TYPE_HEATER;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_TYPE) != NULL) {
            TH_TYPE = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_TYPE)->valuedouble;
        }
        
        // HomeKit Characteristics
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_TEMPERATURE, 0, .min_value=(float[]) {-100}, .max_value=(float[]) {200});
        ch_group->ch[2] = NEW_HOMEKIT_CHARACTERISTIC(ACTIVE, 0, .setter_ex=hkc_th_target_setter);
        ch_group->ch[3] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_HEATER_COOLER_STATE, 0);
        
        float temp_step = 0.1;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_TARGET_TEMP_STEP) != NULL) {
            temp_step = cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_TARGET_TEMP_STEP)->valuedouble;
        }
        
        if (TH_TYPE != THERMOSTAT_TYPE_COOLER) {
            ch_group->ch[5] = NEW_HOMEKIT_CHARACTERISTIC(HEATING_THRESHOLD_TEMPERATURE, default_target_temp - 1, .min_value=(float[]) {min_temp}, .max_value=(float[]) {max_temp}, .min_step=(float[]) {temp_step}, .setter_ex=update_th);
            ch_group->ch[5]->value.float_value = set_initial_state(ch_group->accessory, 5, init_last_state_json, ch_group->ch[5], CH_TYPE_FLOAT, default_target_temp - 1);
        }
        
        if (TH_TYPE != THERMOSTAT_TYPE_HEATER) {
            ch_group->ch[6] = NEW_HOMEKIT_CHARACTERISTIC(COOLING_THRESHOLD_TEMPERATURE, default_target_temp + 1, .min_value=(float[]) {min_temp}, .max_value=(float[]) {max_temp}, .min_step=(float[]) {temp_step}, .setter_ex=update_th);
            ch_group->ch[6]->value.float_value = set_initial_state(ch_group->accessory, 6, init_last_state_json, ch_group->ch[6], CH_TYPE_FLOAT, default_target_temp + 1);
        }
        
        if (ch_group->homekit_enabled) {
            uint8_t calloc_count = 6;
            if (TH_TYPE >= THERMOSTAT_TYPE_HEATERCOOLER) {
                calloc_count += 1;
            }
        
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_HEATER_COOLER;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_HEATER_COOLER;
            }
            
            accessories[accessory]->services[service]->characteristics = calloc(calloc_count, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[2];
            accessories[accessory]->services[service]->characteristics[1] = ch_group->ch[0];
            accessories[accessory]->services[service]->characteristics[2] = ch_group->ch[3];
            
            //service_iid += 5;
        }
        
        switch ((uint8_t) TH_TYPE) {
            case THERMOSTAT_TYPE_COOLER:
                ch_group->ch[4] = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HEATER_COOLER_STATE, THERMOSTAT_TARGET_MODE_COOLER, .min_value=(float[]) {THERMOSTAT_TARGET_MODE_COOLER}, .max_value=(float[]) {THERMOSTAT_TARGET_MODE_COOLER}, .valid_values={.count=1, .values=(uint8_t[]) {THERMOSTAT_TARGET_MODE_COOLER}});
                
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->characteristics[4] = ch_group->ch[6];
                }
                break;
                
            case THERMOSTAT_TYPE_HEATERCOOLER:
                ch_group->ch[4] = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HEATER_COOLER_STATE, THERMOSTAT_TARGET_MODE_AUTO, .setter_ex=update_th);
                
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->characteristics[4] = ch_group->ch[5];
                    accessories[accessory]->services[service]->characteristics[5] = ch_group->ch[6];
                    //service_iid++;
                }
                break;
                
            case THERMOSTAT_TYPE_HEATERCOOLER_NOAUTO:
                ch_group->ch[4] = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HEATER_COOLER_STATE, THERMOSTAT_TARGET_MODE_COOLER, .min_value=(float[]) {THERMOSTAT_TARGET_MODE_HEATER}, .max_value=(float[]) {THERMOSTAT_TARGET_MODE_COOLER}, .valid_values={.count=2, .values=(uint8_t[]) {THERMOSTAT_TARGET_MODE_HEATER, THERMOSTAT_TARGET_MODE_COOLER}}, .setter_ex=update_th);
                
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->characteristics[4] = ch_group->ch[5];
                    accessories[accessory]->services[service]->characteristics[5] = ch_group->ch[6];
                    //service_iid++;
                }
                break;
                
            default:        // case THERMOSTAT_TYPE_HEATER:
                ch_group->ch[4] = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HEATER_COOLER_STATE, THERMOSTAT_TARGET_MODE_HEATER, .min_value=(float[]) {THERMOSTAT_TARGET_MODE_HEATER}, .max_value=(float[]) {THERMOSTAT_TARGET_MODE_HEATER}, .valid_values={.count=1, .values=(uint8_t[]) {THERMOSTAT_TARGET_MODE_HEATER}});
                
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->characteristics[4] = ch_group->ch[5];
                }
                break;
        }
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service]->characteristics[3] = ch_group->ch[4];
        }
        
        if (acc_type == ACC_TYPE_THERMOSTAT_WITH_HUM) {
            service++;
            
            ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_RELATIVE_HUMIDITY, 0);
            
            if (ch_group->homekit_enabled) {
                accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
                accessories[accessory]->services[service]->id = (service - 1) * 50;
                //accessories[accessory]->services[service]->id = service_iid;
                accessories[accessory]->services[service]->primary = false;
                accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
                
                if (homekit_enabled == 2) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_HUMIDITY_SENSOR;
                } else {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_HUMIDITY_SENSOR;
                }

                accessories[accessory]->services[service]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
                accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[1];
                
                //service_iid += 2;
            }
        }
        
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        const float poll_period = th_sensor(ch_group, json_context);
        
        TH_IAIRZONING_CONTROLLER = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_IAIRZONING_CONTROLLER) != NULL) {
            TH_IAIRZONING_CONTROLLER = cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_IAIRZONING_CONTROLLER)->valuedouble;
        }
        
        ch_group->timer2 = esp_timer_create(th_update_delay(json_context) * 1000, false, (void*) ch_group, process_th_timer);
        
        if (TH_SENSOR_GPIO != -1) {
            set_used_gpio((uint8_t) TH_SENSOR_GPIO);
        }
        
        if ((TH_SENSOR_GPIO != -1 || TH_SENSOR_TYPE > 4) && TH_IAIRZONING_CONTROLLER == 0) {
            th_sensor_starter(ch_group, poll_period);
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), th_input, ch_group, 9);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3), th_input_temp, ch_group, THERMOSTAT_TEMP_UP);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_4), th_input_temp, ch_group, THERMOSTAT_TEMP_DOWN);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), th_input, ch_group, 9);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_3), th_input_temp, ch_group, THERMOSTAT_TEMP_UP);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_4), th_input_temp, ch_group, THERMOSTAT_TEMP_DOWN);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), th_input, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), th_input, ch_group, 0);
        
        if (TH_TYPE >= THERMOSTAT_TYPE_HEATERCOOLER) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_5), th_input, ch_group, 5);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_6), th_input, ch_group, 6);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_5), th_input, ch_group, 5);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_6), th_input, ch_group, 6);
            
            if (TH_TYPE == THERMOSTAT_TYPE_HEATERCOOLER) {
                diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_7), th_input, ch_group, 7);
                ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_7), th_input, ch_group, 7);
            }
            
            ch_group->ch[4]->value.int_value = set_initial_state(ch_group->accessory, 4, init_last_state_json, ch_group->ch[4], CH_TYPE_INT8, ch_group->ch[4]->value.int_value);
        }
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), th_input, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), th_input, ch_group, 0);
            
            ch_group->ch[2]->value.int_value = !((uint8_t) set_initial_state(ch_group->accessory, 2, json_context, ch_group->ch[2], CH_TYPE_INT8, 0));
            update_th(ch_group->ch[2], HOMEKIT_UINT8(!ch_group->ch[2]->value.int_value));
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), th_input, ch_group, 1)) {
                th_input(99, ch_group, 1);
            }
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), th_input, ch_group, 0)) {
                ch_group->ch[2]->value.int_value = 1;
                th_input(99, ch_group, 0);
            }
        }
    }
    
    // *** NEW IAIRZONING
    void new_iairzoning(cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(0, 1, 2);
        ch_group->accessory = service_numerator;
        ch_group->acc_type = ACC_TYPE_IAIRZONING;
        
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        
        IAIRZONING_DELAY_ACTION_CH_GROUP = IAIRZONING_DELAY_ACTION_DEFAULT * MS_TO_TICKS(1000);
        if (cJSON_GetObjectItemCaseSensitive(json_context, IAIRZONING_DELAY_ACTION_SET) != NULL) {
            IAIRZONING_DELAY_ACTION_CH_GROUP = cJSON_GetObjectItemCaseSensitive(json_context, IAIRZONING_DELAY_ACTION_SET)->valuedouble * MS_TO_TICKS(1000);
        }
        
        ch_group->timer2 = esp_timer_create(th_update_delay(json_context) * 1000, false, (void*) ch_group, set_zones_timer_worker);
        
        th_sensor_starter(ch_group, sensor_poll_period(json_context, TH_SENSOR_POLL_PERIOD_DEFAULT));
    }
    
    // *** NEW TEMPERATURE SENSOR
    void new_temp_sensor(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(1, 5, 1);
        ch_group->acc_type = ACC_TYPE_TEMP_SENSOR;
        ch_group->accessory = service_numerator;
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_TEMPERATURE, 0, .min_value=(float[]) {-100}, .max_value=(float[]) {200});
  
        const float poll_period = th_sensor(ch_group, json_context);
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_TEMPERATURE_SENSOR;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_TEMPERATURE_SENSOR;
            }
            
            accessories[accessory]->services[service]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
            
            //service_iid += 2;
        }
            
        if (TH_SENSOR_GPIO != -1) {
            set_used_gpio((uint8_t) TH_SENSOR_GPIO);
        }
        
        if (TH_SENSOR_GPIO != -1 || TH_SENSOR_TYPE > 4) {
            th_sensor_starter(ch_group, poll_period);
        }
    }
    
    // *** NEW HUMIDITY SENSOR
    void new_hum_sensor(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(2, 5, 2);
        ch_group->acc_type = ACC_TYPE_HUM_SENSOR;
        ch_group->accessory = service_numerator;
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_RELATIVE_HUMIDITY, 0);
        
        const float poll_period = th_sensor(ch_group, json_context);
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_HUMIDITY_SENSOR;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_HUMIDITY_SENSOR;
            }
            
            accessories[accessory]->services[service]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[1];
            
            //service_iid += 2;
        }
        
        if (TH_SENSOR_GPIO != -1) {
            set_used_gpio((uint8_t) TH_SENSOR_GPIO);
            th_sensor_starter(ch_group, poll_period);
        }
    }
    
    // *** NEW TEMPERATURE AND HUMIDITY SENSOR
    void new_th_sensor(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(2, 5, 2);
        ch_group->acc_type = ACC_TYPE_TH_SENSOR;
        ch_group->accessory = service_numerator;
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_TEMPERATURE, 0, .min_value=(float[]) {-100}, .max_value=(float[]) {200});
        ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_RELATIVE_HUMIDITY, 0);
        
        const float poll_period = th_sensor(ch_group, json_context);
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_TEMPERATURE_SENSOR;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_TEMPERATURE_SENSOR;
            }
            
            accessories[accessory]->services[service]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
            
            //service_iid += 2;
            
            service++;
            
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = (service - 1) * 50;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = false;
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_HUMIDITY_SENSOR;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_HUMIDITY_SENSOR;
            }
            
            accessories[accessory]->services[service]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[1];
            
            //service_iid += 2;
        }
        
        if (TH_SENSOR_GPIO != -1) {
            set_used_gpio((uint8_t) TH_SENSOR_GPIO);
            th_sensor_starter(ch_group, poll_period);
        }
    }
    
    // *** NEW HUMIDIFIER
    void new_humidifier(const uint8_t accessory,  uint8_t service, const uint8_t total_services, cJSON* json_context, const uint8_t acc_type) {
        ch_group_t* ch_group = new_ch_group(7, 9, 5);
        ch_group->acc_type = ACC_TYPE_HUMIDIFIER;
        ch_group->accessory = service_numerator;
        set_killswitch(ch_group, json_context);
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        // Humidity Deadbands
        HM_DEADBAND = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_DEADBAND) != NULL) {
            HM_DEADBAND = cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_DEADBAND)->valuedouble / 2.000f;
        }
        
        HM_DEADBAND_FORCE_IDLE = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_DEADBAND_FORCE_IDLE) != NULL) {
            HM_DEADBAND_FORCE_IDLE = cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_DEADBAND_FORCE_IDLE)->valuedouble;
        }
        
        HM_DEADBAND_SOFT_ON = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_DEADBAND_SOFT_ON) != NULL) {
            HM_DEADBAND_SOFT_ON = cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_DEADBAND_SOFT_ON)->valuedouble;
        }
        
        // Humidifier Type
        HM_TYPE = HUMIDIF_TYPE_HUM;
        if (cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_TYPE) != NULL) {
            HM_TYPE = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_TYPE)->valuedouble;
        }
        
        // HomeKit Characteristics
        ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_RELATIVE_HUMIDITY, 0);
        ch_group->ch[2] = NEW_HOMEKIT_CHARACTERISTIC(ACTIVE, 0, .setter_ex=hkc_humidif_target_setter);
        ch_group->ch[3] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_HUMIDIFIER_DEHUMIDIFIER_STATE, 0);
        
        if (HM_TYPE != HUMIDIF_TYPE_DEHUM) {
            ch_group->ch[5] = NEW_HOMEKIT_CHARACTERISTIC(RELATIVE_HUMIDITY_HUMIDIFIER_THRESHOLD, 40, .setter_ex=update_humidif);
            ch_group->ch[5]->value.float_value = set_initial_state(ch_group->accessory, 5, init_last_state_json, ch_group->ch[5], CH_TYPE_FLOAT, 40);
        }
        
        if (HM_TYPE != HUMIDIF_TYPE_HUM) {
            ch_group->ch[6] = NEW_HOMEKIT_CHARACTERISTIC(RELATIVE_HUMIDITY_DEHUMIDIFIER_THRESHOLD, 60, .setter_ex=update_humidif);
            ch_group->ch[6]->value.float_value = set_initial_state(ch_group->accessory, 6, init_last_state_json, ch_group->ch[6], CH_TYPE_FLOAT, 60);
        }
        
        
        if (ch_group->homekit_enabled) {
            uint8_t calloc_count = 6;
            if (HM_TYPE >= HUMIDIF_TYPE_HUMDEHUM) {
                calloc_count += 1;
            }
        
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_HUMIDIFIER_DEHUMIDIFIER;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_HUMIDIFIER_DEHUMIDIFIER;
            }

            accessories[accessory]->services[service]->characteristics = calloc(calloc_count, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[2];
            accessories[accessory]->services[service]->characteristics[1] = ch_group->ch[1];
            accessories[accessory]->services[service]->characteristics[2] = ch_group->ch[3];
            
            //service_iid += 5;
        }
        
        switch ((uint8_t) HM_TYPE) {
            case HUMIDIF_TYPE_DEHUM:
                ch_group->ch[4] = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HUMIDIFIER_DEHUMIDIFIER_STATE, HUMIDIF_TARGET_MODE_DEHUM, .min_value=(float[]) {HUMIDIF_TARGET_MODE_DEHUM}, .max_value=(float[]) {HUMIDIF_TARGET_MODE_DEHUM}, .valid_values={.count=1, .values=(uint8_t[]) {HUMIDIF_TARGET_MODE_DEHUM}});
                
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->characteristics[4] = ch_group->ch[6];
                }
                break;
                
            case HUMIDIF_TYPE_HUMDEHUM:
                ch_group->ch[4] = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HUMIDIFIER_DEHUMIDIFIER_STATE, HUMIDIF_TARGET_MODE_AUTO, .setter_ex=update_humidif);
                
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->characteristics[4] = ch_group->ch[5];
                    accessories[accessory]->services[service]->characteristics[5] = ch_group->ch[6];
                    //service_iid++;
                }
                break;
                
            case HUMIDIF_TYPE_HUMDEHUM_NOAUTO:
                ch_group->ch[4] = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HUMIDIFIER_DEHUMIDIFIER_STATE, HUMIDIF_TARGET_MODE_HUM, .min_value=(float[]) {HUMIDIF_TARGET_MODE_HUM}, .max_value=(float[]) {HUMIDIF_TARGET_MODE_DEHUM}, .valid_values={.count=2, .values=(uint8_t[]) {HUMIDIF_TARGET_MODE_HUM, HUMIDIF_TARGET_MODE_DEHUM}}, .setter_ex=update_humidif);
                
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->characteristics[4] = ch_group->ch[5];
                    accessories[accessory]->services[service]->characteristics[5] = ch_group->ch[6];
                    //service_iid++;
                }
                break;
                
            default:        // case HUMIDIF_TYPE_HUM:
                ch_group->ch[4] = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HUMIDIFIER_DEHUMIDIFIER_STATE, HUMIDIF_TARGET_MODE_HUM, .min_value=(float[]) {HUMIDIF_TARGET_MODE_HUM}, .max_value=(float[]) {HUMIDIF_TARGET_MODE_HUM}, .valid_values={.count=1, .values=(uint8_t[]) {HUMIDIF_TARGET_MODE_HUM}});

                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->characteristics[4] = ch_group->ch[5];
                }
                break;
        }
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service]->characteristics[3] = ch_group->ch[4];
        }
        
        if (acc_type == ACC_TYPE_HUMIDIFIER_WITH_TEMP) {
            service++;
            
            ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_TEMPERATURE, 0);
            
            if (ch_group->homekit_enabled) {
                accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
                accessories[accessory]->services[service]->id = (service - 1) * 50;
                //accessories[accessory]->services[service]->id = service_iid;
                accessories[accessory]->services[service]->primary = false;
                accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
                
                if (homekit_enabled == 2) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_TEMPERATURE_SENSOR;
                } else {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_TEMPERATURE_SENSOR;
                }

                accessories[accessory]->services[service]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
                accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
                
                //service_iid += 2;
            }
        }
        
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        const float poll_period = th_sensor(ch_group, json_context);
        
        ch_group->timer2 = esp_timer_create(th_update_delay(json_context) * 1000, false, (void*) ch_group, process_humidif_timer);
        
        if (TH_SENSOR_GPIO != -1) {
            set_used_gpio((uint8_t) TH_SENSOR_GPIO);
        }
        
        if (TH_SENSOR_GPIO != -1 || TH_SENSOR_TYPE > 4) {
            th_sensor_starter(ch_group, poll_period);
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), humidif_input, ch_group, 9);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3), humidif_input_temp, ch_group, HUMIDIF_UP);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_4), humidif_input_temp, ch_group, HUMIDIF_DOWN);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), humidif_input, ch_group, 9);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_3), humidif_input_temp, ch_group, HUMIDIF_UP);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_4), humidif_input_temp, ch_group, HUMIDIF_DOWN);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), humidif_input, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), humidif_input, ch_group, 0);
        
        if (TH_TYPE >= HUMIDIF_TYPE_HUMDEHUM) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_5), humidif_input, ch_group, 5);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_6), humidif_input, ch_group, 6);
            
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_5), humidif_input, ch_group, 5);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_6), humidif_input, ch_group, 6);
            
            if (TH_TYPE == HUMIDIF_TYPE_HUMDEHUM) {
                diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_7), humidif_input, ch_group, 7);
                ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_7), humidif_input, ch_group, 7);
            }
            
            ch_group->ch[4]->value.int_value = set_initial_state(ch_group->accessory, 4, init_last_state_json, ch_group->ch[4], CH_TYPE_INT8, ch_group->ch[4]->value.int_value);
        }
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), humidif_input, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), humidif_input, ch_group, 0);
            
            ch_group->ch[2]->value.int_value = !((uint8_t) set_initial_state(ch_group->accessory, 2, json_context, ch_group->ch[2], CH_TYPE_INT8, 0));
            update_humidif(ch_group->ch[2], HOMEKIT_UINT8(!ch_group->ch[2]->value.int_value));
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), humidif_input, ch_group, 1)) {
                humidif_input(99, ch_group, 1);
            }
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), humidif_input, ch_group, 0)) {
                ch_group->ch[2]->value.int_value = 1;
                humidif_input(99, ch_group, 0);
            }
        }
    }
    
    // *** NEW LIGHTBULB
    void new_lightbulb(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(4, 2, 1);
        ch_group->acc_type = ACC_TYPE_LIGHTBULB;
        ch_group->accessory = service_numerator;
        set_killswitch(ch_group, json_context);
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(ON, false, .setter_ex=hkc_rgbw_setter);
        ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(BRIGHTNESS, 100, .setter_ex=hkc_rgbw_setter);
 
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        ch_group->timer2 = autoswitch_time(json_context, ch_group);
        
        lightbulb_group_t* lightbulb_group = malloc(sizeof(lightbulb_group_t));
        memset(lightbulb_group, 0, sizeof(*lightbulb_group));
        
        lightbulb_group->ch0 = ch_group->ch[0];
        for (uint8_t i = 0; i <= 4; i++) {
            lightbulb_group->gpio[i] = MAX_GPIOS;
            lightbulb_group->flux[i] = 1;
        }
        
        LIGHTBULB_MAX_POWER = 1;
        LIGHTBULB_CURVE_FACTOR = 0;
        lightbulb_group->r[0] = 0.6914;
        lightbulb_group->r[1] = 0.3077;
        lightbulb_group->g[0] = 0.1451;
        lightbulb_group->g[1] = 0.7097;
        lightbulb_group->b[0] = 0.1350;
        lightbulb_group->b[1] = 0.0622;
        lightbulb_group->cw[0] = 0.3115;
        lightbulb_group->cw[1] = 0.3338;
        lightbulb_group->ww[0] = 0.4784;
        lightbulb_group->ww[1] = 0.4065;
        lightbulb_group->wp[0] = 0.34567;       // D50
        lightbulb_group->wp[1] = 0.35850;
        lightbulb_group->rgb[0][0] = lightbulb_group->r[0];     // Default to the LED RGB coordinates
        lightbulb_group->rgb[0][1] = lightbulb_group->r[1];
        lightbulb_group->rgb[1][0] = lightbulb_group->g[0];
        lightbulb_group->rgb[1][1] = lightbulb_group->g[1];
        lightbulb_group->rgb[2][0] = lightbulb_group->b[0];
        lightbulb_group->rgb[2][1] = lightbulb_group->b[1];
        lightbulb_group->cmy[0][0] = 0.241393;  // These corrrespond to Kevin's preferred CMY. We could make these be hue ints for RAM saving.
        lightbulb_group->cmy[0][1] = 0.341227;
        lightbulb_group->cmy[1][0] = 0.455425;
        lightbulb_group->cmy[1][1] = 0.227088;
        lightbulb_group->cmy[2][0] = 0.503095;
        lightbulb_group->cmy[2][1] = 0.449426;
        lightbulb_group->step = RGBW_STEP_DEFAULT;
        lightbulb_group->armed_autodimmer = false;
        lightbulb_group->autodimmer_task_delay = MS_TO_TICKS(AUTODIMMER_TASK_DELAY_DEFAULT);
        lightbulb_group->autodimmer_task_step = AUTODIMMER_TASK_STEP_DEFAULT;
        
        lightbulb_group->next = main_config.lightbulb_groups;
        main_config.lightbulb_groups = lightbulb_group;
        
        LIGHTBULB_SET_DELAY_TIMER = esp_timer_create(LIGHTBULB_SET_DELAY_MS, false, (void*) ch_group, lightbulb_task_timer);
        
        LIGHTBULB_TYPE = LIGHTBULB_TYPE_PWM;
        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_TYPE_SET) != NULL) {
            LIGHTBULB_TYPE = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_TYPE_SET)->valuedouble;
        }
        
        cJSON* gpio_array = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_GPIO_ARRAY_SET);
        if (!gpio_array) {
            LIGHTBULB_TYPE = LIGHTBULB_TYPE_VIRTUAL;
            LIGHTBULB_CHANNELS = 1;
        } else {
            LIGHTBULB_CHANNELS = 3;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_CHANNELS_SET) != NULL) {
            LIGHTBULB_CHANNELS = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_CHANNELS_SET)->valuedouble;
        }
        
        if (LIGHTBULB_TYPE != LIGHTBULB_TYPE_VIRTUAL) {
            if (!main_config.set_lightbulb_timer) {
                main_config.set_lightbulb_timer = esp_timer_create(RGBW_PERIOD, true, NULL, rgbw_set_timer_worker);
            }
        }
        
        if (LIGHTBULB_TYPE == LIGHTBULB_TYPE_PWM || LIGHTBULB_TYPE == LIGHTBULB_TYPE_PWM_CWWW) {
            if (cJSON_GetObjectItemCaseSensitive(json_context, PWM_DITHER_SET) != NULL) {
                LIGHTBULB_PWM_DITHER = cJSON_GetObjectItemCaseSensitive(json_context, PWM_DITHER_SET)->valuedouble;
            }
            
            LIGHTBULB_CHANNELS = cJSON_GetArraySize(gpio_array);
            for (uint8_t i = 0; i < LIGHTBULB_CHANNELS; i++) {
                uint8_t gpio = cJSON_GetArrayItem(gpio_array, i)->valuedouble;
                set_used_gpio(gpio % 100);
                
                lightbulb_group->gpio[i] = gpio % 100;
                adv_pwm_new_channel(lightbulb_group->gpio[i], gpio / 100);
            }
            
            adv_pwm_start();
            
        } else if (LIGHTBULB_TYPE == LIGHTBULB_TYPE_MY92X1) {
            lightbulb_group->gpio[0] = cJSON_GetArrayItem(gpio_array, 0)->valuedouble;
            lightbulb_group->gpio[1] = cJSON_GetArrayItem(gpio_array, 1)->valuedouble;
            
            //
            // TO-DO
            //
            
        } else if (LIGHTBULB_TYPE == LIGHTBULB_TYPE_NRZ) {
            lightbulb_group->gpio[0] = cJSON_GetArrayItem(gpio_array, 0)->valuedouble;
            set_used_gpio(lightbulb_group->gpio[0]);
            
            LIGHTBULB_RANGE_START = cJSON_GetArrayItem(gpio_array, 1)->valuedouble;
            LIGHTBULB_RANGE_START -= 1;
            LIGHTBULB_RANGE_START = LIGHTBULB_RANGE_START * LIGHTBULB_CHANNELS;
            LIGHTBULB_RANGE_END = cJSON_GetArrayItem(gpio_array, 2)->valuedouble;
            LIGHTBULB_RANGE_END = LIGHTBULB_RANGE_END * LIGHTBULB_CHANNELS;
            
            addressled_t* addressled = new_addressled(lightbulb_group->gpio[0], LIGHTBULB_RANGE_END);
            
            cJSON* nrz_times = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_NRZ_TIMES_ARRAY_SET);
            
            if (nrz_times) {
                const float t0h = cJSON_GetArrayItem(nrz_times, 0)->valuedouble;
                addressled->time_0 = nrz_ticks(t0h);                                                    // T0H
                addressled->time_1 = nrz_ticks(cJSON_GetArrayItem(nrz_times, 1)->valuedouble);          // T1H
                addressled->period = nrz_ticks(t0h + cJSON_GetArrayItem(nrz_times, 2)->valuedouble);    // T0H + T0L
                
                INFO("NRZ Ticks %i, %i, %i", addressled->time_0, addressled->time_1, addressled->period);
            }
            
            cJSON* color_map = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_COLOR_MAP_SET);
            if (color_map) {
                const uint8_t size = cJSON_GetArraySize(color_map);
                for (uint8_t i = 0; i < size; i++) {
                    addressled->map[i] = cJSON_GetArrayItem(color_map, i)->valuedouble;
                }
            }
        }
        
        INFO("Channels %i Type %i", LIGHTBULB_CHANNELS, LIGHTBULB_TYPE);
        
        bool is_custom_initial = false;
        uint16_t custom_initial[3];
        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_INITITAL_STATE_ARRAY_SET) != NULL) {
            is_custom_initial = true;
            cJSON* init_values_array = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_INITITAL_STATE_ARRAY_SET);
            for (uint8_t i = 0; i < cJSON_GetArraySize(init_values_array); i++) {
                custom_initial[i] = (uint16_t) cJSON_GetArrayItem(init_values_array, i)->valuedouble;
            }
        }
        
        if (LIGHTBULB_TYPE != LIGHTBULB_TYPE_VIRTUAL) {
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_MAX_POWER_SET) != NULL) {
                LIGHTBULB_MAX_POWER = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_MAX_POWER_SET)->valuedouble;
            }
            
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_CURVE_FACTOR_SET) != NULL) {
                LIGHTBULB_CURVE_FACTOR = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_CURVE_FACTOR_SET)->valuedouble;
            }
            
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FLUX_ARRAY_SET) != NULL) {
                cJSON* flux_array = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FLUX_ARRAY_SET);
                for (uint8_t i = 0; i < LIGHTBULB_CHANNELS; i++) {
                    lightbulb_group->flux[i] = (float) cJSON_GetArrayItem(flux_array, i)->valuedouble;
                }
            }
            
            INFO("Flux array %g, %g, %g, %g, %g", lightbulb_group->flux[0], lightbulb_group->flux[1], lightbulb_group->flux[2], lightbulb_group->flux[3], lightbulb_group->flux[4]);
            
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_RGB_ARRAY_SET) != NULL) {
                cJSON* rgb_array = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_RGB_ARRAY_SET);
                for (uint8_t i = 0; i < 6; i++) {
                    lightbulb_group->rgb[i >> 1][i % 2] = (float) cJSON_GetArrayItem(rgb_array, i)->valuedouble;
                }
            }
            
            INFO("Target RGB array %g, %g, %g, %g, %g, %g", lightbulb_group->rgb[0][0], lightbulb_group->rgb[0][1], lightbulb_group->rgb[1][0], lightbulb_group->rgb[1][1], lightbulb_group->rgb[2][0], lightbulb_group->rgb[2][1]);
            
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_CMY_ARRAY_SET) != NULL) {
                cJSON* cmy_array = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_CMY_ARRAY_SET);
                for (uint8_t i = 0; i < 6; i++) {
                    lightbulb_group->cmy[i >> 1][i % 2] = (float) cJSON_GetArrayItem(cmy_array, i)->valuedouble;
                }
            }
            
            INFO("Target CMY array %g, %g, %g, %g, %g, %g", lightbulb_group->cmy[0][0], lightbulb_group->cmy[0][1], lightbulb_group->cmy[1][0], lightbulb_group->cmy[1][1], lightbulb_group->cmy[2][0], lightbulb_group->cmy[2][1]);
            
            INFO("CMY array [%g, %g, %g], [%g, %g, %g]", lightbulb_group->cmy[0][0], lightbulb_group->cmy[0][1], lightbulb_group->cmy[0][2], lightbulb_group->cmy[1][0], lightbulb_group->cmy[1][1], lightbulb_group->cmy[1][2]);
            
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_COORDINATE_ARRAY_SET) != NULL) {
                cJSON* coordinate_array = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_COORDINATE_ARRAY_SET);
                lightbulb_group->r[0] = (float) cJSON_GetArrayItem(coordinate_array, 0)->valuedouble;
                lightbulb_group->r[1] = (float) cJSON_GetArrayItem(coordinate_array, 1)->valuedouble;
                lightbulb_group->g[0] = (float) cJSON_GetArrayItem(coordinate_array, 2)->valuedouble;
                lightbulb_group->g[1] = (float) cJSON_GetArrayItem(coordinate_array, 3)->valuedouble;
                lightbulb_group->b[0] = (float) cJSON_GetArrayItem(coordinate_array, 4)->valuedouble;
                lightbulb_group->b[1] = (float) cJSON_GetArrayItem(coordinate_array, 5)->valuedouble;
                
                if (LIGHTBULB_CHANNELS >= 4) {
                    lightbulb_group->cw[0] = (float) cJSON_GetArrayItem(coordinate_array, 6)->valuedouble;
                    lightbulb_group->cw[1] = (float) cJSON_GetArrayItem(coordinate_array, 7)->valuedouble;
                }
                
                if (LIGHTBULB_CHANNELS == 5) {
                    lightbulb_group->ww[0] = (float) cJSON_GetArrayItem(coordinate_array, 8)->valuedouble;
                    lightbulb_group->ww[1] = (float) cJSON_GetArrayItem(coordinate_array, 9)->valuedouble;
                }
            }

            INFO("Coordinate array [%g, %g], [%g, %g], [%g, %g], [%g, %g], [%g, %g]", lightbulb_group->r[0], lightbulb_group->r[1],
                                                                                        lightbulb_group->g[0], lightbulb_group->g[1],
                                                                                        lightbulb_group->b[0], lightbulb_group->b[1],
                                                                                        lightbulb_group->cw[0], lightbulb_group->cw[1],
                                                                                        lightbulb_group->ww[0], lightbulb_group->ww[1]);
            
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_WHITE_POINT_SET) != NULL) {
                cJSON* white_point = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_WHITE_POINT_SET);
                lightbulb_group->wp[0] = (float) cJSON_GetArrayItem(white_point, 0)->valuedouble;
                lightbulb_group->wp[1] = (float) cJSON_GetArrayItem(white_point, 1)->valuedouble;
            }
            
            INFO("White point [%g, %g]", lightbulb_group->wp[0], lightbulb_group->wp[1]);
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, RGBW_STEP_SET) != NULL) {
            lightbulb_group->step = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_context, RGBW_STEP_SET)->valuedouble;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, AUTODIMMER_TASK_DELAY_SET) != NULL) {
            lightbulb_group->autodimmer_task_delay = cJSON_GetObjectItemCaseSensitive(json_context, AUTODIMMER_TASK_DELAY_SET)->valuedouble * MS_TO_TICKS(1000);
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, AUTODIMMER_TASK_STEP_SET) != NULL) {
            lightbulb_group->autodimmer_task_step = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, AUTODIMMER_TASK_STEP_SET)->valuedouble;
        }
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_LIGHTBULB;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_LIGHTBULB;
            }
            
            //service_iid += 3;
        }
        
        uint8_t calloc_count = 3;

        if (LIGHTBULB_CHANNELS >= 3) {
            // Channels 3+
            calloc_count += 2;
            
            ch_group->ch[2] = NEW_HOMEKIT_CHARACTERISTIC(HUE, 0, .setter_ex=hkc_rgbw_setter);
            ch_group->ch[3] = NEW_HOMEKIT_CHARACTERISTIC(SATURATION, 0, .setter_ex=hkc_rgbw_setter);
            
            if (ch_group->homekit_enabled) {
                accessories[accessory]->services[service]->characteristics = calloc(calloc_count, sizeof(homekit_characteristic_t*));
                accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
                accessories[accessory]->services[service]->characteristics[1] = ch_group->ch[1];
                accessories[accessory]->services[service]->characteristics[2] = ch_group->ch[2];
                accessories[accessory]->services[service]->characteristics[3] = ch_group->ch[3];
                
                //service_iid += 2;
            }
            
            if (is_custom_initial)  {
                ch_group->ch[2]->value.float_value = custom_initial[1];
                ch_group->ch[3]->value.float_value = custom_initial[2];
            } else {
                ch_group->ch[2]->value.float_value = set_initial_state(ch_group->accessory, 2, init_last_state_json, ch_group->ch[2], CH_TYPE_FLOAT, 0);
                ch_group->ch[3]->value.float_value = set_initial_state(ch_group->accessory, 3, init_last_state_json, ch_group->ch[3], CH_TYPE_FLOAT, 0);
            }
            
        } else if (LIGHTBULB_CHANNELS == 2) {
            // Channels 2
            ch_group->chs = 3;
            calloc_count++;
            
            ch_group->ch[2] = NEW_HOMEKIT_CHARACTERISTIC(COLOR_TEMPERATURE, 152, .setter_ex=hkc_rgbw_setter);
            
            if (ch_group->homekit_enabled) {
                accessories[accessory]->services[service]->characteristics = calloc(calloc_count, sizeof(homekit_characteristic_t*));
                accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
                accessories[accessory]->services[service]->characteristics[1] = ch_group->ch[1];
                accessories[accessory]->services[service]->characteristics[2] = ch_group->ch[2];
                
                //service_iid++;
            }
            
            if (is_custom_initial)  {
                ch_group->ch[2]->value.int_value = custom_initial[1];
            } else {
                ch_group->ch[2]->value.int_value = set_initial_state(ch_group->accessory, 2, init_last_state_json, ch_group->ch[2], CH_TYPE_INT, 152);
            }
            
        } else {
            // Channels 1
            ch_group->chs = 2;
            if (ch_group->homekit_enabled) {
                accessories[accessory]->services[service]->characteristics = calloc(calloc_count, sizeof(homekit_characteristic_t*));
                accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
                accessories[accessory]->services[service]->characteristics[1] = ch_group->ch[1];
            }
        }

        if (is_custom_initial)  {
            ch_group->ch[1]->value.int_value = custom_initial[0];
        } else {
            ch_group->ch[1]->value.int_value = set_initial_state(ch_group->accessory, 1, init_last_state_json, ch_group->ch[1], CH_TYPE_INT8, 100);
        }
        
        if (ch_group->ch[1]->value.int_value == 0) {
            ch_group->ch[1]->value.int_value = 100;
        }

        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, 2);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2), rgbw_brightness, ch_group, LIGHTBULB_BRIGHTNESS_UP);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3), rgbw_brightness, ch_group, LIGHTBULB_BRIGHTNESS_DOWN);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_2), rgbw_brightness, ch_group, LIGHTBULB_BRIGHTNESS_UP);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_3), rgbw_brightness, ch_group, LIGHTBULB_BRIGHTNESS_DOWN);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput, ch_group, 0);
        
        if (lightbulb_group->autodimmer_task_step > 0) {
            LIGHTBULB_AUTODIMMER_TIMER = esp_timer_create(AUTODIMMER_DELAY, false, (void*) ch_group->ch[0], no_autodimmer_called);
        }
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0);
            
            ch_group->ch[0]->value.bool_value = !((bool) set_initial_state(ch_group->accessory, 0, json_context, ch_group->ch[0], CH_TYPE_BOOL, 0));
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1)) {
                ch_group->ch[0]->value.bool_value = false;
            }
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0)) {
                ch_group->ch[0]->value.bool_value = true;
            }
        }
        
        lightbulb_group->ch0->value.bool_value = !lightbulb_group->ch0->value.bool_value;
        lightbulb_task_timer(LIGHTBULB_SET_DELAY_TIMER);
        
        if (lightbulb_group->ch0->value.bool_value) {
            vTaskDelay(MS_TO_TICKS(10));
        }
    }
    
    // *** NEW GARAGE DOOR
    void new_garage_door(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(3, 9, 0);
        ch_group->acc_type = ACC_TYPE_GARAGE_DOOR;
        ch_group->accessory = service_numerator;
        set_killswitch(ch_group, json_context);
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        GD_CURRENT_DOOR_STATE = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_DOOR_STATE, 1);
        GD_TARGET_DOOR_STATE = NEW_HOMEKIT_CHARACTERISTIC(TARGET_DOOR_STATE, 1, .setter_ex=hkc_garage_door_setter);
        GD_OBSTRUCTION_DETECTED = NEW_HOMEKIT_CHARACTERISTIC(OBSTRUCTION_DETECTED, false);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_GARAGE_DOOR_OPENER;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_GARAGE_DOOR_OPENER;
            }
            
            accessories[accessory]->services[service]->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = GD_CURRENT_DOOR_STATE;
            accessories[accessory]->services[service]->characteristics[1] = GD_TARGET_DOOR_STATE;
            accessories[accessory]->services[service]->characteristics[2] = GD_OBSTRUCTION_DETECTED;
            
            //service_iid += 4;
        }

        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        GARAGE_DOOR_CURRENT_TIME = GARAGE_DOOR_TIME_MARGIN_DEFAULT;
        GARAGE_DOOR_WORKING_TIME = GARAGE_DOOR_TIME_OPEN_DEFAULT;
        GARAGE_DOOR_TIME_MARGIN = GARAGE_DOOR_TIME_MARGIN_DEFAULT;
        GARAGE_DOOR_CLOSE_TIME_FACTOR = 1;
        GARAGE_DOOR_VIRTUAL_STOP = virtual_stop(json_context);
        
        ch_group->timer = esp_timer_create(1000, true, (void*) ch_group, garage_door_timer_worker);
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_MARGIN_SET) != NULL) {
            GARAGE_DOOR_TIME_MARGIN = cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_MARGIN_SET)->valuedouble;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_OPEN_SET) != NULL) {
            GARAGE_DOOR_WORKING_TIME = cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_OPEN_SET)->valuedouble;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_CLOSE_SET) != NULL) {
            GARAGE_DOOR_CLOSE_TIME_FACTOR = GARAGE_DOOR_WORKING_TIME / cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_CLOSE_SET)->valuedouble;
        }
        
        GARAGE_DOOR_WORKING_TIME += (GARAGE_DOOR_TIME_MARGIN * 2);
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, 2);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_8), garage_door_stop, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_8), garage_door_stop, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_5), garage_door_sensor, ch_group, GARAGE_DOOR_CLOSING);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_4), garage_door_sensor, ch_group, GARAGE_DOOR_OPENING);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_3), garage_door_sensor, ch_group, GARAGE_DOOR_CLOSED);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_2), garage_door_sensor, ch_group, GARAGE_DOOR_OPENED);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_6), garage_door_obstruction, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_7), garage_door_obstruction, ch_group, 1);
        
        GD_CURRENT_DOOR_STATE_INT = (uint8_t) set_initial_state(ch_group->accessory, 0, json_context, GD_CURRENT_DOOR_STATE, CH_TYPE_INT8, 1);
        if (GD_CURRENT_DOOR_STATE_INT > 1) {
            GD_TARGET_DOOR_STATE_INT = GD_CURRENT_DOOR_STATE_INT - 2;
        } else {
            GD_TARGET_DOOR_STATE_INT = GD_CURRENT_DOOR_STATE_INT;
        }
        
        if (GD_CURRENT_DOOR_STATE_INT == 0) {
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
            garage_door_sensor(99, ch_group, GARAGE_DOOR_OPENING);
        }
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_4), garage_door_sensor, ch_group, GARAGE_DOOR_OPENING)) {
            garage_door_sensor(99, ch_group, GARAGE_DOOR_OPENING);
        }
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3), garage_door_sensor, ch_group, GARAGE_DOOR_CLOSED)) {
            garage_door_sensor(99, ch_group, GARAGE_DOOR_CLOSED);
        }
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2), garage_door_sensor, ch_group, GARAGE_DOOR_OPENED)) {
            garage_door_sensor(99, ch_group, GARAGE_DOOR_OPENED);
        }

        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_6), garage_door_obstruction, ch_group, 0)) {
            garage_door_obstruction(99, ch_group, 0);
        }
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_7), garage_door_obstruction, ch_group, 1)) {
            garage_door_obstruction(99, ch_group, 1);
        }
    }
    
    // *** NEW WINDOW COVER
    void new_window_cover(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(4, 9, 1);
        ch_group->acc_type = ACC_TYPE_WINDOW_COVER;
        ch_group->accessory = service_numerator;
        set_killswitch(ch_group, json_context);
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        uint8_t cover_type = WINDOW_COVER_TYPE_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_TYPE_SET) != NULL) {
            cover_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_TYPE_SET)->valuedouble;
        }
        
        WINDOW_COVER_CH_CURRENT_POSITION = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_POSITION, 0);
        WINDOW_COVER_CH_TARGET_POSITION = NEW_HOMEKIT_CHARACTERISTIC(TARGET_POSITION, 0, .setter_ex=hkc_window_cover_setter);
        WINDOW_COVER_CH_STATE = NEW_HOMEKIT_CHARACTERISTIC(POSITION_STATE, WINDOW_COVER_STOP);
        WINDOW_COVER_CH_OBSTRUCTION = NEW_HOMEKIT_CHARACTERISTIC(OBSTRUCTION_DETECTED, false);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
        
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_WINDOW_COVERING;
            } else {
                switch (cover_type) {
                    case 1:
                        accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_WINDOW;
                        break;
                        
                    case 2:
                        accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_DOOR;
                        break;
                        
                    default:    // case 0:
                        accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_WINDOW_COVERING;
                        break;
                }
            }

            accessories[accessory]->services[service]->characteristics = calloc(5, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = WINDOW_COVER_CH_CURRENT_POSITION;
            accessories[accessory]->services[service]->characteristics[1] = WINDOW_COVER_CH_TARGET_POSITION;
            accessories[accessory]->services[service]->characteristics[2] = WINDOW_COVER_CH_STATE;
            accessories[accessory]->services[service]->characteristics[3] = WINDOW_COVER_CH_OBSTRUCTION;
            
            //service_iid += 5;
        }
        
        WINDOW_COVER_TIME_OPEN = WINDOW_COVER_TIME_DEFAULT;
        WINDOW_COVER_TIME_CLOSE = WINDOW_COVER_TIME_DEFAULT;
        WINDOW_COVER_CORRECTION = WINDOW_COVER_CORRECTION_DEFAULT;
        WINDOW_COVER_MARGIN_SYNC = WINDOW_COVER_MARGIN_SYNC_DEFAULT;
        WINDOW_COVER_VIRTUAL_STOP = virtual_stop(json_context);
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        
        ch_group->timer = esp_timer_create(WINDOW_COVER_POLL_PERIOD_MS, true, (void*) ch_group, window_cover_timer_worker);
        
        ch_group->timer2 = esp_timer_create(WINDOW_COVER_STOP_ENABLE_DELAY_MS, false, (void*) ch_group, window_cover_timer_rearm_stop);
        
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
        
        WINDOW_COVER_HOMEKIT_POSITION = set_initial_state(ch_group->accessory, 0, init_last_state_json, WINDOW_COVER_CH_CURRENT_POSITION, CH_TYPE_INT8, 0);
        WINDOW_COVER_MOTOR_POSITION = (WINDOW_COVER_HOMEKIT_POSITION * (1 + (0.02000000f * WINDOW_COVER_CORRECTION))) / (1 + (0.00020000f * WINDOW_COVER_CORRECTION * WINDOW_COVER_HOMEKIT_POSITION));
        WINDOW_COVER_CH_CURRENT_POSITION->value.int_value = (uint8_t) WINDOW_COVER_HOMEKIT_POSITION;
        WINDOW_COVER_CH_TARGET_POSITION->value.int_value = WINDOW_COVER_CH_CURRENT_POSITION->value.int_value;
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, 2);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), window_cover_diginput, ch_group, WINDOW_COVER_CLOSING);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), window_cover_diginput, ch_group, WINDOW_COVER_OPENING);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2), window_cover_diginput, ch_group, WINDOW_COVER_STOP);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3), window_cover_diginput, ch_group, WINDOW_COVER_CLOSING + 3);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_4), window_cover_diginput, ch_group, WINDOW_COVER_OPENING + 3);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, 2);
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
    }
    
    // *** NEW LIGHT SENSOR
    void new_light_sensor(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(1, 5, 1);
        ch_group->acc_type = ACC_TYPE_LIGHT_SENSOR;
        ch_group->accessory = service_numerator;
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_AMBIENT_LIGHT_LEVEL, 0.0001);
  
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_LIGHT_SENSOR;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_LIGHT_SENSOR;
            }
            
            accessories[accessory]->services[service]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
            
            //service_iid += 2;
        }
        
        LIGHT_SENSOR_TYPE = LIGHT_SENSOR_TYPE_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_TYPE_SET) != NULL) {
            LIGHT_SENSOR_TYPE = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_TYPE_SET)->valuedouble;
        }
        
        LIGHT_SENSOR_FACTOR = LIGHT_SENSOR_FACTOR_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_FACTOR_SET) != NULL) {
            LIGHT_SENSOR_FACTOR = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_FACTOR_SET)->valuedouble;
        }
        
        LIGHT_SENSOR_OFFSET = LIGHT_SENSOR_OFFSET_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_OFFSET_SET) != NULL) {
            LIGHT_SENSOR_OFFSET = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_OFFSET_SET)->valuedouble;
        }
        
        if (LIGHT_SENSOR_TYPE < 2) {
            LIGHT_SENSOR_RESISTOR = LIGHT_SENSOR_RESISTOR_DEFAULT;
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_RESISTOR_SET) != NULL) {
                LIGHT_SENSOR_RESISTOR = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_RESISTOR_SET)->valuedouble * 1000;
            }
            
            LIGHT_SENSOR_POW = LIGHT_SENSOR_POW_DEFAULT;
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_POW_SET) != NULL) {
                LIGHT_SENSOR_POW = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_POW_SET)->valuedouble;
            }
        } else if (LIGHT_SENSOR_TYPE == 2.f) {
            cJSON* data_array = cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_I2C_DATA_ARRAY_SET);
            LIGHT_SENSOR_I2C_BUS = (float) cJSON_GetArrayItem(data_array, 0)->valuedouble;
            LIGHT_SENSOR_I2C_ADDR = (float) cJSON_GetArrayItem(data_array, 1)->valuedouble;
            
            const uint8_t start_bh1750 = 0x10;
            i2c_slave_write(LIGHT_SENSOR_I2C_BUS, LIGHT_SENSOR_I2C_ADDR, NULL, 0, &start_bh1750, 1);
        }
        
        const float poll_period = sensor_poll_period(json_context, LIGHT_SENSOR_POLL_PERIOD_DEFAULT);
        esp_timer_start(esp_timer_create(poll_period * 1000, true, (void*) ch_group, light_sensor_timer_worker));
    }
    
    // *** NEW SECURITY SYSTEM
    void new_sec_system(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(2, 0, 0);
        ch_group->acc_type = ACC_TYPE_SECURITY_SYSTEM;
        ch_group->accessory = service_numerator;
        set_killswitch(ch_group, json_context);
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        uint8_t valid_values_len = 4;
        
        cJSON* modes_array = NULL;
        if (cJSON_GetObjectItemCaseSensitive(json_context, SEC_SYSTEM_MODES_ARRAY_SET) != NULL) {
            modes_array = cJSON_GetObjectItemCaseSensitive(json_context, SEC_SYSTEM_MODES_ARRAY_SET);
            valid_values_len = cJSON_GetArraySize(modes_array);
        }

        uint8_t* current_valid_values = (uint8_t*) malloc(valid_values_len + 1);
        uint8_t* target_valid_values = (uint8_t*) malloc(valid_values_len);
        
        if (valid_values_len < 4) {
            for (uint8_t i = 0; i < valid_values_len; i++) {
                current_valid_values[i] = (uint8_t) cJSON_GetArrayItem(modes_array, i)->valuedouble;
                target_valid_values[i] = current_valid_values[i];
            }
        } else {
            for (uint8_t i = 0; i < 4; i++) {
                current_valid_values[i] = i;
                target_valid_values[i] = i;
            }
        }
        
        current_valid_values[valid_values_len] = 4;
        
        SEC_SYSTEM_CH_CURRENT_STATE = NEW_HOMEKIT_CHARACTERISTIC(SECURITY_SYSTEM_CURRENT_STATE, target_valid_values[valid_values_len - 1], .min_value=(float[]) {current_valid_values[0]}, .valid_values={.count=valid_values_len + 1, .values=current_valid_values});
        SEC_SYSTEM_CH_TARGET_STATE = NEW_HOMEKIT_CHARACTERISTIC(SECURITY_SYSTEM_TARGET_STATE, target_valid_values[valid_values_len - 1], .min_value=(float[]) {target_valid_values[0]}, .max_value=(float[]) {target_valid_values[valid_values_len - 1]}, .valid_values={.count=valid_values_len, .values=target_valid_values}, .setter_ex=hkc_sec_system);
  
        SEC_SYSTEM_REC_ALARM_TIMER = esp_timer_create(SEC_SYSTEM_REC_ALARM_PERIOD_MS, true, (void*) ch_group, sec_system_recurrent_alarm);
        
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_SECURITY_SYSTEM;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_SECURITY_SYSTEM;
            }
            
            accessories[accessory]->services[service]->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = SEC_SYSTEM_CH_CURRENT_STATE;
            accessories[accessory]->services[service]->characteristics[1] = SEC_SYSTEM_CH_TARGET_STATE;
            
            //service_iid += 3;
        }
        
        SEC_SYSTEM_CH_CURRENT_STATE->value.int_value = set_initial_state(ch_group->accessory, 1, json_context, SEC_SYSTEM_CH_TARGET_STATE, CH_TYPE_INT8, target_valid_values[valid_values_len - 1]);
        
        const bool exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (exec_actions_on_boot) {
            if (SEC_SYSTEM_CH_CURRENT_STATE->value.int_value == target_valid_values[valid_values_len - 1]) {
                SEC_SYSTEM_CH_TARGET_STATE->value.int_value = target_valid_values[0];
            }
            hkc_sec_system(SEC_SYSTEM_CH_TARGET_STATE, SEC_SYSTEM_CH_CURRENT_STATE->value);
        } else {
            hkc_sec_system_status(SEC_SYSTEM_CH_TARGET_STATE, SEC_SYSTEM_CH_CURRENT_STATE->value);
        }
        
        free(current_valid_values);
        free(target_valid_values);
    }
    
    // *** NEW TV
    void new_tv(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        cJSON* json_inputs = cJSON_GetObjectItemCaseSensitive(json_context, TV_INPUTS_ARRAY);
        uint8_t inputs = cJSON_GetArraySize(json_inputs);
        
        if (inputs == 0) {
            inputs = 1;
        }
        
        ch_group_t* ch_group = new_ch_group(7, 0, 0);
        ch_group->acc_type = ACC_TYPE_TV;
        ch_group->accessory = service_numerator;
        set_killswitch(ch_group, json_context);
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(ACTIVE, 0, .setter_ex=hkc_tv_active);
        ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(CONFIGURED_NAME, "HAA", .setter_ex=hkc_tv_configured_name);
        ch_group->ch[2] = NEW_HOMEKIT_CHARACTERISTIC(ACTIVE_IDENTIFIER, 1, .setter_ex=hkc_tv_active_identifier);
        ch_group->ch[3] = NEW_HOMEKIT_CHARACTERISTIC(REMOTE_KEY, .setter_ex=hkc_tv_key);
        ch_group->ch[4] = NEW_HOMEKIT_CHARACTERISTIC(POWER_MODE_SELECTION, .setter_ex=hkc_tv_power_mode);
        
        ch_group->ch[5] = NEW_HOMEKIT_CHARACTERISTIC(MUTE, false, .setter_ex=hkc_tv_mute);
        ch_group->ch[6] = NEW_HOMEKIT_CHARACTERISTIC(VOLUME_SELECTOR, .setter_ex=hkc_tv_volume);

        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        
        homekit_service_t* new_tv_input_service(const uint8_t service_number, char* name) {
            INFO("TV Input: %s", name);
            
            homekit_service_t* *input_service = calloc(1, sizeof(homekit_service_t*));
            
            input_service[0] = calloc(1, sizeof(homekit_service_t));
            input_service[0]->id = ((service_number + service - 1) * 50) + 26;
            //input_service[0]->id = service_iid;
            input_service[0]->primary = false;
            input_service[0]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                input_service[0]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_INPUT_SOURCE;
            } else {
                input_service[0]->type = HOMEKIT_SERVICE_INPUT_SOURCE;
            }
            
            input_service[0]->characteristics = calloc(6, sizeof(homekit_characteristic_t*));
            input_service[0]->characteristics[0] = NEW_HOMEKIT_CHARACTERISTIC(IDENTIFIER, service_number);
            input_service[0]->characteristics[1] = NEW_HOMEKIT_CHARACTERISTIC(CONFIGURED_NAME, name, .setter_ex=hkc_tv_input_configured_name);
            input_service[0]->characteristics[2] = NEW_HOMEKIT_CHARACTERISTIC(INPUT_SOURCE_TYPE, HOMEKIT_INPUT_SOURCE_TYPE_HDMI);
            input_service[0]->characteristics[3] = NEW_HOMEKIT_CHARACTERISTIC(IS_CONFIGURED, true);
            input_service[0]->characteristics[4] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_VISIBILITY_STATE, HOMEKIT_CURRENT_VISIBILITY_STATE_SHOWN);
            
            //service_iid += 6;
            
            return *input_service;
        }
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 18;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_TELEVISION;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_TELEVISION;
            }
            
            accessories[accessory]->services[service]->characteristics = calloc(8, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
            accessories[accessory]->services[service]->characteristics[1] = ch_group->ch[1];
            accessories[accessory]->services[service]->characteristics[2] = ch_group->ch[2];
            accessories[accessory]->services[service]->characteristics[3] = NEW_HOMEKIT_CHARACTERISTIC(SLEEP_DISCOVERY_MODE, HOMEKIT_SLEEP_DISCOVERY_MODE_ALWAYS_DISCOVERABLE);
            accessories[accessory]->services[service]->characteristics[4] = ch_group->ch[3];
            accessories[accessory]->services[service]->characteristics[5] = NEW_HOMEKIT_CHARACTERISTIC(PICTURE_MODE, HOMEKIT_PICTURE_MODE_STANDARD);
            accessories[accessory]->services[service]->characteristics[6] = ch_group->ch[4];
            
            //service_iid += 8;
            
            accessories[accessory]->services[service]->linked = calloc(inputs + 1, sizeof(homekit_service_t*));
            
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
                
                accessories[accessory]->services[service]->linked[i] = new_tv_input_service(i + 1, name);
                accessories[accessory]->services[service + i + 2] = accessories[accessory]->services[service]->linked[i];
            }
            
            service++;
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 28;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = false;
            accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_TELEVISION_SPEAKER;
            accessories[accessory]->services[service]->characteristics = calloc(5, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[5];
            accessories[accessory]->services[service]->characteristics[1] = NEW_HOMEKIT_CHARACTERISTIC(ACTIVE, 1);
            accessories[accessory]->services[service]->characteristics[2] = NEW_HOMEKIT_CHARACTERISTIC(VOLUME_CONTROL_TYPE, HOMEKIT_VOLUME_CONTROL_TYPE_RELATIVE);
            accessories[accessory]->services[service]->characteristics[3] = ch_group->ch[6];
            
            //service_iid += 5;
        }
        
        uint32_t configured_name = set_initial_state(ch_group->accessory, 1, init_last_state_json, ch_group->ch[1], CH_TYPE_STRING, 0);
        if (configured_name > 0) {
            homekit_value_destruct(&ch_group->ch[1]->value);
            ch_group->ch[1]->value = HOMEKIT_STRING((char*) configured_name);
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_1), digstate, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_0), digstate, ch_group, 0);
        
        const bool exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0);
            
            ch_group->ch[0]->value.int_value = !((uint8_t) set_initial_state(ch_group->accessory, 0, json_context, ch_group->ch[0], CH_TYPE_INT8, 0));
            if (exec_actions_on_boot) {
                hkc_tv_active(ch_group->ch[0], HOMEKIT_UINT8(!ch_group->ch[0]->value.int_value));
            } else {
                hkc_tv_status_active(ch_group->ch[0], HOMEKIT_UINT8(!ch_group->ch[0]->value.int_value));
            }
            
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1)) {
                if (exec_actions_on_boot) {
                    diginput(99, ch_group, 1);
                } else {
                    digstate(99, ch_group, 1);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0)) {
                ch_group->ch[0]->value.int_value = 1;
                if (exec_actions_on_boot) {
                    diginput(99, ch_group, 0);
                } else {
                    digstate(99, ch_group, 0);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1)) {
                digstate(99, ch_group, 1);
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0)) {
                ch_group->ch[0]->value.int_value = 1;
                digstate(99, ch_group, 0);
            }
        }
    }
    
    // *** NEW FAN
    void new_fan(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(2, 0, 1);
        ch_group->accessory = service_numerator;
        set_killswitch(ch_group, json_context);
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        uint8_t max_speed = 100;
        if (cJSON_GetObjectItemCaseSensitive(json_context, FAN_SPEED_STEPS) != NULL) {
            max_speed = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, FAN_SPEED_STEPS)->valuedouble;
        }
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(ON, false, .setter_ex=hkc_fan_setter);
        ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(ROTATION_SPEED, max_speed, .max_value=(float[]) {max_speed}, .setter_ex=hkc_fan_speed_setter);
        
        ch_group->acc_type = ACC_TYPE_FAN;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        ch_group->timer2 = autoswitch_time(json_context, ch_group);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_FAN;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_FAN;
            }
            
            accessories[accessory]->services[service]->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
            accessories[accessory]->services[service]->characteristics[1] = ch_group->ch[1];
            
            //service_iid += 3;
        }
        
        float saved_max_speed = set_initial_state(ch_group->accessory, 1, init_last_state_json, ch_group->ch[1], CH_TYPE_FLOAT, max_speed);
        if (saved_max_speed > max_speed) {
            saved_max_speed = max_speed;
        }
        ch_group->ch[1]->value.float_value = saved_max_speed;
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_1), digstate, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_0), digstate, ch_group, 0);
        
        const bool exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0);
            
            ch_group->ch[0]->value.bool_value = !((uint8_t) set_initial_state(ch_group->accessory, 0, json_context, ch_group->ch[0], CH_TYPE_BOOL, false));
            if (exec_actions_on_boot) {
                 hkc_fan_setter(ch_group->ch[0], HOMEKIT_UINT8(!ch_group->ch[0]->value.bool_value));
            } else {
                hkc_fan_status_setter(ch_group->ch[0], HOMEKIT_UINT8(!ch_group->ch[0]->value.bool_value));
            }
           
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1)) {
                if (exec_actions_on_boot) {
                    diginput(99, ch_group, 1);
                } else {
                    digstate(99, ch_group, 1);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0)) {
                ch_group->ch[0]->value.int_value = 1;
                if (exec_actions_on_boot) {
                    diginput(99, ch_group, 0);
                } else {
                    digstate(99, ch_group, 0);
                }
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1)) {
                digstate(99, ch_group, 1);
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0)) {
                ch_group->ch[0]->value.int_value = 1;
                digstate(99, ch_group, 0);
            }
        }
    }
    
    // *** POWER MONITOR
    void new_power_monitor(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(7, 11, 4);
        ch_group->acc_type = ACC_TYPE_POWER_MONITOR;
        ch_group->accessory = service_numerator;
        uint8_t homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        
        PM_LAST_SAVED_CONSUPTION = 0;
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CUSTOM_VOLT, 0);
        ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(CUSTOM_AMPERE, 0);
        ch_group->ch[2] = NEW_HOMEKIT_CHARACTERISTIC(CUSTOM_WATT, 0);
        ch_group->ch[3] = NEW_HOMEKIT_CHARACTERISTIC(CUSTOM_CONSUMP, 0);
        ch_group->ch[4] = NEW_HOMEKIT_CHARACTERISTIC(CUSTOM_CONSUMP_BEFORE_RESET, 0);
        ch_group->ch[5] = NEW_HOMEKIT_CHARACTERISTIC(CUSTOM_CONSUMP_RESET_DATE, 0);
        ch_group->ch[6] = NEW_HOMEKIT_CHARACTERISTIC(CUSTOM_CONSUMP_RESET, "", .setter_ex=hkc_custom_consumption_reset_setter);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_POWER_MONITOR;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_POWER_MONITOR;
            }
            
            accessories[accessory]->services[service]->characteristics = calloc(8, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
            accessories[accessory]->services[service]->characteristics[1] = ch_group->ch[1];
            accessories[accessory]->services[service]->characteristics[2] = ch_group->ch[2];
            accessories[accessory]->services[service]->characteristics[3] = ch_group->ch[3];
            accessories[accessory]->services[service]->characteristics[4] = ch_group->ch[4];
            accessories[accessory]->services[service]->characteristics[5] = ch_group->ch[5];
            accessories[accessory]->services[service]->characteristics[6] = ch_group->ch[6];
            
            //service_iid += 8;
        }
        
        ch_group->ch[3]->value.float_value = set_initial_state(ch_group->accessory, 3, init_last_state_json, ch_group->ch[3], CH_TYPE_FLOAT, 0);
        ch_group->ch[4]->value.float_value = set_initial_state(ch_group->accessory, 4, init_last_state_json, ch_group->ch[4], CH_TYPE_FLOAT, 0);
        ch_group->ch[5]->value.int_value = set_initial_state(ch_group->accessory, 5, init_last_state_json, ch_group->ch[5], CH_TYPE_INT, 0);
        
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
        
        int16_t data[3] = { PM_SENSOR_DATA_DEFAULT, PM_SENSOR_DATA_DEFAULT, PM_SENSOR_DATA_DEFAULT };
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, PM_SENSOR_DATA_ARRAY_SET) != NULL) {
            cJSON* gpio_array = cJSON_GetObjectItemCaseSensitive(json_context, PM_SENSOR_DATA_ARRAY_SET);
            for (uint8_t i = 0; i < cJSON_GetArraySize(gpio_array); i++) {
                data[i] = (int16_t) cJSON_GetArrayItem(gpio_array, i)->valuedouble;
                
                if (PM_SENSOR_TYPE < 2 && data[i] >= 0) {
                    set_used_gpio(data[i]);
                }
            }
        }
        
        if (PM_SENSOR_TYPE >= 2) {  // ADE7953 chip
            PM_SENSOR_ADE_BUS = data[0];
            PM_SENSOR_ADE_ADDR = data[1];
            
            uint8_t reg[2];
            uint8_t bytes[2];
            
            reg[0] = 0x01;
            reg[1] = 0x02;
            bytes[0] = 0x00;
            bytes[1] = 0x04;
            i2c_slave_write(data[0], data[1], reg, 2, bytes, 2);
            
            reg[0] = 0x00;
            reg[1] = 0xFE;
            uint8_t byte = 0xAD;
            i2c_slave_write(data[0], data[1], reg, 2, &byte, 1);
            
            reg[0] = 0x01;
            reg[1] = 0x20;
            bytes[1] = 0x30;
            i2c_slave_write(data[0], data[1], reg, 2, bytes, 2);
            
        } else {                    // HLW chip
            PM_SENSOR_HLW_GPIO_CF = data[0];
            
            if (data[0] != PM_SENSOR_DATA_DEFAULT) {
                PM_SENSOR_HLW_GPIO = data[0];
            } else {
                PM_SENSOR_HLW_GPIO = data[1];
            }
            
            adv_hlw_unit_create(data[0], data[1], data[2], (uint8_t) PM_SENSOR_TYPE);
        }

        PM_POLL_PERIOD = sensor_poll_period(json_context, PM_POLL_PERIOD_DEFAULT);
        esp_timer_start(esp_timer_create(PM_POLL_PERIOD * 1000, true, (void*) ch_group, power_monitor_timer_worker));
    }
    
    // *** HISTORICAL
    void new_historical(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        cJSON* data_array = cJSON_GetObjectItemCaseSensitive(json_context, HIST_DATA_ARRAY_SET);
        
        const uint16_t hist_accessory = cJSON_GetArrayItem(data_array, 0)->valuedouble;
        const uint8_t hist_ch = cJSON_GetArrayItem(data_array, 1)->valuedouble;
        const uint8_t hist_size = cJSON_GetArrayItem(data_array, 2)->valuedouble;
        
        INFO("Serv: %i, Ch: %i, Registers: %i", hist_accessory, hist_ch, hist_size * HIST_REGISTERS_BY_BLOCK);
        
        ch_group_t* ch_group = new_ch_group(hist_size, 1, 0);
        ch_group->acc_type = ACC_TYPE_HISTORICAL;
        ch_group->accessory = service_numerator;
        set_killswitch(ch_group, json_context);
        ch_group->homekit_enabled = true;
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, HIST_READ_ON_CLOCK_READY_SET) != NULL) {
            ch_group->child_enabled = (bool) cJSON_GetObjectItemCaseSensitive(json_context, HIST_READ_ON_CLOCK_READY_SET)->valuedouble;
        }
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        ch_group->ch_hist = ch_group_find_by_acc(hist_accessory)->ch[hist_ch];
        
        accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
        //accessories[accessory]->services[service]->id = service_iid;
        accessories[accessory]->services[service]->primary = !(service - 1);
        accessories[accessory]->services[service]->hidden = true;
        accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HISTORICAL_DATA;
        
        accessories[accessory]->services[service]->characteristics = calloc(hist_size + 1, sizeof(homekit_characteristic_t*));
        
        //service_iid += (hist_size + 1);
        
        for (uint8_t i = 0; i < hist_size; i++) {
            // Each block uses 132 + HIST_BLOCK_SIZE bytes
            ch_group->ch[i] = NEW_HOMEKIT_CHARACTERISTIC(CUSTOM_HISTORICAL_DATA, NULL, 0);
            char index[4];
            itoa(i, index, 10);
            if (i < 10) {
                memcpy((char*) (ch_group->ch[i]->type + 7), index, 1);
            } else if (i < 100) {
                memcpy((char*) (ch_group->ch[i]->type + 6), index, 2);
            } else {
                memcpy((char*) (ch_group->ch[i]->type + 5), index, 3);
            }
            
            ch_group->ch[i]->value.data_value = malloc(HIST_BLOCK_SIZE);
            memset(ch_group->ch[i]->value.data_value, 0, HIST_BLOCK_SIZE);
            ch_group->ch[i]->value.data_size = 8;
            accessories[accessory]->services[service]->characteristics[i] = ch_group->ch[i];
        }
        
        HIST_LAST_REGISTER = hist_size * HIST_BLOCK_SIZE;
        
        const float poll_period = sensor_poll_period(json_context, 0);
        if (poll_period > 0.00f) {
            esp_timer_start(esp_timer_create(poll_period * 1000, true, (void*) ch_group->ch_hist, historical_timer_worker));
        }
    }
    
    // *** Accessory Builder
    // Root device
    ch_group_t* root_device_ch_group = new_ch_group(0, 0, 0);
    register_actions(root_device_ch_group, json_config, 0);
    set_accessory_ir_protocol(root_device_ch_group, json_config);
    
    // Saved States Timer Function
    root_device_ch_group->timer = esp_timer_create(SAVE_STATES_DELAY_MS, false, NULL, save_states);
    
    // Exec action 0 from root device
    do_actions(root_device_ch_group, 0);
    
    // Initial delay
    if (cJSON_GetObjectItemCaseSensitive(json_config, ACC_CREATION_DELAY) != NULL) {
        vTaskDelay(cJSON_GetObjectItemCaseSensitive(json_config, ACC_CREATION_DELAY)->valuedouble * MS_TO_TICKS(1000));
    }
    
    // Bridge
    if (bridge_needed) {
        INFO("\n** ACCESSORY BRIDGE");
        new_accessory(0, 2, true, NULL);
        acc_count++;
    }
    
    // Creating HomeKit Accessory
    void new_service(const uint16_t acc_count, uint16_t serv_count, const uint16_t total_services, cJSON* json_accessory, const uint8_t acc_type) {
        service_numerator++;
        
        INFO("\n* SERVICE %i", service_numerator);
        printf("Type %i: ", acc_type);

        if (acc_type == ACC_TYPE_BUTTON ||
            acc_type == ACC_TYPE_DOORBELL) {
            INFO("BUTON EVENT / DOORBELL");
            new_button_event(acc_count, serv_count, total_services, json_accessory, acc_type);
            
        } else if (acc_type == ACC_TYPE_LOCK) {
            INFO("LOCK");
            new_lock(acc_count, serv_count, total_services, json_accessory);
            
        } else if (acc_type >= ACC_TYPE_CONTACT_SENSOR &&
                   acc_type <= ACC_TYPE_MOTION_SENSOR) {
            INFO("BINARY SENSOR");
            new_binary_sensor(acc_count, serv_count, total_services, json_accessory, acc_type);
            
        } else if (acc_type == ACC_TYPE_WATER_VALVE) {
            INFO("VALVE");
            new_valve(acc_count, serv_count, total_services, json_accessory);
        
        } else if (acc_type == ACC_TYPE_THERMOSTAT ||
                   acc_type == ACC_TYPE_THERMOSTAT_WITH_HUM) {
            INFO("THERMOSTAT");
            new_thermostat(acc_count, serv_count, total_services, json_accessory, acc_type);
            
        } else if (acc_type == ACC_TYPE_TEMP_SENSOR) {
            INFO("TEMP SENSOR");
            new_temp_sensor(acc_count, serv_count, total_services, json_accessory);
            
        } else if (acc_type == ACC_TYPE_HUM_SENSOR) {
            INFO("HUM SENSOR");
            new_hum_sensor(acc_count, serv_count, total_services, json_accessory);
            
        } else if (acc_type == ACC_TYPE_TH_SENSOR) {
            INFO("TEMP/HUM SENSOR");
            new_th_sensor(acc_count, serv_count, total_services, json_accessory);
            
        } else if (acc_type == ACC_TYPE_HUMIDIFIER ||
                   acc_type == ACC_TYPE_HUMIDIFIER_WITH_TEMP) {
            INFO("HUMIDIFIER");
            new_humidifier(acc_count, serv_count, total_services, json_accessory, acc_type);
            
        } else if (acc_type == ACC_TYPE_LIGHTBULB) {
            INFO("LIGHTBULB");
            new_lightbulb(acc_count, serv_count, total_services, json_accessory);
            
        } else if (acc_type == ACC_TYPE_GARAGE_DOOR) {
            INFO("GARAGE DOOR");
            new_garage_door(acc_count, serv_count, total_services, json_accessory);
            
        } else if (acc_type == ACC_TYPE_WINDOW_COVER) {
            INFO("WINDOW COVER");
            new_window_cover(acc_count, serv_count, total_services, json_accessory);

        } else if (acc_type == ACC_TYPE_LIGHT_SENSOR) {
            INFO("LIGHT SENSOR");
            new_light_sensor(acc_count, serv_count, total_services, json_accessory);
            
        } else if (acc_type == ACC_TYPE_SECURITY_SYSTEM) {
            INFO("SEC SYSTEM");
            new_sec_system(acc_count, serv_count, total_services, json_accessory);
            
        } else if (acc_type == ACC_TYPE_TV) {
            INFO("TV");
            new_tv(acc_count, serv_count, total_services, json_accessory);
            
        } else if (acc_type == ACC_TYPE_FAN) {
            INFO("FAN");
            new_fan(acc_count, serv_count, total_services, json_accessory);
            
        } else if (acc_type == ACC_TYPE_POWER_MONITOR) {
            INFO("POWER MONITOR");
            new_power_monitor(acc_count, serv_count, total_services, json_accessory);
            
        } else if (acc_type == ACC_TYPE_HISTORICAL) {
            INFO("HISTORICAL");
            new_historical(acc_count, serv_count, total_services, json_accessory);
            
        } else if (acc_type == ACC_TYPE_IAIRZONING) {
            INFO("IAIRZONING");
            new_iairzoning(json_accessory);
        
        } else {    // acc_type == ACC_TYPE_SWITCH || acc_type == ACC_TYPE_OUTLET
            INFO("SWITCH / OUTLET");
            new_switch(acc_count, serv_count, total_services, json_accessory, acc_type);
        }
        
        FREEHEAP();
    }
    
    for (uint16_t i = 0; i < total_accessories; i++) {
        INFO("\n** ACCESSORY %i", i + 1);
        
        cJSON* json_accessory = cJSON_GetArrayItem(json_accessories, i);
        uint8_t acc_type = get_acc_type(json_accessory);
        
        uint8_t service = 0;
        uint8_t total_services = get_total_services(acc_type, json_accessory);
        new_service(acc_count, service, total_services, json_accessory, acc_type);
        
        if (acc_homekit_enabled(json_accessory) && acc_type != ACC_TYPE_IAIRZONING) {
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, EXTRA_SERVICES_ARRAY) != NULL) {
                service += get_service_recount(acc_type, json_accessory);

                cJSON* json_extra_services = cJSON_GetObjectItemCaseSensitive(json_accessory, EXTRA_SERVICES_ARRAY);
                for (uint16_t m = 0; m < cJSON_GetArraySize(json_extra_services); m++) {
                    cJSON* json_extra_service = cJSON_GetArrayItem(json_extra_services, m);
                    
                    acc_type = get_acc_type(json_extra_service);
                    new_service(acc_count, service, 0, json_extra_service, acc_type);
                    service += get_service_recount(acc_type, json_extra_service);
                    
                    main_config.setup_mode_toggle_counter = INT8_MIN;
                    
                    // Extra service creation delay
                    if (cJSON_GetObjectItemCaseSensitive(json_extra_service, ACC_CREATION_DELAY) != NULL) {
                        vTaskDelay(cJSON_GetObjectItemCaseSensitive(json_extra_service, ACC_CREATION_DELAY)->valuedouble * MS_TO_TICKS(1000));
                    }
                    
                    taskYIELD();
                }
            }
            
            acc_count++;
        }
        
        main_config.setup_mode_toggle_counter = INT8_MIN;
        
        // Accessory creation delay
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, ACC_CREATION_DELAY) != NULL) {
            vTaskDelay(cJSON_GetObjectItemCaseSensitive(json_accessory, ACC_CREATION_DELAY)->valuedouble * MS_TO_TICKS(1000));
        }
        
        taskYIELD();
    }
    
    sysparam_set_int32(TOTAL_SERV_SYSPARAM, service_numerator);
    
    printf("\n");
    
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
            case ACC_TYPE_DOORBELL:
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
                
            case ACC_TYPE_WATER_VALVE:
                config.category = homekit_accessory_category_faucet;
                break;
                
            case ACC_TYPE_THERMOSTAT:
            case ACC_TYPE_THERMOSTAT_WITH_HUM:
            case ACC_TYPE_IAIRZONING:
                config.category = homekit_accessory_category_air_conditioner;
                break;
                
            case ACC_TYPE_HUMIDIFIER:
            case ACC_TYPE_HUMIDIFIER_WITH_TEMP:
                config.category = homekit_accessory_category_humidifier;
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
                
            case ACC_TYPE_SECURITY_SYSTEM:
                config.category = homekit_accessory_category_security_system;
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
    config.config_number = (uint16_t) last_config_number;
    
    main_config.setup_mode_toggle_counter = 0;
    
    cJSON_Delete(json_haa);
    cJSON_Delete(init_last_state_json);
    
    if (xTaskCreate(delayed_sensor_task, "delayed", DELAYED_SENSOR_START_TASK_SIZE, NULL, DELAYED_SENSOR_START_TASK_PRIORITY, NULL) != pdPASS) {
        ERROR("Creating delayed_sensor");
    }
    
    int8_t wifi_mode = 0;
    sysparam_get_int8(WIFI_MODE_SYSPARAM, &wifi_mode);
    if (wifi_mode == 4) {
        wifi_mode = 1;
    }
    main_config.wifi_mode = (uint8_t) wifi_mode;
    
    vTaskDelay(MS_TO_TICKS((hwrand() % RANDOM_DELAY_MS) + 1000));
    
    wifi_config_init("HAA", NULL, run_homekit_server, custom_hostname, 0);
    
    led_blink(2);
    
    do_actions(root_device_ch_group, 1);
    
    vTaskDelete(NULL);
}

void ir_capture_task(void* args) {
    const int ir_capture_gpio = ((int) args) - 100;
    INFO("\nIR Capture GPIO: %i\n", ir_capture_gpio);
    gpio_enable(ir_capture_gpio, GPIO_INPUT);
    
    bool read, last = true;
    uint16_t buffer[1024], i, c = 0;
    uint32_t now, new_time, current_time = sdk_system_get_time();
    
    for (;;) {
        read = gpio_read(ir_capture_gpio);
        if (read != last) {
            new_time = sdk_system_get_time();
            buffer[c] = new_time - current_time;
            current_time = new_time;
            last = read;
            c++;
        }
        
        now = sdk_system_get_time();
        if (now - current_time > UINT16_MAX) {
            current_time = now;
            if (c > 0) {
                
                INFO("Packets: %i", c - 1);
                INFO("Standard");
                for (i = 1; i < c; i++) {
                    printf("%s%5d ", i & 1 ? "+" : "-", buffer[i]);
                    
                    if ((i - 1) % 16 == 15) {
                        printf("\n");
                    }
                }
                INFO("\n");
                
                INFO("RAW");
                for (i = 1; i < c; i++) {
                    char haa_code[] = "00";
                    
                    haa_code[0] = baseRaw_dic[(buffer[i] / IR_CODE_SCALE) / IR_CODE_LEN];
                    haa_code[1] = baseRaw_dic[(buffer[i] / IR_CODE_SCALE) % IR_CODE_LEN];
                    
                    printf("%s", haa_code);
                }
                INFO("\n");

                c = 0;
            }
        }
    }
}

void init_task() {
    // Sysparam starter
    sysparam_status_t status;
    status = sysparam_init(SYSPARAMSECTOR, 0);
    if (status != SYSPARAM_OK) {
        wifi_config_remove_sys_param();

        sysparam_create_area(SYSPARAMSECTOR, SYSPARAMSIZE, true);
        sysparam_init(SYSPARAMSECTOR, 0);
    }
    
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    snprintf(main_config.name_value, 11, "HAA-%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    
    int8_t haa_setup = 1;
    
    char *wifi_ssid = NULL;
    sysparam_get_string(WIFI_SSID_SYSPARAM, &wifi_ssid);
    
    //sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 2);    // Force to enter always in setup mode. Only for tests. Keep comment for releases
    
    //sysparam_set_string("ota_repo", "1");             // Simulates Installation with OTA. Only for tests. Keep comment for releases
    
    void enter_setup(const int param) {
        reset_uart();
        printf_header();
        INFO("SETUP MODE");
        wifi_config_init("HAA", NULL, NULL, main_config.name_value, param);
    }
    
    sysparam_get_int8(HAA_SETUP_MODE_SYSPARAM, &haa_setup);
    if (haa_setup > 99) {
        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
        
        reset_uart();
        printf_header();
        INFO("IR CAPTURE MODE\n");
        const int ir_capture_gpio = haa_setup;
        xTaskCreate(ir_capture_task, "ir_cap", IR_CAPTURE_TASK_SIZE, (void*) ir_capture_gpio, IR_CAPTURE_TASK_PRIORITY, NULL);
        
    } else if (haa_setup > 0 || !wifi_ssid) {
        enter_setup(0);
        
    } else {
        haa_setup = 0;
        
        // Arming emergency Setup Mode
        void arming() {
            sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 1);
            sysparam_get_int8(HAA_SETUP_MODE_SYSPARAM, &haa_setup);
        }
        arming();
        
        if (haa_setup != 1) {
            ERROR("Sysparam. Recovering");
            sysparam_compact();
            arming();
        }

        if (haa_setup == 1) {
#ifdef HAA_DEBUG
            free_heap_watchdog();
            esp_timer_start(esp_timer_create(1000, true, NULL, free_heap_watchdog));
#endif // HAA_DEBUG
            
            // Arming emergency Setup Mode
            esp_timer_start(esp_timer_create(EXIT_EMERGENCY_SETUP_MODE_TIME, false, NULL, disable_emergency_setup));
            
            name.value = HOMEKIT_STRING(main_config.name_value);
            
            xTaskCreate(normal_mode_init, "init", INITIAL_SETUP_TASK_SIZE, NULL, INITIAL_SETUP_TASK_PRIORITY, NULL);
            
        } else {
            enter_setup(1);
        }
    }
    
    if (wifi_ssid) {
        free(wifi_ssid);
    }
    
    vTaskDelete(NULL);
}

void user_init(void) {
    gpio_enable(15, GPIO_INPUT);
    gpio_enable(16, GPIO_INPUT);
    gpio_enable(3, GPIO_INPUT);
    gpio_enable(2, GPIO_INPUT);
    gpio_enable(1, GPIO_INPUT);
    gpio_enable(0, GPIO_INPUT);
    gpio_enable(4, GPIO_INPUT);
    gpio_enable(5, GPIO_INPUT);
    gpio_enable(12, GPIO_INPUT);
    gpio_enable(13, GPIO_INPUT);
    gpio_enable(14, GPIO_INPUT);
    
    sdk_wifi_station_set_auto_connect(false);
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_disconnect();
    sdk_wifi_set_sleep_type(WIFI_SLEEP_NONE);
    
    xTaskCreate(init_task, "init", 512, NULL, (tskIDLE_PRIORITY + 2), NULL);
}

