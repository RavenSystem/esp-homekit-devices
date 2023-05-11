/*
 * Home Accessory Architect
 *
 * Copyright 2019-2023 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifdef ESP_PLATFORM

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "errno.h"
#include "esp32_port.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_attr.h"

#define sdk_system_get_time_raw()           esp_timer_get_time()

#define HAA_ENTER_CRITICAL_TASK()           portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; taskENTER_CRITICAL(&mux)
#define HAA_EXIT_CRITICAL_TASK()            taskEXIT_CRITICAL(&mux)

#ifndef CONFIG_IDF_TARGET_ESP32C2
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#endif

#else   // ESP_OPEN_RTOS

#include <unistd.h>
#include <string.h>
#include <esp/uart.h>
#include <FreeRTOS.h>
#include <task.h>
#include <espressif/esp_common.h>
#include <rboot-api.h>
#include <sysparam.h>
#include <esplibs/libmain.h>
#include <adv_nrzled.h>

#define HAA_ENTER_CRITICAL_TASK()           taskENTER_CRITICAL()
#define HAA_EXIT_CRITICAL_TASK()            taskEXIT_CRITICAL()

#endif

#include <math.h>

#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>
#include <lwip/etharp.h>

#ifdef HAA_DEBUG
#include <lwip/stats.h>
#endif // HAA_DEBUG

#include <timers_helper.h>
#include <cJSON.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <rs_ping.h>
#include <adv_logger.h>
#include <adv_button.h>
#include <adv_hlw.h>
#include <dht.h>
#include <ds18b20.h>
#include <raven_ntp.h>
#include <unistring.h>
#include <adv_i2c.h>
#include <adv_pwm.h>

#include "setup.h"
#include "ir_code.h"

#include "extra_characteristics.h"
#include "header.h"
#include "types.h"

main_config_t main_config = {
#ifdef ESP_PLATFORM
    .adc_dac_data = NULL,
    .pwmh_channels = NULL,
#endif
    
    .wifi_arp_count = 0,
    .wifi_arp_count_max = WIFI_WATCHDOG_ARP_PERIOD_DEFAULT,
    .wifi_status = WIFI_STATUS_DISCONNECTED,
    .wifi_channel = 0,
    .wifi_ip = 0,
    .wifi_ping_max_errors = 255,
    .wifi_error_count = 0,
    .wifi_roaming_count = 1,
    
    .setup_mode_toggle_counter = INT8_MIN,
    .setup_mode_toggle_counter_max = SETUP_MODE_DEFAULT_ACTIVATE_COUNT,
    .setup_mode_time = 0,
    
    .enable_homekit_server = false,
    
    .ir_tx_freq = 13,
    .ir_tx_gpio = MAX_GPIOS,
    .ir_tx_inv = false,
    
    .rf_tx_gpio = MAX_GPIOS,
    .rf_tx_inv = false,
    
    .ping_poll_period = PING_POLL_PERIOD_DEFAULT,
    
    .clock_ready = false,
    .timetable_ready = false,
    
    //.used_gpio = 0,
    
    .setup_mode_toggle_timer = NULL,
    
    .ch_groups = NULL,
    .lightbulb_groups = NULL,
    .ping_inputs = NULL,
    .last_states = NULL,
    
    .status_led = NULL,
    
    .ntp_host = NULL,
    .timetable_actions = NULL,
    
    .uart_receiver_data = NULL,
    .uart_recv_is_working = false,
};

#ifdef ESP_PLATFORM
static unsigned int IRAM_ATTR private_abs(int number) {
#else
static unsigned int IRAM private_abs(int number) {
#endif
    return (number < 0 ? -number : number);
}

static void show_freeheap() {
#ifdef ESP_PLATFORM
    INFO("Free Heap %li", xPortGetFreeHeapSize());
#else
    INFO("Free Heap %d", xPortGetFreeHeapSize());
#endif
}

// PWM Part
#ifdef ESP_PLATFORM
pwmh_channel_t* get_pwmh_channel(const uint8_t gpio) {
    pwmh_channel_t* pwmh_channel = main_config.pwmh_channels;
    while (pwmh_channel &&
           pwmh_channel->gpio != gpio) {
        pwmh_channel = pwmh_channel->next;
    }
    
    return pwmh_channel;
}

void haa_pwm_set_duty(const uint8_t gpio, const uint16_t duty) {
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    
    pwmh_channel_t* pwmh_channel = get_pwmh_channel(gpio);
    if (pwmh_channel) {
        int new_duty = pwmh_channel->leading ? UINT16_MAX - duty : duty;
        new_duty = new_duty >> PWMH_BITS_DIVISOR;
        
        taskENTER_CRITICAL(&mux);
        
        ledc_set_duty(LEDC_LOW_SPEED_MODE, pwmh_channel->channel, new_duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, pwmh_channel->channel);
        
        taskEXIT_CRITICAL(&mux);
        
    } else {
        taskENTER_CRITICAL(&mux);
        
        adv_pwm_set_duty(gpio, duty);
        
        taskEXIT_CRITICAL(&mux);
    }
}
#else
#define haa_pwm_set_duty(gpio, duty)        adv_pwm_set_duty(gpio, duty)
#endif

// https://martin.ankerl.com/2012/01/25/optimized-approximative-pow-in-c-and-cpp/
double fast_precise_pow(double a, double b) {
    int32_t e = private_abs((int32_t) b);
    
    union {
        double d;
        int32_t x[2];
    } u = { a };
    
    u.x[1] = (int32_t) ((b - e) * (u.x[1] - 1072632447) + 1072632447);
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

/*
int get_used_gpio(const int16_t gpio) {
    if (gpio > -1 && gpio < MAX_GPIOS) {
        const uint64_t bit = 1 << gpio;
        return (bool) (main_config.used_gpio & bit);
    }
    
    return false;
}

void set_used_gpio(const int16_t gpio) {
    if (gpio > -1 && gpio < MAX_GPIOS) {
        if (gpio == 1) {
            gpio_set_iomux_function(1, IOMUX_GPIO1_FUNC_GPIO);
        } else if (gpio == 3) {
            gpio_set_iomux_function(3, IOMUX_GPIO3_FUNC_GPIO);
        }
        
        const uint64_t bit = 1 << gpio;
        main_config.used_gpio |= bit;
    }
}

void set_unused_gpios() {

#ifndef ESP_PLATFORM

    for (int i = 0; i < MAX_GPIOS; i++) {
        if (i == 6) {
            i += 6;
        }
        
        if (!get_used_gpio(i)) {
            //gpio_enable(i, GPIO_INPUT);
            gpio_disable(i);
        }
    }

#endif

}
*/

mcp23017_t* mcp_find_by_index(const int index) {
    mcp23017_t* mcp23017 = main_config.mcp23017s;
    while (mcp23017 && mcp23017->index != index) {
        mcp23017 = mcp23017->next;
    }
    
    return mcp23017;
}

void extended_gpio_write(const int extended_gpio, bool value) {
    if (extended_gpio < 100) {
        gpio_write(extended_gpio, value);
        
    } else {    // MCP23017
        const unsigned int gpio_type = extended_gpio / 100;
        mcp23017_t* mcp23017 = mcp_find_by_index(gpio_type);
        
        if (mcp23017) {
            unsigned int gpio = extended_gpio % 100;
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
            
            adv_i2c_slave_write(mcp23017->bus, mcp23017->addr, &mcp_reg, 1, &mcp_outs, 1);
        }
    }
}

void led_code_run(TimerHandle_t xTimer) {
    unsigned int delay = STATUS_LED_DURATION_OFF;
    
    main_config.status_led->status = !main_config.status_led->status;
    extended_gpio_write(main_config.status_led->gpio, main_config.status_led->status);
    
    if (main_config.status_led->status == main_config.status_led->inverted) {
        main_config.status_led->count++;
    } else {
        delay = STATUS_LED_DURATION_ON;
    }
    
    if (main_config.status_led->count < main_config.status_led->times) {
        rs_esp_timer_change_period(xTimer, delay);
    }
}

void led_blink(const uint8_t times) {
    if (main_config.status_led) {
        rs_esp_timer_stop(main_config.status_led->timer);
        
        main_config.status_led->times = times;
        main_config.status_led->status = main_config.status_led->inverted;
        main_config.status_led->count = 0;
        
        led_code_run(main_config.status_led->timer);
    }
}

void led_create(const uint16_t gpio, const bool inverted) {
    main_config.status_led = malloc(sizeof(led_t));
    memset(main_config.status_led, 0, sizeof(*main_config.status_led));
    
    main_config.status_led->timer = rs_esp_timer_create(10, false, NULL, led_code_run);
    
    main_config.status_led->gpio = gpio;
    
    main_config.status_led->inverted = inverted;
    main_config.status_led->status = inverted;
    
    extended_gpio_write(gpio, inverted);
}

const char http_header1[] = " HTTP/1.1\r\nHost: ";
const char http_header2[] = "\r\nUser-Agent: HAA/"HAA_FIRMWARE_VERSION"\r\nConnection: close\r\n";
const char http_header_len[] = "Content-length: ";

int new_net_con(char* host, uint16_t port_n, bool is_udp, uint8_t* payload, size_t payload_len, int* s, uint8_t rcvtimeout_s, int rcvtimeout_us) {
    struct addrinfo* res;
    struct addrinfo hints;
    int result;
    char port[8];
    *s = -2;
    itoa(port_n, port, 10);
    
    hints.ai_family = AF_UNSPEC;
    
    if (!is_udp) {
        hints.ai_socktype = SOCK_STREAM;
    } else {
        hints.ai_socktype = SOCK_DGRAM;
    }
    
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        free(res);
        return -3;
    }
    
    *s = socket(res->ai_family, res->ai_socktype, 0);
    if (*s < 0) {
        free(res);
        return -2;
    }
    
    const struct timeval sndtimeout = { 3, 0 };
    setsockopt(*s, SOL_SOCKET, SO_SNDTIMEO, &sndtimeout, sizeof(sndtimeout));
    
    if (!is_udp) {
        const struct timeval rcvtimeout = { rcvtimeout_s, rcvtimeout_us };
        setsockopt(*s, SOL_SOCKET, SO_RCVTIMEO, &rcvtimeout, sizeof(rcvtimeout));
        
        if (connect(*s, res->ai_addr, res->ai_addrlen) != 0) {
            free(res);
            return -1;
        }
        
        result = write(*s, payload, payload_len);
        
    } else {
        result = sendto(*s, payload, payload_len, 0, res->ai_addr, res->ai_addrlen);
    }
    
    free(res);
    return result;
}

void hkc_autooff_setter_task(TimerHandle_t xTimer);
void do_actions(ch_group_t* ch_group, uint8_t action);
void do_wildcard_actions(ch_group_t* ch_group, uint8_t index, const float action_value);

#ifdef HAA_DEBUG
uint32_t free_heap = 0;
void free_heap_watchdog() {
    uint32_t size = xPortGetFreeHeapSize();
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

void random_task_delay() {
    vTaskDelay( ( hwrand() % MS_TO_TICKS(RANDOM_DELAY_MS) ) + MS_TO_TICKS(200) );
}

void disable_emergency_setup(TimerHandle_t xTimer) {
    INFO("Disarming Setup");
    sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
    rs_esp_timer_delete(xTimer);
}

uint16_t get_absolut_index(const uint16_t base, const int16_t rel_index) {
    if (rel_index > 7000) {
        return base + rel_index - 7000;
    }
    
    if (rel_index <= 0) {
        return base + rel_index;
    }
    
    return rel_index;
}

int process_hexstr(const char* string, uint8_t** output_hex_string, unistring_t** unistrings) {
    const int len = strlen(string) >> 1;
    uint8_t* hex_string = malloc(len);
    //memset(hex_string, 0, len);
    
    char buffer[3];
    buffer[2] = 0;
    
    for (int i = 0; i < len; i++) {
        buffer[0] = string[i << 1];
        buffer[1] = string[(i << 1) + 1];
                           
        hex_string[i] = (uint8_t) strtol(buffer, NULL, 16);
    }
    
    *output_hex_string = uni_memdup(hex_string, len, unistrings);
    
    free(hex_string);
    
    return len;
}

action_task_t* new_action_task() {
    action_task_t* action_task = malloc(sizeof(action_task_t));
    memset(action_task, 0, sizeof(*action_task));
    
    return action_task;
}

ch_group_t* new_ch_group(const uint8_t chs, const uint8_t nums_i, const uint8_t nums_f, const uint8_t last_wildcard_actions) {
    ch_group_t* ch_group = malloc(sizeof(ch_group_t));
    memset(ch_group, 0, sizeof(*ch_group));
    
    if (chs > 0) {
        const size_t size = chs * sizeof(homekit_characteristic_t*);
        ch_group->chs = chs;
        ch_group->ch = (homekit_characteristic_t**) malloc(size);
        memset(ch_group->ch, 0, size);
    }
    
    if (nums_i > 0) {
        const size_t size = nums_i * sizeof(int8_t);
        ch_group->num_i = (int8_t*) malloc(size);
        memset(ch_group->num_i, 0, size);
    }
    
    if (nums_f > 0) {
        const size_t size = nums_f * sizeof(float);
        ch_group->num_f = (float*) malloc(size);
        memset(ch_group->num_f, 0, size);
    }
    
    if (last_wildcard_actions > 0) {
        ch_group->last_wildcard_action = (float*) malloc(last_wildcard_actions * sizeof(float));
        
        for (int i = 0; i < last_wildcard_actions; i++) {
            ch_group->last_wildcard_action[i] = NO_LAST_WILDCARD_ACTION;
        }
    }
    
    ch_group->main_enabled = true;
    ch_group->child_enabled = true;
    
    ch_group->next = main_config.ch_groups;
    main_config.ch_groups = ch_group;
    
    return ch_group;
}

ch_group_t* ch_group_find(homekit_characteristic_t* ch) {
    ch_group_t* ch_group = main_config.ch_groups;
    while (ch_group) {
        for (int i = 0; i < ch_group->chs; i++) {
            if (ch_group->ch[i] == ch &&
                ch_group->serv_type != SERV_TYPE_DATA_HISTORY &&
                (ch_group->serv_type != SERV_TYPE_FREE_MONITOR ||
                 (ch_group->serv_type == SERV_TYPE_FREE_MONITOR && i == 0))) {
                return ch_group;
            }
        }
        
        ch_group = ch_group->next;
    }

    return NULL;
}

ch_group_t* ch_group_find_by_serv(const uint16_t service) {
    ch_group_t* ch_group = main_config.ch_groups;
    while (ch_group &&
           ch_group->serv_index != service) {
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

#ifndef CONFIG_IDF_TARGET_ESP32C2
addressled_t* addressled_find(const uint8_t gpio) {
    addressled_t* addressled = main_config.addressleds;
    while (addressled &&
           addressled->gpio != gpio) {
        addressled = addressled->next;
    }

    return addressled;
}

addressled_t* new_addressled(uint8_t gpio, const uint16_t max_range, float time_0h, float time_1h, float time_0l) {
    addressled_t* addressled = addressled_find(gpio);
    
    if (addressled) {
        if (max_range > addressled->max_range) {
            addressled->max_range = max_range;
        }
    } else {
        addressled = malloc(sizeof(addressled_t));
        memset(addressled, 0, sizeof(*addressled));
        
        addressled->gpio = gpio;
        addressled->max_range = max_range;
        
        addressled->map[0] = 1;
        addressled->map[1] = 0;
        addressled->map[2] = 2;
        addressled->map[3] = 3;
        addressled->map[4] = 4;
        
#ifdef ESP_PLATFORM
        rmt_tx_channel_config_t tx_chan_config = {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .gpio_num = gpio,
            .mem_block_symbols = HAA_RMT_LED_STRIP_BLOCK_SYMBOLS,
            .resolution_hz = HAA_RMT_LED_STRIP_RESOLUTION_HZ,
            .trans_queue_depth = HAA_RMT_LED_STRIP_QUEUE_DEPTH,
        };
        rmt_new_tx_channel(&tx_chan_config, &addressled->rmt_channel_handle);
        
        rmt_bytes_encoder_config_t bytes_encoder_config = {
            .bit0 = {
                .level0 = 1,
                .duration0 = time_0h * (HAA_RMT_LED_STRIP_RESOLUTION_HZ / 1000000), // T0H
                .level1 = 0,
                .duration1 = time_0l * (HAA_RMT_LED_STRIP_RESOLUTION_HZ / 1000000), // T0L
            },
            .bit1 = {
                .level0 = 1,
                .duration0 = time_1h * (HAA_RMT_LED_STRIP_RESOLUTION_HZ / 1000000), // T1H
                .level1 = 0,
                .duration1 = (time_0h + time_0l - time_1h) * (HAA_RMT_LED_STRIP_RESOLUTION_HZ / 1000000), // T1L
            },
            .flags.msb_first = 1
        };
        
        rmt_new_bytes_encoder(&bytes_encoder_config, &addressled->rmt_encoder_handle);
#else
        addressled->time_0 = nrz_ticks(time_0h);
        addressled->time_1 = nrz_ticks(time_1h);
        addressled->period = nrz_ticks(time_0h + time_0l);
#endif
        
        addressled->next = main_config.addressleds;
        main_config.addressleds = addressled;
    }
    
    return addressled;
}
#endif  // no CONFIG_IDF_TARGET_ESP32C2

float get_hkch_value(homekit_characteristic_t* ch) {
    switch (ch->value.format) {
        case HOMEKIT_FORMAT_BOOL:
            return (ch->value.bool_value == true);
            
        case HOMEKIT_FORMAT_UINT8:
        case HOMEKIT_FORMAT_UINT16:
        case HOMEKIT_FORMAT_UINT32:
        case HOMEKIT_FORMAT_UINT64:
        case HOMEKIT_FORMAT_INT:
            return ch->value.int_value;
            
        case HOMEKIT_FORMAT_FLOAT:
            return ch->value.float_value;
    }
    
    return 0;
}

void save_data_history(homekit_characteristic_t* ch_target) {
    if (!main_config.clock_ready) {
        return;
    }
    
    time_t time = raven_ntp_get_time_t();
    
    ch_group_t* ch_group = main_config.ch_groups;
    while (ch_group) {
        if (ch_group->serv_type == SERV_TYPE_DATA_HISTORY && ch_group->ch[ch_group->chs - 1] == ch_target && ch_group->main_enabled) {
            float value = get_hkch_value(ch_target);
            
            uint32_t final_time = time;
            int32_t final_data = value * FLOAT_FACTOR_SAVE_AS_INT;
            
            //INFO("Saved %i, %i (%0.5f)", final_time, final_data, value);
            
            uint32_t last_register = HIST_LAST_REGISTER;
            last_register += HIST_REGISTER_SIZE;
            uint32_t current_ch = last_register / HIST_BLOCK_SIZE;
            uint32_t current_pos = last_register % HIST_BLOCK_SIZE;
            
            if (current_ch + 1 >= ch_group->chs) {
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

void data_history_timer_worker(TimerHandle_t xTimer) {
    homekit_characteristic_t* ch = (homekit_characteristic_t*) pvTimerGetTimerID(xTimer);
    save_data_history(ch);
}

void wifi_resend_arp() {
#ifdef ESP_PLATFORM
    struct netif* netif = netif_default;
#else
    struct netif* netif = sdk_system_get_netif(STATION_IF);
#endif
    
    if (netif && (netif->flags & NETIF_FLAG_LINK_UP) && (netif->flags & NETIF_FLAG_UP)) {
        LOCK_TCPIP_CORE();
        
        etharp_gratuitous(netif);
        
        UNLOCK_TCPIP_CORE();
    }
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
        ping_result = rs_ping(target_ip);
    }
    
    if (res) {
        freeaddrinfo(res);
    }
    
    return ping_result;
}

void reboot_task() {
    led_blink(5);
    
    INFO("\nRebooting\n");
    rs_esp_timer_stop_forced(WIFI_WATCHDOG_TIMER);
    
    random_task_delay();
    
    sdk_system_restart();
}

void reboot_haa() {
    if (xTaskCreate(reboot_task, "REB", REBOOT_TASK_SIZE, NULL, REBOOT_TASK_PRIORITY, NULL) != pdPASS) {
        ERROR("New REB");
        homekit_remove_oldest_client();
    }
}

void ntp_task() {
    int result = -10;
    unsigned int tries = 0;
    char* ntp_host = main_config.ntp_host;
    
    if (!ntp_host) {
        uint32_t gw_addr = wifi_config_get_full_gw();
        if (gw_addr > 0) {
#ifdef ESP_PLATFORM
            esp_ip4_addr_t esp_ip4_gw_addr;
#else
            struct ip4_addr esp_ip4_gw_addr;
#endif
            esp_ip4_gw_addr.addr = gw_addr;
            
            char gw_host[16];
            snprintf(gw_host, 16, IPSTR, IP2STR(&esp_ip4_gw_addr));
            
            ntp_host = strdup(gw_host);
            
        } else {
            ERROR("NTP GW IP");
            vTaskDelete(NULL);
        }
    }
    
    for (;;) {
        tries++;
        
        if (xSemaphoreTake(main_config.network_busy_mutex, MS_TO_TICKS(2000)) == pdTRUE) {
            result = raven_ntp_update(ntp_host);
            
            xSemaphoreGive(main_config.network_busy_mutex);
            
            INFO("NTP %s (%i)", ntp_host, result);
            
            if (result != 0 && tries < 4) {
                if (tries == 2) {
                    if (ntp_host != main_config.ntp_host) {
                        free(ntp_host);
                    }
                    ntp_host = strdup(NTP_SERVER_FALLBACK);
                }
                vTaskDelay(MS_TO_TICKS(3000));
                
            } else {
                if (!main_config.clock_ready && result == 0) {
                    main_config.clock_ready = true;
                    
                    // Save Data Histories when clock is ready
                    ch_group_t* ch_group = main_config.ch_groups;
                    while (ch_group) {
                        if (ch_group->serv_type == SERV_TYPE_DATA_HISTORY && ch_group->child_enabled) {
                            save_data_history(ch_group->ch[ch_group->chs - 1]);
                        }
                        
                        ch_group = ch_group->next;
                    }
                }
                
                break;
            }
        }
    }
    
    if (ntp_host != main_config.ntp_host) {
        free(ntp_host);
    }
    
    vTaskDelete(NULL);
}

void ntp_timer_worker(TimerHandle_t xTimer) {
    if (!homekit_is_pairing()) {
        if (main_config.wifi_status != WIFI_STATUS_CONNECTED) {
            raven_ntp_get_time_t();
        } else if (xTaskCreate(ntp_task, "NTP", NTP_TASK_SIZE, NULL, NTP_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("New NTP");
            homekit_remove_oldest_client();
            raven_ntp_get_time_t();
        }
    } else {
        ERROR("HK pair");
        raven_ntp_get_time_t();
    }
}

void wifi_ping_gw_task() {
    if (xSemaphoreTake(main_config.network_busy_mutex, MS_TO_TICKS(500)) == pdTRUE) {
        uint32_t gw_addr = wifi_config_get_full_gw();
        if (gw_addr > 0) {
#ifdef ESP_PLATFORM
            esp_ip4_addr_t esp_ip4_gw_addr;
#else
            struct ip4_addr esp_ip4_gw_addr;
#endif
            esp_ip4_gw_addr.addr = gw_addr;
            
            char gw_host[16];
            snprintf(gw_host, 16, IPSTR, IP2STR(&esp_ip4_gw_addr));
            
            int ping_result = ping_host(gw_host);
            
            if (ping_result == 0) {
                main_config.wifi_error_count++;
                ERROR("GW %s ping (%i/%i)", gw_host, main_config.wifi_error_count - 1, main_config.wifi_ping_max_errors - 1);
            } else if (ping_result == 1) {
                main_config.wifi_error_count = 0;
            }
        }
        
        xSemaphoreGive(main_config.network_busy_mutex);
    }
    
    vTaskDelete(NULL);
}

void wifi_reconnection_task(void* args) {
    main_config.wifi_status = WIFI_STATUS_DISCONNECTED;
    rs_esp_timer_stop_forced(WIFI_WATCHDOG_TIMER);
    homekit_mdns_announce_pause();
    
    INFO("Recon start");
    
    do_actions(ch_group_find_by_serv(SERV_TYPE_ROOT_DEVICE), 4);

    led_blink(3);
    
    if ((uint32_t) args == 1) {
        vTaskDelay(MS_TO_TICKS(500));
        wifi_config_reset();
    }
    
    for (;;) {
        vTaskDelay(MS_TO_TICKS(WIFI_RECONNECTION_POLL_PERIOD_MS));
        
        const int new_ip = wifi_config_get_ip();
        if (new_ip >= 0) {
            main_config.wifi_status = WIFI_STATUS_CONNECTED;
            main_config.wifi_error_count = 0;
            main_config.wifi_arp_count = 0;
            
#ifdef ESP_PLATFORM
            uint8_t channel_primary = main_config.wifi_channel;
            esp_wifi_get_channel(&channel_primary, NULL);
            main_config.wifi_channel = channel_primary;
#else
            main_config.wifi_channel = sdk_wifi_get_channel();
#endif
            
            main_config.wifi_ip = new_ip;
            
            save_last_working_phy();
            
            random_task_delay();
            
            homekit_mdns_announce();
            
            do_actions(ch_group_find_by_serv(SERV_TYPE_ROOT_DEVICE), 3);
            
            rs_esp_timer_start_forced(WIFI_WATCHDOG_TIMER);
            
            INFO("Recon OK");
            
            break;
            
        } else if (main_config.wifi_status <= WIFI_STATUS_DISCONNECTED) {
            INFO("Recon...");
            
#ifndef ESP_PLATFORM
            int8_t phy_mode = 4;    // main_config.wifi_status == WIFI_STATUS_LONG_DISCONNECTED
            if (main_config.wifi_status == WIFI_STATUS_DISCONNECTED) {
                phy_mode = 3;
                sysparam_get_int8(WIFI_LAST_WORKING_PHY_SYSPARAM, &phy_mode);
            }
#endif
            
            main_config.wifi_status = WIFI_STATUS_CONNECTING;
            
#ifdef ESP_PLATFORM
            wifi_config_connect(1, true);
#else
            wifi_config_connect(1, phy_mode, true);
#endif
            
        } else {    // main_config.wifi_status == WIFI_STATUS_CONNECTING
            main_config.wifi_error_count++;
            if (main_config.wifi_error_count > WIFI_DISCONNECTED_LONG_TIME) {
                ERROR("Discon long");
                main_config.wifi_error_count = 0;
                main_config.wifi_status = WIFI_STATUS_LONG_DISCONNECTED;
                
                led_blink(3);
                
                vTaskDelay(MS_TO_TICKS(600));
                
                do_actions(ch_group_find_by_serv(SERV_TYPE_ROOT_DEVICE), 5);
            }
        }
    }
    
    vTaskDelete(NULL);
}

void wifi_watchdog() {
    //INFO("Wifi status %i, sleep mode %i", main_config.wifi_status, sdk_wifi_get_sleep_type());
    const int current_ip = wifi_config_get_ip();
    if (current_ip >= 0 && main_config.wifi_error_count <= main_config.wifi_ping_max_errors) {
#ifdef ESP_PLATFORM
        uint8_t channel_primary = main_config.wifi_channel;
        esp_wifi_get_channel(&channel_primary, NULL);
        unsigned int current_channel = channel_primary;
#else
        unsigned int current_channel = sdk_wifi_get_channel();
#endif
        
        if (main_config.wifi_mode == 3) {
            if (main_config.wifi_roaming_count == 0) {
                rs_esp_timer_change_period(WIFI_WATCHDOG_TIMER, WIFI_WATCHDOG_POLL_PERIOD_MS);
            }
            
            main_config.wifi_roaming_count++;
            
            if (main_config.wifi_roaming_count >= WIFI_WATCHDOG_ROAMING_PERIOD) {
                rs_esp_timer_change_period(WIFI_WATCHDOG_TIMER, 5000);
                main_config.wifi_roaming_count = 0;
                wifi_config_smart_connect();
            }
        }
        
        if (main_config.wifi_channel != current_channel) {
            main_config.wifi_channel = current_channel;
            INFO("Wifi new Ch %i", current_channel);
            homekit_mdns_announce();
            do_actions(ch_group_find_by_serv(SERV_TYPE_ROOT_DEVICE), 6);
        }
        
        if (main_config.wifi_ip != current_ip) {
            main_config.wifi_ip = current_ip;
            homekit_mdns_announce();
            do_actions(ch_group_find_by_serv(SERV_TYPE_ROOT_DEVICE), 7);
        }
        
        main_config.wifi_arp_count++;
        if (main_config.wifi_arp_count >= main_config.wifi_arp_count_max) {
            main_config.wifi_arp_count = 0;
            
            wifi_resend_arp();
        }
        
        if (main_config.wifi_ping_max_errors != 255 && !homekit_is_pairing()) {
            if (xTaskCreate(wifi_ping_gw_task, "GWP", WIFI_PING_GW_TASK_SIZE, NULL, WIFI_PING_GW_TASK_PRIORITY, NULL) != pdPASS) {
                ERROR("New GWP");
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
        
        if (xTaskCreate(wifi_reconnection_task, "RCN", WIFI_RECONNECTION_TASK_SIZE, (void*) force_disconnect, WIFI_RECONNECTION_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("New RCN");
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
    if (xSemaphoreTake(main_config.network_busy_mutex, MS_TO_TICKS(500)) == pdTRUE) {
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
            
            unsigned int i = 0;
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
        
        xSemaphoreGive(main_config.network_busy_mutex);
    }
    
    vTaskDelete(NULL);
}

void ping_task_timer_worker() {
    if (!homekit_is_pairing()) {
        if (xTaskCreate(ping_task, "PIN", PING_TASK_SIZE, NULL, PING_TASK_PRIORITY, NULL) != pdPASS) {
            ERROR("New PIN");
            homekit_remove_oldest_client();
        }
    } else {
        ERROR("PING HK pair %i", homekit_is_pairing());
    }
}

// -----

void setup_mode_call(const uint16_t gpio, void* args, const uint8_t param) {
    INFO("Setup call");
    
    if (main_config.setup_mode_time == 0 || xTaskGetTickCount() < main_config.setup_mode_time * (1000 / portTICK_PERIOD_MS)) {
        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 1);
        reboot_haa();
    } else {
        ERROR("Not allowed after %is since boot", main_config.setup_mode_time);
    }
}

void setup_mode_toggle_upcount(const int enabled) {
    if (enabled && main_config.setup_mode_toggle_counter_max > 0) {
        main_config.setup_mode_toggle_counter++;
        INFO("Setup %i/%i", main_config.setup_mode_toggle_counter, main_config.setup_mode_toggle_counter_max);
        
        if (main_config.setup_mode_toggle_counter == main_config.setup_mode_toggle_counter_max) {
            setup_mode_call(99, NULL, 0);
        } else {
            rs_esp_timer_start(main_config.setup_mode_toggle_timer);
        }
    }
}

void setup_mode_toggle() {
    main_config.setup_mode_toggle_counter = 0;
}

// -----
void save_states() {
    INFO("Saving");
    last_state_t* last_state = main_config.last_states;
    sysparam_status_t status;
    
    while (last_state) {
        char saved_state_id[8];
        itoa(last_state->ch_state_id, saved_state_id, 10);
        
        switch (last_state->ch_type) {
            case CH_TYPE_INT8:
                status = sysparam_set_int8(saved_state_id, last_state->ch->value.int_value);
                break;
                
            case CH_TYPE_INT:
                status = sysparam_set_int32(saved_state_id, last_state->ch->value.int_value);
                break;
                
            case CH_TYPE_FLOAT:
                status = sysparam_set_int32(saved_state_id, (int32_t) (last_state->ch->value.float_value * FLOAT_FACTOR_SAVE_AS_INT));
                break;
                
            case CH_TYPE_STRING:
                status = sysparam_set_string(saved_state_id, last_state->ch->value.string_value);
                break;
                
            default:    // case CH_TYPE_BOOL
                status = sysparam_set_bool(saved_state_id, last_state->ch->value.bool_value);
                break;
        }
        
        if (status != SYSPARAM_OK) {
            ERROR("Saving Ch%s", saved_state_id);
        }
        
        last_state = last_state->next;
    }
}

void save_states_callback() {
    rs_esp_timer_start(SAVE_STATES_TIMER);
}

void homekit_characteristic_notify_safe(homekit_characteristic_t *ch) {
    if (ch_group_find(ch)->homekit_enabled && main_config.wifi_status == WIFI_STATUS_CONNECTED && main_config.enable_homekit_server) {
        homekit_characteristic_notify(ch);
    }
}

void hkc_custom_setup_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    if (strncmp(value.string_value, CUSTOM_HAA_COMMAND, strlen(CUSTOM_HAA_COMMAND)) == 0) {
        const unsigned int option = value.string_value[strlen(CUSTOM_HAA_COMMAND)];
        INFO("<0> -> MODE %c", value.string_value[strlen(CUSTOM_HAA_COMMAND)]);
        
        homekit_characteristic_notify(ch);
        
        switch (option) {
            case '0':   // OTA Update
#ifdef ESP_PLATFORM
                setup_set_boot_installer();
#else
                rboot_set_temp_rom(1);
#endif
                reboot_haa();
                break;
                
            case '1':   // Setup Mode
                sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 1);
                reboot_haa();
                break;
                
            case '2':   // Reboot
                reboot_haa();
                break;
                
            case '3':   // WiFi Reconnection
                rs_esp_timer_start_forced(rs_esp_timer_create(1000, false, NULL, wifi_config_reset));
                break;
                
            default:
                break;
        }
    }
}

void hkc_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    INFO("<%i> -> GEN", ch_group->serv_index);
    ch->value = value;
    homekit_characteristic_notify_safe(ch);
    
    save_data_history(ch);
    
    save_states_callback();
}

void pm_custom_consumption_reset(ch_group_t* ch_group) {
    if (!main_config.clock_ready) {
        ERROR("<%i> Clock not ready", ch_group->serv_index);
        return;
    }
    
    led_blink(1);
    INFO("<%i> -> KWh Reset", ch_group->serv_index);
    
    time_t time = raven_ntp_get_time_t();
    
    ch_group->ch[4]->value.float_value = ch_group->ch[3]->value.float_value;
    ch_group->ch[3]->value.float_value = 0;
    ch_group->ch[5]->value.int_value = time;
    
    save_states_callback();
    
    save_data_history(ch_group->ch[4]);
    
    homekit_characteristic_notify_safe(ch_group->ch[3]);
    homekit_characteristic_notify_safe(ch_group->ch[4]);
    homekit_characteristic_notify_safe(ch_group->ch[5]);
    homekit_characteristic_notify_safe(ch_group->ch[6]);
}

void hkc_custom_consumption_reset_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    if (strcmp(value.string_value, CUSTOM_HAA_COMMAND) == 0) {
        ch_group_t* ch_group = ch_group_find(ch);
        pm_custom_consumption_reset(ch_group);
    }
}

// --- SWITCH / OUTLET
void hkc_on_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        if (ch->value.bool_value != value.bool_value) {
            led_blink(1);
            INFO("<%i> -> SW ON %i", ch_group->serv_index, value.bool_value);
            
            ch->value.bool_value = value.bool_value;
            
            if (ch->value.bool_value) {
                rs_esp_timer_start(AUTOOFF_TIMER);
            } else {
                rs_esp_timer_stop(AUTOOFF_TIMER);
            }
            
            setup_mode_toggle_upcount(ch_group->homekit_enabled);
            
            save_states_callback();
            
            if (ch_group->chs > 1 && ch_group->ch[1]->value.int_value > 0) {
                if (value.bool_value) {
                    ch_group->ch[2]->value.int_value = ch_group->ch[1]->value.int_value;
                    rs_esp_timer_start(ch_group->timer);
                } else {
                    ch_group->ch[2]->value.int_value = 0;
                    rs_esp_timer_stop(ch_group->timer);
                }
                
                homekit_characteristic_notify_safe(ch_group->ch[2]);
            }
            
            save_data_history(ch);
            
            do_actions(ch_group, ch->value.bool_value);
        }
    }
    
    homekit_characteristic_notify_safe(ch);
}

void hkc_on_status_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    if (ch->value.bool_value != value.bool_value) {
        led_blink(1);
        ch_group_t* ch_group = ch_group_find(ch);
        INFO("<%i> -> SW St ON %i", ch_group->serv_index, value.bool_value);
        ch->value.bool_value = value.bool_value;
    }
    
    homekit_characteristic_notify_safe(ch);
}

void on_timer_worker(TimerHandle_t xTimer) {
    ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
    
    if (ch_group->ch[2]->value.int_value > 0) {
        ch_group->ch[2]->value.int_value--;
    }
    
    if (ch_group->ch[2]->value.int_value == 0) {
        rs_esp_timer_stop(ch_group->timer);
        hkc_on_setter(ch_group->ch[0], HOMEKIT_BOOL(false));
    }
}

// --- LOCK MECHANISM
void hkc_lock_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        if (ch->value.int_value != value.int_value) {
            led_blink(1);
            INFO("<%i> -> LOCK %i", ch_group->serv_index, value.int_value);
            
            ch->value.int_value = value.int_value;
            
            ch_group->ch[0]->value.int_value = value.int_value;
            
            if (ch->value.int_value == 0) {
                rs_esp_timer_start(AUTOOFF_TIMER);
            } else {
                rs_esp_timer_stop(AUTOOFF_TIMER);
            }
            
            setup_mode_toggle_upcount(ch_group->homekit_enabled);
            
            save_states_callback();
            
            save_data_history(ch);
            
            do_actions(ch_group, ch->value.int_value);
        }
    }
    
    homekit_characteristic_notify_safe(ch);
    homekit_characteristic_notify_safe(ch_group->ch[0]);
}

void hkc_lock_status_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch->value.int_value != value.int_value) {
        led_blink(1);
        
        INFO("<%i> -> LOCK St %i", ch_group->serv_index, value.int_value);
        
        ch->value.int_value = value.int_value;
        
        ch_group->ch[0]->value.int_value = value.int_value;
    }
    
    homekit_characteristic_notify_safe(ch);
    homekit_characteristic_notify_safe(ch_group->ch[0]);
}

// --- BUTTON EVENT / DOORBELL
void button_event(const uint16_t gpio, void* args, const uint8_t event_type) {
    ch_group_t* ch_group = args;
    
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("<%i> -> BE %i", ch_group->serv_index, event_type);
        
        ch_group->ch[0]->value.int_value = event_type;
        homekit_characteristic_notify_safe(ch_group->ch[0]);
        
        save_data_history(ch_group->ch[0]);
        
        do_actions(ch_group, event_type);
        
        if (ch_group->serv_type == SERV_TYPE_DOORBELL && event_type == 0) {
            INFO("<%i> Ring", ch_group->serv_index);
            
            ch_group->ch[1]->value.int_value = !((bool) ch_group->ch[1]->value.int_value);
            homekit_characteristic_notify_safe(ch_group->ch[1]);
            
            save_data_history(ch_group->ch[1]);
        }
    }
}

void button_event_diginput(const uint16_t gpio, void* args, const uint8_t event_type) {
    ch_group_t* ch_group = args;
    
    INFO("<%i> BE DigI GPIO %i", ch_group->serv_index, gpio);
    
    if (ch_group->child_enabled) {
        button_event(gpio, args, event_type);
    }
}

// --- SENSORS
void binary_sensor(const uint16_t gpio, void* args, uint8_t type) {
    ch_group_t* ch_group = args;

    if (ch_group->main_enabled) {
        int new_value = (bool) type;
        if (type >= 4) {
            if (ch_group->serv_type == SERV_TYPE_CONTACT_SENSOR) {
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
        
        if ((ch_group->serv_type == SERV_TYPE_CONTACT_SENSOR &&
            ch_group->ch[0]->value.int_value != new_value) ||
            (ch_group->serv_type == SERV_TYPE_MOTION_SENSOR &&
            ch_group->ch[0]->value.bool_value != new_value)) {
            led_blink(1);
            
            INFO("<%i> DigI Sensor %i GPIO %i", ch_group->serv_index, type, gpio);
            
            if (ch_group->serv_type == SERV_TYPE_CONTACT_SENSOR) {
                ch_group->ch[0]->value.int_value = new_value;
            } else {
                ch_group->ch[0]->value.bool_value = new_value;
            }
            
            if (type <= 1) {
                if ((ch_group->serv_type == SERV_TYPE_CONTACT_SENSOR && ch_group->ch[0]->value.int_value == 1) ||
                    (ch_group->serv_type == SERV_TYPE_MOTION_SENSOR && ch_group->ch[0]->value.bool_value == true)) {
                    rs_esp_timer_start(AUTOOFF_TIMER);
                } else {
                    rs_esp_timer_stop(AUTOOFF_TIMER);
                }
                
                save_data_history(ch_group->ch[0]);
                
                do_actions(ch_group, type);
            }
        }
    }
    
    homekit_characteristic_notify_safe(ch_group->ch[0]);
}

float byte_array_to_num(uint8_t* data, const uint8_t array_size, const uint8_t type) {
    int32_t value = 0;
    
    if (type & 0b01) {    // Little endian
        for (int i = array_size - 1; i >= 0; i--) {
            value = (value << 8) | data[i];
        }
    } else {    // Big endian
        for (int i = 0; i < array_size; i++) {
            value = (value << 8) | data[i];
        }
    }
    
    if (type & 0b10 ) {  // Signed
        uint32_t sign_mask = (1 << ((array_size * 8) - 1));
        
        if (value & sign_mask) {
            for (int i = array_size; i < 4; i++) {
                value |= (0xFF << (8 * i));
            }
        }
        
        return value;
    }
    
    return (uint32_t) value;
}

// --- POWER MONITOR
void power_monitor_task(void* args) {
    ch_group_t* ch_group = args;
    
    float voltage = 0;
    float current = 0;
    float power = 0;
    
    int get_data = true;
    
    const uint8_t pm_sensor_type = PM_SENSOR_TYPE;
    if (pm_sensor_type <= 1) {
        voltage = adv_hlw_get_voltage_freq((uint8_t) PM_SENSOR_HLW_GPIO);
        current = adv_hlw_get_current_freq((uint8_t) PM_SENSOR_HLW_GPIO);
        power = adv_hlw_get_power_freq((uint8_t) PM_SENSOR_HLW_GPIO);
        
    } else if (pm_sensor_type <= 3) {
        const uint8_t bus = PM_SENSOR_ADE_BUS;
        const uint8_t addr = (uint8_t) PM_SENSOR_ADE_ADDR;
        
        uint8_t value[4] = { 0, 0, 0, 0 };
        uint8_t reg1[2] = { 0x03, 0x1C };
        uint8_t reg2[2] = { 0x03, 0x12 };
        adv_i2c_slave_read(bus, addr, reg1, 2, value, 4);
        vTaskDelay(1);
        voltage = byte_array_to_num(value, 4, 0b10);
        
        reg1[1] = 0x1A;
        
        if (pm_sensor_type == 3) {
            // Channel B
            reg1[1]++;  // 0x1B
            reg2[1]++;  // 0x13
        }
        
        adv_i2c_slave_read(bus, addr, reg1, 2, value, 4);
        vTaskDelay(1);
        current = byte_array_to_num(value, 4, 0b10);
        
        if (current >= 2000) {
            adv_i2c_slave_read(bus, addr, reg2, 2, value, 4);
            vTaskDelay(1);
            power = byte_array_to_num(value, 4, 0b10);
        } else {
            get_data = false;
            current = 0;
        }
        
    }
    
    if (pm_sensor_type <= 3) {
        voltage = (PM_VOLTAGE_FACTOR * voltage) + PM_VOLTAGE_OFFSET;
        
        if (voltage < 0) {
            voltage = 0;
        }
        
        if (get_data) {
            current = (PM_CURRENT_FACTOR * current) + PM_CURRENT_OFFSET;
        }
        
        if (pm_sensor_type <= 1 && PM_SENSOR_HLW_GPIO_CF == PM_SENSOR_DATA_DEFAULT) {
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
        
        INFO("<%i> -> PM V=%g, C=%g, P=%g", ch_group->serv_index, voltage, current, power);
        
        if (pm_sensor_type <= 1) {
            if (current < 0) {
                current = 0;
            }
            
            if (power < 0) {
                power = 0;
            }
        }
        
        if (voltage < 500.f && voltage > -500.f) {
            do_wildcard_actions(ch_group, 0, voltage);
            
            if (voltage != ch_group->ch[0]->value.float_value) {
                const int old_voltage = ch_group->ch[0]->value.float_value * 10;
                ch_group->ch[0]->value.float_value = voltage;
                
                if (old_voltage != (int) (voltage * 10)) {
                    homekit_characteristic_notify_safe(ch_group->ch[0]);
                }
            }
        } else {
            ERROR("<%i> PM V", ch_group->serv_index);
        }
        
        if (current < 150.f && current > -150.f) {
            do_wildcard_actions(ch_group, 1, current);
            
            if (current != ch_group->ch[1]->value.float_value) {
                const int old_current = ch_group->ch[1]->value.float_value * 1000;
                ch_group->ch[1]->value.float_value = current;
                
                if (old_current != (int) (current * 1000)) {
                    homekit_characteristic_notify_safe(ch_group->ch[1]);
                }
            }
        } else {
            ERROR("<%i> PM C", ch_group->serv_index);
        }
    } else {
        power = ch_group->ch[2]->value.float_value;
    }
    
    PM_LAST_SAVED_CONSUPTION += PM_POLL_PERIOD;
    
    if (power < 50000.f && power > -50000.f) {
        do_wildcard_actions(ch_group, 2, power);
        
        if (power != ch_group->ch[2]->value.float_value) {
            const int old_power = ch_group->ch[2]->value.float_value * 10;
            ch_group->ch[2]->value.float_value = power;
            
            if (old_power != (int) (power * 10)) {
                homekit_characteristic_notify_safe(ch_group->ch[2]);
            }
        }
        
        const float consumption = ch_group->ch[3]->value.float_value + ((power * PM_POLL_PERIOD) / 3600000.f);
        INFO("<%i> PM KWh=%1.7g", ch_group->serv_index, consumption);
        
        do_wildcard_actions(ch_group, 3, consumption);
        
        if (consumption != ch_group->ch[3]->value.float_value) {
            const int old_consumption = ch_group->ch[3]->value.float_value * 1000;
            ch_group->ch[3]->value.float_value = consumption;
            
            if (old_consumption != (int) (consumption * 1000)) {
                homekit_characteristic_notify_safe(ch_group->ch[3]);
            }
        }
        
        if (PM_LAST_SAVED_CONSUPTION > 14400) {
            PM_LAST_SAVED_CONSUPTION = 0;
            save_states_callback();
        }
        
    } else {
        ERROR("<%i> PM P", ch_group->serv_index);
    }
    
    ch_group->is_working = false;
    
    vTaskDelete(NULL);
}

void power_monitor_timer_worker(TimerHandle_t xTimer) {
    if (!homekit_is_pairing()) {
        ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
        if (ch_group->main_enabled) {
            if (!ch_group->is_working) {
                if (xTaskCreate(power_monitor_task, "PM", POWER_MONITOR_TASK_SIZE, (void*) ch_group, POWER_MONITOR_TASK_PRIORITY, NULL) == pdPASS) {
                    ch_group->is_working = true;
                } else {
                    ERROR("New PM");
                    homekit_remove_oldest_client();
                }
            }
        }
    } else {
        ERROR("HK pair");
    }
}

// --- WATER VALVE
void hkc_valve_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        if (ch->value.int_value != value.int_value) {
            led_blink(1);
            INFO("<%i> -> VALVE %i", ch_group->serv_index, value.int_value);
            
            ch->value.int_value = value.int_value;
            ch_group->ch[1]->value.int_value = value.int_value;
            
            if (ch->value.int_value == 1) {
                rs_esp_timer_start(AUTOOFF_TIMER);
            } else {
                rs_esp_timer_stop(AUTOOFF_TIMER);
            }
            
            setup_mode_toggle_upcount(ch_group->homekit_enabled);
            
            save_states_callback();
            
            if (ch_group->chs > 2 && ch_group->ch[2]->value.int_value > 0) {
                if (value.int_value == 0) {
                    ch_group->ch[3]->value.int_value = 0;
                    rs_esp_timer_stop(ch_group->timer);
                } else {
                    ch_group->ch[3]->value = ch_group->ch[2]->value;
                    rs_esp_timer_start(ch_group->timer);
                }
                
                homekit_characteristic_notify_safe(ch_group->ch[3]);
            }
            
            save_data_history(ch);
            
            do_actions(ch_group, ch->value.int_value);
        }
    }
    
    homekit_characteristic_notify_safe(ch);
    homekit_characteristic_notify_safe(ch_group->ch[1]);
}

void hkc_valve_status_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch->value.int_value != value.int_value) {
        led_blink(1);
        
        ch->value.int_value = value.int_value;
        
        ch_group->ch[1]->value.int_value = value.int_value;
        
        INFO("<%i> -> VALVE St %i", ch_group->serv_index, value.int_value);
    }
    
    homekit_characteristic_notify_safe(ch);
    homekit_characteristic_notify_safe(ch_group->ch[1]);
}

void valve_timer_worker(TimerHandle_t xTimer) {
    ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
    
    if (ch_group->ch[3]->value.int_value > 0) {
        ch_group->ch[3]->value.int_value--;
    }
    
    if (ch_group->ch[3]->value.int_value == 0) {
        rs_esp_timer_stop(ch_group->timer);
        hkc_valve_setter(ch_group->ch[0], HOMEKIT_UINT8(0));
    }
}

// --- IAIRZONING
void set_zones_task(void* args) {
    ch_group_t* iairzoning_group = args;
    
    INFO("<%i> -> iAZ", iairzoning_group->serv_index);

    int iairzoning_final_main_mode = IAIRZONING_LAST_ACTION;
    
    // Fix impossible cases
    ch_group_t* ch_group = main_config.ch_groups;
    while (ch_group) {
        if (ch_group->serv_type == SERV_TYPE_THERMOSTAT && iairzoning_group->serv_index == (uint8_t) TH_IAIRZONING_CONTROLLER) {
            const int th_current_action = THERMOSTAT_CURRENT_ACTION;
            switch (th_current_action) {
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
        if (ch_group->serv_type == SERV_TYPE_THERMOSTAT && iairzoning_group->serv_index == (uint8_t) TH_IAIRZONING_CONTROLLER) {
            const int th_current_action = THERMOSTAT_CURRENT_ACTION;
            if (th_current_action != THERMOSTAT_ACTION_TOTAL_OFF) {
                thermostat_all_off = false;
                
                if (th_current_action == THERMOSTAT_ACTION_HEATER_ON ||
                    th_current_action == THERMOSTAT_ACTION_COOLER_ON) {
                    thermostat_all_idle = false;
                    thermostat_all_soft_on = false;
                    
                } else if (th_current_action == THERMOSTAT_ACTION_HEATER_SOFT_ON ||
                           th_current_action == THERMOSTAT_ACTION_COOLER_SOFT_ON) {
                    thermostat_all_idle = false;

                } else if (th_current_action == THERMOSTAT_ACTION_HEATER_FORCE_IDLE ||
                           th_current_action == THERMOSTAT_ACTION_COOLER_FORCE_IDLE) {
                    thermostat_force_idle = true;
                    
                }
            }
        }
        
        ch_group = ch_group->next;
    }
    
    INFO("<%i> iAZ all OFF %i, all IDLE %i, all soft ON %i, force IDLE %i",
         iairzoning_group->serv_index,
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
                if (ch_group->serv_type == SERV_TYPE_THERMOSTAT && iairzoning_group->serv_index == (uint8_t) TH_IAIRZONING_CONTROLLER) {
                    if (TH_IAIRZONING_GATE_CURRENT_STATE != TH_IAIRZONING_GATE_OPEN) {
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
        
        for (int i = 0; i < 2; i++) {
            ch_group = main_config.ch_groups;
            while (ch_group) {
                if (ch_group->serv_type == SERV_TYPE_THERMOSTAT && iairzoning_group->serv_index == (uint8_t) TH_IAIRZONING_CONTROLLER) {
                    const int th_current_action = THERMOSTAT_CURRENT_ACTION;
                    const int th_gate_current_state = TH_IAIRZONING_GATE_CURRENT_STATE;
                    switch (th_current_action) {
                        case THERMOSTAT_ACTION_HEATER_ON:
                        case THERMOSTAT_ACTION_COOLER_ON:
                        case THERMOSTAT_ACTION_HEATER_SOFT_ON:
                        case THERMOSTAT_ACTION_COOLER_SOFT_ON:
                            if (th_gate_current_state != TH_IAIRZONING_GATE_OPEN) {
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
                                if (th_gate_current_state != TH_IAIRZONING_GATE_OPEN) {
                                    TH_IAIRZONING_GATE_CURRENT_STATE = TH_IAIRZONING_GATE_OPEN;
                                    do_actions(ch_group, THERMOSTAT_ACTION_GATE_OPEN);
                                    vTaskDelay(IAIRZONING_DELAY_ACTION);
                                }
                            } else {
                                if (i == 1 && th_gate_current_state != TH_IAIRZONING_GATE_CLOSE) {
                                    TH_IAIRZONING_GATE_CURRENT_STATE = TH_IAIRZONING_GATE_CLOSE;
                                    do_actions(ch_group, THERMOSTAT_ACTION_GATE_CLOSE);
                                    vTaskDelay(IAIRZONING_DELAY_ACTION);
                                }
                            }
                            break;
                            
                        default:    // THERMOSTAT_OFF
                            if (i == 1 && th_gate_current_state != TH_IAIRZONING_GATE_CLOSE) {
                                TH_IAIRZONING_GATE_CURRENT_STATE = TH_IAIRZONING_GATE_CLOSE;
                                do_actions(ch_group, THERMOSTAT_ACTION_GATE_CLOSE);
                                vTaskDelay(IAIRZONING_DELAY_ACTION);
                            }
                            break;
                    }
                }
                
                ch_group = ch_group->next;
            }
            
            if (i == 0) {
                vTaskDelay(IAIRZONING_DELAY_AFT_CLOSE);
            }
        }
    }
    
    if (iairzoning_final_main_mode != IAIRZONING_LAST_ACTION) {
        INFO("<%i> iAZ mode %i", iairzoning_group->serv_index, iairzoning_final_main_mode);
        IAIRZONING_LAST_ACTION = iairzoning_final_main_mode;
        do_actions(iairzoning_group, iairzoning_final_main_mode);
    }
    
    ch_group->is_working = false;
    
    vTaskDelete(NULL);
}

void set_zones_timer_worker(TimerHandle_t xTimer) {
    if (!homekit_is_pairing()) {
        ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
        if (!ch_group->is_working) {
            if (xTaskCreate(set_zones_task, "iAZ", SET_ZONES_TASK_SIZE, (void*) ch_group, SET_ZONES_TASK_PRIORITY, NULL) == pdPASS) {
                ch_group->is_working = true;
            } else {
                ERROR("New iAZ");
                homekit_remove_oldest_client();
                rs_esp_timer_start(xTimer);
            }
        }
    } else {
        ERROR("HK pair");
    }
}

// --- THERMOSTAT
void process_th_task(void* args) {
    ch_group_t* ch_group = args;
    
    led_blink(1);
    
    INFO("<%i> TH Process", ch_group->serv_index);
    
    const int th_current_action = THERMOSTAT_CURRENT_ACTION;
    int mode_has_changed = false;
    
    void heating(const float deadband, const float deadband_soft_on, const float deadband_force_idle) {
        INFO("<%i> Heating", ch_group->serv_index);
        if (SENSOR_TEMPERATURE_FLOAT < (TH_HEATER_TARGET_TEMP_FLOAT - deadband - deadband_soft_on)) {
            TH_MODE_INT = THERMOSTAT_MODE_HEATER;
            if (th_current_action != THERMOSTAT_ACTION_HEATER_ON) {
                THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_HEATER_ON;
                do_actions(ch_group, THERMOSTAT_ACTION_HEATER_ON);
                mode_has_changed = true;
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT < (TH_HEATER_TARGET_TEMP_FLOAT - deadband)) {
            TH_MODE_INT = THERMOSTAT_MODE_HEATER;
            if (th_current_action != THERMOSTAT_ACTION_HEATER_SOFT_ON) {
                THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_HEATER_SOFT_ON;
                do_actions(ch_group, THERMOSTAT_ACTION_HEATER_SOFT_ON);
                mode_has_changed = true;
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT < (TH_HEATER_TARGET_TEMP_FLOAT + deadband)) {
            if (TH_MODE_INT == THERMOSTAT_MODE_HEATER) {
                if (TH_DEADBAND_SOFT_ON > 0.000f) {
                    if (th_current_action != THERMOSTAT_ACTION_HEATER_SOFT_ON) {
                        THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_HEATER_SOFT_ON;
                        do_actions(ch_group, THERMOSTAT_ACTION_HEATER_SOFT_ON);
                        mode_has_changed = true;
                    }
                } else {
                    if (th_current_action != THERMOSTAT_ACTION_HEATER_ON) {
                        THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_HEATER_ON;
                        do_actions(ch_group, THERMOSTAT_ACTION_HEATER_ON);
                        mode_has_changed = true;
                    }
                }
                
            } else {
                TH_MODE_INT = THERMOSTAT_MODE_IDLE;
                if (th_current_action != THERMOSTAT_ACTION_HEATER_IDLE) {
                    THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_HEATER_IDLE;
                    do_actions(ch_group, THERMOSTAT_ACTION_HEATER_IDLE);
                    mode_has_changed = true;
                }
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT >= (TH_HEATER_TARGET_TEMP_FLOAT + deadband + deadband_force_idle) &&
                   TH_DEADBAND_FORCE_IDLE > 0.000f) {
            TH_MODE_INT = THERMOSTAT_MODE_IDLE;
            if (th_current_action != THERMOSTAT_ACTION_HEATER_FORCE_IDLE) {
                THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_HEATER_FORCE_IDLE;
                do_actions(ch_group, THERMOSTAT_ACTION_HEATER_FORCE_IDLE);
                mode_has_changed = true;
            }
            
        } else {
            TH_MODE_INT = THERMOSTAT_MODE_IDLE;
            if (TH_DEADBAND_FORCE_IDLE == 0.000f ||
                th_current_action != THERMOSTAT_ACTION_HEATER_FORCE_IDLE) {
                if (th_current_action != THERMOSTAT_ACTION_HEATER_IDLE) {
                    THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_HEATER_IDLE;
                    do_actions(ch_group, THERMOSTAT_ACTION_HEATER_IDLE);
                    mode_has_changed = true;
                }
            }
        }
    }
    
    void cooling(const float deadband, const float deadband_soft_on, const float deadband_force_idle) {
        INFO("<%i> Cooling", ch_group->serv_index);
        if (SENSOR_TEMPERATURE_FLOAT > (TH_COOLER_TARGET_TEMP_FLOAT + deadband + deadband_soft_on)) {
            TH_MODE_INT = THERMOSTAT_MODE_COOLER;
            if (th_current_action != THERMOSTAT_ACTION_COOLER_ON) {
                THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_COOLER_ON;
                do_actions(ch_group, THERMOSTAT_ACTION_COOLER_ON);
                mode_has_changed = true;
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT > (TH_COOLER_TARGET_TEMP_FLOAT + deadband)) {
            TH_MODE_INT = THERMOSTAT_MODE_COOLER;
            if (th_current_action != THERMOSTAT_ACTION_COOLER_SOFT_ON) {
                THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_COOLER_SOFT_ON;
                do_actions(ch_group, THERMOSTAT_ACTION_COOLER_SOFT_ON);
                mode_has_changed = true;
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT > (TH_COOLER_TARGET_TEMP_FLOAT - deadband)) {
            if (TH_MODE_INT == THERMOSTAT_MODE_COOLER) {
                if (TH_DEADBAND_SOFT_ON > 0.000f) {
                    if (th_current_action != THERMOSTAT_ACTION_COOLER_SOFT_ON) {
                        THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_COOLER_SOFT_ON;
                        do_actions(ch_group, THERMOSTAT_ACTION_COOLER_SOFT_ON);
                        mode_has_changed = true;
                    }
                } else {
                    if (th_current_action != THERMOSTAT_ACTION_COOLER_ON) {
                        THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_COOLER_ON;
                        do_actions(ch_group, THERMOSTAT_ACTION_COOLER_ON);
                        mode_has_changed = true;
                    }
                }
                
            } else {
                TH_MODE_INT = THERMOSTAT_MODE_IDLE;
                if (th_current_action != THERMOSTAT_ACTION_COOLER_IDLE) {
                    THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_COOLER_IDLE;
                    do_actions(ch_group, THERMOSTAT_ACTION_COOLER_IDLE);
                    mode_has_changed = true;
                }
            }
            
        } else if (SENSOR_TEMPERATURE_FLOAT <= (TH_COOLER_TARGET_TEMP_FLOAT - deadband - deadband_force_idle) &&
                   TH_DEADBAND_FORCE_IDLE > 0.000f) {
            TH_MODE_INT = THERMOSTAT_MODE_IDLE;
            if (th_current_action != THERMOSTAT_ACTION_COOLER_FORCE_IDLE) {
                THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_COOLER_FORCE_IDLE;
                do_actions(ch_group, THERMOSTAT_ACTION_COOLER_FORCE_IDLE);
                mode_has_changed = true;
            }
            
        } else {
            TH_MODE_INT = THERMOSTAT_MODE_IDLE;
            if (TH_DEADBAND_FORCE_IDLE == 0.000f ||
                th_current_action != THERMOSTAT_ACTION_COOLER_FORCE_IDLE) {
                if (th_current_action != THERMOSTAT_ACTION_COOLER_IDLE) {
                    THERMOSTAT_CURRENT_ACTION = THERMOSTAT_ACTION_COOLER_IDLE;
                    do_actions(ch_group, THERMOSTAT_ACTION_COOLER_IDLE);
                    mode_has_changed = true;
                }
            }
        }
    }
    
    if (TH_ACTIVE_INT) {
        if (th_current_action == THERMOSTAT_ACTION_TOTAL_OFF) {
            do_actions(ch_group, THERMOSTAT_ACTION_ON);
        }
        
        if (TH_TARGET_MODE_INT == THERMOSTAT_TARGET_MODE_HEATER) {
            heating(TH_DEADBAND, TH_DEADBAND_SOFT_ON, TH_DEADBAND_FORCE_IDLE);
            homekit_characteristic_notify_safe(ch_group->ch[5]);
                    
        } else if (TH_TARGET_MODE_INT == THERMOSTAT_TARGET_MODE_COOLER) {
            cooling(TH_DEADBAND, TH_DEADBAND_SOFT_ON, TH_DEADBAND_FORCE_IDLE);
            homekit_characteristic_notify_safe(ch_group->ch[6]);
            
        } else {    // THERMOSTAT_TARGET_MODE_AUTO
            const float mid_target = (TH_HEATER_TARGET_TEMP_FLOAT + TH_COOLER_TARGET_TEMP_FLOAT) / 2.000f;
            
            int is_heater = false;
            if (TH_MODE_INT == THERMOSTAT_MODE_OFF) {
                if (SENSOR_TEMPERATURE_FLOAT < mid_target) {
                    is_heater = true;
                }
            } else if (SENSOR_TEMPERATURE_FLOAT <= TH_HEATER_TARGET_TEMP_FLOAT) {
                is_heater = true;
            } else if (SENSOR_TEMPERATURE_FLOAT < (TH_COOLER_TARGET_TEMP_FLOAT + 1.5) &&
                       (th_current_action == THERMOSTAT_ACTION_HEATER_IDLE ||
                        th_current_action == THERMOSTAT_ACTION_HEATER_ON ||
                        th_current_action == THERMOSTAT_ACTION_HEATER_FORCE_IDLE ||
                        th_current_action == THERMOSTAT_ACTION_HEATER_SOFT_ON)) {
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
        INFO("<%i> Off", ch_group->serv_index);
        TH_MODE_INT = THERMOSTAT_MODE_OFF;
        if (th_current_action != THERMOSTAT_ACTION_TOTAL_OFF) {
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
        for (int i = 2; i <= 3; i++) {
            ch_group->last_wildcard_action[i] = NO_LAST_WILDCARD_ACTION;
        }
    }
    
    homekit_characteristic_notify_safe(ch_group->ch[2]);
    homekit_characteristic_notify_safe(ch_group->ch[3]);
    homekit_characteristic_notify_safe(ch_group->ch[4]);
    
    if (TH_IAIRZONING_CONTROLLER != 0 && mode_has_changed) {
        rs_esp_timer_start(ch_group_find_by_serv((uint8_t) TH_IAIRZONING_CONTROLLER)->timer2);
    }
    
    save_states_callback();
    
    save_data_history(ch_group->ch[3]);
    
    ch_group->is_working = false;
    
    vTaskDelete(NULL);
}

void process_th_timer(TimerHandle_t xTimer) {
    ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
    if (!ch_group->is_working) {
        if (xTaskCreate(process_th_task, "TH", PROCESS_TH_TASK_SIZE, (void*) ch_group, PROCESS_TH_TASK_PRIORITY, NULL) == pdPASS) {
            ch_group->is_working = true;
        } else {
            ERROR("New TH");
            homekit_remove_oldest_client();
            rs_esp_timer_start(xTimer);
        }
    }
}

void hkc_th_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        INFO("<%i> -> TH", ch_group->serv_index);
        
        ch->value = value;
        
        if (ch == ch_group->ch[2]) {
            setup_mode_toggle_upcount(ch_group->homekit_enabled);
            homekit_characteristic_notify_safe(ch);
        }
        
        rs_esp_timer_start(ch_group->timer2);
        
        if (ch != ch_group->ch[0]) {
            save_data_history(ch);
        }
        
    } else {
        homekit_characteristic_notify_safe(ch);
    }
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
        
        hkc_th_setter(ch_group->ch[0], ch_group->ch[0]->value);
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
        
        hkc_th_setter(ch_group->ch[0], ch_group->ch[0]->value);
        
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
    
    led_blink(1);
    
    INFO("<%i> Hum Process", ch_group->serv_index);
    
    const int humidif_current_action = HUMIDIF_CURRENT_ACTION;
    
    void hum(const float deadband, const float deadband_soft_on, const float deadband_force_idle) {
        INFO("<%i> Hum", ch_group->serv_index);
        if (SENSOR_HUMIDITY_FLOAT < (HM_HUM_TARGET_FLOAT - deadband - deadband_soft_on)) {
            HM_MODE_INT = HUMIDIF_MODE_HUM;
            if (humidif_current_action != HUMIDIF_ACTION_HUM_ON) {
                HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_HUM_ON;
                do_actions(ch_group, HUMIDIF_ACTION_HUM_ON);
            }
            
        } else if (SENSOR_HUMIDITY_FLOAT < (HM_HUM_TARGET_FLOAT - deadband)) {
            HM_MODE_INT = HUMIDIF_MODE_HUM;
            if (humidif_current_action != HUMIDIF_ACTION_HUM_SOFT_ON) {
                HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_HUM_SOFT_ON;
                do_actions(ch_group, HUMIDIF_ACTION_HUM_SOFT_ON);
            }
            
        } else if (SENSOR_HUMIDITY_FLOAT < (HM_HUM_TARGET_FLOAT + deadband)) {
            if (HM_MODE_INT == HUMIDIF_MODE_HUM) {
                if (HM_DEADBAND_SOFT_ON > 0.000f) {
                    if (humidif_current_action != HUMIDIF_ACTION_HUM_SOFT_ON) {
                        HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_HUM_SOFT_ON;
                        do_actions(ch_group, HUMIDIF_ACTION_HUM_SOFT_ON);
                    }
                } else {
                    if (humidif_current_action != HUMIDIF_ACTION_HUM_ON) {
                        HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_HUM_ON;
                        do_actions(ch_group, HUMIDIF_ACTION_HUM_ON);
                    }
                }
                
            } else {
                HM_MODE_INT = HUMIDIF_MODE_IDLE;
                if (humidif_current_action != HUMIDIF_ACTION_HUM_IDLE) {
                    HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_HUM_IDLE;
                    do_actions(ch_group, HUMIDIF_ACTION_HUM_IDLE);
                }
            }
            
        } else if ((SENSOR_HUMIDITY_FLOAT >= (HM_HUM_TARGET_FLOAT + deadband + deadband_force_idle) ||
                    SENSOR_HUMIDITY_FLOAT == 100.f ) &&
                   HM_DEADBAND_FORCE_IDLE > 0.000f) {
            HM_MODE_INT = HUMIDIF_MODE_IDLE;
            if (humidif_current_action != HUMIDIF_ACTION_HUM_FORCE_IDLE) {
                HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_HUM_FORCE_IDLE;
                do_actions(ch_group, HUMIDIF_ACTION_HUM_FORCE_IDLE);
            }
            
        } else {
            HM_MODE_INT = HUMIDIF_MODE_IDLE;
            if (HM_DEADBAND_FORCE_IDLE == 0.000f ||
                humidif_current_action != HUMIDIF_ACTION_HUM_FORCE_IDLE) {
                if (humidif_current_action != HUMIDIF_ACTION_HUM_IDLE) {
                    HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_HUM_IDLE;
                    do_actions(ch_group, HUMIDIF_ACTION_HUM_IDLE);
                }
            }
        }
    }
    
    void dehum(const float deadband, const float deadband_soft_on, const float deadband_force_idle) {
        INFO("<%i> Dehum", ch_group->serv_index);
        if (SENSOR_HUMIDITY_FLOAT > (HM_DEHUM_TARGET_FLOAT + deadband + deadband_soft_on)) {
            HM_MODE_INT = HUMIDIF_MODE_DEHUM;
            if (humidif_current_action != HUMIDIF_ACTION_DEHUM_ON) {
                HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_DEHUM_ON;
                do_actions(ch_group, HUMIDIF_ACTION_DEHUM_ON);
            }
            
        } else if (SENSOR_HUMIDITY_FLOAT > (HM_DEHUM_TARGET_FLOAT + deadband)) {
            HM_MODE_INT = HUMIDIF_MODE_DEHUM;
            if (humidif_current_action != HUMIDIF_ACTION_DEHUM_SOFT_ON) {
                HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_DEHUM_SOFT_ON;
                do_actions(ch_group, HUMIDIF_ACTION_DEHUM_SOFT_ON);
            }
            
        } else if (SENSOR_HUMIDITY_FLOAT > (HM_DEHUM_TARGET_FLOAT - deadband)) {
            if (HM_MODE_INT == HUMIDIF_MODE_DEHUM) {
                if (HM_DEADBAND_SOFT_ON > 0.000f) {
                    if (humidif_current_action != HUMIDIF_ACTION_DEHUM_SOFT_ON) {
                        HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_DEHUM_SOFT_ON;
                        do_actions(ch_group, HUMIDIF_ACTION_DEHUM_SOFT_ON);
                    }
                } else {
                    if (humidif_current_action != HUMIDIF_ACTION_DEHUM_ON) {
                        HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_DEHUM_ON;
                        do_actions(ch_group, HUMIDIF_ACTION_DEHUM_ON);
                    }
                }
                
            } else {
                HM_MODE_INT = HUMIDIF_MODE_IDLE;
                if (humidif_current_action != HUMIDIF_ACTION_DEHUM_IDLE) {
                    HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_DEHUM_IDLE;
                    do_actions(ch_group, HUMIDIF_ACTION_DEHUM_IDLE);
                }
            }
            
        } else if ((SENSOR_HUMIDITY_FLOAT <= (HM_DEHUM_TARGET_FLOAT - deadband - deadband_force_idle) ||
                    SENSOR_HUMIDITY_FLOAT == 0.f) &&
                   HM_DEADBAND_FORCE_IDLE > 0.000f) {
            HM_MODE_INT = HUMIDIF_MODE_IDLE;
            if (humidif_current_action != HUMIDIF_ACTION_DEHUM_FORCE_IDLE) {
                HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_DEHUM_FORCE_IDLE;
                do_actions(ch_group, HUMIDIF_ACTION_DEHUM_FORCE_IDLE);
            }
            
        } else {
            HM_MODE_INT = HUMIDIF_MODE_IDLE;
            if (HM_DEADBAND_FORCE_IDLE == 0.000f ||
                humidif_current_action != HUMIDIF_ACTION_DEHUM_FORCE_IDLE) {
                if (humidif_current_action != HUMIDIF_ACTION_DEHUM_IDLE) {
                    HUMIDIF_CURRENT_ACTION = HUMIDIF_ACTION_DEHUM_IDLE;
                    do_actions(ch_group, HUMIDIF_ACTION_DEHUM_IDLE);
                }
            }
        }
    }
    
    if (HM_ACTIVE_INT) {
        if (humidif_current_action == HUMIDIF_ACTION_TOTAL_OFF) {
            do_actions(ch_group, HUMIDIF_ACTION_ON);
        }
        
        if (HM_TARGET_MODE_INT == HUMIDIF_TARGET_MODE_HUM) {
            hum(HM_DEADBAND, HM_DEADBAND_SOFT_ON, HM_DEADBAND_FORCE_IDLE);
            homekit_characteristic_notify_safe(ch_group->ch[5]);
                    
        } else if (HM_TARGET_MODE_INT == HUMIDIF_TARGET_MODE_DEHUM) {
            dehum(HM_DEADBAND, HM_DEADBAND_SOFT_ON, HM_DEADBAND_FORCE_IDLE);
            homekit_characteristic_notify_safe(ch_group->ch[6]);
            
        } else {    // HUMIDIF_TARGET_MODE_AUTO
            const float mid_target = (HM_HUM_TARGET_FLOAT + HM_DEHUM_TARGET_FLOAT) / 2.000f;
            
            int is_hum = false;
            if (HM_MODE_INT == HUMIDIF_MODE_OFF) {
                if (SENSOR_HUMIDITY_FLOAT < mid_target) {
                    is_hum = true;
                }
            } else if (SENSOR_HUMIDITY_FLOAT <= HM_HUM_TARGET_FLOAT) {
                is_hum = true;
            } else if (SENSOR_HUMIDITY_FLOAT < (HM_DEHUM_TARGET_FLOAT + 8) &&
                       (humidif_current_action == HUMIDIF_ACTION_HUM_IDLE ||
                        humidif_current_action == HUMIDIF_ACTION_HUM_ON ||
                        humidif_current_action == HUMIDIF_ACTION_HUM_FORCE_IDLE ||
                        humidif_current_action == HUMIDIF_ACTION_HUM_SOFT_ON)) {
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
        INFO("<%i> Off", ch_group->serv_index);
        HM_MODE_INT = HUMIDIF_MODE_OFF;
        if (humidif_current_action != HUMIDIF_ACTION_TOTAL_OFF) {
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
        for (int i = 2; i <= 3; i++) {
            ch_group->last_wildcard_action[i] = NO_LAST_WILDCARD_ACTION;
        }
    }
    
    homekit_characteristic_notify_safe(ch_group->ch[2]);
    homekit_characteristic_notify_safe(ch_group->ch[3]);
    homekit_characteristic_notify_safe(ch_group->ch[4]);
    
    save_states_callback();
    
    save_data_history(ch_group->ch[3]);
    
    ch_group->is_working = false;
    
    vTaskDelete(NULL);
}

void process_humidif_timer(TimerHandle_t xTimer) {
    ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
    if (!ch_group->is_working) {
        if (xTaskCreate(process_hum_task, "HUM", PROCESS_HUMIDIF_TASK_SIZE, (void*) ch_group, PROCESS_HUMIDIF_TASK_PRIORITY, NULL) == pdPASS) {
            ch_group->is_working = true;
        } else {
            ERROR("New HUM");
            homekit_remove_oldest_client();
            rs_esp_timer_start(xTimer);
        }
    }
}

void hkc_humidif_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        INFO("<%i> -> HUM", ch_group->serv_index);
        
        ch->value = value;
        
        if (ch == ch_group->ch[2]) {
            setup_mode_toggle_upcount(ch_group->homekit_enabled);
            homekit_characteristic_notify_safe(ch);
        }
        
        rs_esp_timer_start(ch_group->timer2);

        if (ch != ch_group->ch[1]) {
            save_data_history(ch);
        }
        
    } else {
        homekit_characteristic_notify_safe(ch);
    }
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
        
        hkc_humidif_setter(ch_group->ch[1], ch_group->ch[1]->value);
    }
}

void humidif_input_hum(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;

    if (ch_group->child_enabled) {
        if (HM_TYPE != HUMIDIF_TYPE_DEHUM) {
            int set_h_temp = HM_HUM_TARGET_FLOAT;

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
            int set_c_temp = HM_DEHUM_TARGET_FLOAT;

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
        
        hkc_humidif_setter(ch_group->ch[1], ch_group->ch[1]->value);
        
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
        for (int i = 0; i < 10; i++) {
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
    int get_temp = false;
    unsigned int iairzoning = 0;
    
    if (ch_group->serv_type == SERV_TYPE_IAIRZONING) {
        INFO("<%i> iAZ sensors", ch_group->serv_index);
        iairzoning = ch_group->serv_index;
        ch_group = main_config.ch_groups;
    }
    
    while (ch_group) {
        if (ch_group->serv_type >= SERV_TYPE_THERMOSTAT &&
            ch_group->serv_type <= SERV_TYPE_HUMIDIFIER_WITH_TEMP) {
            const unsigned int sensor_type = TH_SENSOR_TYPE;
            const int sensor_gpio = TH_SENSOR_GPIO;
            if (sensor_gpio != -1 || sensor_type >= 5) {
                get_temp = false;
                
                if (iairzoning == 0 || (iairzoning == (uint8_t) TH_IAIRZONING_CONTROLLER)) {
                    INFO("<%i> TH sensor", ch_group->serv_index);
                    
                    if (sensor_type != 3 && sensor_type < 5) {
                        dht_sensor_type_t current_sensor_type = DHT_TYPE_DHT22; // sensor_type == 2
                        
                        if (sensor_type == 1) {
                            current_sensor_type = DHT_TYPE_DHT11;
                        } else if (sensor_type == 4) {
                            current_sensor_type = DHT_TYPE_SI7021;
                        }
                        
                        get_temp = dht_read_float_data(current_sensor_type, sensor_gpio, &humidity_value, &temperature_value);
                    } else if (sensor_type == 3) {
                        const unsigned int sensor_index = TH_SENSOR_INDEX;
                        ds18b20_addr_t ds18b20_addrs[sensor_index];
                        
                        const unsigned int scaned_devices = ds18b20_scan_devices(sensor_gpio, ds18b20_addrs, sensor_index);
                        
                        if (scaned_devices >= sensor_index) {
                            ds18b20_addr_t ds18b20_addr;
                            ds18b20_addr = ds18b20_addrs[sensor_index - 1];
                            
                            ds18b20_measure_and_read_multi(sensor_gpio, &ds18b20_addr, 1, &temperature_value);
                            
                            if (temperature_value < 130.f && temperature_value > -60.f) {
                                get_temp = true;
                            }
                        }
                        
                    } else {
#ifdef ESP_PLATFORM
                        int adc_int = -1;
                        adc_channel_t adc_channel;
                        adc_oneshot_io_to_channel(sensor_gpio, NULL, &adc_channel);
                        adc_oneshot_read(main_config.adc_dac_data->adc_oneshot_handle, adc_channel, &adc_int);
#else
                        float adc = sdk_system_adc_read();
#endif
                        
#ifdef ESP_PLATFORM
                        if (adc_int >= 0) {
                            float adc = adc_int;
#endif
                            if (sensor_type == 5) {
                                // https://github.com/arendst/Tasmota/blob/7177c7d8e003bb420d8cae39f544c2b8a9af09fe/tasmota/xsns_02_analog.ino#L201
                                temperature_value = KELVIN_TO_CELSIUS(3350 / (3350 / 298.15 + taylor_log(((32000 * adc) / ((HAA_ADC_MAX_VALUE * 3.3) - adc)) / 10000))) - 15;
                                
                            } else if (sensor_type == 6) {
                                temperature_value = KELVIN_TO_CELSIUS(3350 / (3350 / 298.15 - taylor_log(((32000 * adc) / ((HAA_ADC_MAX_VALUE * 3.3) - adc)) / 10000))) + 15;
                                
                            } else if (sensor_type == 7) {
                                temperature_value = HAA_ADC_MAX_VALUE - adc;
                                
                            } else if (sensor_type == 8) {
                                temperature_value = adc;
                                
                            } else if (sensor_type == 9){
                                humidity_value = HAA_ADC_MAX_VALUE - adc;
                                
                            } else {    // th_sensor_type == 10
                                humidity_value = adc;
                            }
                            
                            if (sensor_type >= 9) {
                                humidity_value *= 100.00000000f / HAA_ADC_MAX_VALUE;
                            }
                            
                            if (TH_SENSOR_HUM_OFFSET != 0.000000f && sensor_type < 9) {
                                temperature_value *= TH_SENSOR_HUM_OFFSET;
                                
                            } else if (TH_SENSOR_TEMP_OFFSET != 0.000000f && sensor_type >= 9) {
                                humidity_value *= TH_SENSOR_TEMP_OFFSET;
                            }
                            
                            get_temp = true;
#ifdef ESP_PLATFORM
                        }
#endif
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
                            
                            INFO("<%i> TEMP %.1fC", ch_group->serv_index, temperature_value);
                            
                            if (temperature_value != ch_group->ch[0]->value.float_value) {
                                ch_group->ch[0]->value.float_value = temperature_value;
                                homekit_characteristic_notify_safe(ch_group->ch[0]);
                                
                                if (ch_group->chs > 4) {
                                    hkc_th_setter(ch_group->ch[0], ch_group->ch[0]->value);
                                }
                                
                                if (ch_group->main_enabled) {
                                    do_wildcard_actions(ch_group, 0, temperature_value);
                                }
                            }
                        }
                        
                        if (ch_group->chs > 1 && ch_group->ch[1]) {
                            humidity_value += TH_SENSOR_HUM_OFFSET;
                            if (humidity_value < 0.f) {
                                humidity_value = 0.f;
                            } else if (humidity_value > 100.f) {
                                humidity_value = 100.f;
                            }

                            const unsigned int humidity_value_int = humidity_value;
                            
                            INFO("<%i> HUM %i", ch_group->serv_index, humidity_value_int);
                            
                            if (humidity_value_int != (uint8_t) ch_group->ch[1]->value.float_value) {
                                ch_group->ch[1]->value.float_value = humidity_value_int;
                                homekit_characteristic_notify_safe(ch_group->ch[1]);
                                
                                if (ch_group->chs > 4) {
                                    hkc_humidif_setter(ch_group->ch[1], ch_group->ch[1]->value);
                                }
                                
                                if (ch_group->main_enabled) {
                                    do_wildcard_actions(ch_group, 1, humidity_value_int);
                                }
                            }
                        }
                        
                    } else {
                        led_blink(5);
                        ERROR("<%i> Read", ch_group->serv_index);
                        
                        TH_SENSOR_ERROR_COUNT++;
                        
                        if (TH_SENSOR_ERROR_COUNT > TH_SENSOR_MAX_ALLOWED_ERRORS) {
                            TH_SENSOR_ERROR_COUNT = 0;
                            
                            if (ch_group->chs > 0 && ch_group->ch[0]) {
                                ch_group->ch[0]->value.float_value = TH_SENSOR_TEMP_VALUE_WHEN_ERROR;
                                homekit_characteristic_notify_safe(ch_group->ch[0]);
                            }
                            
                            if (ch_group->chs > 1 && ch_group->ch[1]) {
                                ch_group->ch[1]->value.float_value = 0;
                                homekit_characteristic_notify_safe(ch_group->ch[1]);
                            }
                            
                            if (ch_group->main_enabled) {
                                do_actions(ch_group, THERMOSTAT_ACTION_SENSOR_ERROR);
                            }
                        }
                    }
                }
            }
        }
        
        if (iairzoning > 0) {
            if (ch_group->serv_type == SERV_TYPE_THERMOSTAT && iairzoning == (uint8_t) TH_IAIRZONING_CONTROLLER) {
                vTaskDelay(MS_TO_TICKS(100));
            }

            ch_group = ch_group->next;
            
        } else {
            ch_group = NULL;
        }
    }
    
    ch_group->is_working = false;
    
    vTaskDelete(NULL);
}

void temperature_timer_worker(TimerHandle_t xTimer) {
    if (!homekit_is_pairing()) {
        ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
        if (!ch_group->is_working) {
            if (xTaskCreate(temperature_task, "TEM", TEMPERATURE_TASK_SIZE, (void*) ch_group, TEMPERATURE_TASK_PRIORITY, NULL) == pdPASS) {
                ch_group->is_working = true;
            } else {
                ERROR("New TEM");
                homekit_remove_oldest_client();
            }
        }
    } else {
        ERROR("HK pair");
    }
}

// --- LIGHTBULB
void hsi2rgbw(uint16_t h, float s, uint8_t v, ch_group_t* ch_group) {
    // * All credits and thanks to Kevin John Cutler    *
    // * https://github.com/kevinjohncutler/colormixing *
    // 
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
        for (int i = 1; i < 4; i++) {
            sum = sum + arr[i];
        }
        
        return sum;
    }

    // Helper function to find max of an array
    float array_max(float arr[], const int num_elements) {
        float max = arr[0];
        for (int i = 1; i < num_elements; i++) {
            if (arr[i] > max) {
                max = arr[i];
            }
        }
        
        return max;
    }

    // Helper function to find min of an array
    inline float array_min(float arr[]) {
        float min = arr[0];
        for (int i = 1; i < 3; i++) {
            if (arr[i] < min) {
                min = arr[i];
            }
        }
        
        return min;
    }

    // Helper function to perform dot product
    inline float array_dot(float arr1[], float arr2[]) {
        float sum = arr1[0] * arr2[0];
        for (int i = 1; i < 5; i++) {
            sum += arr1[i] * arr2[i];
        }
        
        return sum;
    }

    // Helper function to multiply array by a constant
    void array_multiply(float arr[], const float scalar, const int num_elements) {
        for (int i = 0; i < num_elements; i++) {
            arr[i] *= scalar;
        }
    }

    // Helper function to asign array componenets
    void array_equals(float arr[], float vals[], const int num_elements) {
        for (int i = 0; i < num_elements; i++) {
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
        for (int i = 0; i < 3; i++) {
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
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                res[i][j] = 0;
                for (int k = 0; k < 3; k++)
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
        for (int i = 0; i < 2; i++) {
            p[i] = a[i] - b[i];
        }
    }
    
    // *** HSI TO RGBW FUNCTION ***
#ifdef LIGHT_DEBUG
    uint32_t run_time = sdk_system_get_time_raw();
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
    for (int i = 0; i < 3; i++) {
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

        for (int i = 0; i < 3; i++) {
            int j = ((i + 1) % 3);
            int k = ((i + 2) % 3); //had to adapt a bit for 0-based indexing
            float pp[2];
            intersect(pp, WP, *(myCMY + i), *(LED_RGB + j), *(LED_RGB + k)); // compute point long the line from white-C(MY) to RG(B) line.
            LED_CMY[i][0] = pp[0];
            LED_CMY[i][1] = pp[1]; // assign coordinates to the LED_CMY array
        }
        L_DEBUG("LED CMY: { {%g, %g}, {%g, %g}, {%g, %g} }", LED_CMY[0][0], LED_CMY[0][1], LED_CMY[1][0], LED_CMY[1][1], LED_CMY[2][0], LED_CMY[2][1]);

        // Go though each region. Most robust is barycentric check, albeing susecpible to numerical imprecision - at least there will be no issue with vertical lines. Ordered to have better time for red and magenta triangles (warm whites and reds).
        
        int i, j, k;
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
        INFO("Chrom %g, %g", p[0], p[1]);
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
        for (int i = 0; i < 4; i++) {
            coeffs[i] += coeffs1[i];
        }
        // If WW, then compute RGBW again with WW, add to the main coeff list. Can easlily add support for any LEDs inside the gamut. The only thing I am worried about is that the RGB is always pulling float-duty with two vertexes instead of so
        if (LIGHTBULB_CHANNELS == 5) {
            float coeffs2[4];
            getWeights(coeffs2, p, R, G, B, WW);
            for (int i = 0; i < 3; i++) {
                coeffs[i] += coeffs2[i];
            }
            coeffs[4] += coeffs2[3];
        }

        L_DEBUG("Coeffs before flux: %g, %g, %g, %g, %g",coeffs[0],coeffs[1],coeffs[2],coeffs[3],coeffs[4]);
        
        // (3.a.0) Correct for differences in intrinsic flux; needed before extraRGB step because we must balance RGB to whites first to see what headroom is left
        for (int i = 0; i < 5; i++) {
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
        for (int i = 0; i < 3; i++) {
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
        for (int i = 0; i < 3; i++) {
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
        brightness *= HAA_MIN(LIGHTBULB_MAX_POWER, flux_ratio) / flux_ratio; // Hard cap at 1 as to not accidentally overdrive
    }

    // (6) Assign the target colors to lightbulb group struct, now in fraction of PWM_SCALE. This min function is just a final check, it should not ever go over PWM_SCALE.
    for (int n = 0; n < 5; n++) {
        lightbulb_group->target[n]  = HAA_MIN(floorf(coeffs[n] * brightness), PWM_SCALE);
    }

#ifdef LIGHT_DEBUG
    INFO("hsi2rgbw runtime: %0.3f ms", ((float) (sdk_system_get_time_raw() - run_time)) * 1e-3);
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
void rgbw_set_timer_worker() {
    int all_channels_ready = true;
    
    lightbulb_group_t* lightbulb_group = main_config.lightbulb_groups;
    
    while (lightbulb_group) {
        if (LIGHTBULB_TYPE != LIGHTBULB_TYPE_VIRTUAL) {
            lightbulb_group->has_changed = false;
            for (int i = 0; i < LIGHTBULB_CHANNELS; i++) {
                if (private_abs(lightbulb_group->target[i] - lightbulb_group->current[i]) > private_abs(LIGHTBULB_STEP_VALUE[i])) {
                    all_channels_ready = false;
                    lightbulb_group->current[i] += LIGHTBULB_STEP_VALUE[i];
                    lightbulb_group->has_changed = true;
                    
                    if (LIGHTBULB_TYPE == LIGHTBULB_TYPE_PWM) {
                        haa_pwm_set_duty(lightbulb_group->gpio[i], lightbulb_group->current[i]);
                    }
                } else if (lightbulb_group->current[i] != lightbulb_group->target[i]) {
                    lightbulb_group->current[i] = lightbulb_group->target[i];
                    lightbulb_group->has_changed = true;
                    
                    if (LIGHTBULB_TYPE == LIGHTBULB_TYPE_PWM) {
                        haa_pwm_set_duty(lightbulb_group->gpio[i], lightbulb_group->current[i]);
                    }
                }
            }
            
            //if (lightbulb_group->has_changed && LIGHTBULB_TYPE == LIGHTBULB_TYPE_MY92X1) {
                //
                // TO-DO
                //
            //}
        }
        
        lightbulb_group = lightbulb_group->next;
    }
    
#ifndef CONFIG_IDF_TARGET_ESP32C2
    addressled_t* addressled = main_config.addressleds;
    while (addressled) {
        uint8_t* colors = malloc(addressled->max_range);
        if (colors) {
            memset(colors, 0, addressled->max_range);
            bool has_changed = false;
            
            lightbulb_group = main_config.lightbulb_groups;
            while (lightbulb_group) {
                if (LIGHTBULB_TYPE == LIGHTBULB_TYPE_NRZ && lightbulb_group->gpio[0] == addressled->gpio) {
                    if (lightbulb_group->has_changed) {
                        has_changed = true;
                    }
                    
                    for (int p = LIGHTBULB_RANGE_START; p < LIGHTBULB_RANGE_END; p = p + LIGHTBULB_CHANNELS) {
                        for (int i = 0; i < LIGHTBULB_CHANNELS; i++) {
                            uint8_t color = lightbulb_group->current[addressled->map[i]] >> 8;
                            memcpy(colors + p + i, &color, 1);
                        }
                    }
                }
                
                lightbulb_group = lightbulb_group->next;
            }
            
            if (has_changed) {
#ifdef ESP_PLATFORM
                rmt_transmit_config_t tx_config = {
                    .loop_count = 0,
                };
                
                rmt_enable(addressled->rmt_channel_handle);
                rmt_transmit(addressled->rmt_channel_handle, addressled->rmt_encoder_handle, colors, addressled->max_range, &tx_config);
                rmt_tx_wait_all_done(addressled->rmt_channel_handle, -1);
                rmt_disable(addressled->rmt_channel_handle);
#else
                HAA_ENTER_CRITICAL_TASK();
                nrzled_set(addressled->gpio, addressled->time_0, addressled->time_1, addressled->period, colors, addressled->max_range);
                HAA_EXIT_CRITICAL_TASK();
#endif
            }
            
            free(colors);
        } else {
            homekit_remove_oldest_client();
            break;
        }
        
        addressled = addressled->next;
    }
#endif  // no CONFIG_IDF_TARGET_ESP32C2
    
    if (all_channels_ready) {
        rs_esp_timer_stop(main_config.set_lightbulb_timer);
        
        INFO("RGBW done");
    }
}

void lightbulb_no_task(ch_group_t* ch_group) {
    lightbulb_group_t* lightbulb_group = lightbulb_group_find(ch_group->ch[0]);
    
    led_blink(1);
    INFO("<%i> -> LB ON %i BRI %i", ch_group->serv_index, ch_group->ch[0]->value.bool_value, ch_group->ch[1]->value.int_value);
    if (LIGHTBULB_CHANNELS == 2) {
        INFO("<%i> LB TEMP %i", ch_group->serv_index, ch_group->ch[2]->value.int_value);
    } else if (LIGHTBULB_CHANNELS >= 3) {
        /*
        if (lightbulb_group->temp_changed) {
            INFO("<%i> LB setter TEMP %i", ch_group->serv_index, ch_group->ch[4]->value.int_value);
        } else {
        */
            INFO("<%i> LB HUE %g SAT %g", ch_group->serv_index, ch_group->ch[2]->value.float_value, ch_group->ch[3]->value.float_value);
        //}
    }
    
    if (ch_group->ch[0]->value.bool_value) {
        // Turn ON
        if (lightbulb_group->target[0] == 0 &&
            lightbulb_group->target[1] == 0 &&
            lightbulb_group->target[2] == 0 &&
            lightbulb_group->target[3] == 0 &&
            lightbulb_group->target[4] == 0) {
            
            setup_mode_toggle_upcount(ch_group->homekit_enabled);
        }

        if (LIGHTBULB_CHANNELS >= 3) {
            // Channels 3+
            hsi2rgbw(ch_group->ch[2]->value.float_value, ch_group->ch[3]->value.float_value, ch_group->ch[1]->value.int_value, ch_group);
            
        } else if (LIGHTBULB_CHANNELS == 2) {
            // Channels 2
            unsigned int target_color = 0;
            if (ch_group->ch[2]->value.int_value >= COLOR_TEMP_MAX - 5) {
                target_color = PWM_SCALE;
                
            } else if (ch_group->ch[2]->value.int_value > COLOR_TEMP_MIN + 1) { // Conversion based on @seritos curve
                target_color = PWM_SCALE * (((0.09 + fast_precise_pow(0.18 + (0.1352 * (ch_group->ch[2]->value.int_value - COLOR_TEMP_MIN - 1)), 0.5)) / 0.0676) - 1) / 100;
            }
            
            if (LIGHTBULB_TYPE == LIGHTBULB_TYPE_PWM_CWWW) {
                const uint32_t w = PWM_SCALE * ch_group->ch[1]->value.int_value / 100;
                lightbulb_group->target[0] = HAA_MIN(LIGHTBULB_MAX_POWER * w, PWM_SCALE);
                lightbulb_group->target[1] = target_color;
                
            } else {
                const uint32_t cw = lightbulb_group->flux[0] * target_color * ch_group->ch[1]->value.int_value / 100;
                const uint32_t ww = lightbulb_group->flux[1] * (PWM_SCALE - target_color) * ch_group->ch[1]->value.int_value / 100;
                lightbulb_group->target[0] = HAA_MIN(LIGHTBULB_MAX_POWER * cw, PWM_SCALE);
                lightbulb_group->target[1] = HAA_MIN(LIGHTBULB_MAX_POWER * ww, PWM_SCALE);
            }
            
        } else {
            // Channels 1
            const uint32_t w = PWM_SCALE * ch_group->ch[1]->value.int_value / 100;
            lightbulb_group->target[0] = HAA_MIN(LIGHTBULB_MAX_POWER * w, PWM_SCALE);
        }
    } else {
        // Turn OFF
        lightbulb_group->autodimmer = 0;
        
        for (int i = 0; i < 5; i++) {
            lightbulb_group->target[i] = 0;
        }
        
        setup_mode_toggle_upcount(ch_group->homekit_enabled);
    }
    
    homekit_characteristic_notify_safe(ch_group->ch[0]);
    
    INFO("<%i> RGBW %i, %i, %i, %i, %i", ch_group->serv_index, lightbulb_group->target[0],
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
            int max_diff = private_abs(lightbulb_group->current[0] - lightbulb_group->target[0]);
            for (int i = 1; i < LIGHTBULB_CHANNELS; i++) {
                const int diff = private_abs(lightbulb_group->current[i] - lightbulb_group->target[i]);
                if (diff > max_diff) {
                    max_diff = diff;
                }
            }
            
            const int steps = (max_diff / lightbulb_group->step) + 1;
            
            for (int i = 0; i < LIGHTBULB_CHANNELS; i++) {
                if (lightbulb_group->target[i] == lightbulb_group->current[i]) {
                    LIGHTBULB_STEP_VALUE[i] = 0;
                } else {
                    LIGHTBULB_STEP_VALUE[i] = ((lightbulb_group->target[i] - lightbulb_group->current[i]) / steps) + (lightbulb_group->target[i] > lightbulb_group->current[i] ? 1 : -1);
                }
            }
        }
        
        if (LIGHTBULB_TYPE != LIGHTBULB_TYPE_VIRTUAL && xTimerIsTimerActive(main_config.set_lightbulb_timer) == pdFALSE) {
            rs_esp_timer_start(main_config.set_lightbulb_timer);
            rgbw_set_timer_worker(main_config.set_lightbulb_timer);
        }
        
        homekit_characteristic_notify_safe(ch_group->ch[1]);
        
        if (ch_group->chs > 2) {
            homekit_characteristic_notify_safe(ch_group->ch[2]);
            if (ch_group->chs > 3) {
                homekit_characteristic_notify_safe(ch_group->ch[3]);
            }
        }
    }
    
    if (lightbulb_group->last_on_action != ch_group->ch[0]->value.bool_value) {
        lightbulb_group->last_on_action = ch_group->ch[0]->value.bool_value;
        do_actions(ch_group, ch_group->ch[0]->value.bool_value);
    }
    
    if (ch_group->ch[0]->value.bool_value) {
        do_wildcard_actions(ch_group, 0, ch_group->ch[1]->value.int_value);
    } else {
        ch_group->last_wildcard_action[0] = NO_LAST_WILDCARD_ACTION;
    }
    
    save_states_callback();
    
    lightbulb_group->lightbulb_task_running = false;
}

void lightbulb_task(void* args) {
    ch_group_t* ch_group = args;
    lightbulb_no_task(ch_group);
    
    vTaskDelete(NULL);
}

void lightbulb_task_timer(TimerHandle_t xTimer) {
    if (xTaskCreate(lightbulb_task, "LB", LIGHTBULB_TASK_SIZE, (void*) pvTimerGetTimerID(xTimer), LIGHTBULB_TASK_PRIORITY, NULL) != pdPASS) {
        ch_group_t* ch_group = (void*) pvTimerGetTimerID(xTimer);
        lightbulb_group_t* lightbulb_group = lightbulb_group_find(ch_group->ch[0]);
        lightbulb_group->lightbulb_task_running = false;
        ERROR("New LB");
        homekit_remove_oldest_client();
        rs_esp_timer_start(xTimer);
    }
}

void hkc_rgbw_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (!ch_group->main_enabled) {
        homekit_characteristic_notify_safe(ch);
        
    } else if (ch != ch_group->ch[0] || value.bool_value != ch_group->ch[0]->value.bool_value) {
        lightbulb_group_t* lightbulb_group = lightbulb_group_find(ch_group->ch[0]);
        
        if (ch == ch_group->ch[1] && value.int_value == 0) {
            lightbulb_group->old_on_value = ch_group->ch[0]->value.bool_value;
            ch_group->ch[0]->value.bool_value = false;
        } else {
            lightbulb_group->old_on_value = ch_group->ch[0]->value.bool_value;
            ch->value = value;
        }
        
        /*
        if (ch == ch_group->ch[4]) {
            lightbulb_group->temp_changed = true;
        } else {
            lightbulb_group->temp_changed = false;
        }
        */
        
        if (ch == ch_group->ch[0] || (ch == ch_group->ch[1] && value.int_value == 0)) {
            if (ch_group->ch[0]->value.bool_value) {
                rs_esp_timer_start(AUTOOFF_TIMER);
            } else {
                rs_esp_timer_stop(AUTOOFF_TIMER);
            }
        }
        
        if (lightbulb_group->old_on_value == ch_group->ch[0]->value.bool_value && lightbulb_group->autodimmer == 0) {
            rs_esp_timer_start(LIGHTBULB_SET_DELAY_TIMER);
        } else if (!lightbulb_group->lightbulb_task_running) {
            lightbulb_group->lightbulb_task_running = true;
            rs_esp_timer_stop(LIGHTBULB_SET_DELAY_TIMER);
            lightbulb_task_timer(LIGHTBULB_SET_DELAY_TIMER);
        }
        
        save_data_history(ch);
        
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
    
    INFO("<%i> AUTODim ON", ch_group->serv_index);
    
    lightbulb_group->autodimmer_reverse = !lightbulb_group->autodimmer_reverse;
    
    while (lightbulb_group->autodimmer > 0) {
        lightbulb_group->autodimmer--;
        
        if (!lightbulb_group->autodimmer_reverse) {
            int new_result = ch_group->ch[1]->value.int_value + lightbulb_group->autodimmer_task_step;
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
            int new_result = ch_group->ch[1]->value.int_value - lightbulb_group->autodimmer_task_step;
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
    
    INFO("<%i> AUTODim OFF", ch_group->serv_index);
    
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
            rs_esp_timer_stop(LIGHTBULB_AUTODIMMER_TIMER);
            
            if (xTaskCreate(autodimmer_task, "DIM", AUTODIMMER_TASK_SIZE, (void*) ch0, AUTODIMMER_TASK_PRIORITY, NULL) != pdPASS) {
                ERROR("<%i> New DIM", ch_group->serv_index);
                homekit_remove_oldest_client();
            }
        } else {
            rs_esp_timer_start(LIGHTBULB_AUTODIMMER_TIMER);
            lightbulb_group->armed_autodimmer = true;
        }
    }
}

// --- GARAGE DOOR
void garage_door_stop(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    if (GD_CURRENT_DOOR_STATE_INT == GARAGE_DOOR_OPENING || GD_CURRENT_DOOR_STATE_INT == GARAGE_DOOR_CLOSING) {
        led_blink(1);
        INFO("<%i> GD Stop", ch_group->serv_index);
        
        GD_CURRENT_DOOR_STATE_INT = GARAGE_DOOR_STOPPED;
        
        rs_esp_timer_stop(GARAGE_DOOR_WORKER_TIMER);
        
        save_data_history(ch_group->ch[0]);
        
        do_actions(ch_group, 10);
        
        homekit_characteristic_notify_safe(GD_TARGET_DOOR_STATE);
        homekit_characteristic_notify_safe(GD_CURRENT_DOOR_STATE);
    }
}

void garage_door_obstruction(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    led_blink(1);
    INFO("<%i> GD Obs %i", ch_group->serv_index, type);
    
    GD_OBSTRUCTION_DETECTED_BOOL = (bool) type;
    rs_esp_timer_stop(AUTOOFF_TIMER);
    
    homekit_characteristic_notify_safe(GD_OBSTRUCTION_DETECTED);
    
    save_data_history(ch_group->ch[2]);
    
    do_actions(ch_group, type + 8);
}

void garage_door_sensor(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    led_blink(1);
    INFO("<%i> GD sensor %i", ch_group->serv_index, type);
    
    if (type == GARAGE_DOOR_OPENED || type == GARAGE_DOOR_CLOSING) {
        GARAGE_DOOR_CURRENT_TIME = GARAGE_DOOR_WORKING_TIME - GARAGE_DOOR_TIME_MARGIN;
    } else {
        GARAGE_DOOR_CURRENT_TIME = GARAGE_DOOR_TIME_MARGIN;
    }
    
    GD_CURRENT_DOOR_STATE_INT = type;
    
    if (type > 1) {
        GD_TARGET_DOOR_STATE_INT = type - 2;
        rs_esp_timer_stop(AUTOOFF_TIMER);
        rs_esp_timer_start(GARAGE_DOOR_WORKER_TIMER);
        
    } else {
        rs_esp_timer_stop(GARAGE_DOOR_WORKER_TIMER);
        GD_TARGET_DOOR_STATE_INT = type;
    }
    
    // Remove obstruction
    if (GD_OBSTRUCTION_DETECTED_BOOL) {
        garage_door_obstruction(99, ch_group, 0);
    }
    
    homekit_characteristic_notify_safe(GD_TARGET_DOOR_STATE);
    homekit_characteristic_notify_safe(GD_CURRENT_DOOR_STATE);
    
    save_data_history(ch_group->ch[0]);
    
    do_actions(ch_group, type + 4);
}

void hkc_garage_door_setter(homekit_characteristic_t* ch1, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch1);
    
    void run_margin_at_start() {
        rs_esp_timer_stop(AUTOOFF_TIMER);
        rs_esp_timer_start(GARAGE_DOOR_WORKER_TIMER);
        homekit_characteristic_notify_safe(ch1);
    }
    
    if (ch_group->main_enabled) {
        int current_door_state = GD_CURRENT_DOOR_STATE_INT;
        int stopped_offset = 0;
        if (current_door_state == GARAGE_DOOR_STOPPED) {
            stopped_offset = GD_FROM_STOPPED_ACTION_OFFSET;
            current_door_state = GD_TARGET_DOOR_STATE_INT;
            GD_CURRENT_DOOR_STATE_INT = current_door_state + 2;
        } else if (current_door_state > GARAGE_DOOR_CLOSED) {
            if (GARAGE_DOOR_VIRTUAL_STOP == 1) {
                garage_door_stop(99, ch_group, 0);
            } else {
                current_door_state -= 2;
            }
        }
        
        if ((value.int_value != current_door_state && GD_CURRENT_DOOR_STATE_INT != GARAGE_DOOR_STOPPED) || GD_OBSTRUCTION_DETECTED_BOOL) {
            led_blink(1);
            INFO("<%i> -> GD %i", ch_group->serv_index, value.int_value);
            
            ch1->value.int_value = value.int_value;
            
            do_actions(ch_group, GD_CURRENT_DOOR_STATE_INT + stopped_offset);
            
            if ((value.int_value == GARAGE_DOOR_OPENED && GARAGE_DOOR_HAS_F4 == 0) ||
                GD_CURRENT_DOOR_STATE_INT == GARAGE_DOOR_CLOSING) {
                garage_door_sensor(99, ch_group, GARAGE_DOOR_OPENING);
                
            } else if ((value.int_value == GARAGE_DOOR_CLOSED && GARAGE_DOOR_HAS_F5 == 0) ||
                       GD_CURRENT_DOOR_STATE_INT == GARAGE_DOOR_OPENING) {
                garage_door_sensor(99, ch_group, GARAGE_DOOR_CLOSING);
                
            } else if (GARAGE_DOOR_TIME_MARGIN > 0 && value.int_value == GARAGE_DOOR_OPENED && GD_CURRENT_DOOR_STATE_INT == GARAGE_DOOR_CLOSED && GARAGE_DOOR_HAS_F4 == 1) {
                GARAGE_DOOR_CURRENT_TIME = 0;
                GD_TARGET_DOOR_STATE_INT = GARAGE_DOOR_OPENED;
                run_margin_at_start();
                
            } else if (GARAGE_DOOR_TIME_MARGIN > 0 && value.int_value == GARAGE_DOOR_CLOSED && GD_CURRENT_DOOR_STATE_INT == GARAGE_DOOR_OPENED && GARAGE_DOOR_HAS_F5 == 1) {
                GARAGE_DOOR_CURRENT_TIME = GARAGE_DOOR_WORKING_TIME;
                GD_TARGET_DOOR_STATE_INT = GARAGE_DOOR_CLOSED;
                run_margin_at_start();
                
            } else {
                homekit_characteristic_notify_safe(ch1);
            }
            
            setup_mode_toggle_upcount(ch_group->homekit_enabled);
            
        } else {
            homekit_characteristic_notify_safe(ch1);
        }
        
        save_data_history(ch1);
        
    } else {
        homekit_characteristic_notify_safe(ch1);
    }
}

void garage_door_timer_worker(TimerHandle_t xTimer) {
    ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
    
    void halt_timer(const bool forced) {
        rs_esp_timer_stop(xTimer);
        
        if (forced || (GARAGE_DOOR_TIME_MARGIN > 0 && GD_CURRENT_DOOR_STATE_INT > 1)) {
            garage_door_obstruction(99, ch_group, 3);
        }
    }
    
    if (GD_CURRENT_DOOR_STATE_INT == GARAGE_DOOR_OPENING) {
        GARAGE_DOOR_CURRENT_TIME++;
        
        if (GARAGE_DOOR_CURRENT_TIME >= (GARAGE_DOOR_WORKING_TIME - GARAGE_DOOR_TIME_MARGIN) && GARAGE_DOOR_HAS_F2 == 0) {
            rs_esp_timer_start(AUTOOFF_TIMER);
            garage_door_sensor(99, ch_group, GARAGE_DOOR_OPENED);
            
        } else if (GARAGE_DOOR_CURRENT_TIME >= GARAGE_DOOR_WORKING_TIME && GARAGE_DOOR_HAS_F2 == 1) {
            halt_timer(false);
        }
        
    } else if (GD_CURRENT_DOOR_STATE_INT == GARAGE_DOOR_CLOSING) {
        GARAGE_DOOR_CURRENT_TIME -= GARAGE_DOOR_CLOSE_TIME_FACTOR;
        
        if (GARAGE_DOOR_CURRENT_TIME <= GARAGE_DOOR_TIME_MARGIN && GARAGE_DOOR_HAS_F3 == 0) {
            garage_door_sensor(99, ch_group, GARAGE_DOOR_CLOSED);
            
        } else if (GARAGE_DOOR_CURRENT_TIME <= 0 && GARAGE_DOOR_HAS_F3 == 1) {
            halt_timer(false);
        }
        
    } else if (GARAGE_DOOR_TIME_MARGIN > 0 &&
               GD_CURRENT_DOOR_STATE_INT == GARAGE_DOOR_CLOSED &&
               GD_TARGET_DOOR_STATE_INT == GARAGE_DOOR_OPENED &&
               GARAGE_DOOR_HAS_F4 == 1) {
        GARAGE_DOOR_CURRENT_TIME++;
        
        if (GARAGE_DOOR_CURRENT_TIME > GARAGE_DOOR_TIME_MARGIN) {
            halt_timer(true);
        }
        
    } else if (GARAGE_DOOR_TIME_MARGIN > 0 &&
               GD_CURRENT_DOOR_STATE_INT == GARAGE_DOOR_OPENED &&
               GD_TARGET_DOOR_STATE_INT == GARAGE_DOOR_CLOSED &&
               GARAGE_DOOR_HAS_F5 == 1) {
        GARAGE_DOOR_CURRENT_TIME -= GARAGE_DOOR_CLOSE_TIME_FACTOR;
        
        if (GARAGE_DOOR_CURRENT_TIME < (GARAGE_DOOR_WORKING_TIME - GARAGE_DOOR_TIME_MARGIN)) {
            halt_timer(true);
        }
        
    } else {
        rs_esp_timer_stop(xTimer);
    }
    
    INFO("<%i> GD Pos %g/%g", ch_group->serv_index, GARAGE_DOOR_CURRENT_TIME, GARAGE_DOOR_WORKING_TIME);
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
    rs_esp_timer_stop(ch_group->timer);
    
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
    
    INFO("<%i> WC Mt %f HK %f", ch_group->serv_index, WINDOW_COVER_MOTOR_POSITION, WINDOW_COVER_HOMEKIT_POSITION);
    
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
    
    homekit_characteristic_notify_safe(WINDOW_COVER_CH_TARGET_POSITION);
    homekit_characteristic_notify_safe(WINDOW_COVER_CH_STATE);
    homekit_characteristic_notify_safe(WINDOW_COVER_CH_CURRENT_POSITION);
    
    setup_mode_toggle_upcount(ch_group->homekit_enabled);
    
    save_data_history(ch_group->ch[0]);
    save_data_history(ch_group->ch[1]);
    save_data_history(ch_group->ch[2]);
    
    save_states_callback();
}

void window_cover_obstruction(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    led_blink(1);
    INFO("<%i> WC Obs %i", ch_group->serv_index, type);
    
    WINDOW_COVER_CH_OBSTRUCTION->value.bool_value = (bool) type;
    
    do_actions(ch_group, type + WINDOW_COVER_OBSTRUCTION);
    
    if (WINDOW_COVER_VIRTUAL_STOP == 2) {
        WINDOW_COVER_VIRTUAL_STOP = 1;
    }
    
    save_data_history(ch_group->ch[3]);
    
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
        rs_esp_timer_start(ch_group->timer2);
    }
    
    void start_wc_timer() {
        if (WINDOW_COVER_CH_STATE->value.int_value == WINDOW_COVER_STOP) {
            rs_esp_timer_start(ch_group->timer);
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
        INFO("<%i> -> WC %i->%i", ch_group->serv_index, WINDOW_COVER_CH_CURRENT_POSITION->value.int_value, value.int_value);
        
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
                
                setup_mode_toggle_upcount(ch_group->homekit_enabled);
                
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
                
                setup_mode_toggle_upcount(ch_group->homekit_enabled);
                
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
        
        save_data_history(ch1);
    }
    
    if (WINDOW_COVER_VIRTUAL_STOP == 2) {
        WINDOW_COVER_VIRTUAL_STOP = 1;
    }
    
    homekit_characteristic_notify_safe(ch1);
    homekit_characteristic_notify_safe(WINDOW_COVER_CH_STATE);
    homekit_characteristic_notify_safe(WINDOW_COVER_CH_CURRENT_POSITION);
}

void window_cover_timer_worker(TimerHandle_t xTimer) {
    ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
    
    int margin = 0;     // Used as covering offset to add extra time when target position completely closed or opened
    if (WINDOW_COVER_CH_TARGET_POSITION->value.int_value == 0 || WINDOW_COVER_CH_TARGET_POSITION->value.int_value == 100) {
        margin = WINDOW_COVER_MARGIN_SYNC;
    }
    
    void normalize_current_position() {
        const int wc_homekit_position = WINDOW_COVER_HOMEKIT_POSITION;
        if (wc_homekit_position < 0) {
            WINDOW_COVER_CH_CURRENT_POSITION->value.int_value = 0;
        } else if (wc_homekit_position > 100) {
            WINDOW_COVER_CH_CURRENT_POSITION->value.int_value = 100;
        } else {
            WINDOW_COVER_CH_CURRENT_POSITION->value.int_value = wc_homekit_position;
        }
    }
    
    float window_cover_homekit_position() {
        return WINDOW_COVER_MOTOR_POSITION / (1.00000000f + ((100.00000000f - WINDOW_COVER_MOTOR_POSITION) * ((float) WINDOW_COVER_CORRECTION) * 0.00020000f));
    }
    
    switch (WINDOW_COVER_CH_STATE->value.int_value) {
        case WINDOW_COVER_CLOSING:
            WINDOW_COVER_MOTOR_POSITION -= window_cover_step(ch_group, WINDOW_COVER_TIME_CLOSE);
            if (WINDOW_COVER_MOTOR_POSITION > 0) {
                WINDOW_COVER_HOMEKIT_POSITION = window_cover_homekit_position();
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
                WINDOW_COVER_HOMEKIT_POSITION = window_cover_homekit_position();
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
void process_fan_task(void* args) {
    ch_group_t* ch_group = args;
    
    led_blink(1);
    
    INFO("<%i> FAN %i Sp %g", ch_group->serv_index, ch_group->ch[0]->value.bool_value, ch_group->ch[1]->value.float_value);
    
    if (FAN_CURRENT_ACTION != ch_group->ch[0]->value.bool_value) {
        FAN_CURRENT_ACTION = ch_group->ch[0]->value.bool_value;
        do_actions(ch_group, FAN_CURRENT_ACTION);
    }
    
    if (ch_group->ch[0]->value.bool_value) {
        do_wildcard_actions(ch_group, 0, ch_group->ch[1]->value.float_value);
    } else {
        ch_group->last_wildcard_action[0] = NO_LAST_WILDCARD_ACTION;
    }
    
    save_states_callback();
    
    homekit_characteristic_notify_safe(ch_group->ch[0]);
    homekit_characteristic_notify_safe(ch_group->ch[1]);
    
    if (FAN_SET_DELAY_TIMER) {
        vTaskDelete(NULL);
    }
}

void process_fan_timer(TimerHandle_t xTimer) {
    if (xTaskCreate(process_fan_task, "FAN", PROCESS_FAN_TASK_SIZE, (void*) pvTimerGetTimerID(xTimer), PROCESS_FAN_TASK_PRIORITY, NULL) != pdPASS) {
        ERROR("New FAN");
        homekit_remove_oldest_client();
        rs_esp_timer_start(xTimer);
    }
}

void hkc_fan_setter(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        INFO("<%i> -> FAN", ch_group->serv_index);
        
        const int old_on_value = ch_group->ch[0]->value.bool_value;
        
        ch->value = value;
        
        if (FAN_SET_DELAY_TIMER) {
            rs_esp_timer_start(FAN_SET_DELAY_TIMER);
        } else {
            process_fan_task((void*) ch_group);
        }
        
        if (ch == ch_group->ch[0] && ch->value.bool_value != old_on_value) {
            setup_mode_toggle_upcount(ch_group->homekit_enabled);
            
            if (ch_group->ch[0]->value.bool_value) {
                rs_esp_timer_start(AUTOOFF_TIMER);
            } else {
                rs_esp_timer_stop(AUTOOFF_TIMER);
            }
        }
        
        save_data_history(ch);

    } else {
        homekit_characteristic_notify_safe(ch);
    }
}

void hkc_fan_status_setter(homekit_characteristic_t* ch0, const homekit_value_t value) {
    if (ch0->value.bool_value != value.bool_value) {
        led_blink(1);
        ch_group_t* ch_group = ch_group_find(ch0);
        
        INFO("<%i> -> FAN St %i", ch_group->serv_index, value.bool_value);
        
        ch0->value.bool_value = value.bool_value;
        
        save_states_callback();
    }
    
    homekit_characteristic_notify_safe(ch0);
}

// --- LIGHT SENSOR
void light_sensor_task(void* args) {
    ch_group_t* ch_group = (ch_group_t*) args;
    
    float luxes = 0.0001f;
    
    const unsigned int light_sersor_type = LIGHT_SENSOR_TYPE;
    if (light_sersor_type < 2) {
        // https://www.allaboutcircuits.com/projects/design-a-luxmeter-using-a-light-dependent-resistor/
#ifdef ESP_PLATFORM
        int adc = 0;
        adc_channel_t adc_channel;
        adc_oneshot_io_to_channel(LIGHT_SENSOR_GPIO, NULL, &adc_channel);
        adc_oneshot_read(main_config.adc_dac_data->adc_oneshot_handle, adc_channel, &adc);
        const float adc_raw_read = adc;
#else
        const float adc_raw_read = sdk_system_adc_read();
#endif
        
        float ldr_resistor;
        if (light_sersor_type == 0) {
            ldr_resistor = LIGHT_SENSOR_RESISTOR * (((HAA_ADC_MAX_VALUE - 0.9) - adc_raw_read) / adc_raw_read);
        } else {
            ldr_resistor = (LIGHT_SENSOR_RESISTOR  * adc_raw_read) / ((HAA_ADC_MAX_VALUE - 0.9) - adc_raw_read);
        }
        
        luxes = 1 / fast_precise_pow(ldr_resistor, LIGHT_SENSOR_POW);
        
    } else if (light_sersor_type == 2) {  // BH1750
        uint8_t value[2] = { 0, 0 };
        adv_i2c_slave_read(LIGHT_SENSOR_I2C_BUS, (uint8_t) LIGHT_SENSOR_I2C_ADDR, NULL, 0, value, 2);
        
        luxes = byte_array_to_num(value, 2, 0b00) / 1.2f;
    }
    
    luxes = (luxes * LIGHT_SENSOR_FACTOR) + LIGHT_SENSOR_OFFSET;
    INFO("<%i> -> LUX %g", ch_group->serv_index, luxes);
    
    if (luxes < 0.0001f) {
        luxes = 0.0001f;
    } else if (luxes > 100000.f) {
        luxes = 100000.f;
    }
    
    if ((uint32_t) ch_group->ch[0]->value.float_value != (uint32_t) luxes) {
        ch_group->ch[0]->value.float_value = luxes;
        homekit_characteristic_notify_safe(ch_group->ch[0]);
        
        if (ch_group->main_enabled) {
            do_wildcard_actions(ch_group, 0, luxes);
        }
    }
    
    ch_group->is_working = false;
    
    vTaskDelete(NULL);
}

void light_sensor_timer_worker(TimerHandle_t xTimer) {
    if (!homekit_is_pairing()) {
        ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
        if (!ch_group->is_working) {
            if (xTaskCreate(light_sensor_task, "LUX", LIGHT_SENSOR_TASK_SIZE, (void*) ch_group, LIGHT_SENSOR_TASK_PRIORITY, NULL) == pdPASS) {
                ch_group->is_working = true;
            } else {
                ERROR("New LUX");
                homekit_remove_oldest_client();
            }
        }
    } else {
        ERROR("HK pair");
    }
}

// --- SECURITY SYSTEM
void hkc_sec_system(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        if (ch->value.int_value != value.int_value) {
            led_blink(1);
            INFO("<%i> -> SEC SYS %i", ch_group->serv_index, value.int_value);
            
            rs_esp_timer_stop(SEC_SYSTEM_REC_ALARM_TIMER);
            
            ch->value.int_value = value.int_value;
            SEC_SYSTEM_CH_CURRENT_STATE->value.int_value = value.int_value;
            
            do_actions(ch_group, value.int_value);
            
            setup_mode_toggle_upcount(ch_group->homekit_enabled);
            
            save_states_callback();
            
            save_data_history(ch);
            save_data_history(SEC_SYSTEM_CH_CURRENT_STATE);
        }
    }
    
    homekit_characteristic_notify_safe(ch);
    homekit_characteristic_notify_safe(SEC_SYSTEM_CH_CURRENT_STATE);
}

void hkc_sec_system_status(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch->value.int_value != value.int_value) {
        led_blink(1);
        INFO("<%i> -> SEC SYS St %i", ch_group->serv_index, value.int_value);
        
        rs_esp_timer_stop(SEC_SYSTEM_REC_ALARM_TIMER);
        
        ch->value.int_value = value.int_value;
        SEC_SYSTEM_CH_CURRENT_STATE->value.int_value = value.int_value;
        
        save_states_callback();
        
        save_data_history(ch);
        save_data_history(SEC_SYSTEM_CH_CURRENT_STATE);
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
            INFO("<%i> -> TV ON %i", ch_group->serv_index, value.int_value);
            
            ch0->value.int_value = value.int_value;
            
            do_actions(ch_group, value.int_value);
            
            setup_mode_toggle_upcount(ch_group->homekit_enabled);
            
            save_states_callback();
            
            save_data_history(ch0);
        }
    }
    
    homekit_characteristic_notify_safe(ch0);
}

void hkc_tv_status_active(homekit_characteristic_t* ch0, const homekit_value_t value) {
    if (ch0->value.int_value != value.int_value) {
        led_blink(1);
        ch_group_t* ch_group = ch_group_find(ch0);
        
        INFO("<%i> -> TV ON St %i", ch_group->serv_index, value.int_value);
        
        ch0->value.int_value = value.int_value;
        
        save_states_callback();
        
        save_data_history(ch0);
    }
    
    homekit_characteristic_notify_safe(ch0);
}

void hkc_tv_active_identifier(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        if (ch->value.int_value != value.int_value) {
            led_blink(1);
            INFO("<%i> -> TV In %i", ch_group->serv_index, value.int_value);
            
            ch->value.int_value = value.int_value;

            do_actions(ch_group, (MAX_ACTIONS - 1) + value.int_value);
            
            save_data_history(ch);
        }
    }
    
    homekit_characteristic_notify_safe(ch);
}

void hkc_tv_key(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("<%i> -> TV Key %i", ch_group->serv_index, value.int_value + 2);
        
        ch->value.int_value = value.int_value;
        
        do_actions(ch_group, value.int_value + 2);
        
        save_data_history(ch);
    }
    
    homekit_characteristic_notify_safe(ch);
}

void hkc_tv_power_mode(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("<%i> -> TV Settings %i", ch_group->serv_index, value.int_value + 30);
        
        ch->value.int_value = value.int_value;
        
        do_actions(ch_group, value.int_value + 30);
        
        save_data_history(ch);
    }
    
    homekit_characteristic_notify_safe(ch);
}

void hkc_tv_mute(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("<%i> -> TV Mute %i", ch_group->serv_index, value.int_value + 20);
        
        ch->value.int_value = value.int_value;
        
        do_actions(ch_group, value.int_value + 20);
        
        save_data_history(ch);
    }
    
    homekit_characteristic_notify_safe(ch);
}

void hkc_tv_volume(homekit_characteristic_t* ch, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch);
    if (ch_group->main_enabled) {
        led_blink(1);
        INFO("<%i> -> TV Vol %i", ch_group->serv_index, value.int_value + 22);
        
        ch->value.int_value = value.int_value;
        
        do_actions(ch_group, value.int_value + 22);
        
        save_data_history(ch);
    }
    
    homekit_characteristic_notify_safe(ch);
}

void hkc_tv_configured_name(homekit_characteristic_t* ch1, const homekit_value_t value) {
    ch_group_t* ch_group = ch_group_find(ch1);
    
    if (value.string_value && strcmp(value.string_value, ch1->value.string_value)) {
        INFO("<%i> -> TV Name %s", ch_group->serv_index, value.string_value);
        
        free(ch1->value.string_value);
        ch1->value.string_value = strdup(value.string_value);
        
        homekit_characteristic_notify_safe(ch1);
        
        save_states_callback();
    }
}

void hkc_tv_input_configured_name(homekit_characteristic_t* ch, const homekit_value_t value) {
    homekit_characteristic_notify_safe(ch);
}

// --- BATTERY
void battery_manager(homekit_characteristic_t* ch, int value, bool is_free_monitor) {
    ch_group_t* ch_group = ch_group_find(ch);
    
    if (ch_group->main_enabled) {
        INFO("<%i> -> BAT %i%%", ch_group->serv_index, value);
        
        if (value < 0 && !is_free_monitor) {
            if (value < 100) {
                value = BATTERY_LEVEL_CH_INT + value + 100;
            } else {
                value = BATTERY_LEVEL_CH_INT - value;
            }
        }
        
        if (value > 100) {
            value = 100;
        } else if (value < 0) {
            value = 0;
        }
        
        if (BATTERY_LEVEL_CH_INT != value || is_free_monitor) {
            BATTERY_LEVEL_CH_INT = value;
            
            do_wildcard_actions(ch_group, 0, value);
            
            save_data_history(ch);
            
            if (!is_free_monitor) {
                homekit_characteristic_notify_safe(ch);
            }
            
            int status_low_changed = false;
            if (value <= BATTERY_LOW_THRESHOLD &&
                BATTERY_STATUS_LOW_CH_INT == 0) {
                BATTERY_STATUS_LOW_CH_INT = 1;
                status_low_changed = true;
                
            } else if (value > BATTERY_LOW_THRESHOLD &&
                BATTERY_STATUS_LOW_CH_INT == 1) {
                BATTERY_STATUS_LOW_CH_INT = 0;
                status_low_changed = true;
            }
            
            if (status_low_changed) {
                INFO("<%i> BAT Low %i", ch_group->serv_index, BATTERY_STATUS_LOW_CH_INT);
                
                save_data_history(BATTERY_STATUS_LOW_CH);
                
                do_actions(ch_group, BATTERY_STATUS_LOW_CH_INT);
                
                homekit_characteristic_notify_safe(BATTERY_STATUS_LOW_CH);
            }
        }
    }
}

bool set_hkch_value(homekit_characteristic_t* ch_target, const float value) {
    int has_changed = false;
    
    switch (ch_target->value.format) {
        case HOMEKIT_FORMAT_BOOL:
            if (ch_target->value.bool_value != ((bool) ((int) value))) {
                ch_target->value.bool_value = (bool) ((int) value);
                has_changed = true;
            }
            break;
            
        case HOMEKIT_FORMAT_UINT8:
        case HOMEKIT_FORMAT_UINT16:
        case HOMEKIT_FORMAT_UINT32:
        case HOMEKIT_FORMAT_UINT64:
        case HOMEKIT_FORMAT_INT:
            if (ch_target->value.int_value != ((int) value)) {
                ch_target->value.int_value = value;
                has_changed = true;
            }
            break;
            
        case HOMEKIT_FORMAT_FLOAT:
            if (((int) (ch_target->value.float_value * 1000)) != ((int) (value * 1000))) {
                ch_target->value.float_value = value;
                has_changed = true;
            }
            break;
            
        default:
            break;
    }
    
    if (has_changed) {
        homekit_characteristic_notify_safe(ch_target);
        
        ch_group_t* ch_target_group = ch_group_find(ch_target);
        
        if (ch_target_group->serv_type == SERV_TYPE_THERMOSTAT) {
            hkc_th_setter(ch_target, ch_target->value);
            
        } else if (ch_target_group->serv_type == SERV_TYPE_HUMIDIFIER) {
            hkc_humidif_setter(ch_target, ch_target->value);
            
        } else if (ch_target_group->serv_type == SERV_TYPE_LIGHTBULB) {
            if (ch_target_group->ch[0] == ch_target) {
                lightbulb_group_t* lightbulb_group = lightbulb_group_find(ch_target);
                lightbulb_group->old_on_value = ch_target->value.bool_value;
                lightbulb_group->last_on_action = ch_target->value.bool_value;
            }
            
        } else if (ch_target_group->serv_type == SERV_TYPE_BATTERY) {
            battery_manager(ch_target, ch_target->value.int_value, true);
        }
    }
    
    return has_changed;
}

// --- FREE MONITOR
bool find_patterns(pattern_t* pattern_base, uint8_t** bytes, int bytes_len) {
    pattern_t* pattern = pattern_base;
    
    uint8_t* bytes_found = *bytes;
    
    if (bytes_len == 0) {
        bytes_len = strlen((char*) bytes_found);
    }
    
    while (pattern) {
        int is_found = false;
        if (pattern->len > 0) {
            if (pattern->len > bytes_len) {
                return false;
            }
            
            for (int pos = 0; pos <= (bytes_len - pattern->len); pos++) {
                if ((*bytes_found == *pattern->pattern) && memcmp(bytes_found, pattern->pattern, pattern->len) == 0) {
                    is_found = true;
                    bytes_found += pattern->len;
                    bytes_len -= pos;
                    bytes_len -= pattern->len;
                    break;
                }
                
                bytes_found++;
            }
        } else {
            is_found = true;
        }
        
        if (!is_found) {
            return false;
        }
        
        bytes_found += pattern->offset;
        bytes_len -= pattern->offset;
        
        pattern = pattern->next;
    }
    
    *bytes = bytes_found;
    
    return true;
}

void reset_uart_buffer() {
    uart_receiver_data_t* uart_receiver_data = main_config.uart_receiver_data;
    while (uart_receiver_data) {
        if (uart_receiver_data->uart_buffer) {
            free(uart_receiver_data->uart_buffer);
            uart_receiver_data->uart_buffer = NULL;
        }
        
        uart_receiver_data->uart_buffer_len = 0;
        
        uart_receiver_data = uart_receiver_data->next;
    }
    
    main_config.uart_recv_is_working = false;
}

#ifdef ESP_PLATFORM
void IRAM_ATTR fm_pulse_interrupt(void* args) {
    const uint64_t time = esp_timer_get_time();
    const uint8_t gpio = (uint32_t) args;
#else
void IRAM fm_pulse_interrupt(const uint8_t gpio) {
    const uint32_t time = sdk_system_get_time_raw();
#endif
    
    ch_group_t* ch_group = main_config.ch_groups;
    while (ch_group) {
        if ((ch_group->serv_type == SERV_TYPE_FREE_MONITOR ||
             ch_group->serv_type == SERV_TYPE_FREE_MONITOR_ACCUMULATVE) &&
            private_abs(FM_SENSOR_TYPE) == FM_SENSOR_TYPE_PULSE_US_TIME &&
            FM_SENSOR_GPIO == gpio) {
            if (FM_NEW_VALUE == 0) {
                FM_NEW_VALUE = time;
            } else {
#ifdef ESP_PLATFORM
                gpio_intr_disable(gpio);
#else
                gpio_set_interrupt(gpio, GPIO_INTTYPE_NONE, NULL);
#endif
                FM_OVERRIDE_VALUE = time;
            }
            
            break;
        }
        
        ch_group = ch_group->next;
    }
}

bool free_monitor_type_is_pattern(const uint8_t fm_sensor_type) {
    if (fm_sensor_type == FM_SENSOR_TYPE_NETWORK_PATTERN_TEXT ||
        fm_sensor_type == FM_SENSOR_TYPE_NETWORK_PATTERN_HEX ||
        fm_sensor_type == FM_SENSOR_TYPE_UART_PATTERN_HEX ||
        fm_sensor_type == FM_SENSOR_TYPE_UART_PATTERN_TEXT) {
        return true;
    }
    
    return false;
}

void free_monitor_task(void* args) {
    int str_to_float(char* found, char* str, float* value) {
        if (found < str + strlen(str)) {
            while (found[0] && !(CHAR_IS_NUMBER(found[0]))) {
                found++;
            }
            
            if (found[0]) {
                if (found > str) {
                    found--;
                    
                    if (found[0] != '-') {
                        found++;
                    }
                }
                
                *value = atof(found);
                return true;
            }
        }
        
        return false;
    }
    
    ch_group_t* ch_group = main_config.ch_groups;
    
    if (args) {
        ch_group = args;
    } else {
        uart_receiver_data_t* uart_receiver_data = main_config.uart_receiver_data;
        while (uart_receiver_data) {
            if (uart_receiver_data->uart_buffer) {
                INFO_NNL("UART%i RECV ", uart_receiver_data->uart_port);
                
                for (int i = 0; i < uart_receiver_data->uart_buffer_len; i++) {
                    INFO_NNL("%02x", uart_receiver_data->uart_buffer[i]);
                }
                
                INFO("");
            }
            
            uart_receiver_data = uart_receiver_data->next;
        }
    }
    
    while (ch_group) {
        if (ch_group->serv_type == SERV_TYPE_FREE_MONITOR ||
            ch_group->serv_type == SERV_TYPE_FREE_MONITOR_ACCUMULATVE) {
            float value = 0;
            int get_value = false;
            
            const int fm_sensor_type = private_abs(FM_SENSOR_TYPE);
            
            if (args) {
                if (fm_sensor_type == FM_SENSOR_TYPE_FREE || FM_OVERRIDE_VALUE != NO_LAST_WILDCARD_ACTION) {
                    value = FM_OVERRIDE_VALUE;
                    FM_OVERRIDE_VALUE = NO_LAST_WILDCARD_ACTION;
                    get_value = true;
                    
                } else if (fm_sensor_type == FM_SENSOR_TYPE_PULSE_FREQ) {
                    value = adv_hlw_get_power_freq((uint8_t) FM_SENSOR_GPIO);
                    get_value = true;
                    
                } else if (fm_sensor_type == FM_SENSOR_TYPE_PULSE_US_TIME) {
                    const float old_final_value = FM_OVERRIDE_VALUE;
                    FM_NEW_VALUE = 0;
#ifdef ESP_PLATFORM
                    gpio_intr_enable(FM_SENSOR_GPIO);
#else
                    gpio_set_interrupt(FM_SENSOR_GPIO, FM_SENSOR_GPIO_INT_TYPE, fm_pulse_interrupt);
#endif
                    
                    
                    if (((uint8_t) FM_SENSOR_GPIO_TRIGGER) < 255) {
                        const bool inverted = ((uint8_t) FM_SENSOR_GPIO_TRIGGER) / 100;
                        const unsigned int gpio_final = ((uint8_t) FM_SENSOR_GPIO_TRIGGER) % 100;
                        
                        HAA_ENTER_CRITICAL_TASK();
                        gpio_write(gpio_final, inverted);
                        sdk_os_delay_us(20);
                        gpio_write(gpio_final, !inverted);
                        HAA_EXIT_CRITICAL_TASK();
                    }
                    
                    vTaskDelay(MS_TO_TICKS(90));
#ifdef ESP_PLATFORM
                    gpio_intr_disable(FM_SENSOR_GPIO);
#else
                    gpio_set_interrupt(FM_SENSOR_GPIO, GPIO_INTTYPE_NONE, NULL);
#endif
                    
                    if (old_final_value != FM_OVERRIDE_VALUE) {
                        value = FM_OVERRIDE_VALUE - FM_NEW_VALUE;
                        get_value = true;
                    }
                    
                    FM_OVERRIDE_VALUE = NO_LAST_WILDCARD_ACTION;
                    
                } else if (fm_sensor_type == FM_SENSOR_TYPE_MATHS) {
                    int float_index = FM_MATHS_FLOAT_FIRST + (FM_SENSOR_TYPE < 0 ? 2 : 0);
                    int int_index = FM_MATHS_FIRST_OPERATION;
                    for (int i = 0; i < FM_MATHS_OPERATIONS; i++) {
                        const int operation = FM_MATHS_INT[int_index];
                        int_index++;
                        const int read_service = FM_MATHS_INT[int_index];
                        
                        if (i == 0) {
                            if (operation == FM_MATHS_OPERATION_NONE) {
                                value = 0;
                            } else {
                                value = ch_group->ch[0]->value.float_value;
                            }
                        }
                        
                        float read_value;
                        if (read_service == 0) {
                            read_value = FM_MATHS_FLOAT[float_index];
                            
                        } else if (read_service > 0) {
                            read_value = get_hkch_value(ch_group_find_by_serv(read_service)->ch[(uint8_t) FM_MATHS_FLOAT[float_index]]);
                            
                        } else {
                            if (!main_config.clock_ready && read_service <=FM_MATHS_GET_TIME_IS_SAVING) {
                                get_value = false;
                                break;
                            }
                            
                            struct tm* timeinfo;
                            time_t time = raven_ntp_get_time_t();
                            timeinfo = localtime(&time);
                            
                            switch (read_service) {
                                case FM_MATHS_GET_TIME_HOUR:
                                    read_value = timeinfo->tm_hour;
                                    break;
                                    
                                case FM_MATHS_GET_TIME_MINUTE:
                                    read_value = timeinfo->tm_min;
                                    break;
                                    
                                case FM_MATHS_GET_TIME_SECOND:
                                    read_value = timeinfo->tm_sec;
                                    break;
                                    
                                case FM_MATHS_GET_TIME_DAYWEEK:
                                    read_value = timeinfo->tm_wday;
                                    break;
                                    
                                case FM_MATHS_GET_TIME_DAYMONTH:
                                    read_value = timeinfo->tm_mday;
                                    break;
                                    
                                case FM_MATHS_GET_TIME_MONTH:
                                    read_value = timeinfo->tm_mon;
                                    break;
                                    
                                case FM_MATHS_GET_TIME_YEAR:
                                    read_value = timeinfo->tm_year;
                                    break;
                                    
                                case FM_MATHS_GET_TIME_DAYYEAR:
                                    read_value = timeinfo->tm_yday;
                                    break;
                                    
                                case FM_MATHS_GET_TIME_IS_SAVING:
                                    read_value = timeinfo->tm_isdst;
                                    break;
                                    
                                default:    // case FM_MATHS_GEN_RANDOM_NUMBER:
                                    read_value = hwrand() % (((uint32_t) FM_MATHS_FLOAT[float_index]) + 1);
                                    break;
                            }
                        }
                        
                        switch (operation) {
                            case FM_MATHS_OPERATION_SUB:
                                value = value - read_value;
                                break;
                                
                            case FM_MATHS_OPERATION_SUB_INV:
                                value = read_value - value;
                                break;
                                
                            case FM_MATHS_OPERATION_MUL:
                                value = value * read_value;
                                break;
                                
                            case FM_MATHS_OPERATION_DIV:
                                value = value / read_value;
                                break;
                                
                            case FM_MATHS_OPERATION_DIV_INV:
                                value = read_value / value;
                                break;
                                
                            case FM_MATHS_OPERATION_MOD:
                                value = ((int) value) % ((int) read_value);
                                break;
                                
                            case FM_MATHS_OPERATION_MOD_INV:
                                value = ((int) read_value) % ((int) value);
                                break;
                                
                            case FM_MATHS_OPERATION_POW:
                                value = fast_precise_pow(value, read_value);
                                break;
                                
                            case FM_MATHS_OPERATION_POW_INV:
                                value = fast_precise_pow(read_value, value);
                                break;
                                
                            case FM_MATHS_OPERATION_INV:
                                value = 1 / value;
                                break;
                                
                            case FM_MATHS_OPERATION_ABS:
                                value = fabs(value);
                                break;
                                
                            default:    // case FM_MATHS_OPERATION_NONE:
                                        // case FM_MATHS_OPERATION_ADD:
                                value = value + read_value;
                                break;
                        }

                        int_index++;
                        float_index++;
                        
                        get_value = true;
                    }
                    
                } else if (fm_sensor_type == FM_SENSOR_TYPE_ADC ||
                         fm_sensor_type == FM_SENSOR_TYPE_ADC_INV) {
#ifdef ESP_PLATFORM
                    int adc = -1;
                    adc_channel_t adc_channel;
                    adc_oneshot_io_to_channel(FM_SENSOR_GPIO, NULL, &adc_channel);
                    adc_oneshot_read(main_config.adc_dac_data->adc_oneshot_handle, adc_channel, &adc);
                    value = adc;
#else
                    value = sdk_system_adc_read();
#endif
                    if (value >= 0) {
                        if (fm_sensor_type == FM_SENSOR_TYPE_ADC_INV) {
                            value = (HAA_ADC_MAX_VALUE - 1) - value;
                        }
                        get_value = true;
                    }
                    
                } else if (fm_sensor_type >= FM_SENSOR_TYPE_NETWORK &&
                           fm_sensor_type <= FM_SENSOR_TYPE_NETWORK_PATTERN_HEX) {
                    if (ch_group->action_network && main_config.wifi_status == WIFI_STATUS_CONNECTED) {
                        
                        action_network_t* action_network = ch_group->action_network;
                        
                        int errors = 0;
                        
                        while (action_network) {
                            if (action_network->action == 0) {
                                int socket;
                                int result = -1;
                                
                                if (xSemaphoreTake(main_config.network_busy_mutex, MS_TO_TICKS(2000)) == pdTRUE) {
                                    INFO("<%i> Connect %s:%i", ch_group->serv_index, action_network->host, action_network->port_n);
                                    
                                    uint8_t rcvtimeout_s = 1;
                                    int rcvtimeout_us = 0;
                                    if (action_network->wait_response > 0) {
                                        rcvtimeout_s = action_network->wait_response / 10;
                                        rcvtimeout_us = (action_network->wait_response % 10) * 100000;
                                    }
                                    
                                    if (action_network->method_n < 10) {
                                        char* req = NULL;
                                        
                                        if (action_network->method_n == 3) {        // TCP RAW
                                            req = action_network->content;
                                            
                                        } else if (action_network->method_n != 4) { // HTTP
                                            size_t content_len_n = 0;
                                            
                                            char* method = strdup("GET");
                                            char* method_req = NULL;
                                            if (action_network->method_n < 3 &&
                                                action_network->method_n > 0) {
                                                content_len_n = strlen(action_network->content);
                                                
                                                char content_len[4];
                                                itoa(content_len_n, content_len, 10);
                                                method_req = malloc(23);
                                                snprintf(method_req, 23, "%s%s\r\n",
                                                         http_header_len,
                                                         content_len);
                                                
                                                free(method);
                                                if (action_network->method_n == 1) {
                                                    method = strdup("PUT");
                                                } else {
                                                    method = strdup("POST");
                                                }
                                            }
                                            
                                            action_network->len = 69 + strlen(method) + ((method_req != NULL) ? strlen(method_req) : 0) + strlen(HAA_FIRMWARE_VERSION) + strlen(action_network->host) +  strlen(action_network->url) + strlen(action_network->header) + content_len_n;
                                            
                                            req = malloc(action_network->len);
                                            
                                            if (!req) {
                                                action_network->is_running = false;
                                                
                                                if (method_req) {
                                                    free(method_req);
                                                }
                                                
                                                free(method);
                                                
                                                homekit_remove_oldest_client();
                                                errors++;
                                                
                                                if (errors < 5) {
                                                    vTaskDelay(MS_TO_TICKS(200));
                                                    continue;
                                                } else {
                                                    break;
                                                }
                                            }
                                            
                                            snprintf(req, action_network->len, "%s /%s%s%s%s%s%s\r\n",
                                                     method,
                                                     action_network->url,
                                                     http_header1,
                                                     action_network->host,
                                                     http_header2,
                                                     action_network->header,
                                                     (method_req != NULL) ? method_req : "");
                                            
                                            free(method);
                                            
                                            if (method_req) {
                                                free(method_req);
                                            }
                                            
                                            if (action_network->method_n > 0) {
                                                strcat(req, action_network->content);
                                            }
                                        }
                                        
                                        result = new_net_con(action_network->host,
                                                             action_network->port_n,
                                                             false,
                                                             action_network->method_n == 4 ? action_network->raw : (uint8_t*) req,
                                                             action_network->len,
                                                             &socket,
                                                             rcvtimeout_s, rcvtimeout_us);
                                        
                                        if (result >= 0) {
                                            INFO_NNL("<%i> Payload", ch_group->serv_index);
                                            if (action_network->method_n == 4) {
                                                INFO(" RAW");
                                            } else {
                                                INFO(":\n%s", req);
                                            }
                                        } else {
                                            ERROR("<%i> TCP (%i)", ch_group->serv_index, result);
                                        }
                                        
                                        if (req && action_network->method_n != 3) {
                                            free(req);
                                        }
                                        
                                    } else {
                                        result = new_net_con(action_network->host,
                                                             action_network->port_n,
                                                             true,
                                                             action_network->method_n == 13 ? (uint8_t*) action_network->content : action_network->raw,
                                                             action_network->len,
                                                             &socket,
                                                             rcvtimeout_s, rcvtimeout_us);
                                        
                                        if (result > 0) {
                                            INFO_NNL("<%i> Payload", ch_group->serv_index);
                                            if (action_network->method_n == 13) {
                                                INFO(":\n%s", action_network->content);
                                            } else {
                                                INFO(" RAW");
                                            }
                                        } else {
                                            ERROR("<%i> UDP", ch_group->serv_index);
                                        }
                                    }
                                    
                                    size_t total_recv = 0;
                                    uint8_t* str = NULL;
                                    if (result > 0) {
                                        // Read response
                                        INFO("<%i> Response", ch_group->serv_index);
                                        int read_byte;
                                        uint8_t* recv_buffer = malloc(65);
                                        do {
                                            memset(recv_buffer, 0, 65);
                                            read_byte = read(socket, recv_buffer, 64);
                                            if (read_byte > 0) {
                                                uint8_t* new_str = realloc(str, total_recv + read_byte + 1);
                                                if (!new_str) {
                                                    break;
                                                }
                                                str = new_str;
                                                memcpy(str + total_recv, recv_buffer, read_byte);
                                                total_recv += read_byte;
                                            }
                                        } while (read_byte > 0 && total_recv < 2048);
                                        
                                        free(recv_buffer);
                                    }
                                    
                                    if (socket >= 0) {
                                        close(socket);
                                    }
                                    
                                    xSemaphoreGive(main_config.network_busy_mutex);
                                    
                                    if (total_recv > 0) {
                                        str[total_recv] = 0;
                                        
                                        if (fm_sensor_type == FM_SENSOR_TYPE_NETWORK || fm_sensor_type == FM_SENSOR_TYPE_NETWORK_PATTERN_TEXT) {
                                            INFO("%s", str);
                                        }
                                        
                                        uint8_t* found = str;
                                        if (action_network->method_n < 3 &&
                                            total_recv > 10 &&
                                            (fm_sensor_type == FM_SENSOR_TYPE_NETWORK ||
                                             fm_sensor_type == FM_SENSOR_TYPE_NETWORK_PATTERN_TEXT)) {
                                            found = (uint8_t*) strstr((char*) str, "\r\n\r\n");
                                            if (found) {
                                                found += 4;
                                            }
                                        }
                                        
                                        if (found < str + total_recv) {
                                            if (FM_BUFFER_LEN_MIN == 0 ||
                                                (total_recv >= (uint8_t) FM_BUFFER_LEN_MIN &&
                                                 total_recv <= (uint8_t) FM_BUFFER_LEN_MAX)) {
                                                int is_pattern_found = false;
                                                if (fm_sensor_type == FM_SENSOR_TYPE_NETWORK_PATTERN_TEXT) {
                                                    is_pattern_found = find_patterns(FM_PATTERN_CH_READ, &found, 0);
                                                } else if (fm_sensor_type == FM_SENSOR_TYPE_NETWORK_PATTERN_HEX) {
                                                    is_pattern_found = find_patterns(FM_PATTERN_CH_READ, &found, total_recv);
                                                }
                                                
                                                if (fm_sensor_type == FM_SENSOR_TYPE_NETWORK ||
                                                    (fm_sensor_type == FM_SENSOR_TYPE_NETWORK_PATTERN_TEXT &&
                                                     is_pattern_found)) {
                                                    
                                                    get_value = str_to_float((char*) found, (char*) str, &value);
                                                    
                                                } else if (fm_sensor_type == FM_SENSOR_TYPE_NETWORK_PATTERN_HEX &&
                                                           is_pattern_found) {
                                                    if (found + FM_VAL_LEN <= str + total_recv) {
                                                        value = byte_array_to_num(found, FM_VAL_LEN, FM_VAL_TYPE);
                                                        get_value = true;
                                                    }
                                                }
                                            }
                                        }
                                        
                                        free(str);
                                        
                                        INFO("<%i> -> %i", ch_group->serv_index, total_recv);
                                    }
                                }
                            }
                            
                            action_network = action_network->next;
                        }
                    }
                    
                } else if (fm_sensor_type == FM_SENSOR_TYPE_I2C ||
                           fm_sensor_type == FM_SENSOR_TYPE_I2C_TRIGGER) {
                    if (fm_sensor_type == FM_SENSOR_TYPE_I2C_TRIGGER) {
                        adv_i2c_slave_write(FM_I2C_BUS, (uint8_t) FM_I2C_ADDR,
                                        (uint8_t*) &FM_I2C_TRIGGER_REG[FM_I2C_TRIGGER_REG_FIRST],
                                        FM_I2C_TRIGGER_REG_LEN,
                                        (uint8_t*) &FM_I2C_TRIGGER_VAL[FM_I2C_TRIGGER_VAL_FIRST],
                                        FM_I2C_TRIGGER_VAL_LEN);
                        
                        if (FM_NEW_VALUE > 0) {
                            vTaskDelay(FM_NEW_VALUE);
                        }
                    }
                    
                    size_t i2c_value_len = FM_VAL_LEN + FM_I2C_VAL_OFFSET;
                    uint8_t i2c_value[i2c_value_len];
                    
                    if (adv_i2c_slave_read(FM_I2C_BUS, (uint8_t) FM_I2C_ADDR, (uint8_t*) &FM_I2C_REG[FM_I2C_REG_FIRST], FM_I2C_REG_LEN, i2c_value, i2c_value_len) == 0) {
                        value = byte_array_to_num(&i2c_value[FM_I2C_VAL_OFFSET], FM_VAL_LEN, FM_VAL_TYPE);
                        get_value = true;
                    }
                }
            } else if (fm_sensor_type >= FM_SENSOR_TYPE_UART) {
                uart_receiver_data_t* uart_receiver_data = main_config.uart_receiver_data;
                while (uart_receiver_data && uart_receiver_data->uart_port != FM_UART_PORT) {
                    uart_receiver_data = uart_receiver_data->next;
                }
                
                if (uart_receiver_data && (FM_BUFFER_LEN_MIN == 0 ||
                    (uart_receiver_data->uart_buffer_len >= (uint8_t) FM_BUFFER_LEN_MIN &&
                     uart_receiver_data->uart_buffer_len <= (uint8_t) FM_BUFFER_LEN_MAX))) {
                    uint8_t* found = uart_receiver_data->uart_buffer;
                    int is_pattern_found = false;
                    if (fm_sensor_type == FM_SENSOR_TYPE_UART_PATTERN_HEX) {
                        is_pattern_found = find_patterns(FM_PATTERN_CH_READ, &found, uart_receiver_data->uart_buffer_len);
                        
                    } else if (fm_sensor_type == FM_SENSOR_TYPE_UART_PATTERN_TEXT) {
                        found[uart_receiver_data->uart_buffer_len] = 0;
                        INFO("<%i> UART Text: %s", ch_group->serv_index, (char*) found);
                        is_pattern_found = find_patterns(FM_PATTERN_CH_READ, &found, 0);
                    }
                    
                    if (fm_sensor_type == FM_SENSOR_TYPE_UART ||
                        (fm_sensor_type == FM_SENSOR_TYPE_UART_PATTERN_HEX &&
                         is_pattern_found)) {
                        
                        if (found + FM_VAL_LEN <= uart_receiver_data->uart_buffer + uart_receiver_data->uart_buffer_len) {
                            value = byte_array_to_num(found, FM_VAL_LEN, FM_VAL_TYPE);
                            get_value = true;
                        }
                        
                    } else if (fm_sensor_type == FM_SENSOR_TYPE_UART_PATTERN_TEXT &&
                               is_pattern_found) {
                        
                        get_value = str_to_float((char*) found, (char*) uart_receiver_data->uart_buffer, &value);
                    }
                }
            }
            
            if (get_value) {
                value = (FM_FACTOR * value) + FM_OFFSET;
                
                if (ch_group->serv_type == SERV_TYPE_FREE_MONITOR_ACCUMULATVE) {
                    if ((int) value == -2182017) {
                        value = 0;
                    } else {
                        value += ch_group->ch[0]->value.float_value;
                    }
                }
                
                INFO("<%i> FM: %g", ch_group->serv_index, value);
                
                if (FM_SENSOR_TYPE > 0 ||
                    (FM_SENSOR_TYPE < 0 &&
                    value >= FM_LIMIT_LOWER &&
                    value <= FM_LIMIT_UPPER)) {
                    
                    do_wildcard_actions(ch_group, 0, value);
                    
                    const int old_value = ch_group->ch[0]->value.float_value * 1000;
                    ch_group->ch[0]->value.float_value = value;
                    
                    if (old_value != ((int) (value * 1000))) {
                        homekit_characteristic_notify_safe(ch_group->ch[0]);
                    }
                    
                    // Target Characteristic
                    if (ch_group->chs == 3 ||
                        (ch_group->chs == 2 && !free_monitor_type_is_pattern(fm_sensor_type))) {
                        homekit_characteristic_t* ch_target = ch_group->ch[ch_group->chs - 1];
                        set_hkch_value(ch_target, value);
                    }
                }
            }
        }
        
        if (args) {
            break;
        }
        
        ch_group = ch_group->next;
    }
    
    if (!args) {
        reset_uart_buffer();
    }
    
    ch_group->is_working = false;
    
    vTaskDelete(NULL);
}

void free_monitor_timer_worker(TimerHandle_t xTimer) {
    if (!homekit_is_pairing()) {
        ch_group_t* ch_group = (ch_group_t*) pvTimerGetTimerID(xTimer);
        if (ch_group->main_enabled) {
            if (!ch_group->is_working) {
                if (xTaskCreate(free_monitor_task, "FM", FREE_MONITOR_TASK_SIZE, (void*) ch_group, FREE_MONITOR_TASK_PRIORITY, NULL) == pdPASS) {
                    ch_group->is_working = true;
                } else {
                    ERROR("New FM");
                    homekit_remove_oldest_client();
                }
            }
        }
    } else {
        ERROR("HK pair");
    }
}

void recv_uart_task() {
    int call_free_monitor = false;
    
    uart_receiver_data_t* uart_receiver_data = main_config.uart_receiver_data;
    while (uart_receiver_data) {
        if (uart_receiver_data->uart_has_data) {
            uart_receiver_data->uart_buffer = malloc(uart_receiver_data->uart_max_len + 1);
            
#ifdef ESP_PLATFORM
            uart_receiver_data->uart_buffer_len = uart_read_bytes(uart_receiver_data->uart_port, uart_receiver_data->uart_buffer, uart_receiver_data->uart_max_len + 1, 0);
            uart_flush(uart_receiver_data->uart_port);
#else
            int ch;
            int count = 0;
            
            for (;;) {
                if ((ch = uart_getc_nowait(0)) >= 0) {
                    count = 0;
                    uart_receiver_data->uart_buffer[uart_receiver_data->uart_buffer_len] = ch;
                    uart_receiver_data->uart_buffer_len++;
                    
                    if (uart_receiver_data->uart_buffer_len == uart_receiver_data->uart_max_len) {
                        break;
                    }
                    
                } else {
                    count++;
                    
                    if (count < 20) {
                        sdk_os_delay_us(1000);
                    } else {
                        break;
                    }
                }
            }
#endif
        }
        
        if (uart_receiver_data->uart_buffer_len >= uart_receiver_data->uart_min_len) {
            call_free_monitor = true;
        }
        
        uart_receiver_data = uart_receiver_data->next;
    }
    
    if (call_free_monitor) {
        if (xTaskCreate(free_monitor_task, "FM", FREE_MONITOR_TASK_SIZE, NULL, FREE_MONITOR_TASK_PRIORITY, NULL) != pdPASS) {
            reset_uart_buffer();
            ERROR("New FM");
            homekit_remove_oldest_client();
        }
    } else {
        reset_uart_buffer();
    }
    
    vTaskDelete(NULL);
}

void recv_uart_timer_worker(TimerHandle_t xTimer) {
    int uart_has_data = false;
    uart_receiver_data_t* uart_receiver_data = main_config.uart_receiver_data;
    while (uart_receiver_data) {
        size_t len = 0;
        
#ifdef ESP_PLATFORM
        uart_get_buffered_data_len(uart_receiver_data->uart_port, &len);
#else
        len = uart_rxfifo_wait(0, 0);
#endif
        
        if (len > 0 &&
            uart_receiver_data->uart_buffer_len == 0 &&
            !homekit_is_pairing()) {
            uart_receiver_data->uart_has_data = true;
            uart_has_data = true;
        }
        
        uart_receiver_data = uart_receiver_data->next;
    }
    
    if (uart_has_data & !main_config.uart_recv_is_working) {
        if (xTaskCreate(recv_uart_task, "RUA", RECV_UART_TASK_SIZE, NULL, RECV_UART_TASK_PRIORITY, NULL) == pdTRUE) {
            main_config.uart_recv_is_working = true;
        }
    }
}

// --- BINARY INPUTS
void window_cover_diginput(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    INFO("<%i> WC DigI GPIO %i", ch_group->serv_index, gpio);

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
                    INFO("<%i> WC DigI Stop ignored", ch_group->serv_index);
                }
                break;
        }
    }
}

void diginput(const uint16_t gpio, void* args, const uint8_t type) {
    ch_group_t* ch_group = args;
    
    INFO("<%i> DigI %i GPIO %i", ch_group->serv_index, type, gpio);

    if (ch_group->child_enabled) {
        switch (ch_group->serv_type) {
            case SERV_TYPE_LOCK:
                if (type == 2) {
                    hkc_lock_setter(ch_group->ch[1], HOMEKIT_UINT8(!ch_group->ch[1]->value.int_value));
                } else {
                    hkc_lock_setter(ch_group->ch[1], HOMEKIT_UINT8(type));
                }
                break;
                
            case SERV_TYPE_WATER_VALVE:
                if (ch_group->ch[0]->value.int_value == 1) {
                    hkc_valve_setter(ch_group->ch[0], HOMEKIT_UINT8(0));
                } else {
                    hkc_valve_setter(ch_group->ch[0], HOMEKIT_UINT8(1));
                }
                break;
                
            case SERV_TYPE_LIGHTBULB:
                if (type == 2) {
                    if (ch_group->ch[1]->value.int_value == 0) {
                        ch_group->ch[1]->value.int_value = 100;
                    }
                    autodimmer_call(ch_group->ch[0], HOMEKIT_BOOL(!ch_group->ch[0]->value.bool_value));
                } else {
                    autodimmer_call(ch_group->ch[0], HOMEKIT_BOOL(type));
                }
                break;
                
            case SERV_TYPE_GARAGE_DOOR:
                if (type == 2) {
                    hkc_garage_door_setter(GD_TARGET_DOOR_STATE, HOMEKIT_UINT8(!GD_TARGET_DOOR_STATE_INT));
                } else {
                    hkc_garage_door_setter(GD_TARGET_DOOR_STATE, HOMEKIT_UINT8(type));
                }
                break;
                
            case SERV_TYPE_WINDOW_COVER:
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
                
            case SERV_TYPE_FAN:
                if (type == 2) {
                    hkc_fan_setter(ch_group->ch[0], HOMEKIT_BOOL(!ch_group->ch[0]->value.bool_value));
                } else {
                    hkc_fan_setter(ch_group->ch[0], HOMEKIT_BOOL(type));
                }
                break;
                
            case SERV_TYPE_TV:
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
    
    INFO("<%i> DigI S%i GPIO %i", ch_group->serv_index, type, gpio);

    if (ch_group->child_enabled) {
        switch (ch_group->serv_type) {
            case SERV_TYPE_LOCK:
                hkc_lock_status_setter(ch_group->ch[1], HOMEKIT_UINT8(type));
                break;
                
            case SERV_TYPE_WATER_VALVE:
                hkc_valve_status_setter(ch_group->ch[0], HOMEKIT_UINT8(type));
                break;
                
            case SERV_TYPE_FAN:
                hkc_fan_status_setter(ch_group->ch[0], HOMEKIT_BOOL(type));
                break;
                
            case SERV_TYPE_TV:
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
    INFO("<%i> AutoOff", ch_group->serv_index);
    
    switch (ch_group->serv_type) {
        case SERV_TYPE_LOCK:
            hkc_lock_setter(ch_group->ch[1], HOMEKIT_UINT8(1));
            break;
            
        case SERV_TYPE_CONTACT_SENSOR:
        case SERV_TYPE_MOTION_SENSOR:
            binary_sensor(99, ch_group, 0);
            break;
            
        case SERV_TYPE_WATER_VALVE:
            hkc_valve_setter(ch_group->ch[0], HOMEKIT_UINT8(0));
            break;
            
        case SERV_TYPE_LIGHTBULB:
            hkc_rgbw_setter(ch_group->ch[0], HOMEKIT_BOOL(false));
            break;
            
        case SERV_TYPE_GARAGE_DOOR:
            hkc_garage_door_setter(GD_TARGET_DOOR_STATE, HOMEKIT_UINT8(1));
            break;
            
        case SERV_TYPE_FAN:
            hkc_fan_setter(ch_group->ch[0], HOMEKIT_BOOL(false));
            break;
            
        default:    // SERV_TYPE_SWITCH  SERV_TYPE_OUTLET
            hkc_on_setter(ch_group->ch[0], HOMEKIT_BOOL(false));
            break;
    }
}

// --- Network Action task
void net_action_task(void* pvParameters) {
    action_task_t* action_task = pvParameters;
    action_network_t* action_network = action_task->ch_group->action_network;
    
    int socket;
    
    size_t search_str_ch_values(str_ch_value_t** str_ch_value_ini, char* content) {
        size_t len = strlen(content);
        
        char* content_search = content;
        str_ch_value_t* str_ch_value_last = NULL;
        
        do {
            content_search = strstr(content_search, NETWORK_ACTION_WILDCARD_VALUE);
            if (content_search) {
                char buffer[15];
                buffer[2] = 0;
                
                buffer[0] = content_search[5];
                buffer[1] = content_search[6];
                
                int acc_number = strtol(buffer, NULL, 10);
                
                ch_group_t* ch_group_found = action_task->ch_group;
                
                if (acc_number > 0) {
                    ch_group_found = ch_group_find_by_serv(acc_number);
                }
                
                buffer[0] = content_search[7];
                buffer[1] = content_search[8];
                
                homekit_value_t* value = &ch_group_found->ch[(int) strtol(buffer, NULL, 10)]->value;
                
                switch (value->format) {
                    case HOMEKIT_FORMAT_BOOL:
                        snprintf(buffer, 15, "%s", value->bool_value ? "true" : "false");
                        break;
                        
                    case HOMEKIT_FORMAT_UINT8:
                    case HOMEKIT_FORMAT_UINT16:
                    case HOMEKIT_FORMAT_UINT32:
                    case HOMEKIT_FORMAT_UINT64:
                    case HOMEKIT_FORMAT_INT:
                        snprintf(buffer, 15, "%i", value->int_value);
                        break;
                        
                    case HOMEKIT_FORMAT_FLOAT:
                        snprintf(buffer, 15, "%1.7g", value->float_value);
                        break;
                        
                    default:
                        buffer[0] = 0;
                        break;
                }
                
                len += strlen(buffer) - 9;
                
                str_ch_value_t* str_ch_value = malloc(sizeof(str_ch_value_t));
                memset(str_ch_value, 0, sizeof(*str_ch_value));
                
                str_ch_value->string = strdup(buffer);
                str_ch_value->next = NULL;
                INFO("Wildcard val: %s", str_ch_value->string);
                
                if (*str_ch_value_ini == NULL) {
                    *str_ch_value_ini = str_ch_value;
                    str_ch_value_last = str_ch_value;
                } else {
                    str_ch_value_last->next = str_ch_value;
                    str_ch_value_last = str_ch_value;
                }

                content_search += 9;
            }
            
        } while (content_search);
        
        return len;
    }
    
    void write_str_ch_values(str_ch_value_t** str_ch_value_ini, char** new_req, char* content) {
        if (*str_ch_value_ini) {
            str_ch_value_t* str_ch_value = *str_ch_value_ini;
            char* content_search = content;
            char* last_pos = content;
            
            do {
                content_search = strstr(last_pos, NETWORK_ACTION_WILDCARD_VALUE);
                
                if (content_search - last_pos > 0) {
                    strncat(*new_req, last_pos, content_search - last_pos);
                }
                
                strcat(*new_req, str_ch_value->string);
                
                free(str_ch_value->string);

                str_ch_value_t* str_ch_value_old = str_ch_value;
                str_ch_value = str_ch_value->next;
                
                free(str_ch_value_old);

                last_pos = content_search + 9;
                
            } while (str_ch_value);
            
            strcat(*new_req, last_pos);
            
        } else {
            strcat(*new_req, content);
        }
    }
    
    while (action_network) {
        if (action_network->action == action_task->action && !action_network->is_running) {
            action_network->is_running = true;
            
            if (xSemaphoreTake(main_config.network_busy_mutex, MS_TO_TICKS(2000)) == pdTRUE) {
                INFO("<%i> Net %s:%i", action_task->ch_group->serv_index, action_network->host, action_network->port_n);
                
                str_ch_value_t* str_ch_value_first = NULL;
                
                if (action_network->method_n < 10) {
                    size_t content_len_n = 0;
                    
                    char* req = NULL;
                    
                    if (action_network->method_n < 3) { // HTTP
                        char* method = strdup("GET");
                        char* method_req = NULL;
                        if (action_network->method_n > 0) {
                            content_len_n = search_str_ch_values(&str_ch_value_first, action_network->content);
                            
                            char content_len[4];
                            itoa(content_len_n, content_len, 10);
                            method_req = malloc(23);
                            snprintf(method_req, 23, "%s%s\r\n",
                                     http_header_len,
                                     content_len);
                            
                            free(method);
                            if (action_network->method_n == 1) {
                                method = strdup("PUT");
                            } else {
                                method = strdup("POST");
                            }
                        }
                        
                        action_network->len = 61 + strlen(method) + ((method_req != NULL) ? strlen(method_req) : 0) + strlen(HAA_FIRMWARE_VERSION) + strlen(action_network->host) +  strlen(action_network->url) + strlen(action_network->header) + content_len_n;
                        
                        req = malloc(action_network->len);
                        
                        snprintf(req, action_network->len, "%s /%s%s%s%s%s%s\r\n",
                                 method,
                                 action_network->url,
                                 http_header1,
                                 action_network->host,
                                 http_header2,
                                 action_network->header,
                                 (method_req != NULL) ? method_req : "");
                        
                        if (method_req) {
                            free(method_req);
                        }
                        
                        free(method);
                    }
                    
                    if (action_network->method_n > 0 &&
                        action_network->method_n < 4) {
                        if (action_network->method_n == 3) {
                            action_network->len = content_len_n;
                            req = malloc(content_len_n + 1);
                            req[0] = 0;
                        }
                        
                        write_str_ch_values(&str_ch_value_first, &req, action_network->content);
                    }
                    
                    uint8_t rcvtimeout_s = 1;
                    int rcvtimeout_us = 0;
                    if (action_network->wait_response > 0) {
                        rcvtimeout_s = action_network->wait_response / 10;
                        rcvtimeout_us = (action_network->wait_response % 10) * 100000;
                    }
                    
                    int result = new_net_con(action_network->host,
                                             action_network->port_n,
                                             false,
                                             action_network->method_n == 4 ? action_network->raw : (uint8_t*) req,
                                             action_network->len,
                                             &socket,
                                             rcvtimeout_s, rcvtimeout_us);
                    
                    if (result >= 0) {
                        INFO_NNL("<%i> Payload", action_task->ch_group->serv_index);
                        if (action_network->method_n == 4) {
                            INFO(" RAW");
                        } else {
                            INFO(":\n%s", req);
                        }
                        
                        if (action_network->wait_response > 0) {
                            INFO("<%i> Response:", action_task->ch_group->serv_index);
                            int read_byte;
                            size_t total_recv = 0;
                            uint8_t* recv_buffer = malloc(65);
                            do {
                                memset(recv_buffer, 0, 65);
                                read_byte = read(socket, recv_buffer, 64);
                                INFO_NNL("%s", recv_buffer);
                                total_recv += read_byte;
                            } while (read_byte > 0 && total_recv < 2048);
                            
                            free(recv_buffer);
                            INFO("-> %i", total_recv);
                        }
                        
                    } else {
                        ERROR("<%i> TCP (%i)", action_task->ch_group->serv_index, result);
                    }
                    
                    if (socket >= 0) {
                        close(socket);
                    }
                    
                    if (req) {
                        free(req);
                    }
                    
                } else {
                    uint8_t* wol = NULL;
                    if (action_network->method_n == 12) {
                        wol = malloc(WOL_PACKET_LEN);
                        for (int i = 0; i < 6; i++) {
                            wol[i] = 255;
                        }
                        
                        for (int i = 6; i < 102; i += 6) {
                            for (int j = 0; j < 6; j++) {
                                wol[i + j] = action_network->raw[j];
                            }
                        }
                    }
                    
                    int result = -1;
                    
                    if (action_network->method_n == 13) {
                        size_t content_len_n = search_str_ch_values(&str_ch_value_first, action_network->content);
                        char* req = malloc(content_len_n + 1);
                        req[0] = 0;
                        write_str_ch_values(&str_ch_value_first, &req, action_network->content);
                        
                        result = new_net_con(action_network->host,
                                             action_network->port_n,
                                             true,
                                             (uint8_t*) req,
                                             content_len_n,
                                             &socket,
                                             1, 0);
                        
                        if (socket >= 0) {
                            close(socket);
                            
                            if (result > 0) {
                                INFO("<%i> Payload\n%s", action_task->ch_group->serv_index, req);
                            }
                        }
                        
                        free(req);
                        
                    } else {
                        int max_attemps = 1;
                        if (wol) {
                            max_attemps = 5;
                        }
                        
                        for (int attemp = 0; attemp < max_attemps; attemp++) {
                            result = new_net_con(action_network->host,
                                                 action_network->port_n,
                                                 true,
                                                 wol ? wol : action_network->raw,
                                                 wol ? WOL_PACKET_LEN : action_network->len,
                                                 &socket,
                                                 1, 0);
                            
                            if (socket >= 0) {
                                close(socket);
                            }
                            
                            if (attemp < (max_attemps - 1)) {
                                vTaskDelay(MS_TO_TICKS(20));
                            }
                        }
                    }
                    
                    if (wol) {
                        free(wol);
                    }
                    
                    if (result > 0) {
                        if (action_network->method_n != 13) {
                            INFO("<%i> Payload RAW", action_task->ch_group->serv_index);
                        }
                    } else {
                        ERROR("<%i> UDP", action_task->ch_group->serv_index);
                    }
                }
                
                xSemaphoreGive(main_config.network_busy_mutex);
            }
            
            action_network->is_running = false;
            
            INFO("<%i> Net done", action_task->ch_group->serv_index);
            
            vTaskDelay(1);
        }
        
        action_network = action_network->next;
    }

    free(pvParameters);
    vTaskDelete(NULL);
}

// --- IR/RF Send task
void irrf_tx_task(void* pvParameters) {
    action_task_t* action_task = pvParameters;
    action_irrf_tx_t* action_irrf_tx = action_task->ch_group->action_irrf_tx;
    
    int errors = 0;
    
    while (action_irrf_tx) {
        if (action_irrf_tx->action == action_task->action) {
            uint16_t* ir_code = NULL;
            size_t ir_code_len = 0;
            
            int freq = main_config.ir_tx_freq;
            if (action_irrf_tx->freq > 1) {
                freq = action_irrf_tx->freq;
            }
            
            // Decoding protocol based IR code
            if (action_irrf_tx->prot_code) {
                char* prot = NULL;
                
                if (action_irrf_tx->prot) {
                    prot = action_irrf_tx->prot;
                } else if (action_task->ch_group->ir_protocol) {
                    prot = action_task->ch_group->ir_protocol;
                } else {
                    prot = ch_group_find_by_serv(SERV_TYPE_ROOT_DEVICE)->ir_protocol;
                }
                
                // Decoding protocol based IR code length
                const size_t ir_action_protocol_len = strlen(prot);
                const size_t json_ir_code_len = strlen(action_irrf_tx->prot_code);
                ir_code_len = 3;
                
                INFO_NNL("<%i> IR Protocol bits: ", action_task->ch_group->serv_index);
                
                switch (ir_action_protocol_len) {
                    case IRRF_ACTION_PROTOCOL_LEN_4BITS:
                        for (unsigned int i = 0; i < json_ir_code_len; i++) {
                            char* found = strchr(baseUC_dic, action_irrf_tx->prot_code[i]);
                            if (found) {
                                if (found - baseUC_dic < 13) {
                                    ir_code_len += (1 + found - baseUC_dic) << 1;
                                } else {
                                    ir_code_len += (1 - 13 + found - baseUC_dic) << 1;
                                }
                            } else {
                                found = strchr(baseLC_dic, action_irrf_tx->prot_code[i]);
                                if (found - baseLC_dic < 13) {
                                    ir_code_len += (1 + found - baseLC_dic) << 1;
                                } else {
                                    ir_code_len += (1 - 13 + found - baseLC_dic) << 1;
                                }
                            }
                        }
                        break;
                        
                    case IRRF_ACTION_PROTOCOL_LEN_6BITS:
                        for (unsigned int i = 0; i < json_ir_code_len; i++) {
                            char* found = strchr(baseUC_dic, action_irrf_tx->prot_code[i]);
                            if (found) {
                                if (found - baseUC_dic < 9) {
                                    ir_code_len += (1 + found - baseUC_dic) << 1;
                                } else if (found - baseUC_dic < 18) {
                                    ir_code_len += (1 - 9 + found - baseUC_dic) << 1;
                                } else {
                                    ir_code_len += (1 - 18 + found - baseUC_dic) << 1;
                                }
                            } else {
                                found = strchr(baseLC_dic, action_irrf_tx->prot_code[i]);
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
                        
                    default:    // case IRRF_ACTION_PROTOCOL_LEN_2BITS:
                        for (unsigned int i = 0; i < json_ir_code_len; i++) {
                            char* found = strchr(baseUC_dic, action_irrf_tx->prot_code[i]);
                            if (found) {
                                ir_code_len += (1 + found - baseUC_dic) << 1;
                            } else {
                                found = strchr(baseLC_dic, action_irrf_tx->prot_code[i]);
                                ir_code_len += (1 + found - baseLC_dic) << 1;
                            }
                        }
                        break;
                }
                
                ir_code = malloc(sizeof(uint16_t) * ir_code_len);
                if (!ir_code) {
                    homekit_remove_oldest_client();
                    errors++;
                    
                    if (errors < ACTION_TASK_MAX_ERRORS) {
                        vTaskDelay(MS_TO_TICKS(110));
                        continue;
                    } else {
                        break;
                    }
                }
                
                INFO("<%i> IR Len %i, Prot %s", action_task->ch_group->serv_index, ir_code_len, prot);
                
                unsigned int bit0_mark = 0, bit0_space = 0, bit1_mark = 0, bit1_space = 0;
                unsigned int bit2_mark = 0, bit2_space = 0, bit3_mark = 0, bit3_space = 0;
                unsigned int bit4_mark = 0, bit4_space = 0, bit5_mark = 0, bit5_space = 0;
                unsigned int packet, index;
                
                for (unsigned int i = 0; i < (ir_action_protocol_len >> 1); i++) {
                    index = i << 1;     // i * 2
                    char* found = strchr(baseRaw_dic, prot[index]);
                    packet = (found - baseRaw_dic) * IRRF_CODE_LEN * IRRF_CODE_SCALE;
                    
                    found = strchr(baseRaw_dic, prot[index + 1]);
                    packet += (found - baseRaw_dic) * IRRF_CODE_SCALE;
                    
#ifdef ESP_PLATFORM
                    INFO_NNL("%s%5i ", i & 1 ? "-" : "+", packet);
#else
                    INFO_NNL("%s%5d ", i & 1 ? "-" : "+", packet);
#endif
                    
                    switch (i) {
                        case IRRF_CODE_HEADER_MARK_POS:
                            ir_code[0] = packet;
                            break;
                            
                        case IRRF_CODE_HEADER_SPACE_POS:
                            ir_code[1] = packet;
                            break;
                            
                        case IRRF_CODE_BIT0_MARK_POS:
                            bit0_mark = packet;
                            break;
                            
                        case IRRF_CODE_BIT0_SPACE_POS:
                            bit0_space = packet;
                            break;
                            
                        case IRRF_CODE_BIT1_MARK_POS:
                            bit1_mark = packet;
                            break;
                            
                        case IRRF_CODE_BIT1_SPACE_POS:
                            bit1_space = packet;
                            break;
                            
                        case IRRF_CODE_BIT2_MARK_POS:
                            if (ir_action_protocol_len == IRRF_ACTION_PROTOCOL_LEN_2BITS) {
                                ir_code[ir_code_len - 1] = packet;
                            } else {
                                bit2_mark = packet;
                            }
                            break;
                                
                        case IRRF_CODE_BIT2_SPACE_POS:
                            bit2_space = packet;
                            break;
                                
                        case IRRF_CODE_BIT3_MARK_POS:
                            bit3_mark = packet;
                            break;
                                
                        case IRRF_CODE_BIT3_SPACE_POS:
                            bit3_space = packet;
                            break;
                            
                        case IRRF_CODE_BIT4_MARK_POS:
                            if (ir_action_protocol_len == IRRF_ACTION_PROTOCOL_LEN_4BITS) {
                                ir_code[ir_code_len - 1] = packet;
                            } else {
                                bit4_mark = packet;
                            }
                            break;
                                    
                        case IRRF_CODE_BIT4_SPACE_POS:
                            bit4_space = packet;
                            break;
                                
                        case IRRF_CODE_BIT5_MARK_POS:
                            bit5_mark = packet;
                            break;
                                
                        case IRRF_CODE_BIT5_SPACE_POS:
                            bit5_space = packet;
                            break;
                            
                        case IRRF_CODE_FOOTER_MARK_POS_6BITS:
                            ir_code[ir_code_len - 1] = packet;
                            break;
                            
                        default:
                            // Do nothing
                            break;
                    }
                }
                
                // Decoding BIT code part
                unsigned int ir_code_index = 2;
                
                void fill_code(const unsigned int count, const unsigned int bit_mark, const unsigned int bit_space) {
                    for (unsigned int j = 0; j < count; j++) {
                        ir_code[ir_code_index] = bit_mark;
                        ir_code_index++;
                        ir_code[ir_code_index] = bit_space;
                        ir_code_index++;
                    }
                }
                
                for (unsigned int i = 0; i < json_ir_code_len; i++) {
                    char* found = strchr(baseUC_dic, action_irrf_tx->prot_code[i]);
                    if (found) {
                        switch (ir_action_protocol_len) {
                            case IRRF_ACTION_PROTOCOL_LEN_4BITS:
                                if (found - baseUC_dic < 13) {
                                    fill_code(1 + found - baseUC_dic, bit1_mark, bit1_space);
                                } else {
                                    fill_code(found - baseUC_dic - 12, bit3_mark, bit3_space);
                                }
                                break;
                                
                            case IRRF_ACTION_PROTOCOL_LEN_6BITS:
                                if (found - baseUC_dic < 9) {
                                    fill_code(1 + found - baseUC_dic, bit1_mark, bit1_space);
                                } else if (found - baseUC_dic < 18) {
                                    fill_code(found - baseUC_dic - 8, bit3_mark, bit3_space);
                                } else {
                                    fill_code(found - baseUC_dic - 17, bit5_mark, bit5_space);
                                }
                                break;
                                
                            default:    // case IRRF_ACTION_PROTOCOL_LEN_2BITS:
                                fill_code(1 + found - baseUC_dic, bit1_mark, bit1_space);
                                break;
                        }
                        
                    } else {
                        found = strchr(baseLC_dic, action_irrf_tx->prot_code[i]);
                        switch (ir_action_protocol_len) {
                            case IRRF_ACTION_PROTOCOL_LEN_4BITS:
                                if (found - baseLC_dic < 13) {
                                    fill_code(1 + found - baseLC_dic, bit0_mark, bit0_space);
                                } else {
                                    fill_code(found - baseLC_dic - 12, bit2_mark, bit2_space);
                                }
                                break;
                                
                            case IRRF_ACTION_PROTOCOL_LEN_6BITS:
                                if (found - baseLC_dic < 9) {
                                    fill_code(1 + found - baseLC_dic, bit0_mark, bit0_space);
                                } else if (found - baseLC_dic < 18) {
                                    fill_code(found - baseLC_dic - 8, bit2_mark, bit2_space);
                                } else {
                                    fill_code(found - baseLC_dic - 17, bit4_mark, bit4_space);
                                }
                                break;
                                
                            default:    // case IRRF_ACTION_PROTOCOL_LEN_2BITS:
                                fill_code(1 + found - baseLC_dic, bit0_mark, bit0_space);
                                break;
                        }
                    }
                }
                
                INFO("\n<%i> IR code %s", action_task->ch_group->serv_index, action_irrf_tx->prot_code);
                for (unsigned int i = 0; i < ir_code_len; i++) {
#ifdef ESP_PLATFORM
                    INFO_NNL("%s%5i ", i & 1 ? "-" : "+", ir_code[i]);
#else
                    INFO_NNL("%s%5d ", i & 1 ? "-" : "+", ir_code[i]);
#endif
                    if (i % 16 == 15) {
                        INFO_NNL("\n");
                    }

                }
                INFO_NNL("\n");
                
            } else {    // IRRF_ACTION_RAW_CODE
                const size_t json_ir_code_len = strlen(action_irrf_tx->raw_code);
                ir_code_len = json_ir_code_len >> 1;
                
                ir_code = malloc(sizeof(uint16_t) * ir_code_len);
                
                INFO("<%i> IR packet (%i)", action_task->ch_group->serv_index, ir_code_len);

                unsigned int index, packet;
                for (unsigned int i = 0; i < ir_code_len; i++) {
                    index = i << 1;
                    char* found = strchr(baseRaw_dic, action_irrf_tx->raw_code[index]);
                    packet = (found - baseRaw_dic) * IRRF_CODE_LEN * IRRF_CODE_SCALE;
                    
                    found = strchr(baseRaw_dic, action_irrf_tx->raw_code[index + 1]);
                    packet += (found - baseRaw_dic) * IRRF_CODE_SCALE;

                    ir_code[i] = packet;
#ifdef ESP_PLATFORM
                    INFO_NNL("%s%5i ", i & 1 ? "-" : "+", packet);
#else
                    INFO_NNL("%s%5d ", i & 1 ? "-" : "+", packet);
#endif
                    if (i % 16 == 15) {
                        INFO_NNL("\n");
                    }
                }
                
                INFO_NNL("\n");
            }
            
            // IR TRANSMITTER
            uint32_t start;
            int ir_true, ir_false, ir_gpio;
            if (freq > 1) {
                ir_true = !main_config.ir_tx_inv;
                ir_false = main_config.ir_tx_inv;
                ir_gpio = main_config.ir_tx_gpio;
            } else {
                ir_true = !main_config.rf_tx_inv;
                ir_false = main_config.rf_tx_inv;
                ir_gpio = main_config.rf_tx_gpio;
            }

            for (int r = 0; r < action_irrf_tx->repeats; r++) {
                
                HAA_ENTER_CRITICAL_TASK();
                
                for (unsigned int i = 0; i < ir_code_len; i++) {
                    if (ir_code[i] > 0) {
                        if (i & 1) {    // Space
                            gpio_write(ir_gpio, ir_false);
                            sdk_os_delay_us(ir_code[i]);
                        } else {        // Mark
                            start = sdk_system_get_time_raw();
                            if (freq > 1) {
                                while ((sdk_system_get_time_raw() - start) < ir_code[i]) {
                                    gpio_write(ir_gpio, ir_true);
                                    sdk_os_delay_us(freq);
                                    gpio_write(ir_gpio, ir_false);
                                    sdk_os_delay_us(freq);
                                }
                            } else {
                                gpio_write(ir_gpio, ir_true);
                                sdk_os_delay_us(ir_code[i]);
                            }
                        }
                    }
                }
                
                gpio_write(ir_gpio, ir_false);

                HAA_EXIT_CRITICAL_TASK();
                
                INFO("<%i> IR %i sent", action_task->ch_group->serv_index, r);
                
                vTaskDelay(action_irrf_tx->pause);
            }
            
            if (ir_code) {
                free(ir_code);
            }
        }
        
        action_irrf_tx = action_irrf_tx->next;
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
            INFO_NNL("<%i> UART%i -> ", action_task->ch_group->serv_index, action_uart->uart);
            for (int i = 0; i < action_uart->len; i++) {
                INFO_NNL("%02x", action_uart->command[i]);
                INFO_NNL("%02x", action_uart->command[i]);
            }
            INFO("");
            
#ifdef ESP_PLATFORM
            uart_write_bytes(action_uart->uart, action_uart->command, action_uart->len);
#else
            HAA_ENTER_CRITICAL_TASK();
            
            for (int i = 0; i < action_uart->len; i++) {
                uart_putc(action_uart->uart, action_uart->command[i]);
            }
            
            uart_flush_txfifo(action_uart->uart);
            
            HAA_EXIT_CRITICAL_TASK();
#endif
        }
        
        vTaskDelay(action_uart->pause);
        
        action_uart = action_uart->next;
    }
    
    free(pvParameters);
    vTaskDelete(NULL);
}

// --- ACTIONS
void autoswitch_timer(TimerHandle_t xTimer) {
    action_binary_output_t* action_binary_output = (action_binary_output_t*) pvTimerGetTimerID(xTimer);
    
    extended_gpio_write(action_binary_output->gpio, !action_binary_output->value);
    INFO("Auto DigO %i->%i", action_binary_output->gpio, !action_binary_output->value);
    
    rs_esp_timer_delete(xTimer);
}

void action_task_timer(TimerHandle_t xTimer) {
    action_task_t* action_task = (action_task_t*) pvTimerGetTimerID(xTimer);
    
    if ((action_task->type == ACTION_TASK_TYPE_UART &&
         xTaskCreate(uart_action_task, "UA", UART_ACTION_TASK_SIZE, action_task, UART_ACTION_TASK_PRIORITY, NULL) != pdPASS) ||
        (action_task->type == ACTION_TASK_TYPE_IRRF &&
            xTaskCreate(irrf_tx_task, "IR", IRRF_TX_TASK_SIZE, action_task, IRRF_TX_TASK_PRIORITY, NULL) != pdPASS) ||
        (action_task->type == ACTION_TASK_TYPE_NETWORK &&
            xTaskCreate(net_action_task, "NET", NETWORK_ACTION_TASK_SIZE, action_task, NETWORK_ACTION_TASK_PRIORITY, NULL) != pdPASS)) {
        action_task->errors++;
        homekit_remove_oldest_client();
        
        ERROR("<%i> AT %i", action_task->ch_group->serv_index, action_task->type);
        
        if (action_task->errors < ACTION_TASK_MAX_ERRORS) {
            rs_esp_timer_change_period(xTimer, 110);
        } else {
            free(action_task);
            rs_esp_timer_delete(xTimer);
        }
    } else {
        rs_esp_timer_delete(xTimer);
    }
}

void do_actions(ch_group_t* ch_group, uint8_t action) {
    INFO("<%i> Run A%i", ch_group->serv_index, action);
    
    // Copy actions
    action_copy_t* action_copy = ch_group->action_copy;
    while (action_copy) {
        if (action_copy->action == action) {
            action = action_copy->new_action;
            action_copy = NULL;
        } else {
            action_copy = action_copy->next;
        }
    }
    
    // Binary outputs
    action_binary_output_t* action_binary_output = ch_group->action_binary_output;
    while (action_binary_output) {
        if (action_binary_output->action == action) {
            extended_gpio_write(action_binary_output->gpio, action_binary_output->value);
#ifdef ESP_PLATFORM
            INFO("<%i> DigO %i->%i (%li)", ch_group->serv_index, action_binary_output->gpio, action_binary_output->value, action_binary_output->inching);
#else
            INFO("<%i> DigO %i->%i (%i)", ch_group->serv_index, action_binary_output->gpio, action_binary_output->value, action_binary_output->inching);
#endif
            
            if (action_binary_output->inching > 0) {
                rs_esp_timer_start(rs_esp_timer_create(action_binary_output->inching, false, (void*) action_binary_output, autoswitch_timer));
            }
        }
        
        action_binary_output = action_binary_output->next;
    }
    
    // Service Notification Manager
    action_serv_manager_t* action_serv_manager = ch_group->action_serv_manager;
    while (action_serv_manager) {
        if (action_serv_manager->action == action) {
            ch_group_t* ch_group = ch_group_find_by_serv(action_serv_manager->serv_index);
            if (ch_group) {
                INFO("<%i> ServNot %i->%g", ch_group->serv_index, action_serv_manager->serv_index, action_serv_manager->value);
                
                int value_int = action_serv_manager->value;
                
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
                    
                    switch (ch_group->serv_type) {
                        case SERV_TYPE_BUTTON:
                        case SERV_TYPE_DOORBELL:
                            button_event(0, ch_group, value_int);
                            break;
                            
                        case SERV_TYPE_LOCK:
                            if (value_int == -1) {
                                if (ch_group->ch[0]->value.int_value == 0) {
                                    rs_esp_timer_start(AUTOOFF_TIMER);
                                }
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
                            
                        case SERV_TYPE_CONTACT_SENSOR:
                        case SERV_TYPE_MOTION_SENSOR:
                            if (value_int == -1) {
                                if ((ch_group->serv_type == SERV_TYPE_CONTACT_SENSOR && ch_group->ch[0]->value.int_value == 1) ||
                                    (ch_group->serv_type == SERV_TYPE_MOTION_SENSOR && ch_group->ch[0]->value.bool_value == true)) {
                                    rs_esp_timer_start(AUTOOFF_TIMER);
                                }
                            } else {
                                binary_sensor(99, ch_group, value_int);
                            }
                            break;
                            
                        case SERV_TYPE_AIR_QUALITY:
                            if (value_int >= 10000) {
                                const int charact = value_int / 10000;
                                const int new_value = value_int % 10000;
                                if (((int) ch_group->ch[charact]->value.float_value) != new_value) {
                                    ch_group->ch[charact]->value.float_value = new_value;
                                    homekit_characteristic_notify_safe(ch_group->ch[charact]);
                                }
                                
                            } else if (ch_group->main_enabled) {
                                if (ch_group->ch[0]->value.int_value != value_int &&
                                    value_int >= 0 && value_int <= 5) {
                                    ch_group->ch[0]->value.int_value = value_int;
                                    do_actions(ch_group, value_int);
                                    homekit_characteristic_notify_safe(ch_group->ch[0]);
                                }
                            }
                            break;
                            
                        case SERV_TYPE_WATER_VALVE:
                            if (value_int < 0) {
                                if (value_int == -1) {
                                    if (ch_group->ch[0]->value.int_value == 1) {
                                        rs_esp_timer_start(AUTOOFF_TIMER);
                                    }
                                }
                                
                                if (ch_group->chs > 2) {
                                    if (value_int < -1) {
                                        hkc_setter(ch_group->ch[2], HOMEKIT_UINT32(- value_int - 2));
                                    }
                                    if (value_int == -1 || ch_group->ch[3]->value.int_value > ch_group->ch[2]->value.int_value) {
                                        ch_group->ch[3]->value.int_value = ch_group->ch[2]->value.int_value;
                                    }
                            
                                }
                            } else {
                                hkc_valve_setter(ch_group->ch[0], HOMEKIT_UINT8(value_int));
                            }
                            break;
                            
                        case SERV_TYPE_THERMOSTAT:
                            value_int = action_serv_manager->value * 100.f;
                            if (value_int == 2) {
                                hkc_th_setter(ch_group->ch[2], HOMEKIT_UINT8(0));
                            } else if (value_int == 3) {
                                hkc_th_setter(ch_group->ch[2], HOMEKIT_UINT8(1));
                            } else if (value_int == 4) {
                                hkc_th_setter(ch_group->ch[4], HOMEKIT_UINT8(2));
                            } else if (value_int == 5) {
                                hkc_th_setter(ch_group->ch[4], HOMEKIT_UINT8(1));
                            } else if (value_int == 6) {
                                hkc_th_setter(ch_group->ch[4], HOMEKIT_UINT8(0));
                            } else {
                                if (value_int % 2 == 0) {
                                    hkc_th_setter(ch_group->ch[5], HOMEKIT_FLOAT(action_serv_manager->value));
                                } else {
                                    hkc_th_setter(ch_group->ch[6], HOMEKIT_FLOAT(action_serv_manager->value - 0.01f));
                                }
                            }
                            break;
                            
                        case SERV_TYPE_HUMIDIFIER:
                            if (value_int < 0) {
                                hkc_humidif_setter(ch_group->ch[4], HOMEKIT_UINT8(value_int + 3));
                            } else if (value_int <= 1) {
                                hkc_humidif_setter(ch_group->ch[2], HOMEKIT_UINT8(value_int));
                            } else if (value_int <= 1100) {
                                hkc_humidif_setter(ch_group->ch[5], HOMEKIT_FLOAT(value_int - 1000));
                            } else {    // if (value_int <= 2100)
                                hkc_humidif_setter(ch_group->ch[6], HOMEKIT_FLOAT(value_int - 2000));
                            }
                            break;
                            
                        case SERV_TYPE_GARAGE_DOOR:
                            if (value_int == -1) {
                                if (GD_CURRENT_DOOR_STATE_INT == GARAGE_DOOR_OPENED) {
                                    rs_esp_timer_start(AUTOOFF_TIMER);
                                }
                            } else if (value_int < 2) {
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
                            
                        case SERV_TYPE_LIGHTBULB:
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
                                    } else {    // action_serv_manager->value == -2
                                        lightbulb_group->autodimmer = 0;
                                    }
                                }
                            } else if (value_int == 200) {
                                hkc_rgbw_setter(ch_group->ch[0], HOMEKIT_BOOL(!ch_group->ch[0]->value.bool_value));
                            } else {
                                hkc_rgbw_setter(ch_group->ch[0], HOMEKIT_BOOL((bool) value_int));
                            }
                            break;
                            
                        case SERV_TYPE_WINDOW_COVER:
                            if (value_int < 0) {
                                window_cover_obstruction(99, ch_group, value_int + 2);
                                
                            } else if (value_int == 101) {
                                hkc_window_cover_setter(WINDOW_COVER_CH_TARGET_POSITION, WINDOW_COVER_CH_CURRENT_POSITION->value);
                                
                            } else if (value_int >= 200) {
                                WINDOW_COVER_HOMEKIT_POSITION = value_int - 200;
                                
                                WINDOW_COVER_CH_CURRENT_POSITION->value.int_value = WINDOW_COVER_HOMEKIT_POSITION;
                                WINDOW_COVER_MOTOR_POSITION = ((double) ((100.00000000f * WINDOW_COVER_CORRECTION * WINDOW_COVER_HOMEKIT_POSITION) + (5000.00000000f * WINDOW_COVER_HOMEKIT_POSITION))) / ((double) ((WINDOW_COVER_CORRECTION * WINDOW_COVER_HOMEKIT_POSITION) + 5000.00000000f));
                                
                                if (WINDOW_COVER_CH_STATE->value.int_value == WINDOW_COVER_STOP) {
                                    WINDOW_COVER_CH_TARGET_POSITION->value.int_value = WINDOW_COVER_CH_CURRENT_POSITION->value.int_value;
                                    homekit_characteristic_notify_safe(WINDOW_COVER_CH_TARGET_POSITION);
                                }
                                
                                homekit_characteristic_notify_safe(WINDOW_COVER_CH_CURRENT_POSITION);
                            } else {
                                hkc_window_cover_setter(WINDOW_COVER_CH_TARGET_POSITION, HOMEKIT_UINT8(value_int));
                            }
                            break;
                            
                        case SERV_TYPE_FAN:
                            if (value_int == 0) {
                                hkc_fan_setter(ch_group->ch[0], HOMEKIT_BOOL(false));
                            } else if (value_int == -201) {
                                if (ch_group->ch[0]->value.int_value == true) {
                                    rs_esp_timer_start(AUTOOFF_TIMER);
                                }
                            } else if (value_int < -100) {
                                const float new_value = ch_group->ch[1]->value.float_value + action_serv_manager->value + 100;
                                if (new_value < *ch_group->ch[1]->min_value) {
                                    hkc_fan_setter(ch_group->ch[1], HOMEKIT_FLOAT(*ch_group->ch[1]->min_value));
                                } else {
                                    hkc_fan_setter(ch_group->ch[1], HOMEKIT_FLOAT(new_value));
                                }
                            } else if (value_int < 0) {
                                const float new_value = ch_group->ch[1]->value.float_value - action_serv_manager->value;
                                if (new_value > *ch_group->ch[1]->max_value) {
                                    hkc_fan_setter(ch_group->ch[1], HOMEKIT_FLOAT(*ch_group->ch[1]->max_value));
                                } else {
                                    hkc_fan_setter(ch_group->ch[1], HOMEKIT_FLOAT(new_value));
                                }
                            } else if (value_int > 100) {
                                hkc_fan_setter(ch_group->ch[0], HOMEKIT_BOOL(true));
                            } else {
                                hkc_fan_setter(ch_group->ch[1], HOMEKIT_FLOAT(action_serv_manager->value));
                            }
                            break;
                            
                        case SERV_TYPE_SECURITY_SYSTEM:
                            if (value_int >= 14) {
                                alarm_recurrent = true;
                                value_int -= 10;
                            }
                            
                            if (value_int == 8) {
                                SEC_SYSTEM_CH_CURRENT_STATE->value.int_value = SEC_SYSTEM_CH_TARGET_STATE->value.int_value;
                                do_actions(ch_group, 8);
                                homekit_characteristic_notify_safe(SEC_SYSTEM_CH_CURRENT_STATE);
                                save_data_history(SEC_SYSTEM_CH_CURRENT_STATE);
                                
                            } else if ((value_int == 4 && SEC_SYSTEM_CH_TARGET_STATE->value.int_value != SEC_SYSTEM_OFF) ||
                                       SEC_SYSTEM_CH_TARGET_STATE->value.int_value == value_int - 5) {
                                SEC_SYSTEM_CH_CURRENT_STATE->value.int_value = 4;
                                do_actions(ch_group, value_int);
                                homekit_characteristic_notify_safe(SEC_SYSTEM_CH_CURRENT_STATE);
                                save_data_history(SEC_SYSTEM_CH_CURRENT_STATE);
                                if (alarm_recurrent) {
                                    INFO("<%i> Rec alarm", ch_group->serv_index);
                                    rs_esp_timer_start(SEC_SYSTEM_REC_ALARM_TIMER);
                                }
                                
                            } else if (value_int >= 10) {
                                hkc_sec_system_status(SEC_SYSTEM_CH_TARGET_STATE, HOMEKIT_UINT8(value_int - 10));
                                
                            } else if (value_int <= 3) {
                                hkc_sec_system(SEC_SYSTEM_CH_TARGET_STATE, HOMEKIT_UINT8(value_int));
                            }
                            break;
                            
                        case SERV_TYPE_TV:
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
                            
                        case SERV_TYPE_POWER_MONITOR:
                            //if (value_int == 0) {
                                pm_custom_consumption_reset(ch_group);
                            //}
                            break;
                            
                        case SERV_TYPE_FREE_MONITOR:
                        case SERV_TYPE_FREE_MONITOR_ACCUMULATVE:
                            if (ch_group->main_enabled) {
                                FM_OVERRIDE_VALUE = action_serv_manager->value;
                                if (xTaskCreate(free_monitor_task, "FM", FREE_MONITOR_TASK_SIZE, (void*) ch_group, FREE_MONITOR_TASK_PRIORITY, NULL) != pdPASS) {
                                    ERROR("New FM");
                                    homekit_remove_oldest_client();
                                }
                            }
                            break;
                            
                        case SERV_TYPE_BATTERY:
                            battery_manager(BATTERY_LEVEL_CH, value_int, false);
                            break;
                            
                        case SERV_TYPE_DATA_HISTORY:
                            //if (value_int == 0) {
                                save_data_history(ch_group->ch[ch_group->chs - 1]);
                            //}
                            break;
                            
                        default:    // ON Type ch
                            if (value_int < 0) {
                                if (value_int == -1) {
                                    if (ch_group->ch[0]->value.bool_value == true) {
                                        rs_esp_timer_start(AUTOOFF_TIMER);
                                    }
                                }
                                
                                if (ch_group->chs > 1) {
                                    if (value_int < -1) {
                                        hkc_setter(ch_group->ch[1], HOMEKIT_UINT32(- value_int - 2));
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
                ERROR("Target");
            }
        }

        action_serv_manager = action_serv_manager->next;
    }
    
    // System actions
    action_system_t* action_system = ch_group->action_system;
    while (action_system) {
        if (action_system->action == action) {
            INFO("<%i> Sys %i", ch_group->serv_index, action_system->value);
            switch (action_system->value) {
                case SYSTEM_ACTION_SETUP_MODE:
                    setup_mode_call(99, NULL, 0);
                    break;
                    
                case SYSTEM_ACTION_OTA_UPDATE:
#ifdef ESP_PLATFORM
                    setup_set_boot_installer();
#else
                    rboot_set_temp_rom(1);
#endif
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
    
    // PWM actions
    action_pwm_t* action_pwm = ch_group->action_pwm;
    while (action_pwm) {
        if (action_pwm->action == action) {
            INFO("<%i> PWM %i->%i, f %i, d %i", ch_group->serv_index, action_pwm->gpio, action_pwm->duty, action_pwm->freq, action_pwm->dithering);
            
#ifdef ESP_PLATFORM
            pwmh_channel_t* pwmh_channel = get_pwmh_channel(action_pwm->gpio);
            if ((!pwmh_channel && action_pwm->gpio >= 0) ||
                (pwmh_channel && action_pwm->dithering)) {
#else
            if (action_pwm->gpio >= 0) {
#endif
                adv_pwm_set_dithering(action_pwm->gpio, action_pwm->dithering);
                haa_pwm_set_duty(action_pwm->gpio, action_pwm->duty);
            }
            
            if (action_pwm->freq > 0) {
#ifdef ESP_PLATFORM
                if (pwmh_channel) {
                    ledc_set_freq(LEDC_LOW_SPEED_MODE, pwmh_channel->timer, action_pwm->freq);
                } else {
                    adv_pwm_set_freq(action_pwm->freq);
                }
#else
                adv_pwm_set_freq(action_pwm->freq);
#endif
            }
        }
        
        action_pwm = action_pwm->next;
    }
    
    // Set Characteristic actions
    action_set_ch_t* action_set_ch = ch_group->action_set_ch;
    while (action_set_ch) {
        if (action_set_ch->action == action) {
            INFO("<%i> SetCh %i.%i->%i.%i", ch_group->serv_index, action_set_ch->source_serv, action_set_ch->source_ch, action_set_ch->target_serv, action_set_ch->target_ch);
            const float value = get_hkch_value(ch_group_find_by_serv(action_set_ch->source_serv)->ch[action_set_ch->source_ch]);
            set_hkch_value(ch_group_find_by_serv(action_set_ch->target_serv)->ch[action_set_ch->target_ch], value);
        }
        
        action_set_ch = action_set_ch->next;
    }
    
    int timer_delay = 0;
    // UART actions
    if (ch_group->action_uart) {
        action_task_t* action_task = new_action_task();
        action_task->action = action;
        action_task->type = ACTION_TASK_TYPE_UART;
        action_task->ch_group = ch_group;
        
        rs_esp_timer_start(rs_esp_timer_create(portTICK_PERIOD_MS, false, action_task, action_task_timer));
        timer_delay += portTICK_PERIOD_MS;
    }
    
    // Network actions
    if (ch_group->action_network && main_config.wifi_status == WIFI_STATUS_CONNECTED) {
        action_task_t* action_task = new_action_task();
        action_task->action = action;
        action_task->type = ACTION_TASK_TYPE_NETWORK;
        action_task->ch_group = ch_group;
        
        rs_esp_timer_start(rs_esp_timer_create(portTICK_PERIOD_MS + timer_delay, false, action_task, action_task_timer));
        timer_delay += portTICK_PERIOD_MS;
    }
    
    // IRRF TX actions
    if (ch_group->action_irrf_tx) {
        action_task_t* action_task = new_action_task();
        action_task->action = action;
        action_task->type = ACTION_TASK_TYPE_IRRF;
        action_task->ch_group = ch_group;
        
        rs_esp_timer_start(rs_esp_timer_create(portTICK_PERIOD_MS + timer_delay, false, action_task, action_task_timer));
    }
}

void do_wildcard_actions(ch_group_t* ch_group, uint8_t index, const float action_value) {
    float last_value;
    float last_diff = 10000000.f;
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
            INFO("<%i> Wild %i->%.2f", ch_group->serv_index, index, last_value);
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
            rs_esp_timer_change_period(xTimer, 60000);
            main_config.timetable_ready = true;
        } else {
            return;
        }
    }
    
    ch_group_t* ch_group = ch_group_find_by_serv(SERV_TYPE_ROOT_DEVICE);
    
    if (ch_group->main_enabled) {
        timetable_action_t* timetable_action = main_config.timetable_actions;
        while (timetable_action) {
            if (
                (timetable_action->mon  == ALL_MONS  || timetable_action->mon  == timeinfo->tm_mon ) &&
                (timetable_action->mday == ALL_MDAYS || timetable_action->mday == timeinfo->tm_mday) &&
                (timetable_action->hour == ALL_HOURS || timetable_action->hour == timeinfo->tm_hour) &&
                (timetable_action->min  == ALL_MINS  || timetable_action->min  == timeinfo->tm_min ) &&
                (timetable_action->wday == ALL_WDAYS || timetable_action->wday == timeinfo->tm_wday)
                ) {
                    
                    do_actions(ch_group, timetable_action->action);
                }
            
            timetable_action = timetable_action->next;
        }
    }
    
    if (timeinfo->tm_hour == 23 &&
        timeinfo->tm_min  == 59 &&
        timeinfo->tm_sec  != 0) {
        rs_esp_timer_change_period(xTimer, 1000);
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
    ch_group_t* ch_group = main_config.ch_groups;
    while (ch_group) {
        if (ch_group->timer &&
            ch_group->serv_type >= SERV_TYPE_THERMOSTAT &&
            ch_group->serv_type <= SERV_TYPE_HUMIDIFIER_WITH_TEMP) {
            vTaskDelay(MS_TO_TICKS(3500));
            
            INFO("<%i> Start TH", ch_group->serv_index);
            
            temperature_timer_worker(ch_group->timer);
            rs_esp_timer_start_forced(ch_group->timer);
        }
        
        ch_group = ch_group->next;
    }
    
    ch_group = main_config.ch_groups;
    while (ch_group) {
        if (ch_group->serv_type == SERV_TYPE_IAIRZONING) {
            vTaskDelay(MS_TO_TICKS(9500));
            
            INFO("<%i> Start iAZ", ch_group->serv_index);
            
            temperature_timer_worker(ch_group->timer);
            rs_esp_timer_start_forced(ch_group->timer);
        }
        
        ch_group = ch_group->next;
    }
    
    vTaskDelete(NULL);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, NULL);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER, "José A. Jiménez Campos");
homekit_characteristic_t model = HOMEKIT_CHARACTERISTIC_(MODEL, "RavenSystem HAA "HAA_FIRMWARE_CODENAME);
homekit_characteristic_t identify_function = HOMEKIT_CHARACTERISTIC_(IDENTIFY, identify);
homekit_characteristic_t firmware = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, HAA_FIRMWARE_VERSION);
homekit_characteristic_t hap_version = HOMEKIT_CHARACTERISTIC_(VERSION, "1.1.0");
homekit_characteristic_t haa_custom_setup_option = HOMEKIT_CHARACTERISTIC_(CUSTOM_SETUP_OPTION, "", .setter_ex=hkc_custom_setup_setter);

homekit_server_config_t config;

void run_homekit_server() {
#ifdef ESP_PLATFORM
    uint8_t channel_primary = 0;
    esp_wifi_get_channel(&channel_primary, NULL);
    main_config.wifi_channel = channel_primary;
#else
    main_config.wifi_channel = sdk_wifi_get_channel();
#endif
    
    main_config.wifi_status = WIFI_STATUS_CONNECTED;
    main_config.wifi_ip = wifi_config_get_ip();
    
    show_freeheap();
    
    if (main_config.enable_homekit_server) {
        random_task_delay();
        homekit_server_init(&config);
        show_freeheap();
    }
    
    rs_esp_timer_start_forced(rs_esp_timer_create(NTP_POLL_PERIOD_MS, true, NULL, ntp_timer_worker));
    
    if (main_config.timetable_actions) {
        rs_esp_timer_start_forced(rs_esp_timer_create(1000, true, NULL, timetable_actions_timer_worker));
    }
    
    vTaskDelay(MS_TO_TICKS(500));
    
    ntp_timer_worker(NULL);
    
    if (homekit_pairing_count() == 0) {
        sysparam_erase(HOMEKIT_PAIRING_COUNT_SYSPARAM);
    }
    
    do_actions(ch_group_find_by_serv(SERV_TYPE_ROOT_DEVICE), 2);
    
    led_blink(4);
    
    WIFI_WATCHDOG_TIMER = rs_esp_timer_create(WIFI_WATCHDOG_POLL_PERIOD_MS, true, NULL, wifi_watchdog);
    rs_esp_timer_start_forced(WIFI_WATCHDOG_TIMER);
    
    if (main_config.ping_inputs) {
        rs_esp_timer_start_forced(rs_esp_timer_create(main_config.ping_poll_period * 1000.00f, true, NULL, ping_task_timer_worker));
    }
}

void printf_header() {
    INFO("\nHome Accessory Architect "HAA_FIRMWARE_VERSION""HAA_FIRMWARE_BETA_REVISION"\n(c) 2018-2023 José A. Jiménez Campos\n");
    
#ifdef HAA_DEBUG
    INFO("HAA DEBUG ENABLED\n");
#endif  // HAA_DEBUG
}

void reset_uart() {
#ifdef ESP_PLATFORM
    //uart_driver_delete(0);
    
    uart_config_t uart_config_data = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    uart_driver_install(0, SDK_UART_BUFFER_SIZE, 0, 0, NULL, 0);
    uart_param_config(0, &uart_config_data);
    gpio_reset_pin(1);
    uart_set_pin(0, 1, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
#else
    //set_used_gpio(1);
    //gpio_disable(1);
    iomux_set_pullup_flags(5, 0);
    iomux_set_function(5, IOMUX_GPIO1_FUNC_UART0_TXD);
    uart_set_baud(0, 115200);
#endif
}

void homekit_re_pair_timer(TimerHandle_t xTimer) {
    if (!homekit_is_paired()) {
        reboot_haa();
    }
    
    rs_esp_timer_delete(xTimer);
}

void normal_mode_init() {
    main_config.network_busy_mutex = xSemaphoreCreateMutex();
    
    unistring_t* unistrings = NULL;
    
    char* txt_config = NULL;
    sysparam_get_string(HAA_SCRIPT_SYSPARAM, &txt_config);
    
    cJSON* json_haa = cJSON_Parse(txt_config);
    
    cJSON* json_config = cJSON_GetObjectItemCaseSensitive(json_haa, GENERAL_CONFIG);
    cJSON* json_accessories = cJSON_GetObjectItemCaseSensitive(json_haa, ACCESSORIES_ARRAY);
    
    const unsigned int total_accessories = cJSON_GetArraySize(json_accessories);
    
    if (total_accessories == 0) {
        sysparam_set_int32(TOTAL_SERV_SYSPARAM, 0);
        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 2);
        
        vTaskDelay(MS_TO_TICKS(200));
        sdk_system_restart();
    }
    
    // Binary Inputs GPIO Setup function
    bool diginput_register(cJSON* json_buttons, void* callback, ch_group_t* ch_group, const uint8_t param) {
        int active = false;
        
        for (int j = 0; j < cJSON_GetArraySize(json_buttons); j++) {
            const int gpio = (uint16_t) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), PIN_GPIO)->valuefloat;
            
            int button_type = 1;
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), BUTTON_PRESS_TYPE) != NULL) {
                button_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_buttons, j), BUTTON_PRESS_TYPE)->valuefloat;
            }
            
            adv_button_register_callback_fn(gpio, callback, button_type, (void*) ch_group, param);
            
            if (adv_button_read_by_gpio(gpio) == button_type) {
                active = true;
            }
            
            INFO("New DigI GPIO %i t %i, s %i", gpio, button_type, active);
        }
        
        return active;
    }
    
    // Ping Setup function
    void ping_register(cJSON* json_pings, void* callback, ch_group_t* ch_group, const uint8_t param) {
        for (int j = 0; j < cJSON_GetArraySize(json_pings); j++) {
            ping_input_t* ping_input = ping_input_find_by_host(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_pings, j), PING_HOST)->valuestring);
            
            if (!ping_input) {
                ping_input = malloc(sizeof(ping_input_t));
                memset(ping_input, 0, sizeof(*ping_input));
                
                ping_input->host = uni_strdup(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_pings, j), PING_HOST)->valuestring, &unistrings);
                
                ping_input->next = main_config.ping_inputs;
                main_config.ping_inputs = ping_input;
            }
            
            int response_type = true;
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_pings, j), PING_RESPONSE_TYPE) != NULL) {
                response_type = (bool) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_pings, j), PING_RESPONSE_TYPE)->valuefloat;
            }
            
            ping_input->ignore_last_response = false;
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_pings, j), PING_IGNORE_LAST_RESPONSE) != NULL) {
                ping_input->ignore_last_response = (bool) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_pings, j), PING_IGNORE_LAST_RESPONSE)->valuefloat;
            }
            
            ping_input_callback_fn_t* ping_input_callback_fn;
            ping_input_callback_fn = malloc(sizeof(ping_input_callback_fn_t));
            memset(ping_input_callback_fn, 0, sizeof(*ping_input_callback_fn));
            
            if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_pings, j), PING_DISABLE_WITHOUT_WIFI) != NULL) {
                ping_input_callback_fn->disable_without_wifi = (bool) cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json_pings, j), PING_DISABLE_WITHOUT_WIFI)->valuefloat;
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
            
            INFO("New PingI h %s, r %i", ping_input->host, response_type);
        }
    }
    
    // Initial state function
    float set_initial_state_data(const uint8_t service, const uint8_t ch_number, cJSON* json_context, homekit_characteristic_t* ch, const uint8_t ch_type, const float default_value, char** string_pointer) {
        float state = default_value;
        INFO_NNL("<%i> Init Ch%i ", service, ch_number);
        if (cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE) != NULL) {
            const unsigned int initial_state = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE)->valuefloat;
            if (initial_state < INIT_STATE_LAST) {
                state = initial_state;
                
            } else {
                INFO_NNL("saved ");
                char saved_state_id[8];
                unsigned int int_saved_state_id = (service * 100) + ch_number;
                itoa(int_saved_state_id, saved_state_id, 10);
                
                last_state_t* last_state = malloc(sizeof(last_state_t));
                memset(last_state, 0, sizeof(*last_state));
                last_state->ch_type = ch_type;
                last_state->ch_state_id = int_saved_state_id;
                last_state->ch = ch;
                last_state->next = main_config.last_states;
                main_config.last_states = last_state;
                
                sysparam_status_t status;
                bool saved_state_bool = false;
                int8_t saved_state_int8;
                int32_t saved_state_int;
                
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
                        status = sysparam_get_string(saved_state_id, string_pointer);
                        
                        if (status == SYSPARAM_OK) {
                            state = 1;
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
                    INFO_NNL("none ");
                }
            }
        }
        
        if (ch_type == CH_TYPE_STRING && state > 0) {
            INFO("\"%s\"", *string_pointer);
        } else {
            INFO("%g", state);
        }
        
        return state;
    }
    
    float set_initial_state(const uint8_t service, const uint8_t ch_number, cJSON* json_context, homekit_characteristic_t* ch, const uint8_t ch_type, const float default_value) {
        return set_initial_state_data(service, ch_number, json_context, ch, ch_type, default_value, NULL);
    }
    
    // REGISTER ACTIONS
    // Copy actions
    inline void new_action_copy(ch_group_t* ch_group, cJSON* json_context, uint8_t fixed_action) {
        action_copy_t* last_action = ch_group->action_copy;
        
        void register_action(cJSON* json_accessory, uint8_t new_int_action) {
            char action[4];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                cJSON* json_action = cJSON_GetObjectItemCaseSensitive(json_accessory, action);
                if (cJSON_GetObjectItemCaseSensitive(json_action, COPY_ACTIONS) != NULL) {
                    action_copy_t* action_copy = malloc(sizeof(action_copy_t));
                    memset(action_copy, 0, sizeof(*action_copy));
                    
                    action_copy->action = new_int_action;
                    action_copy->new_action = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_action, COPY_ACTIONS)->valuefloat;
                    
                    action_copy->next = last_action;
                    last_action = action_copy;
                    
                    INFO("New A%i Copy v %i", new_int_action, action_copy->new_action);
                }
            }
        }
        
        if (fixed_action < MAX_ACTIONS) {
            for (int int_action = 0; int_action < MAX_ACTIONS; int_action++) {
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
            char action[4];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), BINARY_OUTPUTS_ARRAY) != NULL) {
                    cJSON* json_relays = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), BINARY_OUTPUTS_ARRAY);
                    for (int i = cJSON_GetArraySize(json_relays) - 1; i >= 0; i--) {
                        action_binary_output_t* action_binary_output = malloc(sizeof(action_binary_output_t));
                        memset(action_binary_output, 0, sizeof(*action_binary_output));
                        
                        cJSON* json_relay = cJSON_GetArrayItem(json_relays, i);
                        
                        action_binary_output->action = new_int_action;
                        action_binary_output->gpio = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_relay, PIN_GPIO)->valuefloat;
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_relay, VALUE) != NULL) {
                            action_binary_output->value = (bool) cJSON_GetObjectItemCaseSensitive(json_relay, VALUE)->valuefloat;
                        }
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_relay, AUTOSWITCH_TIME) != NULL) {
                            action_binary_output->inching = cJSON_GetObjectItemCaseSensitive(json_relay, AUTOSWITCH_TIME)->valuefloat * 1000;
                        }
                        
                        action_binary_output->next = last_action;
                        last_action = action_binary_output;
                        
#ifdef ESP_PLATFORM
                        INFO("New A%i DigO g %i, v %i, i %li", new_int_action, action_binary_output->gpio, action_binary_output->value, action_binary_output->inching);
#else
                        INFO("New A%i DigO g %i, v %i, i %i", new_int_action, action_binary_output->gpio, action_binary_output->value, action_binary_output->inching);
#endif
                    }
                }
            }
        }
        
        if (fixed_action < MAX_ACTIONS) {
            for (int int_action = 0; int_action < MAX_ACTIONS; int_action++) {
                register_action(json_context, int_action);
            }
        } else {
            register_action(json_context, fixed_action);
        }
        
        ch_group->action_binary_output = last_action;
    }
    
    // Service Manager
    inline void new_action_serv_manager(ch_group_t* ch_group, cJSON* json_context, uint8_t fixed_action) {
        action_serv_manager_t* last_action = ch_group->action_serv_manager;
        
        void register_action(cJSON* json_accessory, uint8_t new_int_action) {
            char action[4];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), SERVICE_MANAGER_ACTIONS_ARRAY) != NULL) {
                    cJSON* json_acc_managers = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), SERVICE_MANAGER_ACTIONS_ARRAY);
                    
                    for (int i = cJSON_GetArraySize(json_acc_managers) - 1; i >= 0; i--) {
                        action_serv_manager_t* action_serv_manager = malloc(sizeof(action_serv_manager_t));
                        memset(action_serv_manager, 0, sizeof(*action_serv_manager));

                        action_serv_manager->action = new_int_action;
                        action_serv_manager->value = 0;
                        
                        action_serv_manager->next = last_action;
                        last_action = action_serv_manager;
                        
                        cJSON* json_acc_manager = cJSON_GetArrayItem(json_acc_managers, i);
                        for (int j = 0; j < cJSON_GetArraySize(json_acc_manager); j++) {
                            const float value = (float) cJSON_GetArrayItem(json_acc_manager, j)->valuefloat;
                            
                            switch (j) {
                                case 0:
                                    action_serv_manager->serv_index = get_absolut_index(ch_group->serv_index, value);
                                    break;
                                    
                                case 1:
                                    action_serv_manager->value = value;
                                    break;
                            }
                        }
                        
                        INFO("New A%i ServNot %i->%g", new_int_action, action_serv_manager->serv_index, action_serv_manager->value);
                    }
                }
            }
        }
        
        if (fixed_action < MAX_ACTIONS) {
            for (int int_action = 0; int_action < MAX_ACTIONS; int_action++) {
                register_action(json_context, int_action);
            }
        } else {
            register_action(json_context, fixed_action);
        }
        
        ch_group->action_serv_manager = last_action;
    }
    
    // Service Manager
    inline void new_action_set_ch(ch_group_t* ch_group, cJSON* json_context, uint8_t fixed_action) {
        action_set_ch_t* last_action = ch_group->action_set_ch;
        
        void register_action(cJSON* json_accessory, uint8_t new_int_action) {
            char action[4];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), SET_CH_ACTIONS_ARRAY) != NULL) {
                    cJSON* json_set_chs = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), SET_CH_ACTIONS_ARRAY);
                    
                    for (int i = cJSON_GetArraySize(json_set_chs) - 1; i >= 0; i--) {
                        action_set_ch_t* action_set_ch = malloc(sizeof(action_set_ch_t));
                        memset(action_set_ch, 0, sizeof(*action_set_ch));

                        action_set_ch->action = new_int_action;
                        
                        action_set_ch->next = last_action;
                        last_action = action_set_ch;
                        
                        cJSON* json_set_ch = cJSON_GetArrayItem(json_set_chs, i);
                        for (int j = 0; j < 4; j++) {
                            const int value = (uint8_t) cJSON_GetArrayItem(json_set_ch, j)->valuefloat;
                            
                            switch (j) {
                                case 0:
                                    action_set_ch->source_serv = get_absolut_index(ch_group->serv_index, value);
                                    break;
                                    
                                case 1:
                                    action_set_ch->source_ch = value;
                                    break;
                                    
                                case 2:
                                    action_set_ch->target_serv = get_absolut_index(ch_group->serv_index, value);
                                    break;
                                    
                                case 3:
                                    action_set_ch->target_ch = value;
                                    break;
                            }
                        }
                        
                        INFO("New A%i SetCh %i.%i->%i.%i", new_int_action, action_set_ch->source_serv, action_set_ch->source_ch, action_set_ch->target_serv, action_set_ch->target_ch);
                    }
                }
            }
        }
        
        if (fixed_action < MAX_ACTIONS) {
            for (int int_action = 0; int_action < MAX_ACTIONS; int_action++) {
                register_action(json_context, int_action);
            }
        } else {
            register_action(json_context, fixed_action);
        }
        
        ch_group->action_set_ch = last_action;
    }
    
    // System Actions
    inline void new_action_system(ch_group_t* ch_group, cJSON* json_context, uint8_t fixed_action) {
        action_system_t* last_action = ch_group->action_system;
        
        void register_action(cJSON* json_accessory, uint8_t new_int_action) {
            char action[4];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), SYSTEM_ACTIONS_ARRAY) != NULL) {
                    cJSON* json_action_systems = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), SYSTEM_ACTIONS_ARRAY);
                    for (int i = cJSON_GetArraySize(json_action_systems) - 1; i >= 0; i--) {
                        action_system_t* action_system = malloc(sizeof(action_system_t));
                        memset(action_system, 0, sizeof(*action_system));
                        
                        cJSON* json_action_system = cJSON_GetArrayItem(json_action_systems, i);
                        
                        action_system->action = new_int_action;
                        
                        action_system->value = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_action_system, SYSTEM_ACTION)->valuefloat;
                        
                        action_system->next = last_action;
                        last_action = action_system;
                        
                        INFO("New A%i Sys v %i", new_int_action, action_system->value);
                    }
                }
            }
        }
        
        if (fixed_action < MAX_ACTIONS) {
            for (int int_action = 0; int_action < MAX_ACTIONS; int_action++) {
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
            char action[4];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), NETWORK_ACTIONS_ARRAY) != NULL) {
                    cJSON* json_action_networks = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), NETWORK_ACTIONS_ARRAY);
                    for (int i = cJSON_GetArraySize(json_action_networks) - 1; i >= 0; i--) {
                        action_network_t* action_network = malloc(sizeof(action_network_t));
                        memset(action_network, 0, sizeof(*action_network));
                        
                        cJSON* json_action_network = cJSON_GetArrayItem(json_action_networks, i);
                        
                        action_network->action = new_int_action;
                        
                        action_network->host = uni_strdup(cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_HOST)->valuestring, &unistrings);
                        
                        action_network->port_n = 80;
                        if (cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_PORT) != NULL) {
                            action_network->port_n = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_PORT)->valuefloat;
                        }
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_WAIT_RESPONSE_SET) != NULL) {
                            action_network->wait_response = (uint8_t) (cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_WAIT_RESPONSE_SET)->valuefloat * 10);
                        }
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_METHOD) != NULL) {
                            action_network->method_n = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_METHOD)->valuefloat;
                        }
                        
                        if (action_network->method_n < 3) {
                            if (cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_URL) != NULL) {
                                action_network->url = uni_strdup(cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_URL)->valuestring, &unistrings);
                            } else {
                                action_network->url = uni_strdup("", &unistrings);
                            }
                            
                            if (cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_HEADER) != NULL) {
                                action_network->header = uni_strdup(cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_HEADER)->valuestring, &unistrings);
                            } else {
                                action_network->header = uni_strdup("Content-type: text/html\r\n", &unistrings);
                            }
                        }
                        
                        if (action_network->method_n > 0) {
                            if (cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_CONTENT) != NULL) {
                                action_network->content = strdup(cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_CONTENT)->valuestring);
                            } else {
                                action_network->content = uni_strdup("", &unistrings);
                            }
                            
                            if (action_network->method_n ==  3 ||
                                action_network->method_n == 13) {
                                action_network->len = strlen(action_network->content);
                            } else if (action_network->method_n ==  4 ||
                                       action_network->method_n == 12 ||
                                       action_network->method_n == 14) {
                                
                                free(action_network->content);
                                action_network->len = process_hexstr(cJSON_GetObjectItemCaseSensitive(json_action_network, NETWORK_ACTION_CONTENT)->valuestring, &action_network->raw, &unistrings);
                            }
                        }
                        
                        INFO("New A%i Net %s:%i", new_int_action, action_network->host, action_network->port_n);
                        
                        action_network->next = last_action;
                        last_action = action_network;
                    }
                }
            }
        }
        
        if (fixed_action < MAX_ACTIONS) {
            for (int int_action = 0; int_action < MAX_ACTIONS; int_action++) {
                register_action(json_context, int_action);
            }
        } else {
            register_action(json_context, fixed_action);
        }
        
        ch_group->action_network = last_action;
    }
    
    // IR TX Actions
    inline void new_action_irrf_tx(ch_group_t* ch_group, cJSON* json_context, uint8_t fixed_action) {
        action_irrf_tx_t* last_action = ch_group->action_irrf_tx;
        
        void register_action(cJSON* json_accessory, uint8_t new_int_action) {
            char action[4];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), IRRF_ACTIONS_ARRAY) != NULL) {
                    cJSON* json_action_irrf_txs = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), IRRF_ACTIONS_ARRAY);
                    for (int i = cJSON_GetArraySize(json_action_irrf_txs) - 1; i >= 0; i--) {
                        action_irrf_tx_t* action_irrf_tx = malloc(sizeof(action_irrf_tx_t));
                        memset(action_irrf_tx, 0, sizeof(*action_irrf_tx));
                        
                        cJSON* json_action_irrf_tx = cJSON_GetArrayItem(json_action_irrf_txs, i);
                        
                        action_irrf_tx->action = new_int_action;
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_irrf_tx, IRRF_ACTION_PROTOCOL) != NULL) {
                            action_irrf_tx->prot = uni_strdup(cJSON_GetObjectItemCaseSensitive(json_action_irrf_tx, IRRF_ACTION_PROTOCOL)->valuestring, &unistrings);
                        }
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_irrf_tx, IRRF_ACTION_PROTOCOL_CODE) != NULL) {
                            action_irrf_tx->prot_code = uni_strdup(cJSON_GetObjectItemCaseSensitive(json_action_irrf_tx, IRRF_ACTION_PROTOCOL_CODE)->valuestring, &unistrings);
                        }
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_irrf_tx, IRRF_ACTION_RAW_CODE) != NULL) {
                            action_irrf_tx->raw_code = uni_strdup(cJSON_GetObjectItemCaseSensitive(json_action_irrf_tx, IRRF_ACTION_RAW_CODE)->valuestring, &unistrings);
                        }
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_irrf_tx, IRRF_ACTION_FREQ) != NULL) {
                            unsigned int freq = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_action_irrf_tx, IRRF_ACTION_FREQ)->valuefloat;
                            if (freq == 1) {    // RF
                                action_irrf_tx->freq = 1;
                            } else {            // IR
                                action_irrf_tx->freq = 1000 / freq / 2;
                            }
                        }
                        
                        action_irrf_tx->repeats = 1;
                        if (cJSON_GetObjectItemCaseSensitive(json_action_irrf_tx, IRRF_ACTION_REPEATS) != NULL) {
                            action_irrf_tx->repeats = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_action_irrf_tx, IRRF_ACTION_REPEATS)->valuefloat;
                        }
                        
                        action_irrf_tx->pause = MS_TO_TICKS(100);
                        if (cJSON_GetObjectItemCaseSensitive(json_action_irrf_tx, IRRF_ACTION_REPEATS_PAUSE) != NULL) {
                            action_irrf_tx->pause = MS_TO_TICKS(cJSON_GetObjectItemCaseSensitive(json_action_irrf_tx, IRRF_ACTION_REPEATS_PAUSE)->valuefloat);
                        }
                        
                        action_irrf_tx->next = last_action;
                        last_action = action_irrf_tx;
                        
                        INFO("New A%i IRRF r %i, p %i", new_int_action, action_irrf_tx->repeats, action_irrf_tx->pause);
                    }
                }
            }
        }
        
        if (fixed_action < MAX_ACTIONS) {
            for (int int_action = 0; int_action < MAX_ACTIONS; int_action++) {
                register_action(json_context, int_action);
            }
        } else {
            register_action(json_context, fixed_action);
        }
        
        ch_group->action_irrf_tx = last_action;
    }
    
    // UART Actions
    inline void new_action_uart(ch_group_t* ch_group, cJSON* json_context, uint8_t fixed_action) {
        action_uart_t* last_action = ch_group->action_uart;
        
        void register_action(cJSON* json_accessory, uint8_t new_int_action) {
            char action[4];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), UART_ACTIONS_ARRAY) != NULL) {
                    cJSON* json_action_uarts = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), UART_ACTIONS_ARRAY);
                    for (int i = cJSON_GetArraySize(json_action_uarts) - 1; i >= 0; i--) {
                        action_uart_t* action_uart = malloc(sizeof(action_uart_t));
                        memset(action_uart, 0, sizeof(*action_uart));
                        
                        cJSON* json_action_uart = cJSON_GetArrayItem(json_action_uarts, i);
                        
                        action_uart->action = new_int_action;
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_uart, UART_ACTION_PAUSE) != NULL) {
                            action_uart->pause = MS_TO_TICKS(cJSON_GetObjectItemCaseSensitive(json_action_uart, UART_ACTION_PAUSE)->valuefloat);
                        }
                        
                        int uart = 0;
                        if (cJSON_GetObjectItemCaseSensitive(json_action_uart, UART_ACTION_UART) != NULL) {
                            uart = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_action_uart, UART_ACTION_UART)->valuefloat;
                        }
                        
                        action_uart->uart = uart % 10;
                        int is_text = uart / 10;
                        
                        if (cJSON_GetObjectItemCaseSensitive(json_action_uart, VALUE) != NULL) {
                            if (is_text) {
                                action_uart->command = (uint8_t*) uni_strdup(cJSON_GetObjectItemCaseSensitive(json_action_uart, VALUE)->valuestring, &unistrings);
                                action_uart->len = strlen((char*) action_uart->command);
                            } else {
                                action_uart->len = process_hexstr(cJSON_GetObjectItemCaseSensitive(json_action_uart, VALUE)->valuestring, &action_uart->command, &unistrings);
                            }
                        }
                        
                        action_uart->next = last_action;
                        last_action = action_uart;
                        
                        INFO("New A%i UART%i p %i, l %i", new_int_action, action_uart->uart, action_uart->pause, action_uart->len);
                    }
                }
            }
        }
        
        if (fixed_action < MAX_ACTIONS) {
            for (int int_action = 0; int_action < MAX_ACTIONS; int_action++) {
                register_action(json_context, int_action);
            }
        } else {
            register_action(json_context, fixed_action);
        }
        
        ch_group->action_uart = last_action;
    }
    
    inline void new_action_pwm(ch_group_t* ch_group, cJSON* json_context, uint8_t fixed_action) {
        action_pwm_t* last_action = ch_group->action_pwm;
        
        void register_action(cJSON* json_accessory, uint8_t new_int_action) {
            char action[4];
            itoa(new_int_action, action, 10);
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, action) != NULL) {
                if (cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), PWM_ACTIONS_ARRAY) != NULL) {
                    cJSON* json_action_pwms = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json_accessory, action), PWM_ACTIONS_ARRAY);
                    for (int i = cJSON_GetArraySize(json_action_pwms) - 1; i >= 0; i--) {
                        action_pwm_t* action_pwm = malloc(sizeof(action_pwm_t));
                        memset(action_pwm, 0, sizeof(*action_pwm));
                        
                        action_pwm->action = new_int_action;
                        
                        action_pwm->next = last_action;
                        last_action = action_pwm;
                        
                        cJSON* json_action_pwm = cJSON_GetArrayItem(json_action_pwms, i);
                        for (int j = 0; j < cJSON_GetArraySize(json_action_pwm); j++) {
                            const int value = cJSON_GetArrayItem(json_action_pwm, j)->valuefloat;
                            
                            switch (j) {
                                case 0:
                                    action_pwm->gpio = value;
                                    break;
                                    
                                case 1:
                                    action_pwm->duty = value;
                                    break;
                                    
                                case 2:
                                    action_pwm->freq = value;
                                    break;
                                    
                                case 3:
                                    action_pwm->dithering = value;
                                    break;
                            }
                        }
                        
                        INFO("New A%i PWM g %i->%i, d %i, f %i", new_int_action, action_pwm->gpio, action_pwm->duty, action_pwm->freq, action_pwm->dithering);
                    }
                }
            }
        }
        
        if (fixed_action < MAX_ACTIONS) {
            for (int int_action = 0; int_action < MAX_ACTIONS; int_action++) {
                register_action(json_context, int_action);
            }
        } else {
            register_action(json_context, fixed_action);
        }
        
        ch_group->action_pwm = last_action;
    }
    
    void register_actions(ch_group_t* ch_group, cJSON* json_accessory, uint8_t fixed_action) {
        new_action_copy(ch_group, json_accessory, fixed_action);
        new_action_binary_output(ch_group, json_accessory, fixed_action);
        new_action_serv_manager(ch_group, json_accessory, fixed_action);
        new_action_system(ch_group, json_accessory, fixed_action);
        new_action_network(ch_group, json_accessory, fixed_action);
        new_action_irrf_tx(ch_group, json_accessory, fixed_action);
        new_action_uart(ch_group, json_accessory, fixed_action);
        new_action_pwm(ch_group, json_accessory, fixed_action);
        new_action_set_ch(ch_group, json_accessory, fixed_action);
    }
    
    void register_wildcard_actions(ch_group_t* ch_group, cJSON* json_accessory) {
        int global_index = MAX_ACTIONS;     // First wirldcard action must have a higher index than highest possible normal action
        wildcard_action_t* last_action = ch_group->wildcard_action;
        
        for (int int_index = 0; int_index < MAX_WILDCARD_ACTIONS; int_index++) {
            char number[4];
            itoa(int_index, number, 10);
            
            char index[6];
            snprintf(index, 5, "%s%s", WILDCARD_ACTIONS_ARRAY_HEADER, number);
            
            cJSON* json_wilcard_actions = cJSON_GetObjectItemCaseSensitive(json_accessory, index);
            for (int i = 0; i < cJSON_GetArraySize(json_wilcard_actions); i++) {
                wildcard_action_t* wildcard_action = malloc(sizeof(wildcard_action_t));
                memset(wildcard_action, 0, sizeof(*wildcard_action));
                
                cJSON* json_wilcard_action = cJSON_GetArrayItem(json_wilcard_actions, i);
                
                wildcard_action->index = int_index;
                wildcard_action->value = (float) cJSON_GetObjectItemCaseSensitive(json_wilcard_action, VALUE)->valuefloat;
                
                if (cJSON_GetObjectItemCaseSensitive(json_wilcard_action, WILDCARD_ACTION_REPEAT) != NULL) {
                    wildcard_action->repeat = (bool) cJSON_GetObjectItemCaseSensitive(json_wilcard_action, WILDCARD_ACTION_REPEAT)->valuefloat;
                }
                
                if (cJSON_GetObjectItemCaseSensitive(json_wilcard_action, "0") != NULL) {
                    char action[4];
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
            return (uint8_t) cJSON_GetObjectItemCaseSensitive(json_accessory, ENABLE_HOMEKIT)->valuefloat;
        }
        return 1;
    }
    
    TimerHandle_t autoswitch_time(cJSON* json_accessory, ch_group_t* ch_group) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, AUTOSWITCH_TIME) != NULL) {
            const uint32_t time = cJSON_GetObjectItemCaseSensitive(json_accessory, AUTOSWITCH_TIME)->valuefloat * 1000.f;
            if (time > 0) {
                return rs_esp_timer_create(time, false, (void*) ch_group, hkc_autooff_setter_task);
            }
        }
        return NULL;
    }
    
    int8_t th_sensor_gpio(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_GPIO) != NULL) {
            /*
            const uint8_t sensor_gpio = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_GPIO)->valuefloat;
            set_used_gpio(sensor_gpio);
            return sensor_gpio;
            */
            
            return (uint8_t) cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_GPIO)->valuefloat;
        }
        return -1;
    }
    
    uint8_t th_sensor_type(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_TYPE) != NULL) {
            return (uint8_t) cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_TYPE)->valuefloat;
        }
        return 2;
    }
    
    float th_sensor_temp_offset(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_OFFSET) != NULL) {
            return (float) cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_OFFSET)->valuefloat;
        }
        return 0;
    }
    
    float th_sensor_hum_offset(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, HUMIDITY_OFFSET) != NULL) {
            return (float) cJSON_GetObjectItemCaseSensitive(json_accessory, HUMIDITY_OFFSET)->valuefloat;
        }
        return 0;
    }
    
    uint8_t th_sensor_index(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_INDEX) != NULL) {
            return (uint8_t) cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_INDEX)->valuefloat;
        }
        return 1;
    }
    
    float sensor_poll_period(cJSON* json_accessory, float poll_period) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_POLL_PERIOD) != NULL) {
            poll_period = (float) cJSON_GetObjectItemCaseSensitive(json_accessory, TEMPERATURE_SENSOR_POLL_PERIOD)->valuefloat;
            
            if (poll_period < TH_SENSOR_POLL_PERIOD_MIN && poll_period > 0.00f) {
                poll_period = TH_SENSOR_POLL_PERIOD_MIN;
            }
        }
        
        return poll_period;
    }
    
    float th_update_delay(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, THERMOSTAT_UPDATE_DELAY) != NULL) {
            float th_delay = (float) cJSON_GetObjectItemCaseSensitive(json_accessory, THERMOSTAT_UPDATE_DELAY)->valuefloat;
            if (th_delay < THERMOSTAT_UPDATE_DELAY_MIN) {
                return THERMOSTAT_UPDATE_DELAY_MIN;
            }
            return th_delay;
        }
        return THERMOSTAT_UPDATE_DELAY_DEFAULT;
    }
    
    float th_sensor(ch_group_t* ch_group, cJSON* json_accessory) {
        TH_SENSOR_GPIO = th_sensor_gpio(json_accessory);
        const unsigned int sensor_type = th_sensor_type(json_accessory);
        TH_SENSOR_TYPE = sensor_type;
        TH_SENSOR_TEMP_OFFSET = th_sensor_temp_offset(json_accessory);
        TH_SENSOR_HUM_OFFSET = th_sensor_hum_offset(json_accessory);
        TH_SENSOR_ERROR_COUNT = 0;
        if (sensor_type == 3) {
            TH_SENSOR_INDEX = th_sensor_index(json_accessory);
        }
        
        return sensor_poll_period(json_accessory, TH_SENSOR_POLL_PERIOD_DEFAULT);
    }
    
    void th_sensor_starter(ch_group_t* ch_group, float poll_period) {
        ch_group->timer = rs_esp_timer_create(poll_period * 1000, true, (void*) ch_group, temperature_timer_worker);
    }
    
    int virtual_stop(cJSON* json_accessory) {
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, VIRTUAL_STOP_SET) != NULL) {
            return (uint8_t) cJSON_GetObjectItemCaseSensitive(json_accessory, VIRTUAL_STOP_SET)->valuefloat;
        }
        return VIRTUAL_STOP_DEFAULT;
    }
    
    // ----- CONFIG SECTION
    
    // Boot early delay
    if (cJSON_GetObjectItemCaseSensitive(json_config, BOOT_EARLY_DELAY) != NULL) {
        vTaskDelay(cJSON_GetObjectItemCaseSensitive(json_config, BOOT_EARLY_DELAY)->valuefloat * MS_TO_TICKS(1000));
    }
    
    // UART configuration
#ifndef ESP_PLATFORM
    int is_uart_swap = false;
#endif
    
    if (cJSON_GetObjectItemCaseSensitive(json_config, UART_CONFIG_ARRAY) != NULL) {
        cJSON* json_uarts = cJSON_GetObjectItemCaseSensitive(json_config, UART_CONFIG_ARRAY);
        for (int i = 0; i < cJSON_GetArraySize(json_uarts); i++) {
            cJSON* json_uart = cJSON_GetArrayItem(json_uarts, i);
            
            if (cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_ENABLE) != NULL) {
                int uart_config = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_ENABLE)->valuefloat;
                
                int uart_with_receiver = false;
                if (uart_config >= 10) {
                    uart_with_receiver = true;
                    uart_config -= 10;
                }
                
#ifdef ESP_PLATFORM
                int uart_pin[4] = { -1, -1, -1, -1 };
                cJSON* json_uart_gpios = cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_GPIO_ARRAY);
                for (unsigned int j = 0; j < cJSON_GetArraySize(json_uart_gpios); j++) {
                    uart_pin[j] = cJSON_GetArrayItem(json_uart_gpios, j)->valuefloat;
                }
                
                uart_driver_delete(uart_config);
                
                uart_config_t uart_config_data = {
                    .baud_rate = 115200,
                    .data_bits = UART_DATA_8_BITS,
                    .parity    = UART_PARITY_DISABLE,
                    .stop_bits = UART_STOP_BITS_1,
                    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                    .source_clk = UART_SCLK_DEFAULT,
                };
                
                uart_driver_install(uart_config, SDK_UART_BUFFER_SIZE, 0, 0, NULL, 0);
                uart_param_config(uart_config, &uart_config_data);
                
                for (unsigned int p = 0; p < 4; p++) {
                    if (uart_pin[p] >= 0) {
                        gpio_reset_pin(uart_pin[p]);
                    }
                }
                
                uart_set_pin(uart_config, uart_pin[0], uart_pin[1], uart_pin[2], uart_pin[3]);
#else
                switch (uart_config) {
                    case 1:
                        //set_used_gpio(2);
                        //gpio_disable(2);
                        gpio_set_iomux_function(2, IOMUX_GPIO2_FUNC_UART1_TXD);
                        break;
                        
                    case 2:
                        //set_used_gpio(15);
                        //gpio_disable(15);
                        sdk_system_uart_swap();
                        is_uart_swap = true;
                        uart_config = 0;
                        break;
                        
                    default:    // case 0:
                        reset_uart();
                        break;
                }
#endif
                
                uint32_t speed = 115200;
                if (cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_SPEED) != NULL) {
                    speed = (uint32_t) cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_SPEED)->valuefloat;
                }
                
                unsigned int stopbits = 1;
                if (cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_STOPBITS) != NULL) {
                    stopbits = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_STOPBITS)->valuefloat;
                }
                
                unsigned int parity = 0;
                if (cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_PARITY) != NULL) {
                    parity = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_PARITY)->valuefloat;
                }
                
#ifdef ESP_PLATFORM
                unsigned int uart_mode = 0;
                if (cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_MODE) != NULL) {
                    uart_mode = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_uart, UART_CONFIG_MODE)->valuefloat;
                }
                uart_set_mode(uart_config, uart_mode);
                
#define uart_set_baud(uart_config, speed)   uart_set_baudrate(uart_config, speed)
#define uart_set_stopbits(port, config)     uart_set_stop_bits(port, config)
#define UART_STOPBITS_1                     UART_STOP_BITS_1
#define UART_STOPBITS_1_5                   UART_STOP_BITS_1_5
#define UART_STOPBITS_2                     UART_STOP_BITS_2
#define UART_STOPBITS_0                     UART_STOP_BITS_MAX
#endif
                
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
#ifndef ESP_PLATFORM
                        uart_set_parity_enabled(uart_config, true);
#endif
                        uart_set_parity(uart_config, UART_PARITY_ODD);
                        break;
                        
                    case 2:
#ifndef ESP_PLATFORM
                        uart_set_parity_enabled(uart_config, true);
#endif
                        uart_set_parity(uart_config, UART_PARITY_EVEN);
                        break;
                        
                    default:    // case 0:
#ifndef ESP_PLATFORM
                        uart_set_parity_enabled(uart_config, false);
#else
                        uart_set_parity(uart_config, UART_PARITY_DISABLE);
#endif
                        break;
                }
                
                
                if (uart_with_receiver) {
                    uart_receiver_data_t* uart_receiver_data = malloc(sizeof(uart_receiver_data_t));
                    memset(uart_receiver_data, 0, sizeof(uart_receiver_data_t));
                    
                    uart_receiver_data->next = main_config.uart_receiver_data;
                    main_config.uart_receiver_data = uart_receiver_data;
                    
                    uart_receiver_data->uart_port = uart_config;
                    uart_receiver_data->uart_min_len = RECV_UART_BUFFER_MIN_LEN_DEFAULT;
                    uart_receiver_data->uart_max_len = RECV_UART_BUFFER_MAX_LEN_DEFAULT;
                    if (cJSON_GetObjectItemCaseSensitive(json_uart, RECV_UART_BUFFER_LEN_ARRAY_SET) != NULL) {
                        cJSON* uart_len_array = cJSON_GetObjectItemCaseSensitive(json_uart, RECV_UART_BUFFER_LEN_ARRAY_SET);
                        uart_receiver_data->uart_min_len = (uint8_t) cJSON_GetArrayItem(uart_len_array, 0)->valuefloat;
                        uart_receiver_data->uart_max_len = (uint8_t) cJSON_GetArrayItem(uart_len_array, 1)->valuefloat;
                    }
                    
#ifndef ESP_PLATFORM
                    if (!is_uart_swap) {
                        gpio_disable(3);
                        iomux_set_pullup_flags(4, IOMUX_PIN_PULLUP);
                        iomux_set_function(4, IOMUX_GPIO3_FUNC_UART0_RXD);
                    }
#endif
                }
            }
        
        }
    }
    
    // Log output type
    unsigned int log_output_type = 0;
    if (cJSON_GetObjectItemCaseSensitive(json_config, LOG_OUTPUT) != NULL) {
        log_output_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, LOG_OUTPUT)->valuefloat;
    }
    
    char* log_output_target = NULL;
    if (cJSON_GetObjectItemCaseSensitive(json_config, LOG_OUTPUT_TARGET) != NULL) {
        log_output_target = uni_strdup(cJSON_GetObjectItemCaseSensitive(json_config, LOG_OUTPUT_TARGET)->valuestring, &unistrings);
    }
    
    adv_logger_init(log_output_type, log_output_target, true);
    free(log_output_target);
    
    if (log_output_type > 0) {
        printf_header();
        //INFO("%s\n", txt_config);
        
        char* txt_config_buffer = malloc(256);
        txt_config_buffer[0] = 0;
        
        for (unsigned int i = 0; i < strlen(txt_config); i += 255) {
            for (unsigned int j = 0; j < 255; j++) {
                txt_config_buffer[j] = txt_config[i + j];
                
                if (txt_config_buffer[j] == 0) {
                    break;
                }
            }
            
            txt_config_buffer[255] = 0;
            INFO("%s", txt_config_buffer);
        }
        
        free(txt_config_buffer);
        
        INFO("");
    }
    
    free(txt_config);
    
    // I2C Bus
    if (cJSON_GetObjectItemCaseSensitive(json_config, I2C_CONFIG_ARRAY) != NULL) {
        cJSON* json_i2cs = cJSON_GetObjectItemCaseSensitive(json_config, I2C_CONFIG_ARRAY);
        for (int i = 0; i < cJSON_GetArraySize(json_i2cs); i++) {
            cJSON* json_i2c = cJSON_GetArrayItem(json_i2cs, i);
            
            unsigned int i2c_scl_gpio = (uint8_t) cJSON_GetArrayItem(json_i2c, 0)->valuefloat;
            //set_used_gpio(i2c_scl_gpio);
            
            unsigned int i2c_sda_gpio = (uint8_t) cJSON_GetArrayItem(json_i2c, 1)->valuefloat;
            //set_used_gpio(i2c_sda_gpio);
            
            uint32_t i2c_freq_hz = (uint32_t) (cJSON_GetArrayItem(json_i2c, 2)->valuefloat * 1000.f);
            
            unsigned int i2c_scl_gpio_pullup = false;
            unsigned int i2c_sda_gpio_pullup = false;
            
            const size_t json_i2c_array_size = cJSON_GetArraySize(json_i2c);
            
            if (json_i2c_array_size > 3) {
                i2c_scl_gpio_pullup = (bool) cJSON_GetArrayItem(json_i2c, 3)->valuefloat;
                
                if (json_i2c_array_size > 4) {
                    i2c_sda_gpio_pullup = (bool) cJSON_GetArrayItem(json_i2c, 4)->valuefloat;
                }
            }
            
            const unsigned int i2c_res = adv_i2c_init_hz(i, i2c_scl_gpio, i2c_sda_gpio, i2c_freq_hz, i2c_scl_gpio_pullup, i2c_sda_gpio_pullup);
            
#ifdef ESP_PLATFORM
            INFO("I2C bus %i: scl %i, sda %i, f %li, r %i", i, i2c_scl_gpio, i2c_sda_gpio, i2c_freq_hz, i2c_res);
#else
            INFO("I2C bus %i: scl %i, sda %i, f %i, r %i", i, i2c_scl_gpio, i2c_sda_gpio, i2c_freq_hz, i2c_res);
#endif
        }
    }
    
    // MCP23017 Interfaces
    if (cJSON_GetObjectItemCaseSensitive(json_config, MCP23017_ARRAY) != NULL) {
        cJSON* json_mcp23017s = cJSON_GetObjectItemCaseSensitive(json_config, MCP23017_ARRAY);
        for (int i = 0; i < cJSON_GetArraySize(json_mcp23017s); i++) {
            cJSON* json_mcp23017 = cJSON_GetArrayItem(json_mcp23017s, i);
            
            mcp23017_t* mcp23017 = malloc(sizeof(mcp23017_t));
            memset(mcp23017, 0, sizeof(*mcp23017));

            mcp23017->next = main_config.mcp23017s;
            main_config.mcp23017s = mcp23017;
            
            mcp23017->index = i + 1;

            mcp23017->bus = (uint8_t) cJSON_GetArrayItem(json_mcp23017, 0)->valuefloat;
            mcp23017->addr = (uint8_t) cJSON_GetArrayItem(json_mcp23017, 1)->valuefloat;
            
            INFO("MCP %i: b %i, a %i", mcp23017->index, mcp23017->bus, mcp23017->addr);
            
            const uint8_t byte_zeros = 0x00;
            const uint8_t byte_ones = 0xFF;
            
            // Full reset
            uint8_t mcp_reg;
            for (mcp_reg = 0x00; mcp_reg < 0x02; mcp_reg++) {
                adv_i2c_slave_write(mcp23017->bus, mcp23017->addr, &mcp_reg, 1, &byte_ones, 1);
            }
            for (mcp_reg = 0x02; mcp_reg < 0x16; mcp_reg++) {
                adv_i2c_slave_write(mcp23017->bus, mcp23017->addr, &mcp_reg, 1, &byte_zeros, 1);
            }
            
            for (int channel = 0; channel < 2; channel++) {
                unsigned int mcp_mode = 258;   // Default set to INPUT and not pullup
                if (channel == 0) {
                    if (cJSON_GetArrayItem(json_mcp23017, 2) != NULL) {
                        mcp_mode = (uint16_t) cJSON_GetArrayItem(json_mcp23017, 2)->valuefloat;
                    }
                    
                    INFO("MCP %i ChA: %i", mcp23017->index, mcp_mode);
                    
                } else {
                    if (cJSON_GetArrayItem(json_mcp23017, 3) != NULL) {
                        mcp_mode = (uint16_t) cJSON_GetArrayItem(json_mcp23017, 3)->valuefloat;
                    }
                    
                    INFO("MCP %i ChB: %i", mcp23017->index, mcp_mode);
                }
                
                uint8_t reg = channel;
                if (mcp_mode > 255) {   // Mode INPUT
                    switch (mcp_mode) {
                        case 256:
                            // Pull-up HIGH
                            reg += 0x0C;
                            adv_i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &byte_ones, 1);
                            // Polarity NORMAL
                            //reg = channel + 0x02;
                            //adv_i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &byte_zeros, 1);
                            break;
                            
                        case 257:
                            // Pull-up HIGH
                            reg += 0x0C;
                            adv_i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &byte_ones, 1);
                            // Polarity INVERTED
                            reg = channel + 0x02;
                            adv_i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &byte_ones, 1);
                            break;
                            
                        case 259:
                            // Pull-up LOW
                            //reg += 0x0C;
                            //adv_i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &byte_zeros, 1);
                            // Polarity INVERTED
                            reg = channel + 0x02;
                            adv_i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &byte_ones, 1);
                            break;
                            
                        default:    // 258
                            // Pull-up LOW
                            //reg += 0x0C;
                            //adv_i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &byte_zeros, 1);
                            // Polarity NORMAL
                            //reg = channel + 0x02;
                            //adv_i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &byte_zeros, 1);
                            break;
                    }
                    
                } else {    // Mode OUTPUT
                    reg += 0x14;
                    uint8_t mcp_channel = channel;
                    adv_i2c_slave_write(mcp23017->bus, mcp23017->addr, &mcp_channel, 1, &byte_zeros, 1);
                    
                    if (channel == 0) {
                        mcp23017->a_outs = mcp_mode;
                        adv_i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &mcp23017->a_outs, 1);
                    } else {
                        mcp23017->b_outs = mcp_mode;
                        adv_i2c_slave_write(mcp23017->bus, mcp23017->addr, &reg, 1, &mcp23017->b_outs, 1);
                    }
                }
            }
        }
    }
    
    // Buttons Main Config
    unsigned int adv_button_filter_value = 0;
    if (cJSON_GetObjectItemCaseSensitive(json_config, BUTTON_FILTER) != NULL) {
        adv_button_filter_value = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_config, BUTTON_FILTER)->valuefloat;
    }
    int adv_button_continuous_mode = false;
    if (cJSON_GetObjectItemCaseSensitive(json_config, BUTTON_CONTINUOS_MODE) != NULL) {
        adv_button_continuous_mode = (bool) cJSON_GetObjectItemCaseSensitive(json_config, BUTTON_CONTINUOS_MODE)->valuefloat;
    }
    if (adv_button_filter_value || adv_button_continuous_mode) {
        adv_button_init(adv_button_filter_value, adv_button_continuous_mode);
    }
    
    // GPIO Hardware Setup
    if (cJSON_GetObjectItemCaseSensitive(json_config, IO_CONFIG_ARRAY) != NULL) {
        cJSON* json_io_configs = cJSON_GetObjectItemCaseSensitive(json_config, IO_CONFIG_ARRAY);
        
        int use_software_pwm = false;
        
        for (int i = 0; i < cJSON_GetArraySize(json_io_configs); i++) {
            int io_value[5] = { 0, 0, 0, 0, 0 };
            
            cJSON* json_io_config = cJSON_GetArrayItem(json_io_configs, i);
            cJSON* json_io_config_gpios = cJSON_GetArrayItem(json_io_config, 0);
            for (int gpio_index = 0; gpio_index < cJSON_GetArraySize(json_io_config_gpios); gpio_index++) {
                const unsigned int gpio = cJSON_GetArrayItem(json_io_config_gpios, gpio_index)->valuefloat;
                
                for (int j = 1; j < cJSON_GetArraySize(json_io_config); j++) {
                    io_value[j - 1] = cJSON_GetArrayItem(json_io_config, j)->valuefloat;
                }
                
                if (gpio < 100) {
#ifdef ESP_PLATFORM
                    gpio_reset_pin(gpio);
#endif
                    
                    if (IO_GPIO_PULL_UP_DOWN >= 0) {
#ifdef ESP_PLATFORM
                        int esp_idf_pull = GPIO_FLOATING;
                        switch (IO_GPIO_PULL_UP_DOWN) {
                            case 1:
                                esp_idf_pull = GPIO_PULLUP_ONLY;
                                break;
                            case 2:
                                esp_idf_pull = GPIO_PULLDOWN_ONLY;
                                break;
                            case 3:
                                esp_idf_pull = GPIO_PULLUP_PULLDOWN;
                                break;
                        }
                        
                        gpio_set_pull_mode(gpio, esp_idf_pull);
                        gpio_sleep_set_pull_mode(gpio, esp_idf_pull);
#else
                        gpio_set_pullup(gpio, IO_GPIO_PULL_UP_DOWN, IO_GPIO_PULL_UP_DOWN);
#endif
                    }
                }
                
                if (IO_GPIO_MODE <= 5) {
#ifdef ESP_PLATFORM
                    gpio_set_direction(gpio, IO_GPIO_MODE);
                    gpio_sleep_set_direction(gpio, IO_GPIO_MODE);
#else
                    if (IO_GPIO_MODE == 0) {
                        gpio_disable(gpio);
                    } else {
                        gpio_enable(gpio, IO_GPIO_MODE - 1);
                    }
#endif
                    
                    if (IO_GPIO_MODE >= 2) {
                        gpio_write(gpio, IO_GPIO_OUTPUT_INIT_VALUE);
                    }
                    
                    INFO("GPIO %i, m %i, p %i, v %i", gpio, IO_GPIO_MODE, IO_GPIO_PULL_UP_DOWN, IO_GPIO_OUTPUT_INIT_VALUE);
                    
                } else if (IO_GPIO_MODE == 6) {
                    const int inverted = (bool) (IO_GPIO_BUTTON_MODE & 0b01);
                    
                    if (gpio < 100) {
#ifdef ESP_PLATFORM
                        gpio_set_direction(gpio, GPIO_MODE_INPUT);
#else
                        gpio_enable(gpio, GPIO_INPUT);
#endif
                        
                        const int pulse_mode = (bool) (IO_GPIO_BUTTON_MODE & 0b10);
                        
                        adv_button_create(gpio, inverted, pulse_mode, 0, IO_GPIO_BUTTON_FILTER);
                        
                        INFO("DigI GPIO %i, p %i, i %i, f %i, m %i", gpio, IO_GPIO_PULL_UP_DOWN, inverted, IO_GPIO_BUTTON_FILTER, pulse_mode);
                        
                    } else {    // MCP23017
                        mcp23017_t* mcp = mcp_find_by_index(gpio / 100);
                        adv_button_create(gpio, inverted, mcp->addr, mcp->bus, IO_GPIO_BUTTON_FILTER);
                        
                        INFO("DigI MCP Pin %i i %i, f %i, b %i, a %i", gpio, inverted, IO_GPIO_BUTTON_FILTER, mcp->bus, mcp->addr);
                    }
                    
                } else if (IO_GPIO_MODE <= 8) {
                    if (!use_software_pwm) {
                        use_software_pwm = true;
                        
                        // PWM-S frequency
                        if (cJSON_GetObjectItemCaseSensitive(json_config, PWMS_FREQ) != NULL) {
                            adv_pwm_set_freq((uint16_t) cJSON_GetObjectItemCaseSensitive(json_config, PWMS_FREQ)->valuefloat);
                        }
                        
                        // PWM-S Zero-Crossing GPIO
                        if (cJSON_GetObjectItemCaseSensitive(json_config, PWM_ZEROCROSSING_ARRAY_SET) != NULL) {
                            cJSON* zc_array = cJSON_GetObjectItemCaseSensitive(json_config, PWM_ZEROCROSSING_ARRAY_SET);
                            adv_pwm_set_zc_gpio((uint8_t) cJSON_GetArrayItem(zc_array, 0)->valuefloat, (uint8_t) cJSON_GetArrayItem(zc_array, 1)->valuefloat);
                        }
                    }
                    
                    if (IO_GPIO_MODE == 7) {
#ifdef ESP_PLATFORM
                        gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
#else
                        gpio_enable(gpio, GPIO_OUTPUT);
#endif
                    } else {
#ifdef ESP_PLATFORM
                        gpio_set_direction(gpio, GPIO_MODE_OUTPUT_OD);
#else
                        gpio_enable(gpio, GPIO_OUT_OPEN_DRAIN);
#endif
                    }
                    
                    const int inverted = (bool) (IO_GPIO_PWM_MODE & 0b0001);
                    const int leading = (bool) (IO_GPIO_PWM_MODE & 0b0010);
                    
                    adv_pwm_new_channel(gpio, inverted, leading, IO_GPIO_PWM_DITHERING, IO_GPIO_OUTPUT_INIT_VALUE);
                    
                    INFO("PWM-S GPIO %i, m %i, v %i, i %i, l %i, d %i", gpio, IO_GPIO_MODE, IO_GPIO_OUTPUT_INIT_VALUE, inverted, leading, IO_GPIO_PWM_DITHERING);
                    
#ifdef ESP_PLATFORM
                } else if (IO_GPIO_MODE == 9) {     // Only ESP32
                    pwmh_channel_t* pwmh_channel = main_config.pwmh_channels;
                    while (pwmh_channel &&
                           pwmh_channel->timer != IO_GPIO_PWMH_TIMER) {
                        pwmh_channel = pwmh_channel->next;
                    }
                    
                    if (!pwmh_channel) {
                        // PWM-H frequency
                        unsigned int pwmh_freq = PWMH_FREQ_DEFAULT;
                        if (cJSON_GetObjectItemCaseSensitive(json_config, PWMH_FREQ_ARRAY) != NULL) {
                            cJSON* pwmh_freq_array = cJSON_GetObjectItemCaseSensitive(json_config, PWMH_FREQ_ARRAY);
                            
                            if (cJSON_GetArraySize(pwmh_freq_array) > IO_GPIO_PWMH_TIMER) {
                                pwmh_freq = (uint16_t) cJSON_GetArrayItem(pwmh_freq_array, IO_GPIO_PWMH_TIMER)->valuefloat;
                            }
                        }
                        
                        ledc_timer_config_t ledc_timer = {
                            .speed_mode       = LEDC_LOW_SPEED_MODE,
                            .timer_num        = IO_GPIO_PWMH_TIMER,
                            .duty_resolution  = LEDC_TIMER_13_BIT,
                            .freq_hz          = pwmh_freq,
                            .clk_cfg          = LEDC_AUTO_CLK,
                        };
                        ledc_timer_config(&ledc_timer);
                    }
                    
                    pwmh_channel_t* new_pwmh_channel = malloc(sizeof(pwmh_channel_t));
                    memset(new_pwmh_channel, 0, sizeof(pwmh_channel_t));
                    
                    unsigned int channel = 0;
                    if (main_config.pwmh_channels) {
                        channel = main_config.pwmh_channels->channel + 1;
                    }
                    
                    const int inverted = (bool) (IO_GPIO_PWM_MODE & 0b0001);
                    const int leading = (bool) (IO_GPIO_PWM_MODE & 0b0010);
                    
                    new_pwmh_channel->gpio = gpio;
                    new_pwmh_channel->channel = channel;
                    new_pwmh_channel->timer = IO_GPIO_PWMH_TIMER;
                    new_pwmh_channel->leading = leading;
                    
                    new_pwmh_channel->next = main_config.pwmh_channels;
                    main_config.pwmh_channels = new_pwmh_channel;
                    
                    unsigned int initial_duty = leading ? (UINT16_MAX - IO_GPIO_OUTPUT_INIT_VALUE) : IO_GPIO_OUTPUT_INIT_VALUE;
                    initial_duty = initial_duty >> PWMH_BITS_DIVISOR;
                    
                    ledc_channel_config_t ledc_channel = {
                        .speed_mode             = LEDC_LOW_SPEED_MODE,
                        .channel                = channel,
                        .timer_sel              = IO_GPIO_PWMH_TIMER,
                        .intr_type              = LEDC_INTR_DISABLE,
                        .gpio_num               = gpio,
                        .duty                   = initial_duty,
                        .hpoint                 = 0,
                        .flags.output_invert    = inverted ^ leading,
                    };
                    ledc_channel_config(&ledc_channel);
                    
                    INFO("PWM-H GPIO %i, m %i, v %i, i %i, l %i", gpio, IO_GPIO_MODE, IO_GPIO_OUTPUT_INIT_VALUE, inverted, leading);
                    
                } else if (IO_GPIO_MODE == 10) {    // Only ESP32
                    if (main_config.adc_dac_data == NULL) {
                        main_config.adc_dac_data = malloc(sizeof(adc_dac_data_t));
                        memset(main_config.adc_dac_data, 0, sizeof(adc_dac_data_t));
                        
                        adc_oneshot_unit_init_cfg_t init_config = {
                            .unit_id = ADC_UNIT_1,
                            .ulp_mode = ADC_ULP_MODE_DISABLE,
                        };
                        adc_oneshot_new_unit(&init_config, &main_config.adc_dac_data->adc_oneshot_handle);
                    }
                    
                    adc_channel_t adc_channel;
                    adc_oneshot_io_to_channel(gpio, NULL, &adc_channel);
                    
                    adc_oneshot_chan_cfg_t config = {
                        .bitwidth = ADC_BITWIDTH_12,    // Max supported width: 12 bits (0 - 4095)
                        .atten = IO_GPIO_ADC_ATTENUATION,
                    };
                    adc_oneshot_config_channel(main_config.adc_dac_data->adc_oneshot_handle, adc_channel, &config);
                    
                    INFO("ADC GPIO %i, m %i a %i", gpio, IO_GPIO_MODE, IO_GPIO_ADC_ATTENUATION);
#endif
                    
                }
            }
        }
        
        if (use_software_pwm) {
            adv_pwm_start();
        }
    }
    
    // Custom Hostname
    char* custom_hostname = name.value.string_value;
    if (cJSON_GetObjectItemCaseSensitive(json_config, CUSTOM_HOSTNAME) != NULL) {
        custom_hostname = uni_strdup(cJSON_GetObjectItemCaseSensitive(json_config, CUSTOM_HOSTNAME)->valuestring, &unistrings);
    }
    
    // Custom NTP Host
    if (cJSON_GetObjectItemCaseSensitive(json_config, CUSTOM_NTP_HOST) != NULL) {
        main_config.ntp_host = uni_strdup(cJSON_GetObjectItemCaseSensitive(json_config, CUSTOM_NTP_HOST)->valuestring, &unistrings);
    }
    
    // Timezone
    if (cJSON_GetObjectItemCaseSensitive(json_config, TIMEZONE) != NULL) {
        setenv("TZ", cJSON_GetObjectItemCaseSensitive(json_config, TIMEZONE)->valuestring, 1);
        tzset();
    }
    
    // Timetable Actions
    if (cJSON_GetObjectItemCaseSensitive(json_config, TIMETABLE_ACTION_ARRAY) != NULL) {
        cJSON* json_timetable_actions = cJSON_GetObjectItemCaseSensitive(json_config, TIMETABLE_ACTION_ARRAY);
        for (int i = 0; i < cJSON_GetArraySize(json_timetable_actions); i++) {
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
            for (int j = 0; j < cJSON_GetArraySize(json_timetable_action); j++) {
                const int value = (int8_t) cJSON_GetArrayItem(json_timetable_action, j)->valuefloat;
                
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
            
            INFO("New TT %i: %ih %im, %i wday, %i mday, %i month", timetable_action->action, timetable_action->hour, timetable_action->min, timetable_action->wday, timetable_action->mday, timetable_action->mon);
        }
    }
    
    // Status LED
    if (cJSON_GetObjectItemCaseSensitive(json_config, STATUS_LED_GPIO) != NULL) {
        const unsigned int led_gpio = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_config, STATUS_LED_GPIO)->valuefloat;
        
        int led_inverted = true;
        if (cJSON_GetObjectItemCaseSensitive(json_config, INVERTED) != NULL) {
            led_inverted = (bool) cJSON_GetObjectItemCaseSensitive(json_config, INVERTED)->valuefloat;
        }
        
        //set_used_gpio(led_gpio);
        led_create(led_gpio, led_inverted);
    }
    
    // IR TX LED GPIO
    if (cJSON_GetObjectItemCaseSensitive(json_config, IR_ACTION_TX_GPIO) != NULL) {
        // IR TX LED Frequency
        if (cJSON_GetObjectItemCaseSensitive(json_config, IRRF_ACTION_FREQ) != NULL) {
            main_config.ir_tx_freq = 1000 / cJSON_GetObjectItemCaseSensitive(json_config, IRRF_ACTION_FREQ)->valuefloat / 2;
        }
        
        // IR TX LED Inverted
        int inverted_value = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_config, IR_ACTION_TX_GPIO_INVERTED) != NULL) {
            inverted_value = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, IR_ACTION_TX_GPIO_INVERTED)->valuefloat;
        }
        
        main_config.ir_tx_inv = inverted_value;
        
        main_config.ir_tx_gpio = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, IR_ACTION_TX_GPIO)->valuefloat;
        //set_used_gpio(main_config.ir_tx_gpio);
        
        gpio_write(main_config.ir_tx_gpio, false ^ main_config.ir_tx_inv);
    }
    
    // RF TX GPIO
    if (cJSON_GetObjectItemCaseSensitive(json_config, RF_ACTION_TX_GPIO) != NULL) {
        // RF TX Inverted
        int inverted_value = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_config, RF_ACTION_TX_GPIO_INVERTED) != NULL) {
            inverted_value = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, RF_ACTION_TX_GPIO_INVERTED)->valuefloat;
        }
        
        main_config.rf_tx_inv = inverted_value;
        
        main_config.rf_tx_gpio = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, RF_ACTION_TX_GPIO)->valuefloat;
        //set_used_gpio(main_config.rf_tx_gpio);
        
        gpio_write(main_config.rf_tx_gpio, false ^ main_config.rf_tx_inv);
    }
    
    // Ping poll period
    if (cJSON_GetObjectItemCaseSensitive(json_config, PING_POLL_PERIOD) != NULL) {
        main_config.ping_poll_period = (float) cJSON_GetObjectItemCaseSensitive(json_config, PING_POLL_PERIOD)->valuefloat;
    }
    
#ifdef ESP_PLATFORM
    // Wifi Sleep Mode
    int wifi_sleep_mode = 1;
    if (cJSON_GetObjectItemCaseSensitive(json_config, WIFI_SLEEP_MODE_SET) != NULL) {
        wifi_sleep_mode = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, WIFI_SLEEP_MODE_SET)->valuefloat;
    }
#endif
    
    // ARP Gratuitous period
    if (cJSON_GetObjectItemCaseSensitive(json_config, WIFI_WATCHDOG_ARP_PERIOD_SET) != NULL) {
        main_config.wifi_arp_count_max = ((uint32_t) cJSON_GetObjectItemCaseSensitive(json_config, WIFI_WATCHDOG_ARP_PERIOD_SET)->valuefloat) / (WIFI_WATCHDOG_POLL_PERIOD_MS / 1000.0f);
    }
    
    // Allowed Setup Mode Time
    if (cJSON_GetObjectItemCaseSensitive(json_config, ALLOWED_SETUP_MODE_TIME) != NULL) {
        main_config.setup_mode_time = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_config, ALLOWED_SETUP_MODE_TIME)->valuefloat;
    }
    
    // HomeKit Server Clients
    config.max_clients = HOMEKIT_SERVER_MAX_CLIENTS_DEFAULT;
    if (cJSON_GetObjectItemCaseSensitive(json_config, HOMEKIT_SERVER_MAX_CLIENTS) != NULL) {
        config.max_clients = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, HOMEKIT_SERVER_MAX_CLIENTS)->valuefloat;
        if (config.max_clients > HOMEKIT_SERVER_MAX_CLIENTS_MAX) {
            config.max_clients = HOMEKIT_SERVER_MAX_CLIENTS_MAX;
        }
    }
    if (config.max_clients > 0) {
        main_config.enable_homekit_server = true;
    }
    
    // Allow unsecure connections
    if (cJSON_GetObjectItemCaseSensitive(json_config, ALLOW_INSECURE_CONNECTIONS) != NULL) {
        config.insecure = (bool) cJSON_GetObjectItemCaseSensitive(json_config, ALLOW_INSECURE_CONNECTIONS)->valuefloat;
    }
    
    // mDNS TTL
    config.mdns_ttl = MDNS_TTL_DEFAULT;
    config.mdns_ttl_period = MDNS_TTL_PERIOD_DEFAULT;
    if (cJSON_GetObjectItemCaseSensitive(json_config, MDNS_TTL) != NULL) {
        cJSON* mdns_ttl_array = cJSON_GetObjectItemCaseSensitive(json_config, MDNS_TTL);
        config.mdns_ttl = (uint16_t) cJSON_GetArrayItem(mdns_ttl_array, 0)->valuefloat;
        config.mdns_ttl_period = config.mdns_ttl;
        
        if (cJSON_GetArraySize(mdns_ttl_array) > 1) {
            config.mdns_ttl_period = (uint16_t) cJSON_GetArrayItem(mdns_ttl_array, 1)->valuefloat;
        }
    }
    
    // Gateway Ping
    if (cJSON_GetObjectItemCaseSensitive(json_config, WIFI_PING_ERRORS) != NULL) {
        main_config.wifi_ping_max_errors = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, WIFI_PING_ERRORS)->valuefloat;
        main_config.wifi_ping_max_errors++;
    }
    
    // Common serial prefix
    char* common_serial_prefix = NULL;
    if (cJSON_GetObjectItemCaseSensitive(json_config, SERIAL_STRING) != NULL) {
        common_serial_prefix = cJSON_GetObjectItemCaseSensitive(json_config, SERIAL_STRING)->valuestring;
    }
    
    // Accessory to include HAA Settings
    unsigned int haa_setup_accessory = 1;
    if (cJSON_GetObjectItemCaseSensitive(json_config, HAA_SETUP_ACCESSORY_SET) != NULL) {
        haa_setup_accessory = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_config, HAA_SETUP_ACCESSORY_SET)->valuefloat;
    }
    haa_setup_accessory--;
    
    // Times to toggle quickly an accessory status to enter setup mode
    if (cJSON_GetObjectItemCaseSensitive(json_config, SETUP_MODE_ACTIVATE_COUNT) != NULL) {
        main_config.setup_mode_toggle_counter_max = (int8_t) cJSON_GetObjectItemCaseSensitive(json_config, SETUP_MODE_ACTIVATE_COUNT)->valuefloat;
    }
    
    if (main_config.setup_mode_toggle_counter_max > 0) {
        main_config.setup_mode_toggle_timer = rs_esp_timer_create(SETUP_MODE_TOGGLE_TIME_MS, false, NULL, setup_mode_toggle);
    }
    
    // Buttons to enter setup mode
    diginput_register(cJSON_GetObjectItemCaseSensitive(json_config, BUTTONS_ARRAY), setup_mode_call, NULL, 0);
    
    // ----- END CONFIG SECTION
    
    uint8_t get_serv_type(cJSON* json_accessory) {
        unsigned int serv_type = SERV_TYPE_SWITCH;
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, SERVICE_TYPE_SET) != NULL) {
            serv_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_accessory, SERVICE_TYPE_SET)->valuefloat;
        }
        
        return serv_type;
    }
    
    unsigned int get_service_recount(const uint8_t serv_type, cJSON* json_context) {
        unsigned int service_recount = 1;
        
        switch (serv_type) {
            case SERV_TYPE_DOORBELL:
            case SERV_TYPE_THERMOSTAT_WITH_HUM:
            case SERV_TYPE_TH_SENSOR:
            case SERV_TYPE_HUMIDIFIER_WITH_TEMP:
                service_recount = 2;
                break;
                
            case SERV_TYPE_TV:
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
    
    unsigned int get_total_services(const uint16_t serv_type, cJSON* json_accessory) {
        unsigned int total_services = 2 + get_service_recount(serv_type, json_accessory);
        
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, EXTRA_SERVICES_ARRAY) != NULL) {
            cJSON* json_extra_services = cJSON_GetObjectItemCaseSensitive(json_accessory, EXTRA_SERVICES_ARRAY);
            for (int i = 0; i < cJSON_GetArraySize(json_extra_services); i++) {
                cJSON* json_extra_service = cJSON_GetArrayItem(json_extra_services, i);
                total_services += get_service_recount(get_serv_type(json_extra_service), json_extra_service);
            }
        }
        
        return total_services;
    }
    
    unsigned int hk_total_ac = 1;
    int bridge_needed = false;
    
    for (unsigned int i = 0; i < total_accessories; i++) {
        cJSON* json_accessory = cJSON_GetArrayItem(json_accessories, i);
        if (acc_homekit_enabled(json_accessory) && get_serv_type(json_accessory) != SERV_TYPE_IAIRZONING) {
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
    int32_t service_numerator = 0;
    unsigned int acc_count = 0;
    
    // HomeKit last config number
    int32_t last_config_number = 1;
    sysparam_get_int32(LAST_CONFIG_NUMBER_SYSPARAM, &last_config_number);
    
    //unsigned int service_iid = 100;
    
    void new_accessory(const uint16_t accessory, uint16_t services, const bool homekit_enabled, cJSON* json_context) {
        if (!homekit_enabled) {
            return;
        }
        
        if (acc_count == 0) {
            services++;
        }
        
        if (acc_count == haa_setup_accessory) {
            services++;
        }
        
        char* serial_prefix = NULL;
        if (json_context && cJSON_GetObjectItemCaseSensitive(json_context, SERIAL_STRING) != NULL) {
            serial_prefix = cJSON_GetObjectItemCaseSensitive(json_context, SERIAL_STRING)->valuestring;
        } else {
            serial_prefix = common_serial_prefix;
        }
        
        unsigned int serial_str_len = SERIAL_STRING_LEN;
        int use_config_number = false;
        if (serial_prefix) {
            serial_str_len += strlen(serial_prefix) + 1;
            
            if (strcmp(serial_prefix, "cn") == 0) {
                serial_str_len += 6;
                use_config_number = true;
            }
        }
        
        char* serial_str = malloc(serial_str_len);
        uint8_t macaddr[6];
        sdk_wifi_get_macaddr(STATION_IF, macaddr);
        
        unsigned int serial_index = accessory + 1;
        if (bridge_needed) {
            serial_index--;
        }
        if (use_config_number) {
#ifdef ESP_PLATFORM
            snprintf(serial_str, serial_str_len, "%li-%02X%02X%02X-%i",
#else
            snprintf(serial_str, serial_str_len, "%i-%02X%02X%02X-%i",
#endif
                     last_config_number, macaddr[3], macaddr[4], macaddr[5], serial_index);
        } else {
            snprintf(serial_str, serial_str_len, "%s%s%02X%02X%02X-%i",
                     serial_prefix ? serial_prefix : "", serial_prefix ? "-" : "", macaddr[3], macaddr[4], macaddr[5], serial_index);
        }
        
        INFO("SN %s", serial_str);
        
        accessories[accessory] = calloc(1, sizeof(homekit_accessory_t));
        accessories[accessory]->id = accessory + 1;
        accessories[accessory]->services = calloc(services, sizeof(homekit_service_t*));
        
        accessories[accessory]->services[0] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[0]->id = 1;
        accessories[accessory]->services[0]->type = HOMEKIT_SERVICE_ACCESSORY_INFORMATION;
        accessories[accessory]->services[0]->characteristics = calloc(7, sizeof(homekit_characteristic_t*));
        accessories[accessory]->services[0]->characteristics[0] = &name;
        accessories[accessory]->services[0]->characteristics[1] = &manufacturer;
        accessories[accessory]->services[0]->characteristics[2] = NEW_HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER_STATIC, serial_str);
        accessories[accessory]->services[0]->characteristics[3] = &model;
        accessories[accessory]->services[0]->characteristics[4] = &firmware;
        accessories[accessory]->services[0]->characteristics[5] = &identify_function;
        
        services--;
        
        if (acc_count == haa_setup_accessory) {
            services--;
            accessories[accessory]->services[services] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[services]->id = 65010;
            accessories[accessory]->services[services]->hidden = true;
            accessories[accessory]->services[services]->type = HOMEKIT_SERVICE_CUSTOM_SETUP_OPTIONS;
            accessories[accessory]->services[services]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[services]->characteristics[0] = &haa_custom_setup_option;
        }
        
        if (acc_count == 0) {
            services--;
            accessories[accessory]->services[services] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[services]->id = 65000;
            accessories[accessory]->services[services]->hidden = true;
            accessories[accessory]->services[services]->type = HOMEKIT_SERVICE_HAP_INFORMATION;
            accessories[accessory]->services[services]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[services]->characteristics[0] = &hap_version;
        }
        
        //service_iid = 100;
    }
    
    void set_killswitch(ch_group_t* ch_group, cJSON* json_context) {
        if (cJSON_GetObjectItemCaseSensitive(json_context, KILLSWITCH) != NULL) {
            const unsigned int killswitch = cJSON_GetObjectItemCaseSensitive(json_context, KILLSWITCH)->valuefloat;
            ch_group->main_enabled = killswitch & 0b01;
            ch_group->child_enabled = killswitch & 0b10;
        }
        
        //INFO("<%i> KS 0b%i%i", ch_group->serv_index, ch_group->main_enabled, ch_group->child_enabled);
    }
    
    void set_accessory_ir_protocol(ch_group_t* ch_group, cJSON* json_context) {
        if (cJSON_GetObjectItemCaseSensitive(json_context, IRRF_ACTION_PROTOCOL) != NULL) {
            ch_group->ir_protocol = uni_strdup(cJSON_GetObjectItemCaseSensitive(json_context, IRRF_ACTION_PROTOCOL)->valuestring, &unistrings);
        }
    }
    
    bool get_exec_actions_on_boot(cJSON* json_context) {
        int exec_actions_on_boot = true;
        if (cJSON_GetObjectItemCaseSensitive(json_context, EXEC_ACTIONS_ON_BOOT) != NULL) {
            exec_actions_on_boot = (bool) cJSON_GetObjectItemCaseSensitive(json_context, EXEC_ACTIONS_ON_BOOT)->valuefloat;
        }
        
        //INFO("Run Ac Boot %i", exec_actions_on_boot);
        
        return exec_actions_on_boot;
    }
    
    uint8_t get_initial_state(cJSON* json_context) {
        unsigned int initial_state = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE) != NULL) {
            initial_state = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, INITIAL_STATE)->valuefloat;
        }
        
        //INFO("Get Init %i", initial_state);
        
        return initial_state;
    }
    
    cJSON* init_last_state_json = cJSON_Parse(INIT_STATE_LAST_STR);
    
    // Helpers to use common code
    void register_bool_inputs(cJSON* json_context, ch_group_t* ch_group, const bool exec_actions_on_boot) {
        int initial_state = 2;
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1)) {
            initial_state = 1;
        }
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0)) {
            ch_group->ch[0]->value.bool_value = true;
            initial_state = 0;
        }
        
        if (initial_state < 2) {
            if (exec_actions_on_boot) {
                diginput(99, ch_group, initial_state);
            } else {
                digstate(99, ch_group, initial_state);
            }
        }
        
        initial_state = 2;
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1)) {
            initial_state = 1;
        }
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0)) {
            ch_group->ch[0]->value.bool_value = true;
            initial_state = 0;
        }
        
        if (initial_state < 2) {
            digstate(99, ch_group, initial_state);
        }
    }
    
    // *** NEW SWITCH / OUTLET
    void new_switch(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context, const uint8_t serv_type) {
        uint32_t max_duration = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, VALVE_MAX_DURATION) != NULL) {
            max_duration = (uint32_t) cJSON_GetObjectItemCaseSensitive(json_context, VALVE_MAX_DURATION)->valuefloat;
        }
        
        unsigned int ch_calloc = 2;
        if (max_duration > 0) {
            ch_calloc += 2;
        }
        
        ch_group_t* ch_group = new_ch_group(ch_calloc - 1, 0, 0, 0);
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(ON, false, .setter_ex=hkc_on_setter);
        
        ch_group->serv_type = serv_type;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        AUTOOFF_TIMER = autoswitch_time(json_context, ch_group);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            accessories[accessory]->services[service]->characteristics = calloc(ch_calloc, sizeof(homekit_characteristic_t*));
            
            if (serv_type == SERV_TYPE_SWITCH) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_SWITCH;
            } else {    // serv_type == SERV_TYPE_OUTLET
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
            
            const uint32_t initial_time = (uint32_t) set_initial_state(ch_group->serv_index, 1, init_last_state_json, ch_group->ch[1], CH_TYPE_INT, max_duration);
            if (initial_time > max_duration) {
                ch_group->ch[1]->value.int_value = max_duration;
            } else {
                ch_group->ch[1]->value.int_value = initial_time;
            }
            
            ch_group->timer = rs_esp_timer_create(1000, true, (void*) ch_group, on_timer_worker);
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_1), digstate, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_0), digstate, ch_group, 0);
        
        const int exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0);
            
            ch_group->ch[0]->value.bool_value = !((bool) set_initial_state(ch_group->serv_index, 0, json_context, ch_group->ch[0], CH_TYPE_BOOL, 0));
            if (exec_actions_on_boot) {
                hkc_on_setter(ch_group->ch[0], HOMEKIT_BOOL(!ch_group->ch[0]->value.bool_value));
            } else {
                hkc_on_status_setter(ch_group->ch[0], HOMEKIT_BOOL(!ch_group->ch[0]->value.bool_value));
            }
        } else {
            register_bool_inputs(json_context, ch_group, exec_actions_on_boot);
        }
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_SWITCH;
            }
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW BUTTON EVENT / DOORBELL
    void new_button_event(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context, const uint8_t serv_type) {
        ch_group_t* ch_group = new_ch_group(1 + (serv_type == SERV_TYPE_DOORBELL), 0, 0, 0);
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;

        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;

        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(PROGRAMMABLE_SWITCH_EVENT, 0);
        
        ch_group->serv_type = serv_type;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        
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
            
            if (serv_type == SERV_TYPE_DOORBELL) {
                service++;
                
                ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(PROGRAMMABLE_SWITCH_EVENT, 1);
                
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
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), button_event_diginput, ch_group, SINGLEPRESS_EVENT);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), button_event_diginput, ch_group, DOUBLEPRESS_EVENT);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2), button_event_diginput, ch_group, LONGPRESS_EVENT);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), button_event_diginput, ch_group, SINGLEPRESS_EVENT);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), button_event_diginput, ch_group, DOUBLEPRESS_EVENT);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_2), button_event_diginput, ch_group, LONGPRESS_EVENT);
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW LOCK
    void new_lock(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(2, 0, 0, 0);
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(LOCK_CURRENT_STATE, 1);
        ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(LOCK_TARGET_STATE, 1, .setter_ex=hkc_lock_setter);
        
        ch_group->serv_type = SERV_TYPE_LOCK;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        AUTOOFF_TIMER = autoswitch_time(json_context, ch_group);
        
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
        
        const int exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0);
            
            ch_group->ch[1]->value.int_value = !((uint8_t) set_initial_state(ch_group->serv_index, 0, json_context, ch_group->ch[1], CH_TYPE_INT8, 1));
            if (exec_actions_on_boot) {
                hkc_lock_setter(ch_group->ch[1], HOMEKIT_UINT8(!ch_group->ch[1]->value.int_value));
            } else {
                hkc_lock_status_setter(ch_group->ch[1], HOMEKIT_UINT8(!ch_group->ch[1]->value.int_value));
            }
        } else {
            int initial_state = 2;
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0)) {
                initial_state = 0;
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1)) {
                ch_group->ch[1]->value.int_value = 0;
                initial_state = 1;
            }
            
            if (initial_state < 2) {
                if (exec_actions_on_boot) {
                    diginput(99, ch_group, initial_state);
                } else {
                    digstate(99, ch_group, initial_state);
                }
            }
            
            initial_state = 2;
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0)) {
                initial_state = 0;
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1)) {
                ch_group->ch[1]->value.int_value = 0;
                initial_state = 1;
            }
            
            if (initial_state < 2) {
                digstate(99, ch_group, initial_state);
            }
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW BINARY SENSOR
    void new_binary_sensor(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context, uint8_t serv_type) {
        ch_group_t* ch_group = new_ch_group(1, 0, 0, 0);
        ch_group->serv_type = SERV_TYPE_CONTACT_SENSOR;
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
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
        
        switch (serv_type) {
            case SERV_TYPE_OCCUPANCY_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_OCCUPANCY_SENSOR;
                }
                ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(OCCUPANCY_DETECTED, 0);
                break;
                
            case SERV_TYPE_LEAK_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_LEAK_SENSOR;
                }
                ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(LEAK_DETECTED, 0);
                break;
                
            case SERV_TYPE_SMOKE_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_SMOKE_SENSOR;
                }
                ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(SMOKE_DETECTED, 0);
                break;
                
            case SERV_TYPE_CARBON_MONOXIDE_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CARBON_MONOXIDE_SENSOR;
                }
                ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CARBON_MONOXIDE_DETECTED, 0);
                break;
                
            case SERV_TYPE_CARBON_DIOXIDE_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CARBON_DIOXIDE_SENSOR;
                }
                ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CARBON_DIOXIDE_DETECTED, 0);
                break;
                
            case SERV_TYPE_FILTER_CHANGE_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_FILTER_MAINTENANCE;
                }
                ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(FILTER_CHANGE_INDICATION, 0);
                break;
                
            case SERV_TYPE_MOTION_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_MOTION_SENSOR;
                }
                ch_group->serv_type = SERV_TYPE_MOTION_SENSOR;
                ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(MOTION_DETECTED, 0);
                break;
                
            default:    // case SERV_TYPE_CONTACT_SENSOR:
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CONTACT_SENSOR;
                }
                ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CONTACT_SENSOR_STATE, 0);
                break;
        }
        
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        AUTOOFF_TIMER = autoswitch_time(json_context, ch_group);

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
        
        const int exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        int initial_state = 4;
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), binary_sensor, ch_group, 0)) {
            if (serv_type == SERV_TYPE_MOTION_SENSOR) {
                ch_group->ch[0]->value.int_value = 1;
            } else {
                ch_group->ch[0]->value.bool_value = true;
            }
            
            initial_state = 0;
        }
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), binary_sensor, ch_group, 1)) {
            initial_state = 1;
        }
        
        if (initial_state < 4) {
            if (!exec_actions_on_boot) {
                initial_state += 2;
            }
            
            binary_sensor(99, ch_group, initial_state);
        }
        
        initial_state = 4;
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), binary_sensor, ch_group, 2)) {
            if (serv_type == SERV_TYPE_MOTION_SENSOR) {
                ch_group->ch[0]->value.int_value = 1;
            } else {
                ch_group->ch[0]->value.bool_value = true;
            }
            
            initial_state = 2;
        }
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), binary_sensor, ch_group, 3)) {
            initial_state = 3;
        }
        
        if (initial_state < 4) {
            binary_sensor(99, ch_group, initial_state);
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW AIR QUALITY
    void new_air_quality(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        size_t extra_data_size = 0;
        cJSON* json_extra_data = NULL;
        if (cJSON_GetObjectItemCaseSensitive(json_context, AQ_EXTRA_DATA_ARRAY_SET) != NULL) {
            json_extra_data = cJSON_GetObjectItemCaseSensitive(json_context, AQ_EXTRA_DATA_ARRAY_SET);
            extra_data_size = cJSON_GetArraySize(json_extra_data);
        }
        
        ch_group_t* ch_group = new_ch_group(1 + extra_data_size, 0, 0, 0);
        ch_group->serv_type = SERV_TYPE_AIR_QUALITY;
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(AIR_QUALITY, 0);
        
        for (unsigned int i = 1; i <= extra_data_size; i++) {
            const unsigned int extra_data_type = (uint8_t) cJSON_GetArrayItem(json_extra_data, i - 1)->valuefloat;
            switch (extra_data_type) {
                case AQ_OZONE_DENSITY:
                    ch_group->ch[i] = NEW_HOMEKIT_CHARACTERISTIC(OZONE_DENSITY, 0);
                    break;
                    
                case AQ_NITROGEN_DIOXIDE_DENSITY:
                    ch_group->ch[i] = NEW_HOMEKIT_CHARACTERISTIC(NITROGEN_DIOXIDE_DENSITY, 0);
                    break;
                    
                case AQ_SULPHUR_DIOXIDE_DENSITY:
                    ch_group->ch[i] = NEW_HOMEKIT_CHARACTERISTIC(SULPHUR_DIOXIDE_DENSITY, 0);
                    break;
                    
                case AQ_PM25_DENSITY:
                    ch_group->ch[i] = NEW_HOMEKIT_CHARACTERISTIC(PM25_DENSITY, 0);
                    break;
                    
                case AQ_PM10_DENSITY:
                    ch_group->ch[i] = NEW_HOMEKIT_CHARACTERISTIC(PM10_DENSITY, 0);
                    break;
                    
                default:    // case AQ_VOC_DENSITY:
                    ch_group->ch[i] = NEW_HOMEKIT_CHARACTERISTIC(VOC_DENSITY, 0);
                    break;
            }
        }
        
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_AIR_QUALITY_SENSOR;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_AIR_QUALITY_SENSOR;
            }
            
            accessories[accessory]->services[service]->characteristics = calloc(2 + extra_data_size, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
            
            for (unsigned int i = 1; i <= extra_data_size; i++) {
                accessories[accessory]->services[service]->characteristics[i] = ch_group->ch[i];
            }
            
            //service_iid += (2 + extra_data_size);
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW WATER VALVE
    void new_valve(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        uint32_t valve_max_duration = VALVE_MAX_DURATION_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, VALVE_MAX_DURATION) != NULL) {
            valve_max_duration = (uint32_t) cJSON_GetObjectItemCaseSensitive(json_context, VALVE_MAX_DURATION)->valuefloat;
        }
        
        unsigned int calloc_count = 4;
        if (valve_max_duration > 0) {
            calloc_count += 2;
        }
        
        ch_group_t* ch_group = new_ch_group(calloc_count - 2, 0, 0, 0);
        ch_group->serv_type = SERV_TYPE_WATER_VALVE;
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        unsigned int valve_type = VALVE_SYSTEM_TYPE_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, VALVE_SYSTEM_TYPE) != NULL) {
            valve_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, VALVE_SYSTEM_TYPE)->valuefloat;
        }
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(ACTIVE, 0, .setter_ex=hkc_valve_setter);
        ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(IN_USE, 0);

        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        AUTOOFF_TIMER = autoswitch_time(json_context, ch_group);
        
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
            
            const uint32_t initial_time = (uint32_t) set_initial_state(ch_group->serv_index, 2, init_last_state_json, ch_group->ch[2], CH_TYPE_INT, valve_max_duration);
            if (initial_time > valve_max_duration) {
                ch_group->ch[2]->value.int_value = valve_max_duration;
            } else {
                ch_group->ch[2]->value.int_value = initial_time;
            }
            
            ch_group->timer = rs_esp_timer_create(1000, true, (void*) ch_group, valve_timer_worker);
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
        
        const int exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0);
            
            ch_group->ch[0]->value.int_value = !((uint8_t) set_initial_state(ch_group->serv_index, 0, json_context, ch_group->ch[0], CH_TYPE_INT8, 0));
            if (exec_actions_on_boot) {
                hkc_valve_setter(ch_group->ch[0], HOMEKIT_UINT8(!ch_group->ch[0]->value.int_value));
            } else {
                hkc_valve_status_setter(ch_group->ch[0], HOMEKIT_UINT8(!ch_group->ch[0]->value.int_value));
            }
            
        } else {
            int initial_state = 2;
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1)) {
                initial_state = 1;
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0)) {
                ch_group->ch[0]->value.int_value = 1;
                initial_state = 0;
            }
            
            if (initial_state < 2) {
                if (exec_actions_on_boot) {
                    diginput(99, ch_group, initial_state);
                } else {
                    digstate(99, ch_group, initial_state);
                }
            }
            
            initial_state = 2;
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1)) {
                initial_state = 1;
            }
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0)) {
                ch_group->ch[0]->value.int_value = 1;
                initial_state = 0;
            }
            
            if (initial_state < 2) {
                digstate(99, ch_group, initial_state);
            }
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW THERMOSTAT
    void new_thermostat(const uint8_t accessory,  uint8_t service, const uint8_t total_services, cJSON* json_context, const uint8_t serv_type) {
        ch_group_t* ch_group = new_ch_group(7, 8, 5, 4);
        ch_group->serv_type = SERV_TYPE_THERMOSTAT;
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        THERMOSTAT_CURRENT_ACTION = -1;
        TH_IAIRZONING_GATE_CURRENT_STATE = -1;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        // Custom ranges of Target Temperatures
        float min_temp = THERMOSTAT_DEFAULT_MIN_TEMP;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MIN_TEMP) != NULL) {
            min_temp = (float) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MIN_TEMP)->valuefloat;
        }
        
        float max_temp = THERMOSTAT_DEFAULT_MAX_TEMP;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MAX_TEMP) != NULL) {
            max_temp = (float) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_MAX_TEMP)->valuefloat;
        }
        
        const float default_target_temp = (min_temp + max_temp) / 2;
        
        // Temperature Deadbands
        TH_DEADBAND = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND) != NULL) {
            TH_DEADBAND = cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND)->valuefloat / 2.000f;
        }
        
        TH_DEADBAND_FORCE_IDLE = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND_FORCE_IDLE) != NULL) {
            TH_DEADBAND_FORCE_IDLE = cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND_FORCE_IDLE)->valuefloat;
        }
        
        TH_DEADBAND_SOFT_ON = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND_SOFT_ON) != NULL) {
            TH_DEADBAND_SOFT_ON = cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_DEADBAND_SOFT_ON)->valuefloat;
        }
        
        // Thermostat Type
        unsigned int th_type = THERMOSTAT_TYPE_HEATER;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_TYPE) != NULL) {
            th_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_TYPE)->valuefloat;
        }
        
        TH_TYPE = th_type;
        
        // HomeKit Characteristics
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_TEMPERATURE, TH_SENSOR_TEMP_VALUE_WHEN_ERROR);
        ch_group->ch[2] = NEW_HOMEKIT_CHARACTERISTIC(ACTIVE, 0, .setter_ex=hkc_th_setter);
        ch_group->ch[3] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_HEATER_COOLER_STATE, 0);
        
        float temp_step = 0.1;
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_TARGET_TEMP_STEP) != NULL) {
            temp_step = cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_TARGET_TEMP_STEP)->valuefloat;
        }
        
        if (th_type != THERMOSTAT_TYPE_COOLER) {
            ch_group->ch[5] = NEW_HOMEKIT_CHARACTERISTIC(HEATING_THRESHOLD_TEMPERATURE, default_target_temp - 1, .min_value=(float[]) {min_temp}, .max_value=(float[]) {max_temp}, .min_step=(float[]) {temp_step}, .setter_ex=hkc_th_setter);
            ch_group->ch[5]->value.float_value = set_initial_state(ch_group->serv_index, 5, init_last_state_json, ch_group->ch[5], CH_TYPE_FLOAT, default_target_temp - 1);
        }
        
        if (th_type != THERMOSTAT_TYPE_HEATER) {
            ch_group->ch[6] = NEW_HOMEKIT_CHARACTERISTIC(COOLING_THRESHOLD_TEMPERATURE, default_target_temp + 1, .min_value=(float[]) {min_temp}, .max_value=(float[]) {max_temp}, .min_step=(float[]) {temp_step}, .setter_ex=hkc_th_setter);
            ch_group->ch[6]->value.float_value = set_initial_state(ch_group->serv_index, 6, init_last_state_json, ch_group->ch[6], CH_TYPE_FLOAT, default_target_temp + 1);
        }
        
        if (ch_group->homekit_enabled) {
            unsigned int calloc_count = 6;
            if (th_type >= THERMOSTAT_TYPE_HEATERCOOLER) {
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
        
        switch (th_type) {
            case THERMOSTAT_TYPE_COOLER:
                ch_group->ch[4] = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HEATER_COOLER_STATE, THERMOSTAT_TARGET_MODE_COOLER, .min_value=(float[]) {THERMOSTAT_TARGET_MODE_COOLER}, .max_value=(float[]) {THERMOSTAT_TARGET_MODE_COOLER}, .valid_values={.count=1, .values=(uint8_t[]) {THERMOSTAT_TARGET_MODE_COOLER}});
                
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->characteristics[4] = ch_group->ch[6];
                }
                break;
                
            case THERMOSTAT_TYPE_HEATERCOOLER:
                ch_group->ch[4] = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HEATER_COOLER_STATE, THERMOSTAT_TARGET_MODE_AUTO, .setter_ex=hkc_th_setter);
                
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->characteristics[4] = ch_group->ch[5];
                    accessories[accessory]->services[service]->characteristics[5] = ch_group->ch[6];
                    //service_iid++;
                }
                break;
                
            case THERMOSTAT_TYPE_HEATERCOOLER_NOAUTO:
                ch_group->ch[4] = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HEATER_COOLER_STATE, THERMOSTAT_TARGET_MODE_COOLER, .min_value=(float[]) {THERMOSTAT_TARGET_MODE_HEATER}, .max_value=(float[]) {THERMOSTAT_TARGET_MODE_COOLER}, .valid_values={.count=2, .values=(uint8_t[]) {THERMOSTAT_TARGET_MODE_HEATER, THERMOSTAT_TARGET_MODE_COOLER}}, .setter_ex=hkc_th_setter);
                
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
        
        if (serv_type == SERV_TYPE_THERMOSTAT_WITH_HUM) {
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
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_IAIRZONING_CONTROLLER) != NULL) {
            TH_IAIRZONING_CONTROLLER = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_IAIRZONING_CONTROLLER)->valuefloat;
        }
        
        ch_group->timer2 = rs_esp_timer_create(th_update_delay(json_context) * 1000, false, (void*) ch_group, process_th_timer);
        
        const int sensor_gpio = TH_SENSOR_GPIO;
        
        if ((sensor_gpio != -1 || TH_SENSOR_TYPE >= 5) && TH_IAIRZONING_CONTROLLER == 0) {
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
        
        if (th_type >= THERMOSTAT_TYPE_HEATERCOOLER) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_5), th_input, ch_group, 5);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_6), th_input, ch_group, 6);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_5), th_input, ch_group, 5);
            ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_6), th_input, ch_group, 6);
            
            if (th_type == THERMOSTAT_TYPE_HEATERCOOLER) {
                diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_7), th_input, ch_group, 7);
                ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_7), th_input, ch_group, 7);
            }
            
            int initial_target_mode = THERMOSTAT_INIT_TARGET_MODE_DEFAULT;
            if (cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_INIT_TARGET_MODE) != NULL) {
                initial_target_mode = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, THERMOSTAT_INIT_TARGET_MODE)->valuefloat;
            }
            
            if (initial_target_mode == THERMOSTAT_INIT_TARGET_MODE_DEFAULT) {
                ch_group->ch[4]->value.int_value = set_initial_state(ch_group->serv_index, 4, init_last_state_json, ch_group->ch[4], CH_TYPE_INT8, ch_group->ch[4]->value.int_value);
            } else {
                ch_group->ch[4]->value.int_value = initial_target_mode;
            }
        }
        
        const int exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), th_input, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), th_input, ch_group, 0);
            
            ch_group->ch[2]->value.int_value = !((uint8_t) set_initial_state(ch_group->serv_index, 2, json_context, ch_group->ch[2], CH_TYPE_INT8, 0));
            
            if (exec_actions_on_boot) {
                hkc_th_setter(ch_group->ch[2], HOMEKIT_UINT8(!ch_group->ch[2]->value.int_value));
            } else {
                ch_group->ch[2]->value.int_value = !ch_group->ch[2]->value.int_value;
            }
        } else {
            int initial_state = 2;
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), th_input, ch_group, 1)) {
                initial_state = 1;
            }
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), th_input, ch_group, 0)) {
                ch_group->ch[2]->value.int_value = 1;
                initial_state = 0;
            }
            
            if (initial_state < 2) {
                if (exec_actions_on_boot) {
                    th_input(99, ch_group, initial_state);
                } else {
                    ch_group->ch[2]->value.int_value = !ch_group->ch[2]->value.int_value;
                }
            }
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW IAIRZONING
    void new_iairzoning(cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(0, 2, 2, 0);
        ch_group->serv_index = service_numerator;
        ch_group->serv_type = SERV_TYPE_IAIRZONING;
        ch_group->num_i[0] = -1;    // IAIRZONING_LAST_ACTION
        ch_group->num_i[1] = -1;    // IAIRZONING_MAIN_MODE
        
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        
        IAIRZONING_DELAY_ACTION_CH_GROUP = IAIRZONING_DELAY_ACTION_DEFAULT * MS_TO_TICKS(1000);
        if (cJSON_GetObjectItemCaseSensitive(json_context, IAIRZONING_DELAY_ACTION_SET) != NULL) {
            IAIRZONING_DELAY_ACTION_CH_GROUP = cJSON_GetObjectItemCaseSensitive(json_context, IAIRZONING_DELAY_ACTION_SET)->valuefloat * MS_TO_TICKS(1000);
        }
        
        IAIRZONING_DELAY_AFT_CLOSE_CH_GROUP = IAIRZONING_DELAY_AFT_CLOSE_DEFAULT * MS_TO_TICKS(1000);
        if (cJSON_GetObjectItemCaseSensitive(json_context, IAIRZONING_DELAY_AFT_CLOSE_SET) != NULL) {
            IAIRZONING_DELAY_AFT_CLOSE_CH_GROUP = cJSON_GetObjectItemCaseSensitive(json_context, IAIRZONING_DELAY_AFT_CLOSE_SET)->valuefloat * MS_TO_TICKS(1000);
        }
        
        ch_group->timer2 = rs_esp_timer_create(th_update_delay(json_context) * 1000, false, (void*) ch_group, set_zones_timer_worker);
        
        th_sensor_starter(ch_group, sensor_poll_period(json_context, TH_SENSOR_POLL_PERIOD_DEFAULT));
    }
    
    // *** NEW TEMPERATURE SENSOR
    void new_temp_sensor(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(1, 4, 2, 1);
        ch_group->serv_type = SERV_TYPE_TEMP_SENSOR;
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_TEMPERATURE, TH_SENSOR_TEMP_VALUE_WHEN_ERROR);
  
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
        
        const int sensor_gpio = TH_SENSOR_GPIO;
        
        if (sensor_gpio != -1 || TH_SENSOR_TYPE >= 5) {
            th_sensor_starter(ch_group, poll_period);
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW HUMIDITY SENSOR
    void new_hum_sensor(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(2, 4, 2, 2);
        ch_group->serv_type = SERV_TYPE_HUM_SENSOR;
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
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
        
        const int sensor_gpio = TH_SENSOR_GPIO;
        
        if (sensor_gpio != -1 || TH_SENSOR_TYPE >= 9) {
            th_sensor_starter(ch_group, poll_period);
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW TEMPERATURE AND HUMIDITY SENSOR
    void new_th_sensor(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(2, 4, 2, 2);
        ch_group->serv_type = SERV_TYPE_TH_SENSOR;
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_TEMPERATURE, TH_SENSOR_TEMP_VALUE_WHEN_ERROR);
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
        
        const int sensor_gpio = TH_SENSOR_GPIO;
        if (sensor_gpio != -1) {
            th_sensor_starter(ch_group, poll_period);
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW HUMIDIFIER
    void new_humidifier(const uint8_t accessory,  uint8_t service, const uint8_t total_services, cJSON* json_context, const uint8_t serv_type) {
        ch_group_t* ch_group = new_ch_group(7, 6, 4, 4);
        ch_group->serv_type = SERV_TYPE_HUMIDIFIER;
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        HUMIDIF_CURRENT_ACTION = -1;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        // Humidity Deadbands
        HM_DEADBAND = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_DEADBAND) != NULL) {
            HM_DEADBAND = cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_DEADBAND)->valuefloat / 2.000f;
        }
        
        HM_DEADBAND_FORCE_IDLE = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_DEADBAND_FORCE_IDLE) != NULL) {
            HM_DEADBAND_FORCE_IDLE = cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_DEADBAND_FORCE_IDLE)->valuefloat;
        }
        
        HM_DEADBAND_SOFT_ON = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_DEADBAND_SOFT_ON) != NULL) {
            HM_DEADBAND_SOFT_ON = cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_DEADBAND_SOFT_ON)->valuefloat;
        }
        
        // Humidifier Type
        int hm_type = HUMIDIF_TYPE_HUM;
        if (cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_TYPE) != NULL) {
            hm_type = cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_TYPE)->valuefloat;
        }
        
        HM_TYPE = hm_type;
        
        // HomeKit Characteristics
        ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_RELATIVE_HUMIDITY, 0);
        ch_group->ch[2] = NEW_HOMEKIT_CHARACTERISTIC(ACTIVE, 0, .setter_ex=hkc_humidif_setter);
        ch_group->ch[3] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_HUMIDIFIER_DEHUMIDIFIER_STATE, 0);
        
        if (hm_type != HUMIDIF_TYPE_DEHUM) {
            ch_group->ch[5] = NEW_HOMEKIT_CHARACTERISTIC(RELATIVE_HUMIDITY_HUMIDIFIER_THRESHOLD, 40, .setter_ex=hkc_humidif_setter);
            ch_group->ch[5]->value.float_value = set_initial_state(ch_group->serv_index, 5, init_last_state_json, ch_group->ch[5], CH_TYPE_FLOAT, 40);
        }
        
        if (hm_type != HUMIDIF_TYPE_HUM) {
            ch_group->ch[6] = NEW_HOMEKIT_CHARACTERISTIC(RELATIVE_HUMIDITY_DEHUMIDIFIER_THRESHOLD, 60, .setter_ex=hkc_humidif_setter);
            ch_group->ch[6]->value.float_value = set_initial_state(ch_group->serv_index, 6, init_last_state_json, ch_group->ch[6], CH_TYPE_FLOAT, 60);
        }
        
        
        if (ch_group->homekit_enabled) {
            int calloc_count = 6;
            if (hm_type >= HUMIDIF_TYPE_HUMDEHUM) {
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
        
        switch (hm_type) {
            case HUMIDIF_TYPE_DEHUM:
                ch_group->ch[4] = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HUMIDIFIER_DEHUMIDIFIER_STATE, HUMIDIF_TARGET_MODE_DEHUM, .min_value=(float[]) {HUMIDIF_TARGET_MODE_DEHUM}, .max_value=(float[]) {HUMIDIF_TARGET_MODE_DEHUM}, .valid_values={.count=1, .values=(uint8_t[]) {HUMIDIF_TARGET_MODE_DEHUM}});
                
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->characteristics[4] = ch_group->ch[6];
                }
                break;
                
            case HUMIDIF_TYPE_HUMDEHUM:
                ch_group->ch[4] = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HUMIDIFIER_DEHUMIDIFIER_STATE, HUMIDIF_TARGET_MODE_AUTO, .setter_ex=hkc_humidif_setter);
                
                if (ch_group->homekit_enabled) {
                    accessories[accessory]->services[service]->characteristics[4] = ch_group->ch[5];
                    accessories[accessory]->services[service]->characteristics[5] = ch_group->ch[6];
                    //service_iid++;
                }
                break;
                
            case HUMIDIF_TYPE_HUMDEHUM_NOAUTO:
                ch_group->ch[4] = NEW_HOMEKIT_CHARACTERISTIC(TARGET_HUMIDIFIER_DEHUMIDIFIER_STATE, HUMIDIF_TARGET_MODE_HUM, .min_value=(float[]) {HUMIDIF_TARGET_MODE_HUM}, .max_value=(float[]) {HUMIDIF_TARGET_MODE_DEHUM}, .valid_values={.count=2, .values=(uint8_t[]) {HUMIDIF_TARGET_MODE_HUM, HUMIDIF_TARGET_MODE_DEHUM}}, .setter_ex=hkc_humidif_setter);
                
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
        
        if (serv_type == SERV_TYPE_HUMIDIFIER_WITH_TEMP) {
            service++;
            
            ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CURRENT_TEMPERATURE, TH_SENSOR_TEMP_VALUE_WHEN_ERROR);
            
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
        
        ch_group->timer2 = rs_esp_timer_create(th_update_delay(json_context) * 1000, false, (void*) ch_group, process_humidif_timer);
        
        const int sensor_gpio = TH_SENSOR_GPIO;
        
        if (sensor_gpio != -1 || TH_SENSOR_TYPE >= 9) {
            th_sensor_starter(ch_group, poll_period);
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), humidif_input, ch_group, 9);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3), humidif_input_hum, ch_group, HUMIDIF_UP);
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_4), humidif_input_hum, ch_group, HUMIDIF_DOWN);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), humidif_input, ch_group, 9);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_3), humidif_input_hum, ch_group, HUMIDIF_UP);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_4), humidif_input_hum, ch_group, HUMIDIF_DOWN);
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
            
            int initial_target_mode = HUMIDIF_INIT_TARGET_MODE_DEFAULT;
            if (cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_INIT_TARGET_MODE) != NULL) {
                initial_target_mode = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, HUMIDIF_INIT_TARGET_MODE)->valuefloat;
            }
            
            if (initial_target_mode == HUMIDIF_INIT_TARGET_MODE_DEFAULT) {
                ch_group->ch[4]->value.int_value = set_initial_state(ch_group->serv_index, 4, init_last_state_json, ch_group->ch[4], CH_TYPE_INT8, ch_group->ch[4]->value.int_value);
            } else {
                ch_group->ch[4]->value.int_value = initial_target_mode;
            }
        }
        
        const int exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), humidif_input, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), humidif_input, ch_group, 0);
            
            ch_group->ch[2]->value.int_value = !((uint8_t) set_initial_state(ch_group->serv_index, 2, json_context, ch_group->ch[2], CH_TYPE_INT8, 0));
            
            if (exec_actions_on_boot) {
                hkc_humidif_setter(ch_group->ch[2], HOMEKIT_UINT8(!ch_group->ch[2]->value.int_value));
            } else {
                ch_group->ch[2]->value.int_value = !ch_group->ch[2]->value.int_value;
            }
        } else {
            int initial_state = 2;
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), humidif_input, ch_group, 1)) {
                initial_state = 1;
            }
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), humidif_input, ch_group, 0)) {
                ch_group->ch[2]->value.int_value = 1;
                initial_state = 0;
            }
            
            if (initial_state < 2) {
                if (exec_actions_on_boot) {
                    humidif_input(99, ch_group, initial_state);
                } else {
                    ch_group->ch[2]->value.int_value = !ch_group->ch[2]->value.int_value;
                }
            }
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW LIGHTBULB
    void new_lightbulb(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(4, 0, 2, 1);
        ch_group->serv_type = SERV_TYPE_LIGHTBULB;
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
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
        AUTOOFF_TIMER = autoswitch_time(json_context, ch_group);
        
        lightbulb_group_t* lightbulb_group = malloc(sizeof(lightbulb_group_t));
        memset(lightbulb_group, 0, sizeof(*lightbulb_group));
        
        lightbulb_group->ch0 = ch_group->ch[0];
        
        for (int i = 0; i < 5; i++) {
            lightbulb_group->gpio[i] = MAX_GPIOS;
            lightbulb_group->flux[i] = 1;
            lightbulb_group->current[i] = 1;
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
        lightbulb_group->wp[0] = 0.34567;   // D50
        lightbulb_group->wp[1] = 0.35850;
        lightbulb_group->rgb[0][0] = lightbulb_group->r[0]; // Default to the LED RGB coordinates
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
        
        LIGHTBULB_SET_DELAY_TIMER = rs_esp_timer_create(LIGHTBULB_SET_DELAY_MS, false, (void*) ch_group, lightbulb_task_timer);
        
        LIGHTBULB_TYPE = LIGHTBULB_TYPE_PWM;
        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_TYPE_SET) != NULL) {
            LIGHTBULB_TYPE = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_TYPE_SET)->valuefloat;
        }
        
        cJSON* gpio_array = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_GPIO_ARRAY_SET);
        if (!gpio_array) {
            LIGHTBULB_TYPE = LIGHTBULB_TYPE_VIRTUAL;
            LIGHTBULB_CHANNELS = 1;
        } else {
            LIGHTBULB_CHANNELS = 3;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_CHANNELS_SET) != NULL) {
            LIGHTBULB_CHANNELS = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_CHANNELS_SET)->valuefloat;
        }
        
        if (LIGHTBULB_TYPE != LIGHTBULB_TYPE_VIRTUAL) {
            if (!main_config.set_lightbulb_timer) {
                main_config.set_lightbulb_timer = rs_esp_timer_create(RGBW_PERIOD, true, NULL, rgbw_set_timer_worker);
            }
        }
        
        if (LIGHTBULB_TYPE == LIGHTBULB_TYPE_PWM || LIGHTBULB_TYPE == LIGHTBULB_TYPE_PWM_CWWW) {
            LIGHTBULB_CHANNELS = cJSON_GetArraySize(gpio_array);
            for (int i = 0; i < LIGHTBULB_CHANNELS; i++) {
                lightbulb_group->gpio[i] = (uint8_t) cJSON_GetArrayItem(gpio_array, i)->valuefloat;
            }
            
        /*
        } else if (LIGHTBULB_TYPE == LIGHTBULB_TYPE_SM16716) {
            lightbulb_group->gpio[0] = cJSON_GetArrayItem(gpio_array, 0)->valuefloat;
            lightbulb_group->gpio[1] = cJSON_GetArrayItem(gpio_array, 1)->valuefloat;
            
            //
            // TO-DO
            //
        */
            
#ifndef CONFIG_IDF_TARGET_ESP32C2
        } else if (LIGHTBULB_TYPE == LIGHTBULB_TYPE_NRZ) {
            lightbulb_group->gpio[0] = cJSON_GetArrayItem(gpio_array, 0)->valuefloat;
            //set_used_gpio(lightbulb_group->gpio[0]);
            
            LIGHTBULB_RANGE_START = cJSON_GetArrayItem(gpio_array, 1)->valuefloat;
            LIGHTBULB_RANGE_START -= 1;
            LIGHTBULB_RANGE_START = LIGHTBULB_RANGE_START * LIGHTBULB_CHANNELS;
            LIGHTBULB_RANGE_END = cJSON_GetArrayItem(gpio_array, 2)->valuefloat;
            LIGHTBULB_RANGE_END = LIGHTBULB_RANGE_END * LIGHTBULB_CHANNELS;
            
            float nrz_time[3] = { 0.4, 0.8, 0.85 };
            
            cJSON* nrz_times = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_NRZ_TIMES_ARRAY_SET);
            if (nrz_times) {
                for (unsigned int n = 0; n < 3; n++) {
                    nrz_time[n] = cJSON_GetArrayItem(nrz_times, n)->valuefloat;
                }
            }
            
            addressled_t* addressled = new_addressled(lightbulb_group->gpio[0], LIGHTBULB_RANGE_END, nrz_time[0], nrz_time[1], nrz_time[2]);
            
            cJSON* color_map = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_COLOR_MAP_SET);
            if (color_map) {
                const unsigned int size = cJSON_GetArraySize(color_map);
                for (unsigned int i = 0; i < size; i++) {
                    addressled->map[i] = cJSON_GetArrayItem(color_map, i)->valuefloat;
                }
            }
#endif  // no CONFIG_IDF_TARGET_ESP32C2
            
        }
        
        INFO("Channels %i Type %i", LIGHTBULB_CHANNELS, LIGHTBULB_TYPE);
        
        int is_custom_initial = false;
        uint16_t custom_initial[3];
        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_INITITAL_STATE_ARRAY_SET) != NULL) {
            is_custom_initial = true;
            cJSON* init_values_array = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_INITITAL_STATE_ARRAY_SET);
            for (int i = 0; i < cJSON_GetArraySize(init_values_array); i++) {
                custom_initial[i] = (uint16_t) cJSON_GetArrayItem(init_values_array, i)->valuefloat;
            }
        }
        
        if (LIGHTBULB_TYPE != LIGHTBULB_TYPE_VIRTUAL) {
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_MAX_POWER_SET) != NULL) {
                LIGHTBULB_MAX_POWER = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_MAX_POWER_SET)->valuefloat;
            }
            
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_CURVE_FACTOR_SET) != NULL) {
                LIGHTBULB_CURVE_FACTOR = (float) cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_CURVE_FACTOR_SET)->valuefloat;
            }
            
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FLUX_ARRAY_SET) != NULL) {
                cJSON* flux_array = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_FLUX_ARRAY_SET);
                for (int i = 0; i < LIGHTBULB_CHANNELS; i++) {
                    lightbulb_group->flux[i] = (float) cJSON_GetArrayItem(flux_array, i)->valuefloat;
                }
            }
            
            //INFO("Flux %g, %g, %g, %g, %g", lightbulb_group->flux[0], lightbulb_group->flux[1], lightbulb_group->flux[2], lightbulb_group->flux[3], lightbulb_group->flux[4]);
            
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_RGB_ARRAY_SET) != NULL) {
                cJSON* rgb_array = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_RGB_ARRAY_SET);
                for (int i = 0; i < 6; i++) {
                    lightbulb_group->rgb[i >> 1][i % 2] = (float) cJSON_GetArrayItem(rgb_array, i)->valuefloat;
                }
            }
            
            //INFO("Target RGB %g, %g, %g, %g, %g, %g", lightbulb_group->rgb[0][0], lightbulb_group->rgb[0][1], lightbulb_group->rgb[1][0], lightbulb_group->rgb[1][1], lightbulb_group->rgb[2][0], lightbulb_group->rgb[2][1]);
            
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_CMY_ARRAY_SET) != NULL) {
                cJSON* cmy_array = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_CMY_ARRAY_SET);
                for (int i = 0; i < 6; i++) {
                    lightbulb_group->cmy[i >> 1][i % 2] = (float) cJSON_GetArrayItem(cmy_array, i)->valuefloat;
                }
            }
            
            //INFO("Target CMY %g, %g, %g, %g, %g, %g", lightbulb_group->cmy[0][0], lightbulb_group->cmy[0][1], lightbulb_group->cmy[1][0], lightbulb_group->cmy[1][1], lightbulb_group->cmy[2][0], lightbulb_group->cmy[2][1]);
            
            //INFO("CMY [%g, %g, %g], [%g, %g, %g]", lightbulb_group->cmy[0][0], lightbulb_group->cmy[0][1], lightbulb_group->cmy[0][2], lightbulb_group->cmy[1][0], lightbulb_group->cmy[1][1], lightbulb_group->cmy[1][2]);
            
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_COORDINATE_ARRAY_SET) != NULL) {
                cJSON* coordinate_array = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_COORDINATE_ARRAY_SET);
                lightbulb_group->r[0] = (float) cJSON_GetArrayItem(coordinate_array, 0)->valuefloat;
                lightbulb_group->r[1] = (float) cJSON_GetArrayItem(coordinate_array, 1)->valuefloat;
                lightbulb_group->g[0] = (float) cJSON_GetArrayItem(coordinate_array, 2)->valuefloat;
                lightbulb_group->g[1] = (float) cJSON_GetArrayItem(coordinate_array, 3)->valuefloat;
                lightbulb_group->b[0] = (float) cJSON_GetArrayItem(coordinate_array, 4)->valuefloat;
                lightbulb_group->b[1] = (float) cJSON_GetArrayItem(coordinate_array, 5)->valuefloat;
                
                if (LIGHTBULB_CHANNELS >= 4) {
                    lightbulb_group->cw[0] = (float) cJSON_GetArrayItem(coordinate_array, 6)->valuefloat;
                    lightbulb_group->cw[1] = (float) cJSON_GetArrayItem(coordinate_array, 7)->valuefloat;
                }
                
                if (LIGHTBULB_CHANNELS == 5) {
                    lightbulb_group->ww[0] = (float) cJSON_GetArrayItem(coordinate_array, 8)->valuefloat;
                    lightbulb_group->ww[1] = (float) cJSON_GetArrayItem(coordinate_array, 9)->valuefloat;
                }
            }
            /*
            INFO("Coordinate [%g, %g], [%g, %g], [%g, %g], [%g, %g], [%g, %g]", lightbulb_group->r[0], lightbulb_group->r[1],
                                                                                        lightbulb_group->g[0], lightbulb_group->g[1],
                                                                                        lightbulb_group->b[0], lightbulb_group->b[1],
                                                                                        lightbulb_group->cw[0], lightbulb_group->cw[1],
                                                                                        lightbulb_group->ww[0], lightbulb_group->ww[1]);
            */
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_WHITE_POINT_SET) != NULL) {
                cJSON* white_point = cJSON_GetObjectItemCaseSensitive(json_context, LIGHTBULB_WHITE_POINT_SET);
                lightbulb_group->wp[0] = (float) cJSON_GetArrayItem(white_point, 0)->valuefloat;
                lightbulb_group->wp[1] = (float) cJSON_GetArrayItem(white_point, 1)->valuefloat;
            }
            
            //INFO("White point [%g, %g]", lightbulb_group->wp[0], lightbulb_group->wp[1]);
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, RGBW_STEP_SET) != NULL) {
            lightbulb_group->step = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_context, RGBW_STEP_SET)->valuefloat;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, AUTODIMMER_TASK_DELAY_SET) != NULL) {
            lightbulb_group->autodimmer_task_delay = cJSON_GetObjectItemCaseSensitive(json_context, AUTODIMMER_TASK_DELAY_SET)->valuefloat * MS_TO_TICKS(1000);
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, AUTODIMMER_TASK_STEP_SET) != NULL) {
            lightbulb_group->autodimmer_task_step = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, AUTODIMMER_TASK_STEP_SET)->valuefloat;
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
        
        int calloc_count = 3;

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
                ch_group->ch[2]->value.float_value = set_initial_state(ch_group->serv_index, 2, init_last_state_json, ch_group->ch[2], CH_TYPE_FLOAT, 0);
                ch_group->ch[3]->value.float_value = set_initial_state(ch_group->serv_index, 3, init_last_state_json, ch_group->ch[3], CH_TYPE_FLOAT, 0);
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
                ch_group->ch[2]->value.int_value = set_initial_state(ch_group->serv_index, 2, init_last_state_json, ch_group->ch[2], CH_TYPE_INT, 152);
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
            ch_group->ch[1]->value.int_value = set_initial_state(ch_group->serv_index, 1, init_last_state_json, ch_group->ch[1], CH_TYPE_INT8, 100);
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
            LIGHTBULB_AUTODIMMER_TIMER = rs_esp_timer_create(AUTODIMMER_DELAY, false, (void*) ch_group->ch[0], no_autodimmer_called);
        }
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0);
            
            ch_group->ch[0]->value.bool_value = !((bool) set_initial_state(ch_group->serv_index, 0, json_context, ch_group->ch[0], CH_TYPE_BOOL, 0));
        } else {
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1)) {
                ch_group->ch[0]->value.bool_value = false;
            }
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0)) {
                ch_group->ch[0]->value.bool_value = true;
            }
        }
        
        lightbulb_group->old_on_value = lightbulb_group->ch0->value.bool_value;
        lightbulb_group->last_on_action = !lightbulb_group->ch0->value.bool_value;
        lightbulb_group->ch0->value.bool_value = !lightbulb_group->ch0->value.bool_value;
        
        lightbulb_group->lightbulb_task_running = true;
        lightbulb_no_task(ch_group);
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW GARAGE DOOR
    void new_garage_door(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(3, 6, 3, 0);
        ch_group->serv_type = SERV_TYPE_GARAGE_DOOR;
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
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
        
        GARAGE_DOOR_WORKER_TIMER = rs_esp_timer_create(1000, true, (void*) ch_group, garage_door_timer_worker);
        
        AUTOOFF_TIMER = autoswitch_time(json_context, ch_group);
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_MARGIN_SET) != NULL) {
            GARAGE_DOOR_TIME_MARGIN = cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_MARGIN_SET)->valuefloat;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_OPEN_SET) != NULL) {
            GARAGE_DOOR_WORKING_TIME = cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_OPEN_SET)->valuefloat;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_CLOSE_SET) != NULL) {
            GARAGE_DOOR_CLOSE_TIME_FACTOR = GARAGE_DOOR_WORKING_TIME / cJSON_GetObjectItemCaseSensitive(json_context, GARAGE_DOOR_TIME_CLOSE_SET)->valuefloat;
        }
        
        GARAGE_DOOR_WORKING_TIME += (GARAGE_DOOR_TIME_MARGIN << 1);
        
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
        
        GD_CURRENT_DOOR_STATE_INT = (uint8_t) set_initial_state(ch_group->serv_index, 0, json_context, GD_CURRENT_DOOR_STATE, CH_TYPE_INT8, 1);
        if (GD_CURRENT_DOOR_STATE_INT > 1) {
            GD_TARGET_DOOR_STATE_INT = GD_CURRENT_DOOR_STATE_INT - 2;
        } else {
            GD_TARGET_DOOR_STATE_INT = GD_CURRENT_DOOR_STATE_INT;
        }
        
        if (GD_CURRENT_DOOR_STATE_INT == 0) {
            GARAGE_DOOR_CURRENT_TIME = GARAGE_DOOR_WORKING_TIME - GARAGE_DOOR_TIME_MARGIN;
        }
        
        int initial_sensor = 2;
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_5) != NULL) {
            GARAGE_DOOR_HAS_F5 = 1;
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_5), garage_door_sensor, ch_group, GARAGE_DOOR_CLOSING)) {
                initial_sensor = GARAGE_DOOR_OPENED;
            }
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_4) != NULL) {
            GARAGE_DOOR_HAS_F4 = 1;
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_4), garage_door_sensor, ch_group, GARAGE_DOOR_OPENING)) {
                initial_sensor = GARAGE_DOOR_OPENED;
            }
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3) != NULL) {
            GARAGE_DOOR_HAS_F3 = 1;
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_3), garage_door_sensor, ch_group, GARAGE_DOOR_CLOSED)) {
                initial_sensor = GARAGE_DOOR_CLOSED;
            }
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2) != NULL) {
            GARAGE_DOOR_HAS_F2 = 1;
            
            if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_2), garage_door_sensor, ch_group, GARAGE_DOOR_OPENED)) {
                initial_sensor = GARAGE_DOOR_OPENED;
            }
        }
        
        if (initial_sensor < 2) {
            garage_door_sensor(99, ch_group, initial_sensor);
        }
        
        initial_sensor = 2;
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_6), garage_door_obstruction, ch_group, 0)) {
            initial_sensor = 0;
        }
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_7), garage_door_obstruction, ch_group, 1)) {
            initial_sensor = 1;
        }
        
        if (initial_sensor < 2) {
            garage_door_obstruction(99, ch_group, initial_sensor);
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW WINDOW COVER
    void new_window_cover(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(4, 4, 5, 1);
        ch_group->serv_type = SERV_TYPE_WINDOW_COVER;
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        int cover_type = WINDOW_COVER_TYPE_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_TYPE_SET) != NULL) {
            cover_type = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_TYPE_SET)->valuefloat;
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
        
        ch_group->timer = rs_esp_timer_create(WINDOW_COVER_TIMER_WORKER_PERIOD_MS, true, (void*) ch_group, window_cover_timer_worker);
        
        ch_group->timer2 = rs_esp_timer_create(WINDOW_COVER_STOP_ENABLE_DELAY_MS, false, (void*) ch_group, window_cover_timer_rearm_stop);
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_TIME_OPEN_SET) != NULL) {
            WINDOW_COVER_TIME_OPEN = cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_TIME_OPEN_SET)->valuefloat;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_TIME_CLOSE_SET) != NULL) {
            WINDOW_COVER_TIME_CLOSE = cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_TIME_CLOSE_SET)->valuefloat;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_CORRECTION_SET) != NULL) {
            WINDOW_COVER_CORRECTION = cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_CORRECTION_SET)->valuefloat;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_MARGIN_SYNC_SET) != NULL) {
            WINDOW_COVER_MARGIN_SYNC = cJSON_GetObjectItemCaseSensitive(json_context, WINDOW_COVER_MARGIN_SYNC_SET)->valuefloat;
        }
        
        WINDOW_COVER_HOMEKIT_POSITION = set_initial_state(ch_group->serv_index, 0, init_last_state_json, WINDOW_COVER_CH_CURRENT_POSITION, CH_TYPE_INT8, 0);
        WINDOW_COVER_MOTOR_POSITION = (((float) WINDOW_COVER_HOMEKIT_POSITION) * (1 + (0.02000000f * ((float) WINDOW_COVER_CORRECTION)))) / (1 + (0.00020000f * ((float) WINDOW_COVER_CORRECTION) * ((float) WINDOW_COVER_HOMEKIT_POSITION)));
        WINDOW_COVER_CH_CURRENT_POSITION->value.int_value = WINDOW_COVER_HOMEKIT_POSITION;
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
        
        int initial_sensor = 2;
        
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_5), window_cover_obstruction, ch_group, 0)) {
            initial_sensor = 0;
        }
        if (diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_6), window_cover_obstruction, ch_group, 1)) {
            initial_sensor = 1;
        }
        
        if (initial_sensor < 2) {
            window_cover_obstruction(99, ch_group, initial_sensor);
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW LIGHT SENSOR
    void new_light_sensor(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(1, 3, 4, 1);
        ch_group->serv_type = SERV_TYPE_LIGHT_SENSOR;
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
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
        
        int light_sensor_type = LIGHT_SENSOR_TYPE_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_TYPE_SET) != NULL) {
            light_sensor_type = cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_TYPE_SET)->valuefloat;
        }
        
        LIGHT_SENSOR_TYPE = light_sensor_type;
        
        LIGHT_SENSOR_FACTOR = LIGHT_SENSOR_FACTOR_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_FACTOR_SET) != NULL) {
            LIGHT_SENSOR_FACTOR = cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_FACTOR_SET)->valuefloat;
        }
        
        LIGHT_SENSOR_OFFSET = LIGHT_SENSOR_OFFSET_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_OFFSET_SET) != NULL) {
            LIGHT_SENSOR_OFFSET = cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_OFFSET_SET)->valuefloat;
        }
        
        if (light_sensor_type < 2) {
            LIGHT_SENSOR_RESISTOR = LIGHT_SENSOR_RESISTOR_DEFAULT;
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_RESISTOR_SET) != NULL) {
                LIGHT_SENSOR_RESISTOR = cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_RESISTOR_SET)->valuefloat * 1000;
            }
            
            LIGHT_SENSOR_POW = LIGHT_SENSOR_POW_DEFAULT;
            if (cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_POW_SET) != NULL) {
                LIGHT_SENSOR_POW = cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_POW_SET)->valuefloat;
            }
            
#ifdef ESP_PLATFORM
            LIGHT_SENSOR_GPIO = cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_GPIO_SET)->valuefloat;
#endif
        } else if (light_sensor_type == 2) {
            cJSON* data_array = cJSON_GetObjectItemCaseSensitive(json_context, LIGHT_SENSOR_I2C_DATA_ARRAY_SET);
            LIGHT_SENSOR_I2C_BUS = cJSON_GetArrayItem(data_array, 0)->valuefloat;
            LIGHT_SENSOR_I2C_ADDR = (uint8_t) cJSON_GetArrayItem(data_array, 1)->valuefloat;
            
            const uint8_t start_bh1750 = 0x10;
            adv_i2c_slave_write(LIGHT_SENSOR_I2C_BUS, (uint8_t) LIGHT_SENSOR_I2C_ADDR, NULL, 0, &start_bh1750, 1);
        }
        
        const float poll_period = sensor_poll_period(json_context, LIGHT_SENSOR_POLL_PERIOD_DEFAULT);
        rs_esp_timer_start_forced(rs_esp_timer_create(poll_period * 1000, true, (void*) ch_group, light_sensor_timer_worker));
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW SECURITY SYSTEM
    void new_sec_system(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(2, 0, 0, 0);
        ch_group->serv_type = SERV_TYPE_SECURITY_SYSTEM;
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
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
            for (int i = 0; i < valid_values_len; i++) {
                current_valid_values[i] = (uint8_t) cJSON_GetArrayItem(modes_array, i)->valuefloat;
                target_valid_values[i] = current_valid_values[i];
            }
        } else {
            for (int i = 0; i < 4; i++) {
                current_valid_values[i] = i;
                target_valid_values[i] = i;
            }
        }
        
        current_valid_values[valid_values_len] = 4;
        
        SEC_SYSTEM_CH_CURRENT_STATE = NEW_HOMEKIT_CHARACTERISTIC(SECURITY_SYSTEM_CURRENT_STATE, target_valid_values[valid_values_len - 1], .min_value=(float[]) {current_valid_values[0]}, .valid_values={.count=valid_values_len + 1, .values=current_valid_values});
        SEC_SYSTEM_CH_TARGET_STATE = NEW_HOMEKIT_CHARACTERISTIC(SECURITY_SYSTEM_TARGET_STATE, target_valid_values[valid_values_len - 1], .min_value=(float[]) {target_valid_values[0]}, .max_value=(float[]) {target_valid_values[valid_values_len - 1]}, .valid_values={.count=valid_values_len, .values=target_valid_values}, .setter_ex=hkc_sec_system);
  
        SEC_SYSTEM_REC_ALARM_TIMER = rs_esp_timer_create(SEC_SYSTEM_REC_ALARM_PERIOD_MS, true, (void*) ch_group, sec_system_recurrent_alarm);
        
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
        
        SEC_SYSTEM_CH_CURRENT_STATE->value.int_value = set_initial_state(ch_group->serv_index, 1, json_context, SEC_SYSTEM_CH_TARGET_STATE, CH_TYPE_INT8, target_valid_values[valid_values_len - 1]);
        
        const int exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
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
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW TV
    void new_tv(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        cJSON* json_inputs = cJSON_GetObjectItemCaseSensitive(json_context, TV_INPUTS_ARRAY);
        uint8_t inputs = cJSON_GetArraySize(json_inputs);
        
        if (inputs == 0) {
            inputs = 1;
        }
        
        ch_group_t* ch_group = new_ch_group(7, 0, 0, 0);
        ch_group->serv_type = SERV_TYPE_TV;
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
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
            INFO("Input %s", name);
            
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
            input_service[0]->characteristics[1] = NEW_HOMEKIT_CHARACTERISTIC(CONFIGURED_NAME_STATIC, name, .setter_ex=hkc_tv_input_configured_name);
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
            
            for (int i = 0; i < inputs; i++) {
                cJSON* json_input = cJSON_GetArrayItem(json_inputs, i);
                
                char* name = strdup("TV");
                if (cJSON_GetObjectItemCaseSensitive(json_input, TV_INPUT_NAME) != NULL) {
                    free(name);
                    name = uni_strdup(cJSON_GetObjectItemCaseSensitive(json_input, TV_INPUT_NAME)->valuestring, &unistrings);
                    if (cJSON_GetObjectItemCaseSensitive(json_input, "0") != NULL) {
                        int int_action = MAX_ACTIONS + i;
                        char action[4];
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
        
        char* configured_name = NULL;
        if (set_initial_state_data(ch_group->serv_index, 1, init_last_state_json, ch_group->ch[1], CH_TYPE_STRING, 0, &configured_name) > 0) {
            free(ch_group->ch[1]->value.string_value);
            ch_group->ch[1]->value.string_value = configured_name;
        }
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_1), digstate, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_0), digstate, ch_group, 0);
        
        const int exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0);
            
            ch_group->ch[0]->value.int_value = !((uint8_t) set_initial_state(ch_group->serv_index, 0, json_context, ch_group->ch[0], CH_TYPE_INT8, 0));
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
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW FAN
    void new_fan(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(2, 1, 0, 1);
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        int max_speed = 100;
        if (cJSON_GetObjectItemCaseSensitive(json_context, FAN_SPEED_STEPS) != NULL) {
            max_speed = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, FAN_SPEED_STEPS)->valuefloat;
        }
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(ON, false, .setter_ex=hkc_fan_setter);
        ch_group->ch[1] = NEW_HOMEKIT_CHARACTERISTIC(ROTATION_SPEED, max_speed, .max_value=(float[]) {max_speed}, .setter_ex=hkc_fan_setter);
        
        FAN_CURRENT_ACTION = -1;
        
        ch_group->serv_type = SERV_TYPE_FAN;
        register_actions(ch_group, json_context, 0);
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        AUTOOFF_TIMER = autoswitch_time(json_context, ch_group);
        
        if (ch_group->homekit_enabled) {
            FAN_SET_DELAY_TIMER = rs_esp_timer_create(FAN_SET_DELAY_MS, false, (void*) ch_group, process_fan_timer);   // Only used with HomeKit
            
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
        
        float saved_speed = set_initial_state(ch_group->serv_index, 1, init_last_state_json, ch_group->ch[1], CH_TYPE_FLOAT, max_speed);
        if (saved_speed > max_speed) {
            saved_speed = max_speed;
        }
        ch_group->ch[1]->value.float_value = saved_speed;
        
        diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, BUTTONS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, PINGS_ARRAY), diginput, ch_group, 2);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_1), diginput, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_ARRAY_0), diginput, ch_group, 0);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_1), digstate, ch_group, 1);
        ping_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_PINGS_STATUS_ARRAY_0), digstate, ch_group, 0);
        
        const int exec_actions_on_boot = get_exec_actions_on_boot(json_context);
        
        if (get_initial_state(json_context) != INIT_STATE_FIXED_INPUT) {
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_1), diginput, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_ARRAY_0), diginput, ch_group, 0);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_1), digstate, ch_group, 1);
            diginput_register(cJSON_GetObjectItemCaseSensitive(json_context, FIXED_BUTTONS_STATUS_ARRAY_0), digstate, ch_group, 0);
            
            ch_group->ch[0]->value.bool_value = !((uint8_t) set_initial_state(ch_group->serv_index, 0, json_context, ch_group->ch[0], CH_TYPE_BOOL, false));
            if (exec_actions_on_boot) {
                 hkc_fan_setter(ch_group->ch[0], HOMEKIT_BOOL(!ch_group->ch[0]->value.bool_value));
            } else {
                hkc_fan_status_setter(ch_group->ch[0], HOMEKIT_BOOL(!ch_group->ch[0]->value.bool_value));
            }
        } else {
            register_bool_inputs(json_context, ch_group, exec_actions_on_boot);
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW BATTERY
    void new_battery(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(2, 1, 0, 1);
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        int battery_low = BATTERY_LOW_THRESHOLD_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, BATTERY_LOW_THRESHOLD_SET) != NULL) {
            battery_low = (uint8_t) cJSON_GetObjectItemCaseSensitive(json_context, BATTERY_LOW_THRESHOLD_SET)->valuefloat;
        }
        
        BATTERY_LOW_THRESHOLD = battery_low;
        
        BATTERY_LEVEL_CH = NEW_HOMEKIT_CHARACTERISTIC(BATTERY_LEVEL, 0);
        BATTERY_STATUS_LOW_CH = NEW_HOMEKIT_CHARACTERISTIC(STATUS_LOW_BATTERY, 0);
        
        ch_group->serv_type = SERV_TYPE_BATTERY;
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
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_BATTERY_SERVICE;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_BATTERY_SERVICE;
            }
            
            accessories[accessory]->services[service]->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = BATTERY_LEVEL_CH;
            accessories[accessory]->services[service]->characteristics[1] = BATTERY_STATUS_LOW_CH;
            
            //service_iid += 3;
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW POWER MONITOR
    void new_power_monitor(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        ch_group_t* ch_group = new_ch_group(7, 3, 8, 4);
        ch_group->serv_type = SERV_TYPE_POWER_MONITOR;
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
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
        
        ch_group->ch[3]->value.float_value = set_initial_state(ch_group->serv_index, 3, init_last_state_json, ch_group->ch[3], CH_TYPE_FLOAT, 0);
        ch_group->ch[4]->value.float_value = set_initial_state(ch_group->serv_index, 4, init_last_state_json, ch_group->ch[4], CH_TYPE_FLOAT, 0);
        ch_group->ch[5]->value.int_value = set_initial_state(ch_group->serv_index, 5, init_last_state_json, ch_group->ch[5], CH_TYPE_INT, 0);
        
        PM_VOLTAGE_FACTOR = PM_VOLTAGE_FACTOR_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, PM_VOLTAGE_FACTOR_SET) != NULL) {
            PM_VOLTAGE_FACTOR = (float) cJSON_GetObjectItemCaseSensitive(json_context, PM_VOLTAGE_FACTOR_SET)->valuefloat;
        }
        
        PM_VOLTAGE_OFFSET = PM_VOLTAGE_OFFSET_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, PM_VOLTAGE_OFFSET_SET) != NULL) {
            PM_VOLTAGE_OFFSET = (float) cJSON_GetObjectItemCaseSensitive(json_context, PM_VOLTAGE_OFFSET_SET)->valuefloat;
        }
        
        PM_CURRENT_FACTOR = PM_CURRENT_FACTOR_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, PM_CURRENT_FACTOR_SET) != NULL) {
            PM_CURRENT_FACTOR = (float) cJSON_GetObjectItemCaseSensitive(json_context, PM_CURRENT_FACTOR_SET)->valuefloat;
        }
        
        PM_CURRENT_OFFSET = PM_CURRENT_OFFSET_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, PM_CURRENT_OFFSET_SET) != NULL) {
            PM_CURRENT_OFFSET = (float) cJSON_GetObjectItemCaseSensitive(json_context, PM_CURRENT_OFFSET_SET)->valuefloat;
        }
        
        PM_POWER_FACTOR = PM_POWER_FACTOR_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, PM_POWER_FACTOR_SET) != NULL) {
            PM_POWER_FACTOR = (float) cJSON_GetObjectItemCaseSensitive(json_context, PM_POWER_FACTOR_SET)->valuefloat;
        }
        
        PM_POWER_OFFSET = PM_POWER_OFFSET_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, PM_POWER_OFFSET_SET) != NULL) {
            PM_POWER_OFFSET = (float) cJSON_GetObjectItemCaseSensitive(json_context, PM_POWER_OFFSET_SET)->valuefloat;
        }
        
        uint8_t pm_sensor_type = PM_SENSOR_TYPE_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, PM_SENSOR_TYPE_SET) != NULL) {
            pm_sensor_type = cJSON_GetObjectItemCaseSensitive(json_context, PM_SENSOR_TYPE_SET)->valuefloat;
        }
        
        PM_SENSOR_TYPE = pm_sensor_type;
        
        int16_t data[4] = { PM_SENSOR_DATA_DEFAULT, PM_SENSOR_DATA_DEFAULT, PM_SENSOR_DATA_DEFAULT, PM_SENSOR_DATA_DEFAULT_INT_TYPE };
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, PM_SENSOR_DATA_ARRAY_SET) != NULL) {
            cJSON* gpio_array = cJSON_GetObjectItemCaseSensitive(json_context, PM_SENSOR_DATA_ARRAY_SET);
            for (int i = 0; i < cJSON_GetArraySize(gpio_array); i++) {
                data[i] = (int16_t) cJSON_GetArrayItem(gpio_array, i)->valuefloat;
                
                /*
                if (i < 3 && pm_sensor_type < 2 && data[i] > -1) {
                    set_used_gpio(data[i]);
                }
                */
            }
        }
        
        if (pm_sensor_type <= 1) {          // HLW chip
            PM_SENSOR_HLW_GPIO_CF = data[0];
            
            if (data[0] != PM_SENSOR_DATA_DEFAULT) {
                PM_SENSOR_HLW_GPIO = data[0];
            } else {
                PM_SENSOR_HLW_GPIO = data[1];
            }
            
            adv_hlw_unit_create(data[0], data[1], data[2], pm_sensor_type, data[3]);
            
        } else if (pm_sensor_type <= 3) {   // ADE7953 chip
            PM_SENSOR_ADE_BUS = data[0];
            PM_SENSOR_ADE_ADDR = data[1];
            
            uint8_t reg[2];
            uint8_t bytes[2];
            
            reg[0] = 0x01;
            reg[1] = 0x02;
            bytes[0] = 0x00;
            bytes[1] = 0x04;
            adv_i2c_slave_write(data[0], data[1], reg, 2, bytes, 2);
            
            reg[0] = 0x00;
            reg[1] = 0xFE;
            uint8_t byte = 0xAD;
            adv_i2c_slave_write(data[0], data[1], reg, 2, &byte, 1);
            
            reg[0] = 0x01;
            reg[1] = 0x20;
            bytes[1] = 0x30;
            adv_i2c_slave_write(data[0], data[1], reg, 2, bytes, 2);
        }
        
        if (pm_sensor_type <= 4) {
            PM_POLL_PERIOD = sensor_poll_period(json_context, PM_POLL_PERIOD_DEFAULT);
            rs_esp_timer_start_forced(rs_esp_timer_create(PM_POLL_PERIOD * 1000, true, (void*) ch_group, power_monitor_timer_worker));
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW FREE MONITOR
    void new_free_monitor(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context, const uint8_t serv_type) {
        int fm_sensor_type = FM_SENSOR_TYPE_DEFAULT;
        
        int tg_serv = 0;
        unsigned int tg_ch = 0;
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, FM_TARGET_CH_ARRAY_SET) != NULL) {
            cJSON* target_array = cJSON_GetObjectItemCaseSensitive(json_context, FM_TARGET_CH_ARRAY_SET);
            tg_serv = (int16_t) cJSON_GetArrayItem(target_array, 0)->valuefloat;
            tg_ch = (uint8_t) cJSON_GetArrayItem(target_array, 1)->valuefloat;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, FM_SENSOR_TYPE_SET) != NULL) {
            fm_sensor_type = cJSON_GetObjectItemCaseSensitive(json_context, FM_SENSOR_TYPE_SET)->valuefloat;
        }
        
        pattern_t* pattern_base = NULL;
        
        if (free_monitor_type_is_pattern(fm_sensor_type)) {
            cJSON* pattern_array = cJSON_GetObjectItemCaseSensitive(json_context, FM_PATTERN_ARRAY_SET);
            for (int i = cJSON_GetArraySize(pattern_array) - 1; i >= 0; i--) {
                pattern_t* new_pattern = malloc(sizeof(pattern_t));
                memset(new_pattern, 0, sizeof(*new_pattern));
                
                cJSON* pattern_data = cJSON_GetArrayItem(pattern_array, i);
                
                if (strlen(cJSON_GetArrayItem(pattern_data, 0)->valuestring) > 0) {
                    if (fm_sensor_type == FM_SENSOR_TYPE_NETWORK_PATTERN_TEXT ||
                        fm_sensor_type == FM_SENSOR_TYPE_UART_PATTERN_TEXT) {
                        new_pattern->pattern = (uint8_t*) uni_strdup(cJSON_GetArrayItem(pattern_data, 0)->valuestring, &unistrings);
                        new_pattern->len = strlen((char*) new_pattern->pattern);
                    } else {
                        new_pattern->len = process_hexstr(cJSON_GetArrayItem(pattern_data, 0)->valuestring, &new_pattern->pattern, &unistrings);
                    }
                }
                
                if (cJSON_GetArraySize(pattern_data) > 1) {
                    new_pattern->offset = (int16_t) cJSON_GetArrayItem(pattern_data, 1)->valuefloat;
                }
                
                new_pattern->next = pattern_base;
                pattern_base = new_pattern;
            }
        }
        
        int has_limits = 0;
        if (cJSON_GetObjectItemCaseSensitive(json_context, FM_LIMIT_ARRAY_SET) != NULL) {
            has_limits = 2;
        }
        
        int is_type_uart = false;
        if (fm_sensor_type >= FM_SENSOR_TYPE_UART) {
            is_type_uart = true;
        }
        
        int is_val_data = 0;
        if (is_type_uart ||
            fm_sensor_type == FM_SENSOR_TYPE_NETWORK_PATTERN_HEX ||
            fm_sensor_type == FM_SENSOR_TYPE_I2C) {
            is_val_data = 2;
        }
        
        int maths_operations = 0;
        if (fm_sensor_type == FM_SENSOR_TYPE_MATHS) {
            cJSON* val_data = cJSON_GetObjectItemCaseSensitive(json_context, FM_READ_COMMAND_DATA_ARRAY);
            maths_operations = cJSON_GetArraySize(val_data) / 3;
        }
        
        ch_group_t* ch_group = new_ch_group(1 +
                                            ( pattern_base ? 1 : 0 ) +
                                            ( tg_serv ? 1 : 0 ),
                                            
                                            1 +
#ifdef ESP_PLATFORM
                                            ( fm_sensor_type == FM_SENSOR_TYPE_ADC || fm_sensor_type == FM_SENSOR_TYPE_ADC_INV ) +
#endif
                                            ( fm_sensor_type == FM_SENSOR_TYPE_PULSE_FREQ ) +
                                            ( fm_sensor_type == FM_SENSOR_TYPE_PULSE_US_TIME ? 3 : 0 ) +
                                            is_type_uart +
                                            is_val_data +
                                            ( fm_sensor_type >= FM_SENSOR_TYPE_NETWORK ? 2 : 0 ) +
                                            ( fm_sensor_type >= FM_SENSOR_TYPE_NETWORK && fm_sensor_type <= FM_SENSOR_TYPE_NETWORK_PATTERN_HEX ? 2 : 0 ) +
                                            ( fm_sensor_type == FM_SENSOR_TYPE_I2C ? 8 : 0 ) +
                                            ( fm_sensor_type == FM_SENSOR_TYPE_I2C_TRIGGER ? 18 : 0 ) +
                                            (maths_operations << 1),
                                            
                                            4 +
                                            has_limits +
                                            maths_operations,
                                            
                                            1);
        
        FM_OVERRIDE_VALUE = NO_LAST_WILDCARD_ACTION;
        
        if (has_limits > 0) {
            cJSON* limits_array = cJSON_GetObjectItemCaseSensitive(json_context, FM_LIMIT_ARRAY_SET);
            FM_LIMIT_LOWER = cJSON_GetArrayItem(limits_array, 0)->valuefloat;
            FM_LIMIT_UPPER = cJSON_GetArrayItem(limits_array, 1)->valuefloat;
            
            FM_SENSOR_TYPE = -fm_sensor_type;   // If there are limits, sensor_type is negative
        } else {
            FM_SENSOR_TYPE = fm_sensor_type;
        }
        
        ch_group->serv_type = serv_type;
        ch_group->serv_index = service_numerator;
        int homekit_enabled = acc_homekit_enabled(json_context);
        ch_group->homekit_enabled = homekit_enabled;
        
        if (fm_sensor_type == FM_SENSOR_TYPE_MATHS) {
            FM_MATHS_OPERATIONS = maths_operations;
            
            cJSON* val_data = cJSON_GetObjectItemCaseSensitive(json_context, FM_READ_COMMAND_DATA_ARRAY);
            
            int float_index = FM_MATHS_FLOAT_FIRST + has_limits;
            int int_index = FM_MATHS_FIRST_OPERATION;
            for (int i = 0; i < (maths_operations * 3); i++) {
                for (int j = 0; j < 2; j++) {
                    FM_MATHS_INT[int_index] = cJSON_GetArrayItem(val_data, i)->valuefloat;
                    int_index++;
                    i++;
                }
                //FM_MATHS_INT[int_index] = cJSON_GetArrayItem(val_data, i)->valuefloat;
                //int_index++;
                //i++;
                //FM_MATHS_INT[int_index] = cJSON_GetArrayItem(val_data, i)->valuefloat;
                //int_index++;
                //i++;
                FM_MATHS_FLOAT[float_index] = cJSON_GetArrayItem(val_data, i)->valuefloat;
                float_index++;
            }
        }
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        set_accessory_ir_protocol(ch_group, json_context);
        register_wildcard_actions(ch_group, json_context);
        new_action_network(ch_group, json_context, 0);
        
        ch_group->ch[0] = NEW_HOMEKIT_CHARACTERISTIC(CUSTOM_FREE_VALUE, 0);
        
        if (tg_serv != 0) {
            ch_group->ch[ch_group->chs - 1] = ch_group_find_by_serv(get_absolut_index(service_numerator, tg_serv))->ch[tg_ch];
        }
        
        if (pattern_base) {
            FM_PATTERN_CH_WRITE = (homekit_characteristic_t*) pattern_base;
        }
        
        if (ch_group->homekit_enabled) {
            accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
            accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
            //accessories[accessory]->services[service]->id = service_iid;
            accessories[accessory]->services[service]->primary = !(service - 1);
            accessories[accessory]->services[service]->hidden = homekit_enabled - 1;
            
            if (homekit_enabled == 2) {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_HAA_HIDDEN_FREE_MONITOR;
            } else {
                accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_FREE_MONITOR;
            }
            
            accessories[accessory]->services[service]->characteristics = calloc(2, sizeof(homekit_characteristic_t*));
            accessories[accessory]->services[service]->characteristics[0] = ch_group->ch[0];
            
            //service_iid += 2;
        }
        
        FM_FACTOR = FM_FACTOR_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, FM_FACTOR_SET) != NULL) {
            FM_FACTOR = (float) cJSON_GetObjectItemCaseSensitive(json_context, FM_FACTOR_SET)->valuefloat;
        }
        
        FM_OFFSET = FM_OFFSET_DEFAULT;
        if (cJSON_GetObjectItemCaseSensitive(json_context, FM_OFFSET_SET) != NULL) {
            FM_OFFSET = (float) cJSON_GetObjectItemCaseSensitive(json_context, FM_OFFSET_SET)->valuefloat;
        }
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, FM_BUFFER_LEN_ARRAY_SET) != NULL) {
            cJSON* buffer_len_array = cJSON_GetObjectItemCaseSensitive(json_context, FM_BUFFER_LEN_ARRAY_SET);
            FM_BUFFER_LEN_MIN = (uint8_t) cJSON_GetArrayItem(buffer_len_array, 0)->valuefloat;
            FM_BUFFER_LEN_MAX = (uint8_t) cJSON_GetArrayItem(buffer_len_array, 1)->valuefloat;
        }
        
        if (fm_sensor_type == FM_SENSOR_TYPE_PULSE_FREQ ||
#ifdef ESP_PLATFORM
            fm_sensor_type == FM_SENSOR_TYPE_ADC || fm_sensor_type == FM_SENSOR_TYPE_ADC_INV ||
#endif
            fm_sensor_type == FM_SENSOR_TYPE_PULSE_US_TIME) {
            cJSON* gpio_array = cJSON_GetObjectItemCaseSensitive(json_context, FM_SENSOR_GPIO_ARRAY_SET);
            unsigned int gpio = (uint8_t) cJSON_GetArrayItem(gpio_array, 0)->valuefloat;
            FM_SENSOR_GPIO = gpio;
            unsigned int interrupt_type;
            
#ifdef ESP_PLATFORM
            if (fm_sensor_type == FM_SENSOR_TYPE_PULSE_FREQ ||
                fm_sensor_type == FM_SENSOR_TYPE_PULSE_US_TIME) {
#endif
                interrupt_type = (uint8_t) cJSON_GetArrayItem(gpio_array, 1)->valuefloat;
                //set_used_gpio(gpio);
#ifdef ESP_PLATFORM
            }
#endif
            
            if (fm_sensor_type == FM_SENSOR_TYPE_PULSE_FREQ) {
                adv_hlw_unit_create(gpio, -1, -1, 0, interrupt_type);
                
            } else if (fm_sensor_type == FM_SENSOR_TYPE_PULSE_US_TIME) {
#ifdef ESP_PLATFORM
                gpio_install_isr_service(0);
                gpio_set_intr_type(gpio, interrupt_type);
                gpio_isr_handler_add(gpio, fm_pulse_interrupt, (void*) ((uint32_t) gpio));
                gpio_intr_disable(gpio);
#endif
                FM_SENSOR_GPIO_INT_TYPE = interrupt_type;
                
                FM_SENSOR_GPIO_TRIGGER = 255;
                if (cJSON_GetArraySize(gpio_array) > 2) {
                    const int gpio_trigger = (uint8_t) cJSON_GetArrayItem(gpio_array, 2)->valuefloat;
                    FM_SENSOR_GPIO_TRIGGER = (uint8_t) gpio_trigger;
                    
                    const bool inverted = gpio_trigger / 100;
                    const unsigned int gpio_final = gpio_trigger % 100;
                    
                    gpio_write(gpio_final, !inverted);
                    //set_used_gpio(gpio_final);
                }
            }
            
        } else if (fm_sensor_type == FM_SENSOR_TYPE_I2C ||
                   fm_sensor_type == FM_SENSOR_TYPE_I2C_TRIGGER) {
            cJSON* i2c_array = cJSON_GetObjectItemCaseSensitive(json_context, FM_I2C_DEVICE_DATA_ARRAY_SET);
            unsigned int i2c_bus = (uint8_t) cJSON_GetArrayItem(i2c_array, 0)->valuefloat;
            unsigned int i2c_addr = (uint8_t) cJSON_GetArrayItem(i2c_array, 1)->valuefloat;
            size_t i2c_read_len = cJSON_GetArraySize(i2c_array) - 2;
            
            for (unsigned int i = 0; i < i2c_read_len; i++) {
                FM_I2C_REG[FM_I2C_REG_FIRST + i] = (uint8_t) cJSON_GetArrayItem(i2c_array, i + 2)->valuefloat;
            }
            
            FM_I2C_BUS = i2c_bus;
            FM_I2C_ADDR = i2c_addr;
            FM_I2C_REG_LEN = i2c_read_len;
            
            FM_I2C_VAL_OFFSET = FM_I2C_VAL_OFFSET_DEFAULT;
            if (cJSON_GetObjectItemCaseSensitive(json_context, FM_I2C_VAL_OFFSET_SET) != NULL) {
                FM_I2C_VAL_OFFSET = cJSON_GetObjectItemCaseSensitive(json_context, FM_I2C_VAL_OFFSET_SET)->valuefloat;
            }
            
            // Commands sent at beginning
            if (cJSON_GetObjectItemCaseSensitive(json_context, FM_I2C_START_COMMANDS_ARRAY_SET) != NULL) {
                cJSON* i2c_inits = cJSON_GetObjectItemCaseSensitive(json_context, FM_I2C_START_COMMANDS_ARRAY_SET);
                for (int i = 0; i < cJSON_GetArraySize(i2c_inits); i++) {
                    cJSON* i2c_init = cJSON_GetArrayItem(i2c_inits, i);
                    
                    uint8_t reg[32];
                    
                    const unsigned int reg_size = (uint8_t) cJSON_GetArrayItem(i2c_init, 0)->valuefloat;
                    if (reg_size > 0) {
                        for (unsigned int j = 0; j < reg_size; j++) {
                            reg[j] = cJSON_GetArrayItem(i2c_init, j + 1)->valuefloat;
                        }
                    }
                    
                    const unsigned int val_offset = reg_size + 1;
                    const unsigned int val_size = cJSON_GetArraySize(i2c_init) - val_offset;
                    uint8_t val[val_size];
                    
                    for (unsigned int j = 0; j < val_size; j++) {
                        val[j] = cJSON_GetArrayItem(i2c_init, j + val_offset)->valuefloat;
                    }
                    
                    INFO("I2C Init %i: %i", i, adv_i2c_slave_write(i2c_bus, i2c_addr, reg, reg_size, val, val_size));
                }
            }
            
            // Trigger
            if (fm_sensor_type == FM_SENSOR_TYPE_I2C_TRIGGER) {
                cJSON* i2c_trigger_array = cJSON_GetObjectItemCaseSensitive(json_context, FM_I2C_TRIGGER_COMMAND_ARRAY_SET);
                size_t i2c_trigger_array_len = (uint8_t) cJSON_GetArraySize(i2c_trigger_array);
                
                FM_NEW_VALUE = cJSON_GetArrayItem(i2c_trigger_array, 0)->valuefloat * MS_TO_TICKS(1000);
                
                size_t trigger_reg_len = (uint8_t) cJSON_GetArrayItem(i2c_trigger_array, 1)->valuefloat;
                for (unsigned int i = 0; i < trigger_reg_len; i++) {
                    FM_I2C_TRIGGER_REG[FM_I2C_TRIGGER_REG_FIRST + i] = (uint8_t) cJSON_GetArrayItem(i2c_trigger_array, i + 2)->valuefloat;
                }
                
                size_t trigger_val_len = i2c_trigger_array_len - trigger_reg_len - 2;
                for (unsigned int i = 0; i < trigger_val_len; i++) {
                    FM_I2C_TRIGGER_VAL[FM_I2C_TRIGGER_VAL_FIRST + i] = (uint8_t) cJSON_GetArrayItem(i2c_trigger_array, i + trigger_reg_len + 2)->valuefloat;
                }
            }
#ifdef ESP_PLATFORM
        } else if (is_type_uart) {
            if (cJSON_GetObjectItemCaseSensitive(json_context, FM_UART_PORT_SET) != NULL) {
                FM_UART_PORT = cJSON_GetObjectItemCaseSensitive(json_context, FM_UART_PORT_SET)->valuefloat;
            }
#endif
        }
        
        if (is_val_data > 0) {
            cJSON* val_data = cJSON_GetObjectItemCaseSensitive(json_context, FM_READ_COMMAND_DATA_ARRAY);
            FM_VAL_LEN = (uint8_t) cJSON_GetArrayItem(val_data, 0)->valuefloat;
            FM_VAL_TYPE = (uint8_t) cJSON_GetArrayItem(val_data, 1)->valuefloat;
        }
        
        if (fm_sensor_type > FM_SENSOR_TYPE_FREE &&
            fm_sensor_type < FM_SENSOR_TYPE_UART) {
            const float poll_period = sensor_poll_period(json_context, FM_POLL_PERIOD_DEFAULT);
            if (poll_period > 0) {
                rs_esp_timer_start_forced(rs_esp_timer_create(poll_period * 1000, true, (void*) ch_group, free_monitor_timer_worker));
            }
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** NEW DATA HISTORY
    void new_data_history(const uint16_t accessory, uint16_t service, const uint16_t total_services, cJSON* json_context) {
        cJSON* data_array = cJSON_GetObjectItemCaseSensitive(json_context, HIST_DATA_ARRAY_SET);
        
        const unsigned int hist_accessory = get_absolut_index(service_numerator, cJSON_GetArrayItem(data_array, 0)->valuefloat);
        const unsigned int hist_ch = cJSON_GetArrayItem(data_array, 1)->valuefloat;
        const size_t hist_size = cJSON_GetArrayItem(data_array, 2)->valuefloat;
        
        INFO("Serv %i, Ch %i, Size %i", hist_accessory, hist_ch, hist_size * HIST_REGISTERS_BY_BLOCK);
        
        ch_group_t* ch_group = new_ch_group(hist_size + 1, 0, 1, 0);
        ch_group->serv_type = SERV_TYPE_DATA_HISTORY;
        ch_group->serv_index = service_numerator;
        ch_group->homekit_enabled = true;
        
        if (cJSON_GetObjectItemCaseSensitive(json_context, HIST_READ_ON_CLOCK_READY_SET) != NULL) {
            ch_group->child_enabled = (bool) cJSON_GetObjectItemCaseSensitive(json_context, HIST_READ_ON_CLOCK_READY_SET)->valuefloat;
        }
        
        if (service == 0) {
            new_accessory(accessory, total_services, ch_group->homekit_enabled, json_context);
        }
        
        service++;
        
        ch_group->ch[hist_size] = ch_group_find_by_serv(hist_accessory)->ch[hist_ch];
        
        accessories[accessory]->services[service] = calloc(1, sizeof(homekit_service_t));
        accessories[accessory]->services[service]->id = ((service - 1) * 50) + 8;
        //accessories[accessory]->services[service]->id = service_iid;
        accessories[accessory]->services[service]->primary = !(service - 1);
        accessories[accessory]->services[service]->hidden = true;
        accessories[accessory]->services[service]->type = HOMEKIT_SERVICE_CUSTOM_DATA_HISTORY;
        
        accessories[accessory]->services[service]->characteristics = calloc(hist_size + 1, sizeof(homekit_characteristic_t*));
        
        //service_iid += (hist_size + 1);
        
        for (unsigned int i = 0; i < hist_size; i++) {
            // Each block uses 132 + HIST_BLOCK_SIZE bytes
            ch_group->ch[i] = NEW_HOMEKIT_CHARACTERISTIC(CUSTOM_DATA_HISTORY, NULL, 0);
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
            rs_esp_timer_start_forced(rs_esp_timer_create(poll_period * 1000, true, (void*) ch_group->ch[hist_size], data_history_timer_worker));
        }
        
        set_killswitch(ch_group, json_context);
    }
    
    // *** Accessory Builder
    // Root device
    ch_group_t* root_device_ch_group = new_ch_group(0, 0, 0, 0);
#if SERV_TYPE_ROOT_DEVICE != 0
    root_device_ch_group->serv_type = SERV_TYPE_ROOT_DEVICE;
#endif
    register_actions(root_device_ch_group, json_config, 0);
    set_accessory_ir_protocol(root_device_ch_group, json_config);
    set_killswitch(root_device_ch_group, json_config);
    
    // Saved States Timer Function
    root_device_ch_group->timer = rs_esp_timer_create(SAVE_STATES_DELAY_MS, false, NULL, save_states);
    
    // Run action 0 from root device
    do_actions(root_device_ch_group, 0);
    
    // Initial delay
    if (cJSON_GetObjectItemCaseSensitive(json_config, NEXT_SERV_CREATION_DELAY) != NULL) {
        vTaskDelay(cJSON_GetObjectItemCaseSensitive(json_config, NEXT_SERV_CREATION_DELAY)->valuefloat * MS_TO_TICKS(1000));
    } else {
        vTaskDelay(1);
    }
    
    // Bridge
    if (bridge_needed) {
        INFO("\n** BRIDGE");
        new_accessory(0, 2, true, NULL);
        acc_count++;
    }
    
    // New HomeKit Service
    void new_service(const uint16_t acc_count, uint16_t serv_count, const uint16_t total_services, cJSON* json_accessory, const uint8_t serv_type) {
        service_numerator++;
#ifdef ESP_PLATFORM
        INFO("\n* SERV %li (%i)", service_numerator, serv_type);
#else
        INFO("\n* SERV %i (%i)", service_numerator, serv_type);
#endif
        
        if (serv_type == SERV_TYPE_BUTTON ||
            serv_type == SERV_TYPE_DOORBELL) {
            new_button_event(acc_count, serv_count, total_services, json_accessory, serv_type);
            
        } else if (serv_type == SERV_TYPE_LOCK) {
            new_lock(acc_count, serv_count, total_services, json_accessory);
            
        } else if (serv_type >= SERV_TYPE_CONTACT_SENSOR &&
                   serv_type <= SERV_TYPE_MOTION_SENSOR) {
            new_binary_sensor(acc_count, serv_count, total_services, json_accessory, serv_type);
            
        } else if (serv_type == SERV_TYPE_AIR_QUALITY) {
            new_air_quality(acc_count, serv_count, total_services, json_accessory);
            
        } else if (serv_type == SERV_TYPE_WATER_VALVE) {
            new_valve(acc_count, serv_count, total_services, json_accessory);
        
        } else if (serv_type == SERV_TYPE_THERMOSTAT ||
                   serv_type == SERV_TYPE_THERMOSTAT_WITH_HUM) {
            new_thermostat(acc_count, serv_count, total_services, json_accessory, serv_type);
            
        } else if (serv_type == SERV_TYPE_TEMP_SENSOR) {
            new_temp_sensor(acc_count, serv_count, total_services, json_accessory);
            
        } else if (serv_type == SERV_TYPE_HUM_SENSOR) {
            new_hum_sensor(acc_count, serv_count, total_services, json_accessory);
            
        } else if (serv_type == SERV_TYPE_TH_SENSOR) {
            new_th_sensor(acc_count, serv_count, total_services, json_accessory);
            
        } else if (serv_type == SERV_TYPE_HUMIDIFIER ||
                   serv_type == SERV_TYPE_HUMIDIFIER_WITH_TEMP) {
            new_humidifier(acc_count, serv_count, total_services, json_accessory, serv_type);
            
        } else if (serv_type == SERV_TYPE_LIGHTBULB) {
            new_lightbulb(acc_count, serv_count, total_services, json_accessory);
            
        } else if (serv_type == SERV_TYPE_GARAGE_DOOR) {
            new_garage_door(acc_count, serv_count, total_services, json_accessory);
            
        } else if (serv_type == SERV_TYPE_WINDOW_COVER) {
            new_window_cover(acc_count, serv_count, total_services, json_accessory);
            
        } else if (serv_type == SERV_TYPE_LIGHT_SENSOR) {
            new_light_sensor(acc_count, serv_count, total_services, json_accessory);
            
        } else if (serv_type == SERV_TYPE_SECURITY_SYSTEM) {
            new_sec_system(acc_count, serv_count, total_services, json_accessory);
            
        } else if (serv_type == SERV_TYPE_TV) {
            new_tv(acc_count, serv_count, total_services, json_accessory);
            
        } else if (serv_type == SERV_TYPE_FAN) {
            new_fan(acc_count, serv_count, total_services, json_accessory);
            
        } else if (serv_type == SERV_TYPE_BATTERY) {
            new_battery(acc_count, serv_count, total_services, json_accessory);
            
        } else if (serv_type == SERV_TYPE_POWER_MONITOR) {
            new_power_monitor(acc_count, serv_count, total_services, json_accessory);
            
        } else if (serv_type == SERV_TYPE_FREE_MONITOR ||
                   serv_type == SERV_TYPE_FREE_MONITOR_ACCUMULATVE) {
            new_free_monitor(acc_count, serv_count, total_services, json_accessory, serv_type);
            
        } else if (serv_type == SERV_TYPE_DATA_HISTORY) {
            new_data_history(acc_count, serv_count, total_services, json_accessory);
            
        } else if (serv_type == SERV_TYPE_IAIRZONING) {
            new_iairzoning(json_accessory);
        
        } else {    // serv_type == SERV_TYPE_SWITCH || serv_type == SERV_TYPE_OUTLET
            new_switch(acc_count, serv_count, total_services, json_accessory, serv_type);
        }
        
        show_freeheap();
    }
    
    for (unsigned int i = 0; i < total_accessories; i++) {
        INFO("\n** ACC %i", i + 1);
        
        cJSON* json_accessory = cJSON_GetArrayItem(json_accessories, i);
        int serv_type = get_serv_type(json_accessory);
        
        int service = 0;
        int total_services = get_total_services(serv_type, json_accessory);
        new_service(acc_count, service, total_services, json_accessory, serv_type);
        
        if (acc_homekit_enabled(json_accessory) && serv_type != SERV_TYPE_IAIRZONING) {
            if (cJSON_GetObjectItemCaseSensitive(json_accessory, EXTRA_SERVICES_ARRAY) != NULL) {
                service += get_service_recount(serv_type, json_accessory);

                cJSON* json_extra_services = cJSON_GetObjectItemCaseSensitive(json_accessory, EXTRA_SERVICES_ARRAY);
                for (int m = 0; m < cJSON_GetArraySize(json_extra_services); m++) {
                    cJSON* json_extra_service = cJSON_GetArrayItem(json_extra_services, m);
                    
                    serv_type = get_serv_type(json_extra_service);
                    new_service(acc_count, service, 0, json_extra_service, serv_type);
                    service += get_service_recount(serv_type, json_extra_service);
                    
                    main_config.setup_mode_toggle_counter = INT8_MIN;
                    
                    // Extra service creation delay
                    if (cJSON_GetObjectItemCaseSensitive(json_extra_service, NEXT_SERV_CREATION_DELAY) != NULL) {
                        vTaskDelay(cJSON_GetObjectItemCaseSensitive(json_extra_service, NEXT_SERV_CREATION_DELAY)->valuefloat * MS_TO_TICKS(1000));
                    } else {
                        taskYIELD();
                    }
                }
            }
            
            acc_count++;
        }
        
        main_config.setup_mode_toggle_counter = INT8_MIN;
        
        // Accessory creation delay
        if (cJSON_GetObjectItemCaseSensitive(json_accessory, NEXT_SERV_CREATION_DELAY) != NULL) {
            vTaskDelay(cJSON_GetObjectItemCaseSensitive(json_accessory, NEXT_SERV_CREATION_DELAY)->valuefloat * MS_TO_TICKS(1000));
        } else {
            taskYIELD();
        }
    }
    
    sysparam_set_int32(TOTAL_SERV_SYSPARAM, service_numerator);
    
    INFO("");
    
    // --- HOMEKIT SET CONFIG
    // HomeKit Device Category
    if (cJSON_GetObjectItemCaseSensitive(json_config, HOMEKIT_DEVICE_CATEGORY_SET) != NULL) {
        config.category = (uint16_t) cJSON_GetObjectItemCaseSensitive(json_config, HOMEKIT_DEVICE_CATEGORY_SET)->valuefloat;
        
    } else if (bridge_needed) {
        config.category = HOMEKIT_DEVICE_CATEGORY_BRIDGE;
        
    } else {
        int first_serv_type = 0;
        for (int i = 1; i <= service_numerator; i++) {
            ch_group_t* ch_group = ch_group_find_by_serv(i);
            if (ch_group->homekit_enabled) {
                first_serv_type = ch_group->serv_type;
                break;
            }
        }
        
        switch (first_serv_type) {
            case SERV_TYPE_SWITCH:
                config.category = HOMEKIT_DEVICE_CATEGORY_SWITCH;
                break;
                
            case SERV_TYPE_OUTLET:
                config.category = HOMEKIT_DEVICE_CATEGORY_OUTLET;
                break;
                
            case SERV_TYPE_BUTTON:
            case SERV_TYPE_DOORBELL:
                config.category = HOMEKIT_DEVICE_CATEGORY_PROGRAMMABLE_SWITCH;
                break;
                
            case SERV_TYPE_LOCK:
                config.category = HOMEKIT_DEVICE_CATEGORY_DOOR_LOCK;
                break;
                
            case SERV_TYPE_CONTACT_SENSOR:
            case SERV_TYPE_MOTION_SENSOR:
            case SERV_TYPE_TEMP_SENSOR:
            case SERV_TYPE_HUM_SENSOR:
            case SERV_TYPE_TH_SENSOR:
            case SERV_TYPE_LIGHT_SENSOR:
            case SERV_TYPE_AIR_QUALITY:
                config.category = HOMEKIT_DEVICE_CATEGORY_SENSOR;
                break;
                
            case SERV_TYPE_WATER_VALVE:
                config.category = HOMEKIT_DEVICE_CATEGORY_FAUCET;
                break;
                
            case SERV_TYPE_THERMOSTAT:
            case SERV_TYPE_THERMOSTAT_WITH_HUM:
            case SERV_TYPE_IAIRZONING:
                config.category = HOMEKIT_DEVICE_CATEGORY_AIR_CONDITIONER;
                break;
                
            case SERV_TYPE_HUMIDIFIER:
            case SERV_TYPE_HUMIDIFIER_WITH_TEMP:
                config.category = HOMEKIT_DEVICE_CATEGORY_HUMIDIFIER;
                break;
                
            case SERV_TYPE_LIGHTBULB:
                config.category = HOMEKIT_DEVICE_CATEGORY_LIGHTBULB;
                break;
                
            case SERV_TYPE_GARAGE_DOOR:
                config.category = HOMEKIT_DEVICE_CATEGORY_GARAGE;
                break;
                
            case SERV_TYPE_WINDOW_COVER:
                config.category = HOMEKIT_DEVICE_CATEGORY_WINDOW_COVERING;
                break;
                
            case SERV_TYPE_SECURITY_SYSTEM:
                config.category = HOMEKIT_DEVICE_CATEGORY_SECURITY_SYSTEM;
                break;
                
            case SERV_TYPE_TV:
                config.category = HOMEKIT_DEVICE_CATEGORY_TELEVISION;
                break;
                
            case SERV_TYPE_FAN:
                config.category = HOMEKIT_DEVICE_CATEGORY_FAN;
                break;
                
            default:
                config.category = HOMEKIT_DEVICE_CATEGORY_OTHER;
                break;
        }
    }
    
    cJSON_Delete(json_haa);
    cJSON_Delete(init_last_state_json);
    
    unistring_destroy(unistrings);
    
    xTaskCreate(delayed_sensor_task, "DS", DELAYED_SENSOR_START_TASK_SIZE, NULL, DELAYED_SENSOR_START_TASK_PRIORITY, NULL);
    
    //set_unused_gpios();
    
    config.accessories = accessories;
    config.config_number = (uint16_t) last_config_number;
    
    int8_t re_pair = 0;
    sysparam_get_int8(HOMEKIT_RE_PAIR_SYSPARAM, &re_pair);
    if (re_pair == 1) {
        config.re_pair = true;
        sysparam_erase(HOMEKIT_RE_PAIR_SYSPARAM);
        rs_esp_timer_start_forced(rs_esp_timer_create(HOMEKIT_RE_PAIR_TIME_MS, false, NULL, homekit_re_pair_timer));
    }
    
    main_config.setup_mode_toggle_counter = 0;
    
    if (main_config.uart_receiver_data) {
#ifdef ESP_PLATFORM
        uart_receiver_data_t* uart_receiver_data = main_config.uart_receiver_data;
        while (uart_receiver_data) {
            uart_flush(uart_receiver_data->uart_port);
            uart_receiver_data = uart_receiver_data->next;
        }
#else
        uart_flush_rxfifo(0);
#endif
        rs_esp_timer_start_forced(rs_esp_timer_create(RECV_UART_POLL_PERIOD_MS, true, NULL, recv_uart_timer_worker));
    }
    
    int8_t wifi_mode = 0;
    sysparam_get_int8(WIFI_MODE_SYSPARAM, &wifi_mode);
    if (wifi_mode == 4) {
        wifi_mode = 1;
    }
    main_config.wifi_mode = (uint8_t) wifi_mode;
    
    random_task_delay();
    
    //main_config.wifi_status = WIFI_STATUS_CONNECTING;     // Not needed
    
#ifdef ESP_PLATFORM
    wifi_config_init("HAA", run_homekit_server, custom_hostname, 0, wifi_sleep_mode);
#else
    wifi_config_init("HAA", run_homekit_server, custom_hostname, 0);
#endif
    
    led_blink(2);
    
    do_actions(root_device_ch_group, 1);
    
    vTaskDelete(NULL);
}

void irrf_capture_task(void* args) {
    const int irrf_capture_gpio = ((int) args) - 100;
    INFO("\nGPIO %i\n", irrf_capture_gpio);
    //set_used_gpio(irrf_capture_gpio);
    
#ifdef ESP_PLATFORM
    gpio_set_direction(irrf_capture_gpio, GPIO_MODE_INPUT);
    gpio_sleep_set_direction(irrf_capture_gpio, GPIO_MODE_INPUT);
#else
    gpio_enable(irrf_capture_gpio, GPIO_INPUT);
#endif
    
    //set_unused_gpios();
    
    int read, last = true;
    unsigned int i, c = 0;
    uint16_t* buffer = malloc(2048 * sizeof(uint16_t));
#ifdef ESP_PLATFORM
    uint64_t now, new_time, current_time = sdk_system_get_time_raw();
#else
    uint32_t now, new_time, current_time = sdk_system_get_time_raw();
#endif
    
    for (;;) {
        read = gpio_read(irrf_capture_gpio);
        if (read != last) {
            new_time = sdk_system_get_time_raw();
            buffer[c] = new_time - current_time;
            current_time = new_time;
            last = read;
            c++;
        }
        
        now = sdk_system_get_time_raw();
        if (now - current_time > UINT16_MAX) {
            current_time = now;
            if (c > 0) {
                INFO("Packets %i", c - 1);
                for (i = 1; i < c; i++) {
#ifdef ESP_PLATFORM
                    INFO_NNL("%s%5li ",
#else
                    INFO_NNL("%s%5d ",
#endif
                           i & 1 ? "+" : "-",
                           (uint32_t) (buffer[i] * 1.02f));
                    
                    if ((i - 1) % 16 == 15) {
                        INFO_NNL("\n");
                    }
                }
                INFO("\n\nRAW");
                
                for (i = 1; i < c; i++) {
                    char haa_code[] = "00";
                    
                    haa_code[0] = baseRaw_dic[(buffer[i] / IRRF_CODE_SCALE) / IRRF_CODE_LEN];
                    haa_code[1] = baseRaw_dic[(buffer[i] / IRRF_CODE_SCALE) % IRRF_CODE_LEN];
                    
                    INFO_NNL("%s", haa_code);
                }
                INFO("\n");

                c = 0;
            }
            
            vTaskDelay(10);
        }
    }
}

void wifi_done() {

}

void init_task() {
#ifdef ESP_PLATFORM
    const esp_partition_t* running_partition = esp_ota_get_running_partition();
    if (running_partition->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0) {
        esp_ota_set_boot_partition(esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL));
        vTaskDelay(MS_TO_TICKS(1000));
        sdk_system_restart();
    }
    
    //free(running);
#endif
                        
    // Sysparam starter
    sysparam_status_t status = sysparam_init(SYSPARAMSECTOR, 0);
    if (status != SYSPARAM_OK) {
#ifdef ESP_PLATFORM
        setup_set_boot_installer();
#else
        rboot_set_temp_rom(1);
#endif
        vTaskDelay(MS_TO_TICKS(200));
        sdk_system_restart();
    }
    
#ifdef ESP_PLATFORM
    esp_netif_init();
    esp_event_loop_create_default();
    setup_set_esp_netif(esp_netif_create_default_wifi_sta());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
#endif
    
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    snprintf(main_config.name_value, 11, "HAA-%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    
    int8_t haa_setup = 1;
    
    char *wifi_ssid = NULL;
    sysparam_get_string(WIFI_SSID_SYSPARAM, &wifi_ssid);
    
    //sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 2);    // Force to enter always in setup mode. Only for tests. Keep commented for releases
    
    //sysparam_set_string("ota_repo", "1");             // Simulates Installation with OTA. Only for tests. Keep commented for releases
    
    void enter_setup(const int param) {
        reset_uart();
        
#ifdef ESP_PLATFORM
        adv_logger_init(ADV_LOGGER_UART0, NULL, false);
#endif
        
        //set_unused_gpios();
        
        printf_header();
        INFO("SETUP");
#ifdef ESP_PLATFORM
        wifi_config_init("HAA", NULL, main_config.name_value, param, 0);
#else
        wifi_config_init("HAA", NULL, main_config.name_value, param);
#endif
    }
    
    sysparam_get_int8(HAA_SETUP_MODE_SYSPARAM, &haa_setup);
    if ((uint8_t) haa_setup > 99) {
        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
        
        reset_uart();
        
        if (wifi_ssid) {
            adv_logger_init(ADV_LOGGER_UART0_UDP, NULL, false);
#ifdef ESP_PLATFORM
            wifi_config_init("HAA", wifi_done, main_config.name_value, 0, 0);
#else
            wifi_config_init("HAA", wifi_done, main_config.name_value, 0);
#endif
            
#ifdef ESP_PLATFORM
        } else {
            adv_logger_init(ADV_LOGGER_UART0, NULL, false);
#endif
        }
        
        printf_header();
        
        const int irrf_capture_gpio = (uint8_t) haa_setup;
        xTaskCreate(irrf_capture_task, "CAP", IRRF_CAPTURE_TASK_SIZE, (void*) irrf_capture_gpio, IRRF_CAPTURE_TASK_PRIORITY, NULL);
        
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
            sysparam_compact();
            arming();
        }
        
        if (haa_setup == 1) {
#ifdef HAA_DEBUG
            free_heap_watchdog();
            rs_esp_timer_start_forced(rs_esp_timer_create(1000, true, NULL, free_heap_watchdog));
#endif // HAA_DEBUG
            
            // Arming emergency Setup Mode
            rs_esp_timer_start_forced(rs_esp_timer_create(EXIT_EMERGENCY_SETUP_MODE_TIME, false, NULL, disable_emergency_setup));
            
            name.value = HOMEKIT_STRING(main_config.name_value, .is_static=true);
            
            xTaskCreate(normal_mode_init, "NOM", INITIAL_SETUP_TASK_SIZE, NULL, INITIAL_SETUP_TASK_PRIORITY, NULL);
            
        } else {
            enter_setup(1);
        }
    }
    
    if (wifi_ssid) {
        free(wifi_ssid);
    }
    
    vTaskDelete(NULL);
}

void user_init() {
// GPIO Init
#ifdef ESP_PLATFORM
/*
#if defined(CONFIG_IDF_TARGET_ESP32)
#define FIRST_SPI_GPIO  (6)

#elif defined(CONFIG_IDF_TARGET_ESP32C2) \
    || defined(CONFIG_IDF_TARGET_ESP32C3)
#define FIRST_SPI_GPIO  (12)

#elif defined(CONFIG_IDF_TARGET_ESP32S2) \
    || defined(CONFIG_IDF_TARGET_ESP32S3)
#define FIRST_SPI_GPIO  (22)

#endif

#if defined(CONFIG_IDF_TARGET_ESP32S2)
#define SPI_GPIO_COUNT  (11)
    
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define SPI_GPIO_COUNT  (16)
    
#else
#define SPI_GPIO_COUNT  (6)
    
#endif
    
    for (unsigned int i = 0; i < GPIO_NUM_MAX; i++) {
        if (i == FIRST_SPI_GPIO) {
            i += SPI_GPIO_COUNT;
#if defined(CONFIG_IDF_TARGET_ESP32)
        } else if (i == 24) {
            i ++;
        } else if (i == 28) {
            i += 4;
#endif
        }
        
        gpio_reset_pin(i);
        gpio_set_direction(i, GPIO_MODE_DISABLE);
    }
*/
    for (unsigned int i = 0; i < UART_NUM_MAX; i++) {
        uart_driver_delete(i);
    }
    
#else // ESP-OPEN-RTOS
    
    for (unsigned int i = 0; i < (MAX_GPIOS - 1); i++) {
        if (i == 6) {
            i += 6;
        }
        
        //gpio_enable(i, GPIO_INPUT);
        gpio_disable(i);
    }
    
    gpio_enable(16, GPIO_INPUT);    // GPIO 16 can not be disable
    
#endif
    
    xTaskCreate(init_task, "INI", (TASK_SIZE_FACTOR * 512), NULL, (tskIDLE_PRIORITY + 2), NULL);
}
