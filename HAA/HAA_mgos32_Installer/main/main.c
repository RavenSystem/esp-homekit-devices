/*
 * Home Accessory Architect Installer
 *
 * Copyright 2020-2025 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

#include <adv_logger.h>

#include "setup.h"
#include "ota.h"


// Use a standard ESP-IDF v4 bootloader binary
#if defined(CONFIG_IDF_TARGET_ESP32)
#include "bootloader_esp32.h"
#define BOOTLOADER_OFFSET       (0x1000)

#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#include "bootloader_esp32c3.h"
#define BOOTLOADER_OFFSET       (0x0000)

#elif defined(CONFIG_IDF_TARGET_ESP32C6)
#include "bootloader_esp32c6.h"
#define BOOTLOADER_OFFSET       (0x0000)

#endif


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
    gpio_reset_pin(HAA_TX_UART_DEFAULT_PIN);
    uart_set_pin(0, HAA_TX_UART_DEFAULT_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    
    adv_logger_init();
    
    INFO("MGOS2HAA");
    
    sysparam_status_t status = sysparam_init(SYSPARAMSECTOR, 0);
    if (status != SYSPARAM_OK) {
        setup_mode_reset_sysparam();
    }
    
    esp_netif_init();
    esp_event_loop_create_default();
    setup_set_esp_netif(esp_netif_create_default_wifi_sta());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    //esp_wifi_set_mode(WIFI_MODE_APSTA);
    
    set_partitions();
    
    uint8_t* flash_buffer = malloc(SPI_FLASH_SECTOR_SIZE);
    esp_flash_read(NULL, flash_buffer, BOOTLOADER_OFFSET + 0x1000, 32);
    
    if (memcmp(flash_buffer, &bootloader_bin[0x1000], 32) != 0) {
        const esp_partition_t* running_partition = esp_ota_get_running_partition();
        
        if (running_partition->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0) {
            INFO("Flashing OTA-0");
            const esp_partition_t* partition_boot0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
            const esp_partition_t* partition_boot1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
            
            for (size_t i = 0; i < partition_boot1->size; i = i + SPI_FLASH_SECTOR_SIZE) {
                esp_partition_read(partition_boot1, i, flash_buffer, SPI_FLASH_SECTOR_SIZE);
                esp_partition_erase_range(partition_boot0, i, SPI_FLASH_SECTOR_SIZE);
                esp_partition_write(partition_boot0, i, flash_buffer, SPI_FLASH_SECTOR_SIZE);
            }
        }
        
        INFO("Flashing Bootloader");
        
        uint8_t flash_config[4];
        esp_flash_read(NULL, flash_config, BOOTLOADER_OFFSET, 4);
        bootloader_bin[2] = flash_config[2];
        bootloader_bin[3] = flash_config[3];
        
        for (size_t i = 0; i < bootloader_bin_len; i = i + SPI_FLASH_SECTOR_SIZE) {
            size_t flash_size_left = bootloader_bin_len - i;
            if (flash_size_left > SPI_FLASH_SECTOR_SIZE) {
                flash_size_left = SPI_FLASH_SECTOR_SIZE;
            }
            
            esp_flash_erase_region(NULL, BOOTLOADER_OFFSET + i, SPI_FLASH_SECTOR_SIZE);
            esp_flash_write(NULL, &bootloader_bin[i], BOOTLOADER_OFFSET + i, flash_size_left);
        }
        
        INFO("Erasing config");
        setup_mode_reset_sysparam();
        
        INFO("Erasing OTA Data");
        const esp_partition_t* ota_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
        for (size_t i = 0; i < ota_partition->size; i = i + SPI_FLASH_SECTOR_SIZE) {
            esp_partition_erase_range(ota_partition, i, SPI_FLASH_SECTOR_SIZE);
        }
        
        INFO("Rebooting");
        sdk_system_restart();
    }
    
    free(flash_buffer);
    
    wifi_config_init(xHandle);
    
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
    
    INFO_NNL("Mgos2HAA Installer "INSTALLER_VERSION"\nREPO http%s://%s %i\n**** ", is_ssl ? "s" : "", user_repo, port);
    
    sysparam_set_blob(HAAMAIN_VERSION_SYSPARAM, NULL, 0);
    sysparam_compact();
    
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
        
        if (tries_count == MAX_GLOBAL_TRIES) {
            break;
        }
        
        vTaskDelay(MS_TO_TICKS(5000));
    }
    
    ota_reboot(0);
}

void user_init() {
    for (unsigned int i = 0; i < UART_NUM_MAX; i++) {
        uart_driver_delete(i);
    }
    
    xTaskCreate(ota_task, "OTA", (TASK_SIZE_FACTOR * 2048), NULL, (tskIDLE_PRIORITY + 1), &xHandle);
}
