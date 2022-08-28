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
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <dirent.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "spiffs_config.h"
#include "../spiffs/src/spiffs.h"

static spiffs fs;
static void *image = 0;
static void *work_buf = 0;
static void *fds_buf = 0;
static void *cache_buf = 0;

typedef struct {
    uint32_t fs_size;
    uint32_t log_page_size;
    uint32_t log_block_size;
} fs_config_t;

static void print_usage(const char *prog_name, const char *error_msg)
{
    if (error_msg) {
        printf("Error: %s\n", error_msg);
    }
    printf("Usage: ");
    printf("%s [-D directory] [-f image-name] [-s size]\n", prog_name);
    printf("\t[-p page-size] [-b block-size]\n\n");
    printf("Example:\n");
    printf("\t%s -D ./my_files -f spiffs.img -s 0x10000 -p 256 -b 8192\n\n", 
            prog_name);
}

static s32_t _read_data(u32_t addr, u32_t size, u8_t *dst)
{
    memcpy(dst, (uint8_t*)image + addr, size);
    return SPIFFS_OK;
}

static s32_t _write_data(u32_t addr, u32_t size, u8_t *src)
{
    uint32_t i;
    uint8_t *dst = image + addr;

    for (i = 0; i < size; i++) {
        dst[i] &= src[i];  // mimic NOR flash, flip only 1 to 0
    }
    return  SPIFFS_OK;
}

static s32_t _erase_data(u32_t addr, u32_t size)
{
    memset((uint8_t*)image + addr, 0xFF, size);
    return SPIFFS_OK;
}

static bool init_spiffs(bool allocate_mem, const fs_config_t *fs_config)
{
    spiffs_config config = {0};
    printf("Initializing SPIFFS, size=%d\n", fs_config->fs_size);

    config.hal_read_f = _read_data;
    config.hal_write_f = _write_data;
    config.hal_erase_f = _erase_data;

    config.phys_addr = 0;
    config.phys_size = fs_config->fs_size;
    config.log_page_size = fs_config->log_page_size;
    config.log_block_size = fs_config->log_block_size;
    config.phys_erase_block = SPIFFS_ESP_ERASE_SIZE;

    // initialize fs.cfg so the following helper functions work correctly
    memcpy(&fs.cfg, &config, sizeof(spiffs_config));

    int workBufSize = 2 * fs_config->log_page_size;
    int fdsBufSize = SPIFFS_buffer_bytes_for_filedescs(&fs, 5);
    int cacheBufSize = SPIFFS_buffer_bytes_for_cache(&fs, 5);

    if (allocate_mem) {
        image = malloc(fs_config->fs_size);
        work_buf = malloc(workBufSize);
        fds_buf = malloc(fdsBufSize);
        cache_buf = malloc(cacheBufSize);
        printf("spiffs memory, work_buf_size=%d, fds_buf_size=%d, cache_buf_size=%d\n",
                workBufSize, fdsBufSize, cacheBufSize);
    }

    int32_t err = SPIFFS_mount(&fs, &config, work_buf, fds_buf, fdsBufSize,
            cache_buf, cacheBufSize, 0);

    return err == SPIFFS_OK;
}

static bool format_spiffs()
{
    SPIFFS_unmount(&fs);

    if (SPIFFS_format(&fs) == SPIFFS_OK) {
        printf("Format complete\n");
    } else {
        printf("Failed to format SPIFFS\n");
        return false;
    }

    return true;
}

static void spiffs_free()
{
    free(image);
    image = NULL;

    free(work_buf);
    work_buf = NULL;

    free(fds_buf);
    fds_buf = NULL;

    free(cache_buf);
    cache_buf = NULL;
}

static bool process_file(const char *src_file, const char *dst_file)
{
    int fd;
    const int buf_size = 256;
    uint8_t buf[buf_size];
    int data_len;

    fd = open(src_file, O_RDONLY);
    if (fd < 0) {
        printf("Error openning file: %s\n", src_file);
    }

    spiffs_file out_fd = SPIFFS_open(&fs, dst_file,
            SPIFFS_O_CREAT | SPIFFS_O_WRONLY, 0);
    while ((data_len = read(fd, buf, buf_size)) != 0) {
        if (SPIFFS_write(&fs, out_fd, buf, data_len) != data_len) {
            printf("Error writing to SPIFFS file\n");
            break;
        }
    }
    SPIFFS_close(&fs, out_fd);
    close(fd);
    return true;
}

static bool process_directory(const char *direcotry)
{
    DIR *dp;
    struct dirent *ep;
    char path[256], *filename;

    dp = opendir(direcotry);
    if (dp != NULL) {
        while ((ep = readdir(dp)) != 0) {
            if (!strcmp(ep->d_name, ".") ||
                !strcmp(ep->d_name, "..")) {
                continue;
            }
            if(ep->d_type == DT_DIR) {
                char *new_dir_name = malloc(strlen(direcotry) + strlen(ep->d_name) + 2);
                sprintf(new_dir_name, "%s/%s", direcotry, ep->d_name);
                process_directory(new_dir_name);
                free(new_dir_name);
                continue;
            }
            if (ep->d_type != DT_REG) {
                continue;  // not a regular file
            }
            sprintf(path, "%s/%s", direcotry, ep->d_name);
            filename = strchr(path, '/');
            filename = filename ? &filename[1] : path;
            
            printf("Processing file source %s, dest: %s\n", path, filename);
            if (!process_file(path, filename)) {
                printf("Error processing file\n");
                break;
            }
        }
        closedir(dp);
    } else {
        printf("Error reading direcotry: %s\n", direcotry);
    }
    return true;
}

static bool write_image(const char *out_file, const fs_config_t *fs_config)
{
    int fd;
    int size = fs_config->fs_size;
    uint8_t *p = (uint8_t*)image;
    fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        printf("Error creating file %s\n", out_file);
        return false;
    }

    printf("Writing image to file: %s\n", out_file);

    while (size != 0) {
        write(fd, p, fs_config->log_page_size);
        p += fs_config->log_page_size;
        size -= fs_config->log_page_size;
    }

    close(fd);
    return true;
}

int main(int argc, char *argv[])
{
    int result = 0;
    int option = 0;
    fs_config_t fs_config = {0};
    char *image_file_name = 0;
    char *directory = 0;

    while ((option = getopt(argc, argv, "D:f:s:p:b:e:")) != -1) {
        switch (option) {
            case 'D':  // directory
                directory = optarg;
                break;
            case 'f':  // image file name
                image_file_name = optarg;
                break;
            case 's':  // file system size
                fs_config.fs_size = (uint32_t)strtol(optarg, NULL, 0);
                break;
            case 'p':  // logical page size
                fs_config.log_page_size = (uint32_t)strtol(optarg, NULL, 0);
                break;
            case 'b':  // logical block size
                fs_config.log_block_size = (uint32_t)strtol(optarg, NULL, 0);
                break;
            default:
                print_usage(argv[0], NULL);
                return -1;
        }
    }

    if (!image_file_name || !directory || !fs_config.fs_size
            || !fs_config.log_page_size || !fs_config.log_block_size) {
        print_usage(argv[0], NULL);
        return -1;
    }

    init_spiffs(/*allocate_mem=*/true, &fs_config);

    if (format_spiffs()) {
        if (init_spiffs(/*allocate_mem=*/false, &fs_config)) {
            if (process_directory(directory)) {
                SPIFFS_unmount(&fs);
                if (!write_image(image_file_name, &fs_config)) {
                    printf("Error writing image\n");
                }
            } else {
                printf("Error processing direcotry\n");
            }
        } else {
            printf("Failed to mount SPIFFS\n");
        }
    } else {
        printf("Error formating spiffs\n");
    }

    spiffs_free();
    return result;
}
