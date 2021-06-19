//////////////////////////////////////////////////
// rBoot OTA and config API for ESP8266.
// Copyright 2015 Richard A Burton
// richardaburton@gmail.com
// See license.txt for license terms.
// OTA code based on SDK sample from Espressif.
//
// esp-open-rtos additions Copyright 2016 Angus Gratton
//////////////////////////////////////////////////

#include <rboot-api.h>
#include <string.h>
//#include <c_types.h>
//#include <spi_flash.h>

// detect rtos sdk (not ideal method!)
#ifdef IRAM_ATTR
#define os_free(s)   vPortFree(s)
#define os_malloc(s) pvPortMalloc(s)
#else
#include <mem.h>
#endif

#ifdef RBOOT_INTEGRATION
#include <rboot-integration.h>
#endif

#include "rboot-api.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(BOOT_CONFIG_CHKSUM) || defined(BOOT_RTC_ENABLED)
// calculate checksum for block of data
// from start up to (but excluding) end
static uint8 calc_chksum(uint8 *start, uint8 *end) {
	uint8 chksum = CHKSUM_INIT;
	while(start < end) {
		chksum ^= *start;
		start++;
	}
	return chksum;
}
#endif

// get the rboot config
rboot_config ICACHE_FLASH_ATTR rboot_get_config(void) {
	rboot_config conf;
	spi_flash_read(BOOT_CONFIG_SECTOR * SECTOR_SIZE, (uint32*)&conf, sizeof(rboot_config));
	return conf;
}

// write the rboot config
// preserves the contents of the rest of the sector,
// so the rest of the sector can be used to store user data
// updates checksum automatically (if enabled)
bool ICACHE_FLASH_ATTR rboot_set_config(rboot_config *conf) {
	uint8 *buffer;
	buffer = (uint8*)os_malloc(SECTOR_SIZE);
	if (!buffer) {
		//os_printf("No ram!\r\n");
		return false;
	}
	
#ifdef BOOT_CONFIG_CHKSUM
	conf->chksum = calc_chksum((uint8*)conf, (uint8*)&conf->chksum);
#endif
	
	spi_flash_read(BOOT_CONFIG_SECTOR * SECTOR_SIZE, (uint32*)((void*)buffer), SECTOR_SIZE);
	memcpy(buffer, conf, sizeof(rboot_config));
	vPortEnterCritical();
	spi_flash_erase_sector(BOOT_CONFIG_SECTOR);
	vPortExitCritical();
	taskYIELD();
	vPortEnterCritical();
	//spi_flash_write(BOOT_CONFIG_SECTOR * SECTOR_SIZE, (uint32*)((void*)buffer), SECTOR_SIZE);
	spi_flash_write(BOOT_CONFIG_SECTOR * SECTOR_SIZE, (uint32*)((void*)buffer), SECTOR_SIZE);
	vPortExitCritical();	
	os_free(buffer);
	return true;
}

// get current boot rom
uint8 ICACHE_FLASH_ATTR rboot_get_current_rom(void) {
	rboot_config conf;
	conf = rboot_get_config();
	return conf.current_rom;
}

// set current boot rom
bool ICACHE_FLASH_ATTR rboot_set_current_rom(uint8 rom) {
	rboot_config conf;
	conf = rboot_get_config();
	if (rom >= conf.count) return false;
	conf.current_rom = rom;
	return rboot_set_config(&conf);
}

// create the write status struct, based on supplied start address
rboot_write_status ICACHE_FLASH_ATTR rboot_write_init(uint32 start_addr) {
	rboot_write_status status = {0};
	status.start_addr = start_addr;
	status.start_sector = start_addr / SECTOR_SIZE;
	status.last_sector_erased = status.start_sector - 1;
	//status.max_sector_count = 200;
	//os_printf("init addr: 0x%08x\r\n", start_addr);
	
	return status;
}

// function to do the actual writing to flash
// call repeatedly with more data (max len per write is the flash sector size (4k))
bool ICACHE_FLASH_ATTR rboot_write_flash(rboot_write_status *status, uint8 *data, uint16 len) {
	
	bool ret = false;
	uint8 *buffer;
	int32 lastsect;
	
	if (data == NULL || len == 0) {
		return true;
	}
	
	// get a buffer
	buffer = (uint8 *)os_malloc(len + status->extra_count);
	if (!buffer) {
		//os_printf("No ram!\r\n");
		return false;
	}

	// copy in any remaining bytes from last chunk
	memcpy(buffer, status->extra_bytes, status->extra_count);
	// copy in new data
	memcpy(buffer + status->extra_count, data, len);

	// calculate length, must be multiple of 4
	// save any remaining bytes for next go
	len += status->extra_count;
	status->extra_count = len % 4;
	len -= status->extra_count;
	memcpy(status->extra_bytes, buffer + len, status->extra_count);

	// check data will fit
	//if (status->start_addr + len < (status->start_sector + status->max_sector_count) * SECTOR_SIZE) {

		// erase any additional sectors needed by this chunk
		lastsect = ((status->start_addr + len) - 1) / SECTOR_SIZE;
		while (lastsect > status->last_sector_erased) {
			status->last_sector_erased++;
			spi_flash_erase_sector(status->last_sector_erased);
		}

		// write current chunk
		//os_printf("write addr: 0x%08x, len: 0x%04x\r\n", status->start_addr, len);
		if (spi_flash_write(status->start_addr, (uint32 *)((void*)buffer), len) == SPI_FLASH_RESULT_OK) {
			ret = true;
			status->start_addr += len;
		}
	//}

	os_free(buffer);
	return ret;
}

#ifdef BOOT_RTC_ENABLED
bool ICACHE_FLASH_ATTR rboot_get_rtc_data(rboot_rtc_data *rtc) {
	if (system_rtc_mem_read(RBOOT_RTC_ADDR, rtc, sizeof(rboot_rtc_data))) {
		return (rtc->chksum == calc_chksum((uint8*)rtc, (uint8*)&rtc->chksum));
	}
	return false;
}

bool ICACHE_FLASH_ATTR rboot_set_rtc_data(rboot_rtc_data *rtc) {
	// calculate checksum
	rtc->chksum = calc_chksum((uint8*)rtc, (uint8*)&rtc->chksum);
	return system_rtc_mem_write(RBOOT_RTC_ADDR, rtc, sizeof(rboot_rtc_data));
}

bool ICACHE_FLASH_ATTR rboot_set_temp_rom(uint8 rom) {
	rboot_rtc_data rtc;
	// invalid data in rtc?
	if (!rboot_get_rtc_data(&rtc)) {
		// set basics
		rtc.magic = RBOOT_RTC_MAGIC;
		rtc.last_mode = MODE_STANDARD;
		rtc.last_rom = 0;
	}
	// set next boot to temp mode with specified rom
	rtc.next_mode = MODE_TEMP_ROM;
	rtc.temp_rom = rom;

	return rboot_set_rtc_data(&rtc);
}

bool ICACHE_FLASH_ATTR rboot_get_last_boot_rom(uint8 *rom) {
	rboot_rtc_data rtc;
	if (rboot_get_rtc_data(&rtc)) {
		*rom = rtc.last_rom;
		return true;
	}
	return false;
}

bool ICACHE_FLASH_ATTR rboot_get_last_boot_mode(uint8 *mode) {
	rboot_rtc_data rtc;
	if (rboot_get_rtc_data(&rtc)) {
		*mode = rtc.last_mode;
		return true;
	}
	return false;
}
#endif

/* NOTE: Functions below here were added for esp-open-rtos only */

uint32_t rboot_get_slot_offset(uint8_t slot) {
    rboot_config conf;
    conf = rboot_get_config();
    if (slot >= conf.count) return (uint32_t)-1;
    return conf.roms[slot];
}

/* Structures for parsing the rboot OTA image format */
typedef struct __attribute__((packed)) {
    uint8_t magic;
    uint8_t section_count;
    uint8_t val[2]; /* flash size & speed when placed @ offset 0, I thik ignored otherwise */
    uint32_t entrypoint;
} image_header_t;

typedef struct __attribute__((packed)) {
    uint32_t load_addr;
    uint32_t length;
} section_header_t;

#define ROM_MAGIC_OLD 0xe9
#define ROM_MAGIC_NEW 0xea

bool rboot_verify_image(uint32_t initial_offset, uint32_t *image_length, const char **error_message)
{
    uint32_t offset = initial_offset;
    const char *error = NULL;
    RBOOT_DEBUG("rboot_verify_image: verifying image at 0x%08x\n", initial_offset);
    if(offset % 4) {
        error = "Unaligned flash offset";
        goto fail;
    }

    /* sanity limit on how far we can read */
    uint32_t end_limit = offset + 0x100000;
    image_header_t image_header __attribute__((aligned(4)));
    if(sdk_spi_flash_read(offset, (uint32_t *)&image_header, sizeof(image_header_t))) {
        error = "Flash fail";
        goto fail;
    }

    offset += sizeof(image_header_t);

    if(image_header.magic != ROM_MAGIC_OLD && image_header.magic != ROM_MAGIC_NEW) {
        error = "Missing initial magic";
        goto fail;
    }

    bool is_new_header = (image_header.magic == ROM_MAGIC_NEW); /* a v1.2/rboot header, so expect a v1.1 header after the initial section */

    int remaining_sections = image_header.section_count;

    uint8_t checksum = CHKSUM_INIT;

    while(remaining_sections > 0 && offset < end_limit)
    {
        /* read section header */
        section_header_t header __attribute__((aligned(4)));
        if(sdk_spi_flash_read(offset, (uint32_t *)&header, sizeof(section_header_t))) {
            error = "Flash fail";
            goto fail;
        }

        RBOOT_DEBUG("Found section @ 0x%08x (abs 0x%08x) length %d load 0x%08x\n", offset-initial_offset, offset, header.length, header.load_addr);
        offset += sizeof(section_header_t);

        if(header.length+offset > end_limit) {
            break; /* sanity check: will reading section take us off end of expected flashregion? */
        }

        if(header.length % 4) {
            error = "Header length not modulo 4";
            goto fail;
        }

        if(!is_new_header) {
            /* Add individual data of the section to the checksum. */
            char chunk[16] __attribute__((aligned(4)));
            for(int i = 0; i < header.length; i++) {
                if(i % sizeof(chunk) == 0)
                    sdk_spi_flash_read(offset+i, (uint32_t *)chunk, sizeof(chunk));
                checksum ^= chunk[i % sizeof(chunk)];
            }
        }

        offset += header.length;
        /* pad section to 4 byte align */
        offset = (offset+3) & ~3;

        remaining_sections--;

        if(is_new_header) {
            /* pad to a 16 byte offset */
            offset = (offset+15) & ~15;

            /* expect a v1.1 header here at start of "real" sections */
            sdk_spi_flash_read(offset, (uint32_t *)&image_header, sizeof(image_header_t));
            offset += sizeof(image_header_t);
            if(image_header.magic != ROM_MAGIC_OLD) {
                error = "Bad second magic";
                goto fail;
            }
            remaining_sections = image_header.section_count;
            is_new_header = false;
        }
    }

    if(remaining_sections > 0) {
        error = "Image truncated";
        goto fail;
    }

    /* add a byte for the image checksum (actually comes after the padding) */
    offset++;
    /* pad the image length to a 16 byte boundary */
    offset = (offset+15) & ~15;

    uint32_t read_checksum;
    sdk_spi_flash_read(offset-1, &read_checksum, 1);
    if((uint8_t)read_checksum != checksum) {
        error = "Invalid checksum";
        goto fail;
    }

    RBOOT_DEBUG("rboot_verify_image: verified expected 0x%08x bytes.\n", offset - initial_offset);

    if(image_length)
        *image_length = offset - initial_offset;

    return true;

 fail:
    if(error_message)
        *error_message = error;
    if(error) {
        printf("%s: %s\n", __func__, error);
    }
    if(image_length)
        *image_length = offset - initial_offset;
    return false;
}

bool rboot_digest_image(uint32_t offset, uint32_t image_length, rboot_digest_update_fn update_fn, void *update_ctx)
{
    uint8_t buf[32] __attribute__((aligned(4)));
    for(int i = 0; i < image_length; i += sizeof(buf)) {
        if(sdk_spi_flash_read(offset+i, (uint32_t *)buf, sizeof(buf)))
            return false;
        uint32_t digest_len = sizeof(buf);
        if(i + digest_len  > image_length)
            digest_len = image_length - i;
        update_fn(update_ctx, buf, digest_len);
    }
    return true;
}

#ifdef __cplusplus
}
#endif
