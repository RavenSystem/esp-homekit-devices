/*
 * Home Accessory Architect
 *
 * Copyright 2019-2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __HAA_HEADER_H__
#define __HAA_HEADER_H__

// Version
#define FIRMWARE_VERSION                    "2.5.1"
#define FIRMWARE_VERSION_OCTAL              020501      // Matches as example: firmware_revision 2.3.8 = 02.03.10 (octal) = config_number 020310

// Sysparam
#define SYSPARAMSECTOR                      0xF3000
#define SYSPARAMSIZE                        8

#define TOTAL_ACC_SYSPARAM                  "total_ac"
#define HAA_JSON_SYSPARAM                   "haa_conf"
#define HAA_SETUP_MODE_SYSPARAM             "setup"

// Characteristic types (ch_type)
#define CH_TYPE_BOOL                        0
#define CH_TYPE_INT8                        1
#define CH_TYPE_INT32                       2
#define CH_TYPE_FLOAT                       3
#define CH_TYPE_STRING                      4

// Auto-off types (type)
#define TYPE_ON                             0
#define TYPE_LOCK                           1
#define TYPE_SENSOR                         2
#define TYPE_SENSOR_BOOL                    3
#define TYPE_VALVE                          4
#define TYPE_LIGHTBULB                      5
#define TYPE_GARAGE_DOOR                    6
#define TYPE_WINDOW_COVER                   7
#define TYPE_FAN                            8
#define TYPE_TV                             9

// Task Stack Sizes                         configMINIMAL_STACK_SIZE = 256
#define INITIAL_SETUP_TASK_SIZE             (configMINIMAL_STACK_SIZE * 4)
#define LED_TASK_SIZE                       (configMINIMAL_STACK_SIZE * 1)
#define REBOOT_TASK_SIZE                    (configMINIMAL_STACK_SIZE * 1.5)
#define PING_TASK_SIZE                      (configMINIMAL_STACK_SIZE * 2)
#define AUTOSWITCH_TASK_SIZE                (configMINIMAL_STACK_SIZE * 1.5)
#define AUTOOFF_SETTER_TASK_SIZE            (configMINIMAL_STACK_SIZE * 1.5)
#define AUTODIMMER_TASK_SIZE                (configMINIMAL_STACK_SIZE * 1)
#define IR_TX_TASK_SIZE                     (configMINIMAL_STACK_SIZE * 2.5)
#define UART_ACTION_TASK_SIZE               (configMINIMAL_STACK_SIZE * 1.5)
#define HTTP_GET_TASK_SIZE                  (configMINIMAL_STACK_SIZE * 2)
#define DELAYED_SENSOR_START_TASK_SIZE      (configMINIMAL_STACK_SIZE * 1.5)
#define TEMPERATURE_TASK_SIZE               (configMINIMAL_STACK_SIZE * 2)

// Task Priorities
#define INITIAL_SETUP_TASK_PRIORITY         (tskIDLE_PRIORITY + 0)
#define LED_TASK_PRIORITY                   (tskIDLE_PRIORITY + 1)
#define AUTODIMMER_TASK_PRIORITY            (tskIDLE_PRIORITY + 1)
#define PING_TASK_PRIORITY                  (tskIDLE_PRIORITY + 0)
#define IR_TX_TASK_PRIORITY                 (configMAX_PRIORITIES - 1)
#define UART_ACTION_TASK_PRIORITY           (tskIDLE_PRIORITY + 6)
#define HTTP_GET_TASK_PRIORITY              (tskIDLE_PRIORITY + 1)
#define TEMPERATURE_TASK_PRIORITY           (tskIDLE_PRIORITY + 1)

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
#define MDNS_TTL                            "ttl"
#define MDNS_TTL_DEFAULT                    4500
#define CUSTOM_HOSTNAME                     "n"
#define LOG_OUTPUT                          "o"
#define ALLOWED_SETUP_MODE_TIME             "m"
#define STATUS_LED_GPIO                     "l"
#define INVERTED                            "i"
#define BUTTON_FILTER                       "f"
#define PWM_FREQ                            "q"
#define ENABLE_HOMEKIT_SERVER               "h"
#define ALLOW_INSECURE_CONNECTIONS          "u"
#define UART_CONFIG_ARRAY                   "r"
#define UART_CONFIG_ENABLE                  "n"
#define UART_CONFIG_STOPBITS                "b"
#define UART_CONFIG_PARITY                  "p"
#define UART_CONFIG_SPEED                   "s"
#define ACCESSORIES                         "a"
#define BUTTONS_ARRAY                       "b"
#define BUTTONS_ARRAY_1                     "b1"
#define FIXED_BUTTONS_ARRAY_0               "f0"
#define FIXED_BUTTONS_ARRAY_1               "f1"
#define FIXED_BUTTONS_ARRAY_2               "f2"
#define FIXED_BUTTONS_ARRAY_3               "f3"
#define FIXED_BUTTONS_ARRAY_4               "f4"
#define FIXED_BUTTONS_ARRAY_5               "f5"
#define FIXED_BUTTONS_ARRAY_6               "f6"
#define FIXED_BUTTONS_ARRAY_7               "f7"
#define FIXED_BUTTONS_ARRAY_8               "f8"
#define FIXED_BUTTONS_STATUS_ARRAY_0        "g0"
#define FIXED_BUTTONS_STATUS_ARRAY_1        "g1"
#define FIXED_BUTTONS_STATUS_ARRAY_2        "g2"
#define FIXED_BUTTONS_STATUS_ARRAY_3        "g3"
#define PINGS_ARRAY                         "l"
#define PINGS_ARRAY_1                       "l1"
#define FIXED_PINGS_ARRAY_0                 "p0"
#define FIXED_PINGS_ARRAY_1                 "p1"
#define FIXED_PINGS_ARRAY_2                 "p2"
#define FIXED_PINGS_ARRAY_3                 "p3"
#define FIXED_PINGS_ARRAY_4                 "p4"
#define FIXED_PINGS_ARRAY_5                 "p5"
#define FIXED_PINGS_ARRAY_6                 "p6"
#define FIXED_PINGS_ARRAY_7                 "p7"
#define FIXED_PINGS_ARRAY_8                 "p8"
#define FIXED_PINGS_STATUS_ARRAY_0          "q0"
#define FIXED_PINGS_STATUS_ARRAY_1          "q1"
#define FIXED_PINGS_STATUS_ARRAY_2          "q2"
#define FIXED_PINGS_STATUS_ARRAY_3          "q3"
#define PING_HOST                           "h"
#define PING_RESPONSE_TYPE                  "r"
#define PING_RETRIES                        3
#define PING_POLL_PERIOD                    "pt"
#define PING_POLL_PERIOD_DEFAULT            4.9
#define BUTTON_PRESS_TYPE                   "t"
#define PULLUP_RESISTOR                     "p"
#define VALUE                               "v"
#define MANAGE_OTHERS_ACC_ARRAY             "m"
#define ACCESSORY_INDEX                     "g"
#define ACCESSORY_INDEX_KILL_SWITCH         "k"
#define AUTOSWITCH_TIME                     "i"
#define AUTOSWITCH_TIME_1                   "i1"
#define PIN_GPIO                            "g"
#define INITIAL_STATE                       "s"
#define KILL_SWITCH                         "k"
#define EXEC_ACTIONS_ON_BOOT                "xa"

#define VALVE_SYSTEM_TYPE                   "w"
#define VALVE_SYSTEM_TYPE_DEFAULT           0
#define VALVE_MAX_DURATION                  "d"
#define VALVE_MAX_DURATION_DEFAULT          3600

#define THERMOSTAT_TYPE                     "w"
#define TH_TYPE                             ch_group->num[5]
#define THERMOSTAT_TYPE_HEATER              1
#define THERMOSTAT_TYPE_COOLER              2
#define THERMOSTAT_TYPE_HEATERCOOLER        3
#define THERMOSTAT_MIN_TEMP                 "m"
#define TH_MIN_TEMP                         ch_group->num[7]
#define THERMOSTAT_DEFAULT_MIN_TEMP         10
#define THERMOSTAT_MAX_TEMP                 "x"
#define TH_MAX_TEMP                         ch_group->num[8]
#define THERMOSTAT_DEFAULT_MAX_TEMP         38
#define THERMOSTAT_DEADBAND                 "d"
#define TH_DEADBAND                         ch_group->num[6]
#define THERMOSTAT_DEADBAND_FORCE_IDLE      "df"
#define TH_DEADBAND_FORCE_IDLE              ch_group->num[10]
#define THERMOSTAT_DEADBAND_SOFT_ON         "ds"
#define TH_DEADBAND_SOFT_ON                 ch_group->num[11]
#define THERMOSTAT_MODE_INT                 ch_group->ch4->value.int_value
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
#define THERMOSTAT_ACTION_TEMP_UP           6
#define THERMOSTAT_ACTION_TEMP_DOWN         7
#define THERMOSTAT_ACTION_HEATER_FORCE_IDLE 8
#define THERMOSTAT_ACTION_COOLER_FORCE_IDLE 9
#define THERMOSTAT_ACTION_HEATER_SOFT_ON    10
#define THERMOSTAT_ACTION_COOLER_SOFT_ON    11
#define THERMOSTAT_TEMP_UP                  0
#define THERMOSTAT_TEMP_DOWN                1
#define TH_SENSOR_MAX_ALLOWED_ERRORS        3
#define TH_HEATER_TARGET_TEMP_FLOAT         ch_group->ch6->value.float_value
#define TH_COOLER_TARGET_TEMP_FLOAT         ch_group->ch7->value.float_value
#define TH_UPDATE_DELAY_MS                  4000

#define TEMPERATURE_SENSOR_GPIO             "g"
#define TH_SENSOR_GPIO                      ch_group->num[0]
#define TEMPERATURE_SENSOR_TYPE             "n"
#define TH_SENSOR_TYPE                      ch_group->num[1]
#define TEMPERATURE_SENSOR_POLL_PERIOD      "j"
#define TH_SENSOR_POLL_PERIOD               ch_group->num[2]
#define TH_SENSOR_POLL_PERIOD_DEFAULT       30
#define TH_SENSOR_POLL_PERIOD_MIN           0.1f
#define TEMPERATURE_OFFSET                  "z"
#define TH_SENSOR_TEMP_OFFSET               ch_group->num[3]
#define HUMIDITY_OFFSET                     "h"
#define TH_SENSOR_HUM_OFFSET                ch_group->num[4]
#define SENSOR_TEMPERATURE_FLOAT            ch_group->ch0->value.float_value
#define TH_SENSOR_ERROR_COUNT               ch_group->num[9]

#define LIGHTBULB_PWM_GPIO_R                "r"
#define LIGHTBULB_PWM_GPIO_G                "g"
#define LIGHTBULB_PWM_GPIO_B                "v"
#define LIGHTBULB_PWM_GPIO_W                "w"
#define LIGHTBULB_PWM_GPIO_CW               "cw"
#define LIGHTBULB_PWM_GPIO_WW               "ww"
#define LIGHTBULB_FACTOR_R                  "fr"
#define LIGHTBULB_FACTOR_G                  "fg"
#define LIGHTBULB_FACTOR_B                  "fv"
#define LIGHTBULB_FACTOR_W                  "fw"
#define LIGHTBULB_FACTOR_CW                 "fcw"
#define LIGHTBULB_FACTOR_WW                 "fww"
#define RGBW_PERIOD                         10
#define RGBW_STEP                           "st"
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

#define CW_RED                              52200
#define CW_GREEN                            56000
#define CW_BLUE                             PWM_SCALE
#define WW_RED                              PWM_SCALE
#define WW_GREEN                            40400
#define WW_BLUE                             15600

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
#define GARAGE_DOOR_CURRENT_TIME            ch_group->num[0]
#define GARAGE_DOOR_WORKING_TIME            ch_group->num[1]
#define GARAGE_DOOR_TIME_MARGIN             ch_group->num[2]
#define GARAGE_DOOR_CLOSE_TIME_FACTOR       ch_group->num[3]
#define GARAGE_DOOR_HAS_F2                  ch_group->num[4]
#define GARAGE_DOOR_HAS_F3                  ch_group->num[5]
#define GARAGE_DOOR_HAS_F4                  ch_group->num[6]
#define GARAGE_DOOR_HAS_F5                  ch_group->num[7]

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
#define WINDOW_COVER_POLL_PERIOD_MS         333
#define WINDOW_COVER_MARGIN_SYNC_SET        "m"
#define WINDOW_COVER_MARGIN_SYNC_DEFAULT    15
#define WINDOW_COVER_STEP_TIME(x)           ((100.0 / (x)) * (WINDOW_COVER_POLL_PERIOD_MS / 1000.0))
#define WINDOW_COVER_STEP_TIME_UP           ch_group->num[0]
#define WINDOW_COVER_STEP_TIME_DOWN         ch_group->num[1]
#define WINDOW_COVER_MOTOR_POSITION         ch_group->num[2]
#define WINDOW_COVER_HOMEKIT_POSITION       ch_group->num[3]
#define WINDOW_COVER_CORRECTION             ch_group->num[4]
#define WINDOW_COVER_MARGIN_SYNC            ch_group->num[5]
#define WINDOW_COVER_CH_CURRENT_POSITION    ch_group->ch0
#define WINDOW_COVER_CH_TARGET_POSITION     ch_group->ch1
#define WINDOW_COVER_CH_STATE               ch_group->ch2
#define WINDOW_COVER_CH_OBSTRUCTION         ch_group->ch3

#define TV_INPUTS_ARRAY                     "i"
#define TV_INPUT_NAME                       "n"

#define FAN_SPEED_STEPS                     "e"

#define PM_SENSOR_TYPE                      "n"
#define PM_POLL_PERIOD                      "j"
#define PM_POLL_PERIOD_DEFAULT              1
#define PM_VOLTAGE_FACTOR                   "vf"
#define PM_VOLTAGE_OFFSET                   "vo"
#define PM_CURRENT_FACTOR                   "cf"
#define PM_CURRENT_OFFSET                   "co"

#define MAX_ACTIONS                         32      // from 0 to (MAX_ACTIONS - 1)
#define MAX_WILDCARD_ACTIONS                3       // from 0 to (MAX_WILDCARD_ACTIONS - 1)
#define WILDCARD_ACTIONS_ARRAY_HEADER       "y"
#define NO_LAST_WILDCARD_ACTION             (-1000)
#define WILDCARD_ACTIONS                    "0"
#define WILDCARD_ACTION_REPEAT              "r"
#define COPY_ACTIONS                        "a"
#define DIGITAL_OUTPUTS_ARRAY               "r"
#define SYSTEM_ACTIONS_ARRAY                "s"
#define HTTP_ACTIONS_ARRAY                  "h"
#define IR_ACTIONS_ARRAY                    "i"
#define IR_ACTION_PROTOCOL                  "p"
#define IR_ACTION_PROTOCOL_LEN_2BITS        14
#define IR_ACTION_PROTOCOL_LEN_4BITS        22
#define IR_ACTION_PROTOCOL_LEN_6BITS        30
#define IR_CODE_HEADER_MARK_POS             0
#define IR_CODE_HEADER_SPACE_POS            1
#define IR_CODE_BIT0_MARK_POS               2
#define IR_CODE_BIT0_SPACE_POS              3
#define IR_CODE_BIT1_MARK_POS               4
#define IR_CODE_BIT1_SPACE_POS              5
#define IR_CODE_BIT2_MARK_POS               6
#define IR_CODE_BIT2_SPACE_POS              7
#define IR_CODE_BIT3_MARK_POS               8
#define IR_CODE_BIT3_SPACE_POS              9
#define IR_CODE_BIT4_MARK_POS               10
#define IR_CODE_BIT4_SPACE_POS              11
#define IR_CODE_BIT5_MARK_POS               12
#define IR_CODE_BIT5_SPACE_POS              13
#define IR_CODE_FOOTER_MARK_POS_2BITS       6
#define IR_CODE_FOOTER_MARK_POS_4BITS       10
#define IR_CODE_FOOTER_MARK_POS_6BITS       14
#define IR_ACTION_PROTOCOL_CODE             "c"
#define IR_ACTION_RAW_CODE                  "w"
#define IR_ACTION_TX_GPIO                   "t"
#define IR_ACTION_TX_GPIO_INVERTED          "j"
#define IR_ACTION_FREQ                      "x"
#define IR_ACTION_REPEATS                   "r"
#define IR_ACTION_REPEATS_PAUSE             "d"
#define SYSTEM_ACTION                       "a"
#define HTTP_ACTION_HOST                    "h"
#define HTTP_ACTION_PORT                    "p"
#define HTTP_ACTION_URL                     "u"
#define HTTP_ACTION_METHOD                  "m"
#define HTTP_ACTION_CONTENT                 "c"
#define HTTP_ACTION_WILDCARD_VALUE          "#HAA@"
#define SYSTEM_ACTION_REBOOT                0
#define SYSTEM_ACTION_SETUP_MODE            1
#define SYSTEM_ACTION_OTA_UPDATE            2
#define UART_ACTIONS_ARRAY                  "u"
#define UART_ACTION_UART                    "n"
#define UART_ACTION_PAUSE                   "d"

#define ACCESSORY_TYPE                      "t"
#define ACC_TYPE_SWITCH                     1
#define ACC_TYPE_OUTLET                     2
#define ACC_TYPE_BUTTON                     3
#define ACC_TYPE_LOCK                       4
#define ACC_TYPE_DOUBLE_LOCK                44
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
#define ACC_TYPE_THERMOSTAT_WITH_HUM        25
#define ACC_TYPE_LIGHTBULB                  30
#define ACC_TYPE_GARAGE_DOOR                40
#define ACC_TYPE_WINDOW_COVER               45
#define ACC_TYPE_LIGHT_SENSOR               50
#define ACC_TYPE_TV                         60
#define ACC_TYPE_FAN                        65
#define ACC_POWER_MONITOR_INIT              75
#define ACC_POWER_MONITOR_END               82

#define ACC_CREATION_DELAY                  "cd"
#define EXIT_EMERGENCY_SETUP_MODE_TIME      2200
#define SETUP_MODE_ACTIVATE_COUNT           "z"
#define SETUP_MODE_DEFAULT_ACTIVATE_COUNT   8
#define SETUP_MODE_TOGGLE_TIME_MS           1050

#define WIFI_STATUS_DISCONNECTED            0
#define WIFI_STATUS_CONNECTING              1
#define WIFI_STATUS_PRECONNECTED            2
#define WIFI_STATUS_CONNECTED               3
#define WIFI_WATCHDOG_POLL_PERIOD_MS        1000
#define WIFI_RECONNECTION_POLL_PERIOD_MS    7000
#define WIFI_PING_ERRORS                    "w"

#define ACCESSORIES_WITHOUT_BRIDGE          4   // Max number of accessories before using a bridge

#define MS_TO_TICK(x)                       ((x) / portTICK_PERIOD_MS)

#define MIN(x, y)                           (((x) < (y)) ? (x) : (y))
#define MAX(x, y)                           (((x) > (y)) ? (x) : (y))

#define KELVIN_TO_CELSIUS(x)                ((x) - 273.15)

#define DEBUG(message, ...)                 printf("%s: " message "\n", __func__, ##__VA_ARGS__)
#define INFO(message, ...)                  printf(message "\n", ##__VA_ARGS__)
#define ERROR(message, ...)                 printf("! " message "\n", ##__VA_ARGS__)

#define FREEHEAP()                          printf("Free Heap: %d\n", xPortGetFreeHeapSize())

#endif // __HAA_HEADER_H__
