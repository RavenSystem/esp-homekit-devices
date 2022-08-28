/*
 * copyright (c) Espressif System 2010
 *
 */

#ifndef __SPI_FLASH_H__
#define __SPI_FLASH_H__

#include "flashchip.h"

#ifdef	__cplusplus
extern "C" {
#endif

typedef enum {
    SPI_FLASH_RESULT_OK,
    SPI_FLASH_RESULT_ERR,
    SPI_FLASH_RESULT_TIMEOUT
} sdk_SpiFlashOpResult;

#define SPI_FLASH_SEC_SIZE      4096

uint32_t sdk_spi_flash_get_id(void);
sdk_SpiFlashOpResult sdk_spi_flash_read_status(uint32_t *status);
sdk_SpiFlashOpResult sdk_spi_flash_write_status(uint32_t status_value);

/* Erase SPI flash sector. Parameter is sector index.

   Sectors are SPI_FLASH_SEC_SIZE (4096) bytes long.
*/
sdk_SpiFlashOpResult sdk_spi_flash_erase_sector(uint16_t sec);

/* Write data to flash.

   des_addr is byte offset to write to. Should be 4-byte aligned.
   src is pointer to a buffer to read bytes from. Should be 4-byte aligned.
   size is length of buffer in bytes. Should be a multiple of 4.
*/
sdk_SpiFlashOpResult sdk_spi_flash_write(uint32_t des_addr, uint32_t *src, uint32_t size);

/* Read data from flash.

   src_addr is byte offset to read from. Should be 4-byte aligned.
   des is pointer to a buffer to read bytes into. Should be 4-byte aligned.
   size is number of bytes to read. Should be a multiple of 4.
*/
sdk_SpiFlashOpResult sdk_spi_flash_read(uint32_t src_addr, uint32_t *des, uint32_t size);

#ifdef	__cplusplus
}
#endif

#endif
