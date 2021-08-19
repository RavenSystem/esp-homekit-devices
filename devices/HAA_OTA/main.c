/*
 * Home Accessory Architect OTA Installer
 *
 * Copyright 2020-2021 José Antonio Jiménez Campos (@RavenSystem)
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
#include <spiflash.h>

#include <espressif/esp_common.h>

#include <wifi_config.h>
#include <sysparam.h>

#include <rboot-api.h>

#include <adv_logger.h>

#include "ota.h"

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
    INFO("\n\nHAA Installer v%s\n\n", OTAVERSION);

#ifdef HAABOOT
    sysparam_set_string(USER_VERSION_SYSPARAM, "0.0.0");
#endif  // HAABOOT
    
    sysparam_status_t status;
    
    status = sysparam_get_string(CUSTOM_REPO_SYSPARAM, &user_repo);
    if (status != SYSPARAM_OK || strcmp(user_repo, "") == 0) {
        user_repo = strdup(OTAREPO);
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
    
    INFO("- Server %s", user_repo);
    INFO("- Port   %i", port);
    INFO("- SSL    %s\n", is_ssl ? "yes" : "no");

    status = sysparam_get_string(USER_VERSION_SYSPARAM, &user_version);
    if (status == SYSPARAM_OK) {
        INFO("Current HAAMAIN installed v%s\n", user_version);

        ota_init(user_repo, is_ssl);
        
        vTaskDelay(MS_TO_TICKS(2000));
        
        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
        
        for (;;) {
            INFO("\n*** STARTING UPDATE PROCESS\n");
            tries_count++;
            
#ifdef HAABOOT
            INFO("\nRunning HAABOOT\n");
/*
            printf("HK data migration\n");
            const char magic1[] = "HAP";
            char magic[sizeof(magic1)];
            memset(magic, 0, sizeof(magic));

            if (!spiflash_read(OLD_SPIFLASH_BASE_ADDR, (byte*) magic, sizeof(magic))) {
                printf("! Read old sector\n");
                
            } else if (strncmp(magic, magic1, sizeof(magic1)) == 0) {
                printf("Formatting new sector 0x%x\n", SPIFLASH_BASE_ADDR);
                if (!spiflash_erase_sector(SPIFLASH_BASE_ADDR)) {
                    printf("! Erase new sector\n");
                } else {
                    printf("Reading data from 0x%x\n", OLD_SPIFLASH_BASE_ADDR);
                    
                    byte data[4096];
                    if (!spiflash_read(OLD_SPIFLASH_BASE_ADDR, data, sizeof(data))) {
                        printf("! Read HK data\n");
                    } else {
                        printf("Writting data to 0x%x\n", SPIFLASH_BASE_ADDR);
                        
                        if (!spiflash_write(SPIFLASH_BASE_ADDR, data, sizeof(data))) {
                            printf("! Write HK data to new sector\n");
                        } else {
                            printf("Erasing old sector 0x%x\n", OLD_SPIFLASH_BASE_ADDR);
                            if (!spiflash_erase_sector(OLD_SPIFLASH_BASE_ADDR)) {
                                printf("! Erase old sector\n");
                            } else {
                                printf("HK data is migrated\n");
                            }
                        }
                    }
                }
            } else {
                printf("Data is already migrated\n\n");
            }
*/
            static char otamainfile[] = OTAMAINFILE;
            if (ota_get_sign(user_repo, otamainfile, signature, port, is_ssl) > 0) {
                file_size = ota_get_file(user_repo, otamainfile, BOOT1SECTOR, port, is_ssl);
                if (file_size > 0 && ota_verify_sign(BOOT1SECTOR, file_size, signature) == 0) {
                    ota_finalize_file(BOOT1SECTOR);
                    INFO("\n*** OTAMAIN installed\n");
                    sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
                    rboot_set_temp_rom(1);
                    ota_reboot();
                } else {
                    ERROR("Installing OTAMAIN");
                    sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 1);
                }
            } else {
                ERROR("Downloading OTAMAIN signature");
                sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 1);
            }
#else   // HAABOOT
            INFO("\nRunning OTAMAIN\n");
            
            if (ota_version) {
                free(ota_version);
                ota_version = NULL;
            }
            static char otaversionfile[] = OTAVERSIONFILE;
            ota_version = ota_get_version(user_repo, otaversionfile, port, is_ssl);
            
            if (ota_version && strcmp(ota_version, OTAVERSION) != 0) {
                static char otabootfile[] = OTABOOTFILE;
                if (ota_get_sign(user_repo, otabootfile, signature, port, is_ssl) > 0) {
                    file_size = ota_get_file(user_repo, otabootfile, BOOT0SECTOR, port, is_ssl);
                    if (file_size > 0 && ota_verify_sign(BOOT0SECTOR, file_size, signature) == 0) {
                        ota_finalize_file(BOOT0SECTOR);
                        INFO("\n* HAABOOT new version installed\n");
                    } else {
                        ERROR("Installing HAABOOT new version\n");
                    }
                    
                    break;
                } else {
                    ERROR("Downloading HAABOOT new version signature\n");
                }
            }
            
            if (new_version) {
                free(new_version);
                new_version = NULL;
            }
            static char haaversionfile[] = HAAVERSIONFILE;
            new_version = ota_get_version(user_repo, haaversionfile, port, is_ssl);
            
            if (new_version && strcmp(new_version, user_version) != 0) {
                static char haamainfile[] = HAAMAINFILE;
                if (ota_get_sign(user_repo, haamainfile, signature, port, is_ssl) > 0) {
                    file_size = ota_get_file(user_repo, haamainfile, BOOT0SECTOR, port, is_ssl);
                    if (file_size > 0 && ota_verify_sign(BOOT0SECTOR, file_size, signature) == 0) {
                        ota_finalize_file(BOOT0SECTOR);
                        sysparam_set_string(USER_VERSION_SYSPARAM, new_version);
                        INFO("\n* HAAMAIN v%s installed\n", new_version);
                    } else {
                        ERROR("Installing HAAMAIN\n");
                    }
                } else {
                    ERROR("Downloading HAAMAIN signature\n");
                }
            }
            
            break;
#endif  // HAABOOT
            
            if (tries_count == MAX_TRIES) {
                break;
            }
            
            vTaskDelay(MS_TO_TICKS(5000));
        }
    } else {
        ERROR("Reading HAAMAIN Version, fixing\n");
        sysparam_set_string(USER_VERSION_SYSPARAM, "0.0.0");
    }
    
    ota_reboot();
}

void on_wifi_ready() {
    xTaskCreate(ota_task, "ota", 4096, NULL, 1, NULL);
}

void user_init(void) {
    sdk_wifi_station_set_auto_connect(false);
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_disconnect();
    sdk_wifi_set_sleep_type(WIFI_SLEEP_NONE);
    
    uart_set_baud(0, 115200);
    
    adv_logger_init(ADV_LOGGER_UART0_UDP_BUFFERED, NULL);
    
    // GPIO Init
    for (uint8_t i = 0; i < 17; i++) {
        if (i < 6 || i > 11) {
            if (!(i == 1 || i == 3)) {
                gpio_enable(i, GPIO_INPUT);
            }
        }
    }
    
    INFO("\n\n");
    
    sysparam_status_t status;

    status = sysparam_init(SYSPARAMSECTOR, 0);
    if (status != SYSPARAM_OK) {
        INFO("No sysparam, erasing");
        
        wifi_config_remove_sys_param();
        
        INFO("Creating new");
        status = sysparam_create_area(SYSPARAMSECTOR, SYSPARAMSIZE, true);
        if (status == SYSPARAM_OK) {
            INFO("Sysparam created");
            status = sysparam_init(SYSPARAMSECTOR, 0);
        }
    }
    
    if (status == SYSPARAM_OK) {
        INFO("Sysparam OK\n");
    } else {
        ERROR("Sysparam %d", status);
    }
        
    wifi_config_init("HAA", NULL, on_wifi_ready);
}
