/*
 * Home Accessory Architect
 *
 * Copyright 2020-2022 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __HAA_COMMON_HEADER_H__
#define __HAA_COMMON_HEADER_H__

#define CUSTOM_REPO_SYSPARAM                "ota_sever"
#define PORT_NUMBER_SYSPARAM                "ota_port"
#define PORT_SECURE_SYSPARAM                "ota_sec"
#define HAAMAIN_VERSION_SYSPARAM            "ota_version"
#define INSTALLER_VERSION_SYSPARAM          "ota_repo"
#define WIFI_SSID_SYSPARAM                  "wifi_ssid"
#define WIFI_PASSWORD_SYSPARAM              "wifi_password"
#define WIFI_MODE_SYSPARAM                  "wifi_mode"
#define WIFI_BSSID_SYSPARAM                 "wifi_bssid"
#define WIFI_LAST_WORKING_PHY_SYSPARAM      "wifi_phy"
#define HOMEKIT_RE_PAIR_SYSPARAM            "re_pair"
#define HOMEKIT_PAIRING_COUNT_SYSPARAM      "pair_count"
#define TOTAL_SERV_SYSPARAM                 "total_ac"
#define HAA_SCRIPT_SYSPARAM                 "haa_conf"
#define HAA_SETUP_MODE_SYSPARAM             "setup"
#define LAST_CONFIG_NUMBER_SYSPARAM         "hkcf"

#define BOOT0SECTOR                         (0x02000)
#define BOOT1SECTOR                         (0x91000)   // Must match the sdk/ld/program1.ld value
#define SYSPARAMSECTOR                      (0xF3000)
#define SYSPARAMSIZE                        (8)

#define WIFI_CONFIG_SERVER_PORT             (4567)
#define AUTO_REBOOT_TIMEOUT                 (90000)
#define AUTO_REBOOT_LONG_TIMEOUT            (900000)
#define AUTO_REBOOT_ON_HANG_OTA_TIMEOUT     (2000000)
#define MAX_BODY_LEN                        (16000)
#define BEST_RSSI_MARGIN                    (1)

#define TASK_SIZE_FACTOR                    (1)

#define HIGH_HOMEKIT_CH_NUMBER              (6)

#define DEBUG(message, ...)                 printf("%s: " message "\n", __func__, ##__VA_ARGS__)
#define INFO(message, ...)                  printf(message "\n", ##__VA_ARGS__)
#define ERROR(message, ...)                 printf("! " message "\n", ##__VA_ARGS__)

#define MS_TO_TICKS(x)                      ((x) / portTICK_PERIOD_MS)

#define CHAR_IS_NUMBER(c)                   ((c) >= '0' && (c) <= '9')

#endif  // __HAA_COMMON_HEADER_H__

