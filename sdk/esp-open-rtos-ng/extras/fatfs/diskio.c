/*
 * FatFs integration to esp-open-rtos
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include "diskio.h"
#include <stdlib.h>
#include <stdio.h>
#include <esp/spi.h>
#include <sdio/sdio.h>
#include <string.h>
#include "ffconf.h"
#include "volumes.h"

static sdio_card_t *devices[FF_VOLUMES] = { 0 };

static const uint8_t card_types[] = {
    [SDIO_TYPE_UNKNOWN] = 0,
    [SDIO_TYPE_MMC]  = (1 << 0),
    [SDIO_TYPE_SD1]  = (1 << 1),
    [SDIO_TYPE_SD2]  = (1 << 2),
    [SDIO_TYPE_SDHC] = (1 << 3) | (1 << 2),
};

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv >= FF_VOLUMES || !devices[pdrv] || devices[pdrv]->type == SDIO_TYPE_UNKNOWN)
        return STA_NOINIT;
    return 0;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv >= FF_VOLUMES)
        return STA_NOINIT;

    // allocate descriptior
    if (!devices[pdrv])
    {
        devices[pdrv] = malloc(sizeof(sdio_card_t));
        if (!devices[pdrv])
            return STA_NOINIT;
    }

    // Init card
    if (sdio_init(devices[pdrv], f_drv_to_gpio(pdrv), _FF_HIGH_SPEED_SPI_FREQ_DIV) != SDIO_ERR_NONE)
        return STA_NOINIT;

    return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv >= FF_VOLUMES || !devices[pdrv])
        return RES_PARERR;
    if (sdio_read_sectors(devices[pdrv], sector, buff, count) != SDIO_ERR_NONE)
        return RES_ERROR;
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv >= FF_VOLUMES || !devices[pdrv])
        return RES_PARERR;
    if (sdio_write_sectors(devices[pdrv], sector, (uint8_t *)buff, count) != SDIO_ERR_NONE)
        return RES_ERROR;
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv >= FF_VOLUMES || !devices[pdrv])
        return RES_PARERR;

    uint8_t *buf8 = (uint8_t *)buff;
    uint32_t *buf32 = (uint32_t *)buff;

    switch (cmd)
    {
        case CTRL_SYNC:
            return RES_OK;
        case GET_SECTOR_COUNT:
            buf32[0] = devices[pdrv]->sectors;
            return RES_OK;
        case GET_BLOCK_SIZE:
            buf32[0] = 512;
            return RES_OK;
        case CTRL_TRIM:
            return sdio_erase_sectors(devices[pdrv], buf32[0], buf32[1]) != SDIO_ERR_NONE
                ? RES_ERROR
                : RES_OK;
        case MMC_GET_TYPE:
            buf8[0] = card_types[devices[pdrv]->type];
            return buf8[0] ? RES_OK : RES_ERROR;
        case MMC_GET_CSD:
            memcpy(buff, devices[pdrv]->csd.data, 16);
            return RES_OK;
        case MMC_GET_CID:
            memcpy(buff, devices[pdrv]->cid.data, 16);
            return RES_OK;
        case MMC_GET_OCR:
            buf32[0] = devices[pdrv]->ocr.data;
            return RES_OK;
    }
    return RES_ERROR;
}
