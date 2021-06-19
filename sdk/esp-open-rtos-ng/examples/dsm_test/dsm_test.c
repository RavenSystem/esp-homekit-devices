/* Very basic example to test the dsm library
 * Led intensity from module will change over time.
 *
 * Part of esp-open-rtos
 * Copyright (C) 2018 zaltora (https://github.com/Zaltora)
 * BSD Licensed as described in the file LICENSE
 */
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "dsm.h"

#define TEST_WITH_160MHZ (0)
#define DSM_PIN          (2)

void task1(void *pvParameters)
{
    uint32_t const init_count = 0;
    uint32_t count = init_count;
    while(1)
    {
        vTaskDelay(100/portTICK_PERIOD_MS);
        printf("Target set to %3u, ", count);
        //Freq = (80,000,000/prescale) * (target / 256) HZ           (0   < target < 128)
        //Freq = (80,000,000/prescale) * ((256 - target) / 256)  HZ  (128 < target < 256)
        if (count < 128)
        {
           printf("Freqency: %.1f Hz\r\n", (80000000.0/255.0 * (count/ 256.0)));
        }
        else
        {
            printf("Freqency: %.1f Hz\r\n", 80000000.0/255.0 * ((256.0-count)/ 256.0));
        }
        dsm_set_target(count);
        count++;
        if (count > UINT8_MAX)
            count = init_count;
    }
}

void user_init(void)
{
    uint8_t pins[1];
    uart_set_baud(0, 115200);

#if (TEST_WITH_160MHZ)
    sdk_system_update_cpu_freq(160);
#endif

    printf("SDK version:%s\r\n", sdk_system_get_sdk_version());

    pins[0] = DSM_PIN;

    /* register pin to use with DSM */
    dsm_init(1, pins);
    /* Set prescale to FF to get a proper signal */
    dsm_set_prescale(0xFF);
    /* Target initial */
    dsm_set_target(0);
    /* start dsm to pin */
    dsm_start();

    printf("dsm start\r\n");

    xTaskCreate(task1, "tsk1", 256, NULL, 2, NULL);
}
