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

/*  (c) 2018-2019 HomeAccessoryKid */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <espressif/esp_common.h>

#include <wifi_config.h>
#include <sysparam.h>

#include <ota.h>
#include <udplogger.h>

#include "header.h"

char* user_repo = NULL;
char* user_version = NULL;

char* new_version = NULL;
char* ota_version = NULL;
byte signature[SIGNSIZE];
int file_size = 0;

uint16_t port = 443;
bool is_ssl = true;

void ota_task(void *arg) {
    UDPLGP("\nHAA Installer Version: %s\n\n", OTAVERSION);

#ifdef HAABOOT
//  if (ota_boot()) {
        sysparam_set_string(USER_VERSION_SYSPARAM, "none");
//  }
#endif  // HAABOOT
    
    sysparam_status_t status;
    
    status = sysparam_get_string(CUSTOM_REPO_SYSPARAM, &user_repo);
    if (status != SYSPARAM_OK || strcmp(user_repo, "") == 0) {
        user_repo = OTAREPO;
    } else {
        int8_t value_is_ssl;
        status = sysparam_get_int8(PORT_SECURE_SYSPARAM, &value_is_ssl);
        if (status == SYSPARAM_OK && value_is_ssl == 0) {
            is_ssl = false;
        }
        
        int32_t value_port;
        status = sysparam_get_int32(PORT_NUMBER_SYSPARAM, &value_port);
        if (status == SYSPARAM_OK && value_port > 0 && value_port <= UINT16_MAX) {
            port = value_port;
        }
    }
    
    UDPLGP("- Server: %s\n", user_repo);
    UDPLGP("- Port:   %i\n", port);
    UDPLGP("- SSL:    %s\n\n", is_ssl ? "yes" : "no");

    status = sysparam_get_string(USER_VERSION_SYSPARAM, &user_version);
    if (status == SYSPARAM_OK) {
        UDPLGP("Current HAAMAIN version installed: %s\n\n", user_version);

        ota_init(user_repo, is_ssl);
        
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        
        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
        
        for (;;) {
            UDPLGP("\n*** STARTING UPDATE PROCESS\n\n");

            if (ota_version) {
                free(ota_version);
            }
            ota_version = ota_get_version(user_repo, OTAVERSIONFILE, port, is_ssl);
            
#ifdef HAABOOT
//          if (ota_boot()) {
                UDPLGP("\nRunning HAABOOT\n\n");

                if (ota_get_sign(user_repo, OTAMAINFILE, signature, port, is_ssl) > 0) {
                    file_size = ota_get_file(user_repo, OTAMAINFILE, BOOT1SECTOR, port, is_ssl);
                    if (file_size > 0 && ota_verify_sign(BOOT1SECTOR, file_size, signature) == 0) {
                        ota_finalize_file(BOOT1SECTOR);
                        UDPLGP("\n*** OTAMAIN installed\n\n");
                        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
                        ota_temp_boot();
                    } else {
                        UDPLGP("\n!!! Error installing OTAMAIN\n\n");
                        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 1);
                    }
                } else {
                    UDPLGP("\n!!! Error downloading OTAMAIN signature\n\n");
                    sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 1);
                }
#else   // HAABOOT
//          } else {
                UDPLGP("\nRunning OTAMAIN\n\n");
                
                if (strcmp(ota_version, OTAVERSION) != 0) {
                    if (ota_get_sign(user_repo, OTABOOTFILE, signature, port, is_ssl) > 0) {
                        file_size = ota_get_file(user_repo, OTABOOTFILE, BOOT0SECTOR, port, is_ssl);
                        if (file_size > 0 && ota_verify_sign(BOOT0SECTOR, file_size, signature) == 0) {
                            ota_finalize_file(BOOT0SECTOR);
                            UDPLGP("\n*** HAABOOT new version installed\n\n");
                        } else {
                            UDPLGP("\n!!! Error installing HAABOOT new version\n\n");
                        }
                        
                        break;
                    } else {
                        UDPLGP("\n!!! Error downloading HAABOOT new version signature\n\n");
                    }
                }
                
                if (new_version) {
                    free(new_version);
                }
                new_version = ota_get_version(user_repo, HAAVERSIONFILE, port, is_ssl);
                
                if (strcmp(new_version, user_version) != 0) {
                    if (ota_get_sign(user_repo, HAAMAINFILE, signature, port, is_ssl) > 0) {
                        file_size = ota_get_file(user_repo, HAAMAINFILE, BOOT0SECTOR, port, is_ssl);
                        if (file_size > 0 && ota_verify_sign(BOOT0SECTOR, file_size, signature) == 0) {
                            ota_finalize_file(BOOT0SECTOR);
                            sysparam_set_string(USER_VERSION_SYSPARAM, new_version);
                            UDPLGP("\n*** HAAMAIN v%s installed\n\n", new_version);
                        } else {
                            UDPLGP("\n!!! Error installing HAAMAIN\n\n");
                        }
                    } else {
                        UDPLGP("\n!!! Error downloading HAAMAIN signature\n\n");
                    }
                }
                
                break;
//          }
#endif  // HAABOOT
            
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
    }
    
    ota_reboot();
}

void on_wifi_ready() {
    xTaskCreate(udplog_send, "logsend", 256, NULL, 2, NULL);
    xTaskCreate(ota_task, "ota_task", 4096, NULL, 1, NULL);
}

void user_init(void) {
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_disconnect();

    uart_set_baud(0, 115200);
    printf("\n\n\n");
    
    sysparam_status_t status;

    status = sysparam_init(SYSPARAMSECTOR, 0);
    if (status == SYSPARAM_NOTFOUND) {
        printf("Sysparam not found, creating...\n");
        status = sysparam_create_area(SYSPARAMSECTOR, SYSPARAMSIZE, true);
        if (status == SYSPARAM_OK) {
            printf("Sysparam created\n");
            status = sysparam_init(SYSPARAMSECTOR, 0);
        }
    }
    
    if (status == SYSPARAM_OK) {
        printf("Sysparam OK\n\n");
    } else {
        printf("! Sysparam %d\n", status);
    }
        
    wifi_config_init("HAA", NULL, on_wifi_ready);
}
