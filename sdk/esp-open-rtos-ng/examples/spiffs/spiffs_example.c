#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "esp8266.h"

#include "fcntl.h"
#include "unistd.h"

#include "spiffs.h"
#include "esp_spiffs.h"

/**
 * This example shows the default SPIFFS configuration when SPIFFS is
 * configured in compile-time (SPIFFS_SINGLETON = 1).
 *
 * To configure SPIFFS in run-time uncomment SPIFFS_SINGLETON in the Makefile
 * and replace the commented esp_spiffs_init in the code below.
 *
 */

static void example_read_file_posix()
{
    const int buf_size = 0xFF;
    uint8_t buf[buf_size];

    int fd = open("test.txt", O_RDONLY);
    if (fd < 0) {
        printf("Error opening file\n");
        return;
    }

    int read_bytes = read(fd, buf, buf_size);
    printf("Read %d bytes\n", read_bytes);

    buf[read_bytes] = '\0';    // zero terminate string
    printf("Data: %s\n", buf);

    close(fd);
}

static void example_read_file_spiffs()
{
    const int buf_size = 0xFF;
    uint8_t buf[buf_size];

    spiffs_file fd = SPIFFS_open(&fs, "other.txt", SPIFFS_RDONLY, 0);
    if (fd < 0) {
        printf("Error opening file\n");
        return;
    }

    int read_bytes = SPIFFS_read(&fs, fd, buf, buf_size);
    printf("Read %d bytes\n", read_bytes);

    buf[read_bytes] = '\0';    // zero terminate string
    printf("Data: %s\n", buf);

    SPIFFS_close(&fs, fd);
}

static void example_write_file()
{
    uint8_t buf[] = "Example data, written by ESP8266";

    int fd = open("other.txt", O_WRONLY|O_CREAT, 0);
    if (fd < 0) {
        printf("Error opening file\n");
        return;
    }

    int written = write(fd, buf, sizeof(buf));
    printf("Written %d bytes\n", written);

    close(fd);
}

static void example_fs_info()
{
    uint32_t total, used;
    SPIFFS_info(&fs, &total, &used);
    printf("Total: %d bytes, used: %d bytes", total, used);
}

void test_task(void *pvParameters)
{
#if SPIFFS_SINGLETON == 1
    esp_spiffs_init();
#else
    // for run-time configuration when SPIFFS_SINGLETON = 0
    esp_spiffs_init(0x200000, 0x10000);
#endif

    if (esp_spiffs_mount() != SPIFFS_OK) {
        printf("Error mount SPIFFS\n");
    }

    while (1) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);

        example_write_file();

        example_read_file_posix();

        example_read_file_spiffs();

        example_fs_info();

        printf("\n\n");
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);

    xTaskCreate(test_task, "test_task", 1024, NULL, 2, NULL);
}
