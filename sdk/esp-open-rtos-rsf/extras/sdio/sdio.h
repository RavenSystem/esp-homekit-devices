/*
 * Hardware SPI driver for MMC/SD/SDHC cards
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#ifndef _EXTRAS_SDIO_H_
#define _EXTRAS_SDIO_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "sdio_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDIO_BLOCK_SIZE 512

typedef enum {
    SDIO_ERR_NONE = 0,
    SDIO_ERR_TIMEOUT,
    SDIO_ERR_UNSUPPORTED,
    SDIO_ERR_IO,
    SDIO_ERR_CRC
} sdio_error_t;

typedef enum {
    SDIO_TYPE_UNKNOWN = 0,
    SDIO_TYPE_MMC,
    SDIO_TYPE_SD1,
    SDIO_TYPE_SD2,
    SDIO_TYPE_SDHC
} sdio_card_type_t;

/**
 * SD card descriptor
 */
typedef struct
{
    sdio_error_t     error;        //!< Last operation result
    uint8_t          cs_pin;       //!< Chip Select GPIO pin
    sdio_card_type_t type;         //!< Card type
    bool             crc_enabled;  //!< True if CRC enabled for IO
    sdio_ocr_t       ocr;          //!< OCR register
    sdio_csd_t       csd;          //!< CSD register
    sdio_cid_t       cid;          //!< CID register
    uint32_t         sectors;      //!< Card size in 512 byte sectors
} sdio_card_t;

/**
 * \brief Init SD card
 * Device descriptor (registers, sectors count and so on) will be filled during initialization
 * SPI_FREQ_DIV_40M is good for modern SD cards, older SD can use SPI_FREQ_DIV_20M and lower
 * \param card Pointer to the device descriptor
 * \param cs_pin GPIO pin used for CS
 * \param high_freq_divider SPI bus frequency divider
 * \return Operation result
 */
sdio_error_t sdio_init(sdio_card_t *card, uint8_t cs_pin, uint32_t high_freq_divider);

/**
 * \brief Read 512 byte sectors from SD card
 * \param card Pointer to the device descriptor
 * \param sector Start sector
 * \param dst Receive buffer
 * \param count Number of sectors to read
 * \return Operation result
 */
sdio_error_t sdio_read_sectors(sdio_card_t *card, uint32_t sector, uint8_t *dst, uint32_t count);

/**
 * \brief Write 512 byte sectors to SD card
 * \param card Pointer to the device descriptor
 * \param sector Start sector
 * \param src Data to write
 * \param count Number of sectors to read
 * \return Operation result
 */
sdio_error_t sdio_write_sectors(sdio_card_t *card, uint32_t sector, uint8_t *src, uint32_t count);

/**
 * \brief Erase 512 byte sectors from SD card
 * \param card Pointer to the device descriptor
 * \param first First sector to erase
 * \param last Last sector to erase
 * \return Operation result
 */
sdio_error_t sdio_erase_sectors(sdio_card_t *card, uint32_t first, uint32_t last);


#ifdef __cplusplus
}
#endif

#endif /* _EXTRAS_SDIO_H_ */
