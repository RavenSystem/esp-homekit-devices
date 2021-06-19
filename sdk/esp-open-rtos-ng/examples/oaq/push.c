/*
 * HTTP-Post the flash sectors to a server.
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
 */
#include "espressif/esp_common.h"

#include <unistd.h>
#include <string.h>
#include <time.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "buffer.h"
#include "config.h"
#include "flash.h"
#include "sha3.h"
#include "ds3231.h"
#include "leds.h"

#include "config.h"

#include "wificfg/wificfg.h"


/* For signaling and waiting for data to post. */
TaskHandle_t post_data_task = NULL;

/*
 * A single buffer is allocated to hold the HTTP data to be sent and it is large
 * enough for the HTTP header plus the content including a signature suffix. The
 * content is located at a fixed position into the buffer and word aligned so
 * the flash data can be copied directly to this buffer. For the computation of
 * a MAC-SHA3 signature the key prefix is copied before the data so the prefix
 * allocation needs to be large enough for this too.
 */

#define SIGNATURE_SIZE 28

 /* At least the key_size of 287 bytes, round up gives an alignment 32. */
#define PREFIX_SIZE 288

/*
 * Sectors are posted in chunks, to limit buffer size here. FIPS202
 * SHA3_224 naturally works in block sizes of 144 bytes, so choose two
 * of these blocks gives a size of 288 bytes.
 */
#define CHUNK_SIZE 288

/* Total size here is 620 bytes. */
#define POST_BUFFER_SIZE (PREFIX_SIZE + 4 + 4 + 4 + 4 + CHUNK_SIZE + SIGNATURE_SIZE)
static uint8_t *post_buf;

#define MAX_HOLD_OFF_TIME 1800000 /* 30 minutes */

static void post_data(void *pvParameters)
{
    uint32_t last_segment = 0;
    uint32_t last_recv_sec = 0;

    /*
     * A retry hold-off time in msec. Reset to zero upon a success and otherwise
     * increased on each retry. This is intended avoid loading the network and
     * server with retries if there is an error processing a post request.
     */
    uint32_t hold_off_time = 0;

    /*
     * Can not flag every index that has been sent and how much, so do a scan
     * and keep the head state: the index currently being pushed or last pushed,
     * the total size of that index, and the amount that has been push, and flag
     * if it was known to have been sealed and not open to being extended.
     */
    uint32_t index_being_pushed = 0;
    uint32_t index_size_being_pushed = 0;
    uint32_t index_size_pushed = 0;
    bool index_being_pushed_sealed = false;
    uint32_t index_being_pushed_next_index = 0xffffffff;

    while (1) {
        xTaskNotifyWait(0, 0, NULL, 120000 / portTICK_PERIOD_MS);

        /* Try to flush all the pending buffers before waiting again. */
        while (1) {
            int j;

            /* Lightweight check if there is anything to post. */
            if (!maybe_buffer_to_post())
                break;

            /* Hold off if retrying. */
            if (hold_off_time > MAX_HOLD_OFF_TIME)
                hold_off_time = MAX_HOLD_OFF_TIME;
            vTaskDelay(hold_off_time / portTICK_PERIOD_MS);
            hold_off_time += (hold_off_time >> 2) + 1000;

            /*
             * Wait until connected, and try connecting to the server before
             * requesting the actual data to post as this might take some time
             * to succeed and by then there might be much more data to
             * send. Also a time is sent and the intention is that it is as
             * close to the time posted as possible and it can not be patched in
             * just before sending as it is part of the signed message.
             */
            
            while (1) {
                uint8_t connect_status = sdk_wifi_station_get_connect_status();
                if (connect_status == STATION_GOT_IP)
                    break;
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }

            /*
             * Notifty wificfg to disable the AP interface on the next restart
             * if that option is enabled.
             */
            wificfg_got_sta_connect();

            const struct addrinfo hints = {
                .ai_family = AF_INET,
                .ai_socktype = SOCK_STREAM,
            };
            struct addrinfo *res;

            int err = getaddrinfo(param_web_server, param_web_port, &hints, &res);
            if (err != 0 || res == NULL) {
                if (res)
                    freeaddrinfo(res);
                continue;
            }

            int s = socket(res->ai_family, res->ai_socktype, 0);
            if (s < 0) {
                freeaddrinfo(res);
                continue;
            }

            /* Route via the station interface, which is always en0. */
            const struct ifreq ifreq = { "en0" };
            setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE, &ifreq, sizeof(ifreq));

            const struct timeval timeout = { 60, 0 }; /* 60 second timeout */
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

            if (connect(s, res->ai_addr, res->ai_addrlen) != 0) {
                close(s);
                freeaddrinfo(res);
                continue;
            }

            freeaddrinfo(res);


            /* Delay this allocation, to avoid using this memory unless a
             * connection is possible. */
            if (!post_buf) {
                post_buf = malloc(POST_BUFFER_SIZE);
                if (!post_buf) {
                    close(s);
                    continue;
                }
            }

            /*
             * Support for pushing the data to the server.
             *
             *
             * For efficiency, want to find an index and total size to send, then
             * send it in chunks, and only when this is done to go back and look
             * for more in that index or move on and get a new index and size.
             */

            uint32_t size;
            do {
                size = index_size_being_pushed - index_size_pushed;
                if (size > 4096) {
                    /* Reset to search for the current index. */
                    index_being_pushed = 0xffffffff;
                    index_size_being_pushed = 0;
                    index_size_pushed = 0;
                    index_being_pushed_sealed = false;
                    index_being_pushed_next_index = 0xffffffff;
                    size = 0;
                    break;
                }
                if (size > 0) {
                    /* More to push in the current known index range.
                     * Limit the size to the maximum chunk size. */
                    if (size > CHUNK_SIZE) {
                        size = CHUNK_SIZE;
                    }
                    if (!get_buffer_range(index_being_pushed, index_size_pushed,
                                          index_size_pushed + size,
                                          &post_buf[PREFIX_SIZE + 16])) {
                        /* Reset to search for the current index. */
                        index_being_pushed = 0xffffffff;
                        index_size_being_pushed = 0;
                        index_size_pushed = 0;
                        index_being_pushed_sealed = false;
                        index_being_pushed_next_index = 0xffffffff;
                        size = 0;
                    }
                    break;
                }

                /* Check if the index range has grown. */
                uint32_t last_index = index_being_pushed;
                index_size_being_pushed = get_buffer_size(index_being_pushed,
                                                          &index_being_pushed,
                                                          &index_being_pushed_next_index,
                                                          &index_being_pushed_sealed);

                if (index_being_pushed != last_index) {
                    /* The index being pushed was not found. This might occur if
                     * the buffer wraps and erases the sector, but this is
                     * unlikely. It might also occur if searching for an index
                     * beyond the head. Push the index found, from the start. */
                    index_size_pushed = 0;
                }

                if (index_size_being_pushed > index_size_pushed) {
                    /* Some more data to push now. */
                    size = index_size_being_pushed - index_size_pushed;
                    if (size > CHUNK_SIZE) {
                        size = CHUNK_SIZE;
                    }
                    if (get_buffer_range(index_being_pushed, index_size_pushed,
                                         index_size_pushed + size,
                                         &post_buf[PREFIX_SIZE + 16])) {
                        /* Done */
                        break;
                    }

                    /* This might occur if the buffer wraps and erases the
                     * sector. Just loop and retry. */
                    index_size_being_pushed = 0;
                    index_size_pushed = 0;
                    index_being_pushed_sealed = false;
                    index_being_pushed_next_index = 0xffffffff;
                    continue;
                }

                if (index_size_being_pushed == index_size_pushed) {
                    /* Nothing more to send for this index */
                    size = 0;
                    if (index_being_pushed_sealed &&
                        index_being_pushed_next_index != 0xffffffff) {
                        /* Move on to the next index and recheck. */
                        index_being_pushed = index_being_pushed_next_index;
                        index_size_being_pushed = 0;
                        index_size_pushed = 0;
                        index_being_pushed_sealed = false;
                        index_being_pushed_next_index = 0xffffffff;
                        continue;
                    }
                    /* No more data is available, wait. */
                    break;
                }

                /* Reset to search for the current index. */
                index_being_pushed = 0xffffffff;
                index_size_being_pushed = 0;
                index_size_pushed = 0;
                index_being_pushed_sealed = false;
                index_being_pushed_next_index = 0xffffffff;
                size = 0;
                break;
            } while (size == 0);

            if (size == 0) {
                clear_maybe_buffer_to_post();
                close(s);
                break;
            }

            /*
             * The sensor ID, and the index of the record, the local time, and
             * the index at which this content starts within the record are
             * prefixed.
             */
            post_buf[PREFIX_SIZE + 0] = param_sensor_id;
            post_buf[PREFIX_SIZE + 1] = param_sensor_id >>  8;
            post_buf[PREFIX_SIZE + 2] = param_sensor_id >> 16;
            post_buf[PREFIX_SIZE + 3] = param_sensor_id >> 24;

            uint32_t time = RTC.COUNTER;
            post_buf[PREFIX_SIZE + 4] = time;
            post_buf[PREFIX_SIZE + 5] = time >>  8;
            post_buf[PREFIX_SIZE + 6] = time >> 16;
            post_buf[PREFIX_SIZE + 7] = time >> 24;

            post_buf[PREFIX_SIZE + 8] = index_being_pushed;
            post_buf[PREFIX_SIZE + 9] = index_being_pushed >>  8;
            post_buf[PREFIX_SIZE + 10] = index_being_pushed >> 16;
            post_buf[PREFIX_SIZE + 11] = index_being_pushed >> 24;

            post_buf[PREFIX_SIZE + 12] = index_size_pushed;
            post_buf[PREFIX_SIZE + 13] = index_size_pushed >>  8;
            post_buf[PREFIX_SIZE + 14] = index_size_pushed >> 16;
            post_buf[PREFIX_SIZE + 15] = index_size_pushed >> 24;

            /*
             * Firstly use the prefix area for the key to implement MAC-SHA3.
             */
            memcpy(&post_buf[PREFIX_SIZE - param_key_size], param_sha3_key, param_key_size);
            FIPS202_SHA3_224(&post_buf[PREFIX_SIZE - param_key_size],
                             param_key_size + 16 + size,
                             &post_buf[PREFIX_SIZE + 16 + size]);

            /*
             * Next use the prefix area for the HTTP header.
             */
            uint32_t header_size = snprintf((char *)post_buf, PREFIX_SIZE,
                                            "POST %s HTTP/1.1\r\n"
                                            "Host: %s:%s\r\n"
                                            "Connection: close\r\n"
                                            "Content-Type: application/octet-stream\r\n"
                                            "Content-Length: %d\r\n"
                                            "\r\n", param_web_path, param_web_server,
                                            param_web_port, 16 + size + SIGNATURE_SIZE);

            /*
             * Move the header up to meet the data.
             */
            for (j = 0; j < header_size; j++)
                post_buf[PREFIX_SIZE - j - 1] = post_buf[header_size - j - 1];

            /*
             * Data ready to send.
             */
            if (write(s, &post_buf[PREFIX_SIZE - header_size],
                      header_size + 16 + size + SIGNATURE_SIZE) < 0) {
                close(s);
                continue;
            }

            char recv_buf[128];
            int r;
            /* Search for the end of the response headers */
            int end = 0;
            int headers_end = 0;
            int i;
            do {
                r = read(s, recv_buf + end, 127 - end);
                if (r > 0) {
                    end += r;
                    if (end >= 4) {
                        for (i = 0; i < end - 4; i++) {
                            if (recv_buf[i + 0] == '\r' &&
                                recv_buf[i + 1] == '\n' &&
                                recv_buf[i + 2] == '\r' &&
                                recv_buf[i + 3] == '\n') {
                                headers_end = i + 4;
                                break;
                            }
                        }
                        if (headers_end)
                            break;
                        /* Keep the last three bytes as they might have
                         * contained part of the end of the headers. */
                        for (i = 0; i < 3; i++)
                            recv_buf[i] = recv_buf[end - 3 + i];
                        end = 3;
                    }
                }
            } while (r > 0);

            if (headers_end) {
                /* Shift down. */
                end -= headers_end;
                for (i = 0; i < end; i++)
                    recv_buf[i] = recv_buf[headers_end + i];
                /* Keep reading the body. */
                while (end < 20) {
                    r = read(s, recv_buf + end, 127 - end);
                    if (r < 0)
                        break;
                    end += r;
                };

                /* Accept larger responses, for future extension. There is a
                 * magic number that indicates a successful response which is
                 * checked. */
                if (end >= 20) {
                    uint32_t recv_magic = recv_buf[0] |
                        (recv_buf[1] << 8) |
                        (recv_buf[2] << 16) |
                        (recv_buf[3] << 24);
                    uint32_t recv_sec = recv_buf[4] |
                        (recv_buf[5] << 8) |
                        (recv_buf[6] << 16) |
                        (recv_buf[7] << 24);
                    uint32_t recv_usec = recv_buf[8] |
                        (recv_buf[9] << 8) |
                        (recv_buf[10] << 16) |
                        (recv_buf[11] << 24);
                    uint32_t recv_index = recv_buf[12] |
                        (recv_buf[13] << 8) |
                        (recv_buf[14] << 16) |
                        (recv_buf[15] << 24);
                    uint32_t recv_size = recv_buf[16] |
                        (recv_buf[17] << 8) |
                        (recv_buf[18] << 16) |
                        (recv_buf[19] << 24);

                    uint32_t magic = param_sensor_id ^ time;
                    if (recv_magic == magic) {
                        /*
                         * Update the clock using the server response time.
                         */
                        ds3231_note_time(recv_sec);

                        /* Log the server time in it's response. This gives time
                         * stamps to the events logged to help synchronize the
                         * RTC counter to the real time. While the server could
                         * log the times to synchronize to the RTC counter, this
                         * gives some resilience against server data loss and
                         * allows the sectors recorded to stand on their own.
                         *
                         * The event time-stamp is close enough to the received
                         * time, and includes the posted time too to allow
                         * matching with the server recorded times and also to
                         * give the round-trip time to send and receive the post
                         * which might help estimate the accuracy. Re-use the
                         * post_buf to build this event.
                         *
                         * Skip logging this event if there was another POST
                         * event logged in the last 60 seconds. This limits the
                         * storage space used when a lot of sectors are posted
                         * one after the other, and one every 60 seconds seems
                         * adequate for the purpose of synchronizing the times.
                         *
                         */
                        if (recv_sec > last_recv_sec + 60) {
                            post_buf[PREFIX_SIZE + 0] = time;
                            post_buf[PREFIX_SIZE + 1] = time >>  8;
                            post_buf[PREFIX_SIZE + 2] = time >> 16;
                            post_buf[PREFIX_SIZE + 3] = time >> 24;

                            post_buf[PREFIX_SIZE + 4] = recv_sec;
                            post_buf[PREFIX_SIZE + 5] = recv_sec >>  8;
                            post_buf[PREFIX_SIZE + 6] = recv_sec >> 16;
                            post_buf[PREFIX_SIZE + 7] = recv_sec >> 24;

                            post_buf[PREFIX_SIZE + 8] = recv_usec;
                            post_buf[PREFIX_SIZE + 9] = recv_usec >>  8;
                            post_buf[PREFIX_SIZE + 10] = recv_usec >> 16;
                            post_buf[PREFIX_SIZE + 11] = recv_usec >> 24;

                            while (1) {
                                uint32_t new_segment = dbuf_append(last_segment,
                                                                   DBUF_EVENT_POST_TIME,
                                                                   &post_buf[PREFIX_SIZE],
                                                                   12, 0);
                                if (new_segment == last_segment) {
                                    last_recv_sec = recv_sec;
                                    break;
                                }
                                last_segment = new_segment;
                            }
                        }

                        /* The server response is used to set the buffer indexes
                         * known to have been received. This allows the server
                         * to request data be re-sent, or to skip over data
                         * already received when restarted.
                         */
                        if (recv_index != index_being_pushed) {
                            if (recv_index > index_being_pushed &&
                                index_being_pushed_next_index == 0xffffffff) {
                                /* Looks like a bad request from the server for
                                 * an index beyond those stored on the
                                 * device. Need to catch this or the device will
                                 * continue sending data back and not stop.
                                 */
                                index_size_pushed = index_size_being_pushed;
                            } else {
                                index_being_pushed = recv_index;
                                /* Ignore the size in this case, to avoid
                                 * getting and checking the new index size.
                                 * The server will move it along again.
                                 */
                                index_size_being_pushed = 0;
                                index_size_pushed = 0;
                                index_being_pushed_sealed = false;
                                index_being_pushed_next_index = 0xffffffff;
                            }
                        } else {
                            if (recv_size > index_size_being_pushed) {
                                recv_size = index_size_being_pushed;
                            }
                            index_size_pushed = recv_size;
                        }
                        blink_white();
                        hold_off_time = 0;
                    }
                }
            }

            /*
             * At this point the server is expected to close the connection, so
             * wait briefly for it to do so before giving up. While here consume
             * any excess input to avoid a connection reset.
             */
            const struct timeval timeout5 = { 5, 0 }; /* 5 second timeout */
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout5, sizeof(timeout5));
            size_t len;
            for (len = 0; len < 4096; len++) {
                char c;
                int res = read(s, &c, 1);
                if (res != 1) break;
            }

            close(s);
        }
    }
}

void init_post()
{
    if (param_web_server && param_web_path && param_sensor_id &&
        param_key_size == 287 && param_sha3_key) {
        /* Only run the post task if there is a station interface. */
        uint8_t mode = sdk_wifi_get_opmode();
        if (mode != STATION_MODE && mode != STATIONAP_MODE) {
            return;
        }
        xTaskCreate(&post_data, "OAQ Push", 448, NULL, 1, &post_data_task);
    }
}
