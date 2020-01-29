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

/* (c) 2018-2019 HomeAccessoryKid
 * LifeCycleManager dual app
 * use local.mk to turn it into the LCM otamain.bin app or the otaboot.bin app
 */

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

void ota_task(void *arg) {
    char* user_repo = NULL;
    char* user_version = NULL;
    char* user_file = NULL;
    char* new_version = NULL;
    char* ota_version = NULL;
    signature_t signature;
    int file_size;

    UDPLGP("OTA Version: %s\n", OTAVERSION);

    ota_init();

    if (ota_boot())
        ota_write_status("0.0.0");
    
    if (!ota_load_user_app(&user_repo, &user_version, &user_file)) {
        if (ota_boot()) {
            new_version = ota_get_version(user_repo);
            if (!strcmp(new_version, "404")) {
                UDPLGP("! %s no exist! HALTED!\n", user_repo);
                vTaskDelete(NULL);
            }
        }
        
        for (;;) {
            UDPLGP("\nSTARTING\n\n");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
         
            if (ota_version)
                free(ota_version);
            
            ota_version = ota_get_version(OTAREPO);
            
            if (ota_boot()) {
                UDPLGP("Running BOOT\n");

                if (ota_get_hash(OTAREPO, ota_version, MAINFILE, &signature)) {
                    file_size = ota_get_file(OTAREPO, ota_version, MAINFILE, BOOT1SECTOR);
                    if (file_size <= 0)
                        continue;
                    
                    ota_finalize_file(BOOT1SECTOR);
                    ota_reboot();

                } else {
                    if (ota_verify_hash(BOOT1SECTOR, &signature)) {
                        file_size = ota_get_file(OTAREPO, ota_version, MAINFILE, BOOT1SECTOR);
                        if (file_size <= 0)
                            continue;
                        
                        if (ota_verify_hash(BOOT1SECTOR, &signature))
                            continue;
                        
                        ota_finalize_file(BOOT1SECTOR);
                    }
                }
       
                ota_temp_boot();
                
            } else {
                UDPLGP("Running MAIN\n");

                if (ota_compare(ota_version, OTAVERSION) > 0) {
                    ota_get_hash(OTAREPO, ota_version, BOOTFILE, &signature);
                    file_size = ota_get_file(OTAREPO, ota_version, BOOTFILE, BOOT0SECTOR);
                    if (file_size <= 0)
                        continue;
                
                    if (ota_verify_hash(BOOT0SECTOR, &signature))
                        continue;
                    
                    ota_finalize_file(BOOT0SECTOR);
                    break;
                }
                
                if (new_version)
                    free(new_version);
                
                new_version = ota_get_version(user_repo);
                if (ota_compare(new_version, user_version) > 0) {
                    UDPLGP("Repo=\'%s\' Version=\'%s\' File=\'%s\'\n", user_repo, new_version, user_file);
                    ota_get_hash(user_repo, new_version, user_file, &signature);
                    file_size = ota_get_file(user_repo, new_version, user_file,  BOOT0SECTOR);
                    
                    if (file_size <= 0 || ota_verify_hash(BOOT0SECTOR, &signature))
                        continue;
                    
                    ota_finalize_file(BOOT0SECTOR);
                    ota_write_status(new_version);
                }
                break;
            }
        }
    }
    
    ota_reboot();
}

void on_wifi_ready() {
    xTaskCreate(udplog_send, "logsend", 256, NULL, 2, NULL);
    xTaskCreate(ota_task,"ota", 4096, NULL, 1, NULL);
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
