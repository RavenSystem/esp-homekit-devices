/*
 * Home Accessory Architect OTA Update
 *
 * Copyright 2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

/*
 * Based on Life-Cycle-Manager (LCM) by HomeAccessoryKid (@HomeACcessoryKid), licensed under Apache License 2.0.
 * https://github.com/HomeACcessoryKid/life-cycle-manager
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

#include <rboot-api.h>

#include <adv_logger.h>

#include "ota.h"
#include "header.h"

char* user_repo = NULL;
char* user_version = NULL;

char* new_version = NULL;
char* ota_version = NULL;
byte signature[SIGNSIZE];
int file_size = 0;

uint16_t port = 443;
bool is_ssl = true;
uint8_t tries_count = 0;

void ota_task(void *arg) {
    printf("\nHAA Installer Version: %s\n\n", OTAVERSION);

#ifdef HAABOOT
    sysparam_set_string(USER_VERSION_SYSPARAM, "none");
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
    
    printf("- Server: %s\n", user_repo);
    printf("- Port:   %i\n", port);
    printf("- SSL:    %s\n\n", is_ssl ? "yes" : "no");

    status = sysparam_get_string(USER_VERSION_SYSPARAM, &user_version);
    if (status == SYSPARAM_OK) {
        printf("Current HAAMAIN version installed: %s\n\n", user_version);

        ota_init(user_repo, is_ssl);
        
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        
        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
        
        for (;;) {
            printf("\n*** STARTING UPDATE PROCESS\n\n");
            tries_count++;
            
#ifdef HAABOOT
            printf("\nRunning HAABOOT\n\n");

            if (ota_get_sign(user_repo, OTAMAINFILE, signature, port, is_ssl) > 0) {
                file_size = ota_get_file(user_repo, OTAMAINFILE, BOOT1SECTOR, port, is_ssl);
                if (file_size > 0 && ota_verify_sign(BOOT1SECTOR, file_size, signature) == 0) {
                    ota_finalize_file(BOOT1SECTOR);
                    printf("\n*** OTAMAIN installed\n\n");
                    sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
                    rboot_set_temp_rom(1);
                    ota_reboot();
                } else {
                    printf("\n!!! Error installing OTAMAIN\n\n");
                    sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 1);
                }
            } else {
                printf("\n!!! Error downloading OTAMAIN signature\n\n");
                sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 1);
            }
#else   // HAABOOT
            printf("\nRunning OTAMAIN\n\n");
            
            if (ota_version) {
                free(ota_version);
                ota_version = NULL;
            }
            ota_version = ota_get_version(user_repo, OTAVERSIONFILE, port, is_ssl);
            
            if (ota_version && strcmp(ota_version, OTAVERSION) != 0) {
                if (ota_get_sign(user_repo, OTABOOTFILE, signature, port, is_ssl) > 0) {
                    file_size = ota_get_file(user_repo, OTABOOTFILE, BOOT0SECTOR, port, is_ssl);
                    if (file_size > 0 && ota_verify_sign(BOOT0SECTOR, file_size, signature) == 0) {
                        ota_finalize_file(BOOT0SECTOR);
                        printf("\n*** HAABOOT new version installed\n\n");
                    } else {
                        printf("\n!!! Error installing HAABOOT new version\n\n");
                    }
                    
                    break;
                } else {
                    printf("\n!!! Error downloading HAABOOT new version signature\n\n");
                }
            }
            
            if (new_version) {
                free(new_version);
                new_version = NULL;
            }
            new_version = ota_get_version(user_repo, HAAVERSIONFILE, port, is_ssl);
            
            if (new_version && strcmp(new_version, user_version) != 0) {
                if (ota_get_sign(user_repo, HAAMAINFILE, signature, port, is_ssl) > 0) {
                    file_size = ota_get_file(user_repo, HAAMAINFILE, BOOT0SECTOR, port, is_ssl);
                    if (file_size > 0 && ota_verify_sign(BOOT0SECTOR, file_size, signature) == 0) {
                        ota_finalize_file(BOOT0SECTOR);
                        sysparam_set_string(USER_VERSION_SYSPARAM, new_version);
                        printf("\n*** HAAMAIN v%s installed\n\n", new_version);
                    } else {
                        printf("\n!!! Error installing HAAMAIN\n\n");
                    }
                } else {
                    printf("\n!!! Error downloading HAAMAIN signature\n\n");
                }
            }
            
            break;
#endif  // HAABOOT
            
            if (tries_count == MAX_TRIES) {
                break;
            }
            
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
    }
    
    ota_reboot();
}

void on_wifi_ready() {
    xTaskCreate(ota_task, "ota_task", 4096, NULL, 1, NULL);
}

void user_init(void) {
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_disconnect();
    
    uart_set_baud(0, 115200);
    
    adv_logger_init(ADV_LOGGER_UART0_UDP);
    
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
