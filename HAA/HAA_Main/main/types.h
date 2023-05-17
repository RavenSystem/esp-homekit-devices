/*
 * Home Accessory Architect
 *
 * Copyright 2019-2022 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __HAA_TYPES_H__
#define __HAA_TYPES_H__

typedef struct _last_state {
    uint8_t ch_type;
    uint16_t ch_state_id;
    
    homekit_characteristic_t* ch;
    
    struct _last_state* next;
} last_state_t;

typedef struct _action_copy {
    uint8_t action;
    uint8_t new_action;
    
    struct _action_copy* next;
} action_copy_t;

typedef struct _action_binary_output {
    uint8_t action;
    uint8_t value;
    uint16_t gpio;
    
    uint32_t inching;
    
    struct _action_binary_output* next;
} action_binary_output_t;

typedef struct _action_serv_manager {
    uint8_t action;
    
    uint16_t serv_index;    // 10 bits
    
    float value;
    
    struct _action_serv_manager* next;
} action_serv_manager_t;

typedef struct _action_system {
    uint8_t action;
    
    uint8_t value;
    
    struct _action_system* next;
} action_system_t;

typedef struct _action_network {
    uint8_t action;
    
    uint8_t method_n;
    uint16_t port_n;
    
    uint16_t len;
    uint8_t wait_response;
    bool is_running;    // 1 bit
    
    char* host;
    
    union {
        struct {
            char* url;
            char* header;
            char* content;
        };
        uint8_t* raw;
    };
    
    struct _action_network* next;
} action_network_t;

typedef struct _action_irrf_tx {
    uint8_t action;
    uint8_t freq;
    uint8_t repeats;
    
    uint16_t pause;
    
    union {
        char* raw_code;
        struct {
            char* prot;
            char* prot_code;
        };
    };
    
    struct _action_irrf_tx* next;
} action_irrf_tx_t;

typedef struct _action_uart {
    uint8_t action;
    
    uint8_t uart;   // 2 bits
    uint16_t len;
    
    uint16_t pause;
    
    uint8_t* command;
    
    struct _action_uart* next;
} action_uart_t;

typedef struct _action_pwm {
    uint8_t action;
    
    int8_t gpio;
    uint16_t duty;
    
    uint16_t dithering;
    uint16_t freq;
    
    struct _action_pwm* next;
} action_pwm_t;

typedef struct _action_set_ch {
    uint8_t action;
    
    uint8_t source_ch;
    uint8_t target_ch;
    
    uint16_t source_serv;
    uint16_t target_serv;
    
    struct _action_set_ch* next;
} action_set_ch_t;

typedef struct _wildcard_action {
    uint8_t index;
    uint8_t target_action;
    bool repeat;    // 1 bit
    float value;

    struct _wildcard_action* next;
} wildcard_action_t;

typedef struct _pattern {
    uint16_t len;
    int16_t offset;
    
    uint8_t* pattern;
    
    struct _pattern* next;
} pattern_t;

typedef struct _ch_group {
    uint16_t serv_index: 12;
    bool main_enabled: 1;
    bool child_enabled: 1;
    bool homekit_enabled: 1;
    bool is_working: 1;
    
    uint8_t chs;
    uint8_t serv_type;
    
    homekit_characteristic_t** ch;
    
    int8_t* num_i;
    float* num_f;
    float* last_wildcard_action;
    
    TimerHandle_t timer;
    TimerHandle_t timer2;
    
    char* ir_protocol;
    
    action_copy_t* action_copy;
    action_binary_output_t* action_binary_output;
    action_serv_manager_t* action_serv_manager;
    action_system_t* action_system;
    action_network_t* action_network;
    action_irrf_tx_t* action_irrf_tx;
    action_uart_t* action_uart;
    action_pwm_t* action_pwm;
    action_set_ch_t* action_set_ch;
    
    wildcard_action_t* wildcard_action;
    
    struct _ch_group* next;
} ch_group_t;

typedef struct _action_task {
    uint8_t action;
    uint8_t errors;
    uint8_t type;
    
    ch_group_t* ch_group;
} action_task_t;

typedef struct _lightbulb_group {
    uint16_t autodimmer: 10;
    uint8_t channels: 3;
    bool armed_autodimmer: 1;
    bool autodimmer_reverse: 1;
    bool lightbulb_task_running: 1;
    uint8_t autodimmer_task_step;
    uint8_t type: 4;
    bool has_changed: 1;
    bool old_on_value: 1;
    bool last_on_action: 1;
    bool _align: 1;
    
    uint16_t step;
    uint16_t autodimmer_task_delay;
    
    int32_t step_value[4];
    
    uint16_t range_start;
    uint16_t range_end;

    TimerHandle_t timer;
    
    homekit_characteristic_t* ch0;
    
    struct _lightbulb_group* next;
    
    float flux[5];
    
    float r[2];
    float g[2];
    float b[2];
    float cw[2];
    float ww[2];
    
    float rgb[3][2];
    float cmy[3][2];
    
    float wp[2];
    
    uint8_t gpio[5];
    
    uint16_t current[5];
    uint16_t target[5];
} lightbulb_group_t;

#ifndef CONFIG_IDF_TARGET_ESP32C2
typedef struct _addressled {
    uint8_t map[5];
    
    uint8_t gpio;
    
    uint16_t max_range;
    
#ifdef ESP_PLATFORM
    rmt_channel_handle_t rmt_channel_handle;
    rmt_encoder_handle_t rmt_encoder_handle;
#else
    uint16_t time_0;
    uint16_t time_1;
    uint16_t period;
#endif
    
    struct _addressled* next;
} addressled_t;
#endif  // no CONFIG_IDF_TARGET_ESP32C2

typedef void (*ping_callback_fn)(uint16_t gpio, void* args, uint8_t param);

typedef struct _ping_input_callback_fn {
    uint8_t param;
    bool disable_without_wifi: 1;
    
    ping_callback_fn callback;
    ch_group_t* ch_group;

    struct _ping_input_callback_fn *next;
} ping_input_callback_fn_t;

typedef struct _ping_input {
    bool last_response: 1;
    bool ignore_last_response: 1;
    
    char* host;
    
    ping_input_callback_fn_t* callback_0;
    ping_input_callback_fn_t* callback_1;
    
    struct _ping_input* next;
} ping_input_t;

typedef struct _str_ch_value {
    char* string;

    struct _str_ch_value* next;
} str_ch_value_t;

typedef struct _mcp23017 {
    uint8_t index;
    uint8_t bus;
    uint8_t addr;
    uint8_t a_outs;
    
    uint8_t b_outs;
    
    struct _mcp23017* next;
} mcp23017_t;

typedef struct _led {
    uint16_t gpio;
    uint8_t count: 7;
    bool status: 1;
    uint8_t times: 7;
    bool inverted: 1;
    
    TimerHandle_t timer;
} led_t;

typedef struct _timetable_action {
    uint8_t action;
    
    uint8_t mon: 4;
    uint8_t mday: 5;
    uint8_t hour: 5;
    uint8_t min: 6;
    uint8_t wday: 4;    // 3 bits
    
    struct _timetable_action* next;
} timetable_action_t;

typedef struct _uart_receiver_data {
    uint8_t uart_port: 7;
    uint8_t uart_has_data: 1;
    uint8_t uart_buffer_len;
    uint8_t uart_min_len;
    uint8_t uart_max_len;
    
    uint8_t* uart_buffer;
    
    struct _uart_receiver_data* next;
} uart_receiver_data_t;

#ifdef ESP_PLATFORM
typedef struct _adc_dac_data {
    adc_oneshot_unit_handle_t adc_oneshot_handle;
} adc_dac_data_t;

typedef struct _pwmh_channel {
    uint8_t gpio;
    uint8_t channel;
    uint8_t timer;
    bool leading;       // 1 bit
    
    struct _pwmh_channel* next;
} pwmh_channel_t;
#endif

typedef struct _main_config {
    uint8_t wifi_status: 2;
    uint8_t wifi_channel: 4;
    bool ir_tx_inv: 1;
    bool rf_tx_inv: 1;
    uint8_t ir_tx_gpio: 6;
    bool timetable_ready: 1;
    bool clock_ready: 1;
    int8_t setup_mode_toggle_counter;
    int8_t setup_mode_toggle_counter_max;
    
    uint16_t setup_mode_time;
    uint16_t wifi_roaming_count;
    
    //uint64_t used_gpio: MAX_GPIOS;
    
    uint8_t wifi_ip;
    uint8_t wifi_ping_max_errors;
    uint8_t wifi_error_count;
    uint8_t rf_tx_gpio: 6;
    bool enable_homekit_server: 1;
    bool uart_recv_is_working: 1;
    
    uint8_t wifi_mode;  // 3 bits
    uint8_t ir_tx_freq; // 6 bits
    uint8_t wifi_arp_count;
    uint8_t wifi_arp_count_max;
    
    float ping_poll_period;
    
    TimerHandle_t setup_mode_toggle_timer;
    TimerHandle_t set_lightbulb_timer;
    
    SemaphoreHandle_t network_busy_mutex;
    
    ch_group_t* ch_groups;
    ping_input_t* ping_inputs;
    lightbulb_group_t* lightbulb_groups;
    last_state_t* last_states;
    
    mcp23017_t* mcp23017s;
    
    char* ntp_host;
    timetable_action_t* timetable_actions;
    
    led_t* status_led;
    
#ifndef CONFIG_IDF_TARGET_ESP32C2
    addressled_t* addressleds;
#endif  // no CONFIG_IDF_TARGET_ESP32C2
    
    uart_receiver_data_t* uart_receiver_data;
    
#ifdef ESP_PLATFORM
    adc_dac_data_t* adc_dac_data;
    pwmh_channel_t* pwmh_channels;
#endif
    
    char name_value[11];
} main_config_t;

#endif // __HAA_TYPES_H__
