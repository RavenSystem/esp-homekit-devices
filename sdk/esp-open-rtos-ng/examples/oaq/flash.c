/*
 * Write the memory resident buffers to flash.
 *
 * Copyright (C) 2016, 2017 OurAirQuality.org
 *
 * Licensed under the Apache License, Version 2.0, January 2004 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE CONTRIBUTORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS WITH THE SOFTWARE.
 *
 * The buffers are written to the flash in a ring buffer. Even after being
 * posted to the server they remain until overwritten. This supports a recovery
 * option in case of loss at the server, and other options for local access.
 *
 * In operation the code only needs to know the head and tail of the flash
 * sector ring. At start-up it needs to be able to find this position as
 * reliably as possible. A 32 bit monotonically increasing index is used to
 * allow this point to be found. This index is not expected to wrap in practical
 * usage which avoids handling wrapping of this index. To protect this index
 * against interrupted writes and bad sectors it is written twice, the second
 * time with each bit inverted. This index is located in the first 8 bytes of
 * each flash sector. This index increases for each complete sector successfully
 * written, and if a write fails then it is retried at the next sector using the
 * same index. A server receiving two sectors with the same index would use the
 * most recent sector and would need to deal with the complexity of sector
 * number wrapping, but the node attempts to invalidate a bad sector written so
 * it might never be sent to the server.
 *
 * The sectors do not have a length encoding, rather the entire sector is always
 * posted to the server. Unused bytes are filled with ones, so the node might
 * omit trailing ones when sending a sector.
 *
 * A new sector is started each time the node restarts, but to minimize
 * unnecessary writes of unused sectors a sector is not initialized until used.
 *
 */

#include "espressif/esp_common.h"

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "sysparam.h"

#include "buffer.h"
#include "leds.h"
#include "push.h"

/*
 * For a 32Mbit flash, or 4MB, there are 1024 flash sectors. The first 256 are
 * reserved here for the user code, but it might be possible to use more. The
 * last 5 sectors are reserved for the SDK code. DEFAULT_SYSPARAM_SECTORS (which
 * is 4) are used for the system parameters. That leaves 759 sectors to use to
 * store the data buffers.
 *
 * Note the current esp-open-rtos SDK binary uses only 4 sector for parameters,
 * but recent SDKs also want a sector for RF-cal data and that defaults to the
 * fifth sector from the end. So to be consistent with both this code allows for
 * 5 reserved sectors at the end of the flash.
 */
#define BUFFER_FLASH_FIRST_SECTOR 256
#define BUFFER_FLASH_NUM_SECTORS (1024 - BUFFER_FLASH_FIRST_SECTOR - 5 - DEFAULT_SYSPARAM_SECTORS)

/*
 * Read and decode a sector index, filling the index on success and returning 1,
 * otherwise returning 0.
 */
static uint32_t decode_flash_sector_index(uint16_t sector, uint32_t *index)
{
    uint32_t addr = sector * 4096;
    uint32_t data[2];
    sdk_SpiFlashOpResult res;
    res = sdk_spi_flash_read(addr, data, 8);
    if (res != SPI_FLASH_RESULT_OK) {
        return 0;
    }

    if (data[0] != (data[1] ^ 0xffffffff))
        return 0;

    *index = data[0];
    return 1;
}

/*
 * Find the sector with the largest valid index, returning 1 on success or 0 on
 * failure, and filling the sector and index on success.
 */
static int find_most_recent_sector(uint16_t *most_recent_sector,
                                   uint32_t *largest_index)
{
    uint16_t sector;
    /* Sector zero is never used for storage of the buffers, so can use zero to
     * flag 'no found valid sectors'. */
    *most_recent_sector = 0;
    /* Find the first valid sector index. */
    *largest_index = 0;
    for (sector = BUFFER_FLASH_FIRST_SECTOR;
         sector < BUFFER_FLASH_FIRST_SECTOR + BUFFER_FLASH_NUM_SECTORS;
         sector++) {
        uint32_t index;
        if (decode_flash_sector_index(sector, &index) &&
            index >= *largest_index) {
            *most_recent_sector = sector;
            *largest_index = index;
        }
    }

    if (!*most_recent_sector) {
        /* No valid sectors. */
        return 0;
    }

    /* Scan forward a little looking for another sector with the same index
     * which might happen if there was a bad sector. */
    int32_t i;
    sector = *most_recent_sector + 1;
    for (i = 0; i < 128; i++, sector++) {
        if (sector >= BUFFER_FLASH_FIRST_SECTOR + BUFFER_FLASH_NUM_SECTORS)
            sector = BUFFER_FLASH_FIRST_SECTOR;
        uint32_t index;
        if (decode_flash_sector_index(sector, &index) &&
            index == *largest_index) {
            *most_recent_sector = sector;
        }
    }

    return 1;
}

/* The head of the flash sector ring buffer. */
static uint16_t flash_sector;
/* Flag to support lazy initialization of the current sector. */
static uint8_t flash_sector_initialized;

/* For signaling and waiting for data to store to flash */
TaskHandle_t flash_data_task = NULL;

/* For protecting access to the flash state. */
SemaphoreHandle_t flash_state_sem = NULL;
static uint8_t flash_buf[4096];


/* Check if a flash sector is erased, returning 1 if erased and 0 if
 * not. */
static int flash_sector_erased(uint16_t sector)
{
    uint32_t addr = sector * 4096;
    int i;

    for (i = 0; i < 4096; i += 16) {
        uint32_t data[4];
        sdk_SpiFlashOpResult res;
        res = sdk_spi_flash_read(addr + i, data, 16);
        if (res != SPI_FLASH_RESULT_OK) {
            return 0;
        }
        if (data[0] != 0xffffffff || data[1] != 0xffffffff ||
            data[2] != 0xffffffff || data[3] != 0xffffffff) {
            return 0;
        }
    }

    return 1;
}

/* Compare a flash sector to the contents of a buffer, returning 1 if
 * equal and 0 if not. */
static int check_flash_sector(uint16_t sector, uint32_t *buf)
{
    uint32_t addr = sector * 4096;
    int i;

    for (i = 0; i < 4096; i += 16, buf += 4) {
        uint32_t data[4];
        sdk_SpiFlashOpResult res;
        res = sdk_spi_flash_read(addr + i, data, 16);
        if (res != SPI_FLASH_RESULT_OK) {
            return 0;
        }
        if (data[0] != buf[0] || data[1] != buf[1] ||
            data[2] != buf[2] || data[3] != buf[3]) {
            return 0;
        }
    }

    return 1;
}

/* Log failures. Perhaps log an event for these. */
static uint32_t flash_write_failures = 0;
static uint32_t flash_index_invalidate_failures = 0;

/* Handle a failure to erase or write to the current flash_sector. */
static void handle_flash_write_failure()
{
    flash_write_failures++;
    /* If the index is invalid then just move on. */
    uint32_t flash_index;
    if (decode_flash_sector_index(flash_sector, &flash_index)) {
        /* If the index decodes as valid then attempt to erase the sector to at
         * least invalidate the index. */
        sdk_spi_flash_erase_sector(flash_sector);
        taskYIELD();
        if (decode_flash_sector_index(flash_sector, &flash_index)) {
            /* Log the failure. */
            flash_index_invalidate_failures++;
        }
    }
    flash_sector++;
    if (flash_sector >= BUFFER_FLASH_FIRST_SECTOR + BUFFER_FLASH_NUM_SECTORS)
        flash_sector = BUFFER_FLASH_FIRST_SECTOR;
    flash_sector_initialized = 0;
}

/* A flag to note if new data might be available to help avoid a full check each
 * time. Set when new data is written and cleared when no data to post is
 * found. */
static volatile uint32_t maybe_flash_to_post = 1;

void flash_data(void *pvParameters)
{
    /*
     * Delay starting to write to flash as the power can bounce on and off if
     * the battery is low, and that would consume flash sectors quickly and
     * might even damage them?
     *
     * This delay also gives a brief period to allow use of the device without
     * it modifying the flash.
     */
    vTaskDelay(180000 / portTICK_PERIOD_MS);

    while (1) {
        xTaskNotifyWait(0, 0, NULL, 120000 / portTICK_PERIOD_MS);

        /* Try to flush all the pending buffers before waiting again. */
        while (1) {
            uint32_t start;
            uint32_t size = get_buffer_to_write(flash_buf, &start);

            if (size == 0)
                break;

            uint32_t index = flash_buf[0] | flash_buf[1] << 8 | flash_buf[2] << 16 | flash_buf[3] << 24 ;

            xSemaphoreTake(flash_state_sem, portMAX_DELAY);

            if (flash_sector_initialized) {
                /* Rewrite to the current flash_sector? */
                uint32_t flash_index;
                if (decode_flash_sector_index(flash_sector, &flash_index) &&
                    flash_index == index) {
                    /* Rewrite to the current flash_sector. Firstly try just
                     * writing from the start position. */

                    /* Round to a word aligned range. */
                    uint32_t aligned_end = (size + 3) & 0xfffffffc;
                    uint32_t aligned_start = start & 0xfffffffc;
                    uint32_t aligned_size = aligned_end - aligned_start;

                    sdk_SpiFlashOpResult res;
                    uint32_t dest_addr = (uint32_t)flash_sector * 4096 + aligned_start;
                    res = sdk_spi_flash_write(dest_addr, (uint32_t *)(flash_buf + aligned_start), aligned_size);
                    taskYIELD();
                    if (res == SPI_FLASH_RESULT_OK &&
                        check_flash_sector(flash_sector, (uint32_t *)flash_buf)) {
                        maybe_flash_to_post = 1;
                        xSemaphoreGive(flash_state_sem);
                        note_buffer_written(index, size);
                        continue;
                    }
                    
                    handle_flash_write_failure();
                } else {
                    /* Either the flash index is bad or the index being written
                     * is more recent, so move on to the next flash sector. */
                    flash_sector++;
                    if (flash_sector >= BUFFER_FLASH_FIRST_SECTOR + BUFFER_FLASH_NUM_SECTORS)
                        flash_sector = BUFFER_FLASH_FIRST_SECTOR;
                    flash_sector_initialized = 0;
                }
            }

            /* At an uninitialized flash sector, with a full buffer to write. */

            /* Retry a limited number of times on write failures. */
            int retries = 0;
            while (1) {
                /* Firstly check if it is erased. */
                if (!flash_sector_erased(flash_sector)) {
                    /* Erase the flash_sector. */
                    sdk_SpiFlashOpResult res;
                    res = sdk_spi_flash_erase_sector(flash_sector);
                    taskYIELD();
                    if (res != SPI_FLASH_RESULT_OK ||
                        !flash_sector_erased(flash_sector)) {
                        /* Just fall through and try the write, it might still
                         * work. */
                    }
                }
                /* Write the sector. */
                sdk_SpiFlashOpResult res;
                uint32_t dest_addr = (uint32_t)flash_sector * 4096;
                res = sdk_spi_flash_write(dest_addr, (uint32_t *)flash_buf, size);
                taskYIELD();
                if (res != SPI_FLASH_RESULT_OK ||
                    !check_flash_sector(flash_sector, (uint32_t *)flash_buf)) {
                    handle_flash_write_failure();
                    if (++retries > 8) {
                        /* Give up, consider it written. */
                        break;
                    }
                    continue;
                }
                /* Success. */
                flash_sector_initialized = 1;
                break;
            }
            maybe_flash_to_post = 1;
            xSemaphoreGive(flash_state_sem);
            note_buffer_written(index, size);
            /* Signal the HTTP-Post thread to re-check. */
            if (post_data_task)
                xTaskNotify(post_data_task, 0, eNoAction);
        }
    }
}

/*
 * Return 0 if there is no data to post otherwise non-zero. The caller is the
 * only task that is expected to reset this, and the flash_data_task the only
 * task to set it. Data might come in after the call, so there might in fact be
 * data to post, but if so then the caller is expected to be signaled by the
 * post_data_sem and will try again.
 */
uint32_t maybe_buffer_to_post()
{
    xSemaphoreTake(flash_state_sem, portMAX_DELAY);
    uint32_t maybe = maybe_flash_to_post;
    xSemaphoreGive(flash_state_sem);
    return maybe;
}

void clear_maybe_buffer_to_post()
{
    xSemaphoreTake(flash_state_sem, portMAX_DELAY);
    maybe_flash_to_post = 0;
    xSemaphoreGive(flash_state_sem);
    return;
}


/*
 * Request the current length of the buffer with the given index or the first
 * buffer with an index less than that requested to make it easy to get both the
 * current index and size. Returns the oldest buffer and its size if no buffers
 * match, when requesting an old index no longer in the flash. Used by the web
 * interface to give the content-length of a response, and the data post thread
 * to search for more data. The buffer might grow while posting, but the
 * response will only send the amount indicated here. This could also be used to
 * support requesting a range, allowing the web client to probe if there is more
 * data to download.
 *
 * All sectors but the current head are sealed and no more data is written to
 * them, and it is useful to know if the returned index is sealed in which case
 * it can not grow to have more data.
 *
 * The first sector with an index is the head. It is useful to know if a
 * returned index is the head, to know if it can be advanced to find more data.
 *
 * The next_index helps stepping forward because there might be gaps in the
 * index sequence and otherwise it would take some more iteration to find the
 * next in the sequence. If there is no next_index then 0xffffffff is returned -
 * the search for the next index should start from there as there might be a gap.
 */
uint32_t get_buffer_size(uint32_t requested_index, uint32_t *index, uint32_t *next_index, bool *sealed)
{
    xSemaphoreTake(flash_state_sem, portMAX_DELAY);

    /* Low sectors hold code and not data, so zero can represent invalid. */
    uint32_t last_sector = 0;
    /* The index of the last_sector. */
    uint32_t last_index = 0xffffffff;

    *next_index = 0xffffffff;

    if (flash_sector_initialized) {
        if (decode_flash_sector_index(flash_sector, index)) {
            last_sector = flash_sector;
            last_index = *index;
            if (*index <= requested_index) {
                sdk_SpiFlashOpResult res;
                res = sdk_spi_flash_read(flash_sector * 4096, (uint32_t *)flash_buf, 4096);
                if (res == SPI_FLASH_RESULT_OK) {
                    uint32_t size = 0;
                    for (size = 4096; size > 0; size--) {
                        if (flash_buf[size - 1] != 0xff)
                            break;
                    }
                    *sealed = false;
                    xSemaphoreGive(flash_state_sem);
                    return size;
                }
            }
        }
    }

    /* Search backwards from the current head flash_sector looking for the first
     * index requested. */
    uint32_t sector = flash_sector - 1;
    if (sector < BUFFER_FLASH_FIRST_SECTOR) {
        sector = BUFFER_FLASH_FIRST_SECTOR + BUFFER_FLASH_NUM_SECTORS - 1;
    }

    while (1) {
        if (decode_flash_sector_index(sector, index) &&
            /* Skip if the index increases (an error), or if this is not the
             * most recent write for this index which might occur if a flash
             * write failed and the sector was re-written. */
            *index < last_index) {
            if (*index <= requested_index) {
                sdk_SpiFlashOpResult res;
                memset(flash_buf, 0xa5, 4096);
                res = sdk_spi_flash_read(sector * 4096, (uint32_t *)flash_buf, 4096);
                if (res == SPI_FLASH_RESULT_OK) {
                    uint32_t size;
                    for (size = 4096; size > 0; size--) {
                        if (flash_buf[size - 1] != 0xff)
                            break;
                    }
                    *next_index = last_index;
                    *sealed = (sector != flash_sector);
                    xSemaphoreGive(flash_state_sem);
                    return size;
                }
                /* TODO a flash read failure above is not handled well, it would
                 * be better to skip this index rather than just moving on to
                 * the next sector which might have the same index and thus be a
                 * bad write. At least this skips noting it as a valid
                 * last_sector. */
            } else {
                /* This must be the first time this index number has been found
                 * so note this sector as it will be used if nothing else is
                 * found. */
                *next_index = last_index;
                last_sector = sector;
                last_index = *index;
            }
        }
        sector--;
        if (sector < BUFFER_FLASH_FIRST_SECTOR) {
            sector = BUFFER_FLASH_FIRST_SECTOR + BUFFER_FLASH_NUM_SECTORS - 1;
        }
        if (sector == flash_sector) {
            /* Wrapped. */
            break;
        }
    }

    if (last_sector != 0) {
        /* Found something, so return it and it's size. */
        sdk_SpiFlashOpResult res;
        res = sdk_spi_flash_read(last_sector * 4096, (uint32_t *)flash_buf, 4096);
        if (res == SPI_FLASH_RESULT_OK) {
            uint32_t size = 0;
            for (size = 4096; size > 0; size--) {
                if (flash_buf[size - 1] != 0xff)
                    break;
            }
            *index = last_index;
            *sealed = (last_sector != flash_sector);
            xSemaphoreGive(flash_state_sem);
            return size;
        }
    }

    xSemaphoreGive(flash_state_sem);

    *index = 0;
    *sealed = false;
    return 0;
}


/*
 * Return a range of the buffer with the given index. If the buffer index is no
 * longer available then return false, otherwise success, which can happen if
 * reading from the tail of the FIFO. The http response will be truncated on
 * such a failure and less than the length probe at the start of the response,
 * the http response will send a response with a content-length and the client
 * can detect the truncated response.
 *
 * The sector that the last request used is cached, so avoid a search if the
 * index has not changed.
 */
static uint32_t last_get_buffer_range_sector = 0;
static uint32_t last_get_buffer_range_index = 0xffffffff;
bool get_buffer_range(uint32_t index, uint32_t start, uint32_t end, uint8_t *buf)
{
    uint32_t i;

    xSemaphoreTake(flash_state_sem, portMAX_DELAY);

    if (last_get_buffer_range_sector && index == last_get_buffer_range_index) {
        sdk_SpiFlashOpResult res;
        res = sdk_spi_flash_read(last_get_buffer_range_sector * 4096,
                                 (uint32_t *)flash_buf, 4096);
        if (res == SPI_FLASH_RESULT_OK) {
            for (i = 0; i < end - start; i++)
                buf[i] = flash_buf[start + i];
            xSemaphoreGive(flash_state_sem);
            return true;
        }
    }

    if (flash_sector_initialized) {
        if (decode_flash_sector_index(flash_sector, &i) && i == index) {
            sdk_SpiFlashOpResult res;
            res = sdk_spi_flash_read(flash_sector * 4096, (uint32_t *)flash_buf, 4096);
            if (res == SPI_FLASH_RESULT_OK) {
                for (i = 0; i < end - start; i++)
                    buf[i] = flash_buf[start + i];
                last_get_buffer_range_sector = flash_sector;
                last_get_buffer_range_index = index;
                xSemaphoreGive(flash_state_sem);
                return true;
            }
        }
    }

    /* Search backwards from the current head flash_sector looking for the first
     * index not posted. */
    uint32_t sector = flash_sector - 1;
    if (sector < BUFFER_FLASH_FIRST_SECTOR) {
        sector = BUFFER_FLASH_FIRST_SECTOR + BUFFER_FLASH_NUM_SECTORS - 1;
    }

    while (1) {
        if (decode_flash_sector_index(sector, &i) && i == index) {
            sdk_SpiFlashOpResult res;
            res = sdk_spi_flash_read(sector * 4096, (uint32_t *)flash_buf, 4096);
            if (res == SPI_FLASH_RESULT_OK) {
                for (i = 0; i < end - start; i++)
                    buf[i] = flash_buf[start + i];
                last_get_buffer_range_sector = sector;
                last_get_buffer_range_index = index;
                xSemaphoreGive(flash_state_sem);
                return true;
            }
        }
        sector--;
        if (sector < BUFFER_FLASH_FIRST_SECTOR) {
            sector = BUFFER_FLASH_FIRST_SECTOR + BUFFER_FLASH_NUM_SECTORS - 1;
        }
        if (sector == flash_sector) {
            /* Wrapped. */
            break;
        }
    }

    last_get_buffer_range_sector = 0;
    last_get_buffer_range_index = 0xffffffff;
    xSemaphoreGive(flash_state_sem);
    return false;
}



/* Erase all the flash data and reinitialize.
 * TODO check how other code interacts with this?
 */
bool erase_flash_data() {
    /*
     * Disable buffer logging during this operation.
     */
    bool logging = set_buffer_logging(0);
    reset_dbuf();

    xSemaphoreTake(flash_state_sem, portMAX_DELAY);
    uint32_t i;
    bool success = true;

    for (i = 0; i < BUFFER_FLASH_NUM_SECTORS; i++) {
        uint32_t flash_sector = i + BUFFER_FLASH_FIRST_SECTOR;
        if (!flash_sector_erased(flash_sector)) {
            /* Erase the flash_sector. */
            sdk_SpiFlashOpResult res;
            res = sdk_spi_flash_erase_sector(flash_sector);
            taskYIELD();
            if (res != SPI_FLASH_RESULT_OK ||
                !flash_sector_erased(flash_sector)) {
                success = false;
            }
        }
    }
    /* No valid sectors, start at the first sector. */
    flash_sector = BUFFER_FLASH_FIRST_SECTOR;
    flash_sector_initialized = 0;
    maybe_flash_to_post = 0;
    last_get_buffer_range_sector = 0;
    last_get_buffer_range_index = 0xffffffff;

    // TODO reset the push task.

    xSemaphoreGive(flash_state_sem);
    set_buffer_logging(logging);
    return success;
}


uint32_t init_flash()
{
    uint32_t flash_index;

    /* Recover the head sector and index. */
    if (find_most_recent_sector(&flash_sector, &flash_index)) {
        /* Start the head at the next sector. */
        flash_sector++;
        if (flash_sector > BUFFER_FLASH_FIRST_SECTOR + BUFFER_FLASH_NUM_SECTORS)
            flash_sector = BUFFER_FLASH_FIRST_SECTOR;
        flash_index++;
    } else {
        /* No valid sectors, start at the first sector. */
        flash_sector = BUFFER_FLASH_FIRST_SECTOR;
        flash_index = 0;
    }
    flash_sector_initialized = 0;

    flash_state_sem = xSemaphoreCreateMutex();

    return flash_index;
}
