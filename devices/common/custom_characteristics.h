/*
 * HomeKit Custom Characteristics
 * 
 * Copyright 2018-2019 José A. Jiménez (@RavenSystem)
 *  
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __HOMEKIT_CUSTOM_CHARACTERISTICS__
#define __HOMEKIT_CUSTOM_CHARACTERISTICS__

#define HOMEKIT_CUSTOM_UUID(value) (value "-03a1-4971-92bf-af2b7d833922")

#define HOMEKIT_SERVICE_CUSTOM_SETUP HOMEKIT_CUSTOM_UUID("F00000FF")

#define HOMEKIT_CHARACTERISTIC_CUSTOM_SHOW_SETUP HOMEKIT_CUSTOM_UUID("00000001")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_SHOW_SETUP(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_SHOW_SETUP, \
    .description = "Show Setup", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_REBOOT_DEVICE HOMEKIT_CUSTOM_UUID("00000002")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_REBOOT_DEVICE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_REBOOT_DEVICE, \
    .description = "Reboot", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_BOARD_TYPE HOMEKIT_CUSTOM_UUID("F0000003")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_BOARD_TYPE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_BOARD_TYPE, \
    .description = "Board Type", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {1}, \
    .max_value = (float[]) {5}, \
    .min_step = (float[]) {1}, \
    .valid_values = { \
    .count = 5, \
    .values = (uint8_t[]) {1, 2, 3, 4, 5}, \
    }, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_OTA_UPDATE HOMEKIT_CUSTOM_UUID("F0000004")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_OTA_UPDATE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_OTA_UPDATE, \
    .description = "Firmware Update", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_OTA_BETA HOMEKIT_CUSTOM_UUID("F0000005")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_OTA_BETA(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_OTA_BETA, \
    .description = "Firmware BETA", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

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
 13. Lock
 14. MagicHome
 15. Stateless Switch
 */
#define HOMEKIT_CHARACTERISTIC_CUSTOM_DEVICE_TYPE HOMEKIT_CUSTOM_UUID("F0000100")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_DEVICE_TYPE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_DEVICE_TYPE, \
    .description = "Device Type", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {1}, \
    .max_value = (float[]) {15}, \
    .min_step = (float[]) {1}, \
    .valid_values = { \
    .count = 15, \
    .values = (uint8_t[]) {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}, \
    }, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_DEVICE_TYPE_NAME HOMEKIT_CUSTOM_UUID("F0000103")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_DEVICE_TYPE_NAME(revision, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_DEVICE_TYPE_NAME, \
    .description = "Actual Dev Type", \
    .format = homekit_format_string, \
    .permissions = homekit_permissions_paired_read, \
    .value = HOMEKIT_STRING_(revision), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_EXTERNAL_TOGGLE1 HOMEKIT_CUSTOM_UUID("F0000104")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_EXTERNAL_TOGGLE1(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_EXTERNAL_TOGGLE1, \
    .description = "External Toggle 1", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {3}, \
    .min_step = (float[]) {1}, \
    .valid_values = { \
    .count = 4, \
    .values = (uint8_t[]) {0, 1, 2, 3}, \
    }, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_EXTERNAL_TOGGLE2 HOMEKIT_CUSTOM_UUID("F0000105")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_EXTERNAL_TOGGLE2(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_EXTERNAL_TOGGLE2, \
    .description = "External Toggle 2", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {3}, \
    .min_step = (float[]) {1}, \
    .valid_values = { \
    .count = 4, \
    .values = (uint8_t[]) {0, 1, 2, 3}, \
    }, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_DHT_SENSOR_TYPE HOMEKIT_CUSTOM_UUID("F0000106")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_DHT_SENSOR_TYPE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_DHT_SENSOR_TYPE, \
    .description = "TH Sensor Type", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {1}, \
    .max_value = (float[]) {3}, \
    .min_step = (float[]) {1}, \
    .valid_values = { \
    .count = 3, \
    .values = (uint8_t[]) {1, 2, 3}, \
    }, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_VALVE_TYPE HOMEKIT_CUSTOM_UUID("F0000107")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_VALVE_TYPE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_VALVE_TYPE, \
    .description = "Valve Type", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {3}, \
    .min_step = (float[]) {1}, \
    .valid_values = { \
    .count = 4, \
    .values = (uint8_t[]) {0, 1, 2, 3}, \
    }, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_TH_PERIOD HOMEKIT_CUSTOM_UUID("F0000108")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_TH_PERIOD(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_TH_PERIOD, \
    .description = "TH Period", \
    .format = homekit_format_uint8, \
    .unit = homekit_unit_seconds, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {3}, \
    .max_value = (float[]) {60}, \
    .min_step = (float[]) {1}, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_LOG_OUTPUT HOMEKIT_CUSTOM_UUID("F0000109")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_LOG_OUTPUT(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_LOG_OUTPUT, \
    .description = "Log output", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {2}, \
    .min_step = (float[]) {1}, \
    .valid_values = { \
    .count = 3, \
    .values = (uint8_t[]) {0, 1, 2}, \
    }, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_GARAGEDOOR_HAS_STOP HOMEKIT_CUSTOM_UUID("F0000110")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_GARAGEDOOR_HAS_STOP(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_GARAGEDOOR_HAS_STOP, \
    .description = "Has Stop", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_GARAGEDOOR_SENSOR_CLOSE_NC HOMEKIT_CUSTOM_UUID("F0000111")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_GARAGEDOOR_SENSOR_CLOSE_NC(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_GARAGEDOOR_SENSOR_CLOSE_NC, \
    .description = "Sensor Close NC", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_GARAGEDOOR_SENSOR_OPEN HOMEKIT_CUSTOM_UUID("F0000112")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_GARAGEDOOR_SENSOR_OPEN(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_GARAGEDOOR_SENSOR_OPEN, \
    .description = "Sensor Open", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {2}, \
    .min_step = (float[]) {1}, \
    .valid_values = { \
    .count = 3, \
    .values = (uint8_t[]) {0, 1, 2}, \
    }, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_INCHING_TIME1 HOMEKIT_CUSTOM_UUID("F0000113")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_INCHING_TIME1(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_INCHING_TIME1, \
    .description = "Inching time 1", \
    .format = homekit_format_float, \
    .unit = homekit_unit_seconds, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {12}, \
    .min_step = (float[]) {0.3}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_INCHING_TIME2 HOMEKIT_CUSTOM_UUID("F0001113")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_INCHING_TIME2(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_INCHING_TIME2, \
    .description = "Inching time 2", \
    .format = homekit_format_float, \
    .unit = homekit_unit_seconds, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {12}, \
    .min_step = (float[]) {0.3}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_INCHING_TIME3 HOMEKIT_CUSTOM_UUID("F0002113")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_INCHING_TIME3(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_INCHING_TIME3, \
    .description = "Inching time 3", \
    .format = homekit_format_float, \
    .unit = homekit_unit_seconds, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {12}, \
    .min_step = (float[]) {0.3}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_INCHING_TIME4 HOMEKIT_CUSTOM_UUID("F0003113")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_INCHING_TIME4(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_INCHING_TIME4, \
    .description = "Inching time 4", \
    .format = homekit_format_float, \
    .unit = homekit_unit_seconds, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {12}, \
    .min_step = (float[]) {0.3}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_INCHING_TIMEDM HOMEKIT_CUSTOM_UUID("F0004113")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_INCHING_TIMEDM(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_INCHING_TIMEDM, \
    .description = "Inching time DM", \
    .format = homekit_format_float, \
    .unit = homekit_unit_seconds, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {12}, \
    .min_step = (float[]) {0.3}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_GARAGEDOOR_WORKINGTIME HOMEKIT_CUSTOM_UUID("F0000114")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_GARAGEDOOR_WORKINGTIME(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_GARAGEDOOR_WORKINGTIME, \
    .description = "Working time", \
    .format = homekit_format_uint8, \
    .unit = homekit_unit_seconds, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {3}, \
    .max_value = (float[]) {90}, \
    .min_step = (float[]) {1}, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_GARAGEDOOR_CONTROL_WITH_BUTTON HOMEKIT_CUSTOM_UUID("F0000115")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_GARAGEDOOR_CONTROL_WITH_BUTTON(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_GARAGEDOOR_CONTROL_WITH_BUTTON, \
    .description = "With Button-Control", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_GARAGEDOOR_SENSOR_OBSTRUCTION HOMEKIT_CUSTOM_UUID("F0000116")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_GARAGEDOOR_SENSOR_OBSTRUCTION(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_GARAGEDOOR_SENSOR_OBSTRUCTION, \
    .description = "Sensor Obstruction", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {2}, \
    .min_step = (float[]) {1}, \
    .valid_values = { \
    .count = 3, \
    .values = (uint8_t[]) {0, 1, 2}, \
    }, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_HUMIDITY_OFFSET HOMEKIT_CUSTOM_UUID("F0000117")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_HUMIDITY_OFFSET(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_HUMIDITY_OFFSET, \
    .description = "Offset HUM", \
    .format = homekit_format_float, \
    .unit = homekit_unit_percentage, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {-15}, \
    .max_value = (float[]) {15}, \
    .min_step = (float[]) {1}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_TEMPERATURE_OFFSET HOMEKIT_CUSTOM_UUID("F0000118")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_TEMPERATURE_OFFSET(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_TEMPERATURE_OFFSET, \
    .description = "Offset TEMP", \
    .format = homekit_format_float, \
    .unit = homekit_unit_celsius, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {-15}, \
    .max_value = (float[]) {15}, \
    .min_step = (float[]) {0.1}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_REVERSE_SW1 HOMEKIT_CUSTOM_UUID("F0000119")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_REVERSE_SW1(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_REVERSE_SW1, \
    .description = "Reverse SW1", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_REVERSE_SW2 HOMEKIT_CUSTOM_UUID("F0000120")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_REVERSE_SW2(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_REVERSE_SW2, \
    .description = "Reverse SW2", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_REVERSE_SW3 HOMEKIT_CUSTOM_UUID("F0000121")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_REVERSE_SW3(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_REVERSE_SW3, \
    .description = "Reverse SW3", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_REVERSE_SW4 HOMEKIT_CUSTOM_UUID("F0000122")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_REVERSE_SW4(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_REVERSE_SW4, \
    .description = "Reverse SW4", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_COVERING_UP_TIME HOMEKIT_CUSTOM_UUID("F0000123")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_COVERING_UP_TIME(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_COVERING_UP_TIME, \
    .description = "Time Open", \
    .format = homekit_format_float, \
    .unit = homekit_unit_seconds, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {3}, \
    .max_value = (float[]) {60}, \
    .min_step = (float[]) {0.2}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_COVERING_DOWN_TIME HOMEKIT_CUSTOM_UUID("F0000124")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_COVERING_DOWN_TIME(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_COVERING_DOWN_TIME, \
    .description = "Time Close", \
    .format = homekit_format_float, \
    .unit = homekit_unit_seconds, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {3}, \
    .max_value = (float[]) {60}, \
    .min_step = (float[]) {0.2}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_COVERING_TYPE HOMEKIT_CUSTOM_UUID("F0000125")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_COVERING_TYPE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_COVERING_TYPE, \
    .description = "Cover Type", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {2}, \
    .min_step = (float[]) {1}, \
    .valid_values = { \
    .count = 3, \
    .values = (uint8_t[]) {0, 1, 2}, \
    }, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_TEMPERATURE_DEADBAND HOMEKIT_CUSTOM_UUID("F0000126")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_TEMPERATURE_DEADBAND(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_TEMPERATURE_DEADBAND, \
    .description = "TEMP Deadband", \
    .format = homekit_format_float, \
    .unit = homekit_unit_celsius, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {3}, \
    .min_step = (float[]) {0.1}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_R_GPIO HOMEKIT_CUSTOM_UUID("F0000127")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_R_GPIO(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_R_GPIO, \
    .description = "Pin R", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {5}, \
    .max_value = (float[]) {16}, \
    .min_step = (float[]) {1}, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_G_GPIO HOMEKIT_CUSTOM_UUID("F0000128")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_G_GPIO(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_G_GPIO, \
    .description = "Pin G", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {5}, \
    .max_value = (float[]) {16}, \
    .min_step = (float[]) {1}, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_B_GPIO HOMEKIT_CUSTOM_UUID("F0000129")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_B_GPIO(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_B_GPIO, \
    .description = "Pin B", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {5}, \
    .max_value = (float[]) {16}, \
    .min_step = (float[]) {1}, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_W_GPIO HOMEKIT_CUSTOM_UUID("F0000130")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_W_GPIO(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_W_GPIO, \
    .description = "Pin W", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {16}, \
    .min_step = (float[]) {1}, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_BOOST HOMEKIT_CUSTOM_UUID("F0000131")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_BOOST(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_BOOST, \
    .description = "Boost", \
    .format = homekit_format_uint8, \
    .unit = homekit_unit_percentage, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {100}, \
    .min_step = (float[]) {1}, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_BUTTON_FILTER HOMEKIT_CUSTOM_UUID("F0000140")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_BUTTON_FILTER(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_BUTTON_FILTER, \
    .description = "Button Filter", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {100}, \
    .min_step = (float[]) {1}, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_SWITCH_DM HOMEKIT_CUSTOM_UUID("F0000150")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_SWITCH_DM(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_SWITCH_DM, \
    .description = "Dummy Switch", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

/* Switch Initial State
 0. OFF
 1. ON
 2. Last state
 3. Opposite to last state
 */
#define HOMEKIT_CHARACTERISTIC_CUSTOM_INIT_STATE_SW1 HOMEKIT_CUSTOM_UUID("F0000501")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_INIT_STATE_SW1(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_INIT_STATE_SW1, \
    .description = "Init State SW1", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {3}, \
    .min_step = (float[]) {1}, \
    .valid_values = { \
    .count = 4, \
    .values = (uint8_t[]) {0, 1, 2, 3}, \
    }, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_INIT_STATE_SW2 HOMEKIT_CUSTOM_UUID("F0000502")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_INIT_STATE_SW2(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_INIT_STATE_SW2, \
    .description = "Init State SW2", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {3}, \
    .min_step = (float[]) {1}, \
    .valid_values = { \
    .count = 4, \
    .values = (uint8_t[]) {0, 1, 2, 3}, \
    }, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_INIT_STATE_SW3 HOMEKIT_CUSTOM_UUID("F0000503")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_INIT_STATE_SW3(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_INIT_STATE_SW3, \
    .description = "Init State SW3", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {3}, \
    .min_step = (float[]) {1}, \
    .valid_values = { \
    .count = 4, \
    .values = (uint8_t[]) {0, 1, 2, 3}, \
    }, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_INIT_STATE_SW4 HOMEKIT_CUSTOM_UUID("F0000504")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_INIT_STATE_SW4(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_INIT_STATE_SW4, \
    .description = "Init State SW4", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {3}, \
    .min_step = (float[]) {1}, \
    .valid_values = { \
    .count = 4, \
    .values = (uint8_t[]) {0, 1, 2, 3}, \
    }, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_INIT_STATE_SWDM HOMEKIT_CUSTOM_UUID("F0000505")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_INIT_STATE_SWDM(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_INIT_STATE_SWDM, \
    .description = "Init State DM", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {3}, \
    .min_step = (float[]) {1}, \
    .valid_values = { \
    .count = 4, \
    .values = (uint8_t[]) {0, 1, 2, 3}, \
    }, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

/* Thermostat Initial State
 0. OFF
 1. Heat
 2. Cool
 3. Last State
 */
#define HOMEKIT_CHARACTERISTIC_CUSTOM_INIT_STATE_TH HOMEKIT_CUSTOM_UUID("F0000510")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_INIT_STATE_TH(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_INIT_STATE_TH, \
    .description = "Init State TH", \
    .format = homekit_format_uint8, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {3}, \
    .min_step = (float[]) {1}, \
    .valid_values = { \
    .count = 4, \
    .values = (uint8_t[]) {0, 1, 2, 3}, \
    }, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_IP_ADDR HOMEKIT_CUSTOM_UUID("F0000201")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_IP_ADDR(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_IP_ADDR, \
    .description = "Wifi IP Addr", \
    .format = homekit_format_string, \
    .permissions = homekit_permissions_paired_read, \
    .value = HOMEKIT_STRING_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_WIFI_RESET HOMEKIT_CUSTOM_UUID("F0000202")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_WIFI_RESET(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_WIFI_RESET, \
    .description = "Wifi Reset", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

// ---------------------

#define HOMEKIT_CHARACTERISTIC_CUSTOM_PING_CHECK HOMEKIT_CUSTOM_UUID("F0000202")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_PING_CHECK(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_PING_CHECK, \
    .description = "Ping check", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_PING_IP_SET HOMEKIT_CUSTOM_UUID("F0000203")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_PING_IP_SET(text, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_PING_IP_SET, \
    .description = "Set Ping IP", \
    .format = homekit_format_string, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .value = HOMEKIT_STRING_(text), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_BOOLTEST HOMEKIT_CUSTOM_UUID("A0000001")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_BOOLTEST(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_BOOLTEST, \
    .description = "Bool Test", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_FLOATTEST HOMEKIT_CUSTOM_UUID("A00000003")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_FLOATTEST(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_FLOATTEST, \
    .description = "Float Test", \
    .format = homekit_format_float, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {25}, \
    .min_step = (float[]) {0.1}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_STRINGTEST HOMEKIT_CUSTOM_UUID("A00000004")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_STRINGTEST(revision, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_STRINGTEST, \
    .description = "String Test", \
    .format = homekit_format_string, \
    .permissions = homekit_permissions_paired_read, \
    .value = HOMEKIT_STRING_(revision), \
    ##__VA_ARGS__


#define HOMEKIT_CHARACTERISTIC_CUSTOM_POWDELAY HOMEKIT_CUSTOM_UUID("A0000005")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_POWDELAY(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_POWDELAY, \
    .description = "POW Delay (0 disabled)", \
    .format = homekit_format_uint8, \
    .unit = homekit_unit_seconds, \
    .permissions = homekit_permissions_paired_read \
    | homekit_permissions_paired_write \
    | homekit_permissions_notify, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {60}, \
    .min_step = (float[]) {5}, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

#endif
