/*
 * Home Accessory Architect
 *
 * Copyright 2019-2020 José Antonio Jiménez Campos (@RavenSystem)
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

typedef struct _action_relay {
    uint8_t action;
    
    uint8_t gpio: 7;
    bool value: 1;
    float inching;
    
    struct _action_relay* next;
} action_relay_t;

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

typedef struct _action_http {
    uint8_t action;
    
    uint8_t method_n;
    uint16_t port_n;
    
    uint16_t len;
    
    char* host;
    char* url;
    char* content;
    
    struct _action_http* next;
} action_http_t;

typedef struct _action_ir_tx {
    uint8_t action;
    uint8_t freq;
    uint8_t repeats;
    
    uint16_t pause;
    
    char* prot;
    char* prot_code;
    char* raw_code;

    struct _action_ir_tx* next;
} action_ir_tx_t;

typedef struct _action_uart {
    uint8_t action;
    
    uint8_t uart: 2;
    uint16_t len;
    
    uint16_t pause;
    char* command;
    
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
    uint8_t acc_type: 7;
    bool child_enabled: 1;
    bool homekit_enabled: 1;
    
    float num[12];
    
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
    action_relay_t* action_relay;
    action_acc_manager_t* action_acc_manager;
    action_system_t* action_system;
    action_http_t* action_http;
    action_ir_tx_t* action_ir_tx;
    action_uart_t* action_uart;
    
    wildcard_action_t* wildcard_action;
    
    float last_wildcard_action[3];
    
    struct _ch_group* next;
} ch_group_t;

typedef struct _action_task {
    uint8_t action;
    ch_group_t* ch_group;
} action_task_t;

typedef struct _lightbulb_group {
    uint8_t pwm_r;
    uint8_t pwm_g;
    uint8_t pwm_b;
    uint8_t pwm_w;
    
    uint8_t pwm_cw;
    uint8_t pwm_ww;
    uint8_t autodimmer;
    uint8_t autodimmer_task_step;
    
    uint16_t target_r;
    uint16_t target_g;
    
    uint16_t target_b;
    uint16_t target_w;
    
    uint16_t target_cw;
    uint16_t target_ww;
    
    uint16_t step;
    uint16_t autodimmer_task_delay;

    float factor_r;
    float factor_g;
    float factor_b;
    float factor_w;
    float factor_cw;
    float factor_ww;
    
    bool is_pwm: 1;
    bool armed_autodimmer: 1;

    homekit_characteristic_t* ch0;
    
    struct _lightbulb_group* next;
} lightbulb_group_t;

typedef void (*ping_callback_fn)(uint8_t gpio, void* args, uint8_t param);

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

typedef struct _main_config {
    uint8_t wifi_status: 3;
    uint8_t wifi_channel: 4;
    bool enable_homekit_server: 1;
    int8_t setup_mode_toggle_counter;
    int8_t setup_mode_toggle_counter_max;
    uint8_t ir_tx_freq;
    
    uint8_t wifi_ping_max_errors;
    uint8_t wifi_error_count;
    uint8_t ir_tx_gpio: 5;
    bool ir_tx_inv: 1;
    bool setpwm_is_running: 1;
    bool setpwm_bool_semaphore: 1;
    uint8_t led_gpio: 5;
    bool wifi_ping_is_running: 1;
    bool ping_is_running: 1;
    bool used_gpio_0: 1;
    
    uint16_t setup_mode_time;
    bool used_gpio_1: 1;
    bool used_gpio_2: 1;
    bool used_gpio_3: 1;
    bool used_gpio_4: 1;
    bool used_gpio_5: 1;
    bool used_gpio_9: 1;
    bool used_gpio_10: 1;
    bool used_gpio_11: 1;
    bool used_gpio_12: 1;
    bool used_gpio_13: 1;
    bool used_gpio_14: 1;
    bool used_gpio_15: 1;
    bool used_gpio_16: 1;
    bool used_gpio_17: 1;
    
    uint16_t pwm_freq;
    uint16_t multipwm_duty[MULTIPWM_MAX_CHANNELS];
    
    char name_value[11];
    char serial_value[13];
    
    float ping_poll_period;
    
    TimerHandle_t save_states_timer;
    TimerHandle_t setup_mode_toggle_timer;
    TimerHandle_t pwm_timer;
    
    ch_group_t* ch_groups;
    ping_input_t* ping_inputs;
    lightbulb_group_t* lightbulb_groups;
    last_state_t* last_states;
    
    pwm_info_t* pwm_info;
    driver_interface_t* driver_interface;
} main_config_t;

#endif // __HAA_TYPES_H__
