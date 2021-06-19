/*
 * Example of using FatFs with RTC clock
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include <esp/uart.h>
#include <espressif/esp_common.h>
#include <stdio.h>
#include <fatfs/ff.h>
#include <FreeRTOS.h>
#include <task.h>
#include <i2c/i2c.h>
#include <ds1307/ds1307.h>

// SD card
#define CS_GPIO_PIN 2

// ds1307
#define I2C_BUS 0
#define SCL_PIN 5
#define SDA_PIN 4

#define TEST_FILENAME "/test_rtc_file.txt"
#define TEST_CONTENTS "Hello! It's a test file and it can be deleted!"
#define DELAY_MS 3000

// This function called by FatFs
uint32_t get_fattime()
{
    struct tm time;
    i2c_dev_t dev = {
        .addr = DS1307_ADDR,
        .bus = I2C_BUS,
    };
    ds1307_get_time(&dev, &time);

    return ((uint32_t)(time.tm_year - 1980) << 25)
        | ((uint32_t)time.tm_mon << 21)
        | ((uint32_t)time.tm_mday << 16)
        | ((uint32_t)time.tm_hour << 11)
        | ((uint32_t)time.tm_min << 5)
        | ((uint32_t)time.tm_sec >> 1);
}

static const char contents[] = TEST_CONTENTS;
static FATFS fs;

static void dump_fileinfo()
{
    FILINFO info;
    printf("File: %s\n", TEST_FILENAME);
    if (f_stat(TEST_FILENAME, &info) != FR_OK)
    {
        printf("Cannot get file status\n");
        return;
    }
    printf("File size: %u bytes\n", (uint32_t)info.fsize);
    printf(
        "Modified: %04d-%02d-%02d %02d:%02d:%02d\n",
        (info.fdate >> 9) + 1980, // year
        (info.fdate >> 5) & 0x0f, // month
        info.fdate & 0x1f,        // day
        info.ftime >> 11,         // hours
        (info.ftime >> 5) & 0x3F, // minutes
        (info.ftime & 0x1f) << 1  // seconds
    );

}

void rewrite_file_task(void *p)
{
    const char *volume = f_gpio_to_volume(CS_GPIO_PIN);

    while (true)
    {
        do
        {
            if (f_mount(&fs, volume, 1) != FR_OK)
            {
                printf("Cannot mount volume %s\n", volume);
                break;
            }

            if (f_chdrive(volume) != FR_OK)
            {
                printf("Cannot set default drive %s\n", volume);
                break;
            }

            printf("\nTest file\n----------------------------\n");

            dump_fileinfo();

            printf("\nRe-creating test file\n----------------------------\n");

            FIL f; //< It's big and it's on the stack! We need larger stack size

            if (f_open(&f, TEST_FILENAME, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
            {
                printf("Cannot create file %s\n", TEST_FILENAME);
                break;
            }

            size_t bw;
            if (f_write(&f, contents, sizeof(contents) - 1, &bw))
            {
                printf("Cannot write to file\n");
                break;
            }
            printf("Bytes written: %d\n", bw);

            if (f_close(&f) != FR_OK)
            {
                printf("Cannot close file\n");
                break;
            }

            dump_fileinfo();

            f_mount(NULL, volume, 0);
        }
        while (false);

        vTaskDelay(DELAY_MS / portTICK_PERIOD_MS);
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n\n", sdk_system_get_sdk_version());

    i2c_init(I2C_BUS, SCL_PIN, SDA_PIN, I2C_FREQ_400K);

    xTaskCreate(rewrite_file_task, "task1", 512, NULL, 2, NULL);
}
