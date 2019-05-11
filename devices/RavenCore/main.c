/*
 * RavenCore
 * 
 * v0.8.10
 * 
 * Copyright 2018-2019 José A. Jiménez (@RavenSystem)
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

/* Device Types
  1. Switch 1ch
  2. Switch 2ch
  3. Socket + Button
  4. Switch 4ch
  5. Thermostat
  6. Switch Basic + TH Sensor
  7. Water Valve
  8. Garage Door
  9. Socket + Button + TH Sensor
 10. ESP01 Switch + Button
 11. Switch 3ch
 12. Window
 13. Lock Mechanism
 14. RGB/W Lights
 15. Stateless Switch
 */

//#include <stdio.h>
//#include <string.h>
#include <esp/uart.h>
//#include <esp8266.h>
//#include <FreeRTOS.h>
//#include <espressif/esp_wifi.h>
//#include <espressif/esp_common.h>
#include <rboot-api.h>
#include <sysparam.h>
//#include <task.h>
#include <math.h>

//#include <etstimer.h>
#include <esplibs/libmain.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <adv_button.h>

#include <multipwm/multipwm.h>

#include <dht/dht.h>
#include <ds18b20/ds18b20.h>

#include "../common/custom_characteristics.h"

// Version
#define FIRMWARE_VERSION                "0.8.10"
#define FIRMWARE_VERSION_OCTAL          001012      // Matches as example: firmware_revision 2.3.8 = 02.03.10 (octal) = config_number 020310

// RGBW
#define INITIAL_R_GPIO                  5
#define INITIAL_G_GPIO                  12
#define INITIAL_B_GPIO                  13
#define INITIAL_W_GPIO                  0

// Shelly 1
#define S1_TOGGLE_GPIO                  5
#define S1_RELAY_GPIO                   4
// Shelly 2
#define S2_TOGGLE1_GPIO                 14
#define S2_TOGGLE2_GPIO                 12
#define S2_RELAY1_GPIO                  4
#define S2_RELAY2_GPIO                  5
// Shelly 2.5
#define S25_TOGGLE1_GPIO                13
#define S25_TOGGLE2_GPIO                5
#define S25_RELAY1_GPIO                 4
#define S25_RELAY2_GPIO                 15
#define S25_BUTTON_GPIO                 2
#define S25_LED_GPIO                    0

// Sonoff
#define LED_GPIO                        13

#define DOOR_OPENED_GPIO                4
#define DOOR_OBSTRUCTION_GPIO           5
//#define DOOR_CLOSED_GPIO              14

#define COVERING_POLL_PERIOD_MS         250

#define TOGGLE_GPIO                     14

#define BUTTON1_GPIO                    0
#define BUTTON2_GPIO                    9
#define BUTTON3_GPIO                    10
//#define BUTTON4_GPIO                  14

#define RELAY1_GPIO                     12
#define RELAY2_GPIO                     5
#define RELAY3_GPIO                     4
#define RELAY4_GPIO                     15

#define DISABLED_TIME                   60
#define ALLOWED_FACTORY_RESET_TIME      60000

#define PWM_RGBW_SCALE                  65535
#define PWM_RGBW_SCALE_OFFSET           100
#define RGBW_DELAY                      10

// SysParam
#define OTA_BETA_SYSPARAM                               "ota_beta"  // Will be removed
#define OTA_REPO_SYSPARAM                               "ota_repo"
#define SHOW_SETUP_SYSPARAM                             "a"
#define DEVICE_TYPE_SYSPARAM                            "b"
#define BOARD_TYPE_SYSPARAM                             "c"
#define EXTERNAL_TOGGLE1_SYSPARAM                       "d"
#define EXTERNAL_TOGGLE2_SYSPARAM                       "e"
#define DHT_SENSOR_TYPE_SYSPARAM                        "f"
#define POLL_PERIOD_SYSPARAM                            "g"
#define VALVE_TYPE_SYSPARAM                             "h"
#define VALVE_SET_DURATION_SYSPARAM                     "i"
#define GARAGEDOOR_WORKING_TIME_SYSPARAM                "j"
#define GARAGEDOOR_HAS_STOP_SYSPARAM                    "k"
#define GARAGEDOOR_SENSOR_CLOSE_NC_SYSPARAM             "l"
#define GARAGEDOOR_SENSOR_OPEN_SYSPARAM                 "m"
#define GARAGEDOOR_SENSOR_OBSTRUCTION_SYSPARAM          "n"
#define GARAGEDOOR_CONTROL_WITH_BUTTON_SYSPARAM         "o"
#define TARGET_TEMPERATURE_SYSPARAM                     "q"
#define INIT_STATE_SW1_SYSPARAM                         "r"
#define INIT_STATE_SW2_SYSPARAM                         "s"
#define INIT_STATE_SW3_SYSPARAM                         "t"
#define INIT_STATE_SW4_SYSPARAM                         "u"
#define INIT_STATE_SWDM_SYSPARAM                        "P"
#define INIT_STATE_TH_SYSPARAM                          "v"
#define LAST_STATE_SW1_SYSPARAM                         "w"
#define LAST_STATE_SW2_SYSPARAM                         "x"
#define LAST_STATE_SW3_SYSPARAM                         "y"
#define LAST_STATE_SW4_SYSPARAM                         "z"
#define LAST_STATE_SWDM_SYSPARAM                        "O"
#define LAST_TARGET_STATE_TH_SYSPARAM                   "0"
#define LOG_OUTPUT_SYSPARAM                             "1"
#define HUM_OFFSET_SYSPARAM                             "2"
#define TEMP_OFFSET_SYSPARAM                            "3"
#define REVERSE_SW1_SYSPARAM                            "4"
#define REVERSE_SW2_SYSPARAM                            "5"
#define REVERSE_SW3_SYSPARAM                            "6"
#define REVERSE_SW4_SYSPARAM                            "7"
#define COVERING_UP_TIME_SYSPARAM                       "8"
#define COVERING_DOWN_TIME_SYSPARAM                     "9"
#define COVERING_TYPE_SYSPARAM                          "!"
#define COVERING_LAST_POSITION_SYSPARAM                 "A"
#define TEMP_DEADBAND_SYSPARAM                          "B"
#define R_GPIO_SYSPARAM                                 "C"
#define G_GPIO_SYSPARAM                                 "D"
#define B_GPIO_SYSPARAM                                 "E"
#define W_GPIO_SYSPARAM                                 "F"
#define LAST_STATE_BRIGHTNESS_SYSPARAM                  "G"
#define LAST_STATE_HUE_SYSPARAM                         "H"
#define LAST_STATE_SATURATION_SYSPARAM                  "I"
#define COLOR_BOOST_SYSPARAM                            "J"
#define INCHING_TIME1_SYSPARAM                          "p"
#define INCHING_TIME2_SYSPARAM                          "K"
#define INCHING_TIME3_SYSPARAM                          "L"
#define INCHING_TIME4_SYSPARAM                          "M"
#define INCHING_TIMEDM_SYSPARAM                         "N"
#define ENABLE_DUMMY_SWITCH_SYSPARAM                    "Q"

bool is_moving = false;
uint8_t device_type_static = 1, reset_toggle_counter = 0, gd_time_state = 0;
uint8_t button1_gpio = BUTTON1_GPIO, button2_gpio = BUTTON2_GPIO, relay1_gpio = RELAY1_GPIO, relay2_gpio = RELAY2_GPIO, led_gpio = LED_GPIO, extra_gpio = TOGGLE_GPIO;
uint8_t r_gpio, g_gpio, b_gpio, w_gpio;
volatile uint32_t last_press_time;
volatile float old_humidity_value = 0.0, old_temperature_value = 0.0, covering_actual_pos = 0.0, covering_step_time_up = 0.2, covering_step_time_down = 0.2;
ETSTimer device_restart_timer, factory_default_toggle_timer, change_settings_timer, save_states_timer, extra_func_timer;
pwm_info_t pwm_info;

homekit_value_t hkc_general_getter(const homekit_characteristic_t *ch);

void switch1_on_callback(homekit_value_t value);
void switch2_on_callback(homekit_value_t value);
void switch3_on_callback(homekit_value_t value);
void switch4_on_callback(homekit_value_t value);
void switchdm_on_callback(homekit_value_t value);

void th_target(homekit_value_t value);
void update_th_state();

void valve_on_callback(homekit_value_t value);

void garage_on_callback(homekit_value_t value);

void covering_on_callback(homekit_value_t value);

void lock_on_callback(homekit_value_t value);

void rgbw_set();
void brightness_callback(homekit_value_t value);
void hue_callback(homekit_value_t value);
void saturation_callback(homekit_value_t value);
void color_boost_callback();

void show_setup_callback();
void ota_firmware_callback();
void change_settings_callback();
void save_states_callback();
homekit_value_t read_ip_addr();

void create_accessory();

// ---------- CHARACTERISTICS ----------
// Switches
homekit_characteristic_t switch1_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter_ex=hkc_general_getter, .setter=switch1_on_callback);
homekit_characteristic_t switch2_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter_ex=hkc_general_getter, .setter=switch2_on_callback);
homekit_characteristic_t switch3_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter_ex=hkc_general_getter, .setter=switch3_on_callback);
homekit_characteristic_t switch4_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter_ex=hkc_general_getter, .setter=switch4_on_callback);
homekit_characteristic_t switchdm_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter_ex=hkc_general_getter, .setter=switchdm_on_callback);

// For Outlets
homekit_characteristic_t switch_outlet_in_use = HOMEKIT_CHARACTERISTIC_(OUTLET_IN_USE, true);   // It has not a real use, but it is a mandatory characteristic

// Stateless Button
homekit_characteristic_t button_event = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);

// Thermostat
homekit_characteristic_t current_temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0, .min_value=(float[]) {-100}, .max_value=(float[]) {200});
homekit_characteristic_t target_temperature  = HOMEKIT_CHARACTERISTIC_(TARGET_TEMPERATURE, 23, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(update_th_state));
homekit_characteristic_t units = HOMEKIT_CHARACTERISTIC_(TEMPERATURE_DISPLAY_UNITS, 0);
homekit_characteristic_t current_state = HOMEKIT_CHARACTERISTIC_(CURRENT_HEATING_COOLING_STATE, 0);
homekit_characteristic_t target_state = HOMEKIT_CHARACTERISTIC_(TARGET_HEATING_COOLING_STATE, 0, .getter_ex=hkc_general_getter, .setter=th_target);
homekit_characteristic_t current_humidity = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);

// Water Valve
homekit_characteristic_t active = HOMEKIT_CHARACTERISTIC_(ACTIVE, 0, .getter_ex=hkc_general_getter, .setter=valve_on_callback);
homekit_characteristic_t in_use = HOMEKIT_CHARACTERISTIC_(IN_USE, 0, .getter_ex=hkc_general_getter);
homekit_characteristic_t valve_type = HOMEKIT_CHARACTERISTIC_(VALVE_TYPE, 0);
homekit_characteristic_t set_duration = HOMEKIT_CHARACTERISTIC_(SET_DURATION, 900, .max_value=(float[]) {14400}, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(save_states_callback));
homekit_characteristic_t remaining_duration = HOMEKIT_CHARACTERISTIC_(REMAINING_DURATION, 0, .max_value=(float[]) {14400}, .getter_ex=hkc_general_getter);

// Garage Door
homekit_characteristic_t current_door_state = HOMEKIT_CHARACTERISTIC_(CURRENT_DOOR_STATE, 1);
homekit_characteristic_t target_door_state = HOMEKIT_CHARACTERISTIC_(TARGET_DOOR_STATE, 1, .getter_ex=hkc_general_getter, .setter=garage_on_callback);
homekit_characteristic_t obstruction_detected = HOMEKIT_CHARACTERISTIC_(OBSTRUCTION_DETECTED, false);

// Window Covering
homekit_characteristic_t covering_current_position = HOMEKIT_CHARACTERISTIC_(CURRENT_POSITION, 0);
homekit_characteristic_t covering_target_position = HOMEKIT_CHARACTERISTIC_(TARGET_POSITION, 0, .getter_ex=hkc_general_getter, .setter=covering_on_callback);
homekit_characteristic_t covering_position_state = HOMEKIT_CHARACTERISTIC_(POSITION_STATE, 2);

// Lock Mechanism
homekit_characteristic_t lock_current_state = HOMEKIT_CHARACTERISTIC_(LOCK_CURRENT_STATE, 1);
homekit_characteristic_t lock_target_state = HOMEKIT_CHARACTERISTIC_(LOCK_TARGET_STATE, 1, .getter_ex=hkc_general_getter, .setter=lock_on_callback);

// RGBW
homekit_characteristic_t brightness = HOMEKIT_CHARACTERISTIC_(BRIGHTNESS, 100, .getter_ex=hkc_general_getter, .setter=brightness_callback);
homekit_characteristic_t hue = HOMEKIT_CHARACTERISTIC_(HUE, 0, .getter_ex=hkc_general_getter, .setter=hue_callback);
homekit_characteristic_t saturation = HOMEKIT_CHARACTERISTIC_(SATURATION, 0, .getter_ex=hkc_general_getter, .setter=saturation_callback);

// ---------- SETUP ----------
// General Setup
homekit_characteristic_t show_setup = HOMEKIT_CHARACTERISTIC_(CUSTOM_SHOW_SETUP, true, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(show_setup_callback));
homekit_characteristic_t device_type = HOMEKIT_CHARACTERISTIC_(CUSTOM_DEVICE_TYPE, 1, .id=102, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t board_type = HOMEKIT_CHARACTERISTIC_(CUSTOM_BOARD_TYPE, 1, .id=126, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t reboot_device = HOMEKIT_CHARACTERISTIC_(CUSTOM_REBOOT_DEVICE, false, .id=103, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(ota_firmware_callback));
homekit_characteristic_t ota_firmware = HOMEKIT_CHARACTERISTIC_(CUSTOM_OTA_UPDATE, false, .id=110, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(ota_firmware_callback));
homekit_characteristic_t log_output = HOMEKIT_CHARACTERISTIC_(CUSTOM_LOG_OUTPUT, 1, .id=129, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t ip_addr = HOMEKIT_CHARACTERISTIC_(CUSTOM_IP_ADDR, "", .id=130, .getter=read_ip_addr);
homekit_characteristic_t wifi_reset = HOMEKIT_CHARACTERISTIC_(CUSTOM_WIFI_RESET, false, .id=131, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t enable_dummy_switch = HOMEKIT_CHARACTERISTIC_(CUSTOM_SWITCH_DM, false, .id=150, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

homekit_characteristic_t custom_inching_time1 = HOMEKIT_CHARACTERISTIC_(CUSTOM_INCHING_TIME1, 0, .id=117, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_inching_time2 = HOMEKIT_CHARACTERISTIC_(CUSTOM_INCHING_TIME2, 0, .id=134, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_inching_time3 = HOMEKIT_CHARACTERISTIC_(CUSTOM_INCHING_TIME3, 0, .id=147, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_inching_time4 = HOMEKIT_CHARACTERISTIC_(CUSTOM_INCHING_TIME4, 0, .id=148, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_inching_timedm = HOMEKIT_CHARACTERISTIC_(CUSTOM_INCHING_TIMEDM, 0, .id=149, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

// Switch Setup
homekit_characteristic_t external_toggle1 = HOMEKIT_CHARACTERISTIC_(CUSTOM_EXTERNAL_TOGGLE1, 0, .id=111, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t external_toggle2 = HOMEKIT_CHARACTERISTIC_(CUSTOM_EXTERNAL_TOGGLE2, 0, .id=128, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

// Thermostat Setup
homekit_characteristic_t dht_sensor_type = HOMEKIT_CHARACTERISTIC_(CUSTOM_DHT_SENSOR_TYPE, 2, .id=112, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t hum_offset = HOMEKIT_CHARACTERISTIC_(CUSTOM_HUMIDITY_OFFSET, 0, .id=132, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t temp_offset = HOMEKIT_CHARACTERISTIC_(CUSTOM_TEMPERATURE_OFFSET, 0.0, .id=133, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t poll_period = HOMEKIT_CHARACTERISTIC_(CUSTOM_TH_PERIOD, 30, .id=125, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t temp_deadband = HOMEKIT_CHARACTERISTIC_(CUSTOM_TEMPERATURE_DEADBAND, 0.0, .id=142, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

// Water Valve Setup
homekit_characteristic_t custom_valve_type = HOMEKIT_CHARACTERISTIC_(CUSTOM_VALVE_TYPE, 0, .id=113, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

// Garage Door Setup
homekit_characteristic_t custom_garagedoor_has_stop = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_HAS_STOP, true, .id=114, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_sensor_close_nc = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_SENSOR_CLOSE_NC, false, .id=115, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_sensor_open = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_SENSOR_OPEN, 0, .id=116, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_working_time = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_WORKINGTIME, 20, .id=118, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_control_with_button = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_CONTROL_WITH_BUTTON, false, .id=119, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_garagedoor_sensor_obstruction = HOMEKIT_CHARACTERISTIC_(CUSTOM_GARAGEDOOR_SENSOR_OBSTRUCTION, 0, .id=127, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

// Window Covering
homekit_characteristic_t custom_covering_up_time = HOMEKIT_CHARACTERISTIC_(CUSTOM_COVERING_UP_TIME, 30, .id=139, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_covering_down_time = HOMEKIT_CHARACTERISTIC_(CUSTOM_COVERING_DOWN_TIME, 30, .id=140, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_covering_type = HOMEKIT_CHARACTERISTIC_(CUSTOM_COVERING_TYPE, 0, .id=141, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

// RGBW
homekit_characteristic_t custom_r_gpio = HOMEKIT_CHARACTERISTIC_(CUSTOM_R_GPIO, INITIAL_R_GPIO, .id=143, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_g_gpio = HOMEKIT_CHARACTERISTIC_(CUSTOM_G_GPIO, INITIAL_G_GPIO, .id=144, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_b_gpio = HOMEKIT_CHARACTERISTIC_(CUSTOM_B_GPIO, INITIAL_B_GPIO, .id=145, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_w_gpio = HOMEKIT_CHARACTERISTIC_(CUSTOM_W_GPIO, INITIAL_W_GPIO, .id=146, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_color_boost = HOMEKIT_CHARACTERISTIC_(CUSTOM_COLOR_BOOST, 0, .id=147, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(color_boost_callback));

// Initial State Setup
homekit_characteristic_t custom_init_state_sw1 = HOMEKIT_CHARACTERISTIC_(CUSTOM_INIT_STATE_SW1, 0, .id=120, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_init_state_sw2 = HOMEKIT_CHARACTERISTIC_(CUSTOM_INIT_STATE_SW2, 0, .id=121, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_init_state_sw3 = HOMEKIT_CHARACTERISTIC_(CUSTOM_INIT_STATE_SW3, 0, .id=122, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_init_state_sw4 = HOMEKIT_CHARACTERISTIC_(CUSTOM_INIT_STATE_SW4, 0, .id=123, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_init_state_swdm = HOMEKIT_CHARACTERISTIC_(CUSTOM_INIT_STATE_SWDM, 0, .id=151, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_init_state_th = HOMEKIT_CHARACTERISTIC_(CUSTOM_INIT_STATE_TH, 0, .id=124, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

// Reverse relays
homekit_characteristic_t custom_reverse_sw1 = HOMEKIT_CHARACTERISTIC_(CUSTOM_REVERSE_SW1, false, .id=135, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_reverse_sw2 = HOMEKIT_CHARACTERISTIC_(CUSTOM_REVERSE_SW2, false, .id=136, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_reverse_sw3 = HOMEKIT_CHARACTERISTIC_(CUSTOM_REVERSE_SW3, false, .id=137, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));
homekit_characteristic_t custom_reverse_sw4 = HOMEKIT_CHARACTERISTIC_(CUSTOM_REVERSE_SW4, false, .id=138, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(change_settings_callback));

// Last used ID = 151
// ---------------------------

void led_write(bool on) {
    gpio_write(led_gpio, on ? 0 : 1);
}

void led_task(void *pvParameters) {
    const uint8_t times = (int) pvParameters;
    
    for (int i=0; i<times; i++) {
        led_write(true);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        led_write(false);
        vTaskDelay(130 / portTICK_PERIOD_MS);
    }
    
    vTaskDelete(NULL);
}

void relay_write(bool on, const uint8_t gpio) {
    if (custom_reverse_sw1.value.bool_value && gpio == relay1_gpio ) {
        on = !on;
    } else if (custom_reverse_sw2.value.bool_value && gpio == relay2_gpio ) {
        on = !on;
    } else if (custom_reverse_sw3.value.bool_value && gpio == RELAY3_GPIO ) {
        on = !on;
    } else if (custom_reverse_sw4.value.bool_value && gpio == RELAY4_GPIO ) {
        on = !on;
    }
    
    last_press_time = xTaskGetTickCountFromISR() + (DISABLED_TIME / portTICK_PERIOD_MS);
    adv_button_set_disable_time();
    gpio_write(gpio, on ? 1 : 0);
}

void save_settings() {
    printf("RC > Saving settings\n");
    
    sysparam_status_t status, flash_error = SYSPARAM_OK;
    
    status = sysparam_set_bool(SHOW_SETUP_SYSPARAM, show_setup.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(DEVICE_TYPE_SYSPARAM, device_type.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(BOARD_TYPE_SYSPARAM, board_type.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    if (board_type.value.int_value == 4) {
        log_output.value.int_value = 0;
    }
    
    status = sysparam_set_int8(LOG_OUTPUT_SYSPARAM, log_output.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(EXTERNAL_TOGGLE1_SYSPARAM, external_toggle1.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(EXTERNAL_TOGGLE2_SYSPARAM, external_toggle2.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }

    status = sysparam_set_int8(DHT_SENSOR_TYPE_SYSPARAM, dht_sensor_type.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(HUM_OFFSET_SYSPARAM, hum_offset.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int32(TEMP_OFFSET_SYSPARAM, temp_offset.value.float_value * 100);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int32(TEMP_DEADBAND_SYSPARAM, temp_deadband.value.float_value * 100);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(POLL_PERIOD_SYSPARAM, poll_period.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }

    status = sysparam_set_int8(VALVE_TYPE_SYSPARAM, custom_valve_type.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }

    status = sysparam_set_int8(GARAGEDOOR_WORKING_TIME_SYSPARAM, custom_garagedoor_working_time.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_bool(GARAGEDOOR_HAS_STOP_SYSPARAM, custom_garagedoor_has_stop.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_bool(GARAGEDOOR_SENSOR_CLOSE_NC_SYSPARAM, custom_garagedoor_sensor_close_nc.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(GARAGEDOOR_SENSOR_OPEN_SYSPARAM, custom_garagedoor_sensor_open.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(GARAGEDOOR_SENSOR_OBSTRUCTION_SYSPARAM, custom_garagedoor_sensor_obstruction.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_bool(GARAGEDOOR_CONTROL_WITH_BUTTON_SYSPARAM, custom_garagedoor_control_with_button.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int32(INCHING_TIME1_SYSPARAM, custom_inching_time1.value.float_value * 100);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int32(INCHING_TIME2_SYSPARAM, custom_inching_time2.value.float_value * 100);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int32(INCHING_TIME3_SYSPARAM, custom_inching_time3.value.float_value * 100);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int32(INCHING_TIME4_SYSPARAM, custom_inching_time4.value.float_value * 100);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int32(INCHING_TIMEDM_SYSPARAM, custom_inching_timedm.value.float_value * 100);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int32(COVERING_UP_TIME_SYSPARAM, custom_covering_up_time.value.float_value * 100);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int32(COVERING_DOWN_TIME_SYSPARAM, custom_covering_down_time.value.float_value * 100);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(COVERING_TYPE_SYSPARAM, custom_covering_type.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(R_GPIO_SYSPARAM, custom_r_gpio.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(G_GPIO_SYSPARAM, custom_g_gpio.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(B_GPIO_SYSPARAM, custom_b_gpio.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(W_GPIO_SYSPARAM, custom_w_gpio.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(COLOR_BOOST_SYSPARAM, custom_color_boost.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(INIT_STATE_SW1_SYSPARAM, custom_init_state_sw1.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(INIT_STATE_SW2_SYSPARAM, custom_init_state_sw2.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(INIT_STATE_SW3_SYSPARAM, custom_init_state_sw3.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(INIT_STATE_SW4_SYSPARAM, custom_init_state_sw4.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(INIT_STATE_SWDM_SYSPARAM, custom_init_state_swdm.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(INIT_STATE_TH_SYSPARAM, custom_init_state_th.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_bool(REVERSE_SW1_SYSPARAM, custom_reverse_sw1.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_bool(REVERSE_SW2_SYSPARAM, custom_reverse_sw2.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_bool(REVERSE_SW3_SYSPARAM, custom_reverse_sw3.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_bool(REVERSE_SW4_SYSPARAM, custom_reverse_sw4.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_bool(ENABLE_DUMMY_SWITCH_SYSPARAM, enable_dummy_switch.value.bool_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    if (flash_error != SYSPARAM_OK) {
        printf("RC ! Saving settings error -> %i\n", flash_error);
    }
}

void save_states() {
    printf("RC > Saving last states\n");
    
    sysparam_status_t status, flash_error = SYSPARAM_OK;
    
    if (custom_init_state_sw1.value.int_value > 1) {
        status = sysparam_set_bool(LAST_STATE_SW1_SYSPARAM, switch1_on.value.bool_value);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    if (custom_init_state_sw2.value.int_value > 1) {
        status = sysparam_set_bool(LAST_STATE_SW2_SYSPARAM, switch2_on.value.bool_value);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    if (custom_init_state_sw3.value.int_value > 1) {
        status = sysparam_set_bool(LAST_STATE_SW3_SYSPARAM, switch3_on.value.bool_value);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    if (custom_init_state_sw4.value.int_value > 1) {
        status = sysparam_set_bool(LAST_STATE_SW4_SYSPARAM, switch4_on.value.bool_value);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    if (custom_init_state_swdm.value.int_value > 1) {
        status = sysparam_set_bool(LAST_STATE_SWDM_SYSPARAM, switchdm_on.value.bool_value);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    if (custom_init_state_th.value.int_value > 2) {
        status = sysparam_set_int8(LAST_TARGET_STATE_TH_SYSPARAM, target_state.value.int_value);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_set_int32(TARGET_TEMPERATURE_SYSPARAM, target_temperature.value.float_value * 100);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int32(VALVE_SET_DURATION_SYSPARAM, set_duration.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int32(COVERING_LAST_POSITION_SYSPARAM, covering_actual_pos * 100);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(LAST_STATE_BRIGHTNESS_SYSPARAM, brightness.value.int_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int32(LAST_STATE_HUE_SYSPARAM, hue.value.float_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    status = sysparam_set_int8(LAST_STATE_SATURATION_SYSPARAM, saturation.value.float_value);
    if (status != SYSPARAM_OK) {
        flash_error = status;
    }
    
    if (flash_error != SYSPARAM_OK) {
        printf("RC ! Saving last states error -> %i\n", flash_error);
    }
}

void device_restart_task() {
    vTaskDelay(5500 / portTICK_PERIOD_MS);
    
    if (wifi_reset.value.bool_value) {
        wifi_config_reset();
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    
    if (ota_firmware.value.bool_value) {
        rboot_set_temp_rom(1);
        vTaskDelay(150 / portTICK_PERIOD_MS);
    }
    
    sdk_system_restart();
    
    vTaskDelete(NULL);
}

void device_restart() {
    printf("RC > Restarting device\n");
    xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 6, 1, NULL);
    xTaskCreate(device_restart_task, "device_restart_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}

void show_setup_callback() {
    if (show_setup.value.bool_value) {
        sdk_os_timer_setfn(&device_restart_timer, device_restart, NULL);
        sdk_os_timer_arm(&device_restart_timer, 5000, 0);
    } else {
        sdk_os_timer_disarm(&device_restart_timer);
    }
    
    save_settings();
}

void factory_default_task() {
    printf("RC > Resetting to factory defaults\n");
    
    relay_write(false, relay1_gpio);
    gpio_disable(relay1_gpio);
    
    sysparam_status_t status;
    
    status = sysparam_set_bool(SHOW_SETUP_SYSPARAM, true);
    //status = sysparam_set_int8(BOARD_TYPE_SYSPARAM, 1);   // This value is not reseted
    
    if (board_type.value.int_value == 4) {
        status = sysparam_set_int8(DEVICE_TYPE_SYSPARAM, 0);
    } else {
        status = sysparam_set_int8(DEVICE_TYPE_SYSPARAM, 1);
    }
    
    status = sysparam_set_int8(LOG_OUTPUT_SYSPARAM, 1);
    
    status = sysparam_set_int8(EXTERNAL_TOGGLE1_SYSPARAM, 0);
    status = sysparam_set_int8(EXTERNAL_TOGGLE2_SYSPARAM, 0);
    
    status = sysparam_set_int8(DHT_SENSOR_TYPE_SYSPARAM, 2);
    status = sysparam_set_int8(HUM_OFFSET_SYSPARAM, 0);
    status = sysparam_set_int32(TEMP_OFFSET_SYSPARAM, 0 * 100);
    status = sysparam_set_int32(TEMP_DEADBAND_SYSPARAM, 0 * 100);
    status = sysparam_set_int8(POLL_PERIOD_SYSPARAM, 30);
    
    status = sysparam_set_int8(VALVE_TYPE_SYSPARAM, 0);
    status = sysparam_set_int32(VALVE_SET_DURATION_SYSPARAM, 900);
    
    status = sysparam_set_bool(ENABLE_DUMMY_SWITCH_SYSPARAM, false);
    
    status = sysparam_set_int8(GARAGEDOOR_WORKING_TIME_SYSPARAM, 20);
    status = sysparam_set_bool(GARAGEDOOR_HAS_STOP_SYSPARAM, true);
    status = sysparam_set_bool(GARAGEDOOR_SENSOR_CLOSE_NC_SYSPARAM, false);
    status = sysparam_set_int8(GARAGEDOOR_SENSOR_OPEN_SYSPARAM, 0);
    status = sysparam_set_int8(GARAGEDOOR_SENSOR_OBSTRUCTION_SYSPARAM, 0);
    status = sysparam_set_bool(GARAGEDOOR_CONTROL_WITH_BUTTON_SYSPARAM, false);
    status = sysparam_set_int32(INCHING_TIME1_SYSPARAM, 0);
    status = sysparam_set_int32(INCHING_TIME2_SYSPARAM, 0);
    status = sysparam_set_int32(INCHING_TIME3_SYSPARAM, 0);
    status = sysparam_set_int32(INCHING_TIME4_SYSPARAM, 0);
    status = sysparam_set_int32(INCHING_TIMEDM_SYSPARAM, 0);
    
    status = sysparam_set_int32(COVERING_UP_TIME_SYSPARAM, 30 * 100);
    status = sysparam_set_int32(COVERING_DOWN_TIME_SYSPARAM, 30 * 100);
    status = sysparam_set_int8(COVERING_TYPE_SYSPARAM, 0);
    status = sysparam_set_int32(COVERING_LAST_POSITION_SYSPARAM, 0);
    
    status = sysparam_set_int8(R_GPIO_SYSPARAM, INITIAL_R_GPIO);
    status = sysparam_set_int8(G_GPIO_SYSPARAM, INITIAL_G_GPIO);
    status = sysparam_set_int8(B_GPIO_SYSPARAM, INITIAL_B_GPIO);
    status = sysparam_set_int8(W_GPIO_SYSPARAM, INITIAL_W_GPIO);
    status = sysparam_set_int8(COLOR_BOOST_SYSPARAM, 0);
    
    status = sysparam_set_int32(TARGET_TEMPERATURE_SYSPARAM, 23 * 100);
    status = sysparam_set_int8(INIT_STATE_SW1_SYSPARAM, 0);
    status = sysparam_set_int8(INIT_STATE_SW2_SYSPARAM, 0);
    status = sysparam_set_int8(INIT_STATE_SW3_SYSPARAM, 0);
    status = sysparam_set_int8(INIT_STATE_SW4_SYSPARAM, 0);
    status = sysparam_set_int8(INIT_STATE_SWDM_SYSPARAM, 0);
    status = sysparam_set_int8(INIT_STATE_TH_SYSPARAM, 0);
    
    status = sysparam_set_bool(REVERSE_SW1_SYSPARAM, false);
    status = sysparam_set_bool(REVERSE_SW2_SYSPARAM, false);
    status = sysparam_set_bool(REVERSE_SW3_SYSPARAM, false);
    status = sysparam_set_bool(REVERSE_SW4_SYSPARAM, false);
    
    status = sysparam_set_bool(LAST_STATE_SW1_SYSPARAM, false);
    status = sysparam_set_bool(LAST_STATE_SW2_SYSPARAM, false);
    status = sysparam_set_bool(LAST_STATE_SW3_SYSPARAM, false);
    status = sysparam_set_bool(LAST_STATE_SW4_SYSPARAM, false);
    status = sysparam_set_bool(LAST_STATE_SWDM_SYSPARAM, false);
    status = sysparam_set_int8(LAST_TARGET_STATE_TH_SYSPARAM, 0);
    
    status = sysparam_set_int8(LAST_STATE_BRIGHTNESS_SYSPARAM, 100);
    status = sysparam_set_int8(LAST_STATE_HUE_SYSPARAM, 0);
    status = sysparam_set_int8(LAST_STATE_SATURATION_SYSPARAM, 0);
    
    if (status != SYSPARAM_OK) {
        printf("RC ! ERROR Flash problem\n");
    }
    
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    homekit_server_reset();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    wifi_config_reset();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    sdk_system_restart();
    
    vTaskDelete(NULL);
}

void factory_default_call(const uint8_t gpio, void *args) {
    printf("RC > Checking factory default call\n");
    
    if (xTaskGetTickCountFromISR() < ALLOWED_FACTORY_RESET_TIME / portTICK_PERIOD_MS) {
        xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 4, 1, NULL);
        xTaskCreate(factory_default_task, "factory_default_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    } else {
        printf("RC ! Reset to factory defaults not allowed after %i msecs since boot. Repower device and try again\n", ALLOWED_FACTORY_RESET_TIME);
    }
}

void factory_default_toggle_upcount() {
    reset_toggle_counter++;
    sdk_os_timer_arm(&factory_default_toggle_timer, 3100, 0);
}

void factory_default_toggle() {
    if (reset_toggle_counter > 10) {
        factory_default_call(0, NULL);
    }
    
    reset_toggle_counter = 0;
}

void ota_firmware_callback() {
    if (ota_firmware.value.bool_value || reboot_device.value.bool_value) {
        sdk_os_timer_setfn(&device_restart_timer, device_restart, NULL);
        sdk_os_timer_arm(&device_restart_timer, 5000, 0);
    } else {
        sdk_os_timer_disarm(&device_restart_timer);
    }
}

void change_settings_callback() {
    sdk_os_timer_arm(&change_settings_timer, 3500, 0);
}

void save_states_callback() {
    sdk_os_timer_arm(&save_states_timer, 5000, 0);
}

// ***** General Getter

homekit_value_t hkc_general_getter(const homekit_characteristic_t *ch) {
    return ch->value;
}

// ***** Switches

void switch_autooff_task(void *pvParameters) {
    const uint8_t switch_number = (int) pvParameters;
    switch (switch_number) {
        case 2:
            vTaskDelay(custom_inching_time2.value.float_value * 1000 / portTICK_PERIOD_MS);
            switch2_on.value.bool_value = false;
            relay_write(switch2_on.value.bool_value, relay2_gpio);
            homekit_characteristic_notify(&switch2_on, switch2_on.value);
            break;
            
        case 3:
            vTaskDelay(custom_inching_time3.value.float_value * 1000 / portTICK_PERIOD_MS);
            switch3_on.value.bool_value = false;
            relay_write(switch3_on.value.bool_value, RELAY3_GPIO);
            homekit_characteristic_notify(&switch3_on, switch3_on.value);
            break;
            
        case 4:
            vTaskDelay(custom_inching_time4.value.float_value * 1000 / portTICK_PERIOD_MS);
            switch4_on.value.bool_value = false;
            relay_write(switch4_on.value.bool_value, RELAY4_GPIO);
            homekit_characteristic_notify(&switch4_on, switch4_on.value);
            break;
            
        case 5:
            vTaskDelay(custom_inching_timedm.value.float_value * 1000 / portTICK_PERIOD_MS);
            switchdm_on.value.bool_value = false;
            homekit_characteristic_notify(&switchdm_on, switchdm_on.value);
            break;
            
        default:    // case 1:
            vTaskDelay(custom_inching_time1.value.float_value * 1000 / portTICK_PERIOD_MS);
            switch1_on.value.bool_value = false;
            relay_write(switch1_on.value.bool_value, relay1_gpio);
            homekit_characteristic_notify(&switch1_on, switch1_on.value);
            break;
    }

    printf("RC > Auto-off SW %i\n", switch_number);
    save_states_callback();
    
    vTaskDelete(NULL);
}

void switch1_on_callback(homekit_value_t value) {
    printf("RC > Toggle SW 1\n");
    switch1_on.value = value;
    
    if (device_type_static != 14) {
        relay_write(switch1_on.value.bool_value, relay1_gpio);
        xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 1, 1, NULL);
        printf("RC > Relay 1 -> %i\n", switch1_on.value.bool_value);
        
        if (custom_inching_time1.value.float_value > 0 && switch1_on.value.bool_value) {
            xTaskCreate(switch_autooff_task, "switch_autooff_task", configMINIMAL_STACK_SIZE, (void *) 1, 1, NULL);
        }
    } else {
        rgbw_set();
    }
    
    factory_default_toggle_upcount();
    
    save_states_callback();
}

void switch2_on_callback(homekit_value_t value) {
    printf("RC > Toggle SW 2\n");
    switch2_on.value = value;
    relay_write(switch2_on.value.bool_value, relay2_gpio);
    xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 1, 1, NULL);
    printf("RC > Relay 2 -> %i\n", switch2_on.value.bool_value);
    
    if (custom_inching_time2.value.float_value > 0 && switch2_on.value.bool_value) {
        xTaskCreate(switch_autooff_task, "switch_autooff_task", configMINIMAL_STACK_SIZE, (void *) 2, 1, NULL);
    }
    
    save_states_callback();
}

void switch3_on_callback(homekit_value_t value) {
    printf("RC > Toggle SW 3\n");
    switch3_on.value = value;
    relay_write(switch3_on.value.bool_value, RELAY3_GPIO);
    xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 1, 1, NULL);
    printf("RC > Relay 3 -> %i\n", switch3_on.value.bool_value);
    
    if (custom_inching_time3.value.float_value > 0 && switch3_on.value.bool_value) {
        xTaskCreate(switch_autooff_task, "switch_autooff_task", configMINIMAL_STACK_SIZE, (void *) 3, 1, NULL);
    }
    
    save_states_callback();
}

void switch4_on_callback(homekit_value_t value) {
    printf("RC > Toggle SW 4\n");
    switch4_on.value = value;
    relay_write(switch4_on.value.bool_value, RELAY4_GPIO);
    xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 1, 1, NULL);
    printf("RC > Relay 4 -> %i\n", switch4_on.value.bool_value);
    
    if (custom_inching_time4.value.float_value > 0 && switch4_on.value.bool_value) {
        xTaskCreate(switch_autooff_task, "switch_autooff_task", configMINIMAL_STACK_SIZE, (void *) 4, 1, NULL);
    }
    
    save_states_callback();
}

void switchdm_on_callback(homekit_value_t value) {
    printf("RC > Toggle SW DM\n");
    switchdm_on.value = value;
    xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 1, 1, NULL);
    
    if (custom_inching_timedm.value.float_value > 0 && switchdm_on.value.bool_value) {
        xTaskCreate(switch_autooff_task, "switch_autooff_task", configMINIMAL_STACK_SIZE, (void *) 5, 1, NULL);
    }
    
    save_states_callback();
}

// ***** Water Valve

void toggle_valve(const uint8_t gpio, void *args) {
    if (active.value.int_value == 1) {
        active.value.int_value = 0;
    } else {
        active.value.int_value = 1;
    }
    
    homekit_characteristic_notify(&active, active.value);
    valve_on_callback(active.value);
}

void valve_control() {
    remaining_duration.value.int_value--;
    if (remaining_duration.value.int_value == 0) {
        printf("RC > Valve OFF\n");
        
        sdk_os_timer_disarm(&extra_func_timer);
        relay_write(false, relay1_gpio);
        
        active.value.int_value = 0;
        in_use.value.int_value = 0;
        
        homekit_characteristic_notify(&active, active.value);
        homekit_characteristic_notify(&in_use, in_use.value);
        
        xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 4, 1, NULL);
    }
}

void valve_on_callback(homekit_value_t value) {
    active.value = value;
    in_use.value.int_value = active.value.int_value;
    
    if (active.value.int_value == 1) {
        printf("RC > Valve ON\n");
        relay_write(true, relay1_gpio);
        
        if (custom_garagedoor_has_stop.value.bool_value) {
            remaining_duration.value = set_duration.value;
            sdk_os_timer_arm(&extra_func_timer, 1000, 1);
        }
    } else {
        printf("RC > Valve manual OFF\n");
        relay_write(false, relay1_gpio);
        
        if (custom_garagedoor_has_stop.value.bool_value) {
            sdk_os_timer_disarm(&extra_func_timer);
            remaining_duration.value.int_value = 0;
        }
    }
    
    homekit_characteristic_notify(&in_use, in_use.value);
    
    if (custom_garagedoor_has_stop.value.bool_value) {
        homekit_characteristic_notify(&remaining_duration, remaining_duration.value);
    }
    
    xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 1, 1, NULL);
}

// ***** Garage Door

void garage_button_task() {
    printf("RC > GD relay working\n");
    if (custom_garagedoor_sensor_open.value.int_value == 0) {
        sdk_os_timer_disarm(&extra_func_timer);
    }
    
    if (obstruction_detected.value.bool_value == false) {
        relay_write(true, relay1_gpio);
        vTaskDelay((custom_inching_time1.value.float_value + 0.05) * 1000 / portTICK_PERIOD_MS);
        relay_write(false, relay1_gpio);

        if (custom_garagedoor_has_stop.value.bool_value && is_moving) {
            vTaskDelay(2500 / portTICK_PERIOD_MS);
            relay_write(true, relay1_gpio);
            vTaskDelay((custom_inching_time1.value.float_value + 0.05) * 1000 / portTICK_PERIOD_MS);
            relay_write(false, relay1_gpio);
        }

        if (custom_garagedoor_sensor_open.value.int_value == 0) {
            if (current_door_state.value.int_value == 0 || current_door_state.value.int_value == 2) {
                printf("RC > GD -> CLOSING\n");
                current_door_state.value.int_value = 3;
                sdk_os_timer_arm(&extra_func_timer, 1000, 1);
            } else if (current_door_state.value.int_value == 3) {
                printf("RC > GD -> OPENING\n");
                current_door_state.value.int_value = 2;
                sdk_os_timer_arm(&extra_func_timer, 1000, 1);
            }
        }
    } else {
        printf("RC ! GD -> OBSTRUCTION DETECTED - RELAY IS OFF\n");
    }

    homekit_characteristic_notify(&current_door_state, current_door_state.value);
    
    vTaskDelete(NULL);
}

void garage_on_callback(homekit_value_t value) {
    printf("RC > GD activated: Current state -> %i, Target state -> %i\n", current_door_state.value.int_value, value.int_value);
    
    uint8_t current_door_state_simple = current_door_state.value.int_value;
    if (current_door_state_simple > 1) {
        current_door_state_simple -= 2;
    }
    
    if (value.int_value != current_door_state_simple) {
        xTaskCreate(garage_button_task, "garage_button_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
        xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 1, 1, NULL);
    } else {
        xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 4, 1, NULL);
    }
    
    target_door_state.value = value;
}

void garage_on_button(const uint8_t gpio, void *args) {
    if (custom_garagedoor_control_with_button.value.bool_value) {
        printf("RC > GD: built-in button PRESSED\n");
        
        if (target_door_state.value.int_value == 0) {
            garage_on_callback(HOMEKIT_UINT8(1));
        } else {
            garage_on_callback(HOMEKIT_UINT8(0));
        }
    } else {
        printf("RC > GD: built-in button DISABLED\n");
        xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 4, 1, NULL);
    }
}

static void homekit_gd_notify() {
    homekit_characteristic_notify(&target_door_state, target_door_state.value);
    homekit_characteristic_notify(&current_door_state, current_door_state.value);
}

void door_opened_0_fn_callback(const uint8_t gpio, void *args) {
    printf("RC > GD -> CLOSING\n");
    is_moving = true;
    target_door_state.value.int_value = 1;
    current_door_state.value.int_value = 3;
    
    homekit_gd_notify();
}

void door_opened_1_fn_callback(const uint8_t gpio, void *args) {
    printf("RC > GD -> OPENED\n");
    is_moving = false;
    target_door_state.value.int_value = 0;
    current_door_state.value.int_value = 0;
    
    homekit_gd_notify();
}

void door_closed_0_fn_callback(const uint8_t gpio, void *args) {
    printf("RC > GD -> OPENING\n");
    is_moving = true;
    target_door_state.value.int_value = 0;
    current_door_state.value.int_value = 2;
    
    if (custom_garagedoor_sensor_open.value.int_value == 0) {
        sdk_os_timer_arm(&extra_func_timer, 1000, 1);
    }
    
    homekit_gd_notify();
}

void door_closed_1_fn_callback(const uint8_t gpio, void *args) {
    if (custom_garagedoor_sensor_open.value.int_value == 0) {
        sdk_os_timer_disarm(&extra_func_timer);
    }
    
    printf("RC > GD -> CLOSED\n");
    gd_time_state = 0;
    is_moving = false;
    target_door_state.value.int_value = 1;
    current_door_state.value.int_value = 1;
    
    homekit_gd_notify();
}

void door_opened_countdown_timer() {
    if (gd_time_state > custom_garagedoor_working_time.value.int_value) {
        gd_time_state = custom_garagedoor_working_time.value.int_value - 1;
    }
    
    if (current_door_state.value.int_value == 2) {
        gd_time_state++;
    
        if (gd_time_state == custom_garagedoor_working_time.value.int_value) {
            printf("RC > GD -> OPENED\n");
            sdk_os_timer_disarm(&extra_func_timer);
            is_moving = false;
            gd_time_state = custom_garagedoor_working_time.value.int_value;
            target_door_state.value.int_value = 0;
            current_door_state.value.int_value = 0;
            
            homekit_gd_notify();
        }
    } else if (current_door_state.value.int_value == 3) {
        gd_time_state--;
        
        if (gd_time_state == 0) {
            sdk_os_timer_disarm(&extra_func_timer);
        }
    }
}

void door_obstructed_0_fn_callback(const uint8_t gpio, void *args) {
    printf("RC > GD -> OBSTRUCTION REMOVED\n");
    obstruction_detected.value.bool_value = false;
    homekit_characteristic_notify(&obstruction_detected, obstruction_detected.value);
}

void door_obstructed_1_fn_callback(const uint8_t gpio, void *args) {
    printf("RC > GD -> OBSTRUCTED\n");
    obstruction_detected.value.bool_value = true;
    homekit_characteristic_notify(&obstruction_detected, obstruction_detected.value);
}

// ***** Window Covering

void normalize_actual_pos() {
    if (covering_actual_pos < 0) {
        covering_actual_pos = 0;
    } else if (covering_actual_pos > 100) {
        covering_actual_pos = 100;
    }
}

void covering_stop() {
    sdk_os_timer_disarm(&extra_func_timer);
    
    relay_write(false, relay1_gpio);
    relay_write(false, relay2_gpio);
    
    normalize_actual_pos();
    
    covering_current_position.value.int_value = covering_actual_pos;
    homekit_characteristic_notify(&covering_current_position, covering_current_position.value);
    
    covering_target_position.value = covering_current_position.value;
    homekit_characteristic_notify(&covering_target_position, covering_target_position.value);
    
    covering_position_state.value.int_value = 2;
    homekit_characteristic_notify(&covering_position_state, covering_position_state.value);
    
    printf("RC > Covering stoped at %f\n", covering_actual_pos);
    xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 1, 1, NULL);
    
    save_states_callback();
}

void covering_on_callback(homekit_value_t value) {
    printf("RC > Covering activated: Current pos -> %i, Target pos -> %i\n", covering_current_position.value.int_value, value.int_value);
    xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 1, 1, NULL);
    
    covering_target_position.value = value;
    homekit_characteristic_notify(&covering_target_position, covering_target_position.value);
    
    normalize_actual_pos();
    
    gd_time_state = 0;      // Used as covering offset to add extra time when target position completely closed or opened
    if (value.int_value == 0 || value.int_value == 100) {
        gd_time_state = 15;
    }
    
    if (value.int_value < covering_current_position.value.int_value) {
        relay_write(false, relay2_gpio);
        covering_position_state.value.int_value = 0;
        
        sdk_os_timer_arm(&extra_func_timer, COVERING_POLL_PERIOD_MS, 1);
        relay_write(true, relay1_gpio);
    } else if (value.int_value > covering_current_position.value.int_value){
        relay_write(false, relay1_gpio);
        covering_position_state.value.int_value = 1;
        
        sdk_os_timer_arm(&extra_func_timer, COVERING_POLL_PERIOD_MS, 1);
        relay_write(true, relay2_gpio);
    } else {
        covering_stop();
    }
    
    homekit_characteristic_notify(&covering_position_state, covering_position_state.value);
}

void covering_worker() {
    void normalize_covering_current_position() {
        if (covering_actual_pos < 0) {
            covering_current_position.value.int_value = 0;
        } else if (covering_actual_pos > 100) {
            covering_current_position.value.int_value = 100;
        } else {
            covering_current_position.value.int_value = covering_actual_pos;
        }
        
        if (((uint8_t)covering_actual_pos * COVERING_POLL_PERIOD_MS) % 2000 == 0) {
            printf("RC > Covering moving at %f\n", covering_actual_pos);
            homekit_characteristic_notify(&covering_current_position, covering_current_position.value);
        }
    }
    
    switch (covering_position_state.value.int_value) {
        case 0: // Down
            covering_actual_pos -= covering_step_time_down;
            normalize_covering_current_position();

            if ((covering_target_position.value.int_value - gd_time_state) >= covering_actual_pos) {
                covering_stop();
            }
            break;
            
        case 1: // Up
            covering_actual_pos += covering_step_time_up;
            normalize_covering_current_position();
            
            if ((covering_target_position.value.int_value + gd_time_state) <= covering_actual_pos) {
                covering_stop();
            }
            break;
            
        default:    // case 2:  // Stop
            covering_stop();
            break;
    }
}

void covering_toggle_up(const uint8_t gpio, void *args) {
    covering_on_callback(HOMEKIT_UINT8(100));
}

void covering_toggle_down(const uint8_t gpio, void *args) {
    covering_on_callback(HOMEKIT_UINT8(0));
    
    factory_default_toggle_upcount();
}

void covering_toggle_stop(const uint8_t gpio, void *args) {
    covering_on_callback(covering_current_position.value);
}

void covering_button_up(const uint8_t gpio, void *args) {
    if (covering_position_state.value.int_value == 1) {
        covering_on_callback(covering_current_position.value);
    } else {
        covering_on_callback(HOMEKIT_UINT8(100));
    }
}

void covering_button_down(const uint8_t gpio, void *args) {
    if (covering_position_state.value.int_value == 0) {
        covering_on_callback(covering_current_position.value);
    } else {
        covering_on_callback(HOMEKIT_UINT8(0));
    }
    
    factory_default_toggle_upcount();
}

// ***** Lock Mechanism

void lock_close() {
    printf("RC > Lock closed\n");
    relay_write(false, relay1_gpio);
    
    lock_target_state.value.int_value = 1;
    homekit_characteristic_notify(&lock_target_state, lock_target_state.value);
    
    lock_current_state.value.int_value = 1;
    homekit_characteristic_notify(&lock_current_state, lock_current_state.value);
}

void lock_close_task() {
    vTaskDelay(custom_inching_time1.value.float_value * 1000 / portTICK_PERIOD_MS);
    
    lock_close();
    
    vTaskDelete(NULL);
}

void lock_on_callback(homekit_value_t value) {
    lock_target_state.value = value;
    
    lock_current_state.value = value;
    homekit_characteristic_notify(&lock_current_state, lock_current_state.value);
    
    if (value.int_value == 0) {
        printf("RC > Lock opened\n");
        xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 1, 1, NULL);
        relay_write(true, relay1_gpio);
        
        if (custom_inching_time1.value.float_value > 0) {
            xTaskCreate(lock_close_task, "lock_close_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
        }
    } else {
        lock_close();
    }
    
    factory_default_toggle_upcount();
}

void lock_intr_callback(const uint8_t gpio, void *args) {
    lock_on_callback(HOMEKIT_UINT8(0));
}

// ***** Buttons

void button_simple1_intr_callback(const uint8_t gpio, void *args) {
    switch1_on.value.bool_value = !switch1_on.value.bool_value;
    switch1_on_callback(switch1_on.value);
    homekit_characteristic_notify(&switch1_on, switch1_on.value);
}

void button_simple2_intr_callback(const uint8_t gpio, void *args) {
    switch2_on.value.bool_value = !switch2_on.value.bool_value;
    switch2_on_callback(switch2_on.value);
    homekit_characteristic_notify(&switch2_on, switch2_on.value);
}

void button_simple3_intr_callback(const uint8_t gpio, void *args) {
    switch3_on.value.bool_value = !switch3_on.value.bool_value;
    switch3_on_callback(switch3_on.value);
    homekit_characteristic_notify(&switch3_on, switch3_on.value);
}

void button_simple4_intr_callback(const uint8_t gpio, void *args) {
    switch4_on.value.bool_value = !switch4_on.value.bool_value;
    switch4_on_callback(switch4_on.value);
    homekit_characteristic_notify(&switch4_on, switch4_on.value);
}

void button_dual_rotation_intr_callback(const uint8_t gpio, void *args) {
    if (!switch1_on.value.bool_value && !switch2_on.value.bool_value) {
        button_simple1_intr_callback(gpio, NULL);
        
    } else if (switch1_on.value.bool_value && !switch2_on.value.bool_value) {
        button_simple1_intr_callback(gpio, NULL);
        button_simple2_intr_callback(gpio, NULL);
        
    } else if (!switch1_on.value.bool_value && switch2_on.value.bool_value) {
        button_simple1_intr_callback(gpio, NULL);
        
    } else {
        button_simple1_intr_callback(gpio, NULL);
        button_simple2_intr_callback(gpio, NULL);
    }
}

void button_event1_intr_callback(const uint8_t gpio, void *args) {
    homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(0));
    printf("RC > Single press\n");
    xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 1, 1, NULL);
}

void button_event2_intr_callback(const uint8_t gpio, void *args) {
    homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(1));
    printf("RC > Double press\n");
    xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 2, 1, NULL);
}

void button_event3_intr_callback(const uint8_t gpio, void *args) {
    homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(2));
    printf("RC > Long press\n");
    xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 3, 1, NULL);
}

// ***** Thermostat

void th_button_intr_callback(const uint8_t gpio, void *args) {
    uint8_t state = target_state.value.int_value + 1;
    switch (state) {
        case 1:
            printf("RC > HEAT\n");
            xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 2, 1, NULL);
            break;
            
        case 2:
            printf("RC > COOL\n");
            xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 3, 1, NULL);
            break;

        default:
            state = 0;
            printf("RC > OFF\n");
            xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 1, 1, NULL);
            break;
    }
    
    target_state.value.int_value = state;
    homekit_characteristic_notify(&target_state, target_state.value);
    
    update_th_state();
}

void th_target(homekit_value_t value) {
    target_state.value = value;
    switch (target_state.value.int_value) {
        case 1:
            printf("RC > HEAT\n");
            xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 2, 1, NULL);
            break;
            
        case 2:
            printf("RC > COOL\n");
            xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 3, 1, NULL);
            break;
            
        default:
            printf("RC > OFF\n");
            xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 1, 1, NULL);
            break;
    }
    
    update_th_state();
}

void update_th_state() {
    void th_state_off() {
        current_state.value.int_value = 0;
        relay_write(false, relay1_gpio);
        homekit_characteristic_notify(&current_state, current_state.value);
    }
    
    switch (target_state.value.int_value) {
        case 0:
            if (current_state.value.int_value != 0) {
                th_state_off();
            }
            break;
            
        case 1:
            if (current_state.value.int_value == 0) {
                if (current_temperature.value.float_value < (target_temperature.value.float_value - temp_deadband.value.float_value)) {
                    current_state.value.int_value = 1;
                    relay_write(true, relay1_gpio);
                    homekit_characteristic_notify(&current_state, current_state.value);
                }
            } else if (current_temperature.value.float_value >= target_temperature.value.float_value) {
                th_state_off();
            }
            break;
            
        case 2:
            if (current_state.value.int_value == 0) {
                if (current_temperature.value.float_value > (target_temperature.value.float_value + temp_deadband.value.float_value)) {
                    current_state.value.int_value = 2;
                    relay_write(true, relay1_gpio);
                    homekit_characteristic_notify(&current_state, current_state.value);
                }
            } else if (current_temperature.value.float_value <= target_temperature.value.float_value) {
                th_state_off();
            }
            break;
            
        default:    // case 3
            target_state.value.int_value = 0;
            homekit_characteristic_notify(&target_state, target_state.value);
            break;
    }
    
    save_states_callback();
}

// ***** Temperature and Humidity sensors

void temperature_sensor_worker() {
    float humidity_value, temperature_value;
    bool get_temp = false;
    
    if (dht_sensor_type.value.int_value < 3) {
        dht_sensor_type_t current_sensor_type = DHT_TYPE_DHT22;
        
        if (dht_sensor_type.value.int_value == 1) {
            current_sensor_type = DHT_TYPE_DHT11;
        }
        
        get_temp = dht_read_float_data(current_sensor_type, extra_gpio, &humidity_value, &temperature_value);
    } else {    // dht_sensor_type.value.int_value == 3
        ds18b20_addr_t ds18b20_addr[1];
        
        if (ds18b20_scan_devices(extra_gpio, ds18b20_addr, 1) == 1) {
            float temps[1];
            ds18b20_measure_and_read_multi(extra_gpio, ds18b20_addr, 1, temps);
            temperature_value = temps[0];
            humidity_value = 0.0;
            get_temp = true;
        }
    }
    
    if (get_temp) {
        temperature_value += temp_offset.value.float_value;
        if (temperature_value < -100) {
            temperature_value = -100;
        } else if (temperature_value > 200) {
            temperature_value = 200;
        }
        
        if (temperature_value != old_temperature_value) {
            old_temperature_value = temperature_value;
            current_temperature.value = HOMEKIT_FLOAT(temperature_value);
            homekit_characteristic_notify(&current_temperature, current_temperature.value);

            if (device_type_static == 5) {
                update_th_state();
            }
        }
        
        humidity_value += hum_offset.value.float_value;
        if (humidity_value < 0) {
            humidity_value = 0;
        } else if (humidity_value > 100) {
            humidity_value = 100;
        }
        
        if (humidity_value != old_humidity_value) {
            old_humidity_value = humidity_value;
            current_humidity.value = HOMEKIT_FLOAT(humidity_value);
            homekit_characteristic_notify(&current_humidity, current_humidity.value);
        }
        
        printf("RC > TEMP %g, HUM %g\n", temperature_value, humidity_value);
    } else {
        printf("RC ! ERROR Sensor\n");
        xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 5, 1, NULL);
        
        if (current_state.value.int_value != 0 && device_type_static == 5) {
            current_state.value = HOMEKIT_UINT8(0);
            homekit_characteristic_notify(&current_state, current_state.value);
            
            relay_write(false, relay1_gpio);
        }
    }
}

// ***** RGBW

typedef union {
    struct {
        uint16_t white;
        uint16_t blue;
        uint16_t green;
        uint16_t red;
    };
    uint64_t color;
} rgb_color_t;

rgb_color_t current_rgbw_color = { { 0, 0, 0, 0 } };
rgb_color_t target_rgbw_color = { { 0, 0, 0, 0 } };

//http://blog.saikoled.com/post/44677718712/how-to-convert-from-hsi-to-rgb-white
void hsi2rgbw(float h, float s, float i, rgb_color_t* rgbw) {
    const float color_boost = (custom_color_boost.value.int_value * 0.02) + 1;
    
    while (h < 0) {
        h += 360.0F;
        
    }
    while (h >= 360) {
        h -= 360.0F;
        
    }
    
    h = 3.14159F * h / 180.0F;
    s /= 100.0F;
    i /= 100.0F;
    s = s > 0 ? (s < 1 ? s : 1) : 0;
    i = i > 0 ? (i < 1 ? i : 1) : 0;
    
    if (h < 2.09439) {
        rgbw->red   = (PWM_RGBW_SCALE - PWM_RGBW_SCALE_OFFSET) * i * color_boost / 3 * (1 + s * cos(h) / cos(1.047196667 - h));
        rgbw->green = (PWM_RGBW_SCALE - PWM_RGBW_SCALE_OFFSET) * i * color_boost / 3 * (1 + s * (1 - cos(h) / cos(1.047196667 - h)));
        rgbw->blue  = (PWM_RGBW_SCALE - PWM_RGBW_SCALE_OFFSET) * i * color_boost / 3 * (1 - s);
        rgbw->white = (PWM_RGBW_SCALE - PWM_RGBW_SCALE_OFFSET) * i * (1 - s);
    } else if (h < 4.188787) {
        h = h - 2.09439;
        rgbw->green = (PWM_RGBW_SCALE - PWM_RGBW_SCALE_OFFSET) * i * color_boost / 3 * (1 + s * cos(h) / cos(1.047196667 - h));
        rgbw->blue  = (PWM_RGBW_SCALE - PWM_RGBW_SCALE_OFFSET) * i * color_boost / 3 * (1 + s * (1 - cos(h) / cos(1.047196667 - h)));
        rgbw->red   = (PWM_RGBW_SCALE - PWM_RGBW_SCALE_OFFSET) * i * color_boost / 3 * (1 - s);
        rgbw->white = (PWM_RGBW_SCALE - PWM_RGBW_SCALE_OFFSET) * i * (1 - s);
    } else {
        h = h - 4.188787;
        rgbw->blue  = (PWM_RGBW_SCALE - PWM_RGBW_SCALE_OFFSET) * i * color_boost / 3 * (1 + s * cos(h) / cos(1.047196667 - h));
        rgbw->red   = (PWM_RGBW_SCALE - PWM_RGBW_SCALE_OFFSET) * i * color_boost / 3 * (1 + s * (1 - cos(h) / cos(1.047196667 - h)));
        rgbw->green = (PWM_RGBW_SCALE - PWM_RGBW_SCALE_OFFSET) * i * color_boost / 3 * (1 - s);
        rgbw->white = (PWM_RGBW_SCALE - PWM_RGBW_SCALE_OFFSET) * i * (1 - s);
    }
}

TaskHandle_t rgbw_set_task_handle;
void rgbw_set_task() {
    void apply_color(rgb_color_t rgbw_color) {
        multipwm_stop(&pwm_info);
        
        multipwm_set_duty(&pwm_info, 0, rgbw_color.red);
        multipwm_set_duty(&pwm_info, 1, rgbw_color.green);
        multipwm_set_duty(&pwm_info, 2, rgbw_color.blue);
        
        if (w_gpio != 0) {
            multipwm_set_duty(&pwm_info, 3, current_rgbw_color.white);
        }
        
        multipwm_start(&pwm_info);
    }
    
    while (1) {
        current_rgbw_color.red   += (target_rgbw_color.red      - current_rgbw_color.red)   >> 4;
        current_rgbw_color.green += (target_rgbw_color.green    - current_rgbw_color.green) >> 4;
        current_rgbw_color.blue  += (target_rgbw_color.blue     - current_rgbw_color.blue)  >> 4;
        current_rgbw_color.white += (target_rgbw_color.white    - current_rgbw_color.white) >> 4;
    
        apply_color(current_rgbw_color);
        
        vTaskDelay(RGBW_DELAY / portTICK_PERIOD_MS);
    
        if (abs(target_rgbw_color.red   - current_rgbw_color.red)   <= current_rgbw_color.red   >> 4 &&
            abs(target_rgbw_color.green - current_rgbw_color.green) <= current_rgbw_color.green >> 4 &&
            abs(target_rgbw_color.blue  - current_rgbw_color.blue)  <= current_rgbw_color.blue  >> 4 &&
            abs(target_rgbw_color.white - current_rgbw_color.white) <= current_rgbw_color.white >> 4) {
            
            apply_color(target_rgbw_color);
            
            printf("RC > Target color established\n");
            vTaskSuspend(NULL);
        }
    }
}

void rgbw_set() {
    if (switch1_on.value.bool_value) {
        printf("RC > Hue -> %0.0f, Sat -> %0.0f, Bri -> %i\n", hue.value.float_value, saturation.value.float_value, brightness.value.int_value);
        hsi2rgbw(hue.value.float_value, saturation.value.float_value, brightness.value.int_value, &target_rgbw_color);
    } else {
        target_rgbw_color.red = 0;
        target_rgbw_color.green = 0;
        target_rgbw_color.blue = 0;
        target_rgbw_color.white = 0;
    }
    
    printf("RC > RGBW -> %i, %i, %i, %i\n", target_rgbw_color.red, target_rgbw_color.green, target_rgbw_color.blue, target_rgbw_color.white);
    
    vTaskResume(rgbw_set_task_handle);
    
    save_states_callback();
}

void brightness_callback(homekit_value_t value) {
    brightness.value = value;
    rgbw_set();
}

void hue_callback(homekit_value_t value) {
    hue.value = value;
    rgbw_set();
}

void saturation_callback(homekit_value_t value) {
    saturation.value = value;
    rgbw_set();
}

void color_boost_callback() {
    change_settings_callback();
    rgbw_set();
}

// ***** Identify

void identify_task() {
    relay_write(false, relay1_gpio);
    
    for (uint8_t i = 0; i < 3; i++) {
        relay_write(true, relay1_gpio);
        vTaskDelay(400 / portTICK_PERIOD_MS);
        relay_write(false, relay1_gpio);
        vTaskDelay(400 / portTICK_PERIOD_MS);
    }
    
    vTaskDelete(NULL);
}

void identify(homekit_value_t _value) {
    printf("RC > Identifying\n");

    switch (device_type_static) {
        case 12:
        case 13:
            xTaskCreate(identify_task, "identify_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
            break;
            
        default:
            xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 8, 1, NULL);
            break;
    }
}

// ***** IP Address

homekit_value_t read_ip_addr() {
    struct ip_info info;
    if (sdk_wifi_get_ip_info(STATION_IF, &info)) {
        char *buffer = malloc(16);
        snprintf(buffer, 16, IPSTR, IP2STR(&info.ip));
        
        return HOMEKIT_STRING(buffer);
    }

    return HOMEKIT_STRING("");
}

void hardware_init() {
    printf("RC > Initializing hardware...\n");
    
    sdk_os_timer_setfn(&factory_default_toggle_timer, factory_default_toggle, NULL);
    sdk_os_timer_setfn(&change_settings_timer, save_settings, NULL);
    sdk_os_timer_setfn(&save_states_timer, save_states, NULL);
    
    void enable_sonoff_device() {
        if (board_type.value.int_value == 2) {
            extra_gpio = 2;
        } else if (board_type.value.int_value == 4) {
            extra_gpio = 3;
            log_output.value.int_value = 0;
        }
        
        gpio_enable(led_gpio, GPIO_OUTPUT);
        gpio_write(led_gpio, false);
        
        adv_button_create(button1_gpio, true);
    }
    
    void register_button_event(const uint8_t gpio) {
        adv_button_register_callback_fn(gpio, button_event1_intr_callback, 1, NULL);
        adv_button_register_callback_fn(gpio, button_event2_intr_callback, 2, NULL);
        adv_button_register_callback_fn(gpio, button_event3_intr_callback, 3, NULL);
        adv_button_register_callback_fn(gpio, button_simple1_intr_callback, 4, NULL);
        adv_button_register_callback_fn(gpio, factory_default_call, 5, NULL);
    }
    
    bool pullup = true;
    switch (device_type_static) {
        case 2:
            if (board_type.value.int_value == 3) {  // It is a Shelly2
                relay1_gpio = S2_RELAY1_GPIO;
                button1_gpio = S2_TOGGLE1_GPIO;
                button2_gpio = S2_TOGGLE2_GPIO;
                pullup = false;
            } else if (board_type.value.int_value == 5) {  // It is a Shelly2.5
                relay1_gpio = S25_RELAY1_GPIO;
                relay2_gpio = S25_RELAY2_GPIO;
                button1_gpio = S25_TOGGLE1_GPIO;
                button2_gpio = S25_TOGGLE2_GPIO;
                led_gpio = S25_LED_GPIO;
                
                adv_button_create(S25_BUTTON_GPIO, false);
                adv_button_register_callback_fn(S25_BUTTON_GPIO, factory_default_call, 5, NULL);
                
                gpio_enable(led_gpio, GPIO_OUTPUT);
                gpio_write(led_gpio, false);
                
                pullup = false;
            } else {                                // It is a Sonoff
                enable_sonoff_device();
                
                adv_button_destroy(button1_gpio);
                
                adv_button_create(BUTTON3_GPIO, true);
                
                adv_button_register_callback_fn(BUTTON3_GPIO, button_simple1_intr_callback, 1, NULL);
                adv_button_register_callback_fn(BUTTON3_GPIO, button_simple2_intr_callback, 2, NULL);
                adv_button_register_callback_fn(BUTTON3_GPIO, factory_default_call, 5, NULL);
                
                if (board_type.value.int_value == 4) {
                    button1_gpio = 3;
                }
            }
            
            switch (external_toggle1.value.int_value) {
                case 1:
                    adv_button_create(button1_gpio, pullup);
                    adv_button_register_callback_fn(button1_gpio, button_simple1_intr_callback, 1, NULL);
                    break;
                    
                case 2:
                    adv_button_create(button1_gpio, pullup);
                    adv_button_register_callback_fn(button1_gpio, button_simple1_intr_callback, 0, NULL);
                    adv_button_register_callback_fn(button1_gpio, button_simple1_intr_callback, 1, NULL);
                    break;
                    
                case 3:
                    adv_button_create(button1_gpio, pullup);
                    adv_button_register_callback_fn(button1_gpio, button_dual_rotation_intr_callback, 1, NULL);
                    break;
                    
                default:
                    break;
            }
            
            switch (external_toggle2.value.int_value) {
                case 1:
                    adv_button_create(button2_gpio, pullup);
                    adv_button_register_callback_fn(button2_gpio, button_simple2_intr_callback, 1, NULL);
                    break;
                    
                case 2:
                    adv_button_create(button2_gpio, pullup);
                    adv_button_register_callback_fn(button2_gpio, button_simple2_intr_callback, 0, NULL);
                    adv_button_register_callback_fn(button2_gpio, button_simple2_intr_callback, 1, NULL);
                    break;
                    
                case 3:
                    adv_button_create(button2_gpio, pullup);
                    adv_button_register_callback_fn(button2_gpio, button_dual_rotation_intr_callback, 1, NULL);
                    break;
                    
                default:
                    break;
            }
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            switch1_on_callback(switch1_on.value);
            
            gpio_enable(relay2_gpio, GPIO_OUTPUT);
            switch2_on_callback(switch2_on.value);

            break;
            
        case 3:
            enable_sonoff_device();
            
            register_button_event(button1_gpio);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            switch1_on_callback(switch1_on.value);
            
            break;
            
        case 4:
            enable_sonoff_device();
            
            adv_button_register_callback_fn(button1_gpio, button_simple1_intr_callback, 1, NULL);
            adv_button_register_callback_fn(button1_gpio, factory_default_call, 5, NULL);
            
            adv_button_create(button2_gpio, true);
            adv_button_register_callback_fn(button2_gpio, button_simple2_intr_callback, 1, NULL);
            
            adv_button_create(BUTTON3_GPIO, true);
            adv_button_register_callback_fn(BUTTON3_GPIO, button_simple3_intr_callback, 1, NULL);
            
            adv_button_create(extra_gpio, true);
            adv_button_register_callback_fn(extra_gpio, button_simple4_intr_callback, 1, NULL);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            switch1_on_callback(switch1_on.value);
            
            gpio_enable(relay2_gpio, GPIO_OUTPUT);
            switch2_on_callback(switch2_on.value);
            
            gpio_enable(RELAY3_GPIO, GPIO_OUTPUT);
            switch3_on_callback(switch3_on.value);
            
            gpio_enable(RELAY4_GPIO, GPIO_OUTPUT);
            switch4_on_callback(switch4_on.value);
            
            break;
            
        case 5:
            enable_sonoff_device();
            
            adv_button_register_callback_fn(button1_gpio, th_button_intr_callback, 1, NULL);
            adv_button_register_callback_fn(button1_gpio, factory_default_call, 5, NULL);
            
            gpio_set_pullup(extra_gpio, false, false);
            
            sdk_os_timer_setfn(&extra_func_timer, temperature_sensor_worker, NULL);
            sdk_os_timer_arm(&extra_func_timer, poll_period.value.int_value * 1000, 1);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            relay_write(false, relay1_gpio);
            
            temperature_sensor_worker();
            
            break;
            
        case 6:
            enable_sonoff_device();
            
            adv_button_register_callback_fn(button1_gpio, button_simple1_intr_callback, 1, NULL);
            adv_button_register_callback_fn(button1_gpio, factory_default_call, 5, NULL);
            
            gpio_set_pullup(extra_gpio, false, false);
            
            sdk_os_timer_setfn(&extra_func_timer, temperature_sensor_worker, NULL);
            sdk_os_timer_arm(&extra_func_timer, poll_period.value.int_value * 1000, 1);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            switch1_on_callback(switch1_on.value);
            
            temperature_sensor_worker();
            
            break;
            
        case 7:
            if (board_type.value.int_value == 3) {  // It is a Shelly1
                relay1_gpio = S1_RELAY_GPIO;
                extra_gpio = S1_TOGGLE_GPIO;
                pullup = false;
            } else {                                // It is a Sonoff
                enable_sonoff_device();
                
                adv_button_register_callback_fn(button1_gpio, toggle_valve, 1, NULL);
                adv_button_register_callback_fn(button1_gpio, factory_default_call, 5, NULL);
            }
            
            sdk_os_timer_setfn(&extra_func_timer, valve_control, NULL);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            relay_write(false, relay1_gpio);
            
            break;
            
        case 8:
            enable_sonoff_device();
            
            adv_button_register_callback_fn(button1_gpio, garage_on_button, 1, NULL);
            adv_button_register_callback_fn(button1_gpio, factory_default_call, 5, NULL);
            
            adv_button_create(extra_gpio, true);
            
            if (custom_garagedoor_sensor_close_nc.value.bool_value) {
                adv_button_register_callback_fn(extra_gpio, door_closed_1_fn_callback, 0, NULL);
                adv_button_register_callback_fn(extra_gpio, door_closed_0_fn_callback, 1, NULL);
            } else {
                adv_button_register_callback_fn(extra_gpio, door_closed_0_fn_callback, 0, NULL);
                adv_button_register_callback_fn(extra_gpio, door_closed_1_fn_callback, 1, NULL);
            }
            
            if (custom_garagedoor_sensor_open.value.int_value > 0) {
                adv_button_create(DOOR_OPENED_GPIO, true);
                
                if (custom_garagedoor_sensor_open.value.int_value == 2) {
                    adv_button_register_callback_fn(DOOR_OPENED_GPIO, door_opened_1_fn_callback, 0, NULL);
                    adv_button_register_callback_fn(DOOR_OPENED_GPIO, door_opened_0_fn_callback, 1, NULL);
                } else {
                    adv_button_register_callback_fn(DOOR_OPENED_GPIO, door_opened_0_fn_callback, 0, NULL);
                    adv_button_register_callback_fn(DOOR_OPENED_GPIO, door_opened_1_fn_callback, 1, NULL);
                }
                
            } else {
                sdk_os_timer_setfn(&extra_func_timer, door_opened_countdown_timer, NULL);
            }
            
            if (custom_garagedoor_sensor_obstruction.value.int_value > 0) {
                adv_button_create(DOOR_OBSTRUCTION_GPIO, true);
                
                if (custom_garagedoor_sensor_obstruction.value.int_value == 2) {
                    adv_button_register_callback_fn(DOOR_OBSTRUCTION_GPIO, door_obstructed_1_fn_callback, 0, NULL);
                    adv_button_register_callback_fn(DOOR_OBSTRUCTION_GPIO, door_obstructed_0_fn_callback, 1, NULL);
                } else {
                    adv_button_register_callback_fn(DOOR_OBSTRUCTION_GPIO, door_obstructed_0_fn_callback, 0, NULL);
                    adv_button_register_callback_fn(DOOR_OBSTRUCTION_GPIO, door_obstructed_1_fn_callback, 1, NULL);
                }
                
                if (gpio_read(DOOR_OBSTRUCTION_GPIO) == custom_garagedoor_sensor_obstruction.value.int_value -1) {
                    door_obstructed_1_fn_callback(DOOR_OBSTRUCTION_GPIO, NULL);
                }
            }
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            relay_write(false, relay1_gpio);
            
            break;
            
        case 9:
            enable_sonoff_device();
            
            register_button_event(button1_gpio);
            
            gpio_set_pullup(extra_gpio, false, false);
            
            sdk_os_timer_setfn(&extra_func_timer, temperature_sensor_worker, NULL);
            sdk_os_timer_arm(&extra_func_timer, poll_period.value.int_value * 1000, 1);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            switch1_on_callback(switch1_on.value);
            
            temperature_sensor_worker();
            break;
            
        case 10:
            enable_sonoff_device();
            
            if (board_type.value.int_value == 2) {
                adv_button_destroy(button1_gpio);
                button1_gpio = 2;
                relay1_gpio = 0;
                adv_button_create(button1_gpio, true);
            } else {
                relay1_gpio = 2;
            }
            
            register_button_event(button1_gpio);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            switch1_on_callback(switch1_on.value);
    
            break;
            
        case 11:
            enable_sonoff_device();
            
            adv_button_register_callback_fn(button1_gpio, button_simple1_intr_callback, 1, NULL);
            adv_button_register_callback_fn(button1_gpio, factory_default_call, 5, NULL);
            
            adv_button_create(button2_gpio, true);
            adv_button_register_callback_fn(button2_gpio, button_simple2_intr_callback, 1, NULL);
            
            adv_button_create(BUTTON3_GPIO, true);
            adv_button_register_callback_fn(BUTTON3_GPIO, button_simple3_intr_callback, 1, NULL);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            switch1_on_callback(switch1_on.value);
            
            gpio_enable(relay2_gpio, GPIO_OUTPUT);
            switch2_on_callback(switch2_on.value);
            
            gpio_enable(RELAY3_GPIO, GPIO_OUTPUT);
            relay_write(switch3_on.value.bool_value, RELAY3_GPIO);
            
            break;
            
        case 12:
            if (board_type.value.int_value == 3) {  // It is a Shelly2
                relay1_gpio = S2_RELAY1_GPIO;
                button1_gpio = S2_TOGGLE1_GPIO;
                button2_gpio = S2_TOGGLE2_GPIO;
                pullup = false;
            } else if (board_type.value.int_value == 5) {  // It is a Shelly2.5
                relay1_gpio = S25_RELAY1_GPIO;
                relay2_gpio = S25_RELAY2_GPIO;
                button1_gpio = S25_TOGGLE1_GPIO;
                button2_gpio = S25_TOGGLE2_GPIO;
                led_gpio = S25_LED_GPIO;
                
                gpio_enable(led_gpio, GPIO_OUTPUT);
                gpio_write(led_gpio, false);
                
                adv_button_create(S25_BUTTON_GPIO, false);
                adv_button_register_callback_fn(S25_BUTTON_GPIO, factory_default_call, 5, NULL);
                
                pullup = false;
            } else {                                // It is a Sonoff
                enable_sonoff_device();
                
                adv_button_destroy(button1_gpio);
                
                adv_button_create(BUTTON3_GPIO, true);
                adv_button_register_callback_fn(BUTTON3_GPIO, factory_default_call, 5, NULL);
                
                if (board_type.value.int_value == 4) {
                    button1_gpio = 3;
                }
            }
            
            switch (external_toggle1.value.int_value) {
                case 1:
                    adv_button_create(button1_gpio, pullup);
                    adv_button_register_callback_fn(button1_gpio, covering_button_down, 1, NULL);
                    
                    adv_button_create(button2_gpio, pullup);
                    adv_button_register_callback_fn(button2_gpio, covering_button_up, 1, NULL);
                    break;
                    
                case 2:
                    adv_button_create(button1_gpio, pullup);
                    adv_button_register_callback_fn(button1_gpio, covering_toggle_down, 0, NULL);
                    adv_button_register_callback_fn(button1_gpio, covering_toggle_stop, 1, NULL);
                    
                    adv_button_create(button2_gpio, pullup);
                    adv_button_register_callback_fn(button2_gpio, covering_toggle_up, 0, NULL);
                    adv_button_register_callback_fn(button2_gpio, covering_toggle_stop, 1, NULL);
                    break;
                    
                default:
                    break;
            }
            
# define STEP_TIME(x)       ((100.0 / (x)) * (COVERING_POLL_PERIOD_MS / 1000.0))
            covering_step_time_up = STEP_TIME(custom_covering_up_time.value.float_value);
            covering_step_time_down = STEP_TIME(custom_covering_down_time.value.float_value);
            
            printf("RC > covering_step_time_up = %f\n", covering_step_time_up);
            printf("RC > covering_step_time_down = %f\n", covering_step_time_down);
            
            sdk_os_timer_setfn(&extra_func_timer, covering_worker, NULL);
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            relay_write(false, relay1_gpio);
            
            gpio_enable(relay2_gpio, GPIO_OUTPUT);
            relay_write(false, relay2_gpio);
            
            break;
            
        case 13:
            if (board_type.value.int_value == 3) {  // It is a Shelly1
                relay1_gpio = S1_RELAY_GPIO;
                extra_gpio = S1_TOGGLE_GPIO;
                pullup = false;
            } else {                                // It is a Sonoff
                enable_sonoff_device();
                
                adv_button_register_callback_fn(button1_gpio, lock_intr_callback, 1, NULL);
                adv_button_register_callback_fn(button1_gpio, factory_default_call, 5, NULL);
            }
            
            switch (external_toggle1.value.int_value) {
                case 1:
                    adv_button_create(extra_gpio, pullup);
                    adv_button_register_callback_fn(extra_gpio, lock_intr_callback, 1, NULL);
                case 2:
                    adv_button_create(extra_gpio, pullup);
                    adv_button_register_callback_fn(extra_gpio, lock_intr_callback, 0, NULL);
                    adv_button_register_callback_fn(extra_gpio, lock_intr_callback, 1, NULL);
                    break;
                    
                default:
                    break;
            }
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            relay_write(false, relay1_gpio);
            
            break;
            
        case 14:
            adv_button_create(button1_gpio, true);
            adv_button_register_callback_fn(button1_gpio, button_simple1_intr_callback, 1, NULL);
            adv_button_register_callback_fn(button1_gpio, factory_default_call, 5, NULL);
            
            r_gpio = custom_r_gpio.value.int_value;
            g_gpio = custom_g_gpio.value.int_value;
            b_gpio = custom_b_gpio.value.int_value;
            w_gpio = custom_w_gpio.value.int_value;
            
            void default_rgbw_pins() {
                custom_r_gpio.value.int_value = INITIAL_R_GPIO;
                custom_g_gpio.value.int_value = INITIAL_G_GPIO;
                custom_b_gpio.value.int_value = INITIAL_B_GPIO;
                custom_w_gpio.value.int_value = INITIAL_W_GPIO;
                
                r_gpio = custom_r_gpio.value.int_value;
                g_gpio = custom_g_gpio.value.int_value;
                b_gpio = custom_b_gpio.value.int_value;
                w_gpio = custom_w_gpio.value.int_value;
                
                change_settings_callback();
            }
            
            if (r_gpio == g_gpio ||
                r_gpio == b_gpio ||
                r_gpio == w_gpio ||
                g_gpio == b_gpio ||
                g_gpio == w_gpio ||
                b_gpio == w_gpio) {
                default_rgbw_pins();
            }
            
            if ((r_gpio > 5 && r_gpio < 9) ||
                (g_gpio > 5 && g_gpio < 9) ||
                (b_gpio > 5 && b_gpio < 9) ||
                (w_gpio > 5 && w_gpio < 9)) {
                default_rgbw_pins();
            }
            
            multipwm_init(&pwm_info);
            
            pwm_info.channels = 3;
            
            multipwm_set_freq(&pwm_info, PWM_RGBW_SCALE);

            multipwm_set_pin(&pwm_info, 0, r_gpio);
            multipwm_set_pin(&pwm_info, 1, g_gpio);
            multipwm_set_pin(&pwm_info, 2, b_gpio);
            
            if (w_gpio != 0) {
                pwm_info.channels++;
                multipwm_set_pin(&pwm_info, 3, w_gpio);
            }
            
            xTaskCreate(rgbw_set_task, "rgbw_set_task", configMINIMAL_STACK_SIZE, NULL, 1, &rgbw_set_task_handle);
            
            rgbw_set();
            
            break;
            
        case 15:
            if (board_type.value.int_value == 0) {
                pullup = false;
            }
                
            if (custom_w_gpio.value.int_value > 5 && custom_w_gpio.value.int_value < 9) {
                custom_w_gpio.value.int_value = INITIAL_W_GPIO;
            }
            
            adv_button_create(custom_w_gpio.value.int_value, pullup);
            
            adv_button_register_callback_fn(custom_w_gpio.value.int_value, button_event1_intr_callback, 1, NULL);
            adv_button_register_callback_fn(custom_w_gpio.value.int_value, button_event2_intr_callback, 2, NULL);
            adv_button_register_callback_fn(custom_w_gpio.value.int_value, button_event3_intr_callback, 3, NULL);
            adv_button_register_callback_fn(custom_w_gpio.value.int_value, factory_default_call, 5, NULL);
            
            break;
            
        default:    // case 1:
            if (board_type.value.int_value == 3) {  // It is a Shelly1
                relay1_gpio = S1_RELAY_GPIO;
                extra_gpio = S1_TOGGLE_GPIO;
                pullup = false;
            } else {                                // It is a Sonoff
                enable_sonoff_device();
                
                adv_button_register_callback_fn(button1_gpio, button_simple1_intr_callback, 1, NULL);
                adv_button_register_callback_fn(button1_gpio, factory_default_call, 5, NULL);
            }
            
            switch (external_toggle1.value.int_value) {
                case 1:
                    adv_button_create(extra_gpio, pullup);
                    adv_button_register_callback_fn(extra_gpio, button_simple1_intr_callback, 1, NULL);
                    break;
                    
                case 2:
                    adv_button_create(extra_gpio, pullup);
                    adv_button_register_callback_fn(extra_gpio, button_simple1_intr_callback, 0, NULL);
                    adv_button_register_callback_fn(extra_gpio, button_simple1_intr_callback, 1, NULL);
                    break;
                    
                default:
                    break;
            }
            
            gpio_enable(relay1_gpio, GPIO_OUTPUT);
            switch1_on_callback(switch1_on.value);
            
            break;
    }
    
    printf("RC > HW ready\n");
    
    wifi_config_init("RavenCore", NULL, create_accessory);
}

void settings_init() {
    // Initial Setup
    sysparam_status_t status, flash_error = SYSPARAM_OK;
    bool bool_value;
    int8_t int8_value;
    int32_t int32_value;
    
    // Load Saved Settings and set factory values for missing settings
    printf("RC > Loading settings\n");
    
    status = sysparam_get_bool(SHOW_SETUP_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        show_setup.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(SHOW_SETUP_SYSPARAM, true);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(OTA_BETA_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data(OTA_BETA_SYSPARAM, NULL, 0, false);
        status = sysparam_compact();
    }
    
    status = sysparam_get_int8(BOARD_TYPE_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        board_type.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(BOARD_TYPE_SYSPARAM, 1);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(DEVICE_TYPE_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        device_type.value.int_value = int8_value;
        device_type_static = int8_value;
    } else {
        status = sysparam_set_int8(DEVICE_TYPE_SYSPARAM, 1);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(EXTERNAL_TOGGLE1_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        external_toggle1.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(EXTERNAL_TOGGLE1_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(EXTERNAL_TOGGLE2_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        external_toggle2.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(EXTERNAL_TOGGLE2_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(DHT_SENSOR_TYPE_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        dht_sensor_type.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(DHT_SENSOR_TYPE_SYSPARAM, 2);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(HUM_OFFSET_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        hum_offset.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(HUM_OFFSET_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int32(TEMP_OFFSET_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        temp_offset.value.float_value = int32_value / 100.00f;
    } else {
        status = sysparam_set_int32(TEMP_OFFSET_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int32(TEMP_DEADBAND_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        temp_deadband.value.float_value = int32_value / 100.00f;
    } else {
        status = sysparam_set_int32(TEMP_DEADBAND_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(POLL_PERIOD_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        poll_period.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(POLL_PERIOD_SYSPARAM, 30);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(VALVE_TYPE_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        valve_type.value.int_value = int8_value;
        custom_valve_type.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(VALVE_TYPE_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(GARAGEDOOR_WORKING_TIME_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_working_time.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(GARAGEDOOR_WORKING_TIME_SYSPARAM, 20);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(GARAGEDOOR_HAS_STOP_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_has_stop.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(GARAGEDOOR_HAS_STOP_SYSPARAM, true);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(GARAGEDOOR_SENSOR_CLOSE_NC_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_sensor_close_nc.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(GARAGEDOOR_SENSOR_CLOSE_NC_SYSPARAM, false);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(GARAGEDOOR_SENSOR_OPEN_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_sensor_open.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(GARAGEDOOR_SENSOR_OPEN_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(GARAGEDOOR_SENSOR_OBSTRUCTION_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_sensor_obstruction.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(GARAGEDOOR_SENSOR_OBSTRUCTION_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(GARAGEDOOR_CONTROL_WITH_BUTTON_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_control_with_button.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(GARAGEDOOR_CONTROL_WITH_BUTTON_SYSPARAM, false);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(ENABLE_DUMMY_SWITCH_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        enable_dummy_switch.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(ENABLE_DUMMY_SWITCH_SYSPARAM, false);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int32(INCHING_TIME1_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        custom_inching_time1.value.float_value = int32_value / 100.00f;
    } else {
        status = sysparam_set_int32(INCHING_TIME1_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int32(INCHING_TIME2_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        custom_inching_time2.value.float_value = int32_value / 100.00f;
    } else {
        status = sysparam_set_int32(INCHING_TIME2_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int32(INCHING_TIME3_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        custom_inching_time3.value.float_value = int32_value / 100.00f;
    } else {
        status = sysparam_set_int32(INCHING_TIME3_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int32(INCHING_TIME4_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        custom_inching_time4.value.float_value = int32_value / 100.00f;
    } else {
        status = sysparam_set_int32(INCHING_TIME4_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int32(INCHING_TIMEDM_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        custom_inching_timedm.value.float_value = int32_value / 100.00f;
    } else {
        status = sysparam_set_int32(INCHING_TIMEDM_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int32(COVERING_UP_TIME_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        custom_covering_up_time.value.float_value = int32_value / 100.00f;
    } else {
        status = sysparam_set_int32(COVERING_UP_TIME_SYSPARAM, 30 * 100);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int32(COVERING_DOWN_TIME_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        custom_covering_down_time.value.float_value = int32_value / 100.00f;
    } else {
        status = sysparam_set_int32(COVERING_DOWN_TIME_SYSPARAM, 30 * 100);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(COVERING_TYPE_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_covering_type.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(COVERING_TYPE_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(R_GPIO_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_r_gpio.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(R_GPIO_SYSPARAM, INITIAL_R_GPIO);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(G_GPIO_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_g_gpio.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(G_GPIO_SYSPARAM, INITIAL_G_GPIO);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(B_GPIO_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_b_gpio.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(B_GPIO_SYSPARAM, INITIAL_B_GPIO);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(W_GPIO_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_w_gpio.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(W_GPIO_SYSPARAM, INITIAL_W_GPIO);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(COLOR_BOOST_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_color_boost.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(COLOR_BOOST_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(INIT_STATE_SW1_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw1.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(INIT_STATE_SW1_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(INIT_STATE_SW2_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw2.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(INIT_STATE_SW2_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(INIT_STATE_SW3_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw3.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(INIT_STATE_SW3_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(INIT_STATE_SW4_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw4.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(INIT_STATE_SW4_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(INIT_STATE_SWDM_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_swdm.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(INIT_STATE_SWDM_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(INIT_STATE_TH_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_th.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(INIT_STATE_TH_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(REVERSE_SW1_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        custom_reverse_sw1.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(REVERSE_SW1_SYSPARAM, false);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(REVERSE_SW2_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        custom_reverse_sw2.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(REVERSE_SW2_SYSPARAM, false);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(REVERSE_SW3_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        custom_reverse_sw3.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(REVERSE_SW3_SYSPARAM, false);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_bool(REVERSE_SW4_SYSPARAM, &bool_value);
    if (status == SYSPARAM_OK) {
        custom_reverse_sw4.value.bool_value = bool_value;
    } else {
        status = sysparam_set_bool(REVERSE_SW4_SYSPARAM, false);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    // Load Saved States
    printf("RC > Loading saved states\n");
    
    if (custom_init_state_sw1.value.int_value > 1) {
        status = sysparam_get_bool(LAST_STATE_SW1_SYSPARAM, &bool_value);
        if (status == SYSPARAM_OK) {
            if (custom_init_state_sw1.value.int_value == 2) {
                switch1_on.value.bool_value = bool_value;
            } else {
                switch1_on.value.bool_value = !bool_value;
            }
        } else {
            status = sysparam_set_bool(LAST_STATE_SW1_SYSPARAM, false);
            if (status != SYSPARAM_OK) {
                flash_error = status;
            }
        }
    } else if (custom_init_state_sw1.value.int_value == 1) {
        switch1_on.value.bool_value = true;
    }
    
    if (custom_init_state_sw2.value.int_value > 1) {
        status = sysparam_get_bool(LAST_STATE_SW2_SYSPARAM, &bool_value);
        if (status == SYSPARAM_OK) {
            if (custom_init_state_sw2.value.int_value == 2) {
                switch2_on.value.bool_value = bool_value;
            } else {
                switch2_on.value.bool_value = !bool_value;
            }
        } else {
            status = sysparam_set_bool(LAST_STATE_SW2_SYSPARAM, false);
            if (status != SYSPARAM_OK) {
                flash_error = status;
            }
        }
    } else if (custom_init_state_sw2.value.int_value == 1) {
        switch2_on.value.bool_value = true;
    }
    
    if (custom_init_state_sw3.value.int_value > 1) {
        status = sysparam_get_bool(LAST_STATE_SW3_SYSPARAM, &bool_value);
        if (status == SYSPARAM_OK) {
            if (custom_init_state_sw3.value.int_value == 2) {
                switch3_on.value.bool_value = bool_value;
            } else {
                switch3_on.value.bool_value = !bool_value;
            }
        } else {
            status = sysparam_set_bool(LAST_STATE_SW3_SYSPARAM, false);
            if (status != SYSPARAM_OK) {
                flash_error = status;
            }
        }
    } else if (custom_init_state_sw3.value.int_value == 1) {
        switch3_on.value.bool_value = true;
    }
    
    if (custom_init_state_sw4.value.int_value > 1) {
        status = sysparam_get_bool(LAST_STATE_SW4_SYSPARAM, &bool_value);
        if (status == SYSPARAM_OK) {
            if (custom_init_state_sw4.value.int_value == 2) {
                switch4_on.value.bool_value = bool_value;
            } else {
                switch4_on.value.bool_value = !bool_value;
            }
        } else {
            status = sysparam_set_bool(LAST_STATE_SW4_SYSPARAM, false);
            if (status != SYSPARAM_OK) {
                flash_error = status;
            }
        }
    } else if (custom_init_state_sw4.value.int_value == 1) {
        switch4_on.value.bool_value = true;
    }
    
    if (custom_init_state_swdm.value.int_value > 1) {
        status = sysparam_get_bool(LAST_STATE_SWDM_SYSPARAM, &bool_value);
        if (status == SYSPARAM_OK) {
            if (custom_init_state_swdm.value.int_value == 2) {
                switchdm_on.value.bool_value = bool_value;
            } else {
                switchdm_on.value.bool_value = !bool_value;
            }
        } else {
            status = sysparam_set_bool(LAST_STATE_SWDM_SYSPARAM, false);
            if (status != SYSPARAM_OK) {
                flash_error = status;
            }
        }
    } else if (custom_init_state_swdm.value.int_value == 1) {
        switchdm_on.value.bool_value = true;
    }
    
    if (custom_init_state_th.value.int_value < 3) {
        target_state.value.int_value = custom_init_state_th.value.int_value;
    } else {
        status = sysparam_get_int8(LAST_TARGET_STATE_TH_SYSPARAM, &int8_value);
        if (status == SYSPARAM_OK) {
            target_state.value.int_value = int8_value;
        } else {
            status = sysparam_set_int8(LAST_TARGET_STATE_TH_SYSPARAM, 0);
            if (status != SYSPARAM_OK) {
                flash_error = status;
            }
        }
    }
    
    status = sysparam_get_int32(TARGET_TEMPERATURE_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        target_temperature.value.float_value = int32_value / 100.00f;
    } else {
        status = sysparam_set_int32(TARGET_TEMPERATURE_SYSPARAM, 23 * 100);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int32(VALVE_SET_DURATION_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        set_duration.value.int_value = int32_value;
    } else {
        status = sysparam_set_int32(VALVE_SET_DURATION_SYSPARAM, 900);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int32(COVERING_LAST_POSITION_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        covering_actual_pos = int32_value / 100.00f;
        covering_current_position.value.int_value = covering_actual_pos;
        covering_target_position.value.int_value = covering_actual_pos;
    } else {
        status = sysparam_set_int32(COVERING_LAST_POSITION_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(LAST_STATE_BRIGHTNESS_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        brightness.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(LAST_STATE_BRIGHTNESS_SYSPARAM, 100);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int32(LAST_STATE_HUE_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        hue.value.float_value = int32_value;
    } else {
        status = sysparam_set_int32(LAST_STATE_HUE_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    status = sysparam_get_int8(LAST_STATE_SATURATION_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        saturation.value.float_value = int8_value;
    } else {
        status = sysparam_set_int8(LAST_STATE_SATURATION_SYSPARAM, 0);
        if (status != SYSPARAM_OK) {
            flash_error = status;
        }
    }
    
    if (flash_error == SYSPARAM_OK) {
        hardware_init();
    } else {
        printf("RC ! ERROR loading settings -> %i\n", flash_error);
        printf("RC ! SYSTEM HALTED\n\n");
    }
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, NULL);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER, "RavenSystem");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, NULL);
homekit_characteristic_t model = HOMEKIT_CHARACTERISTIC_(MODEL, "RavenCore");
homekit_characteristic_t identify_function = HOMEKIT_CHARACTERISTIC_(IDENTIFY, identify);

homekit_characteristic_t switch1_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Switch 1");
homekit_characteristic_t switch2_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Switch 2");
homekit_characteristic_t switch3_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Switch 3");
homekit_characteristic_t switch4_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Switch 4");
homekit_characteristic_t switchdm_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Dummy Switch");

homekit_characteristic_t outlet_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Outlet");
homekit_characteristic_t button_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Button");
homekit_characteristic_t th_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Thermostat");
homekit_characteristic_t temp_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Temperature");
homekit_characteristic_t hum_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Humidity");
homekit_characteristic_t valve_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Water Valve");
homekit_characteristic_t garage_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Garage Door");
homekit_characteristic_t covering_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Window");
homekit_characteristic_t lock_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Lock");
homekit_characteristic_t rgbw_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "RGBW");

homekit_characteristic_t setup_service_name = HOMEKIT_CHARACTERISTIC_(NAME, "Setup", .id=100);
homekit_characteristic_t device_type_name = HOMEKIT_CHARACTERISTIC_(CUSTOM_DEVICE_TYPE_NAME, "", .id=101);

homekit_characteristic_t firmware = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, FIRMWARE_VERSION);

homekit_accessory_category_t accessory_category;

homekit_server_config_t config;

void create_accessory() {
    printf("RC > Creating HK accessory\n");
    
    xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE, (void *) 6, 1, NULL);
    
    // Accessory Name and Serial
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    char *name_value = malloc(17);
    snprintf(name_value, 17, "RavenCore-%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    name.value = HOMEKIT_STRING(name_value);
    
    char *serial_value = malloc(13);
    snprintf(serial_value, 13, "%02X%02X%02X%02X%02X%02X", macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);
    serial.value = HOMEKIT_STRING(serial_value);
    
    uint8_t service_count = 3, service_number = 2;
    
    // Total service count and Accessory Category selection
    if (show_setup.value.bool_value) {
        service_count++;
    }
    
    switch (device_type_static) {
        case 2:
            service_count += 1;
            accessory_category = homekit_accessory_category_switch;
            break;
            
        case 3:
            service_count += 1;
            accessory_category = homekit_accessory_category_outlet;
            break;
            
        case 4:
            service_count += 3;
            accessory_category = homekit_accessory_category_switch;
            break;
            
        case 5:
            service_count += 0;
            accessory_category = homekit_accessory_category_thermostat;
            break;
            
        case 6:
            service_count += 1;
            if (dht_sensor_type.value.int_value != 3) {
                service_count += 1;
            }
            accessory_category = homekit_accessory_category_switch;
            break;
            
        case 7:
            service_count += 0;
            accessory_category = homekit_accessory_category_sprinkler;
            break;
            
        case 8:
            service_count += 0;
            accessory_category = homekit_accessory_category_garage;
            break;
            
        case 9:
            service_count += 2;
            if (dht_sensor_type.value.int_value != 3) {
                service_count += 1;
            }
            accessory_category = homekit_accessory_category_outlet;
            break;
            
        case 10:
            service_count += 1;
            accessory_category = homekit_accessory_category_switch;
            break;
            
        case 11:
            service_count += 2;
            accessory_category = homekit_accessory_category_switch;
            break;
            
        case 12:
            service_count += 0;
            accessory_category = homekit_accessory_category_window_covering;
            break;
            
        case 13:
            service_count += 0;
            accessory_category = homekit_accessory_category_door_lock;
            break;
            
        case 14:
            service_count += 0;
            accessory_category = homekit_accessory_category_lightbulb;
            break;
            
        case 15:
            service_count += 0;
            accessory_category = homekit_accessory_category_programmable_switch;
            break;
            
        default:    // case 1
            service_count += 0;
            accessory_category = homekit_accessory_category_switch;
            break;
    }
    
    if (enable_dummy_switch.value.bool_value) {
        service_count += 1;
    }
    
    
    homekit_accessory_t **accessories = calloc(2, sizeof(homekit_accessory_t*));

    homekit_accessory_t *sonoff = accessories[0] = calloc(1, sizeof(homekit_accessory_t));
        sonoff->id = 1;
        sonoff->services = calloc(service_count, sizeof(homekit_service_t*));

            homekit_service_t *sonoff_info = sonoff->services[0] = calloc(1, sizeof(homekit_service_t));
            sonoff_info->id = 1;
            sonoff_info->type = HOMEKIT_SERVICE_ACCESSORY_INFORMATION;
            sonoff_info->characteristics = calloc(7, sizeof(homekit_characteristic_t*));
                sonoff_info->characteristics[0] = &name;
                sonoff_info->characteristics[1] = &manufacturer;
                sonoff_info->characteristics[2] = &serial;
                sonoff_info->characteristics[3] = &model;
                sonoff_info->characteristics[4] = &firmware;
                sonoff_info->characteristics[5] = &identify_function;
    
            // Define services and characteristics
            void charac_switch_1(const uint8_t service) {
                homekit_service_t *sonoff_switch1 = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch1->id = 8;
                sonoff_switch1->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch1->primary = true;
                sonoff_switch1->characteristics = calloc(4, sizeof(homekit_characteristic_t*));
                    sonoff_switch1->characteristics[0] = &switch1_service_name;
                    sonoff_switch1->characteristics[1] = &switch1_on;
                    sonoff_switch1->characteristics[2] = &show_setup;
            }
    
            void charac_switch_2(const uint8_t service) {
                homekit_service_t *sonoff_switch2 = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch2->id = 12;
                sonoff_switch2->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch2->primary = false;
                sonoff_switch2->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_switch2->characteristics[0] = &switch2_service_name;
                    sonoff_switch2->characteristics[1] = &switch2_on;
            }
    
            void charac_switch_3(const uint8_t service) {
                homekit_service_t *sonoff_switch3 = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch3->id = 15;
                sonoff_switch3->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch3->primary = false;
                sonoff_switch3->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_switch3->characteristics[0] = &switch3_service_name;
                    sonoff_switch3->characteristics[1] = &switch3_on;
            }
    
            void charac_switch_4(const uint8_t service) {
                homekit_service_t *sonoff_switch4 = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_switch4->id = 18;
                sonoff_switch4->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switch4->primary = false;
                sonoff_switch4->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_switch4->characteristics[0] = &switch4_service_name;
                    sonoff_switch4->characteristics[1] = &switch4_on;
            }
    
            void charac_socket(const uint8_t service) {
                homekit_service_t *sonoff_outlet = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_outlet->id = 21;
                sonoff_outlet->type = HOMEKIT_SERVICE_OUTLET;
                sonoff_outlet->primary = true;
                sonoff_outlet->characteristics = calloc(5, sizeof(homekit_characteristic_t*));
                    sonoff_outlet->characteristics[0] = &outlet_service_name;
                    sonoff_outlet->characteristics[1] = &switch1_on;
                    sonoff_outlet->characteristics[2] = &switch_outlet_in_use;
                    sonoff_outlet->characteristics[3] = &show_setup;
            }
    
            void charac_prog_button(const uint8_t service) {
                homekit_service_t *sonoff_button = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_button->id = 26;
                sonoff_button->type = HOMEKIT_SERVICE_STATELESS_PROGRAMMABLE_SWITCH;
                sonoff_button->primary = false;
                sonoff_button->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_button->characteristics[0] = &button_service_name;
                    sonoff_button->characteristics[1] = &button_event;
            }
    
            void charac_thermostat(const uint8_t service) {
                homekit_service_t *sonoff_th = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_th->id = 29;
                sonoff_th->type = HOMEKIT_SERVICE_THERMOSTAT;
                sonoff_th->primary = true;
                sonoff_th->characteristics = calloc(9, sizeof(homekit_characteristic_t*));
                    sonoff_th->characteristics[0] = &th_service_name;
                    sonoff_th->characteristics[1] = &current_temperature;
                    sonoff_th->characteristics[2] = &target_temperature;
                    sonoff_th->characteristics[3] = &current_state;
                    sonoff_th->characteristics[4] = &target_state;
                    sonoff_th->characteristics[5] = &units;
                    sonoff_th->characteristics[6] = &current_humidity;
                    sonoff_th->characteristics[7] = &show_setup;
            }
    
            void charac_temperature(const uint8_t service) {
                homekit_service_t *sonoff_temp = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_temp->id = 38;
                sonoff_temp->type = HOMEKIT_SERVICE_TEMPERATURE_SENSOR;
                sonoff_temp->primary = false;
                sonoff_temp->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_temp->characteristics[0] = &temp_service_name;
                    sonoff_temp->characteristics[1] = &current_temperature;
            }
    
            void charac_humidity(const uint8_t service) {
                homekit_service_t *sonoff_hum = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_hum->id = 41;
                sonoff_hum->type = HOMEKIT_SERVICE_HUMIDITY_SENSOR;
                sonoff_hum->primary = false;
                sonoff_hum->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_hum->characteristics[0] = &hum_service_name;
                    sonoff_hum->characteristics[1] = &current_humidity;
            }
    
            void charac_valve(const uint8_t service, bool with_timer) {
                homekit_service_t *sonoff_valve = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_valve->id = 44;
                sonoff_valve->type = HOMEKIT_SERVICE_VALVE;
                sonoff_valve->primary = true;
                
                uint8_t characs_size = 6;
                if (with_timer) {
                    characs_size += 2;
                }
                
                sonoff_valve->characteristics = calloc(characs_size, sizeof(homekit_characteristic_t*));
                    sonoff_valve->characteristics[0] = &valve_service_name;
                    sonoff_valve->characteristics[1] = &active;
                    sonoff_valve->characteristics[2] = &in_use;
                    sonoff_valve->characteristics[3] = &valve_type;
                    sonoff_valve->characteristics[4] = &show_setup;
                
                    if (with_timer) {
                        sonoff_valve->characteristics[5] = &set_duration;
                        sonoff_valve->characteristics[6] = &remaining_duration;
                    }
            }
    
            void charac_garagedoor(const uint8_t service) {
                homekit_service_t *sonoff_garage = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_garage->id = 52;
                sonoff_garage->type = HOMEKIT_SERVICE_GARAGE_DOOR_OPENER;
                sonoff_garage->primary = true;
                sonoff_garage->characteristics = calloc(6, sizeof(homekit_characteristic_t*));
                    sonoff_garage->characteristics[0] = &garage_service_name;
                    sonoff_garage->characteristics[1] = &current_door_state;
                    sonoff_garage->characteristics[2] = &target_door_state;
                    sonoff_garage->characteristics[3] = &obstruction_detected;
                    sonoff_garage->characteristics[4] = &show_setup;
            }
    
            void charac_covering(const uint8_t service, const uint8_t covering_type) {
                homekit_service_t *sonoff_covering = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                
                switch (covering_type) {
                    case 1:
                        sonoff_covering->id = 64;
                        sonoff_covering->type = HOMEKIT_SERVICE_WINDOW;
                        break;
                        
                    case 2:
                        sonoff_covering->id = 70;
                        sonoff_covering->type = HOMEKIT_SERVICE_DOOR;
                        break;
                        
                    default:
                        sonoff_covering->id = 58;
                        sonoff_covering->type = HOMEKIT_SERVICE_WINDOW_COVERING;
                        break;
                }
                
                sonoff_covering->primary = true;
                sonoff_covering->characteristics = calloc(6, sizeof(homekit_characteristic_t*));
                    sonoff_covering->characteristics[0] = &covering_service_name;
                    sonoff_covering->characteristics[1] = &covering_current_position;
                    sonoff_covering->characteristics[2] = &covering_target_position;
                    sonoff_covering->characteristics[3] = &covering_position_state;
                    sonoff_covering->characteristics[4] = &show_setup;
            }
    
            void charac_lock(const uint8_t service) {
                homekit_service_t *sonoff_lock = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_lock->id = 76;
                sonoff_lock->type = HOMEKIT_SERVICE_LOCK_MECHANISM;
                sonoff_lock->primary = true;
                sonoff_lock->characteristics = calloc(5, sizeof(homekit_characteristic_t*));
                    sonoff_lock->characteristics[0] = &lock_service_name;
                    sonoff_lock->characteristics[1] = &lock_current_state;
                    sonoff_lock->characteristics[2] = &lock_target_state;
                    sonoff_lock->characteristics[3] = &show_setup;
            }
    
            void charac_rgbw(const uint8_t service) {
                homekit_service_t *sonoff_rgbw = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_rgbw->id = 81;
                sonoff_rgbw->type = HOMEKIT_SERVICE_LIGHTBULB;
                sonoff_rgbw->primary = true;
                sonoff_rgbw->characteristics = calloc(7, sizeof(homekit_characteristic_t*));
                    sonoff_rgbw->characteristics[0] = &rgbw_service_name;
                    sonoff_rgbw->characteristics[1] = &switch1_on;
                    sonoff_rgbw->characteristics[2] = &brightness;
                    sonoff_rgbw->characteristics[3] = &hue;
                    sonoff_rgbw->characteristics[4] = &saturation;
                    sonoff_rgbw->characteristics[5] = &show_setup;
            }
    
            void charac_switch_dm(const uint8_t service) {
                homekit_service_t *sonoff_switchdm = sonoff->services[service] = calloc(1, sizeof(homekit_service_t));
                sonoff_switchdm->id = 88;
                sonoff_switchdm->type = HOMEKIT_SERVICE_SWITCH;
                sonoff_switchdm->primary = false;
                sonoff_switchdm->characteristics = calloc(3, sizeof(homekit_characteristic_t*));
                    sonoff_switchdm->characteristics[0] = &switchdm_service_name;
                    sonoff_switchdm->characteristics[1] = &switchdm_on;
            }
            // --------
    
            // Create accessory for selected device type
            if (device_type_static == 2) {
                char *device_type_name_value = malloc(11);
                snprintf(device_type_name_value, 11, "Switch 2ch");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_switch_1(1);
                charac_switch_2(2);
                
                service_number = 3;
                
            } else if (device_type_static == 3) {
                char *device_type_name_value = malloc(14);
                snprintf(device_type_name_value, 14, "Socket Button");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_socket(1);
                charac_prog_button(2);
                
                service_number = 3;
                
            } else if (device_type_static == 4) {
                char *device_type_name_value = malloc(11);
                snprintf(device_type_name_value, 11, "Switch 4ch");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_switch_1(1);
                charac_switch_2(2);
                charac_switch_3(3);
                charac_switch_4(4);
                
                service_number = 5;
                
            } else if (device_type_static == 5) {
                char *device_type_name_value = malloc(11);
                snprintf(device_type_name_value, 11, "Thermostat");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_thermostat(1);
                
            } else if (device_type_static == 6) {
                char *device_type_name_value = malloc(17);
                snprintf(device_type_name_value, 17, "Switch TH Sensor");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_switch_1(1);
                charac_temperature(2);
                
                service_number = 3;
                
                if (dht_sensor_type.value.int_value != 3) {
                    charac_humidity(3);
                    
                    service_number += 1;
                }
                
            } else if (device_type_static == 7) {
                char *device_type_name_value = malloc(12);
                snprintf(device_type_name_value, 12, "Water Valve");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_valve(1, custom_garagedoor_has_stop.value.bool_value);
                
            } else if (device_type_static == 8) {
                char *device_type_name_value = malloc(12);
                snprintf(device_type_name_value, 12, "Garage Door");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_garagedoor(1);
                
            } else if (device_type_static == 9) {
                char *device_type_name_value = malloc(17);
                snprintf(device_type_name_value, 17, "Socket Button TH");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_socket(1);
                charac_prog_button(2);
                charac_temperature(3);
                
                service_number = 4;
                
                if (dht_sensor_type.value.int_value != 3) {
                    charac_humidity(4);
                    
                    service_number += 1;
                }
                
            } else if (device_type_static == 10) {
                char *device_type_name_value = malloc(13);
                snprintf(device_type_name_value, 13, "ESP01 Sw Btn");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_switch_1(1);
                charac_prog_button(2);
                
                service_number = 3;
                
            } else if (device_type_static == 11) {
                char *device_type_name_value = malloc(11);
                snprintf(device_type_name_value, 11, "Switch 3ch");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_switch_1(1);
                charac_switch_2(2);
                charac_switch_3(3);
                
                service_number = 4;
                
            } else if (device_type_static == 12) {
                char *device_type_name_value = malloc(9);
                snprintf(device_type_name_value, 9, "Covering");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_covering(1, custom_covering_type.value.int_value);
                
            } else if (device_type_static == 13) {
                char *device_type_name_value = malloc(5);
                snprintf(device_type_name_value, 5, "Lock");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_lock(1);
                
            } else if (device_type_static == 14) {
                char *device_type_name_value = malloc(5);
                snprintf(device_type_name_value, 5, "RGBW");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_rgbw(1);
                
            } else if (device_type_static == 15) {
                char *device_type_name_value = malloc(8);
                snprintf(device_type_name_value, 8, "Prog SW");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_prog_button(1);
                
            } else { // device_type_static == 1
                char *device_type_name_value = malloc(11);
                snprintf(device_type_name_value, 11, "Switch 1ch");
                device_type_name.value = HOMEKIT_STRING(device_type_name_value);
                
                charac_switch_1(1);
            }
    
            if (enable_dummy_switch.value.bool_value) {
                charac_switch_dm(service_number);
                
                service_number += 1;
            }

            // Setup Accessory, visible only in 3party Apps
            if (show_setup.value.bool_value) {
                printf("RC > Creating Setup\n");
                
                homekit_service_t *sonoff_setup = sonoff->services[service_number] = calloc(1, sizeof(homekit_service_t));
                sonoff_setup->id = 99;
                sonoff_setup->type = HOMEKIT_SERVICE_CUSTOM_SETUP;
                sonoff_setup->primary = false;
        
                uint8_t setting_number = 12;
                uint8_t setting_count = setting_number + 1;

                switch (device_type_static) {
                    case 2:
                        setting_count += 7;
                        break;
                        
                    case 3:
                        setting_count += 2;
                        break;
                        
                    case 4:
                        setting_count += 11;
                        break;
                        
                    case 5:
                        setting_count += 6;
                        break;
                        
                    case 6:
                        setting_count += 6;
                        break;
                        
                    case 7:
                        setting_count += 2;
                        break;
                        
                    case 8:
                        setting_count += 7;
                        break;
                        
                    case 9:
                        setting_count += 6;
                        break;
                        
                    case 10:
                        setting_count += 2;
                        break;
                        
                    case 11:
                        setting_count += 8;
                        break;
                        
                    case 12:
                        setting_count += 5;
                        break;
                        
                    case 13:
                        setting_count += 2;
                        break;
                        
                    case 14:
                        setting_count += 6;
                        break;
                        
                    case 15:
                        setting_count += 1;
                        break;
                        
                    default:    // case 1:
                        setting_count += 3;
                        break;
                }
                
                sysparam_status_t status;
                char *char_value;
                
                status = sysparam_get_string(OTA_REPO_SYSPARAM, &char_value);
                if (status == SYSPARAM_OK) {
                    setting_count += 1;
                }
        
                sonoff_setup->characteristics = calloc(setting_count, sizeof(homekit_characteristic_t*));
                    sonoff_setup->characteristics[0] = &setup_service_name;
                    sonoff_setup->characteristics[1] = &device_type_name;
                    sonoff_setup->characteristics[2] = &device_type;
                    sonoff_setup->characteristics[3] = &reboot_device;
                    sonoff_setup->characteristics[4] = &board_type;
                    sonoff_setup->characteristics[5] = &log_output;
                    sonoff_setup->characteristics[6] = &ip_addr;
                    sonoff_setup->characteristics[7] = &wifi_reset;
                    sonoff_setup->characteristics[8] = &custom_reverse_sw1;
                    sonoff_setup->characteristics[9] = &enable_dummy_switch;
                    sonoff_setup->characteristics[10] = &custom_init_state_swdm;
                    sonoff_setup->characteristics[11] = &custom_inching_timedm;
                
                    if (status == SYSPARAM_OK) {
                        sonoff_setup->characteristics[setting_number] = &ota_firmware;
                        setting_number++;
                    }
                
                    if (device_type_static == 1) {
                        sonoff_setup->characteristics[setting_number] = &external_toggle1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_inching_time1;
                        
                    } else if (device_type_static == 2) {
                        sonoff_setup->characteristics[setting_number] = &external_toggle1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &external_toggle2;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw2;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_reverse_sw2;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_inching_time1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_inching_time2;
                        
                    } else if (device_type_static == 3) {
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_inching_time1;
                        
                    } else if (device_type_static == 4) {
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw2;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw3;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw4;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_reverse_sw2;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_reverse_sw3;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_reverse_sw4;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_inching_time1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_inching_time2;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_inching_time3;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_inching_time4;
                        
                    } else if (device_type_static == 5) {
                        sonoff_setup->characteristics[setting_number] = &dht_sensor_type;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &hum_offset;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &temp_offset;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &temp_deadband;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_th;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &poll_period;
                        
                    } else if (device_type_static == 6) {
                        sonoff_setup->characteristics[setting_number] = &dht_sensor_type;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &hum_offset;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &temp_offset;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &poll_period;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_inching_time1;
                        
                    } else if (device_type_static == 7) {
                        sonoff_setup->characteristics[setting_number] = &custom_valve_type;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_has_stop;
                    
                    } else if (device_type_static == 8) {
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_has_stop;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_sensor_close_nc;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_sensor_open;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_sensor_obstruction;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_working_time;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_garagedoor_control_with_button;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_inching_time1;
                        
                    } else if (device_type_static == 9) {
                        sonoff_setup->characteristics[setting_number] = &dht_sensor_type;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &hum_offset;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &temp_offset;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &poll_period;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_inching_time1;
                        
                    } else if (device_type_static == 10) {
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_inching_time1;
                        
                    } else if (device_type_static == 11) {
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw2;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw3;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_reverse_sw2;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_reverse_sw3;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_inching_time1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_inching_time2;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_inching_time3;
                        
                    } else if (device_type_static == 12) {
                        sonoff_setup->characteristics[setting_number] = &external_toggle1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_reverse_sw2;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_covering_up_time;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_covering_down_time;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_covering_type;
                        
                    } else if (device_type_static == 13) {
                        sonoff_setup->characteristics[setting_number] = &external_toggle1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_inching_time1;
                        
                    } else if (device_type_static == 14) {
                        sonoff_setup->characteristics[setting_number] = &custom_init_state_sw1;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_r_gpio;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_g_gpio;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_b_gpio;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_w_gpio;
                        setting_number++;
                        sonoff_setup->characteristics[setting_number] = &custom_color_boost;
                        
                    } else if (device_type_static == 15) {
                        sonoff_setup->characteristics[setting_number] = &custom_w_gpio;
                        
                    }
            }
    
    config.accessories = accessories;
    config.password = "021-82-017";
    config.setupId = "RAVE";
    config.category = accessory_category;
    config.config_number = FIRMWARE_VERSION_OCTAL;
    
    printf("RC > Starting HomeKit Server\n");
    homekit_server_init(&config);
    
    if (enable_dummy_switch.value.bool_value) {
        switchdm_on_callback(switchdm_on.value);
    }
}

void old_settings_init() {
    sysparam_status_t status;
    bool bool_value;
    int8_t int8_value;
    int32_t int32_value;
    
    // Load old Saved Settings and set factory values for missing settings
    printf("RC > Loading old settings and cleaning\n");
    
    status = sysparam_get_bool("show_setup", &bool_value);
    if (status == SYSPARAM_OK) {
        show_setup.value.bool_value = bool_value;
        status = sysparam_set_data("show_setup", NULL, 0, false);
    }
    
    status = sysparam_get_int8("board_type", &int8_value);
    if (status == SYSPARAM_OK) {
        if (board_type.value.int_value != 3) {
            board_type.value.int_value = int8_value;
        }
        
        status = sysparam_set_data("board_type", NULL, 0, false);
    }
    
    status = sysparam_get_bool("gpio14_toggle", &bool_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("gpio14_toggle", NULL, 0, false);
    }
    
    status = sysparam_get_int8("dht_sensor_type", &int8_value);
    if (status == SYSPARAM_OK) {
        dht_sensor_type.value.int_value = int8_value;
        status = sysparam_set_data("dht_sensor_type", NULL, 0, false);
    }
    
    status = sysparam_get_int8("poll_period", &int8_value);
    if (status == SYSPARAM_OK) {
        poll_period.value.int_value = int8_value;
        status = sysparam_set_data("poll_period", NULL, 0, false);
    }
    
    status = sysparam_get_int8("valve_type", &int8_value);
    if (status == SYSPARAM_OK) {
        valve_type.value.int_value = int8_value;
        custom_valve_type.value.int_value = int8_value;
        status = sysparam_set_data("valve_type", NULL, 0, false);
    }
    
    status = sysparam_get_int8("garagedoor_working_time", &int8_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_working_time.value.int_value = int8_value;
        status = sysparam_set_data("garagedoor_working_time", NULL, 0, false);
    }
    
    status = sysparam_get_bool("garagedoor_has_stop", &bool_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_has_stop.value.bool_value = bool_value;
        status = sysparam_set_data("garagedoor_has_stop", NULL, 0, false);
    }
    
    status = sysparam_get_bool("garagedoor_sensor_close_nc", &bool_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_sensor_close_nc.value.bool_value = bool_value;
        status = sysparam_set_data("garagedoor_sensor_close_nc", NULL, 0, false);
    }
    
    status = sysparam_get_bool("garagedoor_sensor_open_nc", &bool_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("garagedoor_sensor_open_nc", NULL, 0, false);
    }
    
    status = sysparam_get_bool("garagedoor_has_sensor_open", &bool_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("garagedoor_has_sensor_open", NULL, 0, false);
    }
    
    status = sysparam_get_bool("garagedoor_control_with_button", &bool_value);
    if (status == SYSPARAM_OK) {
        custom_garagedoor_control_with_button.value.bool_value = bool_value;
        status = sysparam_set_data("garagedoor_control_with_button", NULL, 0, false);
    }
    
    status = sysparam_get_int8("init_state_sw1", &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw1.value.int_value = int8_value;
        status = sysparam_set_data("init_state_sw1", NULL, 0, false);
    }
    
    status = sysparam_get_int8("init_state_sw2", &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw2.value.int_value = int8_value;
        status = sysparam_set_data("init_state_sw2", NULL, 0, false);
    }
    
    status = sysparam_get_int8("init_state_sw3", &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw3.value.int_value = int8_value;
        status = sysparam_set_data("init_state_sw3", NULL, 0, false);
    }
    
    status = sysparam_get_int8("init_state_sw4", &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_sw4.value.int_value = int8_value;
        status = sysparam_set_data("init_state_sw4", NULL, 0, false);
    }
    
    status = sysparam_get_int8("init_state_th", &int8_value);
    if (status == SYSPARAM_OK) {
        custom_init_state_th.value.int_value = int8_value;
        status = sysparam_set_data("init_state_th", NULL, 0, false);
    }
    
    printf("RC > Removing old saved states\n");
    
    status = sysparam_get_bool("last_state_sw1", &bool_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("last_state_sw1", NULL, 0, false);
    }
    
    status = sysparam_get_bool("last_state_sw2", &bool_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("last_state_sw2", NULL, 0, false);
    }
    
    status = sysparam_get_bool("last_state_sw3", &bool_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("last_state_sw3", NULL, 0, false);
    }
    
    status = sysparam_get_bool("last_state_sw4", &bool_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("last_state_sw4", NULL, 0, false);
    }
    
    status = sysparam_get_int8("last_target_state_th", &int8_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("last_target_state_th", NULL, 0, false);
    }
    
    status = sysparam_get_int32("target_temp", &int32_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("target_temp", NULL, 0, false);
    }
    
    status = sysparam_get_int32("set_duration", &int32_value);
    if (status == SYSPARAM_OK) {
        status = sysparam_set_data("set_duration", NULL, 0, false);
    }
    
    sdk_os_timer_disarm(&change_settings_timer);
    
    save_settings();
    status = sysparam_compact();
    
    if (status == SYSPARAM_OK) {
        device_restart();
    } else {
        printf("RC ! ERROR in old_settings_init loading old settings. Flash problem\n");
    }
}

void user_init(void) {
    sysparam_status_t status;
    int8_t int8_value;
    
    status = sysparam_get_int8(LOG_OUTPUT_SYSPARAM, &int8_value);
    if (status == SYSPARAM_OK) {
        log_output.value.int_value = int8_value;
    } else {
        status = sysparam_set_int8(LOG_OUTPUT_SYSPARAM, 1);
    }
    
    switch (log_output.value.int_value) {
        case 1:
            uart_set_baud(0, 115200);
            break;
            
        default:
            break;
    }
    
    printf("\n\nRC > RavenCore\nRC > Developed by RavenSystem - José Antonio Jiménez\nRC > Version: %s\n\n", firmware.value.string_value);
    
    // Old settings check
    status = sysparam_get_int8("device_type", &int8_value);
    if (status != SYSPARAM_OK) {
        settings_init();
    } else {
        // For old Shelly device types (device type 12 and 13)
        if (int8_value == 12) {
            int8_value = 1;
            board_type.value.int_value = 3;
        }
        
        if (int8_value == 13) {
            int8_value = 2;
            board_type.value.int_value = 3;
        }
        // END - For old Shelly device types (device type 12 and 13)
        
        device_type.value.int_value = int8_value;
        device_type_static = int8_value;
        status = sysparam_set_data("device_type", NULL, 0, false);
        old_settings_init();
    }
    // END Old settings check
}
