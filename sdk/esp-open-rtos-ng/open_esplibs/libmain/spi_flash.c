/* Recreated Espressif libmain os_cpu_o contents.

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE
*/
#include "open_esplibs.h"
#if OPEN_LIBMAIN_SPI_FLASH
// The contents of this file are only built if OPEN_LIBMAIN_SPI_FLASH is set to true

#include "FreeRTOS.h"
#include "common_macros.h"
#include "esp/spi_regs.h"
#include "esp/rom.h"
#include "sdk_internal.h"
#include "espressif/spi_flash.h"

sdk_flashchip_t sdk_flashchip = {
    0x001640ef,      // device_id
    4 * 1024 * 1024, // chip_size
    65536,           // block_size
    4096,            // sector_size
    256,             // page_size
    0x0000ffff,      // status_mask
};

// NOTE: This routine appears to be completely unused in the SDK

int IRAM sdk_SPIReadModeCnfig(uint32_t mode) {
    uint32_t ctrl_bits;

    SPI(0).CTRL0 &= ~(SPI_CTRL0_FASTRD_MODE | SPI_CTRL0_DOUT_MODE | SPI_CTRL0_QOUT_MODE | SPI_CTRL0_DIO_MODE | SPI_CTRL0_QIO_MODE);
    if (mode == 0) {
        ctrl_bits = SPI_CTRL0_FASTRD_MODE | SPI_CTRL0_QIO_MODE;
    } else if (mode == 1) {
        ctrl_bits = SPI_CTRL0_FASTRD_MODE | SPI_CTRL0_QOUT_MODE;
    } else if (mode == 2) {
        ctrl_bits = SPI_CTRL0_FASTRD_MODE | SPI_CTRL0_DIO_MODE;
    } else if (mode == 3) {
        ctrl_bits = SPI_CTRL0_FASTRD_MODE | SPI_CTRL0_DOUT_MODE;
    } else if (mode == 4) {
        ctrl_bits = SPI_CTRL0_FASTRD_MODE;
    } else { 
        ctrl_bits = 0;
    }
    if (mode == 0 || mode == 1) {
        Enable_QMode(&sdk_flashchip);
    } else {
        Disable_QMode(&sdk_flashchip);
    }
    SPI(0).CTRL0 |= ctrl_bits;
    return 0;
}

sdk_SpiFlashOpResult IRAM sdk_SPIWrite(uint32_t des_addr, uint32_t *src_addr, uint32_t size) {
    uint32_t first_page_portion;
    uint32_t pos;
    uint32_t full_pages;
    uint32_t bytes_remaining;

    if (des_addr + size <= sdk_flashchip.chip_size) {
        first_page_portion = sdk_flashchip.page_size - (des_addr % sdk_flashchip.page_size);
        if (size < first_page_portion) {
            if (SPI_page_program(&sdk_flashchip, des_addr, src_addr, size)) {
                return SPI_FLASH_RESULT_ERR;
            } else {
                return SPI_FLASH_RESULT_OK;
            }
        }
    } else {
       return SPI_FLASH_RESULT_ERR;
    }
    if (SPI_page_program(&sdk_flashchip, des_addr, src_addr, first_page_portion)) {
        return SPI_FLASH_RESULT_ERR;
    }
    pos = first_page_portion;
    bytes_remaining = size - first_page_portion;
    full_pages = bytes_remaining / sdk_flashchip.page_size;
    if (full_pages) {
        for (int i = 0; i != full_pages; i++) {
            if (SPI_page_program(&sdk_flashchip, des_addr + pos, src_addr + (pos / 4), sdk_flashchip.page_size)) {
                return SPI_FLASH_RESULT_ERR;
            }
            pos += sdk_flashchip.page_size;
        }
        bytes_remaining = size - pos;
    }
    if (SPI_page_program(&sdk_flashchip, des_addr + pos, src_addr + (pos / 4), bytes_remaining)) {
        return SPI_FLASH_RESULT_ERR;
    }
    return SPI_FLASH_RESULT_OK;
}

sdk_SpiFlashOpResult IRAM sdk_SPIRead(uint32_t src_addr, uint32_t *des_addr, uint32_t size) {
    if (SPI_read_data(&sdk_flashchip, src_addr, des_addr, size)) {
        return SPI_FLASH_RESULT_ERR;
    } else {
        return SPI_FLASH_RESULT_OK;
    }
}

sdk_SpiFlashOpResult IRAM sdk_SPIEraseSector(uint16_t sec) {
    if (sec >= sdk_flashchip.chip_size / sdk_flashchip.sector_size) {
        return SPI_FLASH_RESULT_ERR;
    }
    if (SPI_write_enable(&sdk_flashchip)) {
        return SPI_FLASH_RESULT_ERR;
    }
    if (SPI_sector_erase(&sdk_flashchip, sdk_flashchip.sector_size * sec)) {
        return SPI_FLASH_RESULT_ERR;
    }
    return SPI_FLASH_RESULT_OK;
}

uint32_t IRAM sdk_spi_flash_get_id(void) {
    uint32_t result;

    portENTER_CRITICAL();
    Cache_Read_Disable();
    Wait_SPI_Idle(&sdk_flashchip);
    SPI(0).W[0] = 0;
    SPI(0).CMD = SPI_CMD_READ_ID;
    while (SPI(0).CMD != 0) {}
    result = SPI(0).W[0] & 0x00ffffff;
    Cache_Read_Enable(0, 0, 1);
    portEXIT_CRITICAL();
    return result;
}

sdk_SpiFlashOpResult IRAM sdk_spi_flash_read_status(uint32_t *status) {
    sdk_SpiFlashOpResult result;

    portENTER_CRITICAL();
    Cache_Read_Disable();
    result = SPI_read_status(&sdk_flashchip, status);
    Cache_Read_Enable(0, 0, 1);
    portEXIT_CRITICAL();
    return result;
}

sdk_SpiFlashOpResult IRAM sdk_spi_flash_write_status(uint32_t status) {
    sdk_SpiFlashOpResult result;

    portENTER_CRITICAL();
    Cache_Read_Disable();
    result = SPI_write_status(&sdk_flashchip, status);
    Cache_Read_Enable(0, 0, 1);
    portEXIT_CRITICAL();
    return result;
}

sdk_SpiFlashOpResult IRAM sdk_spi_flash_erase_sector(uint16_t sec) {
    sdk_SpiFlashOpResult result;

    portENTER_CRITICAL();
    Cache_Read_Disable();
    result = sdk_SPIEraseSector(sec);
    Cache_Read_Enable(0, 0, 1);
    portEXIT_CRITICAL();
    return result;
}

sdk_SpiFlashOpResult IRAM sdk_spi_flash_write(uint32_t des_addr, uint32_t *src_addr, uint32_t size) {
    sdk_SpiFlashOpResult result;

    if (!src_addr) {
        return SPI_FLASH_RESULT_ERR;
    }
    if (size & 3) {
        size = (size & ~3) + 4;
    }
    portENTER_CRITICAL();
    Cache_Read_Disable();
    result = sdk_SPIWrite(des_addr, src_addr, size);
    Cache_Read_Enable(0, 0, 1);
    portEXIT_CRITICAL();
    return result;
}

sdk_SpiFlashOpResult IRAM sdk_spi_flash_read(uint32_t src_addr, uint32_t *des_addr, uint32_t size) {
    sdk_SpiFlashOpResult result;

    if (!des_addr) {
        return SPI_FLASH_RESULT_ERR;
    }
    portENTER_CRITICAL();
    Cache_Read_Disable();
    result = sdk_SPIRead(src_addr, des_addr, size);
    Cache_Read_Enable(0, 0, 1);
    portEXIT_CRITICAL();
    return result;
}

#endif /* OPEN_LIBMAIN_SPI_FLASH */
