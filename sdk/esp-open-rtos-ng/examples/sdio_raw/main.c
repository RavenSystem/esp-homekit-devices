/*
 * Example of using SDIO driver
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include <esp/uart.h>
#include <espressif/esp_common.h>
#include <stdio.h>
#include <esp/spi.h>
#include <sdio/sdio.h>
#include <string.h>
#include <esp/hwrand.h>

// Blink while IO :)
#define CS_GPIO_PIN 2
// Set SPI frequency to 20MHz
#define SDIO_SPI_FREQ_DIV SPI_FREQ_DIV_20M

static const char *errors[] = {
    [SDIO_ERR_NONE]        = NULL,
    [SDIO_ERR_TIMEOUT]     = "Timeout",
    [SDIO_ERR_UNSUPPORTED] = "Unsupported",
    [SDIO_ERR_IO]          = "General I/O error",
    [SDIO_ERR_CRC]         = "CRC check failed"
};

static const char *types[] = {
    [SDIO_TYPE_UNKNOWN] = "Unknown",
    [SDIO_TYPE_MMC]     = "MMC",
    [SDIO_TYPE_SD1]     = "SD v1.x",
    [SDIO_TYPE_SD2]     = "SD v2.x",
    [SDIO_TYPE_SDHC]    = "SDHC",
};

static void dump_line(const uint8_t *data)
{
    for (uint8_t i = 0; i < 16; i ++)
        printf(" %02x", data[i]);
    printf("\n");
}

static uint8_t buffer[SDIO_BLOCK_SIZE];

#define TEST_COUNT 1000

inline static void test_read(sdio_card_t *card)
{
    printf("Simple random access test speed: ");
    uint32_t start = sdk_system_get_time();
    //sdk_system_update_cpu_freq(160);
    for (uint32_t i = 0; i < TEST_COUNT; i ++)
    {
        printf("%08u / %08u\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08", i + 1, TEST_COUNT);
        if (sdio_read_sectors(card, hwrand() % card->sectors, buffer, 1) != SDIO_ERR_NONE)
        {
            printf("Error: %d (%s)\n", card->error, errors[card->error]);
            return;
        }
    }
    uint32_t time = sdk_system_get_time() - start;
    float speed = (float)TEST_COUNT / time * 1000000;
    float stime = (time / 1000000.0) / TEST_COUNT;
    uint32_t bs = (uint32_t)speed * 512;
    printf("\nDone. Time: %u ms, speed: %.4f sectors/s, sector in %.4f s, %u bytes/s \n", time, speed, stime, bs);
}

inline static void dump_card(sdio_card_t *card)
{
    char product_name[6];
    memcpy(product_name, card->cid.bits.product_name, 5);
    product_name[5] = 0;

    printf("-------------------------------------------\n");
    printf("             SD/MMC card info\n");
    printf("-------------------------------------------\n");
    printf("%20s : %s\n", "Card type", types[card->type]);
    printf("%20s : %s\n", "CRC enabled", card->crc_enabled ? "yes" : "no");
    printf("%20s : %d\n", "512 byte sectors", card->sectors);
    printf("%20s : 0x%02x\n", "Manufacturer ID", card->cid.bits.manufacturer_id);
    printf("%20s : %c%c\n", "OEM ID", card->cid.bits.oem_id[0], card->cid.bits.oem_id[1]);
    printf("%20s : %s\n", "Product name", product_name);
    printf("%20s : %d.%d\n", "Product revision", card->cid.bits.product_rev_major, card->cid.bits.product_rev_minor);
    printf("%20s : 0x%08x\n", "Serial #", card->cid.bits.product_serial);
    printf("%20s : %02d / %04d\n", "Manufacturing date", card->cid.bits.date_month, card->cid.bits.date_year_h * 10 + card->cid.bits.date_year_l + 2000);
    printf("%20s : 0x%08x\n", "OCR", card->ocr.data);
    printf("%20s :", "CID");
    dump_line(card->cid.data);
    printf("%20s :", "CSD");
    dump_line(card->csd.data);
    test_read(card);
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n\n", sdk_system_get_sdk_version());

    while (true)
    {
        printf("\nInitializing card...\n");
        do
        {
            sdio_card_t card;
            if (sdio_init(&card, CS_GPIO_PIN, SDIO_SPI_FREQ_DIV))
            {
                printf("Error: %d (%s)\n", card.error, errors[card.error]);
                break;
            }
            dump_card(&card);
        } while (0);

        for (size_t i = 0; i < 1000; i ++)
            sdk_os_delay_us(1000);
    }
}
