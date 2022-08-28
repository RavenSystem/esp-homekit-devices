#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "esp/timer.h"
#include "FreeRTOS.h"
#include "task.h"
#include "esp8266.h"
#include <stdio.h>

#include "esp_spiffs.h"
#include "spiffs.h"

#include "fs_test.h"

#include "testcase.h"

DEFINE_SOLO_TESTCASE(05_spiffs)

static fs_time_t get_current_time()
{
     return timer_get_count(FRC2) / 5000;  // to get roughly 1ms resolution
}

static void test_task(void *pvParameters)
{
    esp_spiffs_init();
    esp_spiffs_mount();
    SPIFFS_unmount(&fs);  // FS must be unmounted before formating
    if (SPIFFS_format(&fs) == SPIFFS_OK) {
        printf("Format complete\n");
    } else {
        printf("Format failed\n");
    }
    esp_spiffs_mount();

    TEST_ASSERT_TRUE_MESSAGE(fs_load_test_run(100), "Load test failed");

    float write_rate, read_rate;
    if (fs_speed_test_run(get_current_time, &write_rate, &read_rate)) {
        printf("Read speed: %.0f bytes/s\n", read_rate * 1000);
        printf("Write speed: %.0f bytes/s\n", write_rate * 1000);
    } else {
        TEST_FAIL();
    }
    TEST_PASS();
}

static void a_05_spiffs(void)
{
    xTaskCreate(test_task, "test_task", 1024, NULL, 2, NULL);
}
