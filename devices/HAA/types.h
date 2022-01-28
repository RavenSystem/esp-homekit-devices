/*
 * Home Accessory Architect
 *
 * Copyright 2019-2021 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __HAA_TYPES_H__
#define __HAA_TYPES_H__

#if defined(ESP_OPEN_RTOS)
    #define MAX_GPIOS                       (18)
#elif defined(ESP_IDF)
    #define MAX_GPIOS                       (40)
#else
    #error "!!! UNKNOWN PLATFORM: ESP_OPEN_RTOS or ESP_IDF"
#endif

typedef struct _last_state {
    char *id;
    homekit_characteristic_t* ch;
    uint8_t ch_type;
    struct _last_state* next;
} last_state_t;

typedef struct _action_copy {
    uint8_t action;
    uint8_t new_action;
    
    struct _action_copy* next;
} action_copy_t;

typedef struct _action_binary_output {
    uint8_t action;
    bool value: 1;
    uint16_t gpio;
    
    uint32_t inching;
    
    struct _action_binary_output* next;
} action_binary_output_t;

typedef struct _action_acc_manager {
    uint8_t action;
    
    uint16_t accessory: 10;

    float value;
    
    struct _action_acc_manager* next;
} action_acc_manager_t;

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
    bool wait_response: 1;
    bool is_running: 1;
    
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
    
    uint8_t uart: 2;
    uint16_t len;
    
    uint16_t pause;
    uint8_t* command;
    
    struct _action_uart* next;
} action_uart_t;

typedef struct _wildcard_action {
    uint8_t index;
    uint8_t target_action;
    bool repeat: 1;
    float value;

    struct _wildcard_action* next;
} wildcard_action_t;

typedef struct _pattern {
    uint8_t* pattern;
    
    uint8_t len;
    int16_t offset;
    
    struct _pattern* next;
} pattern_t;

typedef struct _ch_group {
    uint16_t accessory: 10;
    bool main_enabled: 1;
    bool child_enabled: 1;
    bool homekit_enabled: 1;
    
    uint8_t chs;
    uint8_t acc_type: 7;
    
    homekit_characteristic_t** ch;
    homekit_characteristic_t* ch_target;
    
    int8_t* num_i;
    float* num_f;
    float* last_wildcard_action;
    
    TimerHandle_t timer;
    TimerHandle_t timer2;
    
    char* ir_protocol;
    
    action_copy_t* action_copy;
    action_binary_output_t* action_binary_output;
    action_acc_manager_t* action_acc_manager;
    action_system_t* action_system;
    action_network_t* action_network;
    action_irrf_tx_t* action_irrf_tx;
    action_uart_t* action_uart;
    
    wildcard_action_t* wildcard_action;
    
    struct _ch_group* next;
} ch_group_t;

typedef struct _action_task {
    uint8_t action;
    ch_group_t* ch_group;
} action_task_t;

typedef struct _lightbulb_group {
    uint16_t autodimmer: 10;
    uint8_t channels:3;
    bool armed_autodimmer: 1;
    bool autodimmer_reverse: 1;
    bool lightbulb_task_running: 1;
    uint8_t autodimmer_task_step;
    uint8_t type: 6;
    bool has_changed: 1;
    //bool temp_changed: 1;
    
    uint16_t pwm_dither;
    
    uint16_t step;
    uint16_t autodimmer_task_delay;
    
    int32_t step_value[4];
    
    uint16_t range_start;
    uint16_t range_end;
    
    uint8_t gpio[5];
    
    uint16_t current[5];
    uint16_t target[5];

    float flux[5];
    
    float r[2];
    float g[2];
    float b[2];
    float cw[2];
    float ww[2];
    
    float rgb[3][2];
    float cmy[3][2];
    
    float wp[2];

    homekit_characteristic_t* ch0;
    
    TimerHandle_t timer;
    
    struct _lightbulb_group* next;
} lightbulb_group_t;

typedef struct _addressled {
    uint8_t map[5];
    
    uint8_t gpio;
    
    uint16_t max_range;
    uint16_t time_0;
    uint16_t time_1;
    uint16_t period;
    
    struct _addressled* next;
} addressled_t;

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
    uint8_t count;
    uint8_t times;
    
    bool inverted: 1;
    bool status: 1;
    
    TimerHandle_t timer;
} led_t;

typedef struct _timetable_action {
    uint8_t action;
    
    uint8_t mon: 4;
    uint8_t mday: 5;
    uint8_t hour: 5;
    uint8_t min: 6;
    uint8_t wday: 3;
    
    struct _timetable_action* next;
} timetable_action_t;

typedef struct _main_config {
    uint8_t wifi_status: 2;
    uint8_t wifi_channel: 4;
    bool ir_tx_inv: 1;
    bool setpwm_bool_semaphore: 1;
    uint8_t ir_tx_gpio: 6;
    bool setpwm_is_running: 1;
    bool timetable_ready: 1;
    int8_t setup_mode_toggle_counter;
    int8_t setup_mode_toggle_counter_max;
    
    uint8_t ir_tx_freq: 6;
    bool network_is_busy: 1;
    bool enable_homekit_server: 1;
    uint8_t wifi_ping_max_errors;
    uint8_t wifi_error_count;
    uint8_t wifi_arp_count;
    
    uint16_t setup_mode_time;
    uint16_t wifi_roaming_count;
    
    uint64_t used_gpio: MAX_GPIOS;
    
    uint8_t rf_tx_gpio: 6;
    bool rf_tx_inv: 1;
    bool clock_ready: 1;
    uint8_t wifi_ip;
    uint8_t uart_buffer_len;
    uint8_t wifi_mode: 3;
    
    uint8_t uart_min_len;
    uint8_t uart_max_len;
    
    char name_value[11];
    char serial_value[13];
    
    float ping_poll_period;
    
    TimerHandle_t setup_mode_toggle_timer;
    TimerHandle_t set_lightbulb_timer;
    
    ch_group_t* ch_groups;
    ping_input_t* ping_inputs;
    lightbulb_group_t* lightbulb_groups;
    last_state_t* last_states;
    
    mcp23017_t* mcp23017s;
    
    char* ntp_host;
    timetable_action_t* timetable_actions;
    
    led_t* status_led;
    
    uint8_t* uart_buffer;
    
    addressled_t* addressleds;
} main_config_t;

#endif // __HAA_TYPES_H__
