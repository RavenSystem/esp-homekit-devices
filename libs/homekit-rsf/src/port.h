#pragma once

#include <stdint.h>

uint32_t homekit_random();
void homekit_random_fill(uint8_t *data, size_t size);

#ifdef ESP_PLATFORM

#include "esp32_port.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "esp_flash.h"
#include "spi_flash_mmap.h"
#define SPI_FLASH_SECTOR_SIZE               SPI_FLASH_SEC_SIZE
#define spiflash_read(addr, buffer, size)   (esp_partition_read(hap_partition, (addr), (buffer), (size)) == ESP_OK)
#define spiflash_write(addr, data, size)    (esp_partition_write(hap_partition, (addr), (data), (size)) == ESP_OK)
#define spiflash_erase_sector(addr)         (esp_partition_erase_range(hap_partition, (addr), SPI_FLASH_SEC_SIZE) == ESP_OK)
#define sdk_system_restart()                esp_restart()
#define SERVER_TASK_STACK_PAIR              (8320)

#else

#include <spiflash.h>
#define ESP_OK                              (0)
#define SERVER_TASK_STACK_PAIR              (1664)
#define SERVER_TASK_STACK_NORMAL            (1280)

#endif


#define SERVER_TASK_PRIORITY                (tskIDLE_PRIORITY + 2)

void homekit_port_mdns_announce();
void homekit_port_mdns_announce_pause();

void homekit_mdns_init();
void homekit_mdns_buffer_set(const uint16_t size);
void homekit_mdns_configure_init(const char *instance_name, int port);
void homekit_mdns_add_txt(const char *key, const char *format, ...);
void homekit_mdns_configure_finalize(const uint16_t mdns_ttl, const uint16_t mdns_ttl_period);
