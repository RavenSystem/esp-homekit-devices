/*
 * Hardware SPI driver for MMC/SD/SDHC cards
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include <esp/gpio.h>
#include <esp/spi.h>
#include <espressif/esp_common.h>
#include "sdio.h"

#define BUS 1

#define BV(x) (1 << (x))

#define MS 1000

#define INIT_TIMEOUT_US  (2000 * MS)
#define IO_TIMEOUT_US    (500 * MS)

#define MAX_ERR_COUNT 0xff

#define R1_IDLE_STATE    0
#define R1_ERASE_RESET   1
#define R1_ILLEGAL_CMD   2
#define R1_CRC_ERR       3
#define R1_ERASE_SEQ_ERR 4
#define R1_ADDR_ERR      5
#define R1_PARAM_ERR     6
#define R1_BUSY          7
#define R2_LOCKED        8
#define R2_WPE_SKIP_LF   9
#define R2_ERROR         10
#define R2_CC_ERROR      11
#define R2_ECC_FAILED    12
#define R2_WP_VIOLATION  13
#define R2_ERASE_PARAM   14
#define R2_OUT_OF_RANGE  15

#define OCR_CCS  30
#define OCR_BUSY 31
#define OCR_SDHC (BV(OCR_CCS) | BV(OCR_BUSY))

#define TOKEN_SINGLE_TRAN  0xfe
#define TOKEN_MULTI_TRAN   0xfc
#define TOKEN_STOP_TRAN    0xfd
#define WRITE_RES_MASK     0x1f
#define WRITE_RES_OK       0x05


#ifndef SDIO_CMD25_WORKAROUND
    #define SDIO_CMD25_WORKAROUND 0
#endif


#define CMD0   0x00 // GO_IDLE_STATE - Resets the SD Memory Card
#define CMD1   0x01 // SEND_OP_COND - Sends host capacity support information
                    // and activates the card's initialization process.
#define CMD6   0x06 // SWITCH_FUNC - Checks switchable function (mode 0) and
                    // switches card function (mode 1).
#define CMD8   0x08 // SEND_IF_COND - Sends SD Memory Card interface condition
                    // that includes host supply voltage information and asks
                    // the accessed card whether card can operate in supplied
                    // voltage range.
#define CMD9   0x09 // SEND_CSD - Asks the selected card to send its
                    // card-specific data (CSD register)
#define CMD10  0x0a // SEND_CID - Asks the selected card to send its card
                    // identification (CID register)
#define CMD12  0x0c // STOP_TRANSMISSION - Forces the card to stop transmission
                    // in Multiple Block Read Operation
#define CMD13  0x0d // SEND_STATUS - Asks the selected card to send its
                    // status register.
#define CMD16  0x10 // SET_BLOCKLEN - Sets a block length (in bytes) for all
                    // following block commands (read and write) of a Standard
                    // Capacity Card. Block length of the read and write
                    // commands are fixed to 512 bytes in a High Capacity Card.
                    // The length of LOCK_UNLOCK command is set by this command
                    // in both capacity cards.
#define CMD17  0x11 // READ_SINGLE_BLOCK - Reads a block of the size selected
                    // by the SET_BLOCKLEN command.
#define CMD18  0x12 // READ_MULTIPLE_BLOCK - Continuously transfers data blocks
                    // from card to host until interrupted by a
                    // STOP_TRANSMISSION command.
#define CMD24  0x18 // WRITE_BLOCK - Writes a block of the size selected by the
                    // SET_BLOCKLEN command.
#define CMD25  0x19 // WRITE_MULTIPLE_BLOCK - Continuously writes blocks of
                    // data until ’Stop Tran’ token is sent (instead ’Start
                    // Block’).
#define CMD27  0x1b // PROGRAM_CSD - Programming of the programmable bits of
                    // the CSD.
#define CMD28  0x1c // SET_WRITE_PROT
#define CMD29  0x1d // CLR_WRITE_PROT
#define CMD32  0x20 // ERASE_WR_BLK_START - Sets the address of the first block
                    // to be erased.
#define CMD33  0x21 // ERASE_WR_BLK_END - Sets the address of the last block of
                    // the continuous range to be erased.
#define CMD38  0x26 // ERASE - Erases all previously selected blocks.
#define CMD55  0x37 // APP_CMD - Defines to the card that the next command is
                    // an application specific command rather than a standard
                    // command.
#define CMD58  0x3a // READ_OCR - Reads the OCR register of a card.
#define CMD59  0x3b // CRC_ON_OFF - Turns the CRC option on or off.

#define ACMD23 0x17 // SET_WR_BLK_ERASE_COUNT - Sets the number of write blocks
                    // to be pre-erased before writing
#define ACMD41 0x29 // SD_SEND_OP_COMD - Sends host capacity support information
                    // and activates the card's initialization process

static uint8_t crc7(const uint8_t* data, uint8_t n)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < n; i++)
    {
        uint8_t d = data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            crc <<= 1;
            if ((d & 0x80) ^ (crc & 0x80))
                crc ^= 0x09;
            d <<= 1;
        }
    }
    return (crc << 1) | 1;
}

static uint16_t crc_ccitt(const uint8_t *data, size_t n)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < n; i++)
    {
        crc = (uint8_t)(crc >> 8) | (crc << 8);
        crc ^= data[i];
        crc ^= (uint8_t)(crc & 0xff) >> 4;
        crc ^= crc << 12;
        crc ^= (crc & 0xff) << 5;
    }
    return crc;
}

#define spi_cs_low(card)  do { gpio_write(card->cs_pin, false); } while(0)
#define spi_cs_high(card) do { gpio_write(card->cs_pin, true); } while(0)
#define spi_read_byte()   (spi_transfer_8(BUS, 0xff))
#define spi_read_word()   (((uint16_t)spi_read_byte() << 8) | spi_read_byte())
#define spi_read_dword()  (((uint32_t)spi_read_byte() << 24)  | ((uint32_t)spi_read_byte() << 16) | ((uint32_t)spi_read_byte() << 8) | spi_read_byte())
#define spi_skip_word()   do { spi_read_byte(); spi_read_byte(); } while(0)
#define spi_skip_dword()  do { spi_read_byte(); spi_read_byte(); spi_read_byte(); spi_read_byte(); } while(0)

#define timeout_expired(start, len) ((uint32_t)(sdk_system_get_time() - (start)) >= (len))

inline static uint16_t spi_write_word(uint16_t word)
{
    return (spi_transfer_8(BUS, word >> 8) << 8) | spi_transfer_8(BUS, word);
}

inline static void spi_read_bytes(uint8_t *dst, size_t size)
{
    for (uint8_t *offs = dst; offs < dst + size; offs ++)
        *offs = spi_read_byte();
}

static bool wait()
{

    uint32_t start = sdk_system_get_time();
    while (spi_read_byte() != 0xff)
        if (timeout_expired(start, IO_TIMEOUT_US))
            return false;
    return true;
}

static uint8_t command(sdio_card_t *card, uint8_t cmd, uint32_t arg)
{
    uint8_t buf[6] = {
        cmd | 0x40,
        arg >> 24,
        arg >> 16,
        arg >> 8,
        arg
    };
    if (card->crc_enabled)
        buf[5] = crc7(buf, 5);
    else
        buf[5] = cmd == CMD0 ? 0x95 : 0x87;

    spi_cs_low(card);
    wait();
    spi_transfer(BUS, buf, NULL, 6, SPI_8BIT);

    uint8_t res;
    for (uint8_t i = 0; i < MAX_ERR_COUNT; i ++)
    {
        res = spi_read_byte();
        if (!(res & BV(R1_BUSY)))
            break;
    }

    /** If the response is a "busy" type (R1B), then there’s some
     * special handling that needs to be done. The card will
     * output a continuous stream of zeros, so the end of the BUSY
     * state is signaled by any nonzero response.
     */
    if (cmd == CMD12 || cmd == CMD28 || cmd == CMD29)
    {
      for (uint8_t i = 0; i < MAX_ERR_COUNT; i ++)
      {
          res = spi_read_byte();
          if (res != 0)
          {
            spi_transfer_8(BUS, 0xFF);
            return SDIO_ERR_NONE;
          }
      }
    }
    
    return res;
}

inline static uint8_t app_command(sdio_card_t *card, uint8_t cmd, uint32_t arg)
{
    command(card, CMD55, 0);
    return command(card, cmd, arg);
}

inline static sdio_error_t set_error(sdio_card_t *card, sdio_error_t err)
{
    card->error = err;
    spi_cs_high(card);
    return err;
}

static sdio_error_t read_data(sdio_card_t *card, uint8_t *dst, size_t size)
{
    uint32_t start = sdk_system_get_time();

    while (true)
    {
        if (timeout_expired(start, IO_TIMEOUT_US))
            return set_error(card, SDIO_ERR_TIMEOUT);

        uint8_t b = spi_read_byte();
        if (b == TOKEN_SINGLE_TRAN)
            break;

        if (b != 0xff)
            return set_error(card, SDIO_ERR_IO);
    }

    spi_read_bytes(dst, size);

    uint16_t crc = spi_read_word();
    if (card->crc_enabled && crc_ccitt(dst, size) != crc)
        return set_error(card, SDIO_ERR_CRC);

    return SDIO_ERR_NONE;
}

static sdio_error_t read_register(sdio_card_t *card, uint8_t cmd, void *dst)
{
    if (command(card, cmd, 0))
        return set_error(card, SDIO_ERR_IO);
    return read_data(card, dst, 16);
}

static sdio_error_t write_data_block(sdio_card_t *card, uint8_t token, uint8_t *src)
{
    if (!wait())
        return set_error(card, SDIO_ERR_TIMEOUT);
    spi_transfer_8(BUS, token);
    spi_transfer(BUS, src, NULL, SDIO_BLOCK_SIZE, SPI_8BIT);
    spi_write_word(card->crc_enabled ? crc_ccitt(src, SDIO_BLOCK_SIZE) : 0xffff);

    if ((spi_read_byte() & WRITE_RES_MASK) != WRITE_RES_OK)
        return set_error(card, SDIO_ERR_IO);

    return SDIO_ERR_NONE;
}

sdio_error_t sdio_init(sdio_card_t *card, uint8_t cs_pin, uint32_t high_freq_divider)
{
    card->cs_pin = cs_pin;
    card->type = SDIO_TYPE_UNKNOWN;

    // setup SPI at 125kHz
    spi_settings_t s = {
        .mode = SPI_MODE0,
        .freq_divider = SPI_FREQ_DIV_125K,
        .msb = true,
        .endianness = SPI_LITTLE_ENDIAN,
        .minimal_pins = true
    };
    spi_set_settings(BUS, &s);
    gpio_enable(card->cs_pin, GPIO_OUTPUT);

    uint32_t start = sdk_system_get_time();

    spi_cs_low(card);
    spi_cs_high(card);
    for (uint8_t i = 0; i < 10; i++)
        spi_read_byte();

    // Set card to the SPI idle mode
    while (command(card, CMD0, 0) != BV(R1_IDLE_STATE))
    {
        if (timeout_expired(start, INIT_TIMEOUT_US))
            return set_error(card, SDIO_ERR_TIMEOUT);
    }

    // Enable CRC
    card->crc_enabled = command(card, CMD59, 1) == BV(R1_IDLE_STATE);

    // Get card type
    while (true)
    {
        if (command(card, CMD8, 0x1aa) & BV(R1_ILLEGAL_CMD))
        {
            card->type = SDIO_TYPE_SD1;
            break;
        }
        if ((spi_read_dword() & 0xff) == 0xaa)
        {
            card->type = SDIO_TYPE_SD2;
            break;
        }

        if (timeout_expired(start, INIT_TIMEOUT_US))
            return set_error(card, SDIO_ERR_TIMEOUT);
    }

    if (card->type == SDIO_TYPE_SD1)
    {
        // SD1 or MMC3
        if (app_command(card, ACMD41, 0) > 1)
        {
            card->type = SDIO_TYPE_MMC;
            while (command(card, CMD1, 0))
                if (timeout_expired(start, INIT_TIMEOUT_US))
                    return set_error(card, SDIO_ERR_TIMEOUT);
        }
        else
        {
            while (app_command(card, ACMD41, 0))
                if (timeout_expired(start, INIT_TIMEOUT_US))
                    return set_error(card, SDIO_ERR_TIMEOUT);
        }

        if (command(card, CMD16, SDIO_BLOCK_SIZE))
            return set_error(card, SDIO_ERR_UNSUPPORTED);
    }
    else
    {
        // SD2 or SDHC
        while (app_command(card, ACMD41, BV(30)) != 0)
            if (timeout_expired(start, INIT_TIMEOUT_US))
                return set_error(card, SDIO_ERR_TIMEOUT);
    }
    // read OCR
    if (command(card, CMD58, 0))
        return set_error(card, SDIO_ERR_IO);
    card->ocr.data = spi_read_dword();

    if (card->type == SDIO_TYPE_SD2 && (card->ocr.data & OCR_SDHC) == OCR_SDHC)
        card->type = SDIO_TYPE_SDHC;

    spi_set_frequency_div(BUS, high_freq_divider);

    if (read_register(card, CMD10, &card->cid.data) != SDIO_ERR_NONE)
        return card->error;
    if (read_register(card, CMD9, &card->csd.data) != SDIO_ERR_NONE)
        return card->error;

    // Card size
    if (card->csd.v1.csd_ver == 0)
        card->sectors = (uint32_t)(((card->csd.v1.c_size_high << 10) | (card->csd.v1.c_size_mid << 2) | card->csd.v1.c_size_low) + 1)
            << (((card->csd.v1.c_size_mult_high << 1) | card->csd.v1.c_size_mult_low) + card->csd.v1.read_bl_len - 7);
    else if (card->csd.v2.csd_ver == 1)
        card->sectors = (((uint32_t)card->csd.v2.c_size_high << 16) + ((uint32_t)card->csd.v2.c_size_mid << 8) + card->csd.v2.c_size_low + 1) << 10;
    else
        return set_error(card, SDIO_ERR_UNSUPPORTED);

    return set_error(card, SDIO_ERR_NONE);
}

sdio_error_t sdio_read_sectors(sdio_card_t *card, uint32_t sector, uint8_t *dst, uint32_t count)
{
    if (!count)
        return set_error(card, SDIO_ERR_IO);

    if (card->type != SDIO_TYPE_SDHC)
        sector <<= 9;

    bool multi = count > 1;
    if (command(card, multi ? CMD18 : CMD17, sector))
        return set_error(card, SDIO_ERR_IO);

    while (count--)
    {
        if (read_data(card, dst, SDIO_BLOCK_SIZE) != SDIO_ERR_NONE)
            return card->error;
        dst += SDIO_BLOCK_SIZE;
    }

    if (multi && command(card, CMD12, 0))
        return set_error(card, SDIO_ERR_IO);

    return set_error(card, SDIO_ERR_NONE);
}

sdio_error_t sdio_write_sectors(sdio_card_t *card, uint32_t sector, uint8_t *src, uint32_t count)
{
    if (!count)
        return set_error(card, SDIO_ERR_IO);

    if (card->type != SDIO_TYPE_SDHC)
        sector <<= 9;

    bool multi = count != 1;

    // send pre-erase count
    if (multi && (card->type == SDIO_TYPE_SD1
        || card->type == SDIO_TYPE_SD2
        || card->type == SDIO_TYPE_SDHC)
        && app_command(card, ACMD23, count))
    {
        return set_error(card, SDIO_ERR_IO);
    }

#if SDIO_CMD25_WORKAROUND
    // Workaround for very old cards that don't support CMD25
    while (count--)
    {
        // single block
        if (command(card, CMD24, sector))
            return set_error(card, SDIO_ERR_IO);
        if (write_data_block(card, TOKEN_SINGLE_TRAN, src) != SDIO_ERR_NONE)
            return card->error;
        src += SDIO_BLOCK_SIZE;
    }
#else
    if (command(card, multi ? CMD25 : CMD24, sector))
        return set_error(card, SDIO_ERR_IO);

    while (count--)
    {
        if (write_data_block(card, multi ? TOKEN_MULTI_TRAN : TOKEN_SINGLE_TRAN, src) != SDIO_ERR_NONE)
            return card->error;
        src += SDIO_BLOCK_SIZE;
    }
    if (multi && command(card, CMD12, 0))
        return set_error(card, SDIO_ERR_IO);
#endif

    return set_error(card, SDIO_ERR_NONE);
}

sdio_error_t sdio_erase_sectors(sdio_card_t *card, uint32_t first, uint32_t last)
{
    if (!card->csd.v1.erase_blk_en)
    {
        uint8_t mask = (card->csd.v1.sector_size_high << 1) | card->csd.v1.sector_size_low;
        if ((first & mask) || ((last + 1) & mask))
            return set_error(card, SDIO_ERR_UNSUPPORTED);
    }

    if (card->type != SDIO_TYPE_SDHC)
    {
        first <<= 9;
        last <<= 9;
    }

    if (command(card, CMD32, first)
        || command(card, CMD33, last)
        || command(card, CMD38, 0))
    {
        return set_error(card, SDIO_ERR_IO);
    }

    return set_error(card, wait() ? SDIO_ERR_NONE : SDIO_ERR_TIMEOUT);
}
