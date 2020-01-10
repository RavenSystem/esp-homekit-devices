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

#ifndef __HAA_HEADER_H__
#define __HAA_HEADER_H__

// Version
#define FIRMWARE_VERSION                    "1.2.1"
#define FIRMWARE_VERSION_OCTAL              010201      // Matches as example: firmware_revision 2.3.8 = 02.03.10 (octal) = config_number 020310

// Characteristic types (ch_type)
#define CH_TYPE_BOOL                        0
#define CH_TYPE_INT8                        1
#define CH_TYPE_INT32                       2
#define CH_TYPE_FLOAT                       3

// Auto-off types (type)
#define TYPE_ON                             0
#define TYPE_LOCK                           1
#define TYPE_SENSOR                         2
#define TYPE_SENSOR_BOOL                    3
#define TYPE_VALVE                          4
#define TYPE_LIGHTBULB                      5
#define TYPE_GARAGE_DOOR                    6
#define TYPE_WINDOW_COVER                   7

// Button Events
#define SINGLEPRESS_EVENT                   0
#define DOUBLEPRESS_EVENT                   1
#define LONGPRESS_EVENT                     2

// Initial states
#define INIT_STATE_FIXED_INPUT              4
#define INIT_STATE_LAST                     5
#define INIT_STATE_INV_LAST                 6
#define INIT_STATE_LAST_STR                 "{\"s\":5}"

// JSON
#define GENERAL_CONFIG                      "c"
#define CUSTOM_HOSTNAME                     "n"
#define LOG_OUTPUT                          "o"
#define ALLOWED_SETUP_MODE_TIME             "m"
#define STATUS_LED_GPIO                     "l"
#define INVERTED                            "i"
#define BUTTON_FILTER                       "f"
#define PWM_FREQ                            "q"
#define ENABLE_HOMEKIT_SERVER               "h"
#define ALLOW_INSECURE_CONNECTIONS          "u"
#define ACCESSORIES                         "a"
#define BUTTONS_ARRAY                       "b"
#define FIXED_BUTTONS_ARRAY_0               "f0"
#define FIXED_BUTTONS_ARRAY_1               "f1"
#define FIXED_BUTTONS_ARRAY_2               "f2"
#define FIXED_BUTTONS_ARRAY_3               "f3"
#define FIXED_BUTTONS_ARRAY_4               "f4"
#define FIXED_BUTTONS_ARRAY_5               "f5"
#define FIXED_BUTTONS_ARRAY_6               "f6"
#define FIXED_BUTTONS_ARRAY_7               "f7"
#define FIXED_BUTTONS_ARRAY_8               "f8"
#define BUTTON_PRESS_TYPE                   "t"
#define PULLUP_RESISTOR                     "p"
#define VALUE                               "v"
#define DIGITAL_OUTPUTS_ARRAY               "r"
#define MANAGE_OTHERS_ACC_ARRAY             "m"
#define ACCESSORY_INDEX                     "g"
#define ACCESSORY_INDEX_KILL_SWITCH         "k"
#define AUTOSWITCH_TIME                     "i"
#define PIN_GPIO                            "g"
#define INITIAL_STATE                       "s"
#define KILL_SWITCH                         "k"

#define VALVE_SYSTEM_TYPE                   "w"
#define VALVE_SYSTEM_TYPE_DEFAULT           0
#define VALVE_MAX_DURATION                  "d"
#define VALVE_MAX_DURATION_DEFAULT          3600

#define THERMOSTAT_TYPE                     "w"
#define THERMOSTAT_TYPE_HEATER              1
#define THERMOSTAT_TYPE_COOLER              2
#define THERMOSTAT_TYPE_HEATERCOOLER        3
#define THERMOSTAT_MIN_TEMP                 "m"
#define THERMOSTAT_DEFAULT_MIN_TEMP         10
#define THERMOSTAT_MAX_TEMP                 "x"
#define THERMOSTAT_DEFAULT_MAX_TEMP         38
#define THERMOSTAT_DEADBAND                 "d"
#define THERMOSTAT_POLL_PERIOD              "j"
#define THERMOSTAT_DEFAULT_POLL_PERIOD      30
#define THERMOSTAT_MODE_OFF                 0
#define THERMOSTAT_MODE_IDLE                1
#define THERMOSTAT_MODE_HEATER              2
#define THERMOSTAT_MODE_COOLER              3
#define THERMOSTAT_TARGET_MODE_AUTO         0
#define THERMOSTAT_TARGET_MODE_HEATER       1
#define THERMOSTAT_TARGET_MODE_COOLER       2
#define THERMOSTAT_ACTION_TOTAL_OFF         0
#define THERMOSTAT_ACTION_HEATER_IDLE       1
#define THERMOSTAT_ACTION_COOLER_IDLE       2
#define THERMOSTAT_ACTION_HEATER_ON         3
#define THERMOSTAT_ACTION_COOLER_ON         4
#define THERMOSTAT_ACTION_SENSOR_ERROR      5
#define THERMOSTAT_TEMP_UP                  0
#define THERMOSTAT_TEMP_DOWN                1

#define TEMPERATURE_SENSOR_GPIO             "g"
#define TEMPERATURE_SENSOR_TYPE             "n"
#define TEMPERATURE_OFFSET                  "z"
#define HUMIDITY_OFFSET                     "h"
#define TH_SENSOR_DELAYED_TIME_MS           3000

#define LIGHTBULB_PWM_GPIO_R                "r"
#define LIGHTBULB_PWM_GPIO_G                "g"
#define LIGHTBULB_PWM_GPIO_B                "v"
#define LIGHTBULB_PWM_GPIO_W                "w"
#define LIGHTBULB_FACTOR_R                  "fr"
#define LIGHTBULB_FACTOR_G                  "fg"
#define LIGHTBULB_FACTOR_B                  "fv"
#define LIGHTBULB_FACTOR_W                  "fw"
#define RGBW_PERIOD                         10
#define RGBW_STEP                           "p"
#define RGBW_STEP_DEFAULT                   1024
#define PWM_SCALE                           (UINT16_MAX - 1)
#define COLOR_TEMP_MIN                      71
#define COLOR_TEMP_MAX                      400
#define LIGHTBULB_BRIGHTNESS_UP             0
#define LIGHTBULB_BRIGHTNESS_DOWN           1
#define AUTODIMMER_DELAY                    500
#define AUTODIMMER_TASK_DELAY               "d"
#define AUTODIMMER_TASK_DELAY_DEFAULT       1000
#define AUTODIMMER_TASK_STEP                "e"
#define AUTODIMMER_TASK_STEP_DEFAULT        20

#define GARAGE_DOOR_OPENED                  0
#define GARAGE_DOOR_CLOSED                  1
#define GARAGE_DOOR_OPENING                 2
#define GARAGE_DOOR_CLOSING                 3
#define GARAGE_DOOR_STOPPED                 4
#define GARAGE_DOOR_TIME_OPEN_SET           "d"
#define GARAGE_DOOR_TIME_OPEN_DEFAULT       30
#define GARAGE_DOOR_TIME_CLOSE_SET          "c"
#define GARAGE_DOOR_TIME_MARGIN_SET         "e"
#define GARAGE_DOOR_TIME_MARGIN_DEFAULT     0
#define GARAGE_DOOR_CURRENT_TIME            ch_group->num0
#define GARAGE_DOOR_WORKING_TIME            ch_group->num1
#define GARAGE_DOOR_TIME_MARGIN             ch_group->num2
#define GARAGE_DOOR_CLOSE_TIME_FACTOR       ch_group->num3

#define WINDOW_COVER_CLOSING                0
#define WINDOW_COVER_OPENING                1
#define WINDOW_COVER_STOP                   2
#define WINDOW_COVER_CLOSING_FROM_MOVING    3
#define WINDOW_COVER_OPENING_FROM_MOVING    4
#define WINDOW_COVER_STOP_FROM_CLOSING      5
#define WINDOW_COVER_STOP_FROM_OPENING      6
#define WINDOW_COVER_OBSTRUCTION            7
#define WINDOW_COVER_TYPE                   "w"
#define WINDOW_COVER_TYPE_DEFAULT           0
#define WINDOW_COVER_TIME_OPEN_SET          "o"
#define WINDOW_COVER_TIME_OPEN_DEFAULT      15
#define WINDOW_COVER_TIME_CLOSE_SET         "c"
#define WINDOW_COVER_CORRECTION_SET         "f"
#define WINDOW_COVER_CORRECTION_DEFAULT     0
#define WINDOW_COVER_POLL_PERIOD_MS         250
#define WINDOW_COVER_MARGIN_SYNC            15
#define WINDOW_COVER_STEP_TIME(x)           ((100.0 / (x)) * (WINDOW_COVER_POLL_PERIOD_MS / 1000.0))
#define WINDOW_COVER_STEP_TIME_UP           ch_group->num0
#define WINDOW_COVER_STEP_TIME_DOWN         ch_group->num1
#define WINDOW_COVER_POSITION               ch_group->num2
#define WINDOW_COVER_REAL_POSITION          ch_group->num3
#define WINDOW_COVER_CORRECTION             ch_group->num4
#define WINDOW_COVER_CH_CURRENT_POSITION    ch_group->ch0
#define WINDOW_COVER_CH_TARGET_POSITION     ch_group->ch1
#define WINDOW_COVER_CH_STATE               ch_group->ch2
#define WINDOW_COVER_CH_OBSTRUCTION         ch_group->ch3

#define MAX_ACTIONS                         10   // from 0 to ...
#define COPY_ACTIONS                        "a"
#define SYSTEM_ACTIONS_ARRAY                "s"
#define SYSTEM_ACTION                       "a"
#define SYSTEM_ACTION_REBOOT                0
#define SYSTEM_ACTION_SETUP_MODE            1
#define SYSTEM_ACTION_OTA_UPDATE            2

#define ACCESSORY_TYPE                      "t"
#define ACC_TYPE_SWITCH                     1
#define ACC_TYPE_OUTLET                     2
#define ACC_TYPE_BUTTON                     3
#define ACC_TYPE_LOCK                       4
#define ACC_TYPE_CONTACT_SENSOR             5
#define ACC_TYPE_OCCUPANCY_SENSOR           6
#define ACC_TYPE_LEAK_SENSOR                7
#define ACC_TYPE_SMOKE_SENSOR               8
#define ACC_TYPE_CARBON_MONOXIDE_SENSOR     9
#define ACC_TYPE_CARBON_DIOXIDE_SENSOR      10
#define ACC_TYPE_FILTER_CHANGE_SENSOR       11
#define ACC_TYPE_MOTION_SENSOR              12
#define ACC_TYPE_WATER_VALVE                20
#define ACC_TYPE_THERMOSTAT                 21
#define ACC_TYPE_TEMP_SENSOR                22
#define ACC_TYPE_HUM_SENSOR                 23
#define ACC_TYPE_TH_SENSOR                  24
#define ACC_TYPE_LIGHTBULB                  30
#define ACC_TYPE_GARAGE_DOOR                40
#define ACC_TYPE_WINDOW_COVER               45

#define ACC_CREATION_DELAY                  30
#define EXIT_EMERGENCY_SETUP_MODE_TIME      2200
#define SETUP_MODE_ACTIVATE_COUNT           "z"
#define SETUP_MODE_DEFAULT_ACTIVATE_COUNT   8
#define SETUP_MODE_TOGGLE_TIME_MS           1050

#define WIFI_STATUS_DISCONNECTED            0
#define WIFI_STATUS_CONNECTING              1
#define WIFI_STATUS_PRECONNECTED            2
#define WIFI_STATUS_CONNECTED               3
#define WIFI_WATCHDOG_POLL_PERIOD_MS        7500

#define ACCESSORIES_WITHOUT_BRIDGE          4   // Max number of accessories before using a bridge

#define MS_TO_TICK(x)                       ((x) / portTICK_PERIOD_MS)

#define DEBUG(message, ...)                 printf("HAA > %s: " message "\n", __func__, ##__VA_ARGS__);
#define INFO(message, ...)                  printf("HAA > " message "\n", ##__VA_ARGS__);
#define ERROR(message, ...)                 printf("HAA ! " message "\n", ##__VA_ARGS__);

#define FREEHEAP()                          printf("HAA > Free Heap: %d\n", xPortGetFreeHeapSize())

#endif // __HAA_HEADER_H__
