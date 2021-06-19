// The rboot project provides this file for making rboot fit other projects

#ifndef __RBOOT_INTEGRATION_H__
#define __RBOOT_INTEGRATION_H__
#include <stdint.h>
#include <stdbool.h>
#include <espressif/spi_flash.h>
#include <espressif/esp_system.h>
#include <FreeRTOS.h>
#include <task.h>

/***************************************************
 * Platform configuration definitions              *
 ***************************************************/

#define uint8 uint8_t
#define uint16 uint16_t
#define uint32 uint32_t
#define int32 int32_t
#define ICACHE_FLASH_ATTR
#define spi_flash_read sdk_spi_flash_read
#define spi_flash_erase_sector sdk_spi_flash_erase_sector
#define spi_flash_write sdk_spi_flash_write
#define os_malloc malloc
#define os_free free
#define system_rtc_mem_read sdk_system_rtc_mem_read
#define system_rtc_mem_write sdk_system_rtc_mem_write

#if 0
#define RBOOT_DEBUG(f_, ...) printf((f_), __VA_ARGS__)
#else
#define RBOOT_DEBUG(f_, ...)
#endif

/* Enable checksumming when writing out the config,
   so if the bootloader is built with checksumming then
   it will still work.
*/
#define BOOT_CONFIG_CHKSUM


#endif // __RBOOT_INTEGRATION_H__
