/*
* Home Accessory Architect OTA Update
*
* Copyright 2020 José Antonio Jiménez Campos (@RavenSystem)
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

#ifndef __HAA_OTA_HEADER_H__
#define __HAA_OTA_HEADER_H__

#define OTAVERSION              "3.1.0"

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

#endif  // __HAA_OTA_HEADER_H__
