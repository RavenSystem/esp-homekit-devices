/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 sheinz (https://github.com/sheinz)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "fs_test.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define FILE_NAME_MAX_LEN  32
#define MAX_FILE_COUNT 32
#define MAX_FILE_SIZE  (1024 * 16)
#define FILE_BUF_SIZE 256

// Note: MAX_FILE_SIZE * MAX_FILE_COUNT should not exceed file system size

#define DBG_LOG(...)  printf(__VA_ARGS__)

typedef struct {
    bool exist;
    char name[FILE_NAME_MAX_LEN];
    uint32_t size;
    uint32_t checksum;
} TestFile;

static TestFile test_files[MAX_FILE_COUNT];
static uint8_t test_buf[FILE_BUF_SIZE];
static uint32_t total_written;
static uint32_t total_read;

static void fs_test_init()
{
    total_written = 0;
    total_read = 0;
    for (uint32_t i = 0; i < MAX_FILE_COUNT; i++) {
        test_files[i].exist = false;
    }
}

static void get_new_file_name(char *name)
{
    static uint32_t file_counter = 0;
    snprintf(name, FILE_NAME_MAX_LEN, "%x.file", file_counter);
    file_counter++;
}

/**
 * Write test data to data and return sum of all data bytes written
 */
static uint32_t fill_test_data(uint8_t *data, uint32_t size)
{
    uint32_t sum = 0;
    uint8_t first_byte = rand() % 0xFF;

    for (int i = 0; i < size; ++i) {
        data[i] = first_byte++;
        sum += data[i];
    }

    return sum;
}

static bool write_file(int fd, uint32_t size, uint32_t *checksum)
{
    uint32_t written = 0;

    while (written != size) {
        uint32_t remaining = size - written;
        if (remaining >= FILE_BUF_SIZE) {
            *checksum += fill_test_data(test_buf, FILE_BUF_SIZE);

            if (write(fd, test_buf, FILE_BUF_SIZE) != FILE_BUF_SIZE) {
                return false;
            }
            written += FILE_BUF_SIZE;
        } else {
            *checksum += fill_test_data(test_buf, remaining);

            if (write(fd, test_buf, remaining) != remaining) {
                return false;
            }
            written += remaining;
        }
    }
    total_written += written;
    return true;
}

static bool create_file(uint32_t file_index)
{
    uint32_t size = rand() % MAX_FILE_SIZE;
    test_files[file_index].exist = true;
    get_new_file_name(test_files[file_index].name);
    test_files[file_index].size = size;
    test_files[file_index].checksum = 0;
    DBG_LOG("Writing file='%s' size='%d'\n",
            test_files[file_index].name,
            test_files[file_index].size);

    int fd = open(test_files[file_index].name, O_WRONLY | O_CREAT,
            S_IRUSR|S_IWUSR);
    if (fd == -1) {
        return false;
    }

    if (!write_file(fd,
                    test_files[file_index].size,
                    &test_files[file_index].checksum)) {
        return false;
    }
    /* DBG_LOG("File written, checksum=%d\n", */
    /*         test_files[file_index].checksum); */

    return close(fd) == 0;
}

uint32_t calc_checksum(uint8_t *data, uint32_t size)
{
    uint32_t sum = 0;
    for (int i = 0; i < size; ++i) {
        sum += data[i];
    }
    return sum;
}

static bool read_file_checksum(int fd, uint32_t size, uint32_t *sum)
{
    uint32_t r_bytes = 0;
    *sum = 0;

    while (r_bytes != size) {
        uint32_t remaining = size - r_bytes;
        if (remaining >= FILE_BUF_SIZE) {
            if (read(fd, test_buf, FILE_BUF_SIZE) != FILE_BUF_SIZE) {
                return false;
            }
            *sum += calc_checksum(test_buf, FILE_BUF_SIZE);
            r_bytes += FILE_BUF_SIZE;
        } else {
            if (read(fd, test_buf, remaining) != remaining) {
                return false;
            }
            *sum += calc_checksum(test_buf, remaining);
            r_bytes += remaining;
        }
    }
    total_read += r_bytes;
    return true;
}

static bool verify_file(int fd, uint32_t size, uint32_t checksum)
{
    uint32_t sum = 0;

    if (!read_file_checksum(fd, size, &sum)) {
        return false;
    }

    /* DBG_LOG("File verification, checksum=%d\n", sum); */

    return checksum == sum;
}

static bool read_verify_file(uint32_t file_index)
{
    DBG_LOG("Verifying file='%s' size='%d'\n",
            test_files[file_index].name,
            test_files[file_index].size);

    int fd = open(test_files[file_index].name, O_RDONLY);
    if (fd == -1) {
        return false;
    }

    if (!verify_file(fd, test_files[file_index].size,
                test_files[file_index].checksum)) {
        return false;
    }

    return close(fd) == 0;
}

static bool rewrite_file(uint32_t file_index)
{
    uint32_t new_size = rand() % MAX_FILE_SIZE;
    DBG_LOG("Rewriting file='%s' old_size='%d' new_size='%d'\n",
            test_files[file_index].name,
            test_files[file_index].size,
            new_size);

    test_files[file_index].checksum = 0;
    test_files[file_index].size = new_size;

    int fd = open(test_files[file_index].name, O_WRONLY | O_TRUNC);
    if (fd == -1) {
        return false;
    }

    if (!write_file(fd,
                    test_files[file_index].size,
                    &test_files[file_index].checksum)) {
        return false;
    }

    /* DBG_LOG("File rewritten, checksum=%d\n", */
    /*         test_files[file_index].checksum); */

    return close(fd) == 0;
}

static bool append_file(uint32_t file_index)
{
    uint32_t append_size = rand() % (MAX_FILE_SIZE - test_files[file_index].size);
    DBG_LOG("Appending file='%s' file_size='%d' append_size='%d'\n",
            test_files[file_index].name,
            test_files[file_index].size,
            append_size);

    test_files[file_index].size += append_size;

    int fd = open(test_files[file_index].name, O_WRONLY | O_APPEND);
    if (fd == -1) {
        return false;
    }

    if (!write_file(fd,
                    append_size,
                    &test_files[file_index].checksum)) {
        return false;
    }

    /* DBG_LOG("File appended, checksum=%d\n", */
    /*         test_files[file_index].checksum); */

    return close(fd) == 0;
}

static bool remove_file(uint32_t file_index)
{
    DBG_LOG("Removing file='%s' size='%d'\n",
            test_files[file_index].name,
            test_files[file_index].size);

    if (unlink(test_files[file_index].name) != 0) {
        return false;
    }
    test_files[file_index].exist = false;

    return true;
}

static uint32_t number_of_existing_files()
{
    uint32_t files = 0;
    for (uint32_t i = 0; i < MAX_FILE_COUNT; i++) {
        if (test_files[i].exist) {
            files++;
        }
    }
    return files;
}

/**
 * Return -1 if there's no existing files
 * Return file index in test_file array
 */
static int32_t get_random_existing_file_index()
{
    uint32_t files = number_of_existing_files();
    if (files == 0) {
        return -1;
    }
    uint32_t file = rand() % files;
    for (uint32_t i = 0; i < MAX_FILE_COUNT; i++) {
        if (test_files[i].exist) {
            if (file == 0) {
                return i;
            } else {
                file--;
            }
        }
    }
    return -1;
}

/**
 * Return -1 if there's no free file records
 */
static int32_t get_free_file_index()
{
    for (uint32_t i = 0; i < MAX_FILE_COUNT; i++) {
        if (!test_files[i].exist) {
            return i;
        }
    }
    return -1;
}

static bool test_create_file()
{
    int32_t index = get_free_file_index();

    if (index == -1) {
        return true;   // no free file records, skipping
    }

    return create_file(index);
}

static bool test_read_verify_file()
{
    int32_t index = get_random_existing_file_index();

    if (index == -1) {
        return true;   // no existing files, skipping
    }

    return read_verify_file(index);
}

static bool test_rewrite_file()
{
    int32_t index = get_random_existing_file_index();

    if (index == -1) {
        return true;   // no existing files, skipping
    }

    return rewrite_file(index);
}

static bool test_append_file()
{
    int32_t index = get_random_existing_file_index();

    if (index == -1) {
        return true;   // no existing files, skipping
    }

    return append_file(index);
}

static bool test_remove_file()
{
    int32_t index = get_random_existing_file_index();

    if (index == -1) {
        return true;   // no existing files, skipping
    }

    return remove_file(index);
}

static inline bool read_part_with_lseek(uint32_t index,
        uint32_t start, uint32_t size, uint32_t *checksum)
{
    int fd = open(test_files[index].name, O_RDONLY);
    if (fd == -1) {
        return false;
    }

    DBG_LOG("Testing lseek file='%s' pos='%d' size='%d'\n",
            test_files[index].name, start, size);

    if (lseek(fd, start, SEEK_SET) != start) {
        return false;
    }

    if (!read_file_checksum(fd, size, checksum)) {
        return false;
    }

    return close(fd) == 0;
}

static inline bool read_part_without_lseek(uint32_t index,
        uint32_t start, uint32_t size, uint32_t *checksum)
{
    uint32_t ignore;

    int fd = open(test_files[index].name, O_RDONLY);
    if (fd == -1) {
        return false;
    }

    // read data until start, lseek simulation
    if (!read_file_checksum(fd, start, &ignore)) {
        return false;
    }

    if (!read_file_checksum(fd, size, checksum)) {
        return false;
    }

    return close(fd) == 0;
}

/**
 * Read some of the file and verify that read data is correct.
 * lseek is used to move file read pointer.
 */
static bool test_lseek_file()
{
    int32_t index = get_random_existing_file_index();
    uint32_t checksum, sum;

    if (index == -1) {
        return true;   // no existing files, skipping
    }
    if (test_files[index].size == 0) {
        return true;  // if no data in a file, skipping
    }

    int32_t start = rand() % test_files[index].size;
    int32_t size = rand() % (test_files[index].size - start);

    if (!read_part_without_lseek(index, start, size, &checksum)) {
        return false;
    }

    if (!read_part_with_lseek(index, start, size, &sum)) {
        return false;
    }

    return checksum == sum;
}

static bool test_stat_file()
{
    int32_t index = get_random_existing_file_index();

    if (index == -1) {
        return true;   // no existing files, skipping
    }

    DBG_LOG("Testing stat/fstat file='%s'\n", test_files[index].name);

    struct stat sb;
    if (stat(test_files[index].name, &sb) < 0) {
        return false;
    }
    if (sb.st_size != test_files[index].size) {
        DBG_LOG("stat file size doesn't match\n");
        return false;
    }

    int fd = open(test_files[index].name, O_RDONLY);
    if (fd == -1) {
        return false;
    }
    if (fstat(fd, &sb) < 0) {
        return false;
    }
    if (sb.st_size != test_files[index].size) {
        DBG_LOG("fstat file size doesn't match\n");
        return false;
    }
    close(fd);
    return true;
}

static bool test_read_verify_all_files()
{
    for (uint32_t i = 0; i < MAX_FILE_COUNT; i++) {
        if (test_files[i].exist) {
            if (!read_verify_file(i)) {
                return false;
            }
        }
    }
    return true;
}

static bool remove_all_files()
{
    for (uint32_t i = 0; i < MAX_FILE_COUNT; i++) {
        if (test_files[i].exist) {
            if (!remove_file(i)) {
                return false;
            }
        }
    }
    return true;
}

#define CREATE_INTENSITY        (20)
#define READ_INTENSITY          (20 + CREATE_INTENSITY)
#define REWRITE_INTENSITY       (10 + READ_INTENSITY)
#define APPEND_INTENSITY        (10 + REWRITE_INTENSITY)
#define REMOVE_INTENSITY        (10 + APPEND_INTENSITY)
#define LSEEK_INTENSITY         (10 + REMOVE_INTENSITY)
#define STAT_INTENSITY          (10 + LSEEK_INTENSITY)

#define TOTAL_INTENSITY         (STAT_INTENSITY)

bool fs_load_test_run(int32_t iterations)
{
    fs_test_init();

    for (int32_t i = 0; i < iterations; i++)
    {
        int op = rand() % TOTAL_INTENSITY;
        if (op < CREATE_INTENSITY) {
            if (!test_create_file()) {
                return false;
            }
        } else if (op < READ_INTENSITY) {
            if (!test_read_verify_file()) {
                return false;
            }
        } else if (op < REWRITE_INTENSITY) {
            if (!test_rewrite_file()) {
                return false;
            }
        } else if (op < APPEND_INTENSITY) {
            if (!test_append_file()) {
                return false;
            }
        } else if (op < REMOVE_INTENSITY) {
            if (!test_remove_file()) {
                return false;
            }
        } else if (op < LSEEK_INTENSITY) {
            if (!test_lseek_file()) {
                return false;
            }
        } else if (op < STAT_INTENSITY) {
            if (!test_stat_file()) {
                return false;
            }
        }
        printf("Testing %d%%\n", 100 * i / iterations);
    }

    if (!test_read_verify_all_files()) {
        return false;
    }
    if (!remove_all_files()) {
        return false;
    }
    return true;
}

bool fs_speed_test_run(get_current_time_func get_time,
        float *write_rate,
        float *read_rate)
{
    int files = 0;
    fs_time_t start_time, write_time, read_time;
    fs_test_init();

    start_time = get_time();
    for (files = 0; files < MAX_FILE_COUNT; files++) {
        if (!test_create_file()) {
            return false;
        }
    }
    write_time = get_time() - start_time;
    DBG_LOG("Written %d files, %d bytes\n", files, total_written);
    DBG_LOG("Writing took %d ticks\n", write_time);

    start_time = get_time();
    if (!test_read_verify_all_files()) {
        return false;
    }
    read_time = get_time() - start_time;

    DBG_LOG("Read %d files, %d bytes\n", files, total_read);
    DBG_LOG("Reading took %d ticks\n", read_time);

    if (!remove_all_files()) {
        return false;
    }

    *write_rate = (float)total_written / write_time;
    *read_rate = (float)total_read / read_time;

    return true;
}
