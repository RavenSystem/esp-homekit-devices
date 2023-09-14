/*
 * Home Accessory Architect
 *
 * Copyright 2020-2022 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __HAA_COMMON_HEADER_H__
#define __HAA_COMMON_HEADER_H__

#ifdef ESP_PLATFORM

#define TASK_SIZE_FACTOR                    (5)
#define MAX_SETUP_BODY_LEN                  (70000)
#define MAX_GPIOS                           (GPIO_NUM_MAX)

#define SDK_UART_BUFFER_SIZE                (256)   // Should be grater than UART_FIFO_LEN
#include "adv_logger.h"

#define INFO_NNL(message, ...)              adv_logger_printf(message, ##__VA_ARGS__)

#if !defined(CONFIG_IDF_TARGET_ESP32)
#define ESP_HAS_INTERNAL_TEMP_SENSOR        (1)
#endif

#if !defined(CONFIG_IDF_TARGET_ESP32)
#define ESP_HAS_GPIO_GLITCH_FILTER          (1)
#endif

#if defined(CONFIG_IDF_TARGET_ESP32C6)
#define ESP_HAS_GPIO_FLEX_FILTER            (1)
#endif

#else   // ESP-OPEN-RTOS

#define TASK_SIZE_FACTOR                    (1)
#define MAX_SETUP_BODY_LEN                  (16500)
#define MAX_GPIOS                           (17)

#define INFO_NNL(message, ...)              printf(message, ##__VA_ARGS__)

#endif

#ifdef HAA_SINGLE_CORE
#define HAA_SINGLE_CORE_SUFIX               "_1"
#else
#define HAA_SINGLE_CORE_SUFIX               ""
#endif

#ifdef HAA_XTAL26
#define HAA_XTAL26_SUFIX                    "_26"
#else
#define HAA_XTAL26_SUFIX                    ""
#endif

// For ESP-OPEN-RTOS only
#define BOOT0SECTOR                         (0x02000)
#define BOOT1SECTOR                         (0x91000)   // Must match the sdk/ld/program1.ld value
#define SYSPARAMSECTOR                      (0xF3000)
#define SYSPARAMSIZE                        (8)


#define CUSTOM_REPO_SYSPARAM                "ota_sever"
#define PORT_NUMBER_SYSPARAM                "ota_port"
#define PORT_SECURE_SYSPARAM                "ota_sec"
#define HAAMAIN_VERSION_SYSPARAM            "ota_version"
#define INSTALLER_VERSION_SYSPARAM          "ota_repo"
#define WIFI_STA_SSID_SYSPARAM              "wifi_ssid"
#define WIFI_STA_PASSWORD_SYSPARAM          "wifi_password"
#define WIFI_STA_MODE_SYSPARAM              "wifi_mode"
#define WIFI_STA_BSSID_SYSPARAM             "wifi_bssid"
#define WIFI_AP_ENABLE_SYSPARAM             "wifi_ap"
#define WIFI_AP_PASSWORD_SYSPARAM           "wifi_ap_pass"

#ifndef ESP_PLATFORM
#define WIFI_LAST_WORKING_PHY_SYSPARAM      "wifi_phy"
#endif

#define HOMEKIT_RE_PAIR_SYSPARAM            "re_pair"
#define HOMEKIT_PAIRING_COUNT_SYSPARAM      "pair_count"
#define TOTAL_SERV_SYSPARAM                 "total_ac"
#define HAA_SCRIPT_SYSPARAM                 "haa_conf"
#define HAA_SETUP_MODE_SYSPARAM             "setup"
#define LAST_CONFIG_NUMBER_SYSPARAM         "hkcf"

#define WIFI_CONFIG_SERVER_PORT             (4567)
#define AUTO_REBOOT_TIMEOUT                 (90000)
#define AUTO_REBOOT_LONG_TIMEOUT            (900000)
#define AUTO_REBOOT_ON_HANG_OTA_TIMEOUT     (2000000)
#define BEST_RSSI_MARGIN                    (1)

#define HIGH_HOMEKIT_CH_NUMBER              (6)

#define DEBUG(message, ...)                 printf("%s: " message "\n", __func__, ##__VA_ARGS__)
#define INFO(message, ...)                  INFO_NNL(message "\n", ##__VA_ARGS__)
#define ERROR(message, ...)                 INFO("! " message, ##__VA_ARGS__)

#define MS_TO_TICKS(x)                      ((x) / portTICK_PERIOD_MS)

#define CHAR_IS_NUMBER(c)                   ((c) >= '0' && (c) <= '9')

#endif  // __HAA_COMMON_HEADER_H__

