/*
 * Home Accessory Architect OTA Installer
 *
 * Copyright 2020-2021 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __HAA_OTA_HEADER_H__
#define __HAA_OTA_HEADER_H__

#define OTAVERSION              "4.7.2"

#define CUSTOM_REPO_SYSPARAM    "ota_sever"
#define PORT_NUMBER_SYSPARAM    "ota_port"
#define PORT_SECURE_SYSPARAM    "ota_sec"
#define OTA_VERSION_SYSPARAM    "ota_repo"
#define USER_VERSION_SYSPARAM   "ota_version"
#define WIFI_SSID_SYSPARAM      "wifi_ssid"
#define WIFI_PASSWORD_SYSPARAM  "wifi_password"
#define WIFI_MODE_SYSPARAM      "wifi_mode"
#define WIFI_BSSID_SYSPARAM     "wifi_bssid"
#define AUTO_OTA_SYSPARAM       "aota"
#define TOTAL_ACC_SYSPARAM      "total_ac"
#define HAA_JSON_SYSPARAM       "haa_conf"
#define HAA_SETUP_MODE_SYSPARAM "setup"
#define LAST_CONFIG_NUMBER      "hkcf"

#define SYSPARAMSECTOR          0xF3000
#define SYSPARAMSIZE            8

#define SECTORSIZE              4096

#endif  // __HAA_OTA_HEADER_H__
