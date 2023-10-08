/*
 * Home Accessory Architect Installer
 *
 * Copyright 2020-2023 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef ESP_PLATFORM

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "errno.h"
#include "esp32_port.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#else   // ESP_OPEN_RTOS

#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <sysparam.h>
#include <FreeRTOS.h>
#include <task.h>
#include <spiflash.h>

#endif

#include <adv_logger.h>

#include "setup.h"
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
#ifdef ESP_PLATFORM
    
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    uart_driver_install(0, SDK_UART_BUFFER_SIZE, 0, 0, NULL, 0);
    uart_param_config(0, &uart_config);
    gpio_reset_pin(1);
    uart_set_pin(0, 1, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    
#else
    
    gpio_disable(1);
    iomux_set_pullup_flags(5, 0);
    iomux_set_function(5, IOMUX_GPIO1_FUNC_UART0_TXD);
    uart_set_baud(0, 115200);
    
#endif
    
    adv_logger_init();
    
    sysparam_status_t status = sysparam_init(SYSPARAMSECTOR, 0);
    if (status != SYSPARAM_OK) {
        setup_mode_reset_sysparam();
    }
    
#ifdef ESP_PLATFORM
    esp_netif_init();
    esp_event_loop_create_default();
    setup_set_esp_netif(esp_netif_create_default_wifi_sta());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    //esp_wifi_set_mode(WIFI_MODE_APSTA);
    
    const esp_partition_t* running_partition = esp_ota_get_running_partition();
#ifdef HAABOOT
    if (running_partition->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0) {
        ota_reboot(0);
    }
#else
    if (running_partition->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_1 &&
        running_partition->subtype != ESP_PARTITION_SUBTYPE_APP_FACTORY) {
        ota_reboot(1);
    }
#endif // HAABOOT
    
    //free(running);
#endif
    
    wifi_config_init("HAA", xHandle);
    
    vTaskDelete(NULL);
}

void ota_task(void *arg) {
    xTaskCreate(init_task, "INI", (TASK_SIZE_FACTOR * 512), NULL, (tskIDLE_PRIORITY + 2), NULL);
    
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
    
    INFO_NNL("HAA Installer "INSTALLER_VERSION"\nREPO http%s://%s %i\n**** ", is_ssl ? "s" : "", user_repo, port);
    
#ifdef HAABOOT
    INFO("HAABOOT");
    
    sysparam_set_blob(HAAMAIN_VERSION_SYSPARAM, NULL, 0);
    sysparam_compact();
    
#else   // HAABOOT
    INFO_NNL("OTAMAIN\nHAAMAIN ");
    
    status = sysparam_get_string(HAAMAIN_VERSION_SYSPARAM, &haamain_installed_version);
    if (status != SYSPARAM_OK || haamain_installed_version[0] == 0) {
        haamain_installed_version = strdup("not");
    }
        
    INFO_NNL("%s", haamain_installed_version);
    
    INFO(" installed");
#endif  // HAABOOT
    
/*
#ifdef ESP_PLATFORM
    // Show ESP-IDF Partitions
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    
    for (; it != NULL; it = esp_partition_next(it)) {
        const esp_partition_t *part = esp_partition_get(it);
        INFO("Partition '%s' at 0x%02X, Size 0x%02X, Type 0x%02X, Sub 0x%02X", part->label, part->address, part->size, part->type, part->subtype);
    }
    
    esp_partition_iterator_release(it);
#endif
*/
    
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
                ota_reboot(0);
            }
            
            result = ota_get_sign(user_repo, otamainfile, signature, port, is_ssl);
            if (result == 0) {
                result = ota_get_file_part(user_repo, otamainfile, BOOT1SECTOR, port, is_ssl, &file_size);
                if (result == 0) {
                    if (ota_verify_sign(BOOT1SECTOR, file_size, signature) == 0) {
                        ota_finalize_file(BOOT1SECTOR);
                        INFO("* OTAMAIN installed");
                        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
                        ota_reboot(1);
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
                        
                        ota_reboot(0);
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
            INFO_NNL("HAA Installer");
            INFO(" no new version found");
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
                            
                            int32_t last_config_number = 0;
                            sysparam_get_int32(LAST_CONFIG_NUMBER_SYSPARAM, &last_config_number);
                            last_config_number++;
                            
                            if (last_config_number > 65535) {
                                last_config_number = 1;
                            }
                            
                            sysparam_set_int32(LAST_CONFIG_NUMBER_SYSPARAM, last_config_number);
                            
                            INFO("* HAAMAIN %s installed", haamain_new_version);
                        }
                        
                        ota_reboot(0);
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
            INFO_NNL("HAAMAIN");
            INFO(" no new version found");
        }
        
        break;
#endif  // HAABOOT
        
        if (tries_count == MAX_GLOBAL_TRIES) {
            break;
        }
        
        vTaskDelay(MS_TO_TICKS(5000));
    }
    
    ota_reboot(0);
}

void user_init() {
// GPIO Init
    
#ifdef ESP_PLATFORM
    
/*
#if defined(CONFIG_IDF_TARGET_ESP32)
#define FIRST_SPI_GPIO  (6)

#elif defined(CONFIG_IDF_TARGET_ESP32C2) \
    || defined(CONFIG_IDF_TARGET_ESP32C3)
#define FIRST_SPI_GPIO  (12)

#elif defined(CONFIG_IDF_TARGET_ESP32S2) \
    || defined(CONFIG_IDF_TARGET_ESP32S3)
#define FIRST_SPI_GPIO  (22)

#endif

#if defined(CONFIG_IDF_TARGET_ESP32S2)
#define SPI_GPIO_COUNT  (11)
    
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define SPI_GPIO_COUNT  (16)
    
#else
#define SPI_GPIO_COUNT  (6)
    
#endif
    
    for (unsigned int i = 0; i < GPIO_NUM_MAX; i++) {
        if (i == FIRST_SPI_GPIO) {
            i += SPI_GPIO_COUNT;
#if defined(CONFIG_IDF_TARGET_ESP32)
        } else if (i == 24) {
            i ++;
        } else if (i == 28) {
            i += 4;
#endif
        } else if (i == 1) {
            i += 1;
        }
        
        gpio_reset_pin(i);
        gpio_set_direction(i, GPIO_MODE_DISABLE);
    }
*/
    for (unsigned int i = 0; i < UART_NUM_MAX; i++) {
        uart_driver_delete(i);
    }
    
#else // ESP-OPEN-RTOS
    
    for (unsigned int i = 0; i < MAX_GPIOS; i++) {
        if (i == 6) {
            i += 6;
        }
        
        gpio_enable(i, GPIO_INPUT);
    }
    
#endif
    
    xTaskCreate(ota_task, "OTA", (TASK_SIZE_FACTOR * 2048), NULL, (tskIDLE_PRIORITY + 1), &xHandle);
}
