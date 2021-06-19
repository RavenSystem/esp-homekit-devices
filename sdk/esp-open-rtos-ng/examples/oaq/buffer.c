/*
 * Particle counter data logger.
 * Memory resident (RAM) buffer support.
 *
 * Copyright (C) 2016, 2017 OurAirQuality.org
 *
 * Licensed under the Apache License, Version 2.0, January 2004 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
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
 *
 * Data is collected in 4096 byte buffers, the same size as the flash sectors,
 * and then saved to the flash storage, and then posted to a server.
 *
 * Separate tasks: 1. read the UART appending events to a ring of memory
 * resident buffers; 2. save these buffers to flash sectors; 3. HTTP-Post these
 * sectors to a server.
 *
 * Within each buffer the values may be compressed, but each buffer stands
 * alone.
 *
 * Unused bytes in the buffer are filled with ones bits to support writing to
 * flash multiple times for saving partial buffers. The buffer encoding must
 * allow recovery of the entries from these ones-filled buffers, which requires
 * that each entry have at least one zero bit.
 *
 * Events added to the buffer have a time stamp which is delta encoded. These
 * are not required to be real times and are expected to be synchronized to
 * external events such as successful posts to a server.
 */

#include "espressif/esp_common.h"
#include "espressif/esp_system.h"

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <esp/uart.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "buffer.h"
#include "config.h"
#include "leds.h"
#include "flash.h"
#include "web.h"
#include "push.h"
#include "pms.h"
#include "i2c.h"
#include "sht21.h"
#include "bmp180.h"
#include "bme280.h"
#include "ds3231.h"



#define DBUF_DATA_SIZE 4096

typedef struct {
    /* The size of the filled data bytes. */
    uint32_t size;
    /* The size that has been saved successfully. */
    uint32_t save_size;
    /* Time-stamp of the first event written to the buffer after the last save,
     * or the time of the oldest event not saved. */
    uint32_t write_time;
    /* The data. Initialized to all ones bits (0xff). The first two 32 bit words
     * are an unique index that is monotonically increasing. The second copy is
     * for redundancy and is inverted to help catch errors when saved to
     * flash. */
    uint8_t data[DBUF_DATA_SIZE];
} dbuf_t;

/*
 * The buffers are managed as a ring-buffer. If the oldest data is not saved in
 * time then it is discarded.
 */

#define NUM_DBUFS 2

static dbuf_t dbufs[NUM_DBUFS];

/* The current data is written to the dbufs_head buffer. */
static uint32_t dbufs_head;
/* The oldest buffer is the dbufs_tail, which is equal to the dbufs_head if
 * there is only one live buffer. */
static uint32_t dbufs_tail;

/* To synchronize access to the data buffers. */
static SemaphoreHandle_t dbufs_sem;

/*
 * Logging to the data buffers can be disabled by clearing this variable, and
 * this is the start of the data flow so it stops more data entering.
 */
static bool dbuf_logging_enabled = false;

/* Return the index for the buffer number. */
static uint32_t dbuf_index(uint32_t num)
{
    uint32_t *words = (uint32_t *)(dbufs[num].data);
    uint32_t index = words[0];
    return index;
}

uint32_t dbuf_head_index() {
    xSemaphoreTake(dbufs_sem, portMAX_DELAY);
    uint32_t index = dbuf_index(dbufs_head);
    xSemaphoreGive(dbufs_sem);
    return index;
}

static void set_dbuf_index(uint32_t num, uint32_t index)
{
    uint32_t *words = (uint32_t *)(dbufs[num].data);
    words[0] = index;
    words[1] = index ^ 0xffffffff;
}

static void initialize_dbuf(uint32_t num)
{
    dbuf_t *dbuf = &dbufs[num];
    dbuf->size = 0;
    dbuf->save_size = 0;
    uint32_t time = RTC.COUNTER;
    dbuf->write_time = time;
    memset(dbuf->data, 0xff, DBUF_DATA_SIZE);
}

bool get_buffer_logging() {
    return dbuf_logging_enabled;
}

bool set_buffer_logging(bool enable) {
    /* If logging is being paused then note this event before pausing. It is
     * possible that a few other log entries are added after this. */
    if (dbuf_logging_enabled && !enable) {
        uint32_t last_segment = 0;
        while (1) {
            uint32_t new_segment = dbuf_append(last_segment, DBUF_EVENT_PAUSE_LOGGING,
                                               NULL, 0, 1);
            if (new_segment == last_segment)
                break;
            last_segment = new_segment;
        }
    }

    xSemaphoreTake(dbufs_sem, portMAX_DELAY);
    bool old_value = dbuf_logging_enabled;
    dbuf_logging_enabled = enable;
    xSemaphoreGive(dbufs_sem);

    /* If logging has just been enabled then log this event along with an RTC
     * calibration. Otherwise, if the device started up with logging disabled
     * then there would be no startup event to give an RTC calibration. */
    if (!old_value && enable) {
        uint32_t restart[1];
        /* Include a RTC calibration, and average a few calls as it seems rather
         * noisy. */
        restart[0] = 0;
        for (int i = 0; i < 32; i++)
            restart[0] += sdk_system_rtc_clock_cali_proc();
        restart[0] >>= 5;
        uint32_t last_segment = 0;
        while (1) {
            uint32_t new_segment = dbuf_append(last_segment, DBUF_EVENT_START_LOGGING,
                                               (void *)restart, sizeof(restart), 1);
            if (new_segment == last_segment)
                break;
            last_segment = new_segment;
        }
    }

    return old_value;
}


/* Emit an unsigned leb128. */
uint32_t emit_leb128(uint8_t *buf, uint32_t start, uint64_t v)
{
    while (1) {
        if (v < 0x80) {
            buf[start++] = v;
            return start;
        }
        buf[start++] = (v & 0x7f) | 0x80;
        v >>= 7;
    }
}

/* Emit a signed leb128. */
uint32_t emit_leb128_signed(uint8_t *buf, uint32_t start, int64_t v)
{
    while (1) {
        if (-0x40 <= v && v <= 0x3f) {
            buf[start++] = v & 0x7f;
            return start;
        }
        buf[start++] = (v & 0x7f) | 0x80;
        v >>= 7;
    }
}

/*
 * Append an event to the buffers. This function firstly emits the common event
 * header including the event code, size, and the time stamp using the RTC
 * counter.
 *
 * Emitting the code and length here supports skipping unrecognized events.
 *
 * Emitting the time here keeps the times increasing, whereas if the caller
 * emitted the time then multiple callers might race to append their events and
 * the times might not be in order.
 *
 * When the low_res_time option is selected some of the time low bits are
 * allowed to be zeroed, effectively moving the event back in time a
 * little. This can support a more compact encoding for the time. The time is
 * only truncated when it does not cause a backward step in time since the last
 * time-stamp.
 *
 * If the caller wishes to avoid redundantly repeated entries then it should
 * implement that logic itself. That is not possible here in general as
 * dropping entries would invalidate the callers delta encoding.
 *
 * If entries are dropped due to logging being disabled then that will break the
 * callers delta encoding, so a segment restart is flagged in that case.
 *
 * The append operation might fail if there is not room, and the caller is
 * expected to retry. Each buffer stands alone, so delta encoding needs to be
 * reset for each new buffer, and if the delta encoding changes then the encoded
 * data size might change too so the caller needs to re-encode the event
 * data. The caller needs to know when the buffer has changed to reset the state
 * and to do this the segment index is passed in an if not the current segment
 * index then the append aborts and the current segment index is returned.
 */
static uint32_t current_segment;
static bool dbuf_stream_restart_required;
static int32_t last_code;
static int32_t last_size;
static uint32_t last_time;

uint32_t dbuf_append(uint32_t segment, uint16_t code, uint8_t *data, uint32_t size,
                     int low_res_time)
{
    xSemaphoreTake(dbufs_sem, portMAX_DELAY);

    if (!dbuf_logging_enabled) {
        /* An entry is being dropped, and might have been delta encoded, so note
         * that a segment restart is needed. The segment index will be advanced
         * but there is no need to advance it here now, and the callers can keep
         * using the current segment index - the output is just being
         * discarded. When the stream restarts the segment index will change and
         * callers will then need to reset their delta encoding state. */
        dbuf_stream_restart_required = true;

        xSemaphoreGive(dbufs_sem);

        /*
         * Continue to wakeup the flash_data task, even if new data is not
         * being accepted into the data buffers.
         */
        if (flash_data_task)
            xTaskNotify(flash_data_task, 0, eNoAction);

        /* Consume it to allow the caller to proceed. */
        return segment;
    }

    /* A stream restart is required. */
    if (dbuf_stream_restart_required) {
        /* Reset the prior-event state. */
        last_code = 0;
        last_size = 0;
        last_time = 0;
        /* Advance the segment index. */
        current_segment++;
        /* An entry needs to be added to the stream log now, so hijack this call
         * and the caller will retry as the segment index has advanced. If there
         * is no room for this entry then it will advance to the next buffer
         * which resets the state anyway. The segment restart flag can be
         * cleared now as all exits either log a restart event or roll over to a
         * new buffer. */
        dbuf_stream_restart_required = false;
        if (segment == current_segment) {
            printf("Error: unexpected segment index\n");
        }
        segment = current_segment;
        code = DBUF_EVENT_SEGMENT_START;
        data = NULL;
        size = 0;
        low_res_time = 1;
    }

    if (segment != current_segment) {
        /* The stream has been interrupted, so the caller must reset any delta
         * encoding state and retry. */
        segment = current_segment;
        xSemaphoreGive(dbufs_sem);
        return segment;
    }

    uint32_t time = RTC.COUNTER;
    /* Space to emit the code, size, and time. */
    uint8_t header[15];
    uint32_t header_size = 0;

    if (low_res_time) {
        /* Protect against stepping backwards in time, which would look like
         * wrapping which would be a big step forward in time. If the low bits
         * of the last_time are zero then truncating the current time low bits
         * can not step backwards. If the significant bits of the last_time and
         * current time are not equal then it is also safe. */
        if ((last_time & 0x00001fff) == 0 ||
            (last_time & 0xffffe000) != (time & 0xffffe000)) {
            /* Truncate the time, don't need all the precision. Note that the
             * time delta low bits will not necessarily be zero for this event,
             * but if the following event also uses a low_res_time then the time
             * delta low bits will be zero then. */
            time = time & 0xffffe000;
        }
    }

    /* The time is always at least delta encoded, mod32. */
    uint32_t time_delta = time - last_time;

    /* The first two bits, the two lsb, encode the header format.
     *
     * Bit 0:
     *   0 = same code and size as last event.
     *   1 = leb128 event code (two low bits removed), and size.
     *
     * Bit 1:
     *   0 = leb128 time delta.
     *   1 = leb128 truncated time delta.
     *       
     * The event code must have one zero bit in the first 5 bits to ensure that
     * the first byte always has one zero bit if there is an event, and that
     * 0xff terminates the event log.
     */

    if (code == last_code && size == last_size) {
        if ((time_delta & 0x00001fff) == 0) {
            uint64_t v = time_delta >> (13 - 2) | 2 | 0;
            header_size = emit_leb128(header, header_size, v);
        } else {
            uint64_t v = (uint64_t)time_delta << 2UL | 0 | 0;
            header_size = emit_leb128(header, header_size, v);
        }
    } else {
        if ((time_delta & 0x00001fff) == 0) {
            header_size = emit_leb128(header, header_size, (code << 2) | 2 | 1);
            header_size = emit_leb128(header, header_size, size);
            uint64_t v = time_delta >> 13;
            header_size = emit_leb128(header, header_size, v);
        } else {
            header_size = emit_leb128(header, header_size, (code << 2) | 0 | 1);
            header_size = emit_leb128(header, header_size, size);
            uint64_t v = (uint64_t)time_delta;
            header_size = emit_leb128(header, header_size, v);
        }
    }

    uint32_t total_size = header_size + size;

    /* Guard against logging data too big to fit in any buffer. */
    if (total_size > DBUF_DATA_SIZE - 8) {
        xSemaphoreGive(dbufs_sem);
        /* Consume it to clear the error. This will break delta encoding for the
         * caller, but this is an exceptional path that should not occur in
         * normal operation. */
        printf("Error: data too large to buffer?\n");
        return segment;
    }

    /* Check if there is room in the current buffer. */
    dbuf_t *head = &dbufs[dbufs_head];
    if (head->size + total_size > DBUF_DATA_SIZE) {
        /* Full, move to the next buffer. */
        uint32_t index = dbuf_index(dbufs_head) + 1;
        /* Reuse the head buffer if it is the only active buffer and its data
         * has been saved. This check prevents a saved buffer being retained
         * which would break an assumed invariant. */
        if (dbufs_head != dbufs_tail || head->size != head->save_size) {
            /* Can not reuse the head buffer. */
            dbufs_head++;
            if (dbufs_head >= NUM_DBUFS)
                dbufs_head = 0;
            if (dbufs_head == dbufs_tail) {
                /* Wrapped, discard the tail buffer. */
                dbufs_tail++;
                if (dbufs_tail >= NUM_DBUFS)
                    dbufs_tail = 0;
            }
            head = &dbufs[dbufs_head];
        }
        initialize_dbuf(dbufs_head);
        set_dbuf_index(dbufs_head, index);
        head->size = 8;
        /* Reset the prior-event state. */
        last_code = 0;
        last_size = 0;
        last_time = 0;
        /* Advance the segment index. The caller, and other callers using the
         * old segment, must reset any delta encoding state and retry. */
        segment = current_segment + 1;
        current_segment = segment;
        /* Clear the segment restart flag, as it is no longer necessary. */
        dbuf_stream_restart_required = false;
        xSemaphoreGive(dbufs_sem);
        return segment;
    }

    /* Reset the write time if this is the first real write to the buffer, or
     * the first write since the last save. This prevents an immediate or early
     * save of new content added. */
    if (head->size <= 8 || head->size == head->save_size)
        head->write_time = time;

    /* Emit the event header. */
    uint32_t i;
    for (i = 0; i < header_size; i++)
        head->data[head->size + i] = header[i];
    /* Emit the event data. */
    for (i = 0; i < size; i++)
        head->data[head->size + header_size + i] = data[i];

    head->size += total_size;

    last_code = code;
    last_size = size;
    last_time = time;

    xSemaphoreGive(dbufs_sem);

    /* Wakeup the flash_data task. */
    if (flash_data_task)
        xTaskNotify(flash_data_task, 0, eNoAction);

    return segment;
}
    
/*
 * Search for a buffer to write to flash. Fill the target buffer and return the
 * size currently used if there is something to send, otherwise return zero. If
 * some of the buffer has been successfully posted then the start of the
 * non-written elements is set. The full buffer is always copied, to get the
 * trailing ones, and because the flash write might fail and the entire buffer
 * might need to be re-written to the next flash sector.
 *
 * The buffers are always returned in the order of their index, so this starts
 * searching at the tail of the buffer FIFO, and if nothing else then see if the
 * current buffer could be usefully saved.
 *
 * A copy of the buffer is made to allow the dbufs_sem to be released quickly.
 *
 * On success note_buffer_written() should be called to allow the buffer to be
 * freed and reused, and the index is at the head of the buffer.
 *
 * It is assumed that the memory resident buffers are saved well before the RTC
 * time used here can wrap.
 */

uint32_t get_buffer_to_write(uint8_t *buf, uint32_t *start)
{
    uint32_t size = 0;

    xSemaphoreTake(dbufs_sem, portMAX_DELAY);

    if (dbufs_tail != dbufs_head) {
        dbuf_t *dbuf = &dbufs[dbufs_tail];
        if (dbuf->size > dbuf->save_size) {
            uint32_t j;
            size = dbuf->size;
            for (j = 0; j < DBUF_DATA_SIZE; j++)
                buf[j] = dbuf->data[j];
            *start = dbuf->save_size;
            xSemaphoreGive(dbufs_sem);
            return size;
        }
        xSemaphoreGive(dbufs_sem);
        return 0;
    }

    /* Otherwise check if the head buffer needs to be saved.  Don't bother
     * saving a sector with only an index. */
    dbuf_t *head = &dbufs[dbufs_head];
    if (head->size > 8 && head->size > head->save_size) {
        uint32_t delta = RTC.COUNTER - head->write_time;
        // Currently about 120 seconds.
        if (delta > 20000000) {
            uint32_t j;
            size = head->size;
            for (j = 0; j < DBUF_DATA_SIZE; j++)
                buf[j] = head->data[j];
            *start = head->save_size;
            xSemaphoreGive(dbufs_sem);
            return size;
        }
    }

    xSemaphoreGive(dbufs_sem);
    return 0;
}

/*
 * Callback to note that a buffer has been successfully written to flash
 * storage. The buffer index is passed in to locate the buffer. The size is
 * passed in to update the successfully saved size, and is the total size saved,
 * not an incremental update.
 *
 * The buffer might have been filled more and even moved from being the head to
 * a non-head buffer, so the size is used to check if all the buffer has been
 * saved and only then can it be freed. The head buffer is never freed as it
 * likely has room for more events.
 *
 * The buffer might have wrapped and been re-used, in which case it has already
 * been freed.
 */
void note_buffer_written(uint32_t index, uint32_t size)
{
    xSemaphoreTake(dbufs_sem, portMAX_DELAY);

    uint32_t i = dbufs_tail;
    while (1) {
        if (dbuf_index(i) == index)
            break;
        if (i == dbufs_head) {
            /* Did not find the index, possibly wrapped already so give up. */
            xSemaphoreGive(dbufs_sem);
            return;
        }
        if (++i >= NUM_DBUFS)
            i = 0;
    }

    /* Update the save_size */
    dbufs[i].save_size = size;

    /* Update the write_time here. More content might have been saved so this
     * might be a little late for some content, but that would only delay the
     * next write a little. If this is not a head buffer then the time is not
     * even used, rather just the save_size. */
    dbufs[i].write_time = RTC.COUNTER;

    /* Free saved buffers from the tail. */
    for (; dbufs_tail != dbufs_head; ) {
        dbuf_t *dbuf = &dbufs[dbufs_tail];
        if (dbuf->save_size == dbuf->size) {
            dbufs_tail++;
            if (dbufs_tail >= NUM_DBUFS)
                dbufs_tail = 0;
        } else {
            break;
        }
    }

    xSemaphoreGive(dbufs_sem);
    blink_blue();
}

/*
 * Reset the buffers, discarding any data in them. The current segment index is
 * is not reset here but dbuf_stream_restart_required is set.
 */
void reset_dbuf()
{
    xSemaphoreTake(dbufs_sem, portMAX_DELAY);

    dbufs_head = 0;
    dbufs_tail = 0;
    initialize_dbuf(dbufs_head);
    set_dbuf_index(dbufs_head, 0);
    dbufs[dbufs_head].size = 8;

    last_code = 0;
    last_size = 0;
    last_time = 0;
    dbuf_stream_restart_required = true;

    xSemaphoreGive(dbufs_sem);
}



void user_init(void)
{
    init_params();

    init_i2c();

    dbufs_head = 0;
    dbufs_tail = 0;
    initialize_dbuf(dbufs_head);
    uint32_t last_index = init_flash();
    set_dbuf_index(dbufs_head, last_index);
    dbufs[dbufs_head].size = 8;

    current_segment = 0;
    last_code = 0;
    last_size = 0;
    last_time = 0;

    dbufs_sem = xSemaphoreCreateMutex();

    /* Set the flag directly to avoid logging an event. */
    dbuf_logging_enabled = param_logging;

    init_blink();
    blink_red();
    blink_blue();
    blink_green();
    blink_white();

    /* Log a startup event. */
    uint32_t startup[8 + 1];
    /* Include the SDK reset info. */
    struct sdk_rst_info* reset_info = sdk_system_get_rst_info();
    memcpy(startup, reset_info, sizeof(struct sdk_rst_info));
    /* Include the initial RTC calibration, and average a few calls as it seems
     * rather noisy. */
    startup[8] = 0;
    for (int i = 0; i < 32; i++)
        startup[8] += sdk_system_rtc_clock_cali_proc();
    startup[8] >>= 5;
    uint32_t last_segment = current_segment;
    while (1) {
        uint32_t new_segment = dbuf_append(last_segment, DBUF_EVENT_ESP8266_STARTUP,
                                           (void *)startup, sizeof(startup), 1);
        if (new_segment == last_segment)
            break;
        last_segment = new_segment;
    }

    /* Start logging to the RAM buffer immediately. */
    init_pms();
    init_sht2x();
    init_bmp180();
    init_bme280();
    init_ds3231();

    init_web();
    init_post();

    xTaskCreate(&flash_data, "OAQ Flash", 196, NULL, 11, &flash_data_task);
}
