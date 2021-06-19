/*
 * Example of using FatFs
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include <esp/uart.h>
#include <espressif/esp_common.h>
#include <stdio.h>
#include <fatfs/ff.h>

#define CS_GPIO_PIN 2
#define TEST_FILENAME "/test_loooong_filename.txt"
#define TEST_DIRECTORYNAME "my_directory"
#define TEST_CONTENTS "Hello! It's FatFs on esp8266 with ESP Open RTOS!"
#define READBUF_SIZE 256
#define DELAY_MS 3000

static const char contents[] = TEST_CONTENTS;

static const char *results[] = {
    [FR_OK]                  = "Succeeded",
    [FR_DISK_ERR]            = "A hard error occurred in the low level disk I/O layer",
    [FR_INT_ERR]             = "Assertion failed",
    [FR_NOT_READY]           = "The physical drive cannot work",
    [FR_NO_FILE]             = "Could not find the file",
    [FR_NO_PATH]             = "Could not find the path",
    [FR_INVALID_NAME]        = "The path name format is invalid",
    [FR_DENIED]              = "Access denied due to prohibited access or directory full",
    [FR_EXIST]               = "Access denied due to prohibited access",
    [FR_INVALID_OBJECT]      = "The file/directory object is invalid",
    [FR_WRITE_PROTECTED]     = "The physical drive is write protected",
    [FR_INVALID_DRIVE]       = "The logical drive number is invalid",
    [FR_NOT_ENABLED]         = "The volume has no work area",
    [FR_NO_FILESYSTEM]       = "There is no valid FAT volume",
    [FR_MKFS_ABORTED]        = "The f_mkfs() aborted due to any problem",
    [FR_TIMEOUT]             = "Could not get a grant to access the volume within defined period",
    [FR_LOCKED]              = "The operation is rejected according to the file sharing policy",
    [FR_NOT_ENOUGH_CORE]     = "LFN working buffer could not be allocated",
    [FR_TOO_MANY_OPEN_FILES] = "Number of open files > _FS_LOCK",
    [FR_INVALID_PARAMETER]   = "Given parameter is invalid"
};

static char readbuf[READBUF_SIZE];

static bool failed(FRESULT res)
{
    bool fail = res != FR_OK;
    if (fail)
        printf("\n  Error: ");
    printf("\n  %s\n", results[res]);
    return fail;
}

void check_fatfs()
{
    const char *vol = f_gpio_to_volume(CS_GPIO_PIN);

    printf("\nCreating test file\n----------------------------\n");

    FATFS fs;
    // Mount filesystem
    printf("f_mount(&fs, \"%s\")", vol);
    if (failed(f_mount(&fs, vol, 1)))
        return;

    // Set default drive
    printf("f_chdrive(\"%s\")", vol);
    if (failed(f_chdrive(vol)))
        return;

    // Create a directory if it not exists
    FILINFO info;
    if (failed(f_stat(TEST_DIRECTORYNAME, &info)) && info.fattrib & AM_DIR)
    {
      printf("f_mkdir (\"%s\")\n", TEST_DIRECTORYNAME);
      if (failed(f_mkdir(TEST_DIRECTORYNAME)))
          return;
    }
    else
    {
      printf("\"%s\" directory already exists\n", TEST_DIRECTORYNAME);
    }

    FIL f;
    // Create test file
    printf("f_open(&f, \"%s\", FA_WRITE | FA_CREATE_ALWAYS)", TEST_FILENAME);
    if (failed(f_open(&f, TEST_FILENAME, FA_WRITE | FA_CREATE_ALWAYS)))
        return;

    size_t written;
    // Write test string
    printf("f_write(&f, \"%s\")", contents);
    if (failed(f_write(&f, contents, sizeof(contents) - 1, &written)))
        return;
    printf("  Bytes written: %d\n", written);

    // Close file
    printf("f_close(&f)");
    if (failed(f_close(&f)))
        return;

    printf("\nReading test file\n----------------------------\n");

    // Open test file
    printf("f_open(&f, \"%s\", FA_READ)", TEST_FILENAME);
    if (failed(f_open(&f, TEST_FILENAME, FA_READ)))
        return;

    printf("  File size: %u\n", (uint32_t)f_size(&f));

    size_t readed;
    // Read file
    printf("f_read(&f, ...)");
    if (failed(f_read(&f, readbuf, sizeof(readbuf) - 1, &readed)))
        return;
    readbuf[readed] = 0;

    printf("  Readed %u bytes, test file contents: %s\n", readed, readbuf);

    // Close file
    printf("f_close(&f)");
    if (failed(f_close(&f)))
        return;

    // Unmount
    printf("f_mount(NULL, \"%s\")", vol);
    if (failed(f_mount(NULL, vol, 0)))
        return;
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n\n", sdk_system_get_sdk_version());

    while (true)
    {
        printf("***********************************\nTesting FAT filesystem\n***********************************\n");
        check_fatfs();
        printf("\n\n");
        for (size_t i = 0; i < DELAY_MS; i ++)
            sdk_os_delay_us(1000);
    }
}
