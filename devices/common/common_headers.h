/*
 * Home Accessory Architect
 *
 * Copyright 2020-2021 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __HAA_COMMON_HEADER_H__
#define __HAA_COMMON_HEADER_H__

#define CUSTOM_REPO_SYSPARAM                "ota_sever"
#define PORT_NUMBER_SYSPARAM                "ota_port"
#define PORT_SECURE_SYSPARAM                "ota_sec"
#define OTA_VERSION_SYSPARAM                "ota_repo"
#define USER_VERSION_SYSPARAM               "ota_version"
#define WIFI_SSID_SYSPARAM                  "wifi_ssid"
#define WIFI_PASSWORD_SYSPARAM              "wifi_password"
#define WIFI_MODE_SYSPARAM                  "wifi_mode"
#define WIFI_BSSID_SYSPARAM                 "wifi_bssid"
#define AUTO_OTA_SYSPARAM                   "aota"
#define TOTAL_SERV_SYSPARAM                 "total_ac"
#define HAA_JSON_SYSPARAM                   "haa_conf"
#define HAA_SETUP_MODE_SYSPARAM             "setup"
#define LAST_CONFIG_NUMBER_SYSPARAM         "hkcf"

#define BOOT0SECTOR                         (0x02000)
#define BOOT1SECTOR                         (0x91000)   // Must match the sdk/ld/program1.ld value
#define SYSPARAMSECTOR                      (0xF3000)
#define SYSPARAMSIZE                        (8)

#define WIFI_CONFIG_SERVER_PORT             (4567)
#define AUTO_REBOOT_TIMEOUT                 (90000)
#define AUTO_REBOOT_LONG_TIMEOUT            (900000)
#define MAX_BODY_LEN                        (16000)
#define BEST_RSSI_MARGIN                    (1)

#define DEBUG(message, ...)                 printf("%s: " message "\n", __func__, ##__VA_ARGS__)
#define INFO(message, ...)                  printf(message "\n", ##__VA_ARGS__)
#define ERROR(message, ...)                 printf("! " message "\n", ##__VA_ARGS__)

#define MS_TO_TICKS(x)                      ((x) / portTICK_PERIOD_MS)

#define FREEHEAP()                          printf("Free Heap %d\n", xPortGetFreeHeapSize())

#endif  // __HAA_COMMON_HEADER_H__

