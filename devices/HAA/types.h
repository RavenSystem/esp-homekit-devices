/*
 * Home Accessory Architect
 *
 * Copyright 2019-2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __HAA_TYPES_H__
#define __HAA_TYPES_H__

typedef struct _autoswitch_params {
    uint8_t gpio;
    bool value;
    float time;
} autoswitch_params_t;

typedef struct _autooff_setter_params {
    homekit_characteristic_t *ch;
    uint8_t type;
    float time;
} autooff_setter_params_t;

typedef struct _last_state {
    char *id;
    homekit_characteristic_t *ch;
    uint8_t ch_type;
    struct _last_state *next;
} last_state_t;

typedef struct _action_copy {
    uint8_t action;
    
    uint8_t new_action;
    
    struct _action_copy *next;
} action_copy_t;

typedef struct _action_relay {
    uint8_t action;
    
    uint8_t gpio;
    bool value;
    float inching;
    
    struct _action_relay *next;
} action_relay_t;

typedef struct _action_acc_manager {
    uint8_t action;
    
    uint8_t accessory;
    bool is_kill_switch;
    float value;
    
    struct _action_acc_manager *next;
} action_acc_manager_t;

typedef struct _action_system {
    uint8_t action;
    
    uint8_t value;
    
    struct _action_system *next;
} action_system_t;

typedef struct _action_http {
    uint8_t action;
    
    char *host;
    char *url;
    uint16_t port_n;
    uint8_t method_n;
    char *content;
    
    struct _action_http *next;
} action_http_t;

typedef struct _action_ir_tx {
    uint8_t action;
    
    char *prot;
    char *prot_code;
    char *raw_code;
    uint8_t freq;
    uint8_t repeats;
    uint16_t pause;
    
    struct _action_ir_tx *next;
} action_ir_tx_t;

typedef struct _wildcard_action {
    uint8_t index;
    float value;
    
    uint8_t target_action;
    bool repeat;
    
    struct _wildcard_action *next;
} wildcard_action_t;

typedef struct _ch_group {
    uint8_t accessory;
    uint8_t acc_type;
    
    homekit_characteristic_t *ch0;
    homekit_characteristic_t *ch1;
    homekit_characteristic_t *ch2;
    homekit_characteristic_t *ch3;
    homekit_characteristic_t *ch4;
    homekit_characteristic_t *ch5;
    homekit_characteristic_t *ch6;
    homekit_characteristic_t *ch7;
    homekit_characteristic_t *ch_child;
    homekit_characteristic_t *ch_sec;
    
    float num[9];
    
    ETSTimer *timer;
    
    action_copy_t *action_copy;
    action_relay_t *action_relay;
    action_acc_manager_t *action_acc_manager;
    action_system_t *action_system;
    action_http_t *action_http;
    action_ir_tx_t *action_ir_tx;
    
    wildcard_action_t *wildcard_action;
    
    float last_wildcard_action[2];
    
    struct _ch_group *next;
} ch_group_t;

typedef struct _action_task {
    uint8_t action;
    ch_group_t *ch_group;
} action_task_t;

typedef struct _lightbulb_group {
    homekit_characteristic_t *ch0;

    bool is_pwm;
    
    uint8_t pwm_r;
    uint8_t pwm_g;
    uint8_t pwm_b;
    uint8_t pwm_w;
    
    uint16_t target_r;
    uint16_t target_g;
    uint16_t target_b;
    uint16_t target_w;
    
    float factor_r;
    float factor_g;
    float factor_b;
    float factor_w;
    
    uint16_t step;
    
    uint8_t autodimmer;
    bool armed_autodimmer;
    uint16_t autodimmer_task_delay;
    uint8_t autodimmer_task_step;
    
    struct _lightbulb_group *next;
} lightbulb_group_t;

typedef void (*ping_callback_fn)(uint8_t gpio, void *args, uint8_t param);

typedef struct _ping_input_callback_fn {
    ping_callback_fn callback;
    homekit_characteristic_t *ch;
    uint8_t param;
    
    struct _ping_input_callback_fn *next;
} ping_input_callback_fn_t;

typedef struct _ping_input {
    char *host;
    bool last_response;
    
    ping_input_callback_fn_t *callback_0;
    ping_input_callback_fn_t *callback_1;
    
    struct _ping_input *next;
} ping_input_t;

#endif // __HAA_TYPES_H__
