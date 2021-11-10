/*
 * Home Accessory Architect
 *
 * Copyright 2019-2021 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __HAA_HEADER_H__
#define __HAA_HEADER_H__

#include "../common/common_headers.h"

// Version
#define FIRMWARE_VERSION                    "9.1.0"

// Characteristic types (ch_type)
#define CH_TYPE_BOOL                        (0)
#define CH_TYPE_INT8                        (1)
#define CH_TYPE_INT                         (2)
#define CH_TYPE_FLOAT                       (3)
#define CH_TYPE_STRING                      (4)

#define FLOAT_FACTOR_SAVE_AS_INT            (100000.00000f)

// Task Stack Sizes
#define GLOBAL_TASK_SIZE                    (612)

#define INITIAL_SETUP_TASK_SIZE             (1600)
#define NTP_TASK_SIZE                       (512)
#define PING_TASK_SIZE                      GLOBAL_TASK_SIZE
#define AUTODIMMER_TASK_SIZE                GLOBAL_TASK_SIZE
#define IR_TX_TASK_SIZE                     (456)
#define UART_ACTION_TASK_SIZE               (384)
#define NETWORK_ACTION_TASK_SIZE            (512)
#define DELAYED_SENSOR_START_TASK_SIZE      GLOBAL_TASK_SIZE
#define TEMPERATURE_TASK_SIZE               GLOBAL_TASK_SIZE
#define PROCESS_TH_TASK_SIZE                GLOBAL_TASK_SIZE
#define PROCESS_HUMIDIF_TASK_SIZE           GLOBAL_TASK_SIZE
#define SET_ZONES_TASK_SIZE                 GLOBAL_TASK_SIZE
#define LIGHTBULB_TASK_SIZE                 GLOBAL_TASK_SIZE
#define POWER_MONITOR_TASK_SIZE             GLOBAL_TASK_SIZE
#define LIGHT_SENSOR_TASK_SIZE              GLOBAL_TASK_SIZE
#define WIFI_PING_GW_TASK_SIZE              (384)
#define WIFI_RECONNECTION_TASK_SIZE         GLOBAL_TASK_SIZE
#define IR_CAPTURE_TASK_SIZE                (768)
#define REBOOT_TASK_SIZE                    (384)

// Task Priorities
#define INITIAL_SETUP_TASK_PRIORITY         (tskIDLE_PRIORITY + 1)
#define NTP_TASK_PRIORITY                   (tskIDLE_PRIORITY + 1)
#define PING_TASK_PRIORITY                  (tskIDLE_PRIORITY + 2)
#define AUTODIMMER_TASK_PRIORITY            (tskIDLE_PRIORITY + 1)
#define IR_TX_TASK_PRIORITY                 (tskIDLE_PRIORITY + 1)
#define UART_ACTION_TASK_PRIORITY           (tskIDLE_PRIORITY + 1)
#define NETWORK_ACTION_TASK_PRIORITY        (tskIDLE_PRIORITY + 1)
#define DELAYED_SENSOR_START_TASK_PRIORITY  (tskIDLE_PRIORITY + 1)
#define TEMPERATURE_TASK_PRIORITY           (tskIDLE_PRIORITY + 1)
#define PROCESS_TH_TASK_PRIORITY            (tskIDLE_PRIORITY + 1)
#define PROCESS_HUMIDIF_TASK_PRIORITY       (tskIDLE_PRIORITY + 1)
#define SET_ZONES_TASK_PRIORITY             (tskIDLE_PRIORITY + 1)
#define LIGHTBULB_TASK_PRIORITY             (tskIDLE_PRIORITY + 1)
#define POWER_MONITOR_TASK_PRIORITY         (tskIDLE_PRIORITY + 1)
#define LIGHT_SENSOR_TASK_PRIORITY          (tskIDLE_PRIORITY + 1)
#define WIFI_PING_GW_TASK_PRIORITY          (tskIDLE_PRIORITY + 1)
#define WIFI_RECONNECTION_TASK_PRIORITY     (tskIDLE_PRIORITY + 3)
#define IR_CAPTURE_TASK_PRIORITY            (tskIDLE_PRIORITY + 8)
#define REBOOT_TASK_PRIORITY                (tskIDLE_PRIORITY + 3)

// Button Events
#define SINGLEPRESS_EVENT                   (0)
#define DOUBLEPRESS_EVENT                   (1)
#define LONGPRESS_EVENT                     (2)

// Initial states
#define INIT_STATE_FIXED_INPUT              (4)
#define INIT_STATE_LAST                     (5)
#define INIT_STATE_INV_LAST                 (6)
#define INIT_STATE_LAST_STR                 "{\"s\":5}"

// JSON
#define GENERAL_CONFIG                      "c"
#define MDNS_TTL                            "ttl"
#define MDNS_TTL_DEFAULT                    (4500)
#define CUSTOM_HOSTNAME                     "n"
#define CUSTOM_NTP_HOST                     "ntp"
#define TIMEZONE                            "tz"
#define LOG_OUTPUT                          "o"
#define LOG_OUTPUT_TARGET                   "ot"
#define ALLOWED_SETUP_MODE_TIME             "m"
#define STATUS_LED_GPIO                     "l"
#define INVERTED                            "i"
#define BUTTON_FILTER                       "f"
#define PWM_FREQ                            "q"
#define ENABLE_HOMEKIT                      "h"
#define HOMEKIT_SERVER_MAX_CLIENTS          "h"
#define HOMEKIT_SERVER_MAX_CLIENTS_DEFAULT  (20)
#define HOMEKIT_SERVER_MAX_CLIENTS_MAX      (20)
#define ALLOW_INSECURE_CONNECTIONS          "u"
#define UART_CONFIG_ARRAY                   "r"
#define UART_CONFIG_ENABLE                  "n"
#define UART_CONFIG_STOPBITS                "b"
#define UART_CONFIG_PARITY                  "p"
#define UART_CONFIG_SPEED                   "s"
#define ACCESSORIES_ARRAY                   "a"
#define EXTRA_SERVICES_ARRAY                "es"
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
#define PING_IGNORE_LAST_RESPONSE           "i"
#define PING_DISABLE_WITHOUT_WIFI           "d"
#define PING_RETRIES                        (4)
#define PING_POLL_PERIOD                    "pt"
#define PING_POLL_PERIOD_DEFAULT            (5.f)
#define BUTTON_PRESS_TYPE                   "t"
#define BUTTON_MODE                         "m"
#define PULLUP_RESISTOR                     "p"
#define VALUE                               "v"
#define MANAGE_OTHERS_ACC_ARRAY             "m"
#define ACCESSORY_INDEX                     "g"
#define AUTOSWITCH_TIME                     "i"
#define PIN_GPIO                            "g"
#define INITIAL_STATE                       "s"
#define EXEC_ACTIONS_ON_BOOT                "xa"
#define KILLSWITCH                          "ks"
#define KILLSWITCH_DEFAULT                  (3)

#define TIMETABLE_ACTION_ARRAY              "tt"
#define ALL_MONS                            13
#define ALL_MDAYS                           0
#define ALL_HOURS                           24
#define ALL_MINS                            60
#define ALL_WDAYS                           7

#define VALVE_SYSTEM_TYPE                   "w"
#define VALVE_SYSTEM_TYPE_DEFAULT           (0)
#define VALVE_MAX_DURATION                  "d"
#define VALVE_MAX_DURATION_DEFAULT          (3600)

#define DOORBELL_LAST_STATE                 ch_group->num[0]

#define THERMOSTAT_TYPE                     "w"
#define TH_TYPE                             ch_group->num[5]
#define THERMOSTAT_TYPE_HEATER              (1)
#define THERMOSTAT_TYPE_COOLER              (2)
#define THERMOSTAT_TYPE_HEATERCOOLER        (3)
#define THERMOSTAT_TYPE_HEATERCOOLER_NOAUTO (4)
#define THERMOSTAT_MIN_TEMP                 "m"
#define TH_HEATER_MIN_TEMP                  *ch_group->ch[5]->min_value
#define TH_COOLER_MIN_TEMP                  *ch_group->ch[6]->min_value
#define THERMOSTAT_DEFAULT_MIN_TEMP         (10)
#define THERMOSTAT_MAX_TEMP                 "x"
#define TH_HEATER_MAX_TEMP                  *ch_group->ch[5]->max_value
#define TH_COOLER_MAX_TEMP                  *ch_group->ch[6]->max_value
#define THERMOSTAT_DEFAULT_MAX_TEMP         (38)
#define THERMOSTAT_DEADBAND                 "d"
#define TH_DEADBAND                         ch_group->num[6]
#define THERMOSTAT_DEADBAND_FORCE_IDLE      "df"
#define TH_DEADBAND_FORCE_IDLE              ch_group->num[7]
#define THERMOSTAT_DEADBAND_SOFT_ON         "ds"
#define TH_DEADBAND_SOFT_ON                 ch_group->num[8]
#define THERMOSTAT_UPDATE_DELAY             "dl"
#define THERMOSTAT_UPDATE_DELAY_MIN         (0.15f)
#define THERMOSTAT_UPDATE_DELAY_DEFAULT     (3.0f)
#define THERMOSTAT_TARGET_TEMP_STEP         "st"
#define THERMOSTAT_IAIRZONING_CONTROLLER    "ia"
#define TH_IAIRZONING_CONTROLLER            ch_group->num[9]
#define TH_IAIRZONING_GATE_CURRENT_STATE    ch_group->last_wildcard_action[5]
#define TH_IAIRZONING_GATE_CLOSE            (0)
#define TH_IAIRZONING_GATE_OPEN             (1)
#define THERMOSTAT_CURRENT_ACTION           ch_group->last_wildcard_action[4]
#define THERMOSTAT_MODE_OFF                 (0)
#define THERMOSTAT_MODE_IDLE                (1)
#define THERMOSTAT_MODE_HEATER              (2)
#define THERMOSTAT_MODE_COOLER              (3)
#define THERMOSTAT_TARGET_MODE_AUTO         (0)
#define THERMOSTAT_TARGET_MODE_HEATER       (1)
#define THERMOSTAT_TARGET_MODE_COOLER       (2)
#define THERMOSTAT_ACTION_TOTAL_OFF         (0)
#define THERMOSTAT_ACTION_HEATER_IDLE       (1)
#define THERMOSTAT_ACTION_COOLER_IDLE       (2)
#define THERMOSTAT_ACTION_HEATER_ON         (3)
#define THERMOSTAT_ACTION_COOLER_ON         (4)
#define THERMOSTAT_ACTION_SENSOR_ERROR      (5)
#define THERMOSTAT_ACTION_TEMP_UP           (6)
#define THERMOSTAT_ACTION_TEMP_DOWN         (7)
#define THERMOSTAT_ACTION_HEATER_FORCE_IDLE (8)
#define THERMOSTAT_ACTION_COOLER_FORCE_IDLE (9)
#define THERMOSTAT_ACTION_HEATER_SOFT_ON    (10)
#define THERMOSTAT_ACTION_COOLER_SOFT_ON    (11)
#define THERMOSTAT_ACTION_GATE_CLOSE        (12)
#define THERMOSTAT_ACTION_GATE_OPEN         (13)
#define THERMOSTAT_TEMP_UP                  (0)
#define THERMOSTAT_TEMP_DOWN                (1)
#define TH_ACTIVE_INT                       ch_group->ch[2]->value.int_value
#define TH_MODE_INT                         ch_group->ch[3]->value.int_value
#define TH_TARGET_MODE_INT                  ch_group->ch[4]->value.int_value
#define TH_HEATER_TARGET_TEMP_FLOAT         ch_group->ch[5]->value.float_value
#define TH_COOLER_TARGET_TEMP_FLOAT         ch_group->ch[6]->value.float_value

#define IAIRZONING_LAST_ACTION              iairzoning_group->last_wildcard_action[0]
#define IAIRZONING_MAIN_MODE                iairzoning_group->last_wildcard_action[1]
#define IAIRZONING_DELAY_ACTION_SET         "d"
#define IAIRZONING_DELAY_ACTION_DEFAULT     (0.5f)
#define IAIRZONING_DELAY_ACTION_NUM         num[0]
#define IAIRZONING_DELAY_ACTION             iairzoning_group->IAIRZONING_DELAY_ACTION_NUM
#define IAIRZONING_DELAY_ACTION_CH_GROUP    ch_group->IAIRZONING_DELAY_ACTION_NUM

#define TEMPERATURE_SENSOR_GPIO             "g"
#define TH_SENSOR_GPIO                      ch_group->num[0]
#define TEMPERATURE_SENSOR_TYPE             "n"
#define TH_SENSOR_TYPE                      ch_group->num[1]
#define TEMPERATURE_SENSOR_INDEX            "u"
#define TH_SENSOR_INDEX                     ch_group->num[3]
#define TEMPERATURE_SENSOR_POLL_PERIOD      "j"
#define TH_SENSOR_POLL_PERIOD_DEFAULT       (30)
#define TH_SENSOR_POLL_PERIOD_MIN           (0.1f)
#define TEMPERATURE_OFFSET                  "z"
#define TH_SENSOR_TEMP_OFFSET               ch_group->num[2]
#define HUMIDITY_OFFSET                     "k"
#define TH_SENSOR_HUM_OFFSET                ch_group->num[3]
#define SENSOR_TEMPERATURE_FLOAT            ch_group->ch[0]->value.float_value
#define SENSOR_HUMIDITY_FLOAT               ch_group->ch[1]->value.float_value
#define TH_SENSOR_ERROR_COUNT               ch_group->num[4]
#define TH_SENSOR_MAX_ALLOWED_ERRORS        (3)

#define HUMIDIF_TYPE                        "w"
#define HM_TYPE                             ch_group->num[5]
#define HUMIDIF_TYPE_HUM                    (1)
#define HUMIDIF_TYPE_DEHUM                  (2)
#define HUMIDIF_TYPE_HUMDEHUM               (3)
#define HUMIDIF_TYPE_HUMDEHUM_NOAUTO        (4)
#define HUMIDIF_DEADBAND                    "d"
#define HM_DEADBAND                         ch_group->num[6]
#define HUMIDIF_DEADBAND_FORCE_IDLE         "df"
#define HM_DEADBAND_FORCE_IDLE              ch_group->num[7]
#define HUMIDIF_DEADBAND_SOFT_ON            "ds"
#define HM_DEADBAND_SOFT_ON                 ch_group->num[8]
#define HUMIDIF_CURRENT_ACTION              ch_group->last_wildcard_action[4]
#define HUMIDIF_MODE_OFF                    (0)
#define HUMIDIF_MODE_IDLE                   (1)
#define HUMIDIF_MODE_HUM                    (2)
#define HUMIDIF_MODE_DEHUM                  (3)
#define HUMIDIF_TARGET_MODE_AUTO            (0)
#define HUMIDIF_TARGET_MODE_HUM             (1)
#define HUMIDIF_TARGET_MODE_DEHUM           (2)
#define HUMIDIF_ACTION_TOTAL_OFF            (0)
#define HUMIDIF_ACTION_HUM_IDLE             (1)
#define HUMIDIF_ACTION_DEHUM_IDLE           (2)
#define HUMIDIF_ACTION_HUM_ON               (3)
#define HUMIDIF_ACTION_DEHUM_ON             (4)
#define HUMIDIF_ACTION_UP                   (6)
#define HUMIDIF_ACTION_DOWN                 (7)
#define HUMIDIF_ACTION_HUM_FORCE_IDLE       (8)
#define HUMIDIF_ACTION_DEHUM_FORCE_IDLE     (9)
#define HUMIDIF_ACTION_HUM_SOFT_ON          (10)
#define HUMIDIF_ACTION_DEHUM_SOFT_ON        (11)
#define HUMIDIF_UP                          (0)
#define HUMIDIF_DOWN                        (1)
#define HM_ACTIVE_INT                       ch_group->ch[2]->value.int_value
#define HM_MODE_INT                         ch_group->ch[3]->value.int_value
#define HM_TARGET_MODE_INT                  ch_group->ch[4]->value.int_value
#define HM_HUM_TARGET_FLOAT                 ch_group->ch[5]->value.float_value
#define HM_DEHUM_TARGET_FLOAT               ch_group->ch[6]->value.float_value

#define LIGHTBULB_TYPE_SET                  "ty"
#define LIGHTBULB_TYPE                      lightbulb_group->type
#define LIGHTBULB_TYPE_VIRTUAL              (0)
#define LIGHTBULB_TYPE_PWM                  (1)
#define LIGHTBULB_TYPE_PWM_CWWW             (2)
#define LIGHTBULB_TYPE_MY92X1               (3)
#define LIGHTBULB_TYPE_SM16716              (7)
#define LIGHTBULB_TYPE_NRZ                  (8)
#define LIGHTBULB_NRZ_TIMES_ARRAY_SET       "nrz"
#define LIGHTBULB_CHANNELS_SET              "n"
#define LIGHTBULB_CHANNELS                  lightbulb_group->channels
#define LIGHTBULB_INITITAL_STATE_ARRAY_SET  "it"
#define LIGHTBULB_GPIO_ARRAY_SET            "g"
#define LIGHTBULB_RGB_ARRAY_SET             "rgb"
#define LIGHTBULB_CMY_ARRAY_SET             "cmy"
#define LIGHTBULB_MAX_POWER_SET             "mp"
#define LIGHTBULB_MAX_POWER                 ch_group->num[0]
#define LIGHTBULB_CURVE_FACTOR_SET          "cf"
#define LIGHTBULB_CURVE_FACTOR              ch_group->num[1]
#define PWM_DITHER_SET                      "di"
#define LIGHTBULB_COLOR_MAP_SET             "cm"
#define LIGHTBULB_RANGE_START               lightbulb_group->range_start
#define LIGHTBULB_RANGE_END                 lightbulb_group->range_end
#define LIGHTBULB_PWM_DITHER                lightbulb_group->pwm_dither
#define LIGHTBULB_STEP_VALUE                lightbulb_group->step_value
#define LIGHTBULB_FLUX_ARRAY_SET            "fa"
#define LIGHTBULB_COORDINATE_ARRAY_SET      "ca"
#define LIGHTBULB_WHITE_POINT_SET           "wp"
#define RGBW_PERIOD                         (20)
#define RGBW_STEP_SET                       "st"
#define RGBW_STEP_DEFAULT                   (2048)
#define PWM_SCALE                           (UINT16_MAX)
#define COLOR_TEMP_MIN                      (71)
#define COLOR_TEMP_MAX                      (400)
#define LIGHTBULB_BRIGHTNESS_UP             (0)
#define LIGHTBULB_BRIGHTNESS_DOWN           (1)
#define AUTODIMMER_DELAY                    (400)
#define AUTODIMMER_TASK_DELAY_SET           "d"
#define AUTODIMMER_TASK_DELAY_DEFAULT       (1000)
#define AUTODIMMER_TASK_STEP_SET            "e"
#define AUTODIMMER_TASK_STEP_DEFAULT        (20)
#define LIGHTBULB_SET_DELAY_MS              (200)
#define R                                   lightbulb_group->r
#define G                                   lightbulb_group->g
#define B                                   lightbulb_group->b
#define CW                                  lightbulb_group->cw
#define WW                                  lightbulb_group->ww
#define WP                                  lightbulb_group->wp
#define myCMY                               lightbulb_group->cmy
#define myRGB                               lightbulb_group->rgb
#define LIGHTBULB_AUTODIMMER_TIMER          ch_group->timer
#define LIGHTBULB_SET_DELAY_TIMER           lightbulb_group->timer

#define GD_CURRENT_DOOR_STATE               ch_group->ch[0]
#define GD_TARGET_DOOR_STATE                ch_group->ch[1]
#define GD_OBSTRUCTION_DETECTED             ch_group->ch[2]
#define GD_CURRENT_DOOR_STATE_INT           ch_group->ch[0]->value.int_value
#define GD_TARGET_DOOR_STATE_INT            ch_group->ch[1]->value.int_value
#define GD_OBSTRUCTION_DETECTED_BOOL        ch_group->ch[2]->value.bool_value
#define GARAGE_DOOR_OPENED                  (0)
#define GARAGE_DOOR_CLOSED                  (1)
#define GARAGE_DOOR_OPENING                 (2)
#define GARAGE_DOOR_CLOSING                 (3)
#define GARAGE_DOOR_STOPPED                 (4)
#define GARAGE_DOOR_TIME_OPEN_SET           "d"
#define GARAGE_DOOR_TIME_OPEN_DEFAULT       (30)
#define GARAGE_DOOR_TIME_CLOSE_SET          "c"
#define GARAGE_DOOR_TIME_MARGIN_SET         "e"
#define GARAGE_DOOR_TIME_MARGIN_DEFAULT     (0)
#define GARAGE_DOOR_CURRENT_TIME            ch_group->num[0]
#define GARAGE_DOOR_WORKING_TIME            ch_group->num[1]
#define GARAGE_DOOR_TIME_MARGIN             ch_group->num[2]
#define GARAGE_DOOR_CLOSE_TIME_FACTOR       ch_group->num[3]
#define GARAGE_DOOR_HAS_F2                  ch_group->num[4]
#define GARAGE_DOOR_HAS_F3                  ch_group->num[5]
#define GARAGE_DOOR_HAS_F4                  ch_group->num[6]
#define GARAGE_DOOR_HAS_F5                  ch_group->num[7]
#define GARAGE_DOOR_VIRTUAL_STOP            ch_group->num[8]

#define VIRTUAL_STOP_SET                    "vs"
#define VIRTUAL_STOP_DEFAULT                (0)

#define WINDOW_COVER_CLOSING                (0)
#define WINDOW_COVER_OPENING                (1)
#define WINDOW_COVER_STOP                   (2)
#define WINDOW_COVER_CLOSING_FROM_MOVING    (3)
#define WINDOW_COVER_OPENING_FROM_MOVING    (4)
#define WINDOW_COVER_STOP_FROM_CLOSING      (5)
#define WINDOW_COVER_STOP_FROM_OPENING      (6)
#define WINDOW_COVER_OBSTRUCTION            (7)
#define WINDOW_COVER_TYPE_SET               "w"
#define WINDOW_COVER_TYPE_DEFAULT           (0)
#define WINDOW_COVER_TIME_OPEN_SET          "o"
#define WINDOW_COVER_TIME_CLOSE_SET         "c"
#define WINDOW_COVER_TIME_DEFAULT           (15)
#define WINDOW_COVER_CORRECTION_SET         "f"
#define WINDOW_COVER_CORRECTION_DEFAULT     (0)
#define WINDOW_COVER_POLL_PERIOD_MS         (250)
#define WINDOW_COVER_MARGIN_SYNC_SET        "m"
#define WINDOW_COVER_MARGIN_SYNC_DEFAULT    (15)
#define WINDOW_COVER_TIME_OPEN              ch_group->num[0]
#define WINDOW_COVER_TIME_CLOSE             ch_group->num[1]
#define WINDOW_COVER_MOTOR_POSITION         ch_group->num[2]
#define WINDOW_COVER_HOMEKIT_POSITION       ch_group->num[3]
#define WINDOW_COVER_CORRECTION             ch_group->num[4]
#define WINDOW_COVER_MARGIN_SYNC            ch_group->num[5]
#define WINDOW_COVER_LAST_TIME              ch_group->num[6]
#define WINDOW_COVER_STOP_ENABLE            ch_group->num[7]
#define WINDOW_COVER_STOP_ENABLE_DELAY_MS   (80)
#define WINDOW_COVER_VIRTUAL_STOP           ch_group->num[8]
#define WINDOW_COVER_CH_CURRENT_POSITION    ch_group->ch[0]
#define WINDOW_COVER_CH_TARGET_POSITION     ch_group->ch[1]
#define WINDOW_COVER_CH_STATE               ch_group->ch[2]
#define WINDOW_COVER_CH_OBSTRUCTION         ch_group->ch[3]

#define TV_INPUTS_ARRAY                     "i"
#define TV_INPUT_NAME                       "n"

#define FAN_SPEED_STEPS                     "e"

#define PM_SENSOR_TYPE_SET                  "n"
#define PM_SENSOR_TYPE_DEFAULT              (0)
#define PM_SENSOR_TYPE                      ch_group->num[0]
#define PM_LAST_SAVED_CONSUPTION            ch_group->num[10]
#define PM_SENSOR_DATA_ARRAY_SET            "dt"
#define PM_SENSOR_DATA_DEFAULT              (-1)
#define PM_SENSOR_HLW_GPIO                  ch_group->num[1]
#define PM_SENSOR_HLW_GPIO_CF               ch_group->num[2]
#define PM_SENSOR_ADE_BUS                   ch_group->num[1]
#define PM_SENSOR_ADE_ADDR                  ch_group->num[2]
#define PM_POLL_PERIOD_DEFAULT              (5.f)
#define PM_POLL_PERIOD                      ch_group->num[9]
#define PM_VOLTAGE_FACTOR_SET               "vf"
#define PM_VOLTAGE_FACTOR_DEFAULT           (1)
#define PM_VOLTAGE_FACTOR                   ch_group->num[3]
#define PM_VOLTAGE_OFFSET_SET               "vo"
#define PM_VOLTAGE_OFFSET_DEFAULT           (0)
#define PM_VOLTAGE_OFFSET                   ch_group->num[4]
#define PM_CURRENT_FACTOR_SET               "cf"
#define PM_CURRENT_FACTOR_DEFAULT           (1)
#define PM_CURRENT_FACTOR                   ch_group->num[5]
#define PM_CURRENT_OFFSET_SET               "co"
#define PM_CURRENT_OFFSET_DEFAULT           (0)
#define PM_CURRENT_OFFSET                   ch_group->num[6]
#define PM_POWER_FACTOR_SET                 "pf"
#define PM_POWER_FACTOR_DEFAULT             (1)
#define PM_POWER_FACTOR                     ch_group->num[7]
#define PM_POWER_OFFSET_SET                 "po"
#define PM_POWER_OFFSET_DEFAULT             (0)
#define PM_POWER_OFFSET                     ch_group->num[8]

#define LIGHT_SENSOR_TYPE_SET               "n"
#define LIGHT_SENSOR_TYPE_DEFAULT           (0)
#define LIGHT_SENSOR_TYPE                   ch_group->num[0]
#define LIGHT_SENSOR_POLL_PERIOD_DEFAULT    (3.f)
#define LIGHT_SENSOR_FACTOR_SET             "f"
#define LIGHT_SENSOR_FACTOR_DEFAULT         (1)
#define LIGHT_SENSOR_FACTOR                 ch_group->num[1]
#define LIGHT_SENSOR_OFFSET_SET             "o"
#define LIGHT_SENSOR_OFFSET_DEFAULT         (0)
#define LIGHT_SENSOR_OFFSET                 ch_group->num[2]
#define LIGHT_SENSOR_RESISTOR_SET           "re"
#define LIGHT_SENSOR_RESISTOR_DEFAULT       (10000)
#define LIGHT_SENSOR_RESISTOR               ch_group->num[3]
#define LIGHT_SENSOR_POW_SET                "po"
#define LIGHT_SENSOR_POW_DEFAULT            (1)
#define LIGHT_SENSOR_POW                    ch_group->num[4]
#define LIGHT_SENSOR_I2C_DATA_ARRAY_SET     "dt"
#define LIGHT_SENSOR_I2C_BUS                ch_group->num[3]
#define LIGHT_SENSOR_I2C_ADDR               ch_group->num[4]

#define SEC_SYSTEM_MODES_ARRAY_SET          "n"
#define SEC_SYSTEM_CH_CURRENT_STATE         ch_group->ch[0]
#define SEC_SYSTEM_CH_TARGET_STATE          ch_group->ch[1]
#define SEC_SYSTEM_AT_HOME                  (0)
#define SEC_SYSTEM_OFF                      (3)
#define SEC_SYSTEM_ALARM                    (4)
#define SEC_SYSTEM_REC_ALARM_TIMER          ch_group->timer
#define SEC_SYSTEM_REC_ALARM_PERIOD_MS      (6000)

#define HIST_DATA_ARRAY_SET                 "h"
#define HIST_READ_ON_CLOCK_READY_SET        "x"
#define HIST_LAST_REGISTER                  ch_group->num[0]
#define HIST_TIME_SIZE                      (4)     // (sizeof(uint32_t))
#define HIST_DATA_SIZE                      (4)     // (sizeof(int32_t))
#define HIST_REGISTER_SIZE                  (HIST_TIME_SIZE + HIST_DATA_SIZE)
#define HIST_REGISTERS_BY_BLOCK             (100)
#define HIST_BLOCK_SIZE                     (HIST_REGISTERS_BY_BLOCK * HIST_REGISTER_SIZE)

#define MAX_ACTIONS                         (51)    // from 0 to (MAX_ACTIONS - 1)
#define MAX_WILDCARD_ACTIONS                (4)     // from 0 to (MAX_WILDCARD_ACTIONS - 1)
#define WILDCARD_ACTIONS_ARRAY_HEADER       "y"
#define NO_LAST_WILDCARD_ACTION             (-10000)
#define WILDCARD_ACTIONS                    "0"
#define WILDCARD_ACTION_REPEAT              "r"
#define COPY_ACTIONS                        "a"
#define BINARY_OUTPUTS_ARRAY                "r"
#define INITIAL_VALUE                       "n"
#define SYSTEM_ACTIONS_ARRAY                "s"
#define NETWORK_ACTIONS_ARRAY               "h"
#define IR_ACTIONS_ARRAY                    "i"
#define IR_ACTION_PROTOCOL                  "p"
#define IR_ACTION_PROTOCOL_LEN_2BITS        (14)
#define IR_ACTION_PROTOCOL_LEN_4BITS        (22)
#define IR_ACTION_PROTOCOL_LEN_6BITS        (30)
#define IR_CODE_HEADER_MARK_POS             (0)
#define IR_CODE_HEADER_SPACE_POS            (1)
#define IR_CODE_BIT0_MARK_POS               (2)
#define IR_CODE_BIT0_SPACE_POS              (3)
#define IR_CODE_BIT1_MARK_POS               (4)
#define IR_CODE_BIT1_SPACE_POS              (5)
#define IR_CODE_BIT2_MARK_POS               (6)
#define IR_CODE_BIT2_SPACE_POS              (7)
#define IR_CODE_BIT3_MARK_POS               (8)
#define IR_CODE_BIT3_SPACE_POS              (9)
#define IR_CODE_BIT4_MARK_POS               (10)
#define IR_CODE_BIT4_SPACE_POS              (11)
#define IR_CODE_BIT5_MARK_POS               (12)
#define IR_CODE_BIT5_SPACE_POS              (13)
#define IR_CODE_FOOTER_MARK_POS_2BITS       (6)
#define IR_CODE_FOOTER_MARK_POS_4BITS       (10)
#define IR_CODE_FOOTER_MARK_POS_6BITS       (14)
#define IR_ACTION_PROTOCOL_CODE             "c"
#define IR_ACTION_RAW_CODE                  "w"
#define IR_ACTION_TX_GPIO                   "t"
#define IR_ACTION_TX_GPIO_INVERTED          "j"
#define IR_ACTION_FREQ                      "x"
#define IR_ACTION_REPEATS                   "r"
#define IR_ACTION_REPEATS_PAUSE             "d"
#define SYSTEM_ACTION                       "a"
#define NETWORK_ACTION_HOST                 "h"
#define NETWORK_ACTION_PORT                 "p"
#define NETWORK_ACTION_URL                  "u"
#define NETWORK_ACTION_METHOD               "m"
#define NETWORK_ACTION_HEADER               "e"
#define NETWORK_ACTION_CONTENT              "c"
#define NETWORK_ACTION_WAIT_RESPONSE_SET    "w"
#define NETWORK_ACTION_WILDCARD_VALUE       "#HAA@"
#define SYSTEM_ACTION_REBOOT                (0)
#define SYSTEM_ACTION_SETUP_MODE            (1)
#define SYSTEM_ACTION_OTA_UPDATE            (2)
#define SYSTEM_ACTION_WIFI_RECONNECTION     (3)
#define SYSTEM_ACTION_WIFI_RECONNECTION_2   (4)
#define UART_ACTIONS_ARRAY                  "u"
#define UART_ACTION_UART                    "n"
#define UART_ACTION_PAUSE                   "d"

#define I2C_CONFIG_ARRAY                    "ic"

#define MCP23017_ARRAY                      "mc"

#define ACCESSORY_TYPE                      "t"
#define ACC_TYPE_ROOT_DEVICE                (0)
#define ACC_TYPE_SWITCH                     (1)
#define ACC_TYPE_OUTLET                     (2)
#define ACC_TYPE_BUTTON                     (3)
#define ACC_TYPE_LOCK                       (4)
#define ACC_TYPE_CONTACT_SENSOR             (5)
#define ACC_TYPE_OCCUPANCY_SENSOR           (6)
#define ACC_TYPE_LEAK_SENSOR                (7)
#define ACC_TYPE_SMOKE_SENSOR               (8)
#define ACC_TYPE_CARBON_MONOXIDE_SENSOR     (9)
#define ACC_TYPE_CARBON_DIOXIDE_SENSOR      (10)
#define ACC_TYPE_FILTER_CHANGE_SENSOR       (11)
#define ACC_TYPE_MOTION_SENSOR              (12)
#define ACC_TYPE_DOORBELL                   (13)
#define ACC_TYPE_WATER_VALVE                (20)
#define ACC_TYPE_THERMOSTAT                 (21)
#define ACC_TYPE_TEMP_SENSOR                (22)
#define ACC_TYPE_HUM_SENSOR                 (23)
#define ACC_TYPE_TH_SENSOR                  (24)
#define ACC_TYPE_THERMOSTAT_WITH_HUM        (25)
#define ACC_TYPE_HUMIDIFIER                 (26)
#define ACC_TYPE_HUMIDIFIER_WITH_TEMP       (27)
#define ACC_TYPE_LIGHTBULB                  (30)
#define ACC_TYPE_GARAGE_DOOR                (40)
#define ACC_TYPE_WINDOW_COVER               (45)
#define ACC_TYPE_LIGHT_SENSOR               (50)
#define ACC_TYPE_SECURITY_SYSTEM            (55)
#define ACC_TYPE_TV                         (60)
#define ACC_TYPE_FAN                        (65)
#define ACC_TYPE_POWER_MONITOR              (75)
#define ACC_TYPE_HISTORICAL                 (95)
#define ACC_TYPE_IAIRZONING                 (99)

#define SERIAL_STRING                       "sn"
#define SERIAL_STRING_LEN                   (11)

#define HOMEKIT_DEVICE_CATEGORY_SET         "ct"
#define HOMEKIT_DEVICE_CATEGORY_DEFAULT     (1)

#define ACC_CREATION_DELAY                  "cd"
#define EXIT_EMERGENCY_SETUP_MODE_TIME      (2500)
#define SETUP_MODE_ACTIVATE_COUNT           "z"
#define SETUP_MODE_DEFAULT_ACTIVATE_COUNT   (8)
#define SETUP_MODE_TOGGLE_TIME_MS           (1050)
#define CUSTOM_TRIGGER_COMMAND              "#HAA@trcmd"

#define SAVE_STATES_TIMER                   ch_group_find_by_acc(ACC_TYPE_ROOT_DEVICE)->timer
#define WIFI_WATCHDOG_TIMER                 ch_group_find_by_acc(ACC_TYPE_ROOT_DEVICE)->timer2
#define WIFI_STATUS_DISCONNECTED            (0)
#define WIFI_STATUS_CONNECTING              (1)
#define WIFI_STATUS_CONNECTED               (2)
#define WIFI_PING_ERRORS                    "w"

#define WIFI_RECONNECTION_POLL_PERIOD_MS    (5000)
#define WIFI_DISCONNECTED_LONG_TIME         (24)    // * WIFI_RECONNECTION_POLL_PERIOD_MS

#define WIFI_WATCHDOG_POLL_PERIOD_MS        (1500)
#define WIFI_WATCHDOG_ARP_RESEND_PERIOD     (39)    // * WIFI_WATCHDOG_POLL_PERIOD_MS
#define WIFI_WATCHDOG_ROAMING_PERIOD        (1234)  // * WIFI_WATCHDOG_POLL_PERIOD_MS

#define STATUS_LED_DURATION_ON              (30)
#define STATUS_LED_DURATION_OFF             (120)

#define SAVE_STATES_DELAY_MS                (5000)
#define RANDOM_DELAY_MS                     (7000)

#define ACCESSORIES_WITHOUT_BRIDGE          (4)     // Max number of accessories before using a bridge

#define SYSTEM_UPTIME_MS                    ((float) sdk_system_get_time() * 1e-3)

#define MIN(x, y)                           (((x) < (y)) ? (x) : (y))
#define MAX(x, y)                           (((x) > (y)) ? (x) : (y))

#define KELVIN_TO_CELSIUS(x)                ((x) - 273.15)

#define NTP_POLL_PERIOD_MS                  (1 * 3600 * 1000)   // 1 hour

#ifdef HAA_DEBUG
#define LIGHT_DEBUG
#endif

#ifdef LIGHT_DEBUG
#define L_DEBUG(message, ...)               printf(message "\n", ##__VA_ARGS__)
#else
#define L_DEBUG(message, ...)
#endif

#endif // __HAA_HEADER_H__
