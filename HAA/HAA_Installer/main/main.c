/*
 * Home Accessory Architect Installer
 *
 * Copyright 2020-2023 José Antonio Jiménez Campos (@RavenSystem)
 *
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

#include "setup.h"
#include <sysparam.h>

#include <rboot-api.h>

#include <adv_logger.h>

#include "ota.h"

char* user_repo = NULL;
char* haamain_installed_version = NULL;

char* haamain_new_version = NULL;
char* installer_new_version = NULL;
uint8_t signature[SIGNSIZE];
int file_size;
int result;

int port = 443;
bool is_ssl = true;
int tries_count = 0;
int tries_partial_count;

TaskHandle_t xHandle = NULL;

void init_task() {
    uart_set_baud(0, 115200);
    adv_logger_init(ADV_LOGGER_UART0_UDP_BUFFERED, NULL);
    
    sysparam_status_t status = sysparam_init(SYSPARAMSECTOR, 0);
    if (status != SYSPARAM_OK) {
        setup_mode_reset_sysparam();
    }
    
    wifi_config_init("HAA", xHandle);
    
    vTaskDelete(NULL);
}

void ota_task(void *arg) {
    xTaskCreate(init_task, "INI", 512, NULL, (tskIDLE_PRIORITY + 2), NULL);
    
    vTaskSuspend(NULL);
    
    vTaskDelay(MS_TO_TICKS(1000));
    
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
    
    printf("HAA Installer "INSTALLER_VERSION"\nREPO http%s://%s %i\n**** ", is_ssl ? "s" : "", user_repo, port);
    
#ifdef HAABOOT
    INFO("HAABOOT");
    
    sysparam_set_data(HAAMAIN_VERSION_SYSPARAM, NULL, 0, false);
    sysparam_compact();
    
#else   // HAABOOT
    printf("OTAMAIN\nHAAMAIN ");
    
    status = sysparam_get_string(HAAMAIN_VERSION_SYSPARAM, &haamain_installed_version);
    if (status != SYSPARAM_OK) {
        printf("not");
        haamain_installed_version = strdup("");
        
    } else {
        printf("%s", haamain_installed_version);
    }
    
    INFO(" installed");
#endif  // HAABOOT
    
    ota_init(user_repo, is_ssl);
    
    vTaskDelay(MS_TO_TICKS(2000));
    
    sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
    
    for (;;) {
        tries_count++;
        tries_partial_count = 0;
        file_size = 0;
        result = 0;
        
#ifdef HAABOOT
/*
        printf("HK data migration");
        const char magic1[] = "HAP";
        char magic[sizeof(magic1)];
        memset(magic, 0, sizeof(magic));

        if (!spiflash_read(OLD_SPIFLASH_BASE_ADDR, (byte*) magic, sizeof(magic))) {
            printf("! Read old sector");
            
        } else if (strncmp(magic, magic1, sizeof(magic1)) == 0) {
            printf("Formatting new sector 0x%x", SPIFLASH_BASE_ADDR);
            if (!spiflash_erase_sector(SPIFLASH_BASE_ADDR)) {
                printf("! Erase new sector");
            } else {
                printf("Reading data from 0x%x", OLD_SPIFLASH_BASE_ADDR);
                
                byte data[4096];
                if (!spiflash_read(OLD_SPIFLASH_BASE_ADDR, data, sizeof(data))) {
                    printf("! Read HK data");
                } else {
                    printf("Writting data to 0x%x", SPIFLASH_BASE_ADDR);
                    
                    if (!spiflash_write(SPIFLASH_BASE_ADDR, data, sizeof(data))) {
                        printf("! Write HK data to new sector");
                    } else {
                        printf("Erasing old sector 0x%x", OLD_SPIFLASH_BASE_ADDR);
                        if (!spiflash_erase_sector(OLD_SPIFLASH_BASE_ADDR)) {
                            printf("! Erase old sector");
                        } else {
                            printf("HK data is migrated");
                        }
                    }
                }
            }
        } else {
            printf("Data is already migrated");
        }
*/
        static char otamainfile[] = OTAMAINFILE;
        do {
            tries_partial_count++;
            
            void enable_setup_mode() {
                sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 1);
                ota_reboot();
            }
            
            result = ota_get_sign(user_repo, otamainfile, signature, port, is_ssl);
            if (result == 0) {
                result = ota_get_file_part(user_repo, otamainfile, BOOT1SECTOR, port, is_ssl, &file_size);
                if (result == 0) {
                    if (ota_verify_sign(BOOT1SECTOR, file_size, signature) == 0) {
                        ota_finalize_file(BOOT1SECTOR);
                        INFO("* OTAMAIN installed");
                        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
                        rboot_set_temp_rom(1);
                        ota_reboot();
                    } else {
                        enable_setup_mode();
                    }
                } else if (result < 0) {
                    ERROR("Installing OTAMAIN %i", result);
                    enable_setup_mode();
                }
            } else {
                ERROR("OTAMAIN sign %i", result);
                enable_setup_mode();
            }
        } while (tries_partial_count < TRIES_PARTIAL_COUNT_MAX);
        
#else   // HAABOOT
        if (installer_new_version) {
            free(installer_new_version);
            installer_new_version = NULL;
        }
        static char otaversionfile[] = OTAVERSIONFILE;
        installer_new_version = ota_get_version(user_repo, otaversionfile, port, is_ssl);
        
        if (installer_new_version && strcmp(installer_new_version, INSTALLER_VERSION) != 0) {
            static char otabootfile[] = OTABOOTFILE;
            do {
                tries_partial_count++;
                result = ota_get_sign(user_repo, otabootfile, signature, port, is_ssl);
                if (result == 0) {
                    result = ota_get_file_part(user_repo, otabootfile, BOOT0SECTOR, port, is_ssl, &file_size);
                    if (result == 0) {
                        if (ota_verify_sign(BOOT0SECTOR, file_size, signature) == 0) {
                            ota_finalize_file(BOOT0SECTOR);
                            INFO("* HAABOOT %s installed", installer_new_version);
                        }
                        
                        ota_reboot();
                    } else if (result < 0) {
                        ERROR("Installing HAABOOT %i", result);
                        break;
                    }
                } else {
                    ERROR("HAABOOT sign %i", result);
                    break;
                }
            } while (tries_partial_count < TRIES_PARTIAL_COUNT_MAX);
            
            break;
        } else {
            printf("HAA Installer");
            INFO(" is in last version");
        }
        
        if (haamain_new_version) {
            free(haamain_new_version);
            haamain_new_version = NULL;
        }
        static char haaversionfile[] = HAAVERSIONFILE;
        haamain_new_version = ota_get_version(user_repo, haaversionfile, port, is_ssl);
        
        if (haamain_new_version && strcmp(haamain_new_version, haamain_installed_version) != 0) {
            static char haamainfile[] = HAAMAINFILE;
            do {
                tries_partial_count++;
                result = ota_get_sign(user_repo, haamainfile, signature, port, is_ssl);
                if (result == 0) {
                    result = ota_get_file_part(user_repo, haamainfile, BOOT0SECTOR, port, is_ssl, &file_size);
                    if (result == 0) {
                        if (ota_verify_sign(BOOT0SECTOR, file_size, signature) == 0) {
                            ota_finalize_file(BOOT0SECTOR);
                            sysparam_set_string(HAAMAIN_VERSION_SYSPARAM, haamain_new_version);
                            
                            int last_config_number = 0;
                            sysparam_get_int32(LAST_CONFIG_NUMBER_SYSPARAM, &last_config_number);
                            last_config_number++;
                            
                            if (last_config_number > 65535) {
                                last_config_number = 1;
                            }
                            
                            sysparam_set_int32(LAST_CONFIG_NUMBER_SYSPARAM, last_config_number);
                            
                            INFO("* HAAMAIN %s installed", haamain_new_version);
                        }
                        
                        ota_reboot();
                    } else if (result < 0) {
                        ERROR("Installing HAAMAIN %i", result);
                        break;
                    }
                } else {
                    ERROR("HAAMAIN sign %i", result);
                    break;
                }
            } while (tries_partial_count < TRIES_PARTIAL_COUNT_MAX);
        } else {
            printf("HAAMAIN");
            INFO(" is in last version");
        }
        
        break;
#endif  // HAABOOT
        
        if (tries_count == MAX_GLOBAL_TRIES) {
            break;
        }
        
        vTaskDelay(MS_TO_TICKS(5000));
    }
    
    ota_reboot();
}

void user_init() {
    // GPIO Init
    for (int i = 0; i < 17; i++) {
        if (i == 6) {
            i += 6;
        } else if (i == 1) {
            i++;
        }
        
        gpio_enable(i, GPIO_INPUT);
    }
    
    xTaskCreate(ota_task, "OTA", (TASK_SIZE_FACTOR * 2048), NULL, (tskIDLE_PRIORITY + 1), &xHandle);
}
