/*
 * Home Accessory Architect
 *
 * Copyright 2019-2021 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __HAA_TYPES_H__
#define __HAA_TYPES_H__

typedef struct _autooff_setter_params {
    homekit_characteristic_t* ch;
    uint8_t type;
} autooff_setter_params_t;

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
    
    float inching;
    
    struct _action_binary_output* next;
} action_binary_output_t;

typedef struct _action_acc_manager {
    uint8_t action;
    
    uint8_t accessory: 7;
    bool is_kill_switch: 1;
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

typedef struct _action_ir_tx {
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
    
    struct _action_ir_tx* next;
} action_ir_tx_t;

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

typedef struct _ch_group {
    uint8_t accessory: 6;
    bool main_enabled: 1;
    bool child_enabled: 1;
    uint8_t acc_type: 7;
    bool homekit_enabled: 1;
    
    float num[13];
    
    homekit_characteristic_t* ch0;
    homekit_characteristic_t* ch1;
    homekit_characteristic_t* ch2;
    homekit_characteristic_t* ch3;
    homekit_characteristic_t* ch4;
    homekit_characteristic_t* ch5;
    homekit_characteristic_t* ch6;
    homekit_characteristic_t* ch7;

    TimerHandle_t timer;
    TimerHandle_t timer2;
    
    char* ir_protocol;
    
    action_copy_t* action_copy;
    action_binary_output_t* action_binary_output;
    action_acc_manager_t* action_acc_manager;
    action_system_t* action_system;
    action_network_t* action_network;
    action_ir_tx_t* action_ir_tx;
    action_uart_t* action_uart;
    
    wildcard_action_t* wildcard_action;
    
    float last_wildcard_action[4];
    
    struct _ch_group* next;
} ch_group_t;

typedef struct _action_task {
    uint8_t action;
    ch_group_t* ch_group;
} action_task_t;

//typedef void (*startFunc)(); 
//typedef void (*stopFunc)();
//typedef void (*set_freqFunc)(const uint16_t);
//typedef void (*set_dutyFunc)(const uint8_t, const uint16_t, uint16_t);
//typedef uint16_t (*get_dutyFunc)(const uint8_t);
//typedef void (*new_channelFunc)(const uint8_t, const bool);
typedef struct _lightbulb_func_pointers
{
    void (*start)();
    void (*stop)();
    void (*set_freq)(const uint16_t);
    void (*set_duty)(const uint8_t, const uint16_t, uint16_t);
    uint16_t (*get_duty)(const uint8_t);
    void (*new_channel)(const uint8_t, const bool);
} lightbulb_func_pointers_t;

typedef struct _lightbulb_group {
    uint8_t autodimmer;
    uint8_t autodimmer_task_step;
    bool armed_autodimmer: 1;
    //bool temp_changed: 1;

    uint16_t step;
    uint16_t autodimmer_task_delay;
    
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
    
    lightbulb_func_pointers_t funcs;

    struct _lightbulb_group* next;
} lightbulb_group_t;

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
    uint8_t ir_tx_gpio: 5;
    uint8_t wifi_mode: 3;
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
    
    bool setpwm_is_running: 1;
    bool timetable_ready: 1;
    bool used_gpio_0: 1;
    bool used_gpio_1: 1;
    bool used_gpio_2: 1;
    bool used_gpio_3: 1;
    bool used_gpio_4: 1;
    bool used_gpio_5: 1;
    bool used_gpio_9: 1;
    bool used_gpio_10: 1;
    bool used_gpio_12: 1;
    bool used_gpio_13: 1;
    bool used_gpio_14: 1;
    bool used_gpio_15: 1;
    bool used_gpio_16: 1;
    bool used_gpio_17: 1;
    
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
} main_config_t;

#endif // __HAA_TYPES_H__
